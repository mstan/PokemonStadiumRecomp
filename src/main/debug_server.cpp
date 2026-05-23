/*
 * debug_server.cpp — Minimal TCP debug server for PokemonStadiumRecomp.
 *
 * Modeled after nesrecomp's runner/src/debug_server.c (per
 * F:\Projects\recomp-template\NES\TCP.md). Default port 4370,
 * single-client, JSON line protocol.
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
 * Per project principles (F:\Projects\recomp-template\NES\TCP.md):
 * any state we want visible from outside grows TCP commands here.
 * Don't add side-channel logging.
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
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "debug_server.h"
#include "librecomp/ultra_trace.hpp"
#include "librecomp/rsp.hpp"
#include "ares_bridge.h"
#include "ares_worker.h"

#pragma comment(lib, "ws2_32.lib")

// Function-trace ring queries (definitions in extras.c).
extern "C" {
    uint64_t pkmnstadium_trace_write_idx(void);
    const char* pkmnstadium_trace_at(uint64_t idx);
    uint32_t pkmnstadium_trace_capacity(void);
    uint64_t pkmnstadium_interesting_fn_count(int idx);
    const char* pkmnstadium_interesting_fn_name(int idx);
    int pkmnstadium_interesting_fn_total(void);
    int pkmnstadium_resolver_log_total(void);
    void pkmnstadium_resolver_log_get(int idx, uint32_t* arr, uint32_t* base, uint32_t* count);
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

// Default 0 (muted) — see header comment for rationale.
std::atomic<float>    g_audio_volume{0.0f};

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
extern "C" uint64_t ultramodern_submit_gfx_count(void);
extern "C" uint64_t ultramodern_submit_audio_count(void);
extern "C" uint64_t ultramodern_submit_other_count(void);
extern "C" uint64_t ultramodern_sp_complete_count(void);
extern "C" uint64_t ultramodern_dp_complete_count(void);
extern "C" void recomp_sp_task_recent_copy(
    void* out_void, size_t cap, size_t* n_written, uint64_t* next_seq_out);
extern "C" size_t recomp_sp_task_event_size(void);
extern "C" void psr_post_mortem_dump(const char* reason, void* fault_info);
extern "C" int psr_dump_current_dl(const char* path,
                                   uint32_t* out_addr,
                                   uint32_t* out_size);
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
        char buf[1024];
        std::snprintf(buf, sizeof(buf),
            "{\"ok\":true,\"frame\":%llu,\"vi\":%llu,\"fast_forward\":%s,\"input_override\":%s,\"buttons\":%u,\"sx\":%d,\"sy\":%d,"
            "\"send_dl\":%llu,\"send_dl_gfx\":%llu,\"send_dl_audio\":%llu,\"send_dl_other\":%llu,\"update_screen\":%llu,"
            "\"external_requeues\":%llu,"
            "\"submit_gfx\":%llu,\"submit_audio\":%llu,\"submit_other\":%llu,"
            "\"sp_complete\":%llu,\"dp_complete\":%llu}",
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
            (unsigned long long)ultramodern_dp_complete_count()
        );
        return buf;
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
    if (cmd == "set_volume") {
        // Args: {"value": 0.0..1.0}. Out-of-range values are clamped.
        // Float JSON parsing isn't in our crude get_int/get_str family,
        // so use strtod against the raw substring.
        std::string needle = "\"value\"";
        size_t k = line.find(needle);
        float v = 0.0f;
        if (k != std::string::npos) {
            size_t colon = line.find(':', k);
            if (colon != std::string::npos) {
                v = (float)std::strtod(line.c_str() + colon + 1, nullptr);
            }
        }
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        g_audio_volume.store(v);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "{\"ok\":true,\"value\":%.3f}", v);
        return buf;
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
        struct MesgEvent {
            uint64_t seq;
            uint64_t ms;
            uint32_t mq;
            uint32_t msg;
            uint16_t valid_before;
            uint16_t valid_after;
            uint8_t  op;
            uint8_t  block;
            uint8_t  game_thread;
            uint8_t  pad;
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
        const char* op_names[9] = {
            "?", "send_game", "send_external", "recv_enter",
            "recv_block", "recv_return_ok", "ext_deq_ok", "ext_deq_full",
            "do_send_block"
        };
        std::string out = R"({"ok":true,"write_idx":)" + std::to_string(widx)
                        + R"(,"events":[)";
        for (size_t i = 0; i < got; i++) {
            const auto& e = buf[i];
            const char* opn = (e.op < 9) ? op_names[e.op] : "?";
            char b[256];
            std::snprintf(b, sizeof(b),
                "%s{\"seq\":%llu,\"ms\":%llu,\"op\":\"%s\",\"mq\":%u,"
                "\"msg\":%u,\"vb\":%u,\"va\":%u,\"block\":%u,\"gt\":%u}",
                (i ? "," : ""),
                (unsigned long long)e.seq, (unsigned long long)e.ms,
                opn, e.mq, e.msg,
                (unsigned)e.valid_before, (unsigned)e.valid_after,
                (unsigned)e.block, (unsigned)e.game_thread);
            out += b;
        }
        out += "]}";
        return out;
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
            uint32_t task_type;
            uint32_t task_flags;
            uint32_t ucode;
            uint32_t data_ptr;
            uint32_t data_size;
            uint32_t output_buff;
            uint32_t output_buff_size;
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
        for (size_t i = 0; i < got; i++) {
            const auto& e = buf[i];
            char b[384];
            std::snprintf(b, sizeof(b),
                "%s{\"seq\":%llu,\"ms\":%llu,\"frame\":%llu,\"send_dl\":%llu,"
                "\"type\":\"%s\",\"task_ptr\":%u,\"mips_ra\":%u,"
                "\"ucode\":%u,\"data_ptr\":%u,\"data_size\":%u,"
                "\"output_buff\":%u,\"output_buff_size\":%u,\"flags\":%u}",
                (i ? "," : ""),
                (unsigned long long)e.seq, (unsigned long long)e.ms,
                (unsigned long long)e.frame, (unsigned long long)e.send_dl,
                type_name(e.task_type), e.task_ptr, e.mips_ra,
                e.ucode, e.data_ptr, e.data_size,
                e.output_buff, e.output_buff_size, e.task_flags);
            out += b;
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
    // The ares-bridge library is linked into the runner. These commands
    // expose the oracle to external diff harnesses (tools/diff_aspmain.py).
    // The bridge runs in this process; calls block the debug-server thread
    // until the oracle returns. The runner's main thread is unaffected.
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
        std::string path = "F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_run_dl.bin";
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
        FILE* f = fopen("F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_error.log", "rb");
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
        FILE* f = fopen("F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_error.log", "a");
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
