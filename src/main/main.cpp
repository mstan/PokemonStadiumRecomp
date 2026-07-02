/*
 * main.cpp — PokemonStadiumRecomp runner entry point.
 *
 * Adapted from Zelda64Recomp's src/main/main.cpp. Strips:
 *   - recompui (UI overlay; no menu yet)
 *   - mod loader / texture pack subsystem
 *   - Native file dialogs
 *   - Game-specific config + autosave
 *
 * Keeps:
 *   - SDL2 audio queue (queue_samples / get_frames_remaining /
 *     set_frequency / reset_audio)
 *   - SDL2 window + GameController input
 *   - RT64 render context creation via pokestadium::renderer
 *   - recomp::start with full callback wiring
 *
 * No stubs: anywhere we'd want to "skip" something with a stub, we
 * either implement it for real or use a loud-abort pattern that
 * surfaces missing functionality.
 */

#include <array>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <dbghelp.h>
// Avoid Windows.h macro pollution clobbering identifiers in
// librecomp/ultramodern headers below.
#undef ERROR
#undef OUT
#undef IN
#undef OPTIONAL
#undef min
#undef max

#include <SDL.h>
#include <SDL_syswm.h>

/* Round-2 audio: callback-driven clock-domain bridge replaces the per-chunk
 * SDL_AudioCVT + sample-decimation valve (both crackle sources). Plus always-on,
 * env-gated observability. See recomp_audio_drc.h / recomp_audio_debug.h. */
#define RECOMP_AUDIO_DRC_IMPL
#include "recomp_audio_drc.h"
#define RECOMP_AUDIO_DEBUG_IMPL
#include "recomp_audio_debug.h"

#include "app_paths.h"

namespace pkmnstadium {
// Defined here (where <windows.h> is already included with WIN32_LEAN_AND_MEAN /
// NOMINMAX) and shared via app_paths.h with the other translation units.
std::filesystem::path exe_dir() {
#ifdef _WIN32
    char buf[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

std::filesystem::path app_file(const std::string& name) {
    return exe_dir() / name;
}
} // namespace pkmnstadium

// C-callable variant for extras.c (see app_paths.h).
extern "C" const char* psr_app_file_c(const char* name, char* out, unsigned long out_size) {
    if (out == nullptr || out_size == 0) return "";
    const std::string p = pkmnstadium::app_file(name != nullptr ? name : "").string();
    std::snprintf(out, out_size, "%s", p.c_str());
    return out;
}

#include "recomp.h"
#include <librecomp/game.hpp>
#include <ultramodern/ultramodern.hpp>
#include <ultramodern/scheduler_tick.hpp>
#include <ultramodern/error_handling.hpp>
#include <ultramodern/config.hpp>  // renderer::get/set_graphics_config, WindowMode (#18)

#include "pokestadium_render.h"
#include "debug_server.h"
#include <librecomp/ultra_trace.hpp>
#include <librecomp/audio_uaf_protect.hpp>
#include "ares_worker.h"
#include "transfer_pak.h"
#include "ui_seam.h"
#include "input_bindings.h"

extern "C" void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);

namespace pokestadium { void register_overlays(); }
namespace pokestadium::rsp { void register_pre_task_hooks(); }

// RSP microcode entry points provided by RSPRecomp output (the prebuilt
// aspMain.cpp + njpgdspMain.cpp under rsp/, referenced by CMakeLists).
// These implement the libultra standard audio/JPEG microcodes that ship
// with Pokemon Stadium-era N64 games.
extern RspUcodeFunc aspMain;
extern RspUcodeFunc gbTowerMain;
extern RspUcodeFunc gbTowerColorMain;
extern RspUcodeFunc njpgdspMain;

static bool rdram_be_word_matches(uint8_t* rdram, uint32_t vaddr, uint32_t expected) {
    constexpr uint32_t kRdramSize = 0x800000u;
    if (rdram == nullptr) {
        return false;
    }
    uint32_t paddr = vaddr & 0x1FFFFFFFu;
    if (paddr > kRdramSize - sizeof(uint32_t)) {
        return false;
    }
    uint32_t actual = (uint32_t(rdram[(paddr + 0) ^ 3]) << 24) |
                      (uint32_t(rdram[(paddr + 1) ^ 3]) << 16) |
                      (uint32_t(rdram[(paddr + 2) ^ 3]) << 8) |
                      (uint32_t(rdram[(paddr + 3) ^ 3]) << 0);
    return actual == expected;
}

static bool task_boot_matches(uint8_t* rdram, const OSTask* task,
                              uint32_t boot_size,
                              uint32_t word0, uint32_t word1) {
    uint32_t boot = (uint32_t)task->t.ucode_boot;
    return boot != 0 &&
           (uint32_t)task->t.ucode_boot_size == boot_size &&
           rdram_be_word_matches(rdram, boot + 0, word0) &&
           rdram_be_word_matches(rdram, boot + 4, word1);
}

// Single-task replay capture (Milestone 1): wrap aspMain so we can dump the
// RDRAM the task reads (before) and writes (after). Paired with the input dump
// in rsp_aspmain_hook.cpp. Armed only when PSR_ASPMAIN_CAPTURE=<dir> is set;
// otherwise psr_aspmain_capture_dir() returns null and this is a cheap passthrough.
extern "C" const char* psr_aspmain_capture_dir(void);
extern "C" void psr_aspmain_capture_done(void);
// Offline replay entry (aspmain_replay.cpp) — dispatched from main() when
// PSR_ASPMAIN_REPLAY=<capture_dir> is set.
extern "C" int psr_aspmain_replay_main(const char* dir);
// Recomp-side RSP DMA trace ring (librecomp). Mirrors the struct in
// debug_server.cpp / librecomp/src/rsp.cpp. Gated by PSR_RSP_DMA_TRACE=1.
struct PsrRspDmaTraceEvent {
    uint64_t seq;
    uint32_t write;
    uint32_t dmem_addr;
    uint32_t dram_addr;
    uint32_t byte_count;
    uint32_t nonzero_bytes;
    int32_t first_nonzero;
    uint8_t first16[16];
};
extern "C" void recomp_rsp_dma_recent_copy(
    PsrRspDmaTraceEvent* out, uint32_t out_cap,
    uint32_t* out_count, uint64_t* out_write_index);
static RspExitReason aspMain_capture(uint8_t* rdram, uint32_t ucode_addr) {
    const char* dir = psr_aspmain_capture_dir();
    static uint8_t* before = nullptr;   // 8 MiB snapshot of RDRAM pre-task
    if (dir && !before) before = (uint8_t*)malloc(0x800000u);
    if (dir && before) memcpy(before, rdram, 0x800000u);

    // Snapshot the DMA-trace write index BEFORE the task so we can isolate
    // exactly this task's DMAs (the ring accumulates across all audio frames).
    uint64_t dma_idx_before = 0;
    if (dir) recomp_rsp_dma_recent_copy(nullptr, 0, nullptr, &dma_idx_before);

    RspExitReason r = aspMain(rdram, ucode_addr);

    if (dir && before) {
        // Skip SILENT/near-silent tasks: voice-state-only frames change only ~200
        // bytes (no PCM). Require a substantial change so we capture a task that
        // actually renders audio (the output mix buffer is ~700B PCM + voice state).
        // Override threshold via PSR_ASPMAIN_MIN_DELTA.
        size_t changed = 0;
        for (size_t i = 0; i < 0x800000u; i++) if (before[i] != rdram[i]) changed++;
        size_t min_delta = 4096;
        if (const char* e = getenv("PSR_ASPMAIN_MIN_DELTA")) min_delta = (size_t)strtoul(e, nullptr, 0);
        if (changed < min_delta) {
            return r;   // stay armed; hook re-dumps input next task
        }
        fprintf(stderr, "[aspmain_capture] task changed %zu bytes (>= %zu)\n", changed, min_delta);
        char path[640];
        snprintf(path, sizeof(path), "%s/rdram_before.bin", dir);
        if (FILE* f = fopen(path, "wb")) { fwrite(before, 1, 0x800000u, f); fclose(f); }
        snprintf(path, sizeof(path), "%s/rdram_after.bin", dir);
        if (FILE* f = fopen(path, "wb")) { fwrite(rdram, 1, 0x800000u, f); fclose(f); }

        // Dump THIS task's ordered DMA trace (recomp side) for oracle comparison
        // against the ares replay. Requires PSR_RSP_DMA_TRACE=1.
        {
            static std::vector<PsrRspDmaTraceEvent> dbuf(4096);
            uint32_t got = 0; uint64_t widx = 0;
            recomp_rsp_dma_recent_copy(dbuf.data(), (uint32_t)dbuf.size(), &got, &widx);
            snprintf(path, sizeof(path), "%s/recomp_dma.txt", dir);
            if (FILE* f = fopen(path, "w")) {
                fprintf(f, "# recomp aspMain DMA trace (this task: seq >= %llu)\n",
                        (unsigned long long)dma_idx_before);
                uint32_t shown = 0;
                for (uint32_t i = 0; i < got; i++) {
                    const PsrRspDmaTraceEvent& e = dbuf[i];
                    if (e.seq < dma_idx_before) continue;  // earlier task
                    fprintf(f, "%3u %s dram=0x%06X dmem=0x%03X len=0x%X nz=%u first16=",
                            shown++, e.write ? "WR" : "RD",
                            e.dram_addr & 0xFFFFFF, e.dmem_addr & 0xFFF,
                            e.byte_count, e.nonzero_bytes);
                    for (int b = 0; b < 16; b++) fprintf(f, "%02X", e.first16[b]);
                    fprintf(f, "\n");
                }
                fclose(f);
                fprintf(stderr, "[aspmain_capture] wrote recomp_dma.txt (%u dmas this task)\n", shown);
            }
        }
        fprintf(stderr, "[aspmain_capture] non-silent task captured to %s (exit=%d)\n",
                dir, (int)r);
        fflush(stderr);
        psr_aspmain_capture_done();
    }
    return r;
}

static RspUcodeFunc* get_rsp_microcode(uint8_t* rdram, const OSTask* task) {
    if (task_boot_matches(rdram, task, 0xA40, 0x401A6000u, 0xC800207Cu)) {
        return gbTowerMain;
    }
    if (task_boot_matches(rdram, task, 0x9E4, 0x401A6000u, 0xC800207Cu)) {
        return gbTowerColorMain;
    }

    switch (task->t.type) {
        case M_AUDTASK:   return aspMain_capture;   /* passthrough unless capture armed */
        case M_NJPEGTASK: return njpgdspMain;
        default:
            fprintf(stderr, "[Pokemon Stadium] Unknown RSP task type: %u\n", task->t.type);
            return nullptr;
    }
}

// ---- Audio (ported from Zelda64Recomp main.cpp) ----------------------------
// Standard SDL2 audio queue with same channel-swap + resampling as
// Zelda64Recomp uses. Pokemon Stadium's audio output format matches
// MM/OoT (16-bit stereo from libultra audio thread).

constexpr int input_channels  = 2;
constexpr int output_channels = 2;
constexpr int bytes_per_frame = output_channels * sizeof(float);
// Frames duplicated at chunk boundaries to avoid resampling discontinuities.
constexpr size_t duplicated_input_frames = 4;

static SDL_AudioDeviceID audio_device = 0;
static SDL_AudioCVT audio_convert{};
static uint32_t sample_rate        = 32000;
static uint32_t output_sample_rate = 48000;
static uint32_t discarded_output_frames = 0;

// ── Round-2 clock-domain bridge ─────────────────────────────────────
// One persistent band-limited resampler + fill controller, pulled by a real
// SDL audio callback. Replaces both crackle sources in the legacy push path:
// the per-chunk SDL_AudioCVT (boundary discontinuities) and the skip_factor
// sample-decimation valve (hard sample drops when the queue grew). The bridge
// resamples sample_rate->output_sample_rate continuously and never drops a
// sample; the game's osAiGetLength feedback reads the bridge fill instead of
// the raw SDL queue. Env PSR_AUDIO_BRIDGE=0 falls back to the legacy path for
// A/B measurement.
static rab_bridge   g_bridge;
static int          g_bridge_ready = 0;
static SDL_mutex   *g_audio_mtx    = nullptr;
static uint32_t     g_bridge_src   = 0;     // source rate the bridge was built at

static bool audio_bridge_enabled() {
    static const bool en = [] {
        const char* v = std::getenv("PSR_AUDIO_BRIDGE");
        return !(v && v[0] == '0' && v[1] == '\0');   // default ON
    }();
    return en;
}

// Audio thread: pull stereo frames from the bridge (int16) and convert to the
// device's F32 format. Never stalls on producer jitter.
static void psr_audio_cb(void* /*ud*/, Uint8* stream, int len) {
    int frames = len / (int)(output_channels * sizeof(float));
    if (!g_bridge_ready) { SDL_memset(stream, 0, (size_t)len); return; }
    // Callback-delivery timing (Experiment 1): a clean buffer can still arrive
    // late, which the OS papers over with a glitch the T3 content tap can't see.
    if (recomp_audio_debug_enabled()) {
        double now = recomp_audio_debug_now_ms();
        static double s_prev = -1.0;
        double expected = frames * 1000.0 / (output_sample_rate > 0 ? output_sample_rate : 48000);
        if (s_prev >= 0.0) {
            double delta = now - s_prev;
            recomp_audio_debug_eventf("cbk", "frames=%d delta_ms=%.2f exp_ms=%.2f late_ms=%.2f",
                                      frames, delta, expected, delta - expected);
        }
        s_prev = now;
    }
    static std::vector<int16_t> tmp;
    if ((int)tmp.size() < frames * output_channels) tmp.resize((size_t)frames * output_channels);
    SDL_LockMutex(g_audio_mtx);
    rab_pull(&g_bridge, tmp.data(), frames);
    SDL_UnlockMutex(g_audio_mtx);
    float* out = reinterpret_cast<float*>(stream);
    for (int i = 0; i < frames * output_channels; ++i)
        out[i] = (float)tmp[i] * (1.0f / 32768.0f);
    recomp_audio_debug_push_i16("t3_bridge_out", tmp.data(), frames,
                                (double)output_sample_rate, output_channels);
}

// Fill `frames` of interleaved F32 stereo from the bridge — the shared source for
// BOTH the SDL callback and the native-WASAPI render path (audio_host_probe.cpp).
// Mirrors psr_audio_cb's pull+convert so the two output backends are byte-identical.
extern "C" void psr_audio_fill_f32(float* out, int frames) {
    if (!out || frames <= 0) return;
    if (!g_bridge_ready || !g_audio_mtx) {
        for (int i = 0; i < frames * (int)output_channels; ++i) out[i] = 0.0f;
        return;
    }
    static thread_local std::vector<int16_t> tmp;
    if ((int)tmp.size() < frames * (int)output_channels)
        tmp.resize((size_t)frames * output_channels);
    SDL_LockMutex(g_audio_mtx);
    rab_pull(&g_bridge, tmp.data(), frames);
    SDL_UnlockMutex(g_audio_mtx);
    for (int i = 0; i < frames * (int)output_channels; ++i)
        out[i] = (float)tmp[i] * (1.0f / 32768.0f);
    recomp_audio_debug_push_i16("t3_bridge_out", tmp.data(), frames,
                                (double)output_sample_rate, output_channels);
}

// Build (or rebuild on rate change) the bridge for the current sample_rate.
static void bridge_reinit_locked() {
    if (g_bridge_ready) { rab_free(&g_bridge); g_bridge_ready = 0; }
    rab_config rc; rab_config_defaults(&rc);
    rc.channels       = output_channels;
    rc.source_rate    = (double)sample_rate;
    rc.host_rate      = (double)output_sample_rate;
    rc.target_ms      = 80.0;       // a touch deeper than NES: N64 chunks are larger
    rc.ring_ms        = 300.0;
    rc.max_correction = 0.015;
    g_bridge_ready = (rab_init(&g_bridge, &rc) == 0);
    g_bridge_src   = sample_rate;
}

static void update_audio_converter() {
    int ret = SDL_BuildAudioCVT(&audio_convert, AUDIO_F32, input_channels, sample_rate,
                                AUDIO_F32, output_channels, output_sample_rate);
    if (ret < 0) {
        fprintf(stderr, "Error creating SDL audio converter: %s\n", SDL_GetError());
        throw std::runtime_error("Error creating SDL audio converter");
    }
    discarded_output_frames = static_cast<uint32_t>(duplicated_input_frames * output_sample_rate / sample_rate);
}

// ── Always-on host audio-queue ring ─────────────────────────────────
// Records one event per queue_samples() call so the host-side output
// path can be inspected for the music-rate click. Captures the SDL
// queue depth and whether the skip_factor sample-decimation fired
// (which drops samples mid-waveform => discontinuity at chunk
// boundaries). Per the project ring rule this is always-on; query
// backward via the `audio_queue_recent` debug command. Env-gate
// PSR_DISABLE_AUDIO_QUEUE_RING=1 to disable.
namespace {
struct AudioQueueEvent {
    uint64_t seq;
    uint64_t ms;
    uint32_t sample_count;   // input int16 samples this chunk (L+R interleaved)
    uint32_t sample_rate;    // current input sample rate
    uint32_t queued_us;      // SDL queue depth (microseconds) before this queue
    uint32_t skip_factor;    // 0 = no decimation; >0 = drop (1<<skip_factor):1
    uint32_t bytes_queued;   // bytes actually handed to SDL_QueueAudio
    uint32_t decimated;      // 1 if skip_factor != 0 (samples dropped this chunk)
};
constexpr size_t AQ_RING_CAP = 8192;
AudioQueueEvent g_aq_ring[AQ_RING_CAP];
std::atomic<uint64_t> g_aq_seq{0};
std::mutex g_aq_mtx;
std::chrono::steady_clock::time_point g_aq_t0 = std::chrono::steady_clock::now();
bool g_aq_enabled = [] {
    const char* v = std::getenv("PSR_DISABLE_AUDIO_QUEUE_RING");
    return !(v && v[0] != '\0' && v[0] != '0');
}();

void aq_record(uint32_t sample_count, uint32_t srate, uint32_t queued_us,
               uint32_t skip_factor, uint32_t bytes_queued) {
    if (!g_aq_enabled) return;
    std::lock_guard<std::mutex> lk(g_aq_mtx);
    const uint64_t s = g_aq_seq.load(std::memory_order_relaxed);
    AudioQueueEvent& e = g_aq_ring[s % AQ_RING_CAP];
    e.seq = s;
    e.ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - g_aq_t0).count();
    e.sample_count = sample_count;
    e.sample_rate = srate;
    e.queued_us = queued_us;
    e.skip_factor = skip_factor;
    e.bytes_queued = bytes_queued;
    e.decimated = skip_factor != 0 ? 1u : 0u;
    g_aq_seq.store(s + 1, std::memory_order_release);
}
}  // namespace

