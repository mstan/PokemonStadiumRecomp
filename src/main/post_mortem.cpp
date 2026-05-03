/*
 * post_mortem.cpp — unified exit/crash diagnostic dump.
 *
 * Writes a single JSON file (build/last_run_report.json) with the
 * always-on rings + hardware-state snapshot + per-thread host stacks.
 * Triggered from:
 *   - SEH unhandled-exception filter (psr_crash_filter in main.cpp)
 *   - std::atexit on clean shutdown
 *   - TCP "post_mortem_dump" command (on-demand, runner stays alive)
 *
 * Key design principles:
 *   - One file per run, overwritten each dump (last write wins).
 *     Don't tail-append — the file is meant to be parsed.
 *   - All payload comes from already-existing ring buffers; this
 *     module is a serializer, not a recorder.
 *   - Thread host-stack walks use Windows DbgHelp + Toolhelp32 to
 *     enumerate every thread in the process. Suspend each, walk,
 *     resume. Cheap; runs in tens of milliseconds.
 *   - JSON is hand-rolled (no nlohmann to keep the SEH path
 *     allocation-light; the SEH handler runs in unstable state).
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#pragma comment(lib, "dbghelp.lib")

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

#include "post_mortem.hpp"

// ── External hooks into existing rings/counters ──────────────────────

extern "C" const char*  pkmnstadium_trace_at(uint64_t idx);
extern "C" uint64_t     pkmnstadium_trace_write_idx(void);
extern "C" uint32_t     pkmnstadium_trace_capacity(void);

extern "C" uint64_t     recomp_ultra_trace_write_idx(void);
extern "C" uint32_t     recomp_ultra_trace_capacity(void);
struct recomp_ultra_trace_event {
    char name[64];
    uint64_t ms;
    uint32_t pc, a0, a1, a2, a3;
    uint8_t  valid, pad[3];
};
extern "C" int recomp_ultra_trace_get(uint64_t off, recomp_ultra_trace_event* out);

extern "C" uint64_t ultramodern_external_requeues(void);
extern "C" void     ultramodern_mesg_recent_copy(
    void* out_void, size_t cap, size_t* n_written, uint64_t* next_seq_out);
extern "C" size_t   ultramodern_mesg_event_size(void);
extern "C" uint64_t ultramodern_submit_gfx_count(void);
extern "C" uint64_t ultramodern_submit_audio_count(void);
extern "C" uint64_t ultramodern_submit_other_count(void);
extern "C" uint64_t ultramodern_sp_complete_count(void);
extern "C" uint64_t ultramodern_dp_complete_count(void);

// RT64 interpreter probe — surfaces what the gfx thread is currently
// executing. When dp_complete < submit_gfx these tell us how far into
// task #N RT64 got and what its recent visited PCs are.
extern "C" uint64_t rt64_interp_seq(void);
extern "C" uint64_t rt64_interp_step(void);
extern "C" uint64_t rt64_interp_task_index(void);
extern "C" uint64_t rt64_interp_current_pc(void);
extern "C" uint64_t rt64_interp_dl_start(void);
extern "C" void rt64_interp_recent_copy(uint64_t* out, size_t cap,
                                        size_t* n_written, uint64_t* seq_out);
extern "C" size_t rt64_interp_cf_event_size(void);
extern "C" void   rt64_interp_cf_recent_copy(void* out_void, size_t cap,
                                             size_t* n_written, uint64_t* seq_out);
extern "C" void     recomp_sp_task_recent_copy(
    void* out_void, size_t cap, size_t* n_written, uint64_t* next_seq_out);
extern "C" size_t   recomp_sp_task_event_size(void);

// recomp runtime — provides RDRAM base for safe reads
extern "C" uint8_t* recomp_runtime_get_rdram(void);

// status counters in debug_server.cpp
namespace pkmnstadium { namespace dbg {
    extern std::atomic<uint64_t> g_frame_count;
    extern std::atomic<uint64_t> g_vi_ticks;
    extern std::atomic<uint64_t> g_send_dl_count;
    extern std::atomic<uint64_t> g_send_dl_gfx_count;
    extern std::atomic<uint64_t> g_send_dl_audio_count;
    extern std::atomic<uint64_t> g_send_dl_other_count;
    extern std::atomic<uint64_t> g_update_screen_count;
}}

// Dedicated output path — single file, overwritten per dump.
static const char* kReportPath =
    "F:/Projects/PokemonStadiumRecomp/build/last_run_report.json";

// Mutex so on-demand TCP dump and SEH/atexit dump can't race.
static std::mutex g_dump_mutex;

// ── Helpers ──────────────────────────────────────────────────────────

namespace {

// Mirror of the in-librecomp/sp.cpp Event layout — keep in lockstep.
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

const char* sp_task_type_name(uint32_t t) {
    // From ultra64.h: M_GFXTASK=1, M_AUDTASK=2, M_VIDTASK=3, M_NJPEGTASK=4, M_HVQMTASK=6 ...
    switch (t) {
        case 1: return "M_GFXTASK";
        case 2: return "M_AUDTASK";
        case 3: return "M_VIDTASK";
        case 4: return "M_NJPEGTASK";
        case 6: return "M_HVQMTASK";
        default: return "?";
    }
}

// Mirror of the in-ultramodern Event layout — keep in lockstep.
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

const char* mesg_op_name(uint8_t op) {
    switch (op) {
        case 1: return "send_game";
        case 2: return "send_external";
        case 3: return "recv_enter";
        case 4: return "recv_block";
        case 5: return "recv_return_ok";
        case 6: return "ext_deq_ok";
        case 7: return "ext_deq_full";
        case 8: return "do_send_block";
        default: return "?";
    }
}

// Read u32 BE from rdram at vaddr; returns 0 if vaddr is out of range.
uint32_t read_be_u32(uint8_t* rdram, uint32_t vaddr) {
    if (rdram == nullptr) return 0;
    if (vaddr < 0x80000000u || vaddr >= 0x80800000u) return 0;
    uint32_t paddr = vaddr & 0x1FFFFFFFu;
    if (paddr + 4 > 8u * 1024u * 1024u) return 0;
    return  (uint32_t(rdram[(paddr + 0) ^ 3]) << 24)
          | (uint32_t(rdram[(paddr + 1) ^ 3]) << 16)
          | (uint32_t(rdram[(paddr + 2) ^ 3]) <<  8)
          | (uint32_t(rdram[(paddr + 3) ^ 3]));
}

// Walk one thread's host stack, write up to `max_frames` frames as
// JSON array entries to `f`. Caller is responsible for the
// surrounding "[ ... ]" — this writes elements separated by commas.
void dump_thread_stack_json(FILE* f, HANDLE proc, HANDLE thr,
                            CONTEXT* ctx, int max_frames)
{
    STACKFRAME64 frame{};
    DWORD machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset    = ctx->Rip; frame.AddrPC.Mode    = AddrModeFlat;
    frame.AddrFrame.Offset = ctx->Rbp; frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx->Rsp; frame.AddrStack.Mode = AddrModeFlat;
    char symbuf[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO* sym = (SYMBOL_INFO*)symbuf;
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;
    IMAGEHLP_LINE64 line{}; line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    bool first = true;
    for (int i = 0; i < max_frames; i++) {
        if (!StackWalk64(machine, proc, thr, &frame, ctx, NULL,
                         SymFunctionTableAccess64, SymGetModuleBase64, NULL))
            break;
        if (!frame.AddrPC.Offset) break;
        DWORD64 disp64 = 0; DWORD disp32 = 0;
        const char* name = "?"; const char* file = "?"; DWORD lineno = 0;
        if (SymFromAddr(proc, frame.AddrPC.Offset, &disp64, sym)) name = sym->Name;
        if (SymGetLineFromAddr64(proc, frame.AddrPC.Offset, &disp32, &line)) {
            file = line.FileName; lineno = line.LineNumber;
        }
        // Escape backslashes in file path for JSON.
        std::string esc_file;
        for (const char* p = file; *p; p++) {
            if (*p == '\\') esc_file += "\\\\";
            else if (*p == '"') esc_file += "\\\"";
            else esc_file += *p;
        }
        if (!first) std::fputs(",", f);
        first = false;
        std::fprintf(f,
            "{\"i\":%d,\"pc\":%llu,\"name\":\"%s\",\"file\":\"%s\",\"line\":%lu}",
            i, (unsigned long long)frame.AddrPC.Offset,
            name, esc_file.c_str(), lineno);
    }
}

void dump_all_threads_json(FILE* f, EXCEPTION_POINTERS* fault_info) {
    HANDLE proc = GetCurrentProcess();
    SymInitialize(proc, NULL, TRUE);

    DWORD pid = GetCurrentProcessId();
    DWORD self_tid = GetCurrentThreadId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        std::fprintf(f, "  \"threads\": [],\n  \"threads_error\": \"snapshot_failed\",\n");
        return;
    }

    THREADENTRY32 te{}; te.dwSize = sizeof(te);
    bool first_thread = true;
    std::fprintf(f, "  \"threads\": [\n");
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != pid) continue;
            const DWORD tid = te.th32ThreadID;

            // Walk this thread. For the calling thread, use the
            // crash-supplied context if present (matches what the SEH
            // handler used to dump), or capture the current context.
            // For other threads, suspend them, capture, walk, resume.
            CONTEXT thr_ctx{}; thr_ctx.ContextFlags = CONTEXT_FULL;
            HANDLE thr = NULL;
            bool suspended = false;

            if (tid == self_tid) {
                if (fault_info) {
                    thr_ctx = *fault_info->ContextRecord;
                } else {
                    // The current thread's context — use the captured
                    // approach (RtlCaptureContext) which is safe to call
                    // anywhere.
                    RtlCaptureContext(&thr_ctx);
                }
                thr = GetCurrentThread();
            } else {
                thr = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME |
                                 THREAD_QUERY_INFORMATION, FALSE, tid);
                if (!thr) continue;
                if (SuspendThread(thr) == (DWORD)-1) {
                    CloseHandle(thr); continue;
                }
                suspended = true;
                if (!GetThreadContext(thr, &thr_ctx)) {
                    ResumeThread(thr); CloseHandle(thr); continue;
                }
            }

            // Try to derive a thread name (Windows 10+) for filtering.
            wchar_t* name_w = nullptr;
            std::string thr_name;
            if (SUCCEEDED(GetThreadDescription(thr, &name_w)) && name_w) {
                int n = WideCharToMultiByte(CP_UTF8, 0, name_w, -1, NULL, 0, NULL, NULL);
                if (n > 0) {
                    std::string s(n - 1, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, name_w, -1, &s[0], n, NULL, NULL);
                    thr_name = std::move(s);
                }
                LocalFree(name_w);
            }

            if (!first_thread) std::fputs(",\n", f);
            first_thread = false;
            std::fprintf(f,
                "    {\"tid\":%lu,\"name\":\"%s\","
                "\"rip\":%llu,\"rsp\":%llu,\"rbp\":%llu,"
                "\"frames\":[",
                tid, thr_name.c_str(),
                (unsigned long long)thr_ctx.Rip,
                (unsigned long long)thr_ctx.Rsp,
                (unsigned long long)thr_ctx.Rbp);
            dump_thread_stack_json(f, proc, thr, &thr_ctx, 24);
            std::fputs("]}", f);

            if (suspended) {
                ResumeThread(thr);
                CloseHandle(thr);
            }
        } while (Thread32Next(snap, &te));
    }
    std::fprintf(f, "\n  ],\n");
    CloseHandle(snap);
}

void dump_status_json(FILE* f) {
    using namespace pkmnstadium::dbg;
    std::fprintf(f,
        "  \"status\": {\n"
        "    \"frame\": %llu,\n"
        "    \"vi\": %llu,\n"
        "    \"send_dl\": %llu,\n"
        "    \"send_dl_gfx\": %llu,\n"
        "    \"send_dl_audio\": %llu,\n"
        "    \"send_dl_other\": %llu,\n"
        "    \"update_screen\": %llu,\n"
        "    \"submit_gfx\": %llu,\n"
        "    \"submit_audio\": %llu,\n"
        "    \"submit_other\": %llu,\n"
        "    \"sp_complete\": %llu,\n"
        "    \"dp_complete\": %llu,\n"
        "    \"external_requeues\": %llu\n"
        "  },\n",
        (unsigned long long)g_frame_count.load(),
        (unsigned long long)g_vi_ticks.load(),
        (unsigned long long)g_send_dl_count.load(),
        (unsigned long long)g_send_dl_gfx_count.load(),
        (unsigned long long)g_send_dl_audio_count.load(),
        (unsigned long long)g_send_dl_other_count.load(),
        (unsigned long long)g_update_screen_count.load(),
        (unsigned long long)ultramodern_submit_gfx_count(),
        (unsigned long long)ultramodern_submit_audio_count(),
        (unsigned long long)ultramodern_submit_other_count(),
        (unsigned long long)ultramodern_sp_complete_count(),
        (unsigned long long)ultramodern_dp_complete_count(),
        (unsigned long long)ultramodern_external_requeues());
}

void dump_trace_json(FILE* f, int n_max) {
    uint64_t widx = pkmnstadium_trace_write_idx();
    uint32_t cap  = pkmnstadium_trace_capacity();
    int n = n_max;
    if ((uint64_t)n > widx) n = (int)widx;
    if ((uint32_t)n > cap)  n = (int)cap;
    std::fprintf(f, "  \"trace_recent\": {\"write_idx\": %llu, \"entries\": [",
                 (unsigned long long)widx);
    for (int i = 0; i < n; i++) {
        uint64_t off = widx - n + i;
        const char* name = pkmnstadium_trace_at(off);
        std::fprintf(f, "%s\"%s\"", (i ? "," : ""), name ? name : "?");
    }
    std::fprintf(f, "]},\n");
}

void dump_libultra_json(FILE* f, int n_max) {
    uint64_t widx = recomp_ultra_trace_write_idx();
    uint32_t cap  = recomp_ultra_trace_capacity();
    int n = n_max;
    if ((uint64_t)n > widx) n = (int)widx;
    if ((uint32_t)n > cap)  n = (int)cap;
    std::fprintf(f, "  \"libultra_recent\": {\"write_idx\": %llu, \"events\": [",
                 (unsigned long long)widx);
    for (int i = 0; i < n; i++) {
        uint64_t off = widx - n + i;
        recomp_ultra_trace_event ev{};
        int ok = recomp_ultra_trace_get(off, &ev);
        std::fprintf(f,
            "%s{\"i\":%llu,\"valid\":%s,\"name\":\"%s\","
            "\"pc\":%u,\"a0\":%u,\"a1\":%u,\"a2\":%u,\"a3\":%u,\"ms\":%llu}",
            (i ? "," : ""),
            (unsigned long long)off, ok ? "true" : "false", ev.name,
            (unsigned)ev.pc, (unsigned)ev.a0, (unsigned)ev.a1,
            (unsigned)ev.a2, (unsigned)ev.a3,
            (unsigned long long)ev.ms);
    }
    std::fprintf(f, "]},\n");
}

// Capture the bytes of the LAST M_GFXTASK that was submitted before
// freeze, written to build/last_run_freeze_dl.bin. Used to diagnose
// hangs inside RT64::Interpreter::processDisplayLists where send_dl
// started but dp_complete never fired (submit_gfx > dp_complete).
// Offline GBI decoders read the bin to find the offending command.
void dump_last_gfx_dl_to_file() {
    if (recomp_sp_task_event_size() != sizeof(SpTaskEvent)) return;
    // 4096 matches the ring cap in librecomp/sp.cpp. The post-freeze
    // audio flood would push gfx events out of a smaller window.
    std::vector<SpTaskEvent> buf(4096);
    size_t got = 0;
    uint64_t widx = 0;
    recomp_sp_task_recent_copy(buf.data(), buf.size(), &got, &widx);
    const SpTaskEvent* last_gfx = nullptr;
    for (size_t i = got; i > 0; --i) {
        const auto& e = buf[i - 1];
        if (e.task_type == 1 /* M_GFXTASK */) { last_gfx = &e; break; }
    }
    if (!last_gfx) return;
    uint8_t* rdram = recomp_runtime_get_rdram();
    if (!rdram) return;
    uint32_t addr = last_gfx->data_ptr;
    uint32_t size = last_gfx->data_size;
    if (size == 0 || size > 0x100000) return;
    if (addr < 0x80000000u) return;
    uint32_t off = addr - 0x80000000u;
    constexpr uint32_t RDRAM_END = 0x800000u;
    if (off >= RDRAM_END || off + size > RDRAM_END) return;
    FILE* bf = std::fopen("build/last_run_freeze_dl.bin", "wb");
    if (!bf) return;
    // Byte access into rdram uses XOR-by-3 endian compensation (host
    // is LE, rdram bytes are stored as if BE word-swapped). See the
    // rdram_peek path in debug_server.cpp for the same pattern.
    for (uint32_t i = 0; i < size; i++) {
        uint8_t b = rdram[(off + i) ^ 3];
        std::fwrite(&b, 1, 1, bf);
    }
    std::fclose(bf);
}

