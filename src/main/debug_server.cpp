/*
 * debug_server.cpp — Minimal TCP debug server for PokemonStadiumRecomp.
 *
 * A small single-client, JSON-line TCP debug server. Default port
 * 4370.
 *
 * Commands implemented for first-pass visibility:
 *
 *   ping                         → {"ok":true,"pong":true}
 *   status                       → frame counter, vi count, fast_forward state, errors
 *   set_button {name, down}      → simulate controller button press
 *   set_stick {x, y}             → analog stick override (-128..127)
 *   clear_input                  → drop overrides
 *   fast_forward {on}            → flip fast-forward state
 *   enable_instant_present       → tell the renderer to skip vsync
 *   screenshot {path}            → ask the renderer to dump the next framebuffer
 *   tail_errlog                  → returns last_error.log content (post-mortem inspection)
 *   quit                         → exit cleanly
 *
 * Design rule: any state we want visible from outside grows TCP
 * commands here. Don't add side-channel logging.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#pragma comment(lib, "dbghelp.lib")

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "debug_server.h"
#include "app_paths.h"
#include "librecomp/ultra_trace.hpp"
#include "librecomp/rsp.hpp"
#include "librecomp/gbcart.hpp"
#include "ares_bridge.h"
#include "ares_worker.h"

#pragma comment(lib, "ws2_32.lib")

// Function-trace ring queries (definitions in extras.c).
extern "C" {
    struct GBTowerTraceEvent {
        uint64_t seq;
        uint32_t tag;
        uint32_t s0;
        uint32_t ra;
        uint32_t sp;
        uint32_t r2, r3, r4, r5, r6, r7, r8;
        uint32_t r9, r10, r11, r12, r13, r14, r15;
        uint32_t r16, r17, r18, r19, r20, r21, r22, r23, r24, r25;
        uint32_t state;
        uint32_t phase;
        uint32_t g_c4f4;
        uint32_t g_c4f8;
        uint32_t g_c4e0;
        uint32_t g_c4e4;
        uint32_t g_c4e8;
        uint32_t g_c4ec;
        uint32_t g_c4f0;
        uint32_t g_c4fc;
        uint32_t g_b2b8;
        uint32_t s_ctrl0;
        uint32_t s_ctrl1;
        uint32_t s_ctrl2;
        uint32_t s_5dd0;
        uint32_t s_hdr0;
        uint32_t s_hdr1;
        uint32_t s_hdr2;
        uint32_t s_ready0;
        uint32_t s_ready1;
        uint32_t s_pad0;
        uint32_t w5390, w5394, w5398;
        uint32_t w53a4, w53b4, w53bc;
        uint32_t rom_0140, rom_0144, rom_0148, rom_014c;
        uint32_t rom_type;
        uint32_t w5d64, w5d68, w5d6c, w5d70, w5d74, w5d78, w5d7c;
        uint32_t ctx_r2_byte;
        uint32_t gb_pc;
        uint32_t gb_pc_base;
        uint32_t gb_pc_addr;
        uint32_t gb_pc_byte;
        uint32_t op_table0;
        uint32_t op_handler;
        uint32_t io_ff40_43;
        uint32_t io_ff44_47;
        uint32_t io_ff00_03, io_ff04_07, io_ff0c_0f;
        uint32_t io_ff4c_4f, io_ff50_53, io_ff54_57;
        uint32_t hram_ff80_83, hram_ff84_87, hram_ff88_8b;
        uint32_t hram_ffd4_d7, hram_fffc_ff;
        uint32_t h53d6, h53d8, h53da, h53dc, h53de;
        uint32_t w53c0, w53c4, w53c8;
        uint32_t saved_5c44, saved_5c48, saved_5c4c, saved_5c50, saved_5c54;
        uint32_t w5388, w538c, w53b8, w53cc, w53d0, w53f0, w53f2_53f5;
        uint32_t w5484, w5488, w548c;
        uint32_t w55ac, w55b0, w55b4, w55b8, w55bc, w55c0, w55c4, w55c8;
        uint32_t w55ec, w55f0, w55f4, w55f8, w55fc, w5600, w5604, w5608;
        uint32_t w576c;
        uint32_t g_c4e2, g_c4f6, g_c4f7, g_c4f9;
        uint32_t cart_type_flags;
    };

    uint64_t pkmnstadium_trace_write_idx(void);
    const char* pkmnstadium_trace_at(uint64_t idx);
    uint32_t pkmnstadium_trace_capacity(void);
    uint64_t pkmnstadium_interesting_fn_count(int idx);
    const char* pkmnstadium_interesting_fn_name(int idx);
    int pkmnstadium_interesting_fn_total(void);
    uint64_t pkmnstadium_gbtower_trace_seq(void);
    uint32_t pkmnstadium_gbtower_trace_cap(void);
    void pkmnstadium_gbtower_trace_get(uint32_t i, GBTowerTraceEvent* out);
    int pkmnstadium_resolver_log_total(void);
    void pkmnstadium_resolver_log_get(int idx, uint32_t* arr, uint32_t* base, uint32_t* count);
    uint64_t pkmnstadium_memmap_seq(void);
    uint32_t pkmnstadium_memmap_cap(void);
    void pkmnstadium_memmap_get(uint32_t i, uint64_t* seq, uint32_t* kind,
                                uint32_t* id, uint32_t* vaddr, uint32_t* size,
                                uint32_t* caller, uint32_t* gs);
    uint64_t pkmnstadium_arcload_seq(void);
    uint32_t pkmnstadium_arcload_cap(void);
    void pkmnstadium_arcload_get(uint32_t i, uint64_t* seq, uint32_t* kind,
                                 uint32_t* a, uint32_t* b, uint32_t* h0, uint32_t* h1,
                                 uint32_t* h2, uint32_t* h3, uint32_t* e0, uint32_t* e1,
                                 uint32_t* e2, uint32_t* caller, uint32_t* gs);
    int recomp_debug_runtime_fragment(uint32_t id, uint32_t* out_section_index,
                                      int32_t* out_section_addr, int32_t* out_link_addr);
    int pkmnstadium_arcload_badfrag(uint32_t* r_id, uint32_t* r_frag,
                                    uint32_t* r_magic0, uint32_t* r_magic1,
                                    uint32_t* r_relocoff, uint32_t* r_sizeram,
                                    uint32_t* r_caller, uint32_t* r_gs,
                                    uint32_t* a_archive, uint32_t* a_unk00unk02,
                                    uint32_t* a_unk04, uint32_t* a_total,
                                    uint32_t* a_nfiles, uint32_t* a_filenum,
                                    uint32_t* a_foff, uint32_t* a_fsize,
                                    uint32_t* a_funk08, uint32_t* a_caller);
}

namespace pkmnstadium {
namespace dbg {

// ---- Public state polled by the runner each frame --------------------------
std::atomic<bool>     g_fast_forward{false};
std::atomic<bool>     g_enable_instant_present_request{false};
std::atomic<uint64_t> g_vi_ticks{0};
std::atomic<uint64_t> g_frame_count{0};

// Button override surface (controller 0). When `g_input_override_active` is
// true, the runner uses these in place of SDL polling.
std::atomic<bool>     g_input_override_active{false};
std::atomic<uint16_t> g_buttons_override{0};
std::atomic<int>      g_stick_x_override{0};  // -128..127
std::atomic<int>      g_stick_y_override{0};

std::atomic<uint64_t> g_send_dl_count{0};
std::atomic<uint64_t> g_update_screen_count{0};
std::atomic<uint64_t> g_send_dl_audio_count{0};
std::atomic<uint64_t> g_send_dl_gfx_count{0};
std::atomic<uint64_t> g_send_dl_other_count{0};

// ---- Input history ring (for crash repro replay) --------------------------
// Records every input-altering TCP command (claim/clear, set_button,
// set_stick) with the frame counter at command time. Dumped to
// build/last_run_input_history.json on post-mortem so we can re-drive
// the same sequence without a human in the loop.
struct InputEvent {
    uint64_t frame;     // ultramodern frame counter at event time
    uint64_t timestamp_ms; // wall clock ms since session start
    char     kind[16];  // "claim", "clear", "button", "stick"
    char     name[16];  // button name (for "button")
    int      down;      // 0/1 (for "button")
    int      x, y;      // (for "stick")
};
static constexpr size_t INPUT_HISTORY_CAP = 4096;
static InputEvent        s_input_history[INPUT_HISTORY_CAP];
static std::atomic<uint64_t> s_input_history_seq{0};
static std::mutex        s_input_history_mu;
static std::atomic<uint64_t> s_session_start_ms{0};

static uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

static void record_input_event(const char* kind,
                               const char* name = "",
                               int down = 0, int x = 0, int y = 0)
{
    std::lock_guard<std::mutex> lock(s_input_history_mu);
    uint64_t s0 = s_session_start_ms.load();
    if (s0 == 0) {
        s_session_start_ms.store(now_ms());
        s0 = s_session_start_ms.load();
    }
    uint64_t seq = s_input_history_seq.fetch_add(1);
    InputEvent& ev = s_input_history[seq % INPUT_HISTORY_CAP];
    ev.frame = g_frame_count.load();
    ev.timestamp_ms = now_ms() - s0;
    std::snprintf(ev.kind, sizeof(ev.kind), "%s", kind);
    std::snprintf(ev.name, sizeof(ev.name), "%s", name ? name : "");
    ev.down = down;
    ev.x = x;
    ev.y = y;
}

// Public — called by post_mortem.cpp to dump history to JSON file.
extern "C" void psr_dump_input_history_json(const char* path) {
    FILE* f = std::fopen(path, "w");
    if (!f) return;
    std::lock_guard<std::mutex> lock(s_input_history_mu);
    uint64_t total = s_input_history_seq.load();
    uint64_t count = std::min(total, (uint64_t)INPUT_HISTORY_CAP);
    uint64_t start = (total > INPUT_HISTORY_CAP) ? (total - INPUT_HISTORY_CAP) : 0;
    std::fprintf(f, "{\n  \"total_events\": %llu,\n  \"capacity\": %zu,\n  \"events\": [\n",
                 (unsigned long long)total, INPUT_HISTORY_CAP);
    for (uint64_t i = 0; i < count; i++) {
        const InputEvent& ev = s_input_history[(start + i) % INPUT_HISTORY_CAP];
        std::fprintf(f,
            "    {\"frame\":%llu,\"t_ms\":%llu,\"kind\":\"%s\",\"name\":\"%s\","
            "\"down\":%d,\"x\":%d,\"y\":%d}%s\n",
            (unsigned long long)ev.frame,
            (unsigned long long)ev.timestamp_ms,
            ev.kind, ev.name, ev.down, ev.x, ev.y,
            (i + 1 < count) ? "," : "");
    }
    std::fprintf(f, "  ]\n}\n");
    std::fclose(f);
}

// ---- Internals -------------------------------------------------------------
static std::thread        s_thread;
static std::atomic<bool>  s_running{false};
static SOCKET             s_listen_sock = INVALID_SOCKET;

// Surfaced from librecomp's mesgqueue.cpp — counts external-message
// re-queues, which happen when a target OSMesgQueue is full when the
// drain pass reaches it. Surfaced via debug_server's `status` cmd.
extern "C" uint64_t ultramodern_external_requeues(void);
extern "C" void ultramodern_mesg_recent_copy(
    void* out_void, size_t cap, size_t* n_written, uint64_t* next_seq_out);
extern "C" size_t ultramodern_mesg_event_size(void);
extern "C" size_t ultramodern_mesg_qstate_size(void);
extern "C" void ultramodern_mesg_qstates_copy(void* out_void, size_t cap, size_t* n_written);
extern "C" void recomp_coverage_live(
    uint64_t* static_hits, uint64_t* lookup_misses, uint64_t* self_heals,
    uint64_t* self_heal_misses, uint64_t* dispatch_entry_rejects,
    uint64_t* interp_runs, uint64_t* jit_compiles, uint64_t* jit_failures);
extern "C" uint64_t recomp_bubble_dispatch_count(void);
extern "C" uint64_t recomp_host_return_break_count(void);
extern "C" uint32_t recomp_host_return_break_last_target(void);
extern "C" uint32_t recomp_host_return_break_last_depth(void);
extern "C" uint32_t pkmnstadium_frag9_peek32(uint8_t* rdram, uint32_t link_offset);
extern "C" uint32_t pkmnstadium_frag9_base(void);
extern "C" uint64_t ultramodern_submit_gfx_count(void);
extern "C" uint64_t ultramodern_submit_audio_count(void);
extern "C" uint64_t ultramodern_submit_other_count(void);
extern "C" uint64_t ultramodern_sp_complete_count(void);
extern "C" uint64_t ultramodern_dp_complete_count(void);
extern "C" uint32_t recomp_rsp_get_sp_status(void);
extern "C" void ultramodern_get_current_dl_state(
    uint64_t* entry_seq, uint64_t* exit_seq,
    uint64_t* entry_ms,  uint64_t* exit_ms,
    uint32_t* data_ptr,  uint32_t* data_size,
    uint32_t* ucode_ptr);
extern "C" void ultramodern_get_vi_debug_state(
    uint32_t* cur_flags, uint32_t* next_flags,
    uint32_t* cur_fb,    uint32_t* next_fb,
    uint32_t* origin,    uint32_t* h_start,
    uint32_t* y_scale,   uint32_t* status);
extern "C" void recomp_sp_task_recent_copy(
    void* out_void, size_t cap, size_t* n_written, uint64_t* next_seq_out);
extern "C" size_t recomp_sp_task_event_size(void);
struct RspDmaTraceEvent {
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
    RspDmaTraceEvent* out, uint32_t out_cap,
    uint32_t* out_count, uint64_t* out_write_index);
extern "C" void recomp_voice_events_recent_copy(
    void* out_void, size_t cap, size_t* n_written, uint64_t* next_seq_out);
extern "C" size_t recomp_voice_event_size(void);
extern "C" void recomp_adpcm_decode_recent_copy(
    void* out_void, size_t cap, size_t* n_written, uint64_t* next_seq_out);
extern "C" size_t recomp_adpcm_decode_event_size(void);
extern "C" void recomp_audio_queue_recent_copy(
    void* out_void, size_t cap, size_t* n_written, uint64_t* next_seq_out);
extern "C" size_t recomp_audio_queue_event_size(void);
extern "C" void recomp_audio_pcm_recent_copy(
    void* out_void, size_t cap, size_t* n_written, uint64_t* next_seq_out);
extern "C" size_t recomp_audio_pcm_event_size(void);
extern "C" size_t recomp_audio_pcm_window(void);
extern "C" int psr_aspmain_capture_arm(const char* dir);
extern "C" int psr_aspmain_capture_state(void);
// Always-on per-task output-PCM ring (main.cpp) — the seam-analysis ring.
struct TaskPcmEventMirror {
    uint64_t seq;
    uint64_t ms;
    uint64_t dma_seq0;
    uint32_t dram;
    uint32_t len;
    uint32_t exit_reason;
    uint32_t n_dmas;
    int16_t samples[368];
};
extern "C" void recomp_task_pcm_recent_copy(
    void* out_void, size_t cap, size_t* n_written, uint64_t* next_seq_out);
extern "C" size_t recomp_task_pcm_event_size(void);
extern "C" int64_t recomp_task_pcm_dump(const char* path);
// Always-on AI-submission ring (ultramodern/src/audio.cpp) — guest addr +
// byte count of every audio buffer the game submits for playback.
struct AiSubmitEventMirror {
    uint64_t seq;
    uint64_t ms;
    uint32_t guest_addr;
    uint32_t byte_count;
};
extern "C" void ultramodern_ai_submit_recent_copy(
    void* out_void, size_t cap, size_t* n_written, uint64_t* next_seq_out);
extern "C" size_t ultramodern_ai_submit_event_size(void);
extern "C" void psr_post_mortem_dump(const char* reason, void* fault_info);
extern "C" int psr_dump_current_dl(const char* path,
                                   uint32_t* out_addr,
                                   uint32_t* out_size);
extern "C" uint64_t rt64_vtx_ring_write_idx(void);
extern "C" uint32_t rt64_vtx_ring_capacity(void);
extern "C" int rt64_vtx_ring_read(uint64_t seq, uint64_t* out_seq, float out_floats[9]);
extern "C" uint64_t rt64_tmem_ring_write_idx(void);
extern "C" uint32_t rt64_tmem_ring_capacity(void);
extern "C" int rt64_tmem_ring_read(uint64_t seq, uint64_t* out_seq,
                                   uint32_t out_u32[12], uint64_t* out_hash);
extern "C" uint64_t rt64_sprite_ring_write_idx(void);
extern "C" uint32_t rt64_sprite_ring_capacity(void);
extern "C" int rt64_sprite_ring_read(uint64_t seq, uint64_t* out_seq,
                                     int32_t out_i32[4], uint32_t out_u32[2]);
// Defined in lib/rt64/src/hle/rt64_rsp.cpp under #if RT64_PSR_DEBUG_HOOKS.
// We always link against rt64.lib so the symbol is present whenever
// RT64_PSR_DEBUG_HOOKS=1 in the rt64 build (default in PSR's CMake).
// Declared with weak-import semantics on platforms that support it
// (none on MSVC) so a future RT64_PSR_DEBUG_HOOKS=0 build doesn't fail
// the link — for now we just rely on the default being on.
extern "C" void rt64_psr_segments_copy(uint32_t* out16, uint64_t* out_seq);

// Map button name → N64 contStat bit.
static uint16_t button_bit(const std::string& n) {
    if (n == "A")        return 0x8000;
    if (n == "B")        return 0x4000;
    if (n == "Z")        return 0x2000;
    if (n == "Start")    return 0x1000;
    if (n == "DUp")      return 0x0800;
    if (n == "DDown")    return 0x0400;
    if (n == "DLeft")    return 0x0200;
    if (n == "DRight")   return 0x0100;
    if (n == "L")        return 0x0020;
    if (n == "R")        return 0x0010;
    if (n == "CUp")      return 0x0008;
    if (n == "CDown")    return 0x0004;
    if (n == "CLeft")    return 0x0002;
    if (n == "CRight")   return 0x0001;
    return 0;
}

// Crude JSON value extraction — enough for our small command surface.
// We do not pull in a JSON library to avoid one more dep.
static std::string get_str(const std::string& body, const char* key) {
    std::string needle = std::string("\"") + key + "\"";
    size_t k = body.find(needle);
    if (k == std::string::npos) return {};
    size_t colon = body.find(':', k);
    if (colon == std::string::npos) return {};
    size_t qa = body.find('"', colon);
    if (qa == std::string::npos) return {};
    size_t qb = body.find('"', qa + 1);
    if (qb == std::string::npos) return {};
    return body.substr(qa + 1, qb - qa - 1);
}

static int get_int(const std::string& body, const char* key, int dflt) {
    std::string needle = std::string("\"") + key + "\"";
    size_t k = body.find(needle);
    if (k == std::string::npos) return dflt;
    size_t colon = body.find(':', k);
    if (colon == std::string::npos) return dflt;
    return std::atoi(body.c_str() + colon + 1);
}

// Same as get_int but parses an unsigned 32-bit value. atoi
// overflows on values >= 0x80000000 (e.g. K0/K1 N64 vaddrs); use
// strtoul to keep the full unsigned range.
static uint32_t get_uint(const std::string& body, const char* key, uint32_t dflt) {
    std::string needle = std::string("\"") + key + "\"";
    size_t k = body.find(needle);
    if (k == std::string::npos) return dflt;
    size_t colon = body.find(':', k);
    if (colon == std::string::npos) return dflt;
    const char* p = body.c_str() + colon + 1;
    while (*p == ' ' || *p == '\t') p++;
    return (uint32_t)std::strtoul(p, nullptr, 0);
}

static bool get_bool(const std::string& body, const char* key, bool dflt) {
    std::string needle = std::string("\"") + key + "\"";
    size_t k = body.find(needle);
    if (k == std::string::npos) return dflt;
    size_t colon = body.find(':', k);
    if (colon == std::string::npos) return dflt;
    // Skip whitespace.
    const char* p = body.c_str() + colon + 1;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "true", 4) == 0)  return true;
    if (strncmp(p, "false", 5) == 0) return false;
    return std::atoi(p) != 0;
}

static std::string handle_command(const std::string& line) {
    auto cmd = get_str(line, "cmd");
    if (cmd.empty()) {
        // Bare command (no JSON) — match against the line trimmed.
        std::string bare = line;
        while (!bare.empty() && (bare.back() == '\n' || bare.back() == '\r' || bare.back() == ' ')) bare.pop_back();
        cmd = bare;
    }

    if (cmd == "ping") {
        return R"({"ok":true,"pong":true})";
    }
    if (cmd == "status") {
        // The librecomp accessor for the external-message requeue
        // counter — declared at file scope below for proper extern "C"
        // linkage. A sustained nonzero value means some target
        // OSMesgQueue's receiver is being starved relative to
        // host-thread event posts.
        uint8_t* rdram = recomp_runtime_get_rdram();
        uint32_t sp_status_mem = rdram
            ? (uint32_t)MEM_W(0, 0xFFFFFFFFA4040010ull)
            : 0;
        char buf[1200];
        std::snprintf(buf, sizeof(buf),
            "{\"ok\":true,\"frame\":%llu,\"vi\":%llu,\"fast_forward\":%s,\"input_override\":%s,\"buttons\":%u,\"sx\":%d,\"sy\":%d,"
            "\"send_dl\":%llu,\"send_dl_gfx\":%llu,\"send_dl_audio\":%llu,\"send_dl_other\":%llu,\"update_screen\":%llu,"
            "\"external_requeues\":%llu,"
            "\"submit_gfx\":%llu,\"submit_audio\":%llu,\"submit_other\":%llu,"
            "\"sp_complete\":%llu,\"dp_complete\":%llu,"
            "\"sp_status_atomic\":%u,\"sp_status_mem\":%u}",
            (unsigned long long)g_frame_count.load(),
            (unsigned long long)g_vi_ticks.load(),
            g_fast_forward.load() ? "true" : "false",
            g_input_override_active.load() ? "true" : "false",
            (unsigned)g_buttons_override.load(),
            g_stick_x_override.load(),
            g_stick_y_override.load(),
            (unsigned long long)g_send_dl_count.load(),
            (unsigned long long)g_send_dl_gfx_count.load(),
            (unsigned long long)g_send_dl_audio_count.load(),
            (unsigned long long)g_send_dl_other_count.load(),
            (unsigned long long)g_update_screen_count.load(),
            (unsigned long long)ultramodern_external_requeues(),
            (unsigned long long)ultramodern_submit_gfx_count(),
            (unsigned long long)ultramodern_submit_audio_count(),
            (unsigned long long)ultramodern_submit_other_count(),
            (unsigned long long)ultramodern_sp_complete_count(),
            (unsigned long long)ultramodern_dp_complete_count(),
            (unsigned)recomp_rsp_get_sp_status(),
            (unsigned)sp_status_mem
        );
        return buf;
    }
    if (cmd == "current_dl") {
        // Returns the in-flight gfx DL state. When entry_seq > exit_seq,
        // the gfx event thread is currently inside renderer_context->send_dl
        // and the data_ptr/data_size identify exactly which DL is in flight.
        // For a stuck send_dl, entry_seq > exit_seq AND (now - entry_ms) is
        // large — that's the hanging DL, and its raw bytes are at
        // [data_ptr, data_ptr+data_size) in rdram for offline analysis.
        uint64_t entry_seq = 0, exit_seq = 0, entry_ms = 0, exit_ms = 0;
        uint32_t data_ptr = 0, data_size = 0, ucode_ptr = 0;
        ultramodern_get_current_dl_state(&entry_seq, &exit_seq,
                                         &entry_ms, &exit_ms,
                                         &data_ptr, &data_size, &ucode_ptr);
        using namespace std::chrono;
        uint64_t now_ms = duration_cast<milliseconds>(
            steady_clock::now().time_since_epoch()).count();
        bool in_flight = entry_seq > exit_seq;
        uint64_t elapsed_ms = in_flight ? (now_ms - entry_ms) : (exit_ms - entry_ms);
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "{\"ok\":true,\"in_flight\":%s,\"entry_seq\":%llu,\"exit_seq\":%llu,"
            "\"entry_ms\":%llu,\"exit_ms\":%llu,\"now_ms\":%llu,\"elapsed_ms\":%llu,"
            "\"data_ptr\":%u,\"data_size\":%u,\"ucode_ptr\":%u}",
            in_flight ? "true" : "false",
            (unsigned long long)entry_seq, (unsigned long long)exit_seq,
            (unsigned long long)entry_ms,  (unsigned long long)exit_ms,
            (unsigned long long)now_ms,    (unsigned long long)elapsed_ms,
            (unsigned)data_ptr, (unsigned)data_size, (unsigned)ucode_ptr);
        return buf;
    }
    if (cmd == "vi_state") {
        uint32_t cur_flags = 0, next_flags = 0, cur_fb = 0, next_fb = 0;
        uint32_t origin = 0, h_start = 0, y_scale = 0, status = 0;
        ultramodern_get_vi_debug_state(&cur_flags, &next_flags,
                                       &cur_fb, &next_fb,
                                       &origin, &h_start, &y_scale, &status);
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "{\"ok\":true,\"cur_flags\":%u,\"next_flags\":%u,"
            "\"cur_fb\":%u,\"next_fb\":%u,\"origin\":%u,"
            "\"h_start\":%u,\"y_scale\":%u,\"status\":%u,"
            "\"cur_black\":%s,\"next_black\":%s}",
            cur_flags, next_flags, cur_fb, next_fb, origin, h_start, y_scale, status,
            (cur_flags & 0x20u) ? "true" : "false",
            (next_flags & 0x20u) ? "true" : "false");
        return buf;
    }
    if (cmd == "vtx_ring") {
        // Always-on triangle vertex ring (RT64 hle/rt64_vtx_ring). Each
        // entry is one drawIndexedTri call's 3 post-MVP, post-divide,
        // viewport-scaled screen-space positions — the same posScreen
        // values the renderer's drawRect logic reads. Used to answer
        // "do adjacent quads at the same world position produce
        // identical screen-space coords?" without RenderDoc.
        //
        // Args:
        //   "count"      — how many entries to return (default 32)
        //   "start_seq"  — starting sequence (default = write_idx - count + 1)
        //
        // Disable the writer entirely with RT64_VTX_RING_DISABLE=1.
        uint64_t write_idx = rt64_vtx_ring_write_idx();
        uint32_t capacity = rt64_vtx_ring_capacity();
        int count = get_int(line, "count", 32);
        if (count < 1) count = 1;
        if (count > 512) count = 512;
        int64_t start_seq_arg = (int64_t)get_int(line, "start_seq", -1);
        uint64_t start_seq;
        if (start_seq_arg < 0) {
            start_seq = (write_idx > (uint64_t)count) ? (write_idx - count + 1) : 1;
        } else {
            start_seq = (uint64_t)start_seq_arg;
        }
        std::string out;
        char hdr[256];
        std::snprintf(hdr, sizeof(hdr),
            "{\"ok\":true,\"write_idx\":%llu,\"capacity\":%u,\"start_seq\":%llu,\"entries\":[",
            (unsigned long long)write_idx, (unsigned)capacity,
            (unsigned long long)start_seq);
        out = hdr;
        bool first = true;
        for (int i = 0; i < count; i++) {
            uint64_t seq = start_seq + (uint64_t)i;
            if (seq > write_idx) break;
            uint64_t got_seq = 0;
            float fs[9] = {0};
            if (!rt64_vtx_ring_read(seq, &got_seq, fs)) continue;
            if (!first) out += ",";
            first = false;
            char buf[384];
            std::snprintf(buf, sizeof(buf),
                "{\"seq\":%llu,"
                "\"v0\":[%.6f,%.6f,%.6f],"
                "\"v1\":[%.6f,%.6f,%.6f],"
                "\"v2\":[%.6f,%.6f,%.6f]}",
                (unsigned long long)got_seq,
                fs[0], fs[1], fs[2],
                fs[3], fs[4], fs[5],
                fs[6], fs[7], fs[8]);
            out += buf;
        }
        out += "]}";
        return out;
    }
    if (cmd == "tmem_ring") {
        // Always-on TMEM-load ring (RT64 hle/rt64_tmem_ring). Each entry
        // is one RDP texture-load (loadTile/loadBlock/loadTLUT) at the
        // moment RT64 fills TMEM from RDRAM: tile fmt/siz, TMEM dest,
        // source RDRAM addr+span, SETTIMG base, dims, and a content hash
        // of the loaded bytes. Used to diagnose the menu cursor/icon
        // sprite corruption (stale TMEM / freed texture source).
        //   "count"     — entries to return (default 64)
        //   "start_seq" — starting sequence (default = write_idx-count+1)
        // Disable the writer with RT64_TMEM_RING_DISABLE=1.
        uint64_t write_idx = rt64_tmem_ring_write_idx();
        uint32_t capacity = rt64_tmem_ring_capacity();
        int count = get_int(line, "count", 64);
        if (count < 1) count = 1;
        if (count > 2048) count = 2048;
        int64_t start_seq_arg = (int64_t)get_int(line, "start_seq", -1);
        uint64_t start_seq;
        if (start_seq_arg < 0) {
            start_seq = (write_idx > (uint64_t)count) ? (write_idx - count + 1) : 1;
        } else {
            start_seq = (uint64_t)start_seq_arg;
        }
        const char* op_names[3] = {"tile", "block", "tlut"};
        std::string out;
        char hdr[256];
        std::snprintf(hdr, sizeof(hdr),
            "{\"ok\":true,\"write_idx\":%llu,\"capacity\":%u,\"start_seq\":%llu,\"entries\":[",
            (unsigned long long)write_idx, (unsigned)capacity,
            (unsigned long long)start_seq);
        out = hdr;
        bool first = true;
        for (int i = 0; i < count; i++) {
            uint64_t seq = start_seq + (uint64_t)i;
            if (seq > write_idx) break;
            uint64_t got_seq = 0, hash = 0;
            uint32_t u[12] = {0};
            if (!rt64_tmem_ring_read(seq, &got_seq, u, &hash)) continue;
            if (!first) out += ",";
            first = false;
            const char* opn = (u[0] < 3) ? op_names[u[0]] : "?";
            char buf[448];
            std::snprintf(buf, sizeof(buf),
                "{\"seq\":%llu,\"op\":\"%s\",\"fmt\":%u,\"siz\":%u,"
                "\"tmem_addr\":%u,\"tmem_bytes\":%u,\"src_addr\":%u,"
                "\"src_bytes\":%u,\"img_addr\":%u,\"width\":%u,\"rows\":%u,"
                "\"words_per_row\":%u,\"hash\":\"%016llx\"}",
                (unsigned long long)got_seq, opn, u[2], u[3], u[4], u[5],
                u[6], u[7], u[8], u[9], u[10], u[11],
                (unsigned long long)hash);
            out += buf;
        }
        out += "]}";
        return out;
    }
    if (cmd == "sprite_ring") {
        // Always-on sprite-draw ring (RT64 hle/rt64_tmem_ring). Each entry
        // is one drawTexRect: on-screen rect + tile + the texture source
        // RDRAM address. Map a corrupt on-screen sprite to its source by
        // position (e.g. move the menu selection; the cursor's rect moves).
        uint64_t write_idx = rt64_sprite_ring_write_idx();
        uint32_t capacity = rt64_sprite_ring_capacity();
        int count = get_int(line, "count", 128);
        if (count < 1) count = 1;
        if (count > 2048) count = 2048;
        int64_t start_seq_arg = (int64_t)get_int(line, "start_seq", -1);
        uint64_t start_seq;
        if (start_seq_arg < 0) {
            start_seq = (write_idx > (uint64_t)count) ? (write_idx - count + 1) : 1;
        } else {
            start_seq = (uint64_t)start_seq_arg;
        }
        std::string out;
        char hdr[256];
        std::snprintf(hdr, sizeof(hdr),
            "{\"ok\":true,\"write_idx\":%llu,\"capacity\":%u,\"start_seq\":%llu,\"entries\":[",
            (unsigned long long)write_idx, (unsigned)capacity,
            (unsigned long long)start_seq);
        out = hdr;
        bool first = true;
        for (int i = 0; i < count; i++) {
            uint64_t seq = start_seq + (uint64_t)i;
            if (seq > write_idx) break;
            uint64_t got_seq = 0;
            int32_t r[4] = {0};
            uint32_t u[2] = {0};
            if (!rt64_sprite_ring_read(seq, &got_seq, r, u)) continue;
            if (!first) out += ",";
            first = false;
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "{\"seq\":%llu,\"ulx\":%d,\"uly\":%d,\"lrx\":%d,\"lry\":%d,"
                "\"tile\":%u,\"src_addr\":%u}",
                (unsigned long long)got_seq, r[0], r[1], r[2], r[3], u[0], u[1]);
            out += buf;
        }
        out += "]}";
        return out;
    }
    if (cmd == "set_button") {
        auto name = get_str(line, "name");
        bool down = get_bool(line, "down", true);
        uint16_t bit = button_bit(name);
        if (bit == 0) return R"({"ok":false,"error":"unknown button name"})";
        g_input_override_active.store(true);
        uint16_t cur = g_buttons_override.load();
        if (down) cur |= bit; else cur &= ~bit;
        g_buttons_override.store(cur);
        record_input_event("button", name.c_str(), down ? 1 : 0);
        return R"({"ok":true})";
    }
    if (cmd == "set_stick") {
        g_input_override_active.store(true);
        int x = get_int(line, "x", 0);
        int y = get_int(line, "y", 0);
        g_stick_x_override.store(x);
        g_stick_y_override.store(y);
        record_input_event("stick", "", 0, x, y);
        return R"({"ok":true})";
    }
    if (cmd == "clear_input") {
        g_input_override_active.store(false);
        g_buttons_override.store(0);
        g_stick_x_override.store(0);
        g_stick_y_override.store(0);
        record_input_event("clear");
        return R"({"ok":true})";
    }
    if (cmd == "input_history") {
        // Returns full input history ring as JSON (for live inspection;
        // post-mortem also dumps this to build/last_run_input_history.json).
        std::lock_guard<std::mutex> lock(s_input_history_mu);
        uint64_t total = s_input_history_seq.load();
        uint64_t count = std::min(total, (uint64_t)INPUT_HISTORY_CAP);
        uint64_t start = (total > INPUT_HISTORY_CAP) ? (total - INPUT_HISTORY_CAP) : 0;
        std::string out = R"({"ok":true,"total":)" + std::to_string(total) +
                          R"(,"events":[)";
        for (uint64_t i = 0; i < count; i++) {
            const InputEvent& ev = s_input_history[(start + i) % INPUT_HISTORY_CAP];
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "%s{\"frame\":%llu,\"t_ms\":%llu,\"kind\":\"%s\","
                "\"name\":\"%s\",\"down\":%d,\"x\":%d,\"y\":%d}",
                (i ? "," : ""),
                (unsigned long long)ev.frame,
                (unsigned long long)ev.timestamp_ms,
                ev.kind, ev.name, ev.down, ev.x, ev.y);
            out += buf;
        }
        out += "]}";
        return out;
    }
    if (cmd == "memmap_ring") {
        // Always-on ring of gSegments[]/gFragments[] mutations (extras.c,
        // hooked on the four memmap.c mutators). kind: 0=SetSeg 1=ClearSeg
        // 2=SetFrag 3=ClearFrag. Each entry: id, vaddr, size, caller PC,
        // gCurrentGameState. Used to trace stale/wrong segment binding
        // behind the menu cursor/icon corruption (seg 1 -> fragment code).
        uint64_t total = pkmnstadium_memmap_seq();
        uint32_t cap = pkmnstadium_memmap_cap();
        int count = get_int(line, "count", 256);
        if (count < 1) count = 1;
        if (count > (int)cap) count = (int)cap;
        if ((uint64_t)count > total) count = (int)total;
        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(total) +
                          R"(,"capacity":)" + std::to_string(cap) + R"(,"entries":[)";
        bool first = true;
        for (int k = 0; k < count; k++) {
            uint64_t s = total - (uint64_t)count + (uint64_t)k;
            uint64_t seq = 0; uint32_t kind = 0, id = 0, vaddr = 0, size = 0,
                     caller = 0, gs = 0;
            pkmnstadium_memmap_get((uint32_t)(s % cap), &seq, &kind, &id,
                                   &vaddr, &size, &caller, &gs);
            if (seq != s) continue;  // entry rolled past since we sampled total
            if (!first) out += ",";
            first = false;
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "{\"seq\":%llu,\"kind\":%u,\"id\":%u,\"vaddr\":%u,\"size\":%u,"
                "\"caller\":%u,\"gs\":%u}",
                (unsigned long long)seq, kind, id, vaddr, size, caller, gs);
            out += buf;
        }
        out += "]}";
        return out;
    }
    if (cmd == "arcload_ring") {
        // Always-on ring of BinArchive consume (func_8000484C) + fragment
        // relocate dispatch (func_800043BC) events (extras.c). kind 0 =
        // aload: a=archive vaddr, b=file_number, h0=(unk_00<<16|unk_02),
        // h1=unk_04 (compressed source), h2=total_size, h3=num_files,
        // e0=file.offset, e1=file.size, e2=file.unk_08 (cache). kind 1 =
        // relocfrag: a=id (raw s32 indexing gFragments), b=Fragment vaddr,
        // h0/h1=magic ('FRAG'/'MENT'), h2=relocOffset, h3=sizeInRam. A
        // kind-1 with id outside [0,239] is the OOB gFragments[] write
        // behind the cursor/icon corruption (clobbers gSegments[1]).
        uint64_t total = pkmnstadium_arcload_seq();
        uint32_t cap = pkmnstadium_arcload_cap();
        int count = get_int(line, "count", 256);
        if (count < 1) count = 1;
        if (count > (int)cap) count = (int)cap;
        if ((uint64_t)count > total) count = (int)total;
        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(total) +
                          R"(,"capacity":)" + std::to_string(cap) + R"(,"entries":[)";
        bool first = true;
        for (int k = 0; k < count; k++) {
            uint64_t s = total - (uint64_t)count + (uint64_t)k;
            uint64_t seq = 0;
            uint32_t kind = 0, a = 0, b = 0, h0 = 0, h1 = 0, h2 = 0, h3 = 0,
                     e0 = 0, e1 = 0, e2 = 0, caller = 0, gs = 0;
            pkmnstadium_arcload_get((uint32_t)(s % cap), &seq, &kind, &a, &b,
                                    &h0, &h1, &h2, &h3, &e0, &e1, &e2, &caller, &gs);
            if (seq != s) continue;  // entry rolled past since we sampled total
            if (!first) out += ",";
            first = false;
            char buf[384];
            std::snprintf(buf, sizeof(buf),
                "{\"seq\":%llu,\"kind\":%u,\"a\":%u,\"b\":%u,\"h0\":%u,\"h1\":%u,"
                "\"h2\":%u,\"h3\":%u,\"e0\":%u,\"e1\":%u,\"e2\":%u,\"caller\":%u,"
                "\"gs\":%u}",
                (unsigned long long)seq, kind, a, b, h0, h1, h2, h3, e0, e1, e2,
                caller, gs);
            out += buf;
        }
        out += "]}";
        return out;
    }
    if (cmd == "arcload_badfrag") {
        // Non-evicting smoking-gun: the first func_800043BC dispatch with
        // a fragment id outside [0,239], paired with the BinArchive
        // (func_8000484C frame) that produced it. Answers "why is the
        // fragment id corrupt" with the full archive header + the
        // compressed source (unk_04) for reference decompression.
        uint32_t r_id = 0, r_frag = 0, m0 = 0, m1 = 0, ro = 0, sr = 0, rc = 0, rgs = 0;
        uint32_t a_arc = 0, a_hdr = 0, a_unk04 = 0, a_total = 0, a_nf = 0, a_fn = 0,
                 a_fo = 0, a_fs = 0, a_fu = 0, a_c = 0;
        int got = pkmnstadium_arcload_badfrag(&r_id, &r_frag, &m0, &m1, &ro, &sr,
                                              &rc, &rgs, &a_arc, &a_hdr, &a_unk04,
                                              &a_total, &a_nf, &a_fn, &a_fo, &a_fs,
                                              &a_fu, &a_c);
        if (!got) return std::string(R"({"ok":true,"captured":false})");
        char buf[768];
        std::snprintf(buf, sizeof(buf),
            "{\"ok\":true,\"captured\":true,"
            "\"reloc\":{\"id\":%u,\"frag\":%u,\"magic0\":%u,\"magic1\":%u,"
            "\"relocOffset\":%u,\"sizeInRam\":%u,\"caller\":%u,\"gs\":%u},"
            "\"archive\":{\"vaddr\":%u,\"unk00unk02\":%u,\"unk04\":%u,"
            "\"total_size\":%u,\"num_files\":%u,\"file_number\":%u,"
            "\"file_offset\":%u,\"file_size\":%u,\"file_unk08\":%u,\"caller\":%u}}",
            r_id, r_frag, m0, m1, ro, sr, rc, rgs,
            a_arc, a_hdr, a_unk04, a_total, a_nf, a_fn, a_fo, a_fs, a_fu, a_c);
        return std::string(buf);
    }
    if (cmd == "frag_section") {
        // Verification probe for the cursor/icon fix. For a Stadium
        // fragment id, report the section it last registered to, that
        // section's live section_addresses[] value, and its link-time
        // ram_addr. After Memmap_ClearFragmentMemmap the address should
        // fall back to the link literal (is_literal=true) so RELOC_HI16/
        // LO16 stop resolving to the stale runtime base. "id" required.
        uint32_t id = (uint32_t)get_int(line, "id", 0);
        uint32_t sidx = 0; int32_t saddr = 0, laddr = 0;
        int got = recomp_debug_runtime_fragment(id, &sidx, &saddr, &laddr);
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "{\"ok\":true,\"id\":%u,\"registered\":%s,\"section_index\":%u,"
            "\"section_addr\":%u,\"link_addr\":%u,\"is_literal\":%s}",
            id, got ? "true" : "false", sidx,
            (uint32_t)saddr, (uint32_t)laddr,
            (got && saddr == laddr) ? "true" : "false");
        return std::string(buf);
    }
    if (cmd == "interesting_fns") {
        // Returns the non-evicting interesting-function counters from
        // extras.c. Useful when the trace ring is dominated by audio
        // and game-thread activity gets evicted within ~10ms.
        int n = pkmnstadium_interesting_fn_total();
        std::string out = R"({"ok":true,"fns":[)";
        for (int i = 0; i < n; i++) {
            const char* name = pkmnstadium_interesting_fn_name(i);
            uint64_t cnt = pkmnstadium_interesting_fn_count(i);
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "%s{\"name\":\"%s\",\"count\":%llu}",
                (i ? "," : ""),
                name ? name : "?",
                (unsigned long long)cnt);
            out += buf;
        }
        out += "]}";
        return out;
    }
    if (cmd == "frag9") {
        // Read a fragment-9 (GB Tower) global by LINK offset, resolved via
        // section_addresses[9] to the runtime copy. Key target: the CPU-step
        // gate cRam024589ee @ 0x2C512 (nonzero => GB CPU frame-step is skipped
        // for a hardware-register busy-wait). Reads up to 8 consecutive 32-bit
        // words. Args: {"off":0x2C512,"words":1}
        uint32_t off = get_uint(line, "off", 0x2C512u);
        int words = get_int(line, "words", 1);
        if (words < 1) words = 1;
        if (words > 8) words = 8;
        uint8_t* rdram = recomp_runtime_get_rdram();
        if (rdram == nullptr) {
            return R"({"ok":false,"error":"rdram not yet captured"})";
        }
        std::string vals;
        for (int i = 0; i < words; i++) {
            char t[16];
            std::snprintf(t, sizeof(t), "%s\"0x%08X\"", i ? "," : "",
                          pkmnstadium_frag9_peek32(rdram, off + (uint32_t)i * 4u));
            vals += t;
        }
        char hdr[64];
        std::snprintf(hdr, sizeof(hdr), "0x%08X", pkmnstadium_frag9_base());
        return std::string("{\"ok\":true,\"frag9_base\":\"") + hdr +
               "\",\"off\":" + std::to_string(off) + ",\"words\":[" + vals + "]}";
    }
    if (cmd == "gbcart_ring") {
        // Dump the always-on gbcart block-I/O ring (every Transfer Pak cart
        // read/write + bank state + which ROM banks were walked). Compares the
        // GB-Tower cart-access pattern Red(MBC3) vs Yellow(MBC5): did the
        // emulator's full-cart read complete, or stall, and at which bank/addr.
        int tail = get_int(line, "tail", 400);
        const std::string path = "gbcart_ring.txt";
        librecomp::gbcart::ring_dump(path.c_str(), tail);
        return std::string("{\"ok\":true,\"path\":\"") + path + "\"}";
    }
    if (cmd == "gbtower_trace") {
        // Returns the last N GB Tower state-machine checkpoints captured
        // from func_8120572C and its helper calls. Tags are the original
        // MIPS PCs of the hook points.
        int n = get_int(line, "n", 128);
        if (n < 1) n = 1;
        uint64_t widx = pkmnstadium_gbtower_trace_seq();
        uint32_t cap = pkmnstadium_gbtower_trace_cap();
        if ((uint32_t)n > cap) n = (int)cap;
        if ((uint64_t)n > widx) n = (int)widx;

        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(widx) +
                          R"(,"cap":)" + std::to_string(cap) + R"(,"events":[)";
        for (int i = 0; i < n; i++) {
            uint64_t off = widx - n + i;
            GBTowerTraceEvent ev{};
            pkmnstadium_gbtower_trace_get((uint32_t)off, &ev);
            char buf[16384];
            std::snprintf(buf, sizeof(buf),
                "%s{\"seq\":%llu,\"tag\":%u,\"s0\":%u,\"ra\":%u,\"sp\":%u,"
                "\"regs\":[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u],"
                "\"state\":%u,\"phase\":%u,"
                "\"globals\":[%u,%u,%u,%u,%u,%u],"
                "\"global_extra\":[%u,%u,%u],"
                "\"global_bytes\":[%u,%u,%u,%u],"
                "\"ctrl\":[%u,%u,%u,%u],"
                "\"hdr\":[%u,%u,%u],"
                "\"ready\":[%u,%u],"
                "\"pad\":%u,"
                "\"words\":[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u],"
                "\"rom\":{\"w0140\":%u,\"w0144\":%u,\"w0148\":%u,\"w014c\":%u,\"type\":%u,\"type_flags\":%u},"
                "\"cpu\":{\"r2_byte\":%u,\"pc\":%u,\"pc_base\":%u,"
                "\"pc_addr\":%u,\"pc_byte\":%u,\"op_table0\":%u,\"op_handler\":%u},"
                "\"gbio\":{\"ff00_03\":%u,\"ff04_07\":%u,\"ff0c_0f\":%u,"
                "\"ff40_43\":%u,\"ff44_47\":%u,\"ff4c_4f\":%u,\"ff50_53\":%u,\"ff54_57\":%u,"
                "\"hram80_83\":%u,\"hram84_87\":%u,\"hram88_8b\":%u,\"hramd4_d7\":%u,\"hramfc_ff\":%u,"
                "\"h53d6\":%u,\"h53d8\":%u,\"h53da\":%u,\"h53dc\":%u,\"h53de\":%u,"
                "\"w53c0\":%u,\"w53c4\":%u,\"w53c8\":%u,"
                "\"save5c44\":%u,\"save5c48\":%u,\"save5c4c\":%u,\"save5c50\":%u,\"save5c54\":%u},"
                "\"gbstate\":{\"w5388\":%u,\"w538c\":%u,\"w53b8\":%u,\"w53cc\":%u,\"w53d0\":%u,"
                "\"w53f0\":%u,\"w53f2_53f5\":%u,\"w5484\":%u,\"w5488\":%u,\"w548c\":%u,"
                "\"map55ac\":[%u,%u,%u,%u,%u,%u,%u,%u],"
                "\"handlers55ec\":[%u,%u,%u,%u,%u,%u,%u,%u],\"w576c\":%u}}",
                (i ? "," : ""),
                (unsigned long long)ev.seq,
                ev.tag, ev.s0, ev.ra, ev.sp,
                ev.r2, ev.r3, ev.r4, ev.r5, ev.r6, ev.r7, ev.r8,
                ev.r9, ev.r10, ev.r11, ev.r12, ev.r13, ev.r14, ev.r15,
                ev.r16, ev.r17, ev.r18, ev.r19, ev.r20, ev.r21, ev.r22,
                ev.r23, ev.r24, ev.r25,
                ev.state, ev.phase,
                ev.g_c4e0, ev.g_c4e4, ev.g_c4f0, ev.g_b2b8, ev.g_c4f4, ev.g_c4f8,
                ev.g_c4e8, ev.g_c4ec, ev.g_c4fc,
                ev.g_c4e2, ev.g_c4f6, ev.g_c4f7, ev.g_c4f9,
                ev.s_ctrl0, ev.s_ctrl1, ev.s_ctrl2, ev.s_5dd0,
                ev.s_hdr0, ev.s_hdr1, ev.s_hdr2,
                ev.s_ready0, ev.s_ready1,
                ev.s_pad0,
                ev.w5390, ev.w5394, ev.w5398,
                ev.w53a4, ev.w53b4, ev.w53bc,
                ev.w5d64, ev.w5d68, ev.w5d6c, ev.w5d70, ev.w5d74, ev.w5d78, ev.w5d7c,
                ev.rom_0140, ev.rom_0144, ev.rom_0148, ev.rom_014c, ev.rom_type, ev.cart_type_flags,
                ev.ctx_r2_byte, ev.gb_pc, ev.gb_pc_base, ev.gb_pc_addr,
                ev.gb_pc_byte, ev.op_table0, ev.op_handler,
                ev.io_ff00_03, ev.io_ff04_07, ev.io_ff0c_0f,
                ev.io_ff40_43, ev.io_ff44_47, ev.io_ff4c_4f, ev.io_ff50_53, ev.io_ff54_57,
                ev.hram_ff80_83, ev.hram_ff84_87, ev.hram_ff88_8b, ev.hram_ffd4_d7, ev.hram_fffc_ff,
                ev.h53d6, ev.h53d8, ev.h53da, ev.h53dc, ev.h53de,
                ev.w53c0, ev.w53c4, ev.w53c8,
                ev.saved_5c44, ev.saved_5c48, ev.saved_5c4c, ev.saved_5c50, ev.saved_5c54,
                ev.w5388, ev.w538c, ev.w53b8, ev.w53cc, ev.w53d0, ev.w53f0, ev.w53f2_53f5,
                ev.w5484, ev.w5488, ev.w548c,
                ev.w55ac, ev.w55b0, ev.w55b4, ev.w55b8, ev.w55bc, ev.w55c0, ev.w55c4, ev.w55c8,
                ev.w55ec, ev.w55f0, ev.w55f4, ev.w55f8, ev.w55fc, ev.w5600, ev.w5604, ev.w5608,
                ev.w576c);
            out += buf;
        }
        out += "]}";
        return out;
    }
    if (cmd == "resolver_log") {
        // Returns the first N captured (arr, base, count) triplets from
        // libnaudio's offset->absolute lazy-resolver func_8003C204.
        // Phase 1 of audio crash diagnosis: lets us see whether the
        // struct that ends up as arg0->unk_090 in the synth ever got
        // resolved, and if so, which fields/counts.
        int n = pkmnstadium_resolver_log_total();
        std::string out = R"({"ok":true,"entries":[)";
        for (int i = 0; i < n; i++) {
            uint32_t arr = 0, base = 0, count = 0;
            pkmnstadium_resolver_log_get(i, &arr, &base, &count);
            char buf[96];
            std::snprintf(buf, sizeof(buf),
                "%s{\"arr\":\"0x%08X\",\"base\":\"0x%08X\",\"count\":%u}",
                (i ? "," : ""), arr, base, count);
            out += buf;
        }
        out += "]}";
        return out;
    }
    if (cmd == "claim_input") {
        // Arm the override flag without changing button/stick state.
        // Needed by automated harnesses so libultra's osContInit (which
        // runs once during boot, before any set_button) sees a
        // controller present in port 1. Must be sent BEFORE osContInit
        // — i.e. immediately after the TCP server accepts.
        g_input_override_active.store(true);
        record_input_event("claim");
        return R"({"ok":true})";
    }
    if (cmd == "fast_forward") {
        g_fast_forward.store(get_bool(line, "on", true));
        return R"({"ok":true})";
    }
    if (cmd == "enable_instant_present") {
        g_enable_instant_present_request.store(true);
        return R"({"ok":true})";
    }
    if (cmd == "trace_recent") {
        // Returns the last N entries from the function-trace ring
        // (TRACE_ENTRY / TRACE_RETURN hits). N defaults to 64; max 512.
        // Used to diagnose "where is the game looping?" — read it
        // periodically and the same names should keep cycling.
        int n = get_int(line, "n", 64);
        if (n < 1) n = 1;
        if (n > 512) n = 512;
        uint64_t widx = pkmnstadium_trace_write_idx();
        uint32_t cap = pkmnstadium_trace_capacity();
        if ((uint64_t)n > widx) n = (int)widx;
        if ((uint32_t)n > cap) n = (int)cap;
        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(widx) + R"(,"entries":[)";
        for (int i = 0; i < n; i++) {
            uint64_t off = widx - n + i;
            const char* name = pkmnstadium_trace_at(off);
            if (i) out += ",";
            out += "\"";
            out += (name ? name : "?");
            out += "\"";
        }
        out += "]}";
        return out;
    }
    if (cmd == "post_mortem_dump") {
        // On-demand dump — runner stays alive. Produces
        // build/last_run_report.json with status + rings + hardware
        // state + per-thread host stacks. Use this to inspect
        // softlocks (white-screen freezes) without restarting.
        psr_post_mortem_dump("on_demand", nullptr);
        return R"({"ok":true,"path":"build/last_run_report.json"})";
    }
    if (cmd == "mesg_recent") {
        // Returns the last N OSMesgQueue events from the always-on
        // ring in ultramodern. Used to diagnose softlocks where the
        // game thread blocks on a queue waiting for a completion msg
        // that didn't arrive.
        // MUST match mesg_log::Event in ultramodern/src/mesgqueue.cpp exactly
        // (the size guard below loudly rejects drift rather than mis-parsing).
        struct MesgEvent {
            uint64_t seq;
            uint64_t ms;
            uint32_t mq;
            uint32_t msg;
            uint32_t thread;
            uint16_t thread_id;
            uint16_t valid_before;
            uint16_t valid_after;
            uint8_t  op;
            uint8_t  block;
            uint8_t  game_thread;
            uint8_t  pad;
            uint16_t reserved;
        };
        if (ultramodern_mesg_event_size() != sizeof(MesgEvent)) {
            return R"({"ok":false,"error":"mesg event size mismatch"})";
        }
        int n = get_int(line, "n", 128);
        if (n < 1) n = 1;
        // Cap at the ring's actual capacity (bumped to 65536 in
        // ultramodern/mesgqueue.cpp). Larger pulls let us span menu
        // transitions without VI tick saturation.
        if (n > 65536) n = 65536;
        std::vector<MesgEvent> buf(n);
        size_t got = 0;
        uint64_t widx = 0;
        ultramodern_mesg_recent_copy(buf.data(), buf.size(), &got, &widx);
        const char* op_names[11] = {
            "?", "send_game", "send_external", "recv_enter",
            "recv_block", "recv_return_ok", "ext_deq_ok", "ext_deq_full",
            "do_send_block", "ext_deq_drop", "do_send_drop"
        };
        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(widx)
                        + R"(,"events":[)";
        for (size_t i = 0; i < got; i++) {
            const auto& e = buf[i];
            const char* opn = (e.op < 11) ? op_names[e.op] : "?";
            char b[320];
            std::snprintf(b, sizeof(b),
                "%s{\"seq\":%llu,\"ms\":%llu,\"op\":\"%s\",\"mq\":%u,"
                "\"msg\":%u,\"thread\":%u,\"tid\":%u,\"vb\":%u,\"va\":%u,"
                "\"block\":%u,\"gt\":%u}",
                (i ? "," : ""),
                (unsigned long long)e.seq, (unsigned long long)e.ms,
                opn, e.mq, e.msg, e.thread, (unsigned)e.thread_id,
                (unsigned)e.valid_before, (unsigned)e.valid_after,
                (unsigned)e.block, (unsigned)e.game_thread);
            out += b;
        }
        out += "]}";
        return out;
    }
    if (cmd == "mesg_queues") {
        // Per-QUEUE never-evict table from ultramodern: every OSMesgQueue the
        // game ever touched, with the last 64 events on each — survives the main
        // ring wrapping under the VI/audio flood. THE tool for a lost-wakeup:
        // "after thread X blocked on recv of queue Q, did anyone ever send to Q,
        // and was a send dropped (op do_send_drop)?"
        //   mq=<addr>  : dump just that queue's recent events (default: list all)
        //   tail=N     : events per queue when mq= is given (default 16, max 64)
        struct MesgEvent {  // MUST match mesg_log::Event (see mesg_recent)
            uint64_t seq; uint64_t ms; uint32_t mq; uint32_t msg;
            uint32_t thread; uint16_t thread_id; uint16_t valid_before;
            uint16_t valid_after; uint8_t op; uint8_t block; uint8_t game_thread;
            uint8_t pad; uint16_t reserved;
        };
        struct QState { uint32_t queue; uint32_t count; MesgEvent last[64]; };
        if (ultramodern_mesg_event_size() != sizeof(MesgEvent) ||
            ultramodern_mesg_qstate_size() != sizeof(QState)) {
            return R"({"ok":false,"error":"mesg qstate size mismatch"})";
        }
        const uint32_t want_mq = get_uint(line, "mq", 0);  // 0 = list all queues
        int tail = get_int(line, "tail", 16);
        if (tail < 1) tail = 1;
        if (tail > 64) tail = 64;
        const size_t QCAP = 1024;
        std::vector<QState> qs(QCAP);
        size_t got = 0;
        ultramodern_mesg_qstates_copy(qs.data(), qs.size(), &got);
        const char* op_names[11] = {
            "?", "send_game", "send_external", "recv_enter",
            "recv_block", "recv_return_ok", "ext_deq_ok", "ext_deq_full",
            "do_send_block", "ext_deq_drop", "do_send_drop"
        };
        std::string out = R"({"ok":true,"queues":[)";
        bool first = true;
        for (size_t i = 0; i < got; i++) {
            const QState& q = qs[i];
            if (q.queue == 0) continue;
            if (want_mq != 0 && q.queue != want_mq) continue;
            if (!first) out += ",";
            first = false;
            char hdr[96];
            std::snprintf(hdr, sizeof(hdr), "{\"mq\":%u,\"count\":%u,\"events\":[",
                          q.queue, q.count);
            out += hdr;
            // When listing all queues, show only the latest op per queue to keep
            // output bounded; when filtered to one queue, show the last `tail`.
            uint32_t n = (want_mq == 0) ? (q.count ? 1u : 0u)
                                        : std::min<uint32_t>(q.count, (uint32_t)tail);
            uint32_t startc = q.count - n;
            for (uint32_t k = 0; k < n; k++) {
                const MesgEvent& e = q.last[(startc + k) & 63];
                const char* opn = (e.op < 11) ? op_names[e.op] : "?";
                char b[300];
                std::snprintf(b, sizeof(b),
                    "%s{\"seq\":%llu,\"op\":\"%s\",\"msg\":%u,\"tid\":%u,"
                    "\"vb\":%u,\"va\":%u,\"block\":%u,\"gt\":%u}",
                    (k ? "," : ""), (unsigned long long)e.seq, opn, e.msg,
                    (unsigned)e.thread_id, (unsigned)e.valid_before,
                    (unsigned)e.valid_after, (unsigned)e.block,
                    (unsigned)e.game_thread);
                out += b;
            }
            out += "]}";
        }
        out += "]}";
        return out;
    }
    if (cmd == "coverage") {
        // Live self-healing-tier counters straight from the atomics (the
        // runtime_captures.json manifest only flushes on a NEW unique miss, so
        // it goes stale mid-run). Lets a probe watch dispatch_entry_rejects /
        // interp_runs / self_heals climb during a stall in real time — to tell a
        // silently-self-healed dispatch reject from a truly clean dispatch.
        uint64_t sh=0, lm=0, heals=0, hmiss=0, rej=0, interp=0, jc=0, jf=0;
        recomp_coverage_live(&sh, &lm, &heals, &hmiss, &rej, &interp, &jc, &jf);
        uint64_t bubbles = recomp_bubble_dispatch_count();
        uint64_t hrbreaks = recomp_host_return_break_count();
        uint32_t hrbreak_last = recomp_host_return_break_last_target();
        uint32_t hrbreak_depth = recomp_host_return_break_last_depth();
        char b[700];
        std::snprintf(b, sizeof(b),
            "{\"ok\":true,\"static_dispatch_hits\":%llu,\"lookup_misses\":%llu,"
            "\"self_heals\":%llu,\"self_heal_misses\":%llu,"
            "\"dispatch_entry_rejects\":%llu,\"interp_runs\":%llu,"
            "\"jit_compiles\":%llu,\"jit_failures\":%llu,"
            "\"tailcall_bubble_dispatches\":%llu,"
            "\"host_return_breaks\":%llu,\"host_return_break_last_target\":\"0x%08X\","
            "\"host_return_break_last_depth\":%u}",
            (unsigned long long)sh, (unsigned long long)lm,
            (unsigned long long)heals, (unsigned long long)hmiss,
            (unsigned long long)rej, (unsigned long long)interp,
            (unsigned long long)jc, (unsigned long long)jf,
            (unsigned long long)bubbles,
            (unsigned long long)hrbreaks, (unsigned)hrbreak_last, (unsigned)hrbreak_depth);
        return std::string(b);
    }
    if (cmd == "sp_task_recent") {
        // Returns the last N osSpTaskStartGo events from the always-on
        // ring in librecomp/sp.cpp. Used to identify the LAST gfx task
        // submitted before send_dl freezes — answers the gfx-submit-
        // freeze question at frame ~2400 / send_dl ~1157.
        struct SpTaskEvent {
            uint64_t seq;
            uint64_t ms;
            uint64_t frame;
            uint64_t send_dl;
            uint32_t mips_ra;
            uint32_t task_ptr;
            uint32_t wrapper_ptr;
            uint32_t suspect;
            uint32_t task_type;
            uint32_t task_flags;
            uint32_t ucode;
            uint32_t data_ptr;
            uint32_t data_size;
            uint32_t output_buff;
            uint32_t output_buff_size;
            uint32_t wrapper_words[12];
            uint32_t task_words[16];
        };
        if (recomp_sp_task_event_size() != sizeof(SpTaskEvent)) {
            return R"({"ok":false,"error":"sp_task event size mismatch"})";
        }
        int n = get_int(line, "n", 128);
        if (n < 1) n = 1;
        if (n > 4096) n = 4096;
        std::vector<SpTaskEvent> buf(n);
        size_t got = 0;
        uint64_t widx = 0;
        recomp_sp_task_recent_copy(buf.data(), buf.size(), &got, &widx);
        auto type_name = [](uint32_t t) -> const char* {
            switch (t) {
                case 1: return "M_GFXTASK";
                case 2: return "M_AUDTASK";
                case 3: return "M_VIDTASK";
                case 4: return "M_NJPEGTASK";
                case 6: return "M_HVQMTASK";
                default: return "?";
            }
        };
        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(widx)
                        + R"(,"events":[)";
        auto append_words = [](std::string& dst, const char* key,
                               const uint32_t* words, size_t count) {
            dst += ",\"";
            dst += key;
            dst += "\":[";
            for (size_t j = 0; j < count; j++) {
                if (j) dst += ",";
                dst += std::to_string(words[j]);
            }
            dst += "]";
        };
        for (size_t i = 0; i < got; i++) {
            const auto& e = buf[i];
            char b[512];
            std::snprintf(b, sizeof(b),
                "%s{\"seq\":%llu,\"ms\":%llu,\"frame\":%llu,\"send_dl\":%llu,"
                "\"type\":\"%s\",\"task_ptr\":%u,\"wrapper_ptr\":%u,\"mips_ra\":%u,"
                "\"ucode\":%u,\"data_ptr\":%u,\"data_size\":%u,"
                "\"output_buff\":%u,\"output_buff_size\":%u,\"flags\":%u,"
                "\"suspect\":%u",
                (i ? "," : ""),
                (unsigned long long)e.seq, (unsigned long long)e.ms,
                (unsigned long long)e.frame, (unsigned long long)e.send_dl,
                type_name(e.task_type), e.task_ptr, e.wrapper_ptr, e.mips_ra,
                e.ucode, e.data_ptr, e.data_size,
                e.output_buff, e.output_buff_size, e.task_flags, e.suspect);
            out += b;
            if (e.suspect != 0) {
                append_words(out, "wrapper_words", e.wrapper_words, 12);
                append_words(out, "task_words", e.task_words, 16);
            }
            out += "}";
        }
        out += "]}";
        return out;
    }
    if (cmd == "rsp_dma_recent") {
        // Optional ring from librecomp/rsp.cpp. Enable with
        // PSR_RSP_DMA_TRACE=1 before launching the runner.
        int n = get_int(line, "n", 256);
        if (n < 1) n = 1;
        if (n > 4096) n = 4096;
        std::vector<RspDmaTraceEvent> buf((size_t)n);
        uint32_t got = 0;
        uint64_t widx = 0;
        recomp_rsp_dma_recent_copy(buf.data(), (uint32_t)buf.size(), &got, &widx);
        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(widx)
                        + R"(,"events":[)";
        for (uint32_t i = 0; i < got; i++) {
            const auto& e = buf[i];
            char first_hex[33];
            for (int j = 0; j < 16; j++) {
                std::snprintf(first_hex + j * 2, 3, "%02x", e.first16[j]);
            }
            char b[384];
            std::snprintf(b, sizeof(b),
                "%s{\"seq\":%llu,\"dir\":\"%s\",\"dmem\":%u,\"dram\":%u,"
                "\"len\":%u,\"nonzero\":%u,\"first_nonzero\":%d,"
                "\"first16\":\"%s\"}",
                (i ? "," : ""),
                (unsigned long long)e.seq,
                e.write ? "write" : "read",
                e.dmem_addr, e.dram_addr, e.byte_count,
                e.nonzero_bytes, e.first_nonzero, first_hex);
            out += b;
        }
        out += "]}";
        return out;
    }
    if (cmd == "voice_events_recent") {
        // Returns the last N libnaudio voice events (key-on / key-off /
        // sample-change) from the always-on ring in librecomp's
        // audio_uaf_protect.cpp. Used to correlate the music-rate
        // periodic click against voice-start cadence and to inspect the
        // ADPCM predictor carry / dc_first flag at each key-on.
        //
        // Mirror of voice_ring::VoiceEvent — kept in lockstep and
        // validated via recomp_voice_event_size().
        struct VoiceEvent {
            uint64_t seq;
            uint64_t ms;
            uint64_t pass;
            uint32_t voice_ptr;
            uint32_t kind;
            uint32_t em_motion;
            uint32_t prev_motion;
            uint32_t dc_table;
            uint32_t prev_table;
            uint32_t dc_sample;
            uint32_t dc_first;
            int32_t  dc_lastsam;
            uint32_t dc_state;
            int16_t  carry0;
            int16_t  carry1;
            uint32_t wt_base;
            uint32_t wt_len;
            uint32_t wt_type;
            uint32_t loop_start;
            uint32_t loop_end;
            uint32_t loop_count;
        };
        if (recomp_voice_event_size() != sizeof(VoiceEvent)) {
            return R"({"ok":false,"error":"voice event size mismatch"})";
        }
        int n = get_int(line, "n", 128);
        if (n < 1) n = 1;
        if (n > 16384) n = 16384;
        std::vector<VoiceEvent> buf(n);
        size_t got = 0;
        uint64_t widx = 0;
        recomp_voice_events_recent_copy(buf.data(), buf.size(), &got, &widx);
        auto kind_name = [](uint32_t k) -> const char* {
            switch (k) {
                case 0: return "attack";
                case 1: return "key_off";
                case 2: return "realloc";
                default: return "?";
            }
        };
        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(widx)
                        + R"(,"events":[)";
        for (size_t i = 0; i < got; i++) {
            const auto& e = buf[i];
            char b[640];
            std::snprintf(b, sizeof(b),
                "%s{\"seq\":%llu,\"ms\":%llu,\"pass\":%llu,\"voice\":%u,"
                "\"kind\":\"%s\",\"em_motion\":%u,\"prev_motion\":%u,"
                "\"dc_table\":%u,\"prev_table\":%u,\"dc_sample\":%u,"
                "\"dc_first\":%u,\"dc_lastsam\":%d,\"dc_state\":%u,"
                "\"carry0\":%d,\"carry1\":%d,\"wt_base\":%u,\"wt_len\":%u,"
                "\"wt_type\":%u,\"loop_start\":%u,\"loop_end\":%u,"
                "\"loop_count\":%u}",
                (i ? "," : ""),
                (unsigned long long)e.seq, (unsigned long long)e.ms,
                (unsigned long long)e.pass, e.voice_ptr,
                kind_name(e.kind), e.em_motion, e.prev_motion,
                e.dc_table, e.prev_table, e.dc_sample,
                e.dc_first, (int)e.dc_lastsam, e.dc_state,
                (int)e.carry0, (int)e.carry1, e.wt_base, e.wt_len,
                e.wt_type, e.loop_start, e.loop_end, e.loop_count);
            out += b;
        }
        out += "]}";
        return out;
    }
    if (cmd == "adpcm_decode_recent") {
        // Returns the last N ADPCM-decode commands parsed from the audio
        // command list in librecomp's audio_uaf_protect.cpp. This is the
        // ground-truth A_INIT decision per decode (the task-time voice
        // ring cannot see it). `suspect`=1 marks a CONTINUE decode onto a
        // book that changed vs the prior decode on the same state buffer
        // — i.e. stale predictor loaded for a new sample (click site).
        struct AdpcmEvent {
            uint64_t seq;
            uint64_t ms;
            uint64_t frame;
            uint32_t state;
            uint32_t book;
            uint32_t prev_book;
            uint32_t flags;
            uint32_t init;
            uint32_t count;
            uint32_t suspect;
        };
        if (recomp_adpcm_decode_event_size() != sizeof(AdpcmEvent)) {
            return R"({"ok":false,"error":"adpcm event size mismatch"})";
        }
        int n = get_int(line, "n", 256);
        if (n < 1) n = 1;
        if (n > 16384) n = 16384;
        std::vector<AdpcmEvent> buf(n);
        size_t got = 0;
        uint64_t widx = 0;
        recomp_adpcm_decode_recent_copy(buf.data(), buf.size(), &got, &widx);
        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(widx)
                        + R"(,"events":[)";
        for (size_t i = 0; i < got; i++) {
            const auto& e = buf[i];
            char b[384];
            std::snprintf(b, sizeof(b),
                "%s{\"seq\":%llu,\"ms\":%llu,\"frame\":%llu,\"state\":%u,"
                "\"book\":%u,\"prev_book\":%u,\"flags\":%u,\"init\":%u,"
                "\"count\":%u,\"suspect\":%u}",
                (i ? "," : ""),
                (unsigned long long)e.seq, (unsigned long long)e.ms,
                (unsigned long long)e.frame, e.state, e.book, e.prev_book,
                e.flags, e.init, e.count, e.suspect);
            out += b;
        }
        out += "]}";
        return out;
    }
    if (cmd == "audio_queue_recent") {
        // Returns the last N host-side queue_samples() events from the
        // always-on ring in main.cpp. `decimated`=1 means skip_factor
        // sample-dropping fired (a click candidate). `queued_us` is the
        // SDL queue depth; decimation triggers above ~100000us.
        struct AudioQueueEvent {
            uint64_t seq;
            uint64_t ms;
            uint32_t sample_count;
            uint32_t sample_rate;
            uint32_t queued_us;
            uint32_t skip_factor;
            uint32_t bytes_queued;
            uint32_t decimated;
        };
        if (recomp_audio_queue_event_size() != sizeof(AudioQueueEvent)) {
            return R"({"ok":false,"error":"audio queue event size mismatch"})";
        }
        int n = get_int(line, "n", 256);
        if (n < 1) n = 1;
        if (n > 8192) n = 8192;
        std::vector<AudioQueueEvent> buf(n);
        size_t got = 0;
        uint64_t widx = 0;
        recomp_audio_queue_recent_copy(buf.data(), buf.size(), &got, &widx);
        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(widx)
                        + R"(,"events":[)";
        for (size_t i = 0; i < got; i++) {
            const auto& e = buf[i];
            char b[320];
            std::snprintf(b, sizeof(b),
                "%s{\"seq\":%llu,\"ms\":%llu,\"sample_count\":%u,"
                "\"sample_rate\":%u,\"queued_us\":%u,\"skip_factor\":%u,"
                "\"bytes_queued\":%u,\"decimated\":%u}",
                (i ? "," : ""),
                (unsigned long long)e.seq, (unsigned long long)e.ms,
                e.sample_count, e.sample_rate, e.queued_us, e.skip_factor,
                e.bytes_queued, e.decimated);
            out += b;
        }
        out += "]}";
        return out;
    }
    if (cmd == "ai_submit_recent") {
        // The playback stream: guest address + size of every submitted
        // audio buffer, in submission (= playback) order. Joining these
        // addresses against task_pcm entries' dram attributes played PCM
        // to the exact task outputs that composed it.
        if (ultramodern_ai_submit_event_size() != sizeof(AiSubmitEventMirror)) {
            return R"({"ok":false,"error":"ai submit event size mismatch"})";
        }
        int n = get_int(line, "n", 256);
        if (n < 1) n = 1;
        if (n > 16384) n = 16384;
        std::vector<AiSubmitEventMirror> buf(n);
        size_t got = 0;
        uint64_t widx = 0;
        ultramodern_ai_submit_recent_copy(buf.data(), buf.size(), &got, &widx);
        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(widx)
                        + R"(,"events":[)";
        for (size_t i = 0; i < got; i++) {
            const auto& e = buf[i];
            char b[160];
            std::snprintf(b, sizeof(b),
                "%s{\"seq\":%llu,\"ms\":%llu,\"addr\":%u,\"bytes\":%u}",
                (i ? "," : ""),
                (unsigned long long)e.seq, (unsigned long long)e.ms,
                e.guest_addr, e.byte_count);
            out += b;
        }
        out += "]}";
        return out;
    }
    if (cmd == "task_pcm_dump") {
        // Dump the whole always-on per-task output-PCM ring to a file as
        // raw TaskPcmEvent records (oldest -> newest) for offline seam
        // analysis. Requires the session to run with PSR_RSP_DMA_TRACE=1
        // (the ring locates each task's final WR via the DMA trace).
        std::string p = get_str(line, "path");
        if (p.empty()) return R"({"ok":false,"error":"missing path"})";
        int64_t n = recomp_task_pcm_dump(p.c_str());
        if (n < 0) return R"({"ok":false,"error":"open failed"})";
        return R"({"ok":true,"records":)" + std::to_string(n)
             + R"(,"record_size":)" + std::to_string(recomp_task_pcm_event_size())
             + "}";
    }
    if (cmd == "task_pcm_recent") {
        // Light stats over the last N task outputs: seam-relevant fields
        // only (first/last frame per channel); full samples come from
        // task_pcm_dump.
        if (recomp_task_pcm_event_size() != sizeof(TaskPcmEventMirror)) {
            return R"({"ok":false,"error":"task pcm event size mismatch"})";
        }
        int n = get_int(line, "n", 64);
        if (n < 1) n = 1;
        if (n > 2048) n = 2048;
        std::vector<TaskPcmEventMirror> buf(n);
        size_t got = 0;
        uint64_t widx = 0;
        recomp_task_pcm_recent_copy(buf.data(), buf.size(), &got, &widx);
        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(widx)
                        + R"(,"events":[)";
        for (size_t i = 0; i < got; i++) {
            const auto& e = buf[i];
            uint32_t n_samp = e.len / 2;
            if (n_samp > 368) n_samp = 368;
            int fl = n_samp >= 2 ? e.samples[0] : 0;
            int fr = n_samp >= 2 ? e.samples[1] : 0;
            int ll = n_samp >= 2 ? e.samples[n_samp - 2] : 0;
            int lr = n_samp >= 2 ? e.samples[n_samp - 1] : 0;
            char b[320];
            std::snprintf(b, sizeof(b),
                "%s{\"seq\":%llu,\"ms\":%llu,\"dram\":%u,\"len\":%u,"
                "\"exit\":%u,\"n_dmas\":%u,"
                "\"first_l\":%d,\"first_r\":%d,\"last_l\":%d,\"last_r\":%d}",
                (i ? "," : ""),
                (unsigned long long)e.seq, (unsigned long long)e.ms,
                e.dram, e.len, e.exit_reason, e.n_dmas, fl, fr, ll, lr);
            out += b;
        }
        out += "]}";
        return out;
    }
    if (cmd == "aspmain_capture_arm") {
        // Arm (or re-arm) the one-shot aspMain single-task replay capture
        // at runtime — same machinery as PSR_ASPMAIN_CAPTURE=<dir>, but
        // triggerable mid-session so the captured task is the content
        // under investigation (e.g. a battle cry) rather than whatever
        // non-silent task fires first after boot. The next aspMain task
        // that changes >= PSR_ASPMAIN_MIN_DELTA (default 4096) RDRAM
        // bytes dumps dmem/ctx/rdram-before/rdram-after/recomp_dma to
        // `dir`. Poll completion via aspmain_capture_status.
        std::string dir = get_str(line, "dir");
        if (dir.empty()) {
            return R"({"ok":false,"error":"missing dir"})";
        }
        int armed = psr_aspmain_capture_arm(dir.c_str());
        return std::string(R"({"ok":)") + (armed ? "true" : "false")
             + R"(,"state":)" + std::to_string(psr_aspmain_capture_state()) + "}";
    }
    if (cmd == "aspmain_capture_status") {
        // -2 unqueried, -1 disabled, 0 armed (waiting for a non-silent
        // task), 1 done (dump files written).
        return R"({"ok":true,"state":)"
             + std::to_string(psr_aspmain_capture_state()) + "}";
    }
    if (cmd == "audio_pcm_recent") {
        // Returns the last N synthesized-PCM events from the always-on
        // ring in main.cpp — the RAW int16 audio the game produced before
        // host conversion/volume. `mean_abs_d2`/`mean_abs` (HF ratio)
        // localizes the "static": high jaggedness vs amplitude => the
        // static is baked into the synthesized samples (aspMain/mixing),
        // not the host path. `window` is a raw one-channel snapshot.
        struct AudioPcmEvent {
            uint64_t seq;
            uint64_t ms;
            uint32_t sample_count;
            int32_t  min_sample;
            int32_t  max_sample;
            uint32_t mean_abs;
            uint32_t mean_abs_d1;
            uint32_t max_abs_d1;
            uint32_t mean_abs_d2;
            uint32_t max_abs_d2;
            uint32_t out_hf_milli;
            int16_t  window[32];
        };
        if (recomp_audio_pcm_event_size() != sizeof(AudioPcmEvent) ||
            recomp_audio_pcm_window() != 32) {
            return R"({"ok":false,"error":"audio pcm event size/window mismatch"})";
        }
        int n = get_int(line, "n", 128);
        if (n < 1) n = 1;
        if (n > 4096) n = 4096;
        std::vector<AudioPcmEvent> buf(n);
        size_t got = 0;
        uint64_t widx = 0;
        recomp_audio_pcm_recent_copy(buf.data(), buf.size(), &got, &widx);
        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(widx)
                        + R"(,"window":32,"events":[)";
        for (size_t i = 0; i < got; i++) {
            const auto& e = buf[i];
            char b[512];
            std::snprintf(b, sizeof(b),
                "%s{\"seq\":%llu,\"ms\":%llu,\"sample_count\":%u,"
                "\"min\":%d,\"max\":%d,\"mean_abs\":%u,"
                "\"mean_abs_d1\":%u,\"max_abs_d1\":%u,"
                "\"mean_abs_d2\":%u,\"max_abs_d2\":%u,\"out_hf_milli\":%u,\"window\":[",
                (i ? "," : ""),
                (unsigned long long)e.seq, (unsigned long long)e.ms,
                e.sample_count, e.min_sample, e.max_sample, e.mean_abs,
                e.mean_abs_d1, e.max_abs_d1, e.mean_abs_d2, e.max_abs_d2,
                e.out_hf_milli);
            out += b;
            for (int w = 0; w < 32; w++) {
                out += std::to_string((int)e.window[w]);
                if (w != 31) out += ",";
            }
            out += "]}";
        }
        out += "]}";
        return out;
    }
    if (cmd == "libultra_recent") {
        // Returns the last N libultra-call events from the ring in
        // librecomp. Each event: function name, caller PC ($ra),
        // a0..a3, ms-since-first-event, monotonic seq.
        //
        // Per the global "ring buffer" rule the ring is always-on
        // (no arming). This command just queries backward over the
        // window of interest. With cap=4096 and typical call rates
        // this gives plenty of history relative to LLM tool latency.
        int n = get_int(line, "n", 64);
        if (n < 1) n = 1;
        if (n > 1024) n = 1024;
        uint64_t widx = recomp_ultra_trace_write_idx();
        uint32_t cap  = recomp_ultra_trace_capacity();
        if ((uint64_t)n > widx) n = (int)widx;
        if ((uint32_t)n > cap)  n = (int)cap;

        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(widx)
                        + R"(,"events":[)";
        for (int i = 0; i < n; i++) {
            uint64_t off = widx - n + i;
            recomp_ultra_trace_event ev{};
            int valid = recomp_ultra_trace_get(off, &ev);
            char buf[320];
            // Escape name minimally — known to be ASCII C symbols.
            std::snprintf(buf, sizeof(buf),
                "%s{\"i\":%llu,\"valid\":%s,\"name\":\"%s\","
                "\"pc\":%u,\"a0\":%u,\"a1\":%u,\"a2\":%u,\"a3\":%u,"
                "\"ms\":%llu}",
                (i ? "," : ""),
                (unsigned long long)off,
                valid ? "true" : "false",
                ev.name,
                (unsigned)ev.pc,
                (unsigned)ev.a0,
                (unsigned)ev.a1,
                (unsigned)ev.a2,
                (unsigned)ev.a3,
                (unsigned long long)ev.ms);
            out += buf;
        }
        out += "]}";
        return out;
    }
    if (cmd == "libultra_boot") {
        // Returns a slice [start, start+n) from the non-evicting
        // boot snapshot — the FIRST n events recorded since process
        // start. Use this to answer "what did each thread do during
        // boot?" no matter how long the game has been running.
        //
        // Args: {"start": <pos>, "n": <count>}
        //   start: position into the boot buffer (default 0)
        //   n:     max events to return (default 256, max 1024)
        //
        // The "name" field in each event reveals which thread
        // (audio/scheduler/game) was active by which calls it
        // makes; the "ms" field gives ordering relative to other
        // threads.
        int start = get_int(line, "start", 0);
        int n     = get_int(line, "n", 256);
        if (start < 0) start = 0;
        if (n < 1) n = 1;
        if (n > 1024) n = 1024;
        uint32_t total = recomp_ultra_trace_boot_count();
        uint32_t cap   = recomp_ultra_trace_boot_capacity();
        std::string out =
            R"({"ok":true,"count":)" + std::to_string(total) +
            R"(,"capacity":)" + std::to_string(cap) +
            R"(,"start":)" + std::to_string(start) +
            R"(,"events":[)";
        bool first = true;
        for (int i = 0; i < n; i++) {
            uint32_t pos = (uint32_t)start + (uint32_t)i;
            if (pos >= total) break;
            recomp_ultra_trace_event ev{};
            int valid = recomp_ultra_trace_boot_get(pos, &ev);
            char buf[320];
            std::snprintf(buf, sizeof(buf),
                "%s{\"i\":%u,\"valid\":%s,\"name\":\"%s\","
                "\"pc\":%u,\"a0\":%u,\"a1\":%u,\"a2\":%u,\"a3\":%u,"
                "\"ms\":%llu}",
                (first ? "" : ","),
                pos,
                valid ? "true" : "false",
                ev.name,
                (unsigned)ev.pc,
                (unsigned)ev.a0,
                (unsigned)ev.a1,
                (unsigned)ev.a2,
                (unsigned)ev.a3,
                (unsigned long long)ev.ms);
            out += buf;
            first = false;
        }
        out += "]}";
        return out;
    }
    if (cmd == "rdram_peek") {
        // Read N bytes from rdram at a virtual address. Generic
        // diagnostic — useful for inspecting any global state in
        // the recompiled game (struct fields, flags, queue
        // counts, etc.) without needing to add a per-field TCP
        // command for each.
        //
        // Args: {"addr": <vaddr>, "n": <count>}
        //   addr: K0/K1 vaddr (0x80000000+ or 0xA0000000+)
        //         physical addr also accepted (interpreted as KSEG0).
        //   n:    bytes to read, 1..256.
        //
        // Returns hex string (big-endian byte order from N64's
        // perspective — we XOR-3 the rdram index per the
        // recompiler's swap convention).
        uint32_t addr = get_uint(line, "addr", 0);
        int n = get_int(line, "n", 4);
        if (n < 1) n = 1;
        if (n > 256) n = 256;

        // Mask off K0/K1 segment bits, then bounds-check.
        uint32_t paddr = addr & 0x1FFFFFFFu;
        constexpr uint32_t kRdramSize = 8u * 1024u * 1024u;
        if (paddr + (uint32_t)n > kRdramSize) {
            return R"({"ok":false,"error":"oob"})";
        }
        // The recompiler uses XOR-3 byte addressing (big-endian
        // word view of little-endian native rdram). Replicate
        // that here to match what the game sees.
        uint8_t* rdram = recomp_runtime_get_rdram();
        if (rdram == nullptr) {
            return R"({"ok":false,"error":"rdram not yet captured"})";
        }
        std::string hex;
        hex.reserve((size_t)n * 2 + 4);
        char tmp[4];
        for (int i = 0; i < n; i++) {
            uint8_t b = rdram[(paddr + (uint32_t)i) ^ 3];
            std::snprintf(tmp, sizeof(tmp), "%02x", b);
            hex += tmp;
        }
        return R"({"ok":true,"addr":)" + std::to_string(addr) +
               R"(,"n":)" + std::to_string(n) +
               R"(,"hex":")" + hex + R"("})";
    }
    if (cmd == "rdram_digest") {
        // Summarize a larger RDRAM range without returning the whole
        // payload. Args: {"addr": <vaddr>, "n": <count>} with n capped
        // to 256 KiB so route probes stay cheap and bounded.
        uint32_t addr = get_uint(line, "addr", 0);
        int n = get_int(line, "n", 4);
        if (n < 1) n = 1;
        if (n > 0x40000) n = 0x40000;

        uint32_t paddr = addr & 0x1FFFFFFFu;
        constexpr uint32_t kRdramSize = 8u * 1024u * 1024u;
        if (paddr + (uint32_t)n > kRdramSize) {
            return R"({"ok":false,"error":"oob"})";
        }

        uint8_t* rdram = recomp_runtime_get_rdram();
        if (rdram == nullptr) {
            return R"({"ok":false,"error":"rdram not yet captured"})";
        }

        bool seen[256] = {};
        uint32_t unique = 0;
        uint32_t nonzero = 0;
        int first_nonzero = -1;
        int last_nonzero = -1;
        uint64_t fnv = 1469598103934665603ull;

        std::string first_hex;
        first_hex.reserve(128);
        char tmp[4];
        for (int i = 0; i < n; i++) {
            uint8_t b = rdram[(paddr + (uint32_t)i) ^ 3];
            if (i < 64) {
                std::snprintf(tmp, sizeof(tmp), "%02x", b);
                first_hex += tmp;
            }
            if (!seen[b]) {
                seen[b] = true;
                unique++;
            }
            if (b != 0) {
                nonzero++;
                if (first_nonzero < 0) {
                    first_nonzero = i;
                }
                last_nonzero = i;
            }
            fnv ^= b;
            fnv *= 1099511628211ull;
        }

        char hash_buf[32];
        std::snprintf(hash_buf, sizeof(hash_buf), "%016" PRIx64, fnv);
        return R"({"ok":true,"addr":)" + std::to_string(addr) +
               R"(,"n":)" + std::to_string(n) +
               R"(,"nonzero_bytes":)" + std::to_string(nonzero) +
               R"(,"unique_bytes":)" + std::to_string(unique) +
               R"(,"first_nonzero":)" + std::to_string(first_nonzero) +
               R"(,"last_nonzero":)" + std::to_string(last_nonzero) +
               R"(,"fnv64":")" + hash_buf +
               R"(","first64":")" + first_hex + R"("})";
    }
    if (cmd == "rdram_poke") {
        // Write N bytes to rdram at a virtual address. Companion to
        // rdram_peek. Used to live-test runtime patches (e.g. mutate
        // Vtx coords on a running build to check whether a proposed
        // seam-overlap fix actually clears a visual artifact, before
        // committing the patch to extras.c + game.toml).
        //
        // Args: {"addr": <vaddr>, "hex": "<hex_bytes>"}
        //   addr: K0/K1 vaddr; physical also accepted.
        //   hex:  even-length hex string, 1..256 bytes (matches
        //         the encoding rdram_peek returns).
        //
        // Uses the same XOR-3 byte addressing as rdram_peek so the
        // bytes round-trip identically.
        uint32_t addr = get_uint(line, "addr", 0);
        std::string hex = get_str(line, "hex");
        if (hex.size() < 2 || (hex.size() & 1)) {
            return R"({"ok":false,"error":"hex must be even-length, >=2 chars"})";
        }
        size_t n = hex.size() / 2;
        if (n > 256) {
            return R"({"ok":false,"error":"hex >256 bytes"})";
        }
        uint32_t paddr = addr & 0x1FFFFFFFu;
        constexpr uint32_t kRdramSize = 8u * 1024u * 1024u;
        if (paddr + (uint32_t)n > kRdramSize) {
            return R"({"ok":false,"error":"oob"})";
        }
        uint8_t* rdram = recomp_runtime_get_rdram();
        if (rdram == nullptr) {
            return R"({"ok":false,"error":"rdram not yet captured"})";
        }
        for (size_t i = 0; i < n; i++) {
            char hb[3] = { hex[i*2], hex[i*2+1], 0 };
            uint8_t b = (uint8_t)std::strtoul(hb, nullptr, 16);
            rdram[(paddr + (uint32_t)i) ^ 3] = b;
        }
        return R"({"ok":true,"addr":)" + std::to_string(addr) +
               R"(,"n":)" + std::to_string(n) + R"(})";
    }
    // NOTE: rdram_scan_u32 added 2026-05-08 — needs rebuild before use.
    if (cmd == "rdram_scan_u32") {
        // Host-side scan of all rdram for occurrences of a specific 4-byte
        // big-endian-from-N64-perspective value. Args: {"value": <u32>,
        // "limit": <max_hits>}. Returns list of vaddrs where the value is
        // stored. Useful for finding stale-pointer holders: if a DL target
        // is wrong at runtime, find every rdram word that equals that
        // target value — the holder is one of them.
        uint32_t want = get_uint(line, "value", 0);
        int limit = get_int(line, "limit", 64);
        if (limit < 1) limit = 1;
        if (limit > 1024) limit = 1024;

        uint8_t* rdram = recomp_runtime_get_rdram();
        if (rdram == nullptr) {
            return R"({"ok":false,"error":"rdram not yet captured"})";
        }
        constexpr uint32_t kRdramSize = 8u * 1024u * 1024u;
        // Stride 4 — only word-aligned hits matter for pointer-storage.
        std::string out = R"({"ok":true,"value":)" + std::to_string(want) +
                          R"(,"hits":[)";
        int hits = 0;
        for (uint32_t paddr = 0; paddr + 4 <= kRdramSize && hits < limit; paddr += 4) {
            // XOR-3 byte addressing: read 4 bytes BE from N64's perspective.
            uint8_t b3 = rdram[(paddr + 0) ^ 3];
            uint8_t b2 = rdram[(paddr + 1) ^ 3];
            uint8_t b1 = rdram[(paddr + 2) ^ 3];
            uint8_t b0 = rdram[(paddr + 3) ^ 3];
            uint32_t v = ((uint32_t)b3 << 24) | ((uint32_t)b2 << 16) |
                         ((uint32_t)b1 << 8) | (uint32_t)b0;
            if (v == want) {
                if (hits) out += ",";
                out += std::to_string(0x80000000u + paddr);
                hits++;
            }
        }
        out += R"(],"truncated":)";
        out += (hits >= limit) ? "true" : "false";
        out += "}";
        return out;
    }
    // ---------------- Ares oracle bridge -----------------------------------
    // Dev-only divergence-oracle commands, compiled in only when the build is
    // configured with the real Ares bridge (-DWITH_ARES_BRIDGE=ON, which
    // defines PSR_WITH_ARES). The shipped release builds without Ares (so the
    // executable can statically link the CRT and stay self-contained), so these
    // commands are excluded and the runner links none of the Ares trace
    // symbols. They expose the oracle to external diff harnesses
    // (tools/diff_aspmain.py); calls block the debug-server thread (not the
    // runner's main thread) until the oracle returns.
#ifdef PSR_WITH_ARES
    if (cmd == "ares_status") {
        // Read every counter on the dedicated Ares thread so the trace
        // ring's thread_local state is consistent with the writer.
        return ares_worker::dispatch([]() -> std::string {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "{\"ok\":true,\"is_real\":%d,\"version\":\"%s\","
                "\"trace_count\":%llu,\"trace_enabled\":%d,\"trace_boot_count\":%u}",
                ares_bridge_is_real(),
                ares_bridge_version() ? ares_bridge_version() : "?",
                (unsigned long long)ares_rsp_trace_count(),
                ares_rsp_trace_is_enabled(),
                (unsigned)ares_rsp_trace_boot_count());
            return std::string(buf);
        });
    }
    if (cmd == "ares_init_oracle") {
        // Args: {"rom_path": "..."}. ROM path is mandatory — caller picks
        // the same baserom.z64 the runner is using.
        std::string rom = get_str(line, "rom_path");
        if (rom.empty()) return R"({"ok":false,"error":"missing rom_path"})";
        return ares_worker::dispatch([&rom]() -> std::string {
            ares_status_t s = ares_init(rom.c_str());
            if (s != ARES_BRIDGE_OK && s != ARES_BRIDGE_ALREADY_INITIALIZED) {
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                    "{\"ok\":false,\"error\":\"ares_init failed\",\"status\":%d}", (int)s);
                return std::string(buf);
            }
            ares_status_t r = ares_reset();
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "{\"ok\":%s,\"init_status\":%d,\"reset_status\":%d}",
                (r == ARES_BRIDGE_OK ? "true" : "false"), (int)s, (int)r);
            return std::string(buf);
        });
    }
    if (cmd == "ares_reset_oracle") {
        return ares_worker::dispatch([]() -> std::string {
            ares_status_t r = ares_reset();
            char buf[96];
            std::snprintf(buf, sizeof(buf),
                "{\"ok\":%s,\"status\":%d}",
                (r == ARES_BRIDGE_OK ? "true" : "false"), (int)r);
            return std::string(buf);
        });
    }
    if (cmd == "ares_step_frame") {
        // Args: {"n": <count>}. Default 1, max 600 (~10s of emulated
        // time at 60Hz — keeps the server thread responsive).
        int n = get_int(line, "n", 1);
        if (n < 1) n = 1;
        if (n > 600) n = 600;
        return ares_worker::dispatch([n]() -> std::string {
            int done = 0;
            ares_status_t last = ARES_BRIDGE_OK;
            for (int i = 0; i < n; i++) {
                last = ares_step_frame();
                if (last != ARES_BRIDGE_OK) break;
                done++;
            }
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "{\"ok\":%s,\"frames\":%d,\"status\":%d,\"trace_count\":%llu}",
                (last == ARES_BRIDGE_OK ? "true" : "false"),
                done, (int)last,
                (unsigned long long)ares_rsp_trace_count());
            return std::string(buf);
        });
    }
    // CPU-side accessors (ares_read_pc, ares_read_cpu_register,
    // ares_read_memory, ares_set_controller, ares_step_instruction) are
    // intentionally NOT exposed yet — those bridge entry points are
    // unimplemented in ares_core_glue.cpp (Phase 3+) and abort the
    // process. The aspMain divergence diff is RSP-side, so the trace
    // ring below has all the state we need; CPU accessors come online
    // when Phase 3 lands.
    if (cmd == "ares_rsp_trace_recent" || cmd == "ares_rsp_trace_boot" ||
        cmd == "ares_rsp_trace_at_pc") {
      return ares_worker::dispatch([&line, &cmd]() -> std::string {
        // Three RSP trace queries share the same event-rendering tail.
        //   ares_rsp_trace_recent {n}        → last n events from sliding ring
        //   ares_rsp_trace_boot {start, n}   → slice [start, start+n) from boot snapshot
        //   ares_rsp_trace_at_pc {pc, n}     → up to n most-recent events whose pc == arg
        int n = get_int(line, "n", 32);
        if (n < 1) n = 1;
        if (n > 1024) n = 1024;
        std::vector<ares_rsp_trace_event_t> events; events.reserve((size_t)n);

        if (cmd == "ares_rsp_trace_recent") {
            uint64_t total = ares_rsp_trace_count();
            if ((uint64_t)n > total) n = (int)total;
            for (int i = 0; i < n; i++) {
                uint64_t idx = total - (uint64_t)n + (uint64_t)i;
                ares_rsp_trace_event_t ev{};
                if (ares_rsp_trace_get(idx, &ev)) events.push_back(ev);
            }
        } else if (cmd == "ares_rsp_trace_boot") {
            int start = get_int(line, "start", 0);
            if (start < 0) start = 0;
            uint32_t total = ares_rsp_trace_boot_count();
            for (int i = 0; i < n; i++) {
                uint32_t pos = (uint32_t)start + (uint32_t)i;
                if (pos >= total) break;
                ares_rsp_trace_event_t ev{};
                if (ares_rsp_trace_boot_get(pos, &ev)) events.push_back(ev);
            }
        } else { // ares_rsp_trace_at_pc
            // Scan backward from the most recent event for up to "n"
            // matches OR until 65536 events scanned (bound on cost).
            uint32_t want_pc = get_uint(line, "pc", 0) & 0xFFFu;
            uint64_t total = ares_rsp_trace_count();
            int scanned = 0;
            const int kScanCap = 65536;
            for (uint64_t i = total; i-- > 0 && scanned < kScanCap && (int)events.size() < n; ) {
                ares_rsp_trace_event_t ev{};
                if (!ares_rsp_trace_get(i, &ev)) break; // ring rolled past
                scanned++;
                if ((ev.pc & 0xFFFu) == want_pc) events.push_back(ev);
            }
            // events accumulated newest-first; reverse to chronological.
            std::reverse(events.begin(), events.end());
        }

        std::string out = std::string(R"({"ok":true,"trace_count":)") +
                          std::to_string(ares_rsp_trace_count()) +
                          R"(,"events":[)";
        for (size_t i = 0; i < events.size(); i++) {
            const auto& ev = events[i];
            char buf[1024];
            // Render gpr[] inline as a compact array. dma_* fields named
            // to match the C struct so the harness can deserialize without
            // a translation table.
            int off = std::snprintf(buf, sizeof(buf),
                "%s{\"seq\":%llu,\"pc\":%u,\"dma_mem_addr\":%u,"
                "\"dma_dram_addr\":%u,\"dma_rd_len\":%u,\"dma_wr_len\":%u,"
                "\"status\":%u,\"gpr\":[",
                (i ? "," : ""),
                (unsigned long long)ev.seq, (unsigned)ev.pc,
                (unsigned)ev.dma_mem_addr, (unsigned)ev.dma_dram_addr,
                (unsigned)ev.dma_rd_len, (unsigned)ev.dma_wr_len,
                (unsigned)ev.status);
            out.append(buf, (size_t)off);
            for (int g = 0; g < 32; g++) {
                char gbuf[16];
                int goff = std::snprintf(gbuf, sizeof(gbuf), "%s%u",
                    (g ? "," : ""), (unsigned)ev.gpr[g]);
                out.append(gbuf, (size_t)goff);
            }
            out += "]}";
        }
        out += "]}";
        return out;
      });
    }
    if (cmd == "ares_rsp_trace_set_enabled") {
        bool on = get_bool(line, "on", true);
        return ares_worker::dispatch([on]() -> std::string {
            ares_rsp_trace_set_enabled(on ? 1 : 0);
            return std::string(R"({"ok":true,"enabled":)") +
                   std::to_string(ares_rsp_trace_is_enabled()) + "}";
        });
    }
#endif // PSR_WITH_ARES

    if (cmd == "get_last_pc_trail") {
        // Returns the live pc_trail of the most-recently-launched RSP
        // ucode (typically aspMain on Stadium). Use this to monitor
        // a hang in real-time without waiting for the watchdog to trip.
        // Returns 32 PCs in chronological order (oldest first), the
        // current ring index, and the watchdog counter.
        recomp::rsp::PcTrailSnapshot snap{};
        bool ok = recomp::rsp::get_last_pc_trail(&snap);
        if (!ok || !snap.valid) {
            return R"({"ok":true,"valid":false})";
        }
        // Render entries in chronological order: ring start = (idx & 31),
        // wrapping forward 32 times reaches the newest entry just
        // before idx.
        std::string out = R"({"ok":true,"valid":true,"idx":)" +
                          std::to_string(snap.idx) +
                          R"(,"watchdog_count":)" +
                          std::to_string(snap.watchdog_count) +
                          R"(,"pc_trail":[)";
        for (int i = 0; i < 32; i++) {
            uint32_t pos = (snap.idx + (uint32_t)i) & 31u;
            char tmp[16];
            std::snprintf(tmp, sizeof(tmp), "%s%u",
                (i ? "," : ""), (unsigned)snap.entries[pos]);
            out += tmp;
        }
        out += "]}";
        return out;
    }
    if (cmd == "rt64_segments") {
        // RT64's resolved segment table — gSegments[0..15] as the
        // interpreter sees them. Updated whenever the game emits
        // gsSPSegment via G_MOVEWORD. Used to triangulate stale-
        // segment binding bugs (e.g. menu sprite/border corruption
        // in Stadium where seg 3 textures resolve to wrong RDRAM).
        // Backed by the RT64_PSR_DEBUG_HOOKS-gated snapshot in
        // rt64_rsp.cpp; if that define is 0 the symbol won't exist
        // and the link will fail — that's intentional, you opted out.
        uint32_t segs[16] = {};
        uint64_t seq = 0;
        rt64_psr_segments_copy(segs, &seq);
        std::string out = R"({"ok":true,"seq":)" + std::to_string(seq) +
                          R"(,"segments":[)";
        for (int i = 0; i < 16; i++) {
            char b[24];
            std::snprintf(b, sizeof(b), "%u", (unsigned)segs[i]);
            if (i) out += ",";
            out += b;
        }
        out += "]}";
        return out;
    }
    if (cmd == "dump_current_dl") {
        // Walk the SP task ring buffer for the last M_GFXTASK and dump
        // its bytes from rdram to a file. Always-on, query-by-window:
        // every gfx submit is in the ring; this command consumes it
        // without any "arm + capture" timing dance. Used to capture
        // the DL feeding any visible frame (e.g. a sprite-corruption
        // repro screen) for offline GBI decoding.
        std::string path = pkmnstadium::app_file("last_run_dl.bin").string();
        std::string p = get_str(line, "path");
        if (!p.empty()) path = p;
        uint32_t addr = 0, size = 0;
        int rc = psr_dump_current_dl(path.c_str(), &addr, &size);
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "{\"ok\":%s,\"rc\":%d,\"path\":\"%s\",\"addr\":%u,\"size\":%u}",
            rc == 0 ? "true" : "false", rc, path.c_str(),
            (unsigned)addr, (unsigned)size);
        return buf;
    }
    if (cmd == "tail_errlog") {
        const std::string errlog_path = pkmnstadium::app_file("last_error.log").string();
        FILE* f = fopen(errlog_path.c_str(), "rb");
        if (!f) return R"({"ok":true,"errlog":""})";
        char chunk[4096]{};
        size_t n = fread(chunk, 1, sizeof(chunk) - 1, f);
        chunk[n] = 0;
        fclose(f);
        // Naive escape — replace " and \ with safe substitutes.
        std::string out = R"({"ok":true,"errlog":")";
        for (size_t i = 0; i < n; i++) {
            char c = chunk[i];
            if (c == '"' || c == '\\') { out += '\\'; out += c; }
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if ((unsigned char)c < 0x20) ; // skip
            else out += c;
        }
        out += R"("})";
        return out;
    }
    if (cmd == "dump_threads") {
        // Walk every OS thread in this process, capture its host call
        // stack, symbolize, and write it to last_error.log. Lets us
        // diagnose deep stalls (e.g. attract demo blocked in an
        // OSMesgQueue wait) without having to instrument every libultra
        // primitive.
        const std::string errlog_path = pkmnstadium::app_file("last_error.log").string();
        FILE* f = fopen(errlog_path.c_str(), "a");
        if (!f) return R"({"ok":false,"error":"could not open log"})";
        fprintf(f, "\n=== dump_threads ===\n");

        HANDLE proc = GetCurrentProcess();
        SymInitialize(proc, NULL, TRUE);

        DWORD self_pid = GetCurrentProcessId();
        DWORD self_tid = GetCurrentThreadId();
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        int n_threads = 0;
        if (snap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te{};
            te.dwSize = sizeof(te);
            if (Thread32First(snap, &te)) {
                do {
                    if (te.th32OwnerProcessID != self_pid) continue;
                    if (te.th32ThreadID == self_tid) continue;  // skip caller
                    HANDLE th = OpenThread(
                        THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME |
                        THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
                    if (!th) continue;
                    SuspendThread(th);
                    CONTEXT ctx{};
                    ctx.ContextFlags = CONTEXT_FULL;
                    if (GetThreadContext(th, &ctx)) {
                        fprintf(f, "\n[thread %lu]\n", te.th32ThreadID);
                        STACKFRAME64 frame{};
                        frame.AddrPC.Offset    = ctx.Rip; frame.AddrPC.Mode    = AddrModeFlat;
                        frame.AddrFrame.Offset = ctx.Rbp; frame.AddrFrame.Mode = AddrModeFlat;
                        frame.AddrStack.Offset = ctx.Rsp; frame.AddrStack.Mode = AddrModeFlat;
                        char symbuf[sizeof(SYMBOL_INFO) + 256];
                        SYMBOL_INFO* sym = (SYMBOL_INFO*)symbuf;
                        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
                        sym->MaxNameLen = 255;
                        IMAGEHLP_LINE64 line{}; line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
                        for (int i = 0; i < 24; i++) {
                            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, th,
                                             &frame, &ctx, NULL,
                                             SymFunctionTableAccess64,
                                             SymGetModuleBase64, NULL)) break;
                            if (!frame.AddrPC.Offset) break;
                            DWORD64 disp64 = 0; DWORD disp32 = 0;
                            const char* name = "?"; const char* file = "?"; DWORD lineno = 0;
                            if (SymFromAddr(proc, frame.AddrPC.Offset, &disp64, sym)) name = sym->Name;
                            if (SymGetLineFromAddr64(proc, frame.AddrPC.Offset, &disp32, &line)) {
                                file = line.FileName; lineno = line.LineNumber;
                            }
                            fprintf(f, "  #%02d 0x%016llX %s (%s:%lu)\n",
                                i, (unsigned long long)frame.AddrPC.Offset,
                                name, file, lineno);
                        }
                        n_threads++;
                    }
                    ResumeThread(th);
                    CloseHandle(th);
                } while (Thread32Next(snap, &te));
            }
            CloseHandle(snap);
        }
        fprintf(f, "\n=== dump_threads end (%d threads) ===\n", n_threads);
        fclose(f);
        char buf[64];
        std::snprintf(buf, sizeof(buf), R"({"ok":true,"threads":%d})", n_threads);
        return buf;
    }
    if (cmd == "quit") {
        ExitProcess(0);  // hard exit — debug-driven shutdown
        return R"({"ok":true})";
    }
    return R"({"ok":false,"error":"unknown command"})";
}

static void server_loop(int port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[dbg] WSAStartup failed\n");
        return;
    }
    s_listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_listen_sock == INVALID_SOCKET) {
        fprintf(stderr, "[dbg] socket() failed\n");
        return;
    }

    BOOL reuse = TRUE;
    setsockopt(s_listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
    if (bind(s_listen_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "[dbg] bind() failed err=%d\n", WSAGetLastError());
        return;
    }
    if (listen(s_listen_sock, 1) == SOCKET_ERROR) {
        fprintf(stderr, "[dbg] listen() failed\n");
        return;
    }
    fprintf(stderr, "[dbg] listening on 127.0.0.1:%d\n", port);
    fflush(stderr);

    while (s_running.load()) {
        sockaddr_in caddr{};
        int caddr_len = sizeof(caddr);
        SOCKET client = accept(s_listen_sock, (sockaddr*)&caddr, &caddr_len);
        if (client == INVALID_SOCKET) {
            if (!s_running.load()) break;
            continue;
        }
        // Read line-by-line until disconnect.
        std::string buf;
        buf.reserve(1024);
        char chunk[1024];
        while (s_running.load()) {
            int n = recv(client, chunk, sizeof(chunk), 0);
            if (n <= 0) break;
            buf.append(chunk, chunk + n);
            // Process complete lines.
            size_t nl;
            while ((nl = buf.find('\n')) != std::string::npos) {
                std::string line = buf.substr(0, nl);
                buf.erase(0, nl + 1);
                std::string resp = handle_command(line);
                resp += "\n";
                send(client, resp.c_str(), (int)resp.size(), 0);
            }
        }
        closesocket(client);
    }

    closesocket(s_listen_sock);
    WSACleanup();
}

void start(int port) {
    if (s_running.load()) return;
    s_running.store(true);
    s_thread = std::thread(server_loop, port);
    s_thread.detach();
}

void shutdown() {
    s_running.store(false);
    if (s_listen_sock != INVALID_SOCKET) {
        closesocket(s_listen_sock);
    }
}

} // namespace dbg
} // namespace pkmnstadium
