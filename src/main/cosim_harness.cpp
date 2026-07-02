#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "recomp.h"
#include "librecomp/cosim_state.hpp"
#include "ultramodern/ultra_trace.hpp"
#include "ultramodern/ultramodern.hpp"

extern uint64_t total_vis;

extern "C" size_t ultramodern_sched_thread_state_size(void);
extern "C" void ultramodern_sched_thread_states_copy(
    void* out_void,
    size_t cap,
    size_t* n_written);
extern "C" void ultramodern_get_event_queues(uint32_t* out, int count);

namespace {

constexpr uint32_t kRdramSize = 8u * 1024u * 1024u;
constexpr size_t kMaxContexts = 64;
constexpr size_t kWindowCap = 1024;
constexpr size_t kMaxSchedThreads = 128;

struct SchedThreadState {
    uint32_t valid;
    uint32_t id;
    uint32_t thread;
    int32_t priority;
    uint32_t last_op;
    uint32_t last_queue;
    uint32_t last_head_after;
    uint64_t last_ms;
    uint32_t last_mq;
    uint64_t last_mq_ms;
    uint64_t count;
};

struct ThreadContextMap {
    uint32_t thread;
    recomp_context* ctx;
};

std::atomic<recomp_context*> g_active_ctx{nullptr};
std::mutex g_mu;
std::mutex g_step_mu;
std::condition_variable g_step_cv;
std::vector<recomp_context*> g_contexts;
std::vector<ThreadContextMap> g_thread_contexts;
std::vector<recomp::cosim::Checkpoint> g_window;
std::vector<ultramodern_cosim_quiescence_state> g_window_quiescence;
std::vector<uint32_t> g_window_context_threads;
std::vector<uint8_t> g_last_checkpoint_rdram;
uint64_t g_cp = 0;
uint64_t g_chain = 0;
uint64_t g_stride = 1;
bool g_vi_origin_claimed = false;
bool g_vis_base_set = false;
uint64_t g_vis_base = 0;
bool g_last_raw_vis_set = false;
uint64_t g_last_raw_vis = 0;
std::atomic<uint64_t> g_published_cp{0};
ultramodern_cosim_quiescence_state g_last_quiescence{};
bool g_parked = false;
bool g_parked_by_callback = false;
uint64_t g_parked_cp = 0;
uint64_t g_release_count = 0;
std::mutex g_start_mu;
std::condition_variable g_start_cv;
bool g_start_requested = false;

uint64_t modeled_cycle_now() {
    return ultramodern_cosim_get_time_ticks();
}

uint64_t modeled_cpu_retired_now() {
    return ultramodern_cosim_get_cpu_retired();
}

bool is_rdram_vaddr(uint32_t addr) {
    return addr >= 0x80000000u && addr < 0x80800000u;
}

OSThread* live_thread_ptr(uint8_t* rdram, uint32_t thread) {
    if (rdram == nullptr || !is_rdram_vaddr(thread)) {
        return nullptr;
    }
    return reinterpret_cast<OSThread*>(rdram + (thread & 0x007FFFFFu));
}

const char* thread_state_name(uint32_t state) {
    switch (state) {
        case OSThreadState::STOPPED: return "stopped";
        case OSThreadState::QUEUED: return "queued";
        case OSThreadState::RUNNING: return "running";
        case OSThreadState::BLOCKED: return "blocked";
        default: return "unknown";
    }
}

const char* queue_event_name(uint32_t queue, const uint32_t event_queues[5]) {
    if (queue == 0) {
        return "none";
    }
    static const char* kNames[5] = {"sp", "dp", "ai", "si", "vi"};
    for (size_t i = 0; i < 5; i++) {
        if (event_queues[i] != 0 && queue == event_queues[i]) {
            return kNames[i];
        }
    }
    return "other";
}

void remember_context(recomp_context* ctx) {
    if (ctx == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    if (std::find(g_contexts.begin(), g_contexts.end(), ctx) == g_contexts.end()) {
        if (g_contexts.size() < kMaxContexts) {
            g_contexts.push_back(ctx);
        }
    }
}

void remember_thread_context(uint32_t thread, recomp_context* ctx) {
    if (!is_rdram_vaddr(thread) || ctx == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    for (ThreadContextMap& entry : g_thread_contexts) {
        if (entry.thread == thread) {
            entry.ctx = ctx;
            return;
        }
    }
    if (g_thread_contexts.size() < kMaxContexts) {
        g_thread_contexts.push_back(ThreadContextMap{ thread, ctx });
    }
}

recomp_context* context_for_thread_locked(uint32_t thread) {
    for (const ThreadContextMap& entry : g_thread_contexts) {
        if (entry.thread == thread) {
            return entry.ctx;
        }
    }
    return nullptr;
}

recomp_context* select_snapshot_context(uint8_t* rdram, recomp_context* fallback, uint32_t& selected_thread) {
    selected_thread = 0;
    if (rdram == nullptr) {
        return fallback;
    }

    SchedThreadState states[kMaxSchedThreads]{};
    size_t n = 0;
    ultramodern_sched_thread_states_copy(states, kMaxSchedThreads, &n);

    std::lock_guard<std::mutex> lock(g_mu);
    for (size_t i = 0; i < n; i++) {
        const uint32_t thread = states[i].thread;
        if (!states[i].valid || !is_rdram_vaddr(thread)) {
            continue;
        }
        OSThread* live = live_thread_ptr(rdram, thread);
        if (live == nullptr || live->context == nullptr) {
            continue;
        }
        if (live->state == OSThreadState::RUNNING && live->priority <= 0) {
            if (recomp_context* ctx = context_for_thread_locked(thread)) {
                selected_thread = thread;
                return ctx;
            }
        }
    }

    for (size_t i = 0; i < n; i++) {
        const uint32_t thread = states[i].thread;
        if (!states[i].valid || !is_rdram_vaddr(thread)) {
            continue;
        }
        OSThread* live = live_thread_ptr(rdram, thread);
        if (live == nullptr || live->context == nullptr) {
            continue;
        }
        if (live->state == OSThreadState::RUNNING) {
            if (recomp_context* ctx = context_for_thread_locked(thread)) {
                selected_thread = thread;
                return ctx;
            }
        }
    }

    return fallback;
}

std::string json_u64_array(const uint64_t* vals, size_t count) {
    std::string out = "[";
    for (size_t i = 0; i < count; i++) {
        if (i) out += ",";
        out += std::to_string(vals[i]);
    }
    out += "]";
    return out;
}

std::string json_u32_array(const uint32_t* vals, size_t count) {
    std::string out = "[";
    for (size_t i = 0; i < count; i++) {
        if (i) out += ",";
        out += std::to_string(vals[i]);
    }
    out += "]";
    return out;
}

std::string checkpoint_json_fields(const recomp::cosim::Checkpoint& cp) {
    char buf[640];
    std::snprintf(buf, sizeof(buf),
        "\"cp\":%llu,\"vis\":%llu,\"cycle\":%llu,\"cycle_count\":%llu,\"cpu_retired\":%llu,"
        "\"cpu_int\":%llu,\"cp0\":%llu,\"cp1\":%llu,\"rdram\":%llu,\"chain\":%llu",
        (unsigned long long)cp.cp,
        (unsigned long long)cp.vis,
        (unsigned long long)cp.cycle,
        (unsigned long long)cp.cycle,
        (unsigned long long)cp.cpu_retired,
        (unsigned long long)cp.sub.cpu_int,
        (unsigned long long)cp.sub.cp0,
        (unsigned long long)cp.sub.cp1,
        (unsigned long long)cp.sub.rdram,
        (unsigned long long)cp.chain);
    return std::string(buf);
}

std::string checkpoint_json(const recomp::cosim::Checkpoint& cp, bool prefix_comma) {
    return std::string(prefix_comma ? ",{" : "{") + checkpoint_json_fields(cp) + "}";
}

bool normalize_host_only_rdram(
    const uint8_t* rdram,
    std::vector<uint8_t>& out,
    std::string& error)
{
    const size_t runtime_size = ultramodern_sched_thread_state_size();
    if (runtime_size != sizeof(SchedThreadState)) {
        error = "scheduler thread state size mismatch";
        return false;
    }

    out.assign(rdram, rdram + kRdramSize);

    SchedThreadState states[kMaxSchedThreads]{};
    size_t n = 0;
    ultramodern_sched_thread_states_copy(states, kMaxSchedThreads, &n);
    for (size_t i = 0; i < n; i++) {
        const uint32_t thread = states[i].thread;
        if (!states[i].valid || !is_rdram_vaddr(thread)) {
            continue;
        }
        const uint32_t paddr = thread & 0x007FFFFFu;
        constexpr size_t kContextOff = offsetof(OSThread, context);
        if (paddr + kContextOff + sizeof(UltraThreadContext*) > out.size()) {
            continue;
        }
        std::memset(out.data() + paddr + kContextOff, 0, sizeof(UltraThreadContext*));

        recomp_context* ctx = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            ctx = context_for_thread_locked(thread);
        }
        if (ctx == nullptr) {
            continue;
        }
        const uint32_t sp = static_cast<uint32_t>(ctx->r29);
        if (!is_rdram_vaddr(sp)) {
            continue;
        }

        // Bytes below the saved stack pointer are outside the live MIPS frame.
        // HLE/libultra handoff paths can leave host-order residue there without
        // changing guest-visible state at the checkpoint.
        constexpr uint32_t kDeadStackRedzone = 64;
        const uint32_t sp_paddr = sp & 0x007FFFFFu;
        const uint32_t start = sp_paddr > kDeadStackRedzone
            ? sp_paddr - kDeadStackRedzone
            : 0;
        if (start < sp_paddr && sp_paddr <= out.size()) {
            std::memset(out.data() + start, 0, sp_paddr - start);
        }
    }
    return true;
}

bool snapshot(
    recomp::cosim::Checkpoint& out,
    bool advance_chain,
    std::string& error,
    std::vector<uint8_t>* normalized_capture = nullptr,
    uint32_t* selected_thread_capture = nullptr,
    uint64_t* raw_vis_capture = nullptr)
{
    uint8_t* rdram = recomp_runtime_get_rdram();
    recomp_context* ctx = g_active_ctx.load(std::memory_order_acquire);
    if (rdram == nullptr) {
        error = "rdram not captured yet";
        return false;
    }
    if (ctx == nullptr) {
        error = "recomp_context not captured yet";
        return false;
    }

    const uint64_t raw_vis = total_vis;
    const uint64_t cycle = modeled_cycle_now();
    const uint64_t cpu_retired = modeled_cpu_retired_now();
    if (raw_vis_capture != nullptr) {
        *raw_vis_capture = raw_vis;
    }
    std::vector<uint8_t> normalized_rdram;
    if (!normalize_host_only_rdram(rdram, normalized_rdram, error)) {
        return false;
    }
    uint32_t selected_thread = 0;
    ctx = select_snapshot_context(rdram, ctx, selected_thread);
    if (selected_thread_capture != nullptr) {
        *selected_thread_capture = selected_thread;
    }
    if (normalized_capture != nullptr) {
        *normalized_capture = normalized_rdram;
    }
    const recomp::cosim::SubHashes sub =
        recomp::cosim::hash_state(ctx, normalized_rdram.data(), kRdramSize);

    std::lock_guard<std::mutex> lock(g_mu);
    if (advance_chain && g_last_raw_vis_set && raw_vis <= g_last_raw_vis) {
        error = "not a new VI boundary";
        return false;
    }
    if (advance_chain && !g_vis_base_set) {
        g_vis_base = raw_vis;
        g_vis_base_set = true;
    }
    const uint64_t vis = g_vis_base_set ? raw_vis - g_vis_base : raw_vis;
    uint64_t cp_index = g_cp;
    uint64_t chain = g_chain;
    if (advance_chain) {
        chain = recomp::cosim::chain_advance(g_chain, sub, vis, cycle);
        cp_index = ++g_cp;
        g_chain = chain;
        g_last_raw_vis = raw_vis;
        g_last_raw_vis_set = true;
    }

    out = recomp::cosim::Checkpoint{
        cp_index,
        vis,
        cycle,
        cpu_retired,
        sub,
        chain,
    };

    if (advance_chain) {
        if (g_window.size() == kWindowCap) {
            g_window.erase(g_window.begin());
            if (!g_window_quiescence.empty()) {
                g_window_quiescence.erase(g_window_quiescence.begin());
            }
            if (!g_window_context_threads.empty()) {
                g_window_context_threads.erase(g_window_context_threads.begin());
            }
        }
        g_window.push_back(out);
        g_window_quiescence.push_back(g_last_quiescence);
        g_window_context_threads.push_back(selected_thread);
        g_last_checkpoint_rdram = normalized_rdram;
        g_published_cp.store(out.cp, std::memory_order_release);
    }
    return true;
}

bool commit_captured_checkpoint(
    const recomp::cosim::Checkpoint& captured,
    uint64_t raw_vis,
    const std::vector<uint8_t>& normalized_rdram,
    uint32_t selected_thread,
    recomp::cosim::Checkpoint& out,
    std::string& error)
{
    if (normalized_rdram.size() != kRdramSize) {
        error = "captured RDRAM size mismatch";
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mu);
    if (g_last_raw_vis_set && raw_vis <= g_last_raw_vis) {
        error = "not a new VI boundary";
        return false;
    }
    if (!g_vis_base_set) {
        g_vis_base = raw_vis;
        g_vis_base_set = true;
    }
    const uint64_t vis = raw_vis - g_vis_base;
    const uint64_t chain =
        recomp::cosim::chain_advance(g_chain, captured.sub, vis, captured.cycle);

    out = recomp::cosim::Checkpoint{
        ++g_cp,
        vis,
        captured.cycle,
        captured.cpu_retired,
        captured.sub,
        chain,
    };
    g_chain = chain;
    g_last_raw_vis = raw_vis;
    g_last_raw_vis_set = true;

    if (g_window.size() == kWindowCap) {
        g_window.erase(g_window.begin());
        if (!g_window_quiescence.empty()) {
            g_window_quiescence.erase(g_window_quiescence.begin());
        }
        if (!g_window_context_threads.empty()) {
            g_window_context_threads.erase(g_window_context_threads.begin());
        }
    }
    g_window.push_back(out);
    g_window_quiescence.push_back(g_last_quiescence);
    g_window_context_threads.push_back(selected_thread);
    g_last_checkpoint_rdram = normalized_rdram;
    g_published_cp.store(out.cp, std::memory_order_release);
    return true;
}

std::string quiescence_json_fields(const ultramodern_cosim_quiescence_state& q) {
    char buf[360];
    std::snprintf(buf, sizeof(buf),
        "\"vi_queue\":%u,\"running_head\":%u,\"known_threads\":%u,"
        "\"blocked_on_vi\":%u,\"blocked_on_other\":%u,"
        "\"idle_running\":%u,\"runnable_or_unknown\":%u,"
        "\"external_pending\":%u,\"scheduler_active\":%u,\"quiescent\":%s",
        q.vi_queue,
        q.running_head,
        q.known_threads,
        q.blocked_on_vi,
        q.blocked_on_other,
        q.idle_running,
        q.runnable_or_unknown,
        q.external_pending,
        q.scheduler_active,
        q.quiescent ? "true" : "false");
    return std::string(buf);
}

std::string park_json_fields(
    bool parked,
    bool callback,
    uint64_t parked_cp,
    uint64_t release_count)
{
    char buf[192];
    std::snprintf(buf, sizeof(buf),
        "\"parked\":%s,\"source\":\"%s\",\"parked_cp\":%llu,\"release_count\":%llu",
        parked ? "true" : "false",
        callback ? "callback" : "poll",
        (unsigned long long)parked_cp,
        (unsigned long long)release_count);
    return std::string(buf);
}

bool latest_checkpoint(recomp::cosim::Checkpoint& out) {
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_window.empty()) {
        return false;
    }
    out = g_window.back();
    return true;
}

bool same_quiescence_state(
    const ultramodern_cosim_quiescence_state& a,
    const ultramodern_cosim_quiescence_state& b)
{
    return a.vi_queue == b.vi_queue &&
           a.running_head == b.running_head &&
           a.known_threads == b.known_threads &&
           a.blocked_on_vi == b.blocked_on_vi &&
           a.blocked_on_other == b.blocked_on_other &&
           a.idle_running == b.idle_running &&
           a.runnable_or_unknown == b.runnable_or_unknown &&
           a.external_pending == b.external_pending &&
           a.scheduler_active == b.scheduler_active &&
           a.quiescent == b.quiescent;
}

bool scheduler_epoch_is_stable(uint64_t epoch) {
    return (epoch & 1u) == 0;
}

uint32_t request_deterministic_vi(uint32_t count) {
    const uint32_t accepted = ultramodern_cosim_request_vi(count);
    if (accepted != 0) {
        std::lock_guard<std::mutex> lock(g_mu);
        g_vi_origin_claimed = true;
    }
    return accepted;
}

void claim_polled_vi_origin_if_needed() {
    bool should_rebase = false;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (!g_vi_origin_claimed) {
            g_vi_origin_claimed = true;
            should_rebase = true;
        }
    }
    if (should_rebase) {
        ultramodern_cosim_reset_vi_counter();
    }
}