extern "C" void recomp_audio_queue_recent_copy(
    void* out_void, size_t cap, size_t* n_written, uint64_t* next_seq_out)
{
    std::lock_guard<std::mutex> lk(g_aq_mtx);
    const uint64_t s = g_aq_seq.load(std::memory_order_relaxed);
    if (next_seq_out) *next_seq_out = s;
    if (cap == 0 || out_void == nullptr) { if (n_written) *n_written = 0; return; }
    const size_t available = (s < AQ_RING_CAP) ? size_t(s) : AQ_RING_CAP;
    const size_t want = (cap < available) ? cap : available;
    AudioQueueEvent* out = static_cast<AudioQueueEvent*>(out_void);
    const size_t start = (s - want) % AQ_RING_CAP;
    for (size_t i = 0; i < want; i++) out[i] = g_aq_ring[(start + i) % AQ_RING_CAP];
    if (n_written) *n_written = want;
}

extern "C" size_t recomp_audio_queue_event_size(void) {
    return sizeof(AudioQueueEvent);
}

// ── Always-on synthesized-PCM ring ──────────────────────────────────
// Records one event per queue_samples() call capturing the RAW int16
// audio the game synthesized, BEFORE any host conversion/volume. Used to
// localize the "static" artifact: high sample-to-sample jaggedness
// (large 2nd-differences relative to amplitude) means the static is baked
// into the synthesized PCM (aspMain / mixing), not introduced host-side.
// Metrics are computed on ONE channel (even indices) so L/R interleave
// doesn't inflate the diffs. Query backward via `audio_pcm_recent`.
// Env-gate PSR_DISABLE_AUDIO_PCM_RING=1.
namespace {
constexpr size_t PCM_WINDOW = 32;  // raw int16 one-channel snapshot
struct AudioPcmEvent {
    uint64_t seq;
    uint64_t ms;
    uint32_t sample_count;   // int16 samples this chunk (L+R interleaved)
    int32_t  min_sample;
    int32_t  max_sample;
    uint32_t mean_abs;       // mean |x| (one channel) — signal level reference
    uint32_t mean_abs_d1;    // mean |x[n]-x[n-1]|  (consecutive frames)
    uint32_t max_abs_d1;
    uint32_t mean_abs_d2;    // mean |x[n]-2x[n-1]+x[n-2]| — HF energy / static signature
    uint32_t max_abs_d2;
    uint32_t out_hf_milli;   // (mean|d2|/mean|x|)*1000 on the RESAMPLED output (one ch)
    int16_t  window[PCM_WINDOW];
};
constexpr size_t APCM_RING_CAP = 4096;
AudioPcmEvent g_apcm_ring[APCM_RING_CAP];
std::atomic<uint64_t> g_apcm_seq{0};
std::mutex g_apcm_mtx;
bool g_apcm_enabled = [] {
    const char* v = std::getenv("PSR_DISABLE_AUDIO_PCM_RING");
    return !(v && v[0] != '\0' && v[0] != '0');
}();

inline uint32_t iabs32(int32_t v) { return (uint32_t)(v < 0 ? -v : v); }

void apcm_record(const int16_t* audio_data, size_t sample_count) {
    if (!g_apcm_enabled || audio_data == nullptr || sample_count < 6) return;
    const size_t frames = sample_count / 2;  // one channel = even indices
    int32_t mn = 32767, mx = -32768;
    uint64_t sum_abs = 0, sum_d1 = 0, sum_d2 = 0;
    uint32_t max_d1 = 0, max_d2 = 0;
    for (size_t f = 0; f < frames; f++) {
        int32_t x = audio_data[2 * f];
        if (x < mn) mn = x;
        if (x > mx) mx = x;
        sum_abs += iabs32(x);
        if (f >= 1) {
            uint32_t d1 = iabs32(x - (int32_t)audio_data[2 * (f - 1)]);
            sum_d1 += d1; if (d1 > max_d1) max_d1 = d1;
        }
        if (f >= 2) {
            uint32_t d2 = iabs32(x - 2 * (int32_t)audio_data[2 * (f - 1)]
                                   + (int32_t)audio_data[2 * (f - 2)]);
            sum_d2 += d2; if (d2 > max_d2) max_d2 = d2;
        }
    }
    std::lock_guard<std::mutex> lk(g_apcm_mtx);
    const uint64_t s = g_apcm_seq.load(std::memory_order_relaxed);
    AudioPcmEvent& e = g_apcm_ring[s % APCM_RING_CAP];
    e.seq = s;
    e.ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - g_aq_t0).count();
    e.sample_count = (uint32_t)sample_count;
    e.min_sample = mn;
    e.max_sample = mx;
    e.mean_abs    = frames     ? (uint32_t)(sum_abs / frames)       : 0;
    e.mean_abs_d1 = frames > 1 ? (uint32_t)(sum_d1 / (frames - 1))  : 0;
    e.max_abs_d1  = max_d1;
    e.mean_abs_d2 = frames > 2 ? (uint32_t)(sum_d2 / (frames - 2))  : 0;
    e.max_abs_d2  = max_d2;
    for (size_t i = 0; i < PCM_WINDOW; i++)
        e.window[i] = (2 * i < sample_count) ? audio_data[2 * i] : (int16_t)0;
    e.out_hf_milli = 0;  // filled in by apcm_record_output after resampling
    g_apcm_seq.store(s + 1, std::memory_order_release);
}

// Second-pass: HF ratio of the RESAMPLED output buffer (float, one channel).
// Updates the most-recent event (same chunk; queue_samples runs serially on
// the audio thread). Lets us compare output HF vs the clean input HF — if the
// resampler injects HF (imaging/aliasing), out_hf >> in_hf.
void apcm_record_output(const float* out, size_t out_floats) {
    if (!g_apcm_enabled || out == nullptr || out_floats < 12) return;
    const size_t frames = out_floats / 2;
    double sum_abs = 0.0, sum_d2 = 0.0;
    for (size_t f = 0; f < frames; f++) {
        double x = out[2 * f];
        sum_abs += x < 0 ? -x : x;
        if (f >= 2) {
            double d2 = (double)out[2 * f] - 2.0 * out[2 * (f - 1)] + out[2 * (f - 2)];
            sum_d2 += d2 < 0 ? -d2 : d2;
        }
    }
    double mean_abs = frames     ? sum_abs / frames       : 0.0;
    double mean_d2  = frames > 2 ? sum_d2 / (frames - 2)   : 0.0;
    uint32_t hf_milli = mean_abs > 1e-9 ? (uint32_t)((mean_d2 / mean_abs) * 1000.0) : 0;
    std::lock_guard<std::mutex> lk(g_apcm_mtx);
    const uint64_t s = g_apcm_seq.load(std::memory_order_relaxed);
    if (s == 0) return;
    g_apcm_ring[(s - 1) % APCM_RING_CAP].out_hf_milli = hf_milli;
}
}  // namespace

