#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
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

std::atomic<recomp_context*> g_active_ctx{nullptr};
std::mutex g_mu;
std::mutex g_step_mu;
std::condition_variable g_step_cv;
std::vector<recomp_context*> g_contexts;
std::vector<recomp::cosim::Checkpoint> g_window;
uint64_t g_cp = 0;
uint64_t g_chain = 0;
uint64_t g_stride = 1;
std::atomic<uint64_t> g_published_cp{0};
ultramodern_cosim_quiescence_state g_last_quiescence{};
bool g_parked = false;
bool g_parked_by_callback = false;
uint64_t g_parked_cp = 0;
uint64_t g_release_count = 0;

uint64_t modeled_cycle_now() {
    return 0; // Reserved for T5 modeled guest-cycle clock.
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

std::string checkpoint_json(const recomp::cosim::Checkpoint& cp, bool prefix_comma) {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "%s{\"cp\":%llu,\"vis\":%llu,\"cycle\":%llu,"
        "\"cpu_int\":%llu,\"cp0\":%llu,\"cp1\":%llu,\"rdram\":%llu,\"chain\":%llu}",
        prefix_comma ? "," : "",
        (unsigned long long)cp.cp,
        (unsigned long long)cp.vis,
        (unsigned long long)cp.cycle,
        (unsigned long long)cp.sub.cpu_int,
        (unsigned long long)cp.sub.cp0,
        (unsigned long long)cp.sub.cp1,
        (unsigned long long)cp.sub.rdram,
        (unsigned long long)cp.chain);
    return std::string(buf);
}

bool snapshot(recomp::cosim::Checkpoint& out, bool advance_chain, std::string& error) {
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

    const uint64_t vis = total_vis;
    const uint64_t cycle = modeled_cycle_now();
    const recomp::cosim::SubHashes sub = recomp::cosim::hash_state(ctx, rdram, kRdramSize);

    std::lock_guard<std::mutex> lock(g_mu);
    uint64_t cp_index = g_cp;
    uint64_t chain = g_chain;
    if (advance_chain) {
        chain = recomp::cosim::chain_advance(g_chain, sub, vis, cycle);
        cp_index = ++g_cp;
        g_chain = chain;
    }

    out = recomp::cosim::Checkpoint{
        cp_index,
        vis,
        cycle,
        sub,
        chain,
    };

    if (advance_chain) {
        if (g_window.size() == kWindowCap) {
            g_window.erase(g_window.begin());
        }
        g_window.push_back(out);
        g_published_cp.store(out.cp, std::memory_order_release);
    }
    return true;
}

std::string quiescence_json_fields(const ultramodern_cosim_quiescence_state& q) {
    char buf[320];
    std::snprintf(buf, sizeof(buf),
        "\"vi_queue\":%u,\"running_head\":%u,\"known_threads\":%u,"
        "\"blocked_on_vi\":%u,\"blocked_on_other\":%u,"
        "\"idle_running\":%u,\"runnable_or_unknown\":%u,"
        "\"external_pending\":%u,\"quiescent\":%s",
        q.vi_queue,
        q.running_head,
        q.known_threads,
        q.blocked_on_vi,
        q.blocked_on_other,
        q.idle_running,
        q.runnable_or_unknown,
        q.external_pending,
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
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_last_quiescence = q;
    }

    recomp::cosim::Checkpoint cp{};
    std::string error;
    if (!snapshot(cp, true, error)) {
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
        g_active_ctx.store(static_cast<recomp_context*>(ctx), std::memory_order_release);
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

namespace pkmnstadium::cosim {

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
        "{\"ok\":true,\"mode\":\"t2_live_nonparked\",\"cp\":%llu,\"vis\":%llu,\"cycle\":%llu,"
        "\"cpu_int\":%llu,\"cp0\":%llu,\"cp1\":%llu,\"rdram\":%llu}",
        (unsigned long long)cp.cp,
        (unsigned long long)cp.vis,
        (unsigned long long)cp.cycle,
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

        ultramodern_cosim_request_vi(1);
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
                g_step_cv.notify_all();
            } else {
                g_parked = false;
                g_parked_cp = 0;
                request_vi = true;
            }
        }

        if (request_vi) {
            ultramodern_cosim_request_vi(1);
        }

        while (g_published_cp.load(std::memory_order_acquire) <= before) {
            {
                std::lock_guard<std::mutex> lock(g_step_mu);
                if (g_parked && g_published_cp.load(std::memory_order_acquire) > before) {
                    break;
                }
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
        out += checkpoint_json(g_window[start + i], i != 0);
    }
    out += "]}";
    return out;
}

std::string reset_json() {
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_cp = 0;
        g_chain = 0;
        g_window.clear();
        g_last_quiescence = {};
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
        out += "}";
    }

    out += "]}";
    return out;
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