bool publish_polled_quiescence_checkpoint(const ultramodern_cosim_quiescence_state& q) {
    if (!q.quiescent) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(g_step_mu);
        if (g_parked) {
            return true;
        }
    }

    constexpr uint32_t kStableAttempts = 8;
    for (uint32_t attempt = 0; attempt < kStableAttempts; attempt++) {
        const uint64_t scheduler_epoch_before = ultramodern_cosim_scheduler_epoch();
        if (!scheduler_epoch_is_stable(scheduler_epoch_before)) {
            continue;
        }

        ultramodern_cosim_quiescence_state before{};
        ultramodern_cosim_get_quiescence(&before);
        if (ultramodern_cosim_scheduler_epoch() != scheduler_epoch_before) {
            continue;
        }
        if (!before.quiescent) {
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(g_mu);
            g_last_quiescence = before;
        }

        claim_polled_vi_origin_if_needed();

        recomp::cosim::Checkpoint captured{};
        std::vector<uint8_t> captured_rdram;
        uint32_t selected_thread = 0;
        uint64_t raw_vis = 0;
        std::string error;
        if (!snapshot(
                captured,
                false,
                error,
                &captured_rdram,
                &selected_thread,
                &raw_vis)) {
            return false;
        }
        if (ultramodern_cosim_scheduler_epoch() != scheduler_epoch_before) {
            continue;
        }

        ultramodern_cosim_quiescence_state after{};
        ultramodern_cosim_get_quiescence(&after);
        if (ultramodern_cosim_scheduler_epoch() != scheduler_epoch_before) {
            continue;
        }
        if (!after.quiescent ||
            !same_quiescence_state(before, after) ||
            total_vis != raw_vis) {
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(g_mu);
            g_last_quiescence = after;
        }

        recomp::cosim::Checkpoint cp{};
        if (!commit_captured_checkpoint(
                captured,
                raw_vis,
                captured_rdram,
                selected_thread,
                cp,
                error)) {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(g_step_mu);
            if (!g_parked) {
                g_parked = true;
                g_parked_by_callback = false;
                g_parked_cp = cp.cp;
            }
        }
        g_step_cv.notify_all();
        return true;
    }
    return false;
}