extern "C" void recomp_audio_pcm_recent_copy(
    void* out_void, size_t cap, size_t* n_written, uint64_t* next_seq_out)
{
    std::lock_guard<std::mutex> lk(g_apcm_mtx);
    const uint64_t s = g_apcm_seq.load(std::memory_order_relaxed);
    if (next_seq_out) *next_seq_out = s;
    if (cap == 0 || out_void == nullptr) { if (n_written) *n_written = 0; return; }
    const size_t available = (s < APCM_RING_CAP) ? size_t(s) : APCM_RING_CAP;
    const size_t want = (cap < available) ? cap : available;
    AudioPcmEvent* out = static_cast<AudioPcmEvent*>(out_void);
    const size_t start = (s - want) % APCM_RING_CAP;
    for (size_t i = 0; i < want; i++) out[i] = g_apcm_ring[(start + i) % APCM_RING_CAP];
    if (n_written) *n_written = want;
}

extern "C" size_t recomp_audio_pcm_event_size(void) {
    return sizeof(AudioPcmEvent);
}

extern "C" size_t recomp_audio_pcm_window(void) {
    return PCM_WINDOW;
}

static void queue_samples(int16_t* audio_data, size_t sample_count) {
    apcm_record(audio_data, sample_count);

    // T1: raw emulator-rate PCM exactly as the game produced it (R,L order; the
    // swap is irrelevant for discontinuity detection).
    recomp_audio_debug_push_i16("t1_raw", audio_data,
                                (int)(sample_count / input_channels),
                                (double)sample_rate, input_channels);

    // Headless capture: RECOMP_AUDIO_DEBUG_DUMP_SECS=N dumps + exits after N
    // seconds of audio time (data collection, not an ear test).
    if (recomp_audio_debug_enabled()) {
        static double s_dump_secs = -2.0;
        static double s_audio_secs = 0.0;
        if (s_dump_secs == -2.0) {
            const char* e = std::getenv("RECOMP_AUDIO_DEBUG_DUMP_SECS");
            s_dump_secs = (e && *e) ? atof(e) : -1.0;
        }
        if (s_dump_secs > 0.0 && sample_rate > 0) {
            s_audio_secs += (double)(sample_count / input_channels) / (double)sample_rate;
            if (s_audio_secs >= s_dump_secs) {
                recomp_audio_debug_dump(0.0, nullptr);
                fprintf(stderr, "[audio-debug] dumped after %.1fs audio; exiting\n", s_audio_secs);
                fflush(stderr);
                std::exit(0);
            }
        }
    }

    // ── Round-2 bridge path ─────────────────────────────────────────
    // Push de-swapped, volume-scaled int16 stereo straight into the bridge.
    // The bridge resamples to the device rate continuously (no SDL_AudioCVT)
    // and the audio callback drains it (no decimation valve).
    if (audio_bridge_enabled()) {
        size_t frames = sample_count / input_channels;
        if (frames == 0) return;
        static std::vector<int16_t> push_buf;
        if (push_buf.size() < frames * output_channels) push_buf.resize(frames * output_channels);
        // libultra interleaves R,L per word; emit L,R. Keep the legacy 0.5
        // headroom so levels match the old path.
        // RECOMP_AUDIO_SYNTH=sine|square|impulse|silence replaces the game audio
        // with a known-clean test signal through the EXACT same path. If a pure
        // sine crackles, the defect is the path, not the game's HLE audio.
        static int s_synth = -2;
        static uint64_t s_synth_pos = 0;
        if (s_synth == -2) s_synth = recomp_audio_synth_mode();
        if (s_synth != RAD_SYNTH_OFF) {
            recomp_audio_synth_fill(s_synth, push_buf.data(), (int)frames,
                                    output_channels, (double)sample_rate, &s_synth_pos);
        } else {
            const float scale = 0.5f;
            for (size_t f = 0; f < frames; ++f) {
                int l = (int)lrintf((float)audio_data[f * input_channels + 1] * scale);
                int r = (int)lrintf((float)audio_data[f * input_channels + 0] * scale);
                if (l >  32767) l =  32767; if (l < -32768) l = -32768;
                if (r >  32767) r =  32767; if (r < -32768) r = -32768;
                push_buf[f * output_channels + 0] = (int16_t)l;
                push_buf[f * output_channels + 1] = (int16_t)r;
            }
        }
        recomp_audio_debug_push_i16("t2_bridge_in", push_buf.data(), (int)frames,
                                    (double)sample_rate, output_channels);
        if (g_bridge_ready) {
            SDL_LockMutex(g_audio_mtx);
            if (g_bridge_src != sample_rate) bridge_reinit_locked();
            rab_push(&g_bridge, push_buf.data(), (int)frames);
            double fill = rab_fill_ms(&g_bridge);
            rab_stats st; rab_get_stats(&g_bridge, &st);
            SDL_UnlockMutex(g_audio_mtx);
            recomp_audio_debug_eventf("bfill", "fill_ms=%.1f under=%llu over=%llu src=%u",
                                      fill, (unsigned long long)st.underrun_events,
                                      (unsigned long long)st.overflow_drops, sample_rate);
        }
        return;
    }

    static std::vector<float> swap_buffer;
    static std::array<float, duplicated_input_frames * input_channels> duplicated_sample_buffer{};

    size_t resampled_sample_count = sample_count + duplicated_input_frames * input_channels;
    size_t max_sample_count = std::max(resampled_sample_count, resampled_sample_count * size_t(audio_convert.len_mult));
    if (max_sample_count > swap_buffer.size()) {
        swap_buffer.resize(max_sample_count);
    }

    // Carry over duplicated frames from the previous chunk.
    for (size_t i = 0; i < duplicated_input_frames * input_channels; i++) {
        swap_buffer[i] = duplicated_sample_buffer[i];
    }

    // Convert int16→float and swap stereo (libultra interleaves R,L per word).
    for (size_t i = 0; i < sample_count; i += input_channels) {
        swap_buffer[i + 0 + duplicated_input_frames * input_channels] = audio_data[i + 1] * (0.5f / 32768.0f);
        swap_buffer[i + 1 + duplicated_input_frames * input_channels] = audio_data[i + 0] * (0.5f / 32768.0f);
    }

    if (sample_count <= duplicated_input_frames * input_channels) {
        // Chunk too small to reseed duplicated buffer; skip queueing this chunk.
        return;
    }
    for (size_t i = 0; i < duplicated_input_frames * input_channels; i++) {
        duplicated_sample_buffer[i] = swap_buffer[i + sample_count];
    }

    audio_convert.buf = reinterpret_cast<Uint8*>(swap_buffer.data());
    audio_convert.len = static_cast<int>((sample_count + duplicated_input_frames * input_channels) * sizeof(swap_buffer[0]));

    if (SDL_ConvertAudio(&audio_convert) < 0) {
        fprintf(stderr, "Error using SDL audio converter: %s\n", SDL_GetError());
        return;
    }

    uint64_t cur_queued_microseconds = uint64_t(SDL_GetQueuedAudioSize(audio_device)) / bytes_per_frame * 1000000 / sample_rate;
    uint32_t num_bytes_to_queue = audio_convert.len_cvt - output_channels * discarded_output_frames * sizeof(swap_buffer[0]);
    float* samples_to_queue = swap_buffer.data() + output_channels * discarded_output_frames / 2;

    // PSR_AUDIO_NO_DECIMATE=1: disable the skip_factor sample-decimation
    // entirely. Diagnostic lever for the music-rate click investigation —
    // lets us observe whether the SDL queue self-stabilizes (game's
    // osAiGetLength feedback loop) or grows unbounded (true clock
    // mismatch) when the decimation drain is removed.
    static const bool no_decimate = [] {
        const char* v = std::getenv("PSR_AUDIO_NO_DECIMATE");
        return v && v[0] != '\0' && v[0] != '0';
    }();
    uint32_t skip_factor = no_decimate ? 0u
        : static_cast<uint32_t>(cur_queued_microseconds / 100000);
    if (skip_factor != 0) {
        uint32_t skip_ratio = 1u << skip_factor;
        num_bytes_to_queue /= skip_ratio;
        for (size_t i = 0; i < num_bytes_to_queue / (output_channels * sizeof(swap_buffer[0])); i++) {
            samples_to_queue[2 * i + 0] = samples_to_queue[2 * skip_ratio * i + 0];
            samples_to_queue[2 * i + 1] = samples_to_queue[2 * skip_ratio * i + 1];
        }
    }

    aq_record((uint32_t)sample_count, sample_rate,
              (uint32_t)cur_queued_microseconds, skip_factor, num_bytes_to_queue);
    apcm_record_output(samples_to_queue, num_bytes_to_queue / sizeof(swap_buffer[0]));
    SDL_QueueAudio(audio_device, samples_to_queue, num_bytes_to_queue);
}

static size_t get_frames_remaining() {
    // Fast-forward: report an empty audio buffer so the game thread
    // keeps generating audio chunks (and advancing logic) without
    // waiting for playback. Real audio queues will desync but the
    // game runs as fast as the host can recompile + render.
    if (pkmnstadium::dbg::g_fast_forward.load()) {
        return 0;
    }
    // Bridge path: the game's AI_LEN pacing must read the BRIDGE fill (the real
    // buffer now), not the SDL queue (which the callback keeps near-empty).
    if (audio_bridge_enabled() && g_bridge_ready) {
        SDL_LockMutex(g_audio_mtx);
        double fill_ms = rab_fill_ms(&g_bridge);
        SDL_UnlockMutex(g_audio_mtx);
        double frames = fill_ms * 0.001 * (double)sample_rate;   // source frames buffered
        double lag    = (double)(sample_rate / 60);              // hold ~1 VI of slack
        double rem    = frames > lag ? frames - lag : 0.0;
        return (size_t)rem;
    }
    constexpr float buffer_offset_frames = 1.0f;
    uint64_t buffered_byte_count = SDL_GetQueuedAudioSize(audio_device);
    buffered_byte_count = buffered_byte_count * 2 * sample_rate / output_sample_rate / output_channels;
    uint32_t frames_per_vi = (sample_rate / 60);
    if (buffered_byte_count > uint64_t(buffer_offset_frames * bytes_per_frame * frames_per_vi)) {
        buffered_byte_count -= uint64_t(buffer_offset_frames * bytes_per_frame * frames_per_vi);
    } else {
        buffered_byte_count = 0;
    }
    return static_cast<size_t>(buffered_byte_count / bytes_per_frame);
}

static void set_frequency(uint32_t freq) {
    sample_rate = freq;
    update_audio_converter();
    if (audio_bridge_enabled() && audio_device != 0) {
        SDL_LockMutex(g_audio_mtx);
        bridge_reinit_locked();
        SDL_UnlockMutex(g_audio_mtx);
    }
}