// Mirror of the in-rt64 CfEvent layout — keep in lockstep.
struct InterpCfEvent {
    uint64_t step;
    uint64_t pc;
    uint32_t w0;
    uint32_t w1;
    uint64_t target_or_pop;
    uint32_t stack_depth_after;
    uint32_t pad;
};

void dump_interp_cf_json(FILE* f) {
    if (rt64_interp_cf_event_size() != sizeof(InterpCfEvent)) {
        std::fprintf(f, "  \"interp_cf\": {\"error\": \"size_mismatch\"},\n");
        return;
    }
    constexpr size_t N = 16384;
    std::vector<InterpCfEvent> buf(N);
    size_t got = 0;
    uint64_t seq = 0;
    rt64_interp_cf_recent_copy(buf.data(), buf.size(), &got, &seq);
    std::fprintf(f,
        "  \"interp_cf\": {\"seq\": %llu, \"events\": [",
        (unsigned long long)seq);
    for (size_t i = 0; i < got; i++) {
        const auto& e = buf[i];
        const uint8_t op = (e.w0 >> 24) & 0xFF;
        const char* name = (op == 0xDE) ? "G_DL"
                       : (op == 0xDF) ? "G_ENDDL"
                       : "?";
        std::fprintf(f,
            "%s{\"step\":%llu,\"pc\":%llu,\"w0\":%u,\"w1\":%u,"
            "\"target_or_pop\":%llu,\"op\":\"%s\",\"depth\":%u}",
            (i ? "," : ""),
            (unsigned long long)e.step,
            (unsigned long long)e.pc,
            e.w0, e.w1,
            (unsigned long long)e.target_or_pop,
            name, e.stack_depth_after);
    }
    std::fprintf(f, "]},\n");
}

