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
 * Per project principles: anywhere we'd want to "skip" something
 * with a stub, we either implement it for real or use a
 * loud-abort pattern that surfaces missing functionality.
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

#include "recomp.h"
#include <librecomp/game.hpp>
#include <ultramodern/ultramodern.hpp>
#include <ultramodern/scheduler_tick.hpp>
#include <ultramodern/error_handling.hpp>

#include "pokestadium_render.h"
#include "debug_server.h"
#include <librecomp/ultra_trace.hpp>
#include <librecomp/audio_uaf_protect.hpp>
#include "ares_worker.h"
#include "transfer_pak.h"

extern "C" void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);

namespace pokestadium { void register_overlays(); }
namespace pokestadium::rsp { void register_pre_task_hooks(); }

// RSP microcode entry points provided by RSPRecomp output (Zelda's
// built aspMain.cpp + njpgdspMain.cpp at F:/Projects/n64recomp/Zelda64Recomp/rsp,
// referenced by CMakeLists). These implement the libultra standard
// audio/JPEG microcodes that ship with Pokemon Stadium-era N64 games.
extern RspUcodeFunc aspMain;
extern RspUcodeFunc njpgdspMain;

static RspUcodeFunc* get_rsp_microcode(const OSTask* task) {
    switch (task->t.type) {
        case M_AUDTASK:   return aspMain;
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

static void queue_samples(int16_t* audio_data, size_t sample_count) {
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
    // Master volume is configurable: defaults to 0 (mute) so harness/test
    // re-launches don't blast the boot audio in a loop. Override per-launch
    // via PSR_VOLUME env var or at runtime via TCP `set_volume`.
    const float main_volume = pkmnstadium::dbg::g_audio_volume.load();
    if (main_volume == 0.0f) {
        // Skip the conversion entirely when muted — saves CPU cycles
        // on the audio thread during long automated runs.
        return;
    }
    for (size_t i = 0; i < sample_count; i += input_channels) {
        swap_buffer[i + 0 + duplicated_input_frames * input_channels] = audio_data[i + 1] * (0.5f / 32768.0f) * main_volume;
        swap_buffer[i + 1 + duplicated_input_frames * input_channels] = audio_data[i + 0] * (0.5f / 32768.0f) * main_volume;
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
}

static void reset_audio(uint32_t output_freq) {
    SDL_AudioSpec spec_desired{};
    spec_desired.freq    = (int)output_freq;
    spec_desired.format  = AUDIO_F32;
    spec_desired.channels = (Uint8)output_channels;
    spec_desired.samples = 0x100;

    audio_device = SDL_OpenAudioDevice(nullptr, false, &spec_desired, nullptr, 0);
    if (audio_device == 0) {
        fprintf(stderr, "SDL error opening audio device: %s\n", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }
    SDL_PauseAudioDevice(audio_device, 0);
    output_sample_rate = output_freq;
    update_audio_converter();
}

// ---- Window / gfx callbacks (SDL2-backed) ----------------------------------

static SDL_Window* g_window = nullptr;

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
static std::atomic<bool> s_turbo_persistent{true};

static void update_gfx(void*) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            std::exit(0);
        }
        if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_TAB &&
            ev.key.repeat == 0) {
            pkmnstadium::dbg::g_fast_forward.store(true);
        } else if (ev.type == SDL_KEYUP && ev.key.keysym.sym == SDLK_TAB) {
            pkmnstadium::dbg::g_fast_forward.store(
                s_turbo_persistent.load());
        }
    }
}

// ---- Input (minimal SDL GameController-only; no gyro / no remap UI) --------
// Per principles: no stub. If a real controller isn't connected, polling
// returns zeroed input — the game runs as if the player is idle, which is
// real behavior, not a fake. Real input takes effect when a controller is
// plugged in and SDL_GameControllerOpen succeeds.

static SDL_GameController* g_pad = nullptr;

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
    ensure_pad_open();
}