bool parse_gpr_field(const std::string& field, int& idx) {
    const char* p = nullptr;
    if (field.size() >= 2 && field[0] == 'r') {
        p = field.c_str() + 1;
    } else if (field.rfind("gpr", 0) == 0 && field.size() >= 4) {
        p = field.c_str() + 3;
    }
    if (p == nullptr || *p == '\0') {
        return false;
    }
    char* end = nullptr;
    long v = std::strtol(p, &end, 10);
    if (*end != '\0' || v < 0 || v > 31) {
        return false;
    }
    idx = static_cast<int>(v);
    return true;
}

uint32_t addr_to_paddr(uint32_t addr) {
    return addr & 0x1FFFFFFFu;
}

} // namespace

extern "C" void psr_cosim_register_context(uint8_t* rdram, recomp_context* ctx) {
    if (rdram != nullptr) {
        recomp_runtime_set_rdram(rdram);
    }
    if (ctx != nullptr) {
        g_active_ctx.store(ctx, std::memory_order_release);
        remember_context(ctx);
    }
}

extern "C" void psr_cosim_set_active_context(unsigned char* rdram, void* ctx) {
    if (rdram != nullptr) {
        recomp_runtime_set_rdram(rdram);
    }
    if (ctx != nullptr) {
        recomp_context* recomp_ctx = static_cast<recomp_context*>(ctx);
        g_active_ctx.store(recomp_ctx, std::memory_order_release);
        remember_context(recomp_ctx);
        if (ultramodern::is_game_thread()) {
            remember_thread_context(static_cast<uint32_t>(ultramodern::this_thread()), recomp_ctx);
        }
    }
}