// Snapshot of RT64::Interpreter::processDisplayLists state.
// Critical for diagnosing infinite loops inside the renderer:
//   step  = number of inner-loop iterations since current task started
//   current_pc = host pointer of the DisplayList being processed *right now*
//   recent[] = last N visited host pointers (sliding window)
// If step is very high (millions), RT64 is in a true loop, and the
// recent[] window contains the loop body.
void dump_interp_probe_json(FILE* f) {
    constexpr size_t N_PCS = 1024;
    std::vector<uint64_t> buf(N_PCS);
    size_t got = 0;
    uint64_t seq = 0;
    rt64_interp_recent_copy(buf.data(), buf.size(), &got, &seq);
    uint64_t rdram_host_base = (uint64_t)recomp_runtime_get_rdram();
    std::fprintf(f,
        "  \"interp_probe\": {\n"
        "    \"seq\": %llu,\n"
        "    \"step\": %llu,\n"
        "    \"task_index\": %llu,\n"
        "    \"current_pc\": %llu,\n"
        "    \"dl_start\": %llu,\n"
        "    \"rdram_host_base\": %llu,\n"
        "    \"recent_pcs\": [",
        (unsigned long long)seq,
        (unsigned long long)rt64_interp_step(),
        (unsigned long long)rt64_interp_task_index(),
        (unsigned long long)rt64_interp_current_pc(),
        (unsigned long long)rt64_interp_dl_start(),
        (unsigned long long)rdram_host_base);
    for (size_t i = 0; i < got; i++) {
        std::fprintf(f, "%s%llu", (i ? "," : ""), (unsigned long long)buf[i]);
    }
    std::fprintf(f, "]\n  },\n");
}