// Pick the output device.
//
// Goal: follow the Windows *default* output endpoint, so the game plays
// through whatever the user has chosen — and, on the WASAPI backend that
// we force, live-migrates when they change the default mid-game (open with
// nullptr to get that behavior). The one trap is a game-controller endpoint
// (e.g. a DualSense's built-in speaker) becoming the system default: that
// routes game audio into the controller (tinny "static", inaudible on real
// speakers). So:
//   1. PSR_AUDIO_DEVICE=<name substring> set  -> pin that named device.
//   2. system default is NOT a controller     -> open SDL default (nullptr),
//                                                 which follows + live-migrates.
//   3. system default IS a controller         -> fall back to the first
//                                                 non-controller named device.
// Returning nullptr means "let SDL track the system default."
static const char* select_audio_device() {
    const int count = SDL_GetNumAudioDevices(0 /* iscapture=0 -> output */);
    if (count <= 0) return nullptr;  // can't enumerate — use SDL default

    auto low = [](std::string s) {
        for (auto& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        return s;
    };
    auto contains_ci = [&](const char* hay, const char* needle) {
        if (!hay || !needle) return false;
        return low(hay).find(low(needle)) != std::string::npos;
    };

    static const char* kAvoid[] = {
        "dualsense", "dualshock", "wireless controller", "ps5", "ps4",
        "steam streaming",
    };
    auto is_controller_endpoint = [&](const char* name) {
        for (const char* a : kAvoid) if (contains_ci(name, a)) return true;
        return false;
    };

    fprintf(stderr, "[PSR] audio output devices:\n");
    for (int i = 0; i < count; i++) {
        const char* nm = SDL_GetAudioDeviceName(i, 0);
        fprintf(stderr, "[PSR]   [%d] %s\n", i, nm ? nm : "(null)");
    }

    // (1) Explicit pin always wins. Resolved as PSR_AUDIO_DEVICE > launcher.cfg
    // `audio_device` (Settings > Audio output) > empty (follow system default).
    const std::string want_s = pkmnstadium::ui_seam::startup_audio_device();
    const char* want = want_s.empty() ? nullptr : want_s.c_str();
    if (want && want[0]) {
        for (int i = 0; i < count; i++) {
            const char* name = SDL_GetAudioDeviceName(i, 0);
            if (name && contains_ci(name, want)) {
                fprintf(stderr, "[PSR] -> audio device: \"%s\" (PSR_AUDIO_DEVICE match)\n", name);
                return name;
            }
        }
        fprintf(stderr, "[PSR] PSR_AUDIO_DEVICE=\"%s\" matched no device; "
                        "falling back to system default\n", want);
    }

    // (2) Follow the Windows default endpoint unless it's a controller speaker.
    char* def_name = nullptr;
    SDL_AudioSpec def_spec{};  // some SDL backends reject a null spec ptr
    if (SDL_GetDefaultAudioInfo(&def_name, &def_spec, 0) == 0 && def_name) {
        const bool def_is_controller = is_controller_endpoint(def_name);
        fprintf(stderr, "[PSR] system default output: \"%s\"%s\n", def_name,
                def_is_controller ? " (controller endpoint — avoiding)" : "");
        SDL_free(def_name);
        if (!def_is_controller) {
            fprintf(stderr, "[PSR] -> audio device: SDL default "
                            "(follows Windows default endpoint, live-migrates)\n");
            return nullptr;
        }
    } else {
        fprintf(stderr, "[PSR] SDL_GetDefaultAudioInfo failed (%s); "
                        "checking enumerated devices\n", SDL_GetError());
    }

    // (3) Default is a controller (or unknown): first non-controller device.
    for (int i = 0; i < count; i++) {
        const char* name = SDL_GetAudioDeviceName(i, 0);
        if (name && !is_controller_endpoint(name)) {
            fprintf(stderr, "[PSR] -> audio device: \"%s\" "
                            "(auto: skipped controller default)\n", name);
            return name;
        }
    }

    fprintf(stderr, "[PSR] -> audio device: SDL default "
                    "(no non-controller device found)\n");
    return nullptr;  // nothing better — let SDL pick
}

// WASAPI host-boundary probes (audio_host_probe.cpp): (1) log the TRUE engine mix
// format — SDL's `obtained` can hide a shared-mode resample; (2) opt-in T5 loopback
// tap (PSR_AUDIO_T5_LOOPBACK=1) that captures what Windows sends the endpoint.
extern "C" void psr_log_wasapi_mix_format(void);
extern "C" void psr_audio_t5_start(void);
// Native WASAPI render path (cpal-equivalent, audio_host_probe.cpp). Opt-in
// alternative to SDL2 audio output via PSR_AUDIO_WASAPI=1.
extern "C" int      psr_audio_wasapi_enabled(void);
extern "C" int      psr_audio_wasapi_start(void);
extern "C" unsigned psr_query_wasapi_mix_rate(void);

static void reset_audio(uint32_t output_freq) {
    bool use_bridge = audio_bridge_enabled();
    // ---- Native WASAPI render path (cpal-style: event-driven, shared-mode, native
    // mix format, default engine buffer, Pro-Audio thread). Bypasses SDL2 audio so
    // we can A/B against it by ear. The bridge still produces the PCM; only the
    // host handoff changes. ----
    if (use_bridge && psr_audio_wasapi_enabled()) {
        psr_log_wasapi_mix_format();
        unsigned mr = psr_query_wasapi_mix_rate();
        output_sample_rate = mr ? mr : output_freq;
        update_audio_converter();
        if (!g_audio_mtx) g_audio_mtx = SDL_CreateMutex();
        SDL_LockMutex(g_audio_mtx);
        bridge_reinit_locked();          // host_rate == engine mix rate
        SDL_UnlockMutex(g_audio_mtx);
        recomp_audio_debug_init();
        psr_audio_t5_start();
        if (psr_audio_wasapi_start()) {
            fprintf(stderr, "[PSR][audio] NATIVE WASAPI render ON  src=%u -> host=%u  ready=%d\n",
                    sample_rate, output_sample_rate, g_bridge_ready);
            return;  // do NOT open SDL audio output
        }
        fprintf(stderr, "[PSR][audio] WASAPI render failed to start — falling back to SDL\n");
        // fall through to the SDL path below
    }
    SDL_AudioSpec spec_desired{};
    spec_desired.freq    = (int)output_freq;
    spec_desired.format  = AUDIO_F32;
    spec_desired.channels = (Uint8)output_channels;
    // Callback buffer must exceed the WASAPI shared-mode engine period (~10 ms) so
    // the OS isn't underfed every period (that underfeed was the perpetual crackle:
    // 256 frames = 5.33 ms @48k sat UNDER the period; measured callbacks paced at
    // ~9.4 ms with 53% "late"). But it must also stay BELOW the bridge fill so a
    // single pull never drains the ring (1024 = 21.3 ms underran the ~27 ms fill).
    // 512 frames = 10.67 ms @48k: above the engine period, below the fill. Matches
    // the value NES uses (512 @44.1k = 11.6 ms), which is clean.
    spec_desired.samples = 0x200;   /* 512 frames */
    spec_desired.callback = use_bridge ? psr_audio_cb : nullptr;

    // Close any previously-opened device first (so reset_audio can be re-invoked
    // to switch devices — e.g. the launcher's Settings > Audio output — without
    // leaking the prior handle).
    if (audio_device != 0) {
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }

    const char* dev_name = select_audio_device();
    // CRITICAL (round-2 N64 crackle fix): read the OBTAINED spec and allow the
    // frequency to change to the device/endpoint native rate. The old call passed
    // obtained=nullptr + flags=0, which forced SDL to accept our hardcoded 48000
    // and do its OWN internal conversion to the real endpoint rate (e.g. 44100) --
    // a SECOND resample on top of the bridge's. We must target the rate the host
    // actually opened so there is exactly ONE resampler (the bridge). Channels and
    // format stay fixed (F32 stereo == typical WASAPI mix format -> no conversion).
    SDL_AudioSpec obtained{};
    audio_device = SDL_OpenAudioDevice(dev_name, false, &spec_desired, &obtained,
                                       SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (audio_device == 0) {
        fprintf(stderr, "SDL error opening audio device: %s\n", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }
    // Target everything at the rate SDL actually gave us, never the hardcoded one.
    output_sample_rate = (obtained.freq > 0) ? (uint32_t)obtained.freq : output_freq;
    update_audio_converter();
    // Report the TRUE WASAPI engine mix format + device period. If this rate differs
    // from `obtained` above, Windows is doing a hidden shared-mode resample (the
    // double-resample suspect) — target it to eliminate the second SRC.
    psr_log_wasapi_mix_format();
    fprintf(stderr, "[PSR][audio] driver=%s  desired=%dHz F32 %dch samp=%d  "
            "obtained=%dHz fmt=0x%04x %dch samp=%d\n",
            SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "?",
            spec_desired.freq, spec_desired.channels, spec_desired.samples,
            obtained.freq, obtained.format, obtained.channels, obtained.samples);
    if (use_bridge) {
        if (!g_audio_mtx) g_audio_mtx = SDL_CreateMutex();
        SDL_LockMutex(g_audio_mtx);
        bridge_reinit_locked();          // uses output_sample_rate == obtained.freq
        SDL_UnlockMutex(g_audio_mtx);
        recomp_audio_debug_init();
        fprintf(stderr, "[PSR][audio] bridge ON  src=%u -> host=%u (obtained)  ready=%d\n",
                sample_rate, output_sample_rate, g_bridge_ready);
    }
    // Start the T5 WASAPI-loopback tap if requested (needs RECOMP_AUDIO_DEBUG too).
    // Captures exactly what Windows sends the endpoint — the first tap past T3.
    psr_audio_t5_start();
    SDL_PauseAudioDevice(audio_device, 0);
}

// ---- Window / gfx callbacks (SDL2-backed) ----------------------------------

static SDL_Window* g_window = nullptr;

// Non-static alias to the SDL window, consumed by the SS Anne UI overlay
// (src/main/ui_seam.cpp) and by recompui's ui_state.cpp in Phase 1, both of
// which reference a global `SDL_Window* window`. Assigned in create_window().
SDL_Window* window = nullptr;

static ultramodern::gfx_callbacks_t::gfx_data_t create_gfx() {
    fprintf(stderr, "[PSR] create_gfx: SDL_InitSubSystem(VIDEO)\n"); fflush(stderr);
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    fprintf(stderr, "[PSR] create_gfx: done\n"); fflush(stderr);
    return {};
}

static ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t) {
    fprintf(stderr, "[PSR] create_window: SDL_CreateWindow\n"); fflush(stderr);
    g_window = SDL_CreateWindow(
        "Pokemon Stadium (PokemonStadiumRecomp)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        960, 720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN
    );
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }
    window = g_window;
    fprintf(stderr, "[PSR] create_window: ShowWindow\n"); fflush(stderr);
    SDL_ShowWindow(g_window);

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(g_window, &wmInfo);
#if defined(_WIN32)
    return ultramodern::renderer::WindowHandle{ wmInfo.info.win.window, GetCurrentThreadId() };
#elif defined(__linux__)
    return ultramodern::renderer::WindowHandle{ wmInfo.info.x11.window };
#elif defined(__APPLE__)
    return ultramodern::renderer::WindowHandle{ wmInfo.info.cocoa.window, nullptr };
#endif
}

// TAB-hold turbo state. While TAB is pressed, g_fast_forward is forced
// ON regardless of the env-var-driven persistent default. On release,
// the persistent state is restored. Lets the user run at real-time
// for reading text/cinematics and momentarily skip with TAB.
//
// Persistent state is set during init from PSR_TURBO env var; only
// the TAB key driver mutates it back to the persistent value here.
// TCP `fast_forward` toggles still update g_fast_forward directly,
// which is consistent so long as TAB isn't being held at toggle time.
static std::atomic<bool> s_turbo_persistent{false};

static void update_gfx(void*) {
    // Open all game controllers once while the launcher is up, so SDL delivers
    // their button/axis events for menu navigation (the game otherwise only
    // opens controllers after boot).
    if (pkmnstadium::ui_seam::launcher_visible()) {
        static bool s_launcher_pads_opened = false;
        if (!s_launcher_pads_opened) {
            s_launcher_pads_opened = true;
            SDL_GameControllerUpdate();
            const int njoy = SDL_NumJoysticks();
            for (int i = 0; i < njoy; ++i) {
                if (SDL_IsGameController(i)) SDL_GameControllerOpen(i);
            }
        }
    }
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            // Clean-but-fast close. The full shutdown (recomp's join sequence)
            // can deadlock on game_thread.join()/RT64/static destructors — that's
            // the "hang on close". But we must NOT just _Exit: the N64 save
            // (FlashRAM/SRAM/EEPROM — e.g. Stadium's registered teams) is written
            // by a background thread that coalesces writes for up to ~1.3s, so a
            // bare _Exit can lose a just-made save.
            //
            // So: signal quit (exited=true), then join the saving thread, which
            // flushes any pending save and returns quickly; THEN hard-exit past
            // the joins that hang. Transfer Pak (.sav) writes are already
            // write-through (transfer_pak.cpp flushes after every block).
            ultramodern::quit();
            ultramodern::join_saving_thread();
            std::fflush(stdout);
            std::fflush(stderr);
            std::_Exit(0);
        }
        // While the SS Anne launcher is the active screen, route input to it
        // (mouse/keyboard for clicking PLAY, toggles, etc.).
        if (pkmnstadium::ui_seam::launcher_visible()) {
            pkmnstadium::ui_seam::queue_event(ev);
            continue;
        }
        if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_TAB &&
            ev.key.repeat == 0) {
            pkmnstadium::dbg::g_fast_forward.store(true);
        } else if (ev.type == SDL_KEYUP && ev.key.keysym.sym == SDLK_TAB) {
            pkmnstadium::dbg::g_fast_forward.store(
                s_turbo_persistent.load());
        } else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_F9 &&
                   ev.key.repeat == 0) {
            // Audio observability: dump the trailing ring window NOW (press at
            // the moment crackle is heard). No-op unless RECOMP_AUDIO_DEBUG is set.
            if (recomp_audio_debug_enabled()) {
                recomp_audio_debug_dump(0.0, nullptr);
                std::fprintf(stderr, "[audio-debug] F9 dump written\n");
                std::fflush(stderr);
            }
        }
    }
}

// ---- Input (minimal SDL GameController-only; no gyro / no remap UI) --------
// Per principles: no stub. If a real controller isn't connected, polling
// returns zeroed input — the game runs as if the player is idle, which is
// real behavior, not a fake. Real input takes effect when a controller is
// plugged in and SDL_GameControllerOpen succeeds.

static SDL_GameController* g_pad = nullptr;

