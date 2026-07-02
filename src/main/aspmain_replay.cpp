/*
 * aspmain_replay.cpp — offline replay of a captured aspMain audio task.
 *
 * Companion to the single-task capture in rsp_aspmain_hook.cpp /
 * main.cpp (PSR_ASPMAIN_CAPTURE=<dir>, or the debug-server command
 * `aspmain_capture_arm`). Launch the runner with
 *
 *     PSR_ASPMAIN_REPLAY=<capture_dir>
 *
 * and instead of booting the game it:
 *   1. Restores the captured task's inputs: dmem.bin into the RSP DMEM,
 *      rdram_before.bin into a fresh guest RDRAM, and the RspContext
 *      (ctx_full.bin when present — includes the persistent vector-unit
 *      state the scalar ctx.bin misses; ctx.bin scalar fields otherwise).
 *   2. Runs the recompiled aspMain on that state with the RSP DMA trace
 *      enabled.
 *   3. Writes rdram_replay.bin / dmem_replay.bin / replay_dma.txt next
 *      to the capture, then reports two diffs:
 *        a. DMA-sequence diff vs recomp_dma.txt — first diverging DMA
 *           (direction/dmem/dram/len) localizes at the op level.
 *        b. RDRAM diff vs rdram_after.bin restricted to the byte ranges
 *           the captured task's WR DMAs covered. The in-game snapshots
 *           are taken while OTHER host threads keep mutating RDRAM, so
 *           a whole-window diff is contaminated by unrelated writes;
 *           only the task's own output ranges are meaningful. The
 *           whole-window count is still printed as an FYI.
 *
 * A clean (a)+(b) proves the capture is complete and the task is a pure
 * function of the captured state — making the capture valid input for a
 * ground-truth RSP (Ares LLE / reference interpreter) diff: the first
 * sample where ground truth disagrees with rdram_replay.bin localizes
 * the recompiled-synthesis defect. A dirty (a)/(b) means the capture is
 * missing state — fix the capture before trusting any downstream diff.
 *
 * Exit codes: 0 = clean, 1 = divergence, 2 = usage / IO error.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "librecomp/rsp.hpp"

extern RspUcodeFunc aspMain;
extern uint8_t dmem[];

// Reference RSP interpreter (aspmain_refint.cpp) — the ground-truth
// engine the recompiled run is diffed against.
extern "C" int psr_aspmain_refint_run(uint8_t* rdram,
                                      const RspContext* seed,
                                      const char* imem_path,
                                      const char* dma_txt_path);

// Mirror of librecomp's RSP DMA trace event (rsp.cpp / debug_server.cpp /
// main.cpp all share this shape by convention; there is no exported
// header for it).
struct ReplayRspDmaTraceEvent {
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
    ReplayRspDmaTraceEvent* out, uint32_t out_cap,
    uint32_t* out_count, uint64_t* out_write_index);

namespace {

constexpr size_t kRdramWindow = 0x800000;    // 8 MiB — the captured window
// Guest memory allocation size. Must match recomp::mem_size (1 GiB) so
// the MEM_* macros' RECOMP_MEM_MASK offset decoding matches the in-game
// run (e.g. the SP_STATUS mirror write at 0xA4040010 lands at the same
// masked offset instead of corrupting the diff window).
constexpr size_t kMemSize = 0x40000000;

bool read_file(const std::string& path, std::vector<uint8_t>& out,
               size_t expect_size) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return false; }
    out.resize((size_t)len);
    size_t got = out.empty() ? 0 : fread(out.data(), 1, out.size(), f);
    fclose(f);
    if (got != out.size()) return false;
    if (expect_size != 0 && out.size() != expect_size) {
        fprintf(stderr, "[aspmain_replay] %s: size %zu != expected %zu\n",
                path.c_str(), out.size(), expect_size);
        return false;
    }
    return true;
}

bool write_file(const std::string& path, const uint8_t* data, size_t len) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t put = fwrite(data, 1, len, f);
    fclose(f);
    return put == len;
}

struct DmaRec {
    bool write;
    uint32_t dram, dmem, len;
};

// Parse recomp_dma.txt lines of the form
//   "  0 WR dram=0x123456 dmem=0x1B0 len=0x140 nz=17 first16=..."
// (format produced by aspMain_capture in main.cpp).
bool parse_dma_txt(const std::string& path, std::vector<DmaRec>& out) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        unsigned idx = 0, dram = 0, dmemv = 0, len = 0, nz = 0;
        char dir[3] = {0};
        int n = sscanf(line, " %u %2s dram=0x%X dmem=0x%X len=0x%X nz=%u",
                       &idx, dir, &dram, &dmemv, &len, &nz);
        if (n >= 5) {
            out.push_back({dir[0] == 'W', dram, dmemv, len});
        }
    }
    fclose(f);
    return true;
}

// Captured context, restored into the wrapper's persistent RspContext by
// the pre-task hook below (the thread_local ctx is only reachable there).
struct {
    bool valid_full = false;
    std::vector<uint8_t> full_bytes;      // raw RspContext (same binary)
    uint32_t r[32] = {};                  // scalar fallback: r1..r31
    uint32_t dma_mem = 0;
    uint32_t dma_dram = 0;
} g_cap;

void replay_pre_task_hook(uint8_t* /*rdram*/, RspContext* ctx,
                          const char* /*ucode_name*/,
                          uint32_t /*ucode_addr*/) {
    if (g_cap.valid_full && g_cap.full_bytes.size() == sizeof(RspContext)) {
        memcpy(ctx, g_cap.full_bytes.data(), sizeof(RspContext));
        return;
    }
    // Scalar-only restore (pre-ctx_full captures). The persistent VU
    // state stays default-initialized; a diff mismatch under this path
    // is inconclusive — recapture with a build that dumps ctx_full.bin.
    uint32_t* gpr = &ctx->r1;             // r1..r31 are contiguous
    for (int i = 1; i <= 31; i++) gpr[i - 1] = g_cap.r[i];
    ctx->dma_mem_address = g_cap.dma_mem;
    ctx->dma_dram_address = g_cap.dma_dram;
}

}  // namespace