// Dump the full 8MB RDRAM image to build/last_run_rdram.bin so that
// offline tools can follow G_DL chains through sub-DLs that live
// outside the freeze task's data_ptr window. XOR-3 byte access used.
void dump_full_rdram_to_file() {
    uint8_t* rdram = recomp_runtime_get_rdram();
    if (!rdram) return;
    constexpr uint32_t RDRAM_SIZE = 0x800000u;
    FILE* bf = std::fopen("build/last_run_rdram.bin", "wb");
    if (!bf) return;
    // Buffer in 64KB chunks to avoid 1-byte fwrite overhead.
    constexpr uint32_t CHUNK = 0x10000u;
    std::vector<uint8_t> buf(CHUNK);
    for (uint32_t base = 0; base < RDRAM_SIZE; base += CHUNK) {
        for (uint32_t i = 0; i < CHUNK; i++) {
            buf[i] = rdram[(base + i) ^ 3];
        }
        std::fwrite(buf.data(), 1, CHUNK, bf);
    }
    std::fclose(bf);
}

void dump_sp_task_json(FILE* f, int n_max) {
    if (recomp_sp_task_event_size() != sizeof(SpTaskEvent)) {
        std::fprintf(f, "  \"sp_task_recent\": {\"error\": \"size_mismatch\"},\n");
        return;
    }
    std::vector<SpTaskEvent> buf(n_max);
    size_t got = 0;
    uint64_t widx = 0;
    recomp_sp_task_recent_copy(buf.data(), buf.size(), &got, &widx);
    std::fprintf(f, "  \"sp_task_recent\": {\"write_idx\": %llu, \"events\": [",
                 (unsigned long long)widx);
    for (size_t i = 0; i < got; i++) {
        const auto& e = buf[i];
        std::fprintf(f,
            "%s{\"seq\":%llu,\"ms\":%llu,\"frame\":%llu,\"send_dl\":%llu,"
            "\"type\":\"%s\",\"task_ptr\":%u,\"mips_ra\":%u,"
            "\"ucode\":%u,\"data_ptr\":%u,\"data_size\":%u,"
            "\"output_buff\":%u,\"output_buff_size\":%u,\"flags\":%u}",
            (i ? "," : ""),
            (unsigned long long)e.seq, (unsigned long long)e.ms,
            (unsigned long long)e.frame, (unsigned long long)e.send_dl,
            sp_task_type_name(e.task_type), e.task_ptr, e.mips_ra,
            e.ucode, e.data_ptr, e.data_size,
            e.output_buff, e.output_buff_size, e.task_flags);
    }
    std::fprintf(f, "]},\n");
}