// ---- per-port input routing (driven by the SS Anne launcher) --------------
// When the launcher commits a config at PLAY, each N64 port (0..3) is routed to
// a specific device. Until then (or under PSR_AUTOBOOT) the legacy single-human
// model applies: port 0 = keyboard + first pad, ports 1-3 = Transfer-Pak idle.
static std::atomic<bool> g_launcher_input_active{false};
static pkmnstadium::ui_seam::DeviceKind g_port_kind[4] = {};
static int g_port_instance[4] = {-1, -1, -1, -1};
static bool g_port_enabled[4] = {false, false, false, false};
static SDL_GameController* g_port_pad[4] = {nullptr, nullptr, nullptr, nullptr};

// Snapshot the launcher's assignment into the per-port routing tables. Called
// once from the boot thread when PLAY is clicked. SDL handles are opened lazily
// in poll_inputs (on the gfx thread that owns the subsystem).
static void apply_launcher_input_config() {
    for (int port = 0; port < 4; ++port) {
        const auto pa = pkmnstadium::ui_seam::port_assignment(port);
        g_port_enabled[port] = pa.enabled;
        g_port_kind[port] = pa.enabled ? pa.kind : pkmnstadium::ui_seam::DeviceKind::None;
        g_port_instance[port] = pa.gamepad_instance;
    }
    g_launcher_input_active.store(true);
    std::fprintf(stderr, "[input] launcher config applied: "
                 "P1=%d P2=%d P3=%d P4=%d (0=None 1=Kbd 2=Pad)\n",
                 (int)g_port_kind[0], (int)g_port_kind[1],
                 (int)g_port_kind[2], (int)g_port_kind[3]);
    std::fflush(stderr);
}

// Open the SDL gamepad with the given instance id (find its device index).
static SDL_GameController* open_pad_by_instance(int instance) {
    // Reuse the handle if it's already open (e.g. opened for launcher nav).
    if (SDL_GameController* existing = SDL_GameControllerFromInstanceID(instance)) {
        return existing;
    }
    const int njoy = SDL_NumJoysticks();
    for (int i = 0; i < njoy; ++i) {
        if (SDL_IsGameController(i) && SDL_JoystickGetDeviceInstanceID(i) == instance) {
            return SDL_GameControllerOpen(i);
        }
    }
    return nullptr;
}

// Lazy-detect/open the first SDL gamepad. Safe to call from any of the
// input callbacks — both poll_inputs and get_connected_device_info
// invoke this so the device status is correct even when osContInit
// queries before the first poll cycle.
static void ensure_pad_open() {
    if (g_pad) return;
    // SDL only sees newly-attached pads after the event subsystem has
    // pumped at least once. get_connected_device_info() is called by
    // libultra's osContInit BEFORE the runner enters its frame loop
    // (which is what would normally call SDL_GameControllerUpdate),
    // so without an explicit pump SDL_NumJoysticks returns 0 and the
    // game reports "Controller 1 not connected" even with a pad
    // plugged in.
    SDL_PumpEvents();
    SDL_GameControllerUpdate();
    int njoy = SDL_NumJoysticks();
    for (int i = 0; i < njoy; i++) {
        if (SDL_IsGameController(i)) {
            g_pad = SDL_GameControllerOpen(i);
            if (g_pad) {
                const char* ctrl_name = SDL_GameControllerName(g_pad);
                fprintf(stderr,
                    "[input] OPENED controller: name='%s'\n",
                    ctrl_name ? ctrl_name : "(null)");
                fflush(stderr);
                return;
            }
        }
    }
}

static void poll_inputs() {
    SDL_GameControllerUpdate();
    if (g_launcher_input_active.load()) {
        // Lazily open each port's assigned gamepad (on this gfx thread).
        for (int port = 0; port < 4; ++port) {
            if (g_port_kind[port] == pkmnstadium::ui_seam::DeviceKind::Gamepad &&
                g_port_pad[port] == nullptr && g_port_instance[port] >= 0) {
                g_port_pad[port] = open_pad_by_instance(g_port_instance[port]);
            }
        }
    } else {
        ensure_pad_open();
    }
}

static bool get_n64_input(int controller_num, uint16_t* buttons_out, float* x_out, float* y_out) {
    *buttons_out = 0;
    *x_out = 0.0f;
    *y_out = 0.0f;

    // A controller "responds" to a read iff get_connected_device_info() reports
    // it connected. osContGetReadData() turns a `false` return into
    // CONT_NO_RESPONSE_ERROR. Transfer-Pak-only ports answer reads with idle
    // input (present, no live device).
    //
    // Pick the device driving this N64 port. In launcher mode each port is
    // routed explicitly by the SS Anne config; otherwise the legacy single-human
    // model applies (port 0 = keyboard + first pad, ports 1-3 = TP idle).
    SDL_GameController* pad = nullptr;
    bool use_kb = false;
    bool primary_port = false; // the port that receives the TCP debug override
    if (g_launcher_input_active.load()) {
        if (!g_port_enabled[controller_num]) {
            return false; // disabled slot: absent
        }
        switch (g_port_kind[controller_num]) {
            case pkmnstadium::ui_seam::DeviceKind::Gamepad:
                pad = g_port_pad[controller_num];
                break;
            case pkmnstadium::ui_seam::DeviceKind::Keyboard:
                use_kb = true;
                break;
            default:
                // No controller assigned: respond idle iff a Transfer Pak cart
                // is present (matches legacy TP-only port behavior).
                return pkmnstadium::transfer_pak::has_transfer_pak(controller_num);
        }
        primary_port = (controller_num == 0);
    } else {
        if (controller_num != 0) {
            return pkmnstadium::transfer_pak::has_transfer_pak(controller_num);
        }
        pad = g_pad;
        use_kb = true;
        primary_port = true;
    }

    // TCP override (debug harness) layers only onto the primary port.
    uint16_t override_buttons = 0;
    float override_x = 0.0f, override_y = 0.0f;
    if (primary_port && pkmnstadium::dbg::g_input_override_active.load()) {
        override_buttons = pkmnstadium::dbg::g_buttons_override.load();
        override_x = float(pkmnstadium::dbg::g_stick_x_override.load());
        override_y = float(pkmnstadium::dbg::g_stick_y_override.load());
    }

    // Diagnostic input log: when buttons change, print what we're
    // about to deliver. Lets us diff "what the user pressed" vs
    // "what we're telling Stadium," which is the cheapest way to
    // diagnose mis-routed buttons (e.g. Start triggering a reset
    // because we accidentally OR'd in another bit).
    static uint16_t s_last_buttons = 0;

    uint16_t b = 0;
    int32_t lx = 0, ly = 0;

    // All button + stick mapping now flows through the rebindable binding tables
    // (input_bindings.cpp), edited per device type via the launcher's per-port
    // rebind menu. The keyboard table is consulted iff a keyboard drives this
    // port (use_kb); the controller table iff a pad does. accumulate() ORs the
    // digital buttons into `b` and sums the analog-stick contribution into
    // lx/ly (raw, signed, pre-deadzone): keyboard keys give full deflection, pad
    // axes give proportional tilt — exactly what the old hardcoded path did.
    //
    // The default bindings reproduce the historical layout precisely (X->A,
    // Z->B, arrows->D-pad, Q/E->L/R, I/K/J/L->C, WASD->analog, LShift/Space->Z,
    // Enter->Start; pad A/B/Start/dpad, bumpers->L/R, triggers->Z, right
    // stick->C, left stick->analog — the issue-#8 layout), so a fresh build with
    // no input.cfg behaves identically.
    //
    // SDL_GetKeyboardState requires the VIDEO subsystem (initialized by
    // create_gfx). A pad and keyboard driving the same port still coexist
    // because their tables are OR'd together.
    const Uint8* ks = use_kb ? SDL_GetKeyboardState(nullptr) : nullptr;
    pkmnstadium::input::accumulate(pad, ks, &b, &lx, &ly);

    // Saturate the summed stick (a pad tilt plus keyboard keys can exceed the
    // axis range) before the deadzone rescale.
    if (lx >  32767) lx =  32767;
    if (lx < -32768) lx = -32768;
    if (ly >  32767) ly =  32767;
    if (ly < -32768) ly = -32768;

    *buttons_out = b;

    // Radial deadzone: Xbox One controller sticks rest at ~±300-1500
    // raw; without a deadzone, Stadium reads idle noise as "player
    // tilted the stick" and combines it with button presses — pressing
    // Start with even small drift gets interpreted as "cancel/skip"
    // rather than "confirm," which is what was making the title screen
    // soft-reset back to the boot sequence on any button press.
    //
    // 8000 raw matches the C-button deadzone; well above typical resting
    // drift but well below intentional tilts. Re-scale OUTSIDE the deadzone
    // so the player gets full N64 stick range from a partial Xbox tilt.
    constexpr int32_t LSTICK_DEADZONE = 8000;
    auto apply_deadzone = [](int32_t v) -> float {
        if (v >  LSTICK_DEADZONE) {
            return float(v - LSTICK_DEADZONE) / float(32767 - LSTICK_DEADZONE) * 80.0f;
        }
        if (v < -LSTICK_DEADZONE) {
            return float(v + LSTICK_DEADZONE) / float(32767 - LSTICK_DEADZONE) * 80.0f;
        }
        return 0.0f;
    };
    *x_out =  apply_deadzone(lx);
    *y_out = -apply_deadzone(ly);

    // Apply TCP override: OR buttons in, replace stick if non-zero.
    // claim_input arms the override flag (so osContInit reports
    // port-1 connected), but a harness that wants pure-TCP control
    // sets specific buttons via set_button. Otherwise keyboard/pad
    // pass through normally.
    *buttons_out |= override_buttons;
    if (override_x != 0.0f) *x_out = override_x;
    if (override_y != 0.0f) *y_out = override_y;

    // Diagnostic: log only when the button mask changes (don't flood
    // stderr every poll cycle). Useful for diagnosing
    // "press Start, game soft-resets" type bugs where we may be
    // delivering an extra button alongside Start.
    if (primary_port && *buttons_out != s_last_buttons) {
        fprintf(stderr,
            "[input] btn change: 0x%04X (was 0x%04X) "
            "stick=(%+.2f,%+.2f)%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
            *buttons_out, s_last_buttons, *x_out, *y_out,
            (*buttons_out & 0x8000) ? " A"      : "",
            (*buttons_out & 0x4000) ? " B"      : "",
            (*buttons_out & 0x2000) ? " Z"      : "",
            (*buttons_out & 0x1000) ? " START"  : "",
            (*buttons_out & 0x0800) ? " D-Up"   : "",
            (*buttons_out & 0x0400) ? " D-Down" : "",
            (*buttons_out & 0x0200) ? " D-Left" : "",
            (*buttons_out & 0x0100) ? " D-Right": "",
            (*buttons_out & 0x0020) ? " L"      : "",
            (*buttons_out & 0x0010) ? " R"      : "",
            (*buttons_out & 0x0008) ? " C-Up"   : "",
            (*buttons_out & 0x0004) ? " C-Down" : "",
            (*buttons_out & 0x0002) ? " C-Left" : "",
            (*buttons_out & 0x0001) ? " C-Right": "");
        fflush(stderr);
        s_last_buttons = *buttons_out;
    }

    return true;
}

static void set_rumble(int controller_num, bool on) {
    SDL_GameController* pad = nullptr;
    if (g_launcher_input_active.load()) {
        if (controller_num >= 0 && controller_num < 4) pad = g_port_pad[controller_num];
    } else if (controller_num == 0) {
        pad = g_pad;
    }
    if (!pad) return;
    SDL_GameControllerRumble(pad, on ? 0xFFFF : 0, on ? 0xFFFF : 0, 1000);
}

static ultramodern::input::connected_device_info_t get_connected_device_info(int controller_num) {
    const bool has_transfer_pak = pkmnstadium::transfer_pak::has_transfer_pak(controller_num);
    ultramodern::input::connected_device_info_t info{};

    // Launcher mode: a port is present iff its slot is enabled AND it has either
    // an assigned controller or a Transfer Pak cart. Disabled slots are absent,
    // which is how the launcher suppresses unconfigured players.
    if (g_launcher_input_active.load()) {
        const bool enabled = (controller_num >= 0 && controller_num < 4) && g_port_enabled[controller_num];
        const bool has_dev = enabled &&
            (g_port_kind[controller_num] != pkmnstadium::ui_seam::DeviceKind::None || has_transfer_pak);
        info.connected_device = has_dev
            ? ultramodern::input::Device::Controller
            : ultramodern::input::Device::None;
        info.connected_pak = (enabled && has_transfer_pak)
            ? ultramodern::input::Pak::RumblePak
            : ultramodern::input::Pak::None;
        return info;
    }

    // Legacy: libultra's osContInit() queries before the first poll, so
    // lazy-detect the pad here too (else "Controller 1 not connected").
    if (controller_num == 0) {
        ensure_pad_open();
    }
    // Port 0 always reports present (keyboard/pad/TCP all feed get_n64_input);
    // ports 1-3 present only when a Transfer Pak cart is configured.
    info.connected_device = (controller_num == 0 || has_transfer_pak)
        ? ultramodern::input::Device::Controller
        : ultramodern::input::Device::None;
    // Stadium identifies the *kind* of pak by reading its bus
    // signature (handled in transfer_pak.cpp's read/write shims for
    // __osContRamRead / __osContRamWrite); connected_pak is consumed
    // here only as a "something is plugged in" presence bit.
    //
    // ultramodern's Pak enum only has {None, RumblePak} -- there is no
    // first-class TransferPak value -- so we report RumblePak as the
    // presence stand-in. Stadium's own pak-type discrimination happens
    // over the bus, so it should see "Transfer Pak" despite the label.
    // If ultramodern ever grows Pak::TransferPak, switch to that.
    info.connected_pak = has_transfer_pak
        ? ultramodern::input::Pak::RumblePak
        : ultramodern::input::Pak::None;
    return info;
}