extern "C" int psr_cosim_on_quiescent_vi(
    uint8_t* rdram,
    const ultramodern_cosim_quiescence_state* state)
{
    if (rdram != nullptr) {
        recomp_runtime_set_rdram(rdram);
    }
    if (state != nullptr) {
        std::lock_guard<std::mutex> lock(g_mu);
        g_last_quiescence = *state;
        g_vi_origin_claimed = true;
    }
    {
        std::lock_guard<std::mutex> lock(g_step_mu);
        if (g_parked) {
            return 0;
        }
    }

    recomp::cosim::Checkpoint cp{};
    std::string error;
    if (snapshot(cp, true, error)) {
        {
            std::unique_lock<std::mutex> lock(g_step_mu);
            g_parked = true;
            g_parked_by_callback = true;
            g_parked_cp = cp.cp;
            g_step_cv.notify_all();
            g_step_cv.wait(lock, []() {
                return g_release_count != 0;
            });
            g_release_count--;
            g_parked = false;
            g_parked_by_callback = false;
        }
        g_step_cv.notify_all();
        return 1;
    }
    return 0;
}

extern "C" void psr_cosim_wait_for_start() {
    std::unique_lock<std::mutex> lock(g_start_mu);
    g_start_cv.wait(lock, []() {
        return g_start_requested;
    });
}