static bool get_n64_input(int controller_num, uint16_t* buttons_out, float* x_out, float* y_out) {
    *buttons_out = 0;
    *x_out = 0.0f;
    *y_out = 0.0f;

    if (controller_num != 0) return false;

    // TCP override is OR'd in (not exclusive). claim_input arms the
    // override so libultra's osContInit reports port 1 connected,
    // but unattended keyboard/pad input should still get through.
    // The harness's set_button calls layer on top.
    uint16_t override_buttons = 0;
    float override_x = 0.0f, override_y = 0.0f;
    if (pkmnstadium::dbg::g_input_override_active.load()) {
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
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;

    if (g_pad) {
        auto pressed = [&](SDL_GameControllerButton btn) {
            return SDL_GameControllerGetButton(g_pad, btn) ? 1 : 0;
        };

        // N64 button bit layout (libultra contStat):
        //   0x8000 A         0x4000 B         0x2000 Z         0x1000 Start
        //   0x0800 D-Up      0x0400 D-Down    0x0200 D-Left    0x0100 D-Right
        //   0x0020 L         0x0010 R
        //   0x0008 C-Up      0x0004 C-Down    0x0002 C-Left    0x0001 C-Right
        if (pressed(SDL_CONTROLLER_BUTTON_A))             b |= 0x8000; // A
        if (pressed(SDL_CONTROLLER_BUTTON_B))             b |= 0x4000; // B
        if (pressed(SDL_CONTROLLER_BUTTON_LEFTSHOULDER))  b |= 0x2000; // Z
        if (pressed(SDL_CONTROLLER_BUTTON_START))         b |= 0x1000; // Start
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_UP))       b |= 0x0800;
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN))     b |= 0x0400;
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT))     b |= 0x0200;
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT))    b |= 0x0100;
        if (pressed(SDL_CONTROLLER_BUTTON_LEFTSTICK))     b |= 0x0020; // L
        if (pressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) b |= 0x0010; // R

        rx = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_RIGHTX);
        ry = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_RIGHTY);
        lx = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX);
        ly = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY);
    }

    // Keyboard fallback / supplement. SDL_GetKeyboardState requires
    // the VIDEO subsystem (initialized by create_gfx). The keyboard
    // layout below is the standard for N64 emulators (Project64 /
    // Mupen-style). Pressed keys OR-in to whatever the pad reports,
    // so a pad and keyboard can coexist.
    //
    //   X       -> A           Z       -> B
    //   Left-Shift -> Z trigger Enter   -> Start
    //   Q       -> L            E       -> R
    //   Arrows  -> D-pad
    //   I/K/J/L -> C-Up/Down/Left/Right
    //   W/A/S/D -> Analog stick (full deflection per key)
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    if (ks != nullptr) {
        if (ks[SDL_SCANCODE_X])      b |= 0x8000;        // A
        if (ks[SDL_SCANCODE_Z])      b |= 0x4000;        // B
        if (ks[SDL_SCANCODE_LSHIFT] ||
            ks[SDL_SCANCODE_SPACE])  b |= 0x2000;        // Z
        if (ks[SDL_SCANCODE_RETURN] ||
            ks[SDL_SCANCODE_KP_ENTER]) b |= 0x1000;      // Start
        if (ks[SDL_SCANCODE_UP])     b |= 0x0800;        // D-Up
        if (ks[SDL_SCANCODE_DOWN])   b |= 0x0400;        // D-Down
        if (ks[SDL_SCANCODE_LEFT])   b |= 0x0200;        // D-Left
        if (ks[SDL_SCANCODE_RIGHT])  b |= 0x0100;        // D-Right
        if (ks[SDL_SCANCODE_Q])      b |= 0x0020;        // L
        if (ks[SDL_SCANCODE_E])      b |= 0x0010;        // R
        if (ks[SDL_SCANCODE_I])      b |= 0x0008;        // C-Up
        if (ks[SDL_SCANCODE_K])      b |= 0x0004;        // C-Down
        if (ks[SDL_SCANCODE_J])      b |= 0x0002;        // C-Left
        if (ks[SDL_SCANCODE_L])      b |= 0x0001;        // C-Right

        // Digital-to-analog stick from WASD. Each key gives full
        // deflection — diagonals saturate to (±32767, ±32767) before
        // the deadzone-rescale. If a real pad is also tilting the
        // stick, sum + saturate.
        int32_t kx = 0, ky = 0;
        if (ks[SDL_SCANCODE_A]) kx -= 32767;
        if (ks[SDL_SCANCODE_D]) kx += 32767;
        if (ks[SDL_SCANCODE_W]) ky -= 32767;  // SDL Y-axis: up = negative
        if (ks[SDL_SCANCODE_S]) ky += 32767;
        int32_t sx = int32_t(lx) + kx;
        int32_t sy = int32_t(ly) + ky;
        if (sx >  32767) sx =  32767;
        if (sx < -32768) sx = -32768;
        if (sy >  32767) sy =  32767;
        if (sy < -32768) sy = -32768;
        lx = (int16_t)sx;
        ly = (int16_t)sy;
    }
    *buttons_out = b;

    // C-buttons from right stick.
    constexpr int16_t deadzone = 8000;
    if (ry < -deadzone) *buttons_out |= 0x0008; // C-Up
    if (ry >  deadzone) *buttons_out |= 0x0004; // C-Down
    if (rx < -deadzone) *buttons_out |= 0x0002; // C-Left
    if (rx >  deadzone) *buttons_out |= 0x0001; // C-Right

    // Radial deadzone: Xbox One controller sticks rest at ~±300-1500
    // raw; without a deadzone, Stadium reads idle noise as "player
    // tilted the stick" and combines it with button presses — pressing
    // Start with even small drift gets interpreted as "cancel/skip"
    // rather than "confirm," which is what was making the title screen
    // soft-reset back to the boot sequence on any button press.
    //
    // 8000 raw matches the C-button deadzone we already use for the
    // right stick; well above typical resting drift but well below
    // intentional tilts. Re-scale OUTSIDE the deadzone so the player
    // gets full N64 stick range from a partial Xbox tilt.
    constexpr int16_t LSTICK_DEADZONE = 8000;
    auto apply_deadzone = [](int16_t raw) -> float {
        int32_t v = raw;
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
    if (*buttons_out != s_last_buttons) {
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
    if (controller_num != 0 || !g_pad) return;
    SDL_GameControllerRumble(g_pad, on ? 0xFFFF : 0, on ? 0xFFFF : 0, 1000);
}

static ultramodern::input::connected_device_info_t get_connected_device_info(int controller_num) {
    // libultra's osContInit() calls this BEFORE the first poll cycle,
    // so we have to lazy-detect the SDL pad here too. Without this,
    // Stadium's title screen reports "Controller 1 not connected"
    // even when an Xbox/etc. pad is plugged in.
    if (controller_num == 0) {
        ensure_pad_open();
    }
    // Port 0 always reports a controller present: keyboard is always
    // available (as a digital fallback in get_n64_input), the TCP
    // override may have been claimed by a harness, and a real SDL pad
    // may or may not be plugged in. All three paths feed buttons into
    // the same get_n64_input return surface, so the game is told
    // "yes, there's a controller in port 1" regardless of which
    // input device the user is actually using.
    const bool has_transfer_pak = pkmnstadium::transfer_pak::has_transfer_pak(controller_num);
    ultramodern::input::connected_device_info_t info{};
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
    // Unified post-mortem dump first (writes build/last_run_report.json).
    // The legacy last_error.log path below is preserved for back-compat
    // with existing tooling; remove it once everything reads
    // last_run_report.json.
    psr_post_mortem_dump("seh", info);

    FILE* f = fopen("F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_error.log", "a");
    if (f) {
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
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

// ---- Error handling -------------------------------------------------------

static void error_message_box(const char* msg) {
    // Always-on persistent error log — this fires before
    // ultramodern::error_handling::quick_exit() terminates the process,
    // and stderr buffering can swallow the message in headless runs.
    // Write a known file path so post-mortem inspection is easy.
    FILE* f = fopen("F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_error.log", "a");
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
    SetUnhandledExceptionFilter(psr_crash_filter);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    // Also dump on clean exit so last_run_report.json reflects the
    // final state regardless of how the runner exited.
    std::atexit([]() { psr_post_mortem_dump("atexit", nullptr); });
#endif

#ifdef _WIN32
    // Force WASAPI for stable sample queueing.
    SDL_setenv("SDL_AUDIODRIVER", "wasapi", true);
#endif

    std::fprintf(stderr, "[PSR] before SDL_InitSubSystem\n"); std::fflush(stderr);

    // PSR_VOLUME env var overrides the default-muted master volume.
    // Accepts e.g. "0.5", "1.0". Invalid / unset leaves volume at 0.
    if (const char* vol_env = std::getenv("PSR_VOLUME")) {
        float v = (float)std::strtod(vol_env, nullptr);
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        pkmnstadium::dbg::g_audio_volume.store(v);
        std::fprintf(stderr, "[PSR] PSR_VOLUME=%s -> master volume %.3f\n", vol_env, v);
    } else {
        std::fprintf(stderr, "[PSR] master volume defaulting to 0 (muted). "
            "Set PSR_VOLUME=1.0 to unmute, or use TCP `set_volume`.\n");
    }

    // PSR_TURBO env var: turbo (fast-forward) on by default while we're
    // chasing softlocks; flip off with PSR_TURBO=0 for real-time
    // playback. Reproducing attract takes ~1/4 the wallclock time
    // with turbo on.
    bool turbo_default = true;
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
    std::fprintf(stderr, "[PSR] SDL audio/controller init OK\n"); std::fflush(stderr);
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
    pkmnstadium::dbg::start(4371);
    std::fprintf(stderr, "[PSR] debug server started on tcp:4371\n"); std::fflush(stderr);

    recomp::Version project_version{0, 1, 0, ""};
    recomp::register_config_path(std::filesystem::current_path());
    pkmnstadium::transfer_pak::initialize();

    recomp::GameEntry game{};
    game.rom_hash               = 0x6E46EACF8F27011DULL;  // XXH3_64bits of baserom.z64 (US v1.0)
                                                         // computed by tools/compute_rom_hash.exe
    game.internal_name          = "POKEMON STADIUM";
    game.game_id                = u8"pokestadium.us.1.0";
    game.mod_game_id            = "pokestadium";
    game.save_type              = recomp::SaveType::Eep4k;  // Pokemon Stadium uses EEPROM
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
    std::thread([game_id = game.game_id]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        recomp::start_game(game_id);
        std::fprintf(stderr, "[PSR] start_game fired (deferred 100ms)\n"); std::fflush(stderr);
    }).detach();
    std::fprintf(stderr, "[PSR] start_game scheduled, about to recomp::start\n"); std::fflush(stderr);

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