// ---- Crash diagnostics ----------------------------------------------------

#ifdef _WIN32
extern "C" const char* pkmnstadium_trace_at(uint64_t idx);
extern "C" uint64_t pkmnstadium_trace_write_idx(void);
extern "C" uint32_t pkmnstadium_trace_capacity(void);

// Forward decl — defined in post_mortem.cpp.
extern "C" void psr_post_mortem_dump(const char* reason,
                                     EXCEPTION_POINTERS* fault_info);

static LONG WINAPI psr_crash_filter(EXCEPTION_POINTERS* info) {
    // Write the compact stack log before the larger post-mortem dump.
    // If the process is already corrupt enough for the full JSON dump to
    // fault, this path still leaves a durable fault address and trace tail.
    const std::string err_log_path = pkmnstadium::app_file("last_error.log").string();
    FILE* f = fopen(err_log_path.c_str(), "a");
    if (f) {
        setvbuf(f, nullptr, _IONBF, 0);
        fprintf(f, "\n=== UNHANDLED EXCEPTION ===\n");
        fprintf(f, "  code:    0x%08lX\n", info->ExceptionRecord->ExceptionCode);
        fprintf(f, "  address: %p\n",      info->ExceptionRecord->ExceptionAddress);
        // Stack walk + symbol resolution so we know which function crashed.
        HANDLE proc = GetCurrentProcess();
        SymInitialize(proc, NULL, TRUE);
        CONTEXT* ctx = info->ContextRecord;
        STACKFRAME64 frame{};
        DWORD machine = IMAGE_FILE_MACHINE_AMD64;
        frame.AddrPC.Offset = ctx->Rip; frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = ctx->Rbp; frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = ctx->Rsp; frame.AddrStack.Mode = AddrModeFlat;
        char symbuf[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* sym = (SYMBOL_INFO*)symbuf;
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        IMAGEHLP_LINE64 line{}; line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        for (int i = 0; i < 20; i++) {
            if (!StackWalk64(machine, proc, GetCurrentThread(), &frame, ctx, NULL,
                             SymFunctionTableAccess64, SymGetModuleBase64, NULL)) break;
            if (!frame.AddrPC.Offset) break;
            DWORD64 disp64 = 0;
            DWORD disp32 = 0;
            const char* name = "?";
            if (SymFromAddr(proc, frame.AddrPC.Offset, &disp64, sym)) name = sym->Name;
            const char* file = "?"; DWORD lineno = 0;
            if (SymGetLineFromAddr64(proc, frame.AddrPC.Offset, &disp32, &line)) {
                file = line.FileName; lineno = line.LineNumber;
            }
            fprintf(f, "  #%02d 0x%016llX %s (%s:%lu)\n",
                i, (unsigned long long)frame.AddrPC.Offset, name, file, lineno);
        }
        if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
            && info->ExceptionRecord->NumberParameters >= 2) {
            uintptr_t fault_host = (uintptr_t)info->ExceptionRecord->ExceptionInformation[1];
            fprintf(f, "  access:  %s @ 0x%p\n",
                info->ExceptionRecord->ExceptionInformation[0] == 0 ? "read" :
                info->ExceptionRecord->ExceptionInformation[0] == 1 ? "write" : "execute",
                (void*)fault_host);
            uint8_t* rdram_base = recomp_runtime_get_rdram();
            if (rdram_base != nullptr) {
                intptr_t off = (intptr_t)(fault_host - (uintptr_t)rdram_base);
                uint32_t vaddr = (uint32_t)(0x80000000u + (uint32_t)off);
                fprintf(f, "  rdram_base: %p\n", (void*)rdram_base);
                fprintf(f, "  rdram_off:  0x%llX (%lld dec)\n",
                    (unsigned long long)(uint64_t)off, (long long)off);
                fprintf(f, "  decoded vaddr: 0x%08X\n", vaddr);
            }
        }
        // Dump last 32 trace ring entries — what the game thread was
        // executing right before the crash. Critical for diagnosis.
        uint64_t cap = (uint64_t)pkmnstadium_trace_capacity();
        uint64_t widx = pkmnstadium_trace_write_idx();
        fprintf(f, "  trace ring (write_idx=%llu, capacity=%llu):\n",
            (unsigned long long)widx, (unsigned long long)cap);
        uint64_t n = (widx < 32) ? widx : 32;
        for (uint64_t i = 0; i < n; i++) {
            uint64_t slot = (widx - n + i) % cap;
            const char* name = pkmnstadium_trace_at(slot);
            fprintf(f, "    %4llu: %s\n", (unsigned long long)slot, name ? name : "(null)");
        }
        fclose(f);
    }

    // Unified post-mortem dump (writes build/last_run_report.json).
    psr_post_mortem_dump("seh", info);
    return EXCEPTION_EXECUTE_HANDLER;
}

static LONG CALLBACK psr_vectored_exception_logger(EXCEPTION_POINTERS* info) {
    if (info == nullptr || info->ExceptionRecord == nullptr) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION &&
        code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_STACK_OVERFLOW) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    static volatile LONG logged = 0;
    if (InterlockedExchange(&logged, 1) == 0) {
        psr_crash_filter(info);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

// ---- Error handling -------------------------------------------------------

static void error_message_box(const char* msg) {
    // Always-on persistent error log — this fires before
    // ultramodern::error_handling::quick_exit() terminates the process,
    // and stderr buffering can swallow the message in headless runs.
    // Write a known file path so post-mortem inspection is easy.
    const std::string err_log_path = pkmnstadium::app_file("last_error.log").string();
    FILE* f = fopen(err_log_path.c_str(), "a");
    if (f) {
        fprintf(f, "[PSR ERROR] %s\n", msg);
        fclose(f);
    }
    fprintf(stderr, "[PSR ERROR] %s\n", msg);
    fflush(stderr);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "PokemonStadiumRecomp", msg, nullptr);
}

// ---- Game thread name ----------------------------------------------------

static std::string get_game_thread_name(const OSThread* t) {
    return std::string("PokemonStadium");
}

// ---- Boot ------------------------------------------------------------------

// lookup.cpp defines this with C++ linkage (it's a .cpp file with no extern "C").
gpr get_entrypoint_address();