void dump_mesg_json(FILE* f, int n_max) {
    if (ultramodern_mesg_event_size() != sizeof(MesgEvent)) {
        std::fprintf(f, "  \"mesg_recent\": {\"error\": \"size_mismatch\"},\n");
        return;
    }
    std::vector<MesgEvent> buf(n_max);
    size_t got = 0;
    uint64_t widx = 0;
    ultramodern_mesg_recent_copy(buf.data(), buf.size(), &got, &widx);
    std::fprintf(f, "  \"mesg_recent\": {\"write_idx\": %llu, \"events\": [",
                 (unsigned long long)widx);
    for (size_t i = 0; i < got; i++) {
        const auto& e = buf[i];
        std::fprintf(f,
            "%s{\"seq\":%llu,\"ms\":%llu,\"op\":\"%s\",\"mq\":%u,"
            "\"msg\":%u,\"vb\":%u,\"va\":%u,\"block\":%u,\"gt\":%u}",
            (i ? "," : ""),
            (unsigned long long)e.seq, (unsigned long long)e.ms,
            mesg_op_name(e.op), e.mq, e.msg,
            (unsigned)e.valid_before, (unsigned)e.valid_after,
            (unsigned)e.block, (unsigned)e.game_thread);
    }
    std::fprintf(f, "]},\n");
}