namespace pkmnstadium::cosim {

std::string start_json() {
    {
        std::lock_guard<std::mutex> lock(g_start_mu);
        g_start_requested = true;
    }
    g_start_cv.notify_all();
    return R"({"ok":true,"started":true})";
}

std::string chain_json() {
    recomp::cosim::Checkpoint cp{};
    bool parked = false;
    {
        std::lock_guard<std::mutex> lock(g_step_mu);
        parked = g_parked;
    }
    const char* mode = parked
        ? "t3_parked_deterministic_vi"
        : "t3_quiescent_observed_unparked";
    if (!latest_checkpoint(cp)) {
        std::string error;
        if (!snapshot(cp, false, error)) {
            return std::string(R"({"ok":false,"error":")") + error + R"("})";
        }
        mode = "t2_live_nonparked";
    }
    return std::string(R"({"ok":true,"mode":")") + mode + R"(",)" +
           checkpoint_json(cp, false).substr(1);
}

std::string sub_json() {
    recomp::cosim::Checkpoint cp{};
    std::string error;
    if (!snapshot(cp, false, error)) {
        return std::string(R"({"ok":false,"error":")") + error + R"("})";
    }
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"mode\":\"t2_live_nonparked\",\"cp\":%llu,\"vis\":%llu,"
        "\"cycle\":%llu,\"cycle_count\":%llu,\"cpu_retired\":%llu,"
        "\"cpu_int\":%llu,\"cp0\":%llu,\"cp1\":%llu,\"rdram\":%llu}",
        (unsigned long long)cp.cp,
        (unsigned long long)cp.vis,
        (unsigned long long)cp.cycle,
        (unsigned long long)cp.cycle,
        (unsigned long long)cp.cpu_retired,
        (unsigned long long)cp.sub.cpu_int,
        (unsigned long long)cp.sub.cp0,
        (unsigned long long)cp.sub.cp1,
        (unsigned long long)cp.sub.rdram);
    return std::string(buf);
}

std::string regs_json() {
    recomp_context* ctx = g_active_ctx.load(std::memory_order_acquire);
    if (ctx == nullptr) {
        return R"({"ok":false,"error":"recomp_context not captured yet"})";
    }

    uint64_t gpr[32];
    std::memcpy(gpr, &ctx->r0, sizeof(gpr));

    uint64_t fpr[32];
    const ::fpr* fp = &ctx->f0;
    for (int i = 0; i < 32; i++) {
        fpr[i] = fp[i].u64;
    }

    size_t context_count = 0;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        context_count = g_contexts.size();
    }

    std::string out = R"({"ok":true,"mode":"t2_live_nonparked")";
    out += R"(,"active_context":)";
    out += std::to_string(reinterpret_cast<uintptr_t>(ctx));
    out += R"(,"context_count":)";
    out += std::to_string(context_count);
    out += R"(,"hi":)";
    out += std::to_string(ctx->hi);
    out += R"(,"lo":)";
    out += std::to_string(ctx->lo);
    out += R"(,"status_reg":)";
    out += std::to_string(ctx->status_reg);
    out += R"(,"mips3_float_mode":)";
    out += std::to_string(ctx->mips3_float_mode);
    out += R"(,"gpr":)";
    out += json_u64_array(gpr, 32);
    out += R"(,"cp0":)";
    out += json_u32_array(ctx->cop0_regs, 32);
    out += R"(,"fpr":)";
    out += json_u64_array(fpr, 32);
    out += "}";
    return out;
}

std::string stride_json(uint64_t stride) {
    if (stride == 0) {
        stride = 1;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    g_stride = stride;
    return std::string(R"({"ok":true,"stride":)") + std::to_string(g_stride) + "}";
}

std::string step_json(uint64_t frames, uint64_t timeout_ms) {
    if (frames == 0) {
        frames = 1;
    }
    if (timeout_ms == 0) {
        timeout_ms = 5000;
    }

    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);

    uint64_t stride = 1;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        stride = g_stride;
    }

    auto timeout_json = [&](const char* stage, uint64_t start, uint64_t target) {
        ultramodern_cosim_quiescence_state q{};
        ultramodern_cosim_get_quiescence(&q);
        bool parked = false;
        bool callback = false;
        uint64_t parked_cp = 0;
        uint64_t release_count = 0;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            g_last_quiescence = q;
        }
        {
            std::lock_guard<std::mutex> lock(g_step_mu);
            parked = g_parked;
            callback = g_parked_by_callback;
            parked_cp = g_parked_cp;
            release_count = g_release_count;
        }
        return std::string(R"({"ok":false,"error":"timed out waiting for VI quiescence checkpoint",)"
                           R"("stage":")") +
               stage +
               R"(","published_cp":)" +
               std::to_string(g_published_cp.load(std::memory_order_acquire)) +
               R"(,"park":{)" +
               park_json_fields(parked, callback, parked_cp, release_count) +
               R"(},"start_cp":)" +
               std::to_string(start) +
               R"(,"target_cp":)" +
               std::to_string(target) +
               R"(,"stride":)" +
               std::to_string(stride) +
               R"(,"timeout_ms":)" +
               std::to_string(timeout_ms) +
               R"(,"mode":"t3_parked_deterministic_vi","quiescence":{)" +
               quiescence_json_fields(q) +
               "}}";
    };

    while (true) {
        {
            std::lock_guard<std::mutex> lock(g_step_mu);
            if (g_parked) {
                break;
            }
        }

        ultramodern_cosim_quiescence_state q{};
        ultramodern_cosim_get_quiescence(&q);
        if (publish_polled_quiescence_checkpoint(q)) {
            break;
        }

        const auto slice_deadline = std::min(
            deadline,
            Clock::now() + std::chrono::milliseconds(16));
        {
            std::unique_lock<std::mutex> lock(g_step_mu);
            if (g_step_cv.wait_until(lock, slice_deadline, []() {
                    return g_parked;
                })) {
                break;
            }
        }
        ultramodern_cosim_get_quiescence(&q);
        if (publish_polled_quiescence_checkpoint(q)) {
            break;
        }

        if (Clock::now() >= deadline) {
            const uint64_t cp = g_published_cp.load(std::memory_order_acquire);
            return timeout_json("initial_park", cp, cp + frames * stride);
        }
    }

    const uint64_t start = g_published_cp.load(std::memory_order_acquire);
    const uint64_t target = start + frames * stride;

    while (g_published_cp.load(std::memory_order_acquire) < target) {
        const uint64_t before = g_published_cp.load(std::memory_order_acquire);
        recomp::cosim::Checkpoint before_cp{};
        const uint64_t before_vis = latest_checkpoint(before_cp) ? before_cp.vis : total_vis;
        bool request_vi = false;
        {
            std::unique_lock<std::mutex> lock(g_step_mu);
            const bool parked = g_step_cv.wait_until(lock, deadline, [&]() {
                return g_parked && g_parked_cp >= before;
            });
            if (!parked) {
                lock.unlock();
                return timeout_json("release_ready", start, target);
            }
            if (g_parked_by_callback) {
                g_release_count++;
                request_vi = true;
                g_step_cv.notify_all();
            } else {
                g_parked = false;
                g_parked_cp = 0;
                request_vi = true;
            }
        }

        if (request_vi) {
            std::unique_lock<std::mutex> lock(g_step_mu);
            if (!g_step_cv.wait_until(lock, deadline, []() {
                    return !g_parked;
                })) {
                lock.unlock();
                return timeout_json("release_unpark", start, target);
            }
        }

        if (request_vi) {
            request_deterministic_vi(1);
        }

        while (g_published_cp.load(std::memory_order_acquire) <= before) {
            {
                std::lock_guard<std::mutex> lock(g_step_mu);
                if (g_parked && g_published_cp.load(std::memory_order_acquire) > before) {
                    break;
                }
            }

            ultramodern_cosim_quiescence_state idle_q{};
            ultramodern_cosim_get_quiescence(&idle_q);
            if (idle_q.quiescent &&
                ultramodern_cosim_advance_due_vi(recomp_runtime_get_rdram(), 1) != 0) {
                continue;
            }

            if (total_vis > before_vis) {
                ultramodern_cosim_quiescence_state q{};
                ultramodern_cosim_get_quiescence(&q);
                if (publish_polled_quiescence_checkpoint(q) &&
                    g_published_cp.load(std::memory_order_acquire) > before) {
                    break;
                }
            }

            if (Clock::now() >= deadline) {
                return timeout_json("next_park", start, target);
            }
            std::unique_lock<std::mutex> lock(g_step_mu);
            g_step_cv.wait_until(lock, std::min(
                deadline,
                Clock::now() + std::chrono::milliseconds(16)));
        }
    }

    recomp::cosim::Checkpoint cp{};
    if (!latest_checkpoint(cp)) {
        return R"({"ok":false,"error":"checkpoint reached but no row is available"})";
    }
    return std::string(R"({"ok":true,"mode":"t3_parked_deterministic_vi",)") +
           checkpoint_json(cp, false).substr(1);
}

