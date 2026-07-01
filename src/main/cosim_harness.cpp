#include <algorithm>
#include <atomic>
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

extern uint64_t total_vis;

namespace {

constexpr uint32_t kRdramSize = 8u * 1024u * 1024u;
constexpr size_t kMaxContexts = 64;
constexpr size_t kWindowCap = 1024;

std::atomic<recomp_context*> g_active_ctx{nullptr};
std::mutex g_mu;
std::vector<recomp_context*> g_contexts;
std::vector<recomp::cosim::Checkpoint> g_window;
uint64_t g_cp = 0;
uint64_t g_chain = 0;
uint64_t g_stride = 1;

uint64_t modeled_cycle_now() {
    return 0; // Reserved for T5 modeled guest-cycle clock.
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
    }
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

namespace pkmnstadium::cosim {

std::string chain_json() {
    recomp::cosim::Checkpoint cp{};
    std::string error;
    if (!snapshot(cp, true, error)) {
        return std::string(R"({"ok":false,"error":")") + error + R"("})";
    }
    return std::string(R"({"ok":true,"mode":"t2_live_nonparked",)") +
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

std::string step_json(uint64_t frames) {
    if (frames == 0) {
        frames = 1;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    return std::string(R"({"ok":false,"error":"cosim_step requires T3 quiescent VI parking",)"
                       R"("requested_frames":)") +
           std::to_string(frames) +
           R"(,"stride":)" +
           std::to_string(g_stride) +
           R"(,"mode":"t2_protocol_only"})";
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
    std::lock_guard<std::mutex> lock(g_mu);
    g_cp = 0;
    g_chain = 0;
    g_window.clear();
    return R"({"ok":true,"cp":0,"chain":0})";
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