void dump_hardware_state_json(FILE* f) {
    uint8_t* rdram = recomp_runtime_get_rdram();
    if (rdram == nullptr) {
        std::fprintf(f, "  \"hardware\": {\"error\": \"no_rdram\"},\n");
        return;
    }

    std::fprintf(f, "  \"hardware\": {\n");

    // gCurrentGameState (vram 0x80075668)
    std::fprintf(f, "    \"gCurrentGameState\": %u,\n",
                 read_be_u32(rdram, 0x80075668));

    // gSegments[16] starting at 0x800A5870 — each entry is { vaddr, size } = 8 bytes.
    std::fprintf(f, "    \"gSegments\": [");
    for (int i = 0; i < 16; i++) {
        uint32_t vaddr = read_be_u32(rdram, 0x800A5870 + i * 8 + 0);
        uint32_t size  = read_be_u32(rdram, 0x800A5870 + i * 8 + 4);
        std::fprintf(f, "%s{\"id\":%d,\"vaddr\":%u,\"size\":%u}",
                     (i ? "," : ""), i, vaddr, size);
    }
    std::fprintf(f, "],\n");

    // gFragments[240] — at 0x800A58F0 (gSegments + 0x80). Each entry { vaddr, size }.
    // Only emit non-zero entries to keep output small.
    std::fprintf(f, "    \"gFragments_loaded\": [");
    bool first_frag = true;
    for (int i = 0; i < 240; i++) {
        uint32_t vaddr = read_be_u32(rdram, 0x800A58F0 + i * 8 + 0);
        uint32_t size  = read_be_u32(rdram, 0x800A58F0 + i * 8 + 4);
        if (vaddr == 0 && size == 0) continue;
        if (!first_frag) std::fputs(",", f);
        first_frag = false;
        std::fprintf(f, "{\"id\":%d,\"vaddr\":%u,\"size\":%u}", i, vaddr, size);
    }
    std::fprintf(f, "],\n");

    // D_800A7464 (scheduler struct ptr) and the first 24 bytes it points at.
    uint32_t sched_ptr = read_be_u32(rdram, 0x800A7464);
    std::fprintf(f, "    \"D_800A7464_ptr\": %u,\n", sched_ptr);
    if (sched_ptr >= 0x80000000u && sched_ptr < 0x80800000u) {
        std::fprintf(f, "    \"D_800A7464_first24\": [");
        for (int i = 0; i < 24; i++) {
            uint32_t paddr = (sched_ptr & 0x1FFFFFFFu) + i;
            uint8_t b = (paddr < 8u * 1024u * 1024u) ? rdram[paddr ^ 3] : 0;
            std::fprintf(f, "%s%u", (i ? "," : ""), (unsigned)b);
        }
        std::fprintf(f, "],\n");
    }

    std::fprintf(f, "    \"_end\": true\n  },\n");
}