std::string window_json(uint64_t count) {
    std::lock_guard<std::mutex> lock(g_mu);
    if (count == 0 || count > g_window.size()) {
        count = g_window.size();
    }
    const size_t start = g_window.size() - static_cast<size_t>(count);
    std::string out = R"({"ok":true,"count":)";
    out += std::to_string(count);
    out += R"(,"rows":[)";
    for (size_t i = 0; i < static_cast<size_t>(count); i++) {
        if (i != 0) out += ",";
        out += "{";
        out += checkpoint_json_fields(g_window[start + i]);
        if (start + i < g_window_quiescence.size()) {
            out += R"(,"quiescence":{)";
            out += quiescence_json_fields(g_window_quiescence[start + i]);
            out += "}";
        }
        if (start + i < g_window_context_threads.size()) {
            out += R"(,"ctx_thread":)";
            out += std::to_string(g_window_context_threads[start + i]);
        }
        out += "}";
    }
    out += "]}";
    return out;
}

std::string reset_json() {
    ultramodern_cosim_reset_vi_counter();
    ultramodern_cosim_reset_time();
    ultramodern_cosim_reset_rcp_events();
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_cp = 0;
        g_chain = 0;
        g_window.clear();
        g_window_quiescence.clear();
        g_window_context_threads.clear();
        g_last_checkpoint_rdram.clear();
        g_last_quiescence = {};
        g_vi_origin_claimed = false;
        g_vis_base_set = false;
        g_vis_base = 0;
        g_last_raw_vis_set = false;
        g_last_raw_vis = 0;
        g_published_cp.store(0, std::memory_order_release);
    }
    {
        std::lock_guard<std::mutex> lock(g_step_mu);
        if (g_parked && g_parked_by_callback) {
            g_release_count++;
        }
        g_parked = false;
        g_parked_by_callback = false;
        g_parked_cp = 0;
    }
    g_step_cv.notify_all();
    return R"({"ok":true,"cp":0,"chain":0})";
}

std::string quiescence_json() {
    ultramodern_cosim_quiescence_state q{};
    ultramodern_cosim_get_quiescence(&q);
    bool parked = false;
    bool callback = false;
    uint64_t parked_cp = 0;
    uint64_t release_count = 0;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_last_quiescence = q;
    }
    {
        std::lock_guard<std::mutex> lock(g_step_mu);
        parked = g_parked;
        callback = g_parked_by_callback;
        parked_cp = g_parked_cp;
        release_count = g_release_count;
    }
    return std::string(R"({"ok":true,)") +
           quiescence_json_fields(q) +
           R"(,"park":{)" +
           park_json_fields(parked, callback, parked_cp, release_count) +
           "}}";
}