int main(int argc, char** argv) {
    // Crash-localization breadcrumbs — flushed immediately so a silent
    // exit reveals exactly how far we got.
    std::fprintf(stderr, "[PSR] main() entered\n"); std::fflush(stderr);

#ifdef _WIN32
    // Catch unhandled SEH exceptions from any thread (incl. game thread)
    // so silent crashes get diagnosed. Disable the Win32 error dialog
    // so the process exits immediately into our handler.
    const bool crash_handler_disabled = std::getenv("PSR_DISABLE_CRASH_HANDLER") != nullptr;
    if (!crash_handler_disabled) {
        SetUnhandledExceptionFilter(psr_crash_filter);
        AddVectoredExceptionHandler(1, psr_vectored_exception_logger);
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        // Also dump on clean exit so last_run_report.json reflects the
        // final state regardless of how the runner exited.
        std::atexit([]() { psr_post_mortem_dump("atexit", nullptr); });
    }
#endif

    // Offline aspMain replay (audio-crackle differential harness):
    // PSR_ASPMAIN_REPLAY=<capture_dir> re-runs a captured audio task
    // through the recompiled aspMain, diffs RDRAM against the captured
    // in-game result, and exits without booting the game. See
    // aspmain_replay.cpp for the capture/replay contract.
    if (const char* replay_dir = std::getenv("PSR_ASPMAIN_REPLAY")) {
        return psr_aspmain_replay_main(replay_dir);
    }

#ifdef _WIN32
    // Force WASAPI for stable sample queueing.
    SDL_setenv("SDL_AUDIODRIVER", "wasapi", true);
#endif

    std::fprintf(stderr, "[PSR] before SDL_InitSubSystem\n"); std::fflush(stderr);

    // PSR_TURBO env var: turbo (fast-forward) now defaults OFF for
    // real-time playback. Turbo makes the game over-produce audio (the
    // empty-buffer report in get_frames_remaining), pushing the SDL queue
    // past the 100ms decimation threshold -> sample-dropping -> audible
    // clicks. Opt back in with PSR_TURBO=1 for fast softlock reproduction.
    bool turbo_default = false;
    if (const char* turbo_env = std::getenv("PSR_TURBO")) {
        // Accept 0/1, false/true, off/on (case-insensitive first char).
        char c = turbo_env[0];
        if (c == '0' || c == 'f' || c == 'F' || c == 'n' || c == 'N' ||
            c == 'o' || c == 'O') {
            // distinguish "off" from "on" by second char if first is 'o'/'O'.
            turbo_default = !(c == '0' || c == 'f' || c == 'F' || c == 'n' || c == 'N'
                || (turbo_env[1] == 'f' || turbo_env[1] == 'F'));
        } else {
            turbo_default = true;
        }
    }
    pkmnstadium::dbg::g_fast_forward.store(turbo_default);
    s_turbo_persistent.store(turbo_default);
    std::fprintf(stderr, "[PSR] PSR_TURBO=%s -> turbo %s "
                 "(toggle via TCP `fast_forward`; hold TAB for momentary turbo)\n",
                 std::getenv("PSR_TURBO") ? std::getenv("PSR_TURBO") : "(unset)",
                 turbo_default ? "ON" : "off");
    std::fflush(stderr);

    // PSR_DISABLE_VOLUNTARY_PREEMPTION env var: short-circuit ultramodern's
    // host-monitored "no context-switch in N seconds" yield mechanism.
    // The mechanism replaces three per-site Stadium hacks
    // (free-battle-modal-softlock, petit-cup-softlock, asset-pending-bypass)
    // with a generic cooperative-scheduler preemption that fires when a
    // game thread monopolizes the CPU. If audio synth timing regresses
    // (prior 2026-05-09 attempt flipped a pre-existing UAF — that UAF
    // has since been fixed by SecondaryVoiceTableLayout, but this gate
    // gives us a quick rollback if a new regression surfaces).
    {
        const char* dis_env = std::getenv("PSR_DISABLE_VOLUNTARY_PREEMPTION");
        bool enabled = true;
        if (dis_env && dis_env[0] != '\0' && dis_env[0] != '0') {
            enabled = false;
        }
        ultramodern_voluntary_preemption_set_enabled(enabled ? 1 : 0);
        std::fprintf(stderr,
            "[PSR] voluntary preemption %s (set PSR_DISABLE_VOLUNTARY_PREEMPTION=1 to disable)\n",
            enabled ? "ENABLED" : "DISABLED");
        std::fflush(stderr);
    }

    SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);
    // Deliver raw SDL_JOY* events too, not just SDL_CONTROLLER* ones, so the
    // rebind scan can capture inputs the GameController mapping can't express
    // (issue #15: 8BitDo 64 C-buttons on raw buttons 16/17 + a Z-rotation axis).
    SDL_JoystickEventState(SDL_ENABLE);
    std::fprintf(stderr, "[PSR] SDL audio/controller init OK\n"); std::fflush(stderr);

    // ── Controller mapping database (issue #15: 8BitDo 64 & others) ───────────
    // SDL only surfaces a device through the SDL_GameController API if it has a
    // mapping; an unmapped joystick (e.g. the 8BitDo 64 in D-Input / Bluetooth
    // mode) is invisible to both the launcher and the game (SDL_IsGameController
    // returns false). Load the bundled community DB so thousands of controllers
    // — including the 8BitDo 64's per-platform/transport GUIDs — are recognized,
    // then add a hardcoded 8BitDo 64 fallback so it works even if the file is
    // missing. Users can also drop an updated gamecontrollerdb.txt next to the
    // exe (assets/) without a rebuild. Must run before any SDL_IsGameController.
    {
        const std::string db = pkmnstadium::app_file("assets/gamecontrollerdb.txt").string();
        int added = SDL_GameControllerAddMappingsFromFile(db.c_str());
        if (added < 0) {
            std::fprintf(stderr, "[PSR] controller DB NOT loaded (%s): %s\n", db.c_str(), SDL_GetError());
        } else {
            std::fprintf(stderr, "[PSR] controller DB: %d mappings loaded from %s\n", added, db.c_str());
        }
        // Hardcoded 8BitDo 64 fallback (all platforms/transports). SDL silently
        // ignores mapping lines whose platform field doesn't match the host, so
        // listing every variant is safe and future-proofs the Linux/Mac builds.
        static const char* const k_8bitdo64_maps[] = {
            "03000000c82d00001930000000000000,8BitDo 64,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,righttrigger:b9,rightx:a3,righty:a4,start:b11,platform:Windows,",
            "03000000c82d00001930000000000000,8BitDo 64,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,righttrigger:b9,rightx:a2,righty:a3,start:b11,platform:Mac OS X,",
            "03000000c82d00001930000000020000,8BitDo 64,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,righttrigger:b9,rightx:a2,righty:a3,start:b11,platform:Mac OS X,",
            "03000000c82d00001930000001000000,8BitDo 64,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,righttrigger:b9,rightx:a2,righty:a3,start:b11,platform:Mac OS X,",
            "03000000c82d00001930000011010000,8BitDo 64,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,righttrigger:b9,rightx:a2,righty:a3,start:b11,platform:Linux,",
            "05000000c82d00001930000001000000,8BitDo 64,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,righttrigger:b9,rightx:a2,righty:a3,start:b11,platform:Linux,",
        };
        for (const char* m : k_8bitdo64_maps) SDL_GameControllerAddMapping(m);
    }

    // Load the user's input bindings (per-device-type keyboard/controller maps,
    // edited via the launcher's per-port rebind menu). Falls back to the
    // historical default layout if input.cfg is absent, so the build is
    // immediately playable. Must run before get_n64_input is first polled.
    pkmnstadium::input::load();

    // Enumerate connected gamepads for the SS Anne launcher's controller
    // dropdowns. Done here (before the render hooks initialize) so the list is
    // ready when the launcher document loads.
    {
        SDL_GameControllerUpdate();
        std::vector<std::pair<int, std::string>> pads;
        const int njoy = SDL_NumJoysticks();
        // Issue #15 diagnostics: log EVERY joystick (not just recognized game
        // controllers) with its SDL GUID, so an unmapped device in a user's log
        // can be identified and given a precise mapping.
        for (int i = 0; i < njoy; ++i) {
            char guid[33] = {0};
            SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof(guid));
            const char* jn = SDL_JoystickNameForIndex(i);
            std::fprintf(stderr, "[PSR] joystick %d: '%s' guid=%s game_controller=%s\n",
                i, jn ? jn : "?", guid,
                SDL_IsGameController(i) ? "yes" : "NO (no SDL mapping)");
        }
        std::fflush(stderr);
        for (int i = 0; i < njoy; ++i) {
            if (!SDL_IsGameController(i)) continue;
            const char* nm = SDL_GameControllerNameForIndex(i);
            pads.emplace_back(SDL_JoystickGetDeviceInstanceID(i),
                              nm ? std::string(nm) : std::string("Controller"));
        }
        // Disambiguate identical controller names (e.g. two "DualSense Wireless
        // Controller") so the dropdown can tell them apart. Assignment is still
        // by unique instance id; this only clarifies the label.
        {
            std::vector<std::string> orig;
            orig.reserve(pads.size());
            for (auto& p : pads) orig.push_back(p.second);
            for (size_t i = 0; i < pads.size(); ++i) {
                int total = 0, idx = 0;
                for (size_t j = 0; j < pads.size(); ++j) {
                    if (orig[j] == orig[i]) { ++total; if (j <= i) idx = total; }
                }
                if (total > 1) pads[i].second = orig[i] + " #" + std::to_string(idx);
            }
        }
        std::fprintf(stderr, "[PSR] %zu gamepad(s) detected for launcher\n", pads.size());
        std::fflush(stderr);
        pkmnstadium::ui_seam::set_gamepads(pads);
    }

    // Enumerate audio output devices for the launcher's Settings > Audio output
    // dropdown. The actual device selection (select_audio_device) happens at
    // reset_audio time and honors the saved name; this is just the picker list.
    {
        std::vector<std::string> audio_devs;
        const int acount = SDL_GetNumAudioDevices(0 /* output */);
        for (int i = 0; i < acount; ++i) {
            const char* nm = SDL_GetAudioDeviceName(i, 0);
            if (nm != nullptr && nm[0] != '\0') audio_devs.emplace_back(nm);
        }
        std::fprintf(stderr, "[PSR] %zu audio output device(s) for launcher\n", audio_devs.size());
        pkmnstadium::ui_seam::set_audio_devices(audio_devs);
    }

    reset_audio(48000);
    std::fprintf(stderr, "[PSR] reset_audio done\n"); std::fflush(stderr);

    // Start the dedicated Ares-oracle worker thread BEFORE the debug
    // server so all ares_* TCP commands route to that one thread (Ares'
    // libco scheduler uses thread_local state, so cothreads must be
    // created and run on the same OS thread).
    pkmnstadium::ares_worker::start();
    std::fprintf(stderr, "[PSR] ares worker thread started\n"); std::fflush(stderr);

    // Start the TCP debug server before recomp::start so a debugger
    // script can connect immediately and observe the boot.
    int debug_port = 4371;
    if (const char* port_env = std::getenv("PSR_DEBUG_PORT")) {
        int parsed = std::atoi(port_env);
        if (parsed > 0 && parsed <= 65535) {
            debug_port = parsed;
        }
    }
    pkmnstadium::dbg::start(debug_port);
    std::fprintf(stderr, "[PSR] debug server started on tcp:%d\n", debug_port); std::fflush(stderr);

    recomp::Version project_version{0, 1, 0, ""};
    recomp::register_config_path(std::filesystem::current_path());
    pkmnstadium::transfer_pak::initialize();

    // Resolve all launch-time graphics settings from launcher.cfg (+ env
    // overrides) and apply them BEFORE recomp::start creates the render context.
    // The launcher's Settings page persists these; fullscreen + supersampling +
    // MSAA also apply live there (update_config), but they must be seeded here so
    // the very first context honors the saved values.
    //   - Fullscreen (#18): wm_option.
    //   - Graphics API (#11): api_option (Auto -> rt64 auto-falls-back D3D12->Vulkan).
    //   - Supersampling: ds_option (1/2/4). MSAA: msaa_option.
    {
        ultramodern::renderer::GraphicsConfig gconf = ultramodern::renderer::get_graphics_config();

        gconf.wm_option = pkmnstadium::ui_seam::startup_fullscreen()
            ? ultramodern::renderer::WindowMode::Fullscreen
            : ultramodern::renderer::WindowMode::Windowed;

        const std::string api = pkmnstadium::ui_seam::startup_graphics_api();
        if (api == "vulkan")     gconf.api_option = ultramodern::renderer::GraphicsApi::Vulkan;
        else if (api == "d3d12") gconf.api_option = ultramodern::renderer::GraphicsApi::D3D12;
        else                     gconf.api_option = ultramodern::renderer::GraphicsApi::Auto;

        gconf.ds_option = pkmnstadium::ui_seam::startup_ds_option();

        using AA = ultramodern::renderer::Antialiasing;
        switch (pkmnstadium::ui_seam::startup_msaa()) {
            case 0:  gconf.msaa_option = AA::None;   break;
            case 2:  gconf.msaa_option = AA::MSAA2X; break;
            case 8:  gconf.msaa_option = AA::MSAA8X; break;
            default: gconf.msaa_option = AA::MSAA4X; break;
        }

        ultramodern::renderer::set_graphics_config(gconf);
        std::fprintf(stderr, "[PSR] graphics: wm=%s api=%s ds=%d msaa=%d\n",
            gconf.wm_option == ultramodern::renderer::WindowMode::Fullscreen ? "fullscreen" : "windowed",
            api.c_str(), gconf.ds_option, pkmnstadium::ui_seam::startup_msaa());
    }

    recomp::GameEntry game{};
    game.rom_hash               = 0x6E46EACF8F27011DULL;  // XXH3_64bits of baserom.z64 (US v1.0)
                                                         // computed by tools/compute_rom_hash.exe
    game.internal_name          = "POKEMON STADIUM";
    game.game_id                = u8"pokestadium.us.1.0";
    game.mod_game_id            = "pokestadium";
    game.save_type              = recomp::SaveType::Flashram;  // Pokemon Stadium uses 1Mbit FlashRAM
    game.is_enabled             = true;
    game.has_compressed_code    = false;
    game.entrypoint_address     = get_entrypoint_address();
    game.entrypoint             = recomp_entrypoint;

    // Activate Stadium's dead-code expansion-pak path before Util_InitMainPools
    // runs. Util_InitMainPools reads gExpansionRAMStart (vaddr 0x80068B90)
    // to choose POOL_END_4MB vs POOL_END_6MB. The 6MB path is only taken
    // when gExpansionRAMStart > 0 AND osMemSize > 0x600000. Our runtime
    // reports osMemSize=8MB correctly, but gExpansionRAMStart lives in
    // BSS and has NO writer anywhere in the recompiled binary — on real
    // hardware some hardware-detect path (RDP probe / silicon side-effect)
    // sets it; in our HLE harness it stays zeroed and the 6MB code path is
    // unreachable. Stadium's working set marginally exceeds the 3MB usable
    // cap of the 4MB path, so the title-screen pokemon-models loader hits
    // an alloc failure without this. Forcing the value at on_init_callback
    // is the proper layer for this kind of "missing-from-HLE hardware-detect
    // side-effect" — it fires once after rdram is wired up but before the
    // game thread reads the global. Previously implemented as a hook on
    // Util_InitMainPools entry; that worked but required a recompile-time
    // hook for a one-time runtime-init fact. See TEMP_PATCHES.md
    // 'force-expansion-ram' entry (retired by this change).
    game.on_init_callback = [](uint8_t* rdram, recomp_context* /*ctx*/) {
        // gExpansionRAMStart at kseg0 0x80068B90 -> physical 0x00068B90.
        // Write u32 = 1 with XOR-3 byte order to match recompiled MEM_W.
        constexpr uint32_t paddr = 0x00068B90u;
        rdram[(paddr + 0) ^ 3] = 0;
        rdram[(paddr + 1) ^ 3] = 0;
        rdram[(paddr + 2) ^ 3] = 0;
        rdram[(paddr + 3) ^ 3] = 1;
        std::fprintf(stderr,
            "[PSR] on_init: forced gExpansionRAMStart=1 "
            "(POOL_END_6MB path active)\n");
        std::fflush(stderr);
    };

    recomp::register_game(game);
    std::fprintf(stderr, "[PSR] game registered\n"); std::fflush(stderr);

    // Feed the generated section/overlay table into librecomp so its
    // func_map knows about every section beyond patches. Without this,
    // any LOOKUP_FUNC at runtime fails with "Failed to find function
    // at 0x...". Defined in src/main/register_overlays.cpp.
    pokestadium::register_overlays();
    std::fprintf(stderr, "[PSR] overlays registered\n"); std::fflush(stderr);

    // Register RSP pre-task hooks. Stadium's aspMain (the standard
    // libultra audio ucode, stripped variant at ROM 0x68020) skips
    // the command-load setup that Zelda's variant does inline; we
    // replicate rspboot's residue (DMEM[0x2B0] = first chunk, $29 =
    // 0x2B0, $30 = chunk_size, dma regs harmless) so the dispatch
    // loop reads real commands instead of dispatch-table residue.
    // Defined in src/main/rsp_aspmain_hook.cpp.
    pokestadium::rsp::register_pre_task_hooks();
    std::fprintf(stderr, "[PSR] rsp pre-task hooks registered\n"); std::fflush(stderr);

    // Register Stadium's libnaudio voice-list layout so the generic
    // audio-UAF protector in librecomp can silence voices whose backing
    // wavetable memory is about to be freed by main_pool_pop_state.
    //
    // Walk strategy: pAllocList + pLameList in N_ALSynth catch every
    // voice the synth might still pull from on the next audio frame
    // regardless of bus routing — a previous auxBus->sources walk
    // missed voices that were allocated but routed elsewhere.
    //
    // Offsets verified against disasm/src/libnaudio/n_synthInternals.h
    // (N_PVoice: dc_table=0x20, em_motion=0x84) and recompiled
    // n_alSynNew at 0x800491C0 (N_ALSynth: pAllocList=0x0C,
    // pLameList=0x14). n_syn lives at vaddr 0x80078584. The offsets
    // are libnaudio standards; future libnaudio games can reuse this
    // registration shape and only swap n_syn_var_vaddr.
    {
        librecomp::audio_uaf::VoiceLayout layout{};
        layout.n_syn_var_vaddr                = 0x80078584u;
        layout.alloc_list_offset              = 0x0Cu;  // N_ALSynth.pAllocList
        layout.lame_list_offset               = 0x14u;  // N_ALSynth.pLameList
        layout.voice_em_motion_offset         = 0x84u;  // N_PVoice.em_motion
        layout.voice_dc_table_offset          = 0x20u;  // N_PVoice.dc_table
        layout.voice_em_motion_stopped_value  = 0u;     // AL_STOPPED
        layout.max_voice_count                = 64u;
        // Diagnostic-only offsets for the always-on voice-event ring.
        // Standard libnaudio N_PVoice ABI (n_synthInternals.h): the
        // ADPCM load-filter fields. Used to capture predictor carry /
        // first-decode flag / sample position at each key-on so the
        // music-rate click can be correlated against voice events.
        layout.voice_dc_state_offset          = 0x0Cu;  // N_PVoice.dc_state (ADPCM_STATE*)
        layout.voice_dc_sample_offset         = 0x30u;  // N_PVoice.dc_sample
        layout.voice_dc_lastsam_offset        = 0x34u;  // N_PVoice.dc_lastsam
        layout.voice_dc_first_offset          = 0x38u;  // N_PVoice.dc_first
        librecomp::audio_uaf::register_voice_layout(layout);
    }

    // Stadium-side secondary voice table — array-style high-level synth
    // voices at D_800FC7D0 (count at 0x800FC7CC, array ptr at 0x800FC7D0).
    // The wavetable-pointer-ARRAY base lives at voice.unk_090 + 0x2C
    // (verified against the audio_diag hook capture: ctx->r11 = $t3 =
    // MEM_W($s0, 0x2C) where $s0 = arg0->unk_090). For Stadium's UAF
    // source (the 1 MiB SoundBank pool buffer freed at fragment36
    // cleanup), the array base AND its wavetable entries all live in
    // the same buffer, so range-checking the array base correctly
    // catches the freed-bank case. Silence by zeroing voice.unk_038 —
    // Stadium's func_8003AD58 skips voices whose unk_038 is null.
    // Routing this through audio_uaf_protect retires the per-scene
    // func_8004FF20 hook (was pkmnstadium_audio_stop_voices) and makes
    // coverage range-checked + global across all pool pops.
    {
        librecomp::audio_uaf::SecondaryVoiceTableLayout sec{};
        sec.count_vaddr           = 0x800FC7CCu;
        sec.array_ptr_vaddr       = 0x800FC7D0u;
        sec.voice_size            = 0x150u;
        sec.max_voice_count       = 64u;
        sec.chain_step_count      = 2u;
        sec.chain_offsets[0]      = 0x90u;  // voice.unk_090 -> inner struct ptr
        sec.chain_offsets[1]      = 0x2Cu;  // .unk_2C       -> wavetable-array base
        sec.silence_field_offset  = 0x38u;  // voice.unk_038
        sec.silence_value         = 0u;
        librecomp::audio_uaf::register_secondary_voice_table(sec);
    }

    // Validate + load the ROM before recomp::start. select_rom verifies
    // the hash matches our registered GameEntry and stashes it as the
    // active ROM so start_game can find it.
    //
    // Resolution order:
    //   1. argv[1] (if not a flag) — backwards-compatible CLI override.
    //   2. <exe_dir>/rom.cfg — last-used path from a prior successful pick.
    //   3. legacy "baserom.z64" next to the exe (or parent dir, dev layout).
    //   4. Win32 file-picker dialog. Loop until a valid ROM is chosen or
    //      the user cancels (which exits the runner).
    {
        std::u8string game_id = game.game_id;

#ifdef _WIN32
        char exe_path_buf[MAX_PATH] = {0};
        GetModuleFileNameA(NULL, exe_path_buf, MAX_PATH);
        std::filesystem::path exe_dir = std::filesystem::path(exe_path_buf).parent_path();
#else
        std::filesystem::path exe_dir = std::filesystem::current_path();
#endif
        std::filesystem::path cfg_path = exe_dir / "rom.cfg";

        auto read_rom_cfg = [&]() -> std::filesystem::path {
            std::ifstream f(cfg_path);
            if (!f) return {};
            std::string line;
            if (!std::getline(f, line)) return {};
            return line;
        };
        auto write_rom_cfg = [&](const std::filesystem::path& p) {
            std::ofstream f(cfg_path, std::ios::trunc);
            if (f) f << p.string() << "\n";
        };
        auto show_picker = [&](std::filesystem::path& out) -> bool {
#ifdef _WIN32
            char picked[MAX_PATH] = {0};
            OPENFILENAMEA ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = NULL;
            ofn.lpstrFilter = "N64 ROM (*.z64;*.n64;*.v64)\0*.z64;*.n64;*.v64\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile   = picked;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrTitle  = "Select Pokemon Stadium (US v1.0) ROM";
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (!GetOpenFileNameA(&ofn)) return false;
            out = picked;
            return true;
#else
            std::fprintf(stderr, "[PSR] no file picker on this platform; pass ROM path on the command line.\n");
            return false;
#endif
        };

        // 1. argv[1] override (CLI). Validated but never persisted to rom.cfg.
        std::filesystem::path rom_path;
        bool cli_override = false;
        if (argc >= 2 && argv[1] && argv[1][0] != '-') {
            rom_path = argv[1];
            cli_override = true;
        }

        // 2. rom.cfg from a prior successful pick.
        if (rom_path.empty()) {
            auto saved = read_rom_cfg();
            if (!saved.empty() && std::filesystem::exists(saved)) {
                rom_path = saved;
            }
        }

        // 3. Legacy "baserom.z64" next to exe or one dir up.
        if (rom_path.empty()) {
            auto legacy = exe_dir / "baserom.z64";
            if (!std::filesystem::exists(legacy)) {
                legacy = exe_dir.parent_path() / "baserom.z64";
            }
            if (std::filesystem::exists(legacy)) rom_path = legacy;
        }

        // 4. Loop: validate the candidate; if it fails, show a message and
        //    re-open the picker. Exit if the user cancels the picker.
        while (true) {
            if (!rom_path.empty() && std::filesystem::exists(rom_path)) {
                std::fprintf(stderr, "[PSR] selecting ROM: %s\n", rom_path.string().c_str());
                std::fflush(stderr);
                auto err = recomp::select_rom(rom_path, game_id);
                if (err == recomp::RomValidationError::Good) {
                    std::fprintf(stderr, "[PSR] select_rom OK\n"); std::fflush(stderr);
                    if (!cli_override) write_rom_cfg(rom_path);
                    break;
                }
                // Validation failed — describe to the user and re-pick.
                const char* err_name = "";
                switch (err) {
                    case recomp::RomValidationError::FailedToOpen:    err_name = "could not open file"; break;
                    case recomp::RomValidationError::NotARom:         err_name = "not a recognizable N64 ROM"; break;
                    case recomp::RomValidationError::IncorrectRom:    err_name = "wrong game"; break;
                    case recomp::RomValidationError::NotYet:          err_name = "not yet supported"; break;
                    case recomp::RomValidationError::IncorrectVersion:err_name = "wrong region/revision (need US v1.0)"; break;
                    default:                                          err_name = "validation error"; break;
                }
                std::fprintf(stderr, "[PSR] select_rom error: %d (%s)\n", (int)err, err_name); std::fflush(stderr);
#ifdef _WIN32
                std::string msg = "The selected ROM did not validate (" + std::string(err_name) + ").\n\n"
                                  "Path: " + rom_path.string() + "\n\n"
                                  "Required: Pokemon Stadium (US v1.0)\n"
                                  "Required MD5: ed1378bc12115f71209a77844965ba50\n\n"
                                  "Please select the correct ROM.";
                MessageBoxA(NULL, msg.c_str(), "PokemonStadiumRecomp — wrong ROM", MB_ICONWARNING | MB_OK);
#endif
                rom_path.clear();
            }

            // No valid candidate — pick interactively.
            if (!show_picker(rom_path)) {
#ifdef _WIN32
                MessageBoxA(NULL,
                    "No ROM selected — exiting.\n\n"
                    "PokemonStadiumRecomp needs a legal copy of Pokemon Stadium (US v1.0) "
                    "to run. Launch the runner again and pick the .z64 file when prompted.",
                    "PokemonStadiumRecomp", MB_ICONINFORMATION | MB_OK);
#endif
                std::fprintf(stderr, "[PSR] no ROM selected — exiting\n"); std::fflush(stderr);
                return 1;
            }
        }
    }

    // Defer start_game until after recomp::start has had a moment to spin
    // up the VI thread. The VI thread's update_vi() reads next_state->mode
    // unconditionally, but mode only gets initialized to a real OSViMode
    // when the game calls osViSetMode (or to a dummy when is_game_started()
    // is false). If we set game_status to Running BEFORE the VI thread
    // first ticks, set_dummy_vi never runs and update_vi crashes on a
    // NULL mode pointer. The game itself races to call osViSetMode in its
    // very first frames, so we need a small grace window where the dummy
    // VI mode gets installed first.
    // The SS Anne launcher is the first screen: the game stays unstarted while
    // the launcher renders, and boots only when the user clicks PLAY. A
    // dedicated thread waits for that signal and then calls start_game (kept off
    // the render thread, like the original deferred start). The natural delay
    // before a click also satisfies the VI-thread grace window above (the dummy
    // VI mode must be installed before game_status flips to Running).
    // PSR_AUTOBOOT=1 bypasses the launcher and boots immediately (dev/regression).
    const bool autoboot = std::getenv("PSR_AUTOBOOT") != nullptr;
    std::thread([game_id = game.game_id, autoboot]() {
        if (autoboot) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::fprintf(stderr, "[PSR] PSR_AUTOBOOT=1 -> skipping launcher\n");
        } else {
            while (!pkmnstadium::ui_seam::play_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
            // Lock in the launcher's controller/enabled config for input routing.
            apply_launcher_input_config();
            // Apply the audio output device chosen in the launcher's Settings
            // (re-selects from launcher.cfg). The launcher is shown only at boot,
            // so this is where the choice takes effect for the game — no app
            // relaunch needed. Re-selecting the same device is harmless (no audio
            // is playing yet).
            reset_audio(output_sample_rate);
        }
        recomp::start_game(game_id);
        std::fprintf(stderr, "[PSR] start_game fired\n"); std::fflush(stderr);
    }).detach();
    std::fprintf(stderr, "[PSR] launcher-first: waiting for PLAY (set PSR_AUTOBOOT=1 to skip)\n");
    std::fflush(stderr);

    recomp::rsp::callbacks_t rsp_callbacks{
        .get_rsp_microcode = get_rsp_microcode,
    };
    ultramodern::renderer::callbacks_t renderer_callbacks{
        .create_render_context = pokestadium::renderer::create_render_context,
    };
    ultramodern::audio_callbacks_t audio_callbacks{
        .queue_samples        = queue_samples,
        .get_frames_remaining = get_frames_remaining,
        .set_frequency        = set_frequency,
    };
    ultramodern::input::callbacks_t input_callbacks{
        .poll_input                 = poll_inputs,
        .get_input                  = get_n64_input,
        .set_rumble                 = set_rumble,
        .get_connected_device_info  = get_connected_device_info,
    };
    ultramodern::gfx_callbacks_t gfx_callbacks{
        .create_gfx    = create_gfx,
        .create_window = create_window,
        .update_gfx    = update_gfx,
    };
    // VI heartbeat — published to TCP debug server (g_vi_ticks) so a
    // debugger client can poll {"cmd":"status"} to confirm the renderer
    // is alive. Also bumps frame counter; counter / VI tick should
    // advance at roughly 60Hz when the game is running normally.
    static auto vi_heartbeat = []() {
        pkmnstadium::dbg::g_vi_ticks.fetch_add(1);
        pkmnstadium::dbg::g_frame_count.fetch_add(1);
    };
    ultramodern::events::callbacks_t events_callbacks{
        .vi_callback = vi_heartbeat,
    };
    ultramodern::error_handling::callbacks_t error_handling_callbacks{
        .message_box = error_message_box,
    };
    ultramodern::threads::callbacks_t threads_callbacks{
        .get_game_thread_name = get_game_thread_name,
    };

    recomp::start(
        project_version,
        {},  // window_handle (created by create_window callback)
        rsp_callbacks,
        renderer_callbacks,
        audio_callbacks,
        input_callbacks,
        gfx_callbacks,
        events_callbacks,
        error_handling_callbacks,
        threads_callbacks
    );

    std::fprintf(stderr, "[PSR] recomp::start returned\n"); std::fflush(stderr);
    return EXIT_SUCCESS;
}