void dump_seh_json(FILE* f, EXCEPTION_POINTERS* info) {
    if (info == nullptr) {
        std::fprintf(f, "  \"seh\": null,\n");
        return;
    }
    EXCEPTION_RECORD* er = info->ExceptionRecord;
    std::fprintf(f,
        "  \"seh\": {\n"
        "    \"code\": %lu,\n"
        "    \"address\": %llu,\n",
        er->ExceptionCode, (unsigned long long)(uintptr_t)er->ExceptionAddress);
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
        && er->NumberParameters >= 2) {
        const char* op =
            er->ExceptionInformation[0] == 0 ? "read" :
            er->ExceptionInformation[0] == 1 ? "write" : "execute";
        uintptr_t fault_host = (uintptr_t)er->ExceptionInformation[1];
        uint8_t* rdram = recomp_runtime_get_rdram();
        intptr_t off = rdram ? (intptr_t)(fault_host - (uintptr_t)rdram) : 0;
        uint32_t vaddr = rdram ? (uint32_t)(0x80000000u + (uint32_t)off) : 0;
        std::fprintf(f,
            "    \"access\": \"%s\",\n"
            "    \"fault_host\": %llu,\n"
            "    \"rdram_off\": %lld,\n"
            "    \"decoded_vaddr\": %u,\n",
            op, (unsigned long long)fault_host, (long long)off, vaddr);
    }
    std::fprintf(f, "    \"_end\": true\n  },\n");
}

}  // anon namespace

// ── Public entry ─────────────────────────────────────────────────────

extern "C" void psr_post_mortem_dump(const char* reason,
                                     EXCEPTION_POINTERS* fault_info)
{
    std::lock_guard<std::mutex> lock(g_dump_mutex);

    FILE* f = std::fopen(kReportPath, "w");
    if (!f) return;

    auto t_now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(t_now);
    char timebuf[64] = "?";
    struct tm tmbuf{};
    if (gmtime_s(&tmbuf, &tt) == 0) {
        std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ", &tmbuf);
    }

    std::fprintf(f,
        "{\n"
        "  \"reason\": \"%s\",\n"
        "  \"timestamp\": \"%s\",\n",
        reason ? reason : "unknown", timebuf);

    dump_seh_json(f, fault_info);
    dump_status_json(f);
    dump_hardware_state_json(f);
    dump_trace_json(f, 256);
    dump_libultra_json(f, 128);
    dump_mesg_json(f, 256);
    dump_sp_task_json(f, 4096);
    dump_last_gfx_dl_to_file();
    dump_full_rdram_to_file();
    dump_interp_probe_json(f);
    dump_interp_cf_json(f);
    dump_all_threads_json(f, fault_info);

    std::fprintf(f, "  \"_end\": true\n}\n");
    std::fclose(f);
}