std::string threads_json() {
    const size_t runtime_size = ultramodern_sched_thread_state_size();
    if (runtime_size != sizeof(SchedThreadState)) {
        return std::string(R"({"ok":false,"error":"scheduler thread state size mismatch",)") +
               R"("runtime_size":)" +
               std::to_string(runtime_size) +
               R"(,"harness_size":)" +
               std::to_string(sizeof(SchedThreadState)) +
               "}";
    }

    uint8_t* rdram = recomp_runtime_get_rdram();
    uint32_t event_queues[5]{};
    ultramodern_get_event_queues(event_queues, 5);

    SchedThreadState states[kMaxSchedThreads]{};
    size_t n = 0;
    ultramodern_sched_thread_states_copy(states, kMaxSchedThreads, &n);

    std::string out = R"({"ok":true,"count":)";
    out += std::to_string(n);
    out += R"(,"event_queues":{"sp":)";
    out += std::to_string(event_queues[0]);
    out += R"(,"dp":)";
    out += std::to_string(event_queues[1]);
    out += R"(,"ai":)";
    out += std::to_string(event_queues[2]);
    out += R"(,"si":)";
    out += std::to_string(event_queues[3]);
    out += R"(,"vi":)";
    out += std::to_string(event_queues[4]);
    out += R"(},"threads":[)";

    for (size_t i = 0; i < n; i++) {
        const SchedThreadState& s = states[i];
        OSThread* live = live_thread_ptr(rdram, s.thread);
        const bool live_valid = live != nullptr && live->context != nullptr;
        const uint32_t live_state = live != nullptr ? live->state : 0xFFFFFFFFu;
        const uint32_t live_queue = live != nullptr ? static_cast<uint32_t>(live->queue) : 0u;
        const int32_t live_priority = live != nullptr ? live->priority : 0;
        const uint32_t wait_queue =
            live_valid && live_state == OSThreadState::BLOCKED
                ? live_queue
                : s.last_mq;
        const char* wait_event = queue_event_name(wait_queue, event_queues);
        recomp_context* thread_ctx = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            thread_ctx = context_for_thread_locked(s.thread);
        }

        if (i) {
            out += ",";
        }
        out += "{";
        out += R"("id":)";
        out += std::to_string(s.id);
        out += R"(,"thread":)";
        out += std::to_string(s.thread);
        out += R"(,"live":)";
        out += live_valid ? "true" : "false";
        out += R"(,"state":")";
        out += thread_state_name(live_state);
        out += R"(","state_id":)";
        out += std::to_string(live_state);
        out += R"(,"priority":)";
        out += std::to_string(live_priority);
        out += R"(,"queue":)";
        out += std::to_string(live_queue);
        out += R"(,"wait_queue":)";
        out += std::to_string(wait_queue);
        out += R"(,"wait_event":")";
        out += wait_event;
        out += R"(","last_op":)";
        out += std::to_string(s.last_op);
        out += R"(,"last_queue":)";
        out += std::to_string(s.last_queue);
        out += R"(,"last_mq":)";
        out += std::to_string(s.last_mq);
        out += R"(,"last_ms":)";
        out += std::to_string(s.last_ms);
        out += R"(,"count":)";
        out += std::to_string(s.count);
        if (thread_ctx != nullptr) {
            out += R"(,"ctx":true,"sp":)";
            out += std::to_string(thread_ctx->r29);
            out += R"(,"ra":)";
            out += std::to_string(thread_ctx->r31);
            out += R"(,"v0":)";
            out += std::to_string(thread_ctx->r2);
            out += R"(,"v1":)";
            out += std::to_string(thread_ctx->r3);
        } else {
            out += R"(,"ctx":false)";
        }
        out += "}";
    }

    out += "]}";
    return out;
}

std::string normalized_rdram_peek_json(uint32_t addr, uint64_t count) {
    if (count < 1) {
        count = 1;
    }
    if (count > 256) {
        count = 256;
    }
    const uint32_t paddr = addr_to_paddr(addr);
    if (paddr + count > kRdramSize) {
        return R"({"ok":false,"error":"oob"})";
    }

    uint8_t* rdram = recomp_runtime_get_rdram();
    if (rdram == nullptr) {
        return R"({"ok":false,"error":"rdram not captured yet"})";
    }

    std::vector<uint8_t> normalized;
    std::string error;
    if (!normalize_host_only_rdram(rdram, normalized, error)) {
        return std::string(R"({"ok":false,"error":")") + error + R"("})";
    }

    std::string hex;
    hex.reserve(static_cast<size_t>(count) * 2);
    char tmp[4];
    for (uint64_t i = 0; i < count; i++) {
        std::snprintf(tmp, sizeof(tmp), "%02x", normalized[paddr + static_cast<uint32_t>(i)]);
        hex += tmp;
    }
    return R"({"ok":true,"addr":)" + std::to_string(addr) +
           R"(,"paddr":)" + std::to_string(paddr) +
           R"(,"n":)" + std::to_string(count) +
           R"(,"hex":")" + hex + R"("})";
}

std::string normalized_rdram_digest_json(uint32_t addr, uint64_t count) {
    if (count < 1) {
        count = 1;
    }
    if (count > 0x40000) {
        count = 0x40000;
    }
    const uint32_t paddr = addr_to_paddr(addr);
    if (paddr + count > kRdramSize) {
        return R"({"ok":false,"error":"oob"})";
    }

    uint8_t* rdram = recomp_runtime_get_rdram();
    if (rdram == nullptr) {
        return R"({"ok":false,"error":"rdram not captured yet"})";
    }

    std::vector<uint8_t> normalized;
    std::string error;
    if (!normalize_host_only_rdram(rdram, normalized, error)) {
        return std::string(R"({"ok":false,"error":")") + error + R"("})";
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
    for (uint64_t i = 0; i < count; i++) {
        const uint8_t b = normalized[paddr + static_cast<uint32_t>(i)];
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
                first_nonzero = static_cast<int>(i);
            }
            last_nonzero = static_cast<int>(i);
        }
        fnv ^= b;
        fnv *= 1099511628211ull;
    }

    char hash_buf[32];
    std::snprintf(hash_buf, sizeof(hash_buf), "%016llx", (unsigned long long)fnv);
    return R"({"ok":true,"addr":)" + std::to_string(addr) +
           R"(,"paddr":)" + std::to_string(paddr) +
           R"(,"n":)" + std::to_string(count) +
           R"(,"nonzero_bytes":)" + std::to_string(nonzero) +
           R"(,"unique_bytes":)" + std::to_string(unique) +
           R"(,"first_nonzero":)" + std::to_string(first_nonzero) +
           R"(,"last_nonzero":)" + std::to_string(last_nonzero) +
           R"(,"fnv64":")" + hash_buf +
           R"(","first64":")" + first_hex + R"("})";
}