extern "C" int psr_aspmain_replay_main(const char* dir_c) {
    std::string dir = dir_c;
    fprintf(stderr, "[aspmain_replay] capture dir: %s\n", dir.c_str());

    // The librecomp DMA-trace ring gates on PSR_RSP_DMA_TRACE (latched at
    // first use, which is below). Force it on: the DMA-sequence diff is
    // half the value of the replay.
#ifdef _WIN32
    _putenv_s("PSR_RSP_DMA_TRACE", "1");
#else
    setenv("PSR_RSP_DMA_TRACE", "1", 1);
#endif

    std::vector<uint8_t> dmem_bytes, ctx_bytes, rdram_before, rdram_after;
    if (!read_file(dir + "/dmem.bin", dmem_bytes, 0x1000)) {
        fprintf(stderr, "[aspmain_replay] missing/short dmem.bin\n");
        return 2;
    }
    if (!read_file(dir + "/ctx.bin", ctx_bytes, 34 * 4)) {
        fprintf(stderr, "[aspmain_replay] missing/short ctx.bin\n");
        return 2;
    }
    if (!read_file(dir + "/rdram_before.bin", rdram_before, kRdramWindow)) {
        fprintf(stderr, "[aspmain_replay] missing/short rdram_before.bin\n");
        return 2;
    }
    bool have_after = read_file(dir + "/rdram_after.bin", rdram_after, kRdramWindow);
    g_cap.valid_full = read_file(dir + "/ctx_full.bin", g_cap.full_bytes,
                                 sizeof(RspContext));
    fprintf(stderr, "[aspmain_replay] ctx_full.bin: %s\n",
            g_cap.valid_full ? "present (full VU state restore)"
                             : "ABSENT (scalar-only restore; VU state zeroed)");
    std::vector<DmaRec> cap_dma;
    bool have_dma = parse_dma_txt(dir + "/recomp_dma.txt", cap_dma);
    fprintf(stderr, "[aspmain_replay] recomp_dma.txt: %s (%zu DMAs)\n",
            have_dma ? "parsed" : "ABSENT", cap_dma.size());

    // ctx.bin layout (see aspmain_capture_input): r1..r31, dma_mem,
    // dma_dram, ucode_addr — 34 little-endian words.
    const uint32_t* w = reinterpret_cast<const uint32_t*>(ctx_bytes.data());
    for (int i = 0; i < 31; i++) g_cap.r[i + 1] = w[i];
    g_cap.dma_mem = w[31];
    g_cap.dma_dram = w[32];
    uint32_t ucode_addr = w[33];

    uint8_t* rdram = static_cast<uint8_t*>(calloc(kMemSize, 1));
    if (!rdram) {
        fprintf(stderr, "[aspmain_replay] 1 GiB guest RDRAM alloc failed\n");
        return 2;
    }
    memcpy(rdram, rdram_before.data(), kRdramWindow);
    memcpy(dmem, dmem_bytes.data(), 0x1000);

    recomp::rsp::constants_init();
    recomp::rsp::set_pre_task_hook("aspMain", replay_pre_task_hook);

    uint64_t dma_idx_before = 0;
    recomp_rsp_dma_recent_copy(nullptr, 0, nullptr, &dma_idx_before);

    RspExitReason exit_reason = aspMain(rdram, ucode_addr);
    fprintf(stderr, "[aspmain_replay] aspMain returned %d (%s)\n",
            (int)exit_reason,
            exit_reason == RspExitReason::Broke ? "Broke — clean task end"
                                                : "NOT Broke — abnormal");

    write_file(dir + "/rdram_replay.bin", rdram, kRdramWindow);
    write_file(dir + "/dmem_replay.bin", dmem, 0x1000);

    // Collect this replay's DMA trace and dump it in recomp_dma.txt's
    // format for eyeballing.
    std::vector<DmaRec> rep_dma;
    {
        std::vector<ReplayRspDmaTraceEvent> dbuf(4096);
        uint32_t got = 0; uint64_t widx = 0;
        recomp_rsp_dma_recent_copy(dbuf.data(), (uint32_t)dbuf.size(), &got, &widx);
        FILE* f = fopen((dir + "/replay_dma.txt").c_str(), "w");
        if (f) fprintf(f, "# replay aspMain DMA trace (seq >= %llu)\n",
                       (unsigned long long)dma_idx_before);
        uint32_t shown = 0;
        for (uint32_t i = 0; i < got; i++) {
            const ReplayRspDmaTraceEvent& e = dbuf[i];
            if (e.seq < dma_idx_before) continue;
            rep_dma.push_back({e.write != 0, e.dram_addr & 0xFFFFFFu,
                               e.dmem_addr & 0xFFFu, e.byte_count});
            if (f) {
                fprintf(f, "%3u %s dram=0x%06X dmem=0x%03X len=0x%X nz=%u first16=",
                        shown++, e.write ? "WR" : "RD",
                        e.dram_addr & 0xFFFFFF, e.dmem_addr & 0xFFF,
                        e.byte_count, e.nonzero_bytes);
                for (int b = 0; b < 16; b++) fprintf(f, "%02X", e.first16[b]);
                fprintf(f, "\n");
            }
        }
        if (f) fclose(f);
    }

    int verdict = 0;

    // (a) DMA-sequence diff — op-level localization.
    //
    // Known in-game-only artifact: aspmain_pre_task preloads the first
    // command chunk with the SAME DMA the ucode's L_1120 pump then
    // redundantly repeats (see the r28 seeding rationale in
    // rsp_aspmain_hook.cpp). The replay restores DMEM directly instead
    // of re-running the hook, so the in-game trace carries one extra
    // duplicate entry at the front. Drop it before comparing.
    if (cap_dma.size() >= 2 &&
        cap_dma[0].write == cap_dma[1].write &&
        cap_dma[0].dram == cap_dma[1].dram &&
        cap_dma[0].dmem == cap_dma[1].dmem &&
        cap_dma[0].len == cap_dma[1].len) {
        fprintf(stderr, "[aspmain_replay] dropping in-game hook-preload "
                        "duplicate DMA[0] before sequence compare\n");
        cap_dma.erase(cap_dma.begin());
    }
    if (have_dma) {
        size_t n = cap_dma.size() < rep_dma.size() ? cap_dma.size() : rep_dma.size();
        size_t first_bad = (size_t)-1;
        for (size_t i = 0; i < n; i++) {
            const DmaRec& a = cap_dma[i];
            const DmaRec& b = rep_dma[i];
            if (a.write != b.write || a.dram != b.dram ||
                a.dmem != b.dmem || a.len != b.len) {
                first_bad = i;
                break;
            }
        }
        if (first_bad == (size_t)-1 && cap_dma.size() == rep_dma.size()) {
            fprintf(stderr, "[aspmain_replay] DMA sequence MATCH (%zu DMAs)\n",
                    cap_dma.size());
        } else {
            verdict = 1;
            if (first_bad != (size_t)-1) {
                const DmaRec& a = cap_dma[first_bad];
                const DmaRec& b = rep_dma[first_bad];
                fprintf(stderr,
                        "[aspmain_replay] DMA sequence DIVERGES at index %zu:\n"
                        "  ingame: %s dram=0x%06X dmem=0x%03X len=0x%X\n"
                        "  replay: %s dram=0x%06X dmem=0x%03X len=0x%X\n",
                        first_bad,
                        a.write ? "WR" : "RD", a.dram, a.dmem, a.len,
                        b.write ? "WR" : "RD", b.dram, b.dmem, b.len);
            } else {
                fprintf(stderr,
                        "[aspmain_replay] DMA count differs: ingame=%zu replay=%zu "
                        "(common prefix matches)\n",
                        cap_dma.size(), rep_dma.size());
            }
        }
    }

    if (!have_after) {
        fprintf(stderr, "[aspmain_replay] no rdram_after.bin — skipping RDRAM diff\n");
        free(rdram);
        return verdict;
    }

    // (b) RDRAM diff restricted to the captured task's WR DMA ranges.
    // Compare on word-aligned host ranges (the guest byte order is XOR-3
    // within each 32-bit word, so aligning covers the permutation).
    size_t wr_diff = 0, wr_bytes = 0;
    size_t first_wr_diff = (size_t)-1;
    for (const DmaRec& d : cap_dma) {
        if (!d.write) continue;
        size_t start = d.dram & ~3u;
        size_t end = ((size_t)d.dram + d.len + 3) & ~3u;
        if (end > kRdramWindow) end = kRdramWindow;
        for (size_t i = start; i < end; i++) {
            wr_bytes++;
            if (rdram[i] != rdram_after[i]) {
                wr_diff++;
                if (first_wr_diff == (size_t)-1) first_wr_diff = i;
            }
        }
    }

    // Whole-window diff — informational ONLY. The in-game snapshots race
    // against other host threads mutating RDRAM, so nonzero counts here
    // do NOT indicate a replay problem by themselves.
    size_t win_diff = 0;
    for (size_t i = 0; i < kRdramWindow; i++) {
        if (rdram[i] != rdram_after[i]) win_diff++;
    }

    if (have_dma && wr_bytes > 0) {
        if (wr_diff == 0) {
            fprintf(stderr,
                    "[aspmain_replay] WR-range RDRAM MATCH (%zu bytes across "
                    "%zu WR DMAs; whole-window FYI: %zu differing bytes from "
                    "concurrent non-RSP writes)\n",
                    wr_bytes, cap_dma.size(), win_diff);
        } else {
            verdict = 1;
            fprintf(stderr,
                    "[aspmain_replay] WR-range RDRAM MISMATCH: %zu/%zu bytes "
                    "differ, first at host offset 0x%zX (be addr 0x%zX). "
                    "Whole-window FYI: %zu.\n",
                    wr_diff, wr_bytes, first_wr_diff, first_wr_diff ^ 3, win_diff);
            // Show a little context around the first mismatch.
            size_t base = first_wr_diff & ~7u;
            fprintf(stderr, "  replay @0x%06zX:", base);
            for (size_t b = 0; b < 16 && base + b < kRdramWindow; b++)
                fprintf(stderr, " %02X", rdram[base + b]);
            fprintf(stderr, "\n  ingame @0x%06zX:", base);
            for (size_t b = 0; b < 16 && base + b < kRdramWindow; b++)
                fprintf(stderr, " %02X", rdram_after[base + b]);
            fprintf(stderr, "\n");
        }
    } else {
        fprintf(stderr,
                "[aspmain_replay] no WR DMA ranges to diff (recomp_dma.txt %s); "
                "whole-window differing bytes: %zu (contaminated by concurrent "
                "writes — informational only)\n",
                have_dma ? "had no WR entries" : "absent", win_diff);
    }

    // ── Ground-truth pass: run the reference interpreter on the SAME
    // capture and diff it against the recompiled replay. This is the
    // decisive comparison: the interpreter decodes the ucode binary
    // independently, so any divergence here is an RSPRecomp emission /
    // DMA-model defect; a full match exonerates the recompiled RSP for
    // this task. Enabled by default; skip with PSR_ASPMAIN_NOREF=1. ──
    if (!getenv("PSR_ASPMAIN_NOREF")) {
        const char* imem_path = getenv("PSR_ASPMAIN_IMEM");
        std::string imem_fallback;
        if (!imem_path) {
            FILE* probe = fopen("aspmain_combined.bin", "rb");
            if (probe) { fclose(probe); imem_fallback = "aspmain_combined.bin"; }
            else imem_fallback = "../aspmain_combined.bin";
            imem_path = imem_fallback.c_str();
        }

        uint8_t* ref_rdram = static_cast<uint8_t*>(calloc(kMemSize, 1));
        if (!ref_rdram) {
            fprintf(stderr, "[aspmain_replay] ref RDRAM alloc failed\n");
            free(rdram);
            return 2;
        }
        memcpy(ref_rdram, rdram_before.data(), kRdramWindow);
        // Save the recomp replay's post-DMEM, then restore the captured
        // input DMEM for the reference run (the global array is shared).
        std::vector<uint8_t> dmem_recomp(dmem, dmem + 0x1000);
        memcpy(dmem, dmem_bytes.data(), 0x1000);

        RspContext seed{};
        if (g_cap.valid_full && g_cap.full_bytes.size() == sizeof(RspContext)) {
            memcpy(&seed, g_cap.full_bytes.data(), sizeof(RspContext));
        } else {
            uint32_t* gpr = &seed.r1;
            for (int i = 1; i <= 31; i++) gpr[i - 1] = g_cap.r[i];
            seed.dma_mem_address = g_cap.dma_mem;
            seed.dma_dram_address = g_cap.dma_dram;
        }

        int ref_rc = psr_aspmain_refint_run(ref_rdram, &seed, imem_path,
                                            (dir + "/ref_dma.txt").c_str());
        if (ref_rc != 0) {
            fprintf(stderr, "[aspmain_replay] REF run failed (rc=%d) — no "
                            "ground-truth verdict\n", ref_rc);
            free(ref_rdram);
            free(rdram);
            return verdict ? verdict : 2;
        }
        write_file(dir + "/rdram_ref.bin", ref_rdram, kRdramWindow);
        write_file(dir + "/dmem_ref.bin", dmem, 0x1000);

        // (1) DMA sequence: reference vs recompiled replay.
        std::vector<DmaRec> ref_dma;
        parse_dma_txt(dir + "/ref_dma.txt", ref_dma);
        size_t n = ref_dma.size() < rep_dma.size() ? ref_dma.size() : rep_dma.size();
        size_t dma_bad = (size_t)-1;
        for (size_t i = 0; i < n; i++) {
            const DmaRec& a = ref_dma[i];
            const DmaRec& b = rep_dma[i];
            if (a.write != b.write || a.dram != b.dram ||
                a.dmem != b.dmem || a.len != b.len) { dma_bad = i; break; }
        }
        bool dma_ok = dma_bad == (size_t)-1 && ref_dma.size() == rep_dma.size();

        // (2) RDRAM over the reference's WR ranges (word-aligned).
        size_t wr_diff2 = 0, wr_bytes2 = 0, first2 = (size_t)-1;
        for (const DmaRec& d : ref_dma) {
            if (!d.write) continue;
            size_t start = d.dram & ~3u;
            size_t end = ((size_t)d.dram + d.len + 3) & ~3u;
            if (end > kRdramWindow) end = kRdramWindow;
            for (size_t i = start; i < end; i++) {
                wr_bytes2++;
                if (rdram[i] != ref_rdram[i]) {
                    wr_diff2++;
                    if (first2 == (size_t)-1) first2 = i;
                }
            }
        }

        // (3) Post-run DMEM.
        size_t dmem_diff = 0, dmem_first = (size_t)-1;
        for (size_t i = 0; i < 0x1000; i++) {
            if (dmem[i] != dmem_recomp[i]) {
                dmem_diff++;
                if (dmem_first == (size_t)-1) dmem_first = i;
            }
        }

        if (dma_ok && wr_diff2 == 0 && dmem_diff == 0) {
            fprintf(stderr,
                    "[aspmain_replay] GROUND TRUTH MATCH — reference "
                    "interpreter reproduces the recompiled run exactly "
                    "(%zu DMAs, %zu WR bytes, DMEM identical). The "
                    "recompiled RSP is correct for this task.\n",
                    ref_dma.size(), wr_bytes2);
        } else {
            verdict = 1;
            fprintf(stderr, "[aspmain_replay] GROUND TRUTH DIVERGENCE:\n");
            if (!dma_ok) {
                if (dma_bad != (size_t)-1) {
                    const DmaRec& a = ref_dma[dma_bad];
                    const DmaRec& b = rep_dma[dma_bad];
                    fprintf(stderr,
                            "  DMA[%zu] ref: %s dram=0x%06X dmem=0x%03X len=0x%X\n"
                            "  DMA[%zu] rec: %s dram=0x%06X dmem=0x%03X len=0x%X\n",
                            dma_bad, a.write ? "WR" : "RD", a.dram, a.dmem, a.len,
                            dma_bad, b.write ? "WR" : "RD", b.dram, b.dmem, b.len);
                } else {
                    fprintf(stderr, "  DMA count: ref=%zu rec=%zu\n",
                            ref_dma.size(), rep_dma.size());
                }
            }
            if (wr_diff2) {
                fprintf(stderr,
                        "  WR-range bytes: %zu/%zu differ, first at host "
                        "0x%zX (be addr 0x%zX)\n",
                        wr_diff2, wr_bytes2, first2, first2 ^ 3);
            }
            if (dmem_diff) {
                fprintf(stderr,
                        "  post-DMEM bytes: %zu differ, first at host 0x%zX\n",
                        dmem_diff, dmem_first);
            }
        }
        free(ref_rdram);
    }

    free(rdram);
    return verdict;
}