std::string checkpoint_rdram_peek_json(uint32_t addr, uint64_t count) {
    if (count < 1) {
        count = 1;
    }
    if (count > 256) {
        count = 256;
    }
    const uint32_t paddr = addr_to_paddr(addr);
    if (paddr + count > kRdramSize) {
        return R"({"ok":false,"error":"oob"})";
    }

    std::vector<uint8_t> snapshot;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        snapshot = g_last_checkpoint_rdram;
    }
    if (snapshot.size() != kRdramSize) {
        return R"({"ok":false,"error":"checkpoint RDRAM not captured yet"})";
    }

    std::string hex;
    hex.reserve(static_cast<size_t>(count) * 2);
    char tmp[4];
    for (uint64_t i = 0; i < count; i++) {
        std::snprintf(tmp, sizeof(tmp), "%02x", snapshot[paddr + static_cast<uint32_t>(i)]);
        hex += tmp;
    }
    return R"({"ok":true,"addr":)" + std::to_string(addr) +
           R"(,"paddr":)" + std::to_string(paddr) +
           R"(,"n":)" + std::to_string(count) +
           R"(,"hex":")" + hex + R"("})";
}

std::string checkpoint_rdram_digest_json(uint32_t addr, uint64_t count) {
    if (count < 1) {
        count = 1;
    }
    if (count > 0x40000) {
        count = 0x40000;
    }
    const uint32_t paddr = addr_to_paddr(addr);
    if (paddr + count > kRdramSize) {
        return R"({"ok":false,"error":"oob"})";
    }

    std::vector<uint8_t> snapshot;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        snapshot = g_last_checkpoint_rdram;
    }
    if (snapshot.size() != kRdramSize) {
        return R"({"ok":false,"error":"checkpoint RDRAM not captured yet"})";
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
    for (uint64_t i = 0; i < count; i++) {
        const uint8_t b = snapshot[paddr + static_cast<uint32_t>(i)];
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
                first_nonzero = static_cast<int>(i);
            }
            last_nonzero = static_cast<int>(i);
        }
        fnv ^= b;
        fnv *= 1099511628211ull;
    }

    char hash_buf[32];
    std::snprintf(hash_buf, sizeof(hash_buf), "%016llx", (unsigned long long)fnv);
    return R"({"ok":true,"addr":)" + std::to_string(addr) +
           R"(,"paddr":)" + std::to_string(paddr) +
           R"(,"n":)" + std::to_string(count) +
           R"(,"nonzero_bytes":)" + std::to_string(nonzero) +
           R"(,"unique_bytes":)" + std::to_string(unique) +
           R"(,"first_nonzero":)" + std::to_string(first_nonzero) +
           R"(,"last_nonzero":)" + std::to_string(last_nonzero) +
           R"(,"fnv64":")" + hash_buf +
           R"(","first64":")" + first_hex + R"("})";
}

std::string inject_json(const std::string& field, uint64_t value, uint32_t addr) {
    uint8_t* rdram = recomp_runtime_get_rdram();
    recomp_context* ctx = g_active_ctx.load(std::memory_order_acquire);
    if (field.empty()) {
        return R"({"ok":false,"error":"missing field"})";
    }

    int gpr_index = -1;
    if (parse_gpr_field(field, gpr_index)) {
        if (ctx == nullptr) {
            return R"({"ok":false,"error":"recomp_context not captured yet"})";
        }
        uint64_t* gpr = &ctx->r0;
        gpr[gpr_index] = value;
        char buf[160];
        std::snprintf(buf, sizeof(buf),
            "{\"ok\":true,\"field\":\"r%d\",\"value\":%llu}",
            gpr_index, (unsigned long long)value);
        return std::string(buf);
    }

    if (field == "rdram" || field == "rdram_byte") {
        if (rdram == nullptr) {
            return R"({"ok":false,"error":"rdram not captured yet"})";
        }
        const uint32_t paddr = addr_to_paddr(addr);
        if (paddr >= kRdramSize) {
            return R"({"ok":false,"error":"addr outside 8MB RDRAM"})";
        }
        rdram[paddr ^ 3u] = static_cast<uint8_t>(value);
        char buf[192];
        std::snprintf(buf, sizeof(buf),
            "{\"ok\":true,\"field\":\"rdram_byte\",\"addr\":%u,\"paddr\":%u,\"value\":%u}",
            addr, paddr, (unsigned)(value & 0xFFu));
        return std::string(buf);
    }

    return R"({"ok":false,"error":"unsupported inject field"})";
}

} // namespace pkmnstadium::cosim
