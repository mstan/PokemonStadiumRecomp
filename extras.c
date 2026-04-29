/*
 * extras.c — game-specific runtime hooks for PokemonStadiumRecomp.
 *
 * Hosts the recomp_unhandled_* entry points that the engine emits
 * for instructions it can't translate at compile time. Per project
 * principles (PRINCIPLES.md #12): NOT stubs. These are real
 * implementations that abort loudly with full diagnostic context
 * if reached. They are the runtime side of the "tolerant emit
 * mode" engine commit (N64Recomp 512191b).
 *
 * Policy here = "log + abort with diagnostics." Future work could
 * escalate to e.g. an in-process emulator handler instead of
 * aborting, but for first boot we want loud failures.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Forward-declare recomp_context — librecomp defines the real one. */
struct recomp_context;
typedef struct recomp_context recomp_context;

/* Diagnostic: log r2/r24/r19 just before func_86B0027C's AV-causing
 * `lw $a3, 0x0($t8)` at 0x86B002C0. Also dump 0x18 bytes from r2
 * (the struct base) to see whether Stadium's R_MIPS_32 relocations
 * were applied to it. Original link-time bytes at fragment78+0x24E1C:
 *   +0x00: 0x00090001
 *   +0x04: 0x0D060000
 *   +0x08: 0x8FF24DC0   ← should become runtime_base+0x24DC0
 *   +0x0C: 0x8FF24DC8   ← should become runtime_base+0x24DC8
 *   +0x10: 0x8FF24E00   ← should become runtime_base+0x24E00
 *   +0x14: 0x8FF00020   ← should become runtime_base+0x20
 * If we see 0x8FFxxxxx values, the relocation never ran.
 * If we see 0 / garbage, something else corrupted the data. */
/* Diagnostic: log process_geo_layout's entry args (pool, segptr) and
 * the post-Util_ConvertAddrToVirtAddr command pointer + first 8 bytes
 * there. The point is to see whether the second crash-probe iteration
 * gets an empty/sentinel layout and exits the dispatch loop immediately. */
void psr_log_geo_entry(uint64_t pool, uint64_t segptr) {
    fprintf(stderr,
        "[geo-entry] pool=0x%016llX segptr=0x%016llX\n",
        (unsigned long long)pool,
        (unsigned long long)segptr);
    fflush(stderr);
}

void psr_log_geo_command(uint8_t *rdram, uint64_t converted_addr) {
    uint32_t v = (uint32_t)converted_addr;
    fprintf(stderr,
        "[geo-cmd]   converted_addr=0x%016llX",
        (unsigned long long)converted_addr);
    if (v == 0) {
        fprintf(stderr, " (NULL — loop will exit immediately)\n");
    } else {
        uint32_t paddr = v & 0x1FFFFFFFu;
        fprintf(stderr, " bytes:");
        for (uint32_t off = 0; off < 8; off++) {
            uint32_t b = rdram[(paddr + off) ^ 3];
            fprintf(stderr, " %02X", b);
        }
        fprintf(stderr, "\n");
    }
    fflush(stderr);
}

void psr_log_crash_regs(uint8_t *rdram, uint64_t r2, uint64_t r24, uint64_t r19) {
    uint32_t r2_v = (uint32_t)r2;
    uint32_t paddr = r2_v & 0x1FFFFFFFu;
    /* Find which fragment instance r2 lives in — peek at the FRAGMENT
     * magic that should be at fragment_base+0x08. r2 = base + 0x24E1C
     * (fragment78's struct offset). Try base = r2 - 0x24E1C first
     * and confirm. If that doesn't have FRAG magic, scan backward in
     * 4-byte steps over a 64KB window. */
    /* Try base = r2 - 0x24E1C (fragment78's struct offset) first, then
     * scan backward in 4-byte steps over 64KB. */
    {
        int found = 0;
        uint32_t cand = r2_v - 0x24E1Cu;
        for (int i = 0; !found && i < 0x4000; i++) {
            if (cand >= 0x80000000u && (cand & 0x1FFFFFFFu) + 0x20 <= 0x800000u) {
                uint32_t bp = cand & 0x1FFFFFFFu;
                if (rdram[(bp + 0x08) ^ 3] == 'F' &&
                    rdram[(bp + 0x09) ^ 3] == 'R' &&
                    rdram[(bp + 0x0A) ^ 3] == 'A' &&
                    rdram[(bp + 0x0B) ^ 3] == 'G') {
                    fprintf(stderr,
                        "[crash-probe] enclosing FRAG header at 0x%08X (r2-0x%X) "
                        "+0x14(relocOffset)=0x%02X%02X%02X%02X "
                        "+0x1C(sizeInRam)=0x%02X%02X%02X%02X\n",
                        cand, r2_v - cand,
                        rdram[(bp + 0x14) ^ 3], rdram[(bp + 0x15) ^ 3],
                        rdram[(bp + 0x16) ^ 3], rdram[(bp + 0x17) ^ 3],
                        rdram[(bp + 0x1C) ^ 3], rdram[(bp + 0x1D) ^ 3],
                        rdram[(bp + 0x1E) ^ 3], rdram[(bp + 0x1F) ^ 3]);
                    found = 1;
                    break;
                }
            }
            if (i == 0) {
                cand = (r2_v & ~3u) - 4u;  /* start the scan-backward sweep */
            } else {
                cand -= 4u;
            }
        }
        if (!found) {
            fprintf(stderr,
                "[crash-probe] no FRAG header found in 64KB before r2=0x%08X\n",
                r2_v);
        }
    }
    fprintf(stderr,
        "[crash-probe] r2=0x%016llX r24=0x%016llX r19=0x%016llX struct@r2:",
        (unsigned long long)r2,
        (unsigned long long)r24,
        (unsigned long long)r19);
    for (uint32_t off = 0; off < 0x18; off += 4) {
        uint32_t b0 = rdram[(paddr + off + 0) ^ 3];
        uint32_t b1 = rdram[(paddr + off + 1) ^ 3];
        uint32_t b2 = rdram[(paddr + off + 2) ^ 3];
        uint32_t b3 = rdram[(paddr + off + 3) ^ 3];
        uint32_t w = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
        fprintf(stderr, " +%u=0x%08X", off, w);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
}


static void unhandled_abort(const char *kind, uint32_t pc, const char *detail) {
    /* Persistent-file copy: stderr in headless runs gets buffered and
     * the abort() can wipe it before the parent process reads. The
     * file is the source of truth for post-mortem inspection. */
    FILE *f = fopen("F:/Projects/PokemonStadiumRecomp/build/last_error.log", "a");
    if (f) {
        fprintf(f,
            "\n=== N64Recomp UNHANDLED %s ===\n"
            "  PC:     0x%08X\n"
            "  Detail: %s\n",
            kind, pc, detail);
        fclose(f);
    }
    fprintf(stderr,
        "\n=== N64Recomp UNHANDLED %s ===\n"
        "  PC:     0x%08X\n"
        "  Detail: %s\n"
        "Engine could not translate this code path. Aborting.\n",
        kind, pc, detail);
    fflush(stderr);
    abort();
}

void recomp_unhandled_branch(uint8_t *rdram, recomp_context *ctx,
                             uint32_t instr_vram, uint32_t branch_target) {
    char detail[64];
    snprintf(detail, sizeof(detail), "branch target 0x%08X (no symbol)", branch_target);
    unhandled_abort("BRANCH", instr_vram, detail);
}

void recomp_unhandled_call(uint8_t *rdram, recomp_context *ctx,
                           uint32_t instr_vram, uint32_t target) {
    char detail[64];
    snprintf(detail, sizeof(detail), "jal target 0x%08X (no symbol)", target);
    unhandled_abort("CALL", instr_vram, detail);
}

void recomp_unhandled_jalr(uint8_t *rdram, recomp_context *ctx,
                           uint32_t instr_vram, uint64_t target_value, int rd) {
    char detail[96];
    snprintf(detail, sizeof(detail),
        "jalr with non-RA link (rd=r%d, runtime target=0x%016llX)",
        rd, (unsigned long long)target_value);
    unhandled_abort("JALR", instr_vram, detail);
}

uint64_t recomp_unhandled_cop0_read(uint8_t *rdram, recomp_context *ctx,
                                    uint32_t instr_vram, int cop0_reg) {
    char detail[64];
    snprintf(detail, sizeof(detail), "mfc0 from cop0 register %d (not modeled)", cop0_reg);
    unhandled_abort("COP0_READ", instr_vram, detail);
    return 0;  /* unreachable */
}

void recomp_unhandled_cop0_write(uint8_t *rdram, recomp_context *ctx,
                                 uint32_t instr_vram, int cop0_reg, uint64_t value) {
    char detail[96];
    snprintf(detail, sizeof(detail),
        "mtc0 to cop0 register %d = 0x%016llX (not modeled)",
        cop0_reg, (unsigned long long)value);
    unhandled_abort("COP0_WRITE", instr_vram, detail);
}

void recomp_unhandled_instruction(uint8_t *rdram, recomp_context *ctx,
                                  uint32_t instr_vram, const char *opcode_name) {
    unhandled_abort("INSTRUCTION", instr_vram, opcode_name);
}

/* ------------------------------------------------------------------ */
/* libultra functions librecomp doesn't reimplement.                  */
/*                                                                    */
/* Pokemon Stadium pulls in libultra functions for: CPU control       */
/* registers (SR/Cause/Count), 64DD/Leo disk drive, controller pak    */
/* I/O, and EPI device I/O. librecomp provides 138 _recomp libultra   */
/* shims but not these. Per project principles (PRINCIPLES.md #12)    */
/* these are NOT stubs — they abort loudly with full context if       */
/* reached, surfacing the missing implementation as a clearly-named   */
/* runtime failure rather than silent wrong behavior.                 */
/*                                                                    */
/* Real implementations would either model the libultra behavior      */
/* against context state, or escalate to an in-process emulator       */
/* handler. For first boot we want loud failure to identify what      */
/* code paths actually need real impls.                               */
/* ------------------------------------------------------------------ */

#define UNIMPL_LIBULTRA(name)                                                  \
    void name##_recomp(uint8_t *rdram, recomp_context *ctx) {                  \
        FILE *f = fopen("F:/Projects/PokemonStadiumRecomp/build/last_error.log", "a"); \
        if (f) {                                                               \
            fprintf(f, "\n=== UNIMPLEMENTED LIBULTRA: %s ===\n", #name);       \
            fclose(f);                                                         \
        }                                                                      \
        fprintf(stderr,                                                        \
            "\n=== N64Recomp UNIMPLEMENTED LIBULTRA: %s ===\n"                 \
            "  librecomp does not provide a _recomp implementation.\n"         \
            "  Add a real model in extras.c or librecomp.\n",                  \
            #name);                                                            \
        fflush(stderr);                                                        \
        abort();                                                               \
    }

// osEPiWriteIo is implemented in librecomp/src/pi.cpp.
// osPfsIsPlug is implemented in librecomp/src/pak.cpp.
UNIMPL_LIBULTRA(__osContRamWrite)
UNIMPL_LIBULTRA(__osContRamRead)
UNIMPL_LIBULTRA(osLeoDiskInit)
UNIMPL_LIBULTRA(__osSetSR)
UNIMPL_LIBULTRA(__osSetCause)
UNIMPL_LIBULTRA(__osSetCount)
UNIMPL_LIBULTRA(__osPfsGetStatus)
UNIMPL_LIBULTRA(__osSetCompare)
UNIMPL_LIBULTRA(osDpGetCounters)
UNIMPL_LIBULTRA(rmonPrintf)

/* ------------------------------------------------------------------ */
/* TRACE_ENTRY / TRACE_RETURN ring (powered by game.toml             */
/* trace_mode = true). See include/trace.h.                          */
/* ------------------------------------------------------------------ */

#define TRACE_RING_CAP 4096   /* power of two — wraps cheaply */

static const char* trace_ring[TRACE_RING_CAP];
static volatile uint64_t trace_ring_write_idx = 0;  /* monotonic */

/* Non-overwriting counter for a hard-coded set of "interesting" sched
 * functions, since the 4096-entry trace ring is overrun by audio in
 * <2 seconds and we lose history of one-off sched events.
 *
 * Each entry stores: function pointer (literal string) + atomic count.
 * Lookup is identity comparison on the const char* — safe because the
 * recompiler emits the same string literal for every call site of a
 * given function, and string literals are deduplicated by the linker.
 *
 * Stderr-prints the FIRST entry per function, then keeps counting
 * silently (avoids flooding stderr while still surfacing rare events).
 */
#define INTERESTING_FN_COUNT 12
static const char* const k_interesting_fns[INTERESTING_FN_COUNT] = {
    "func_80005084",  /* SP DONE handler (case 0x64) */
    "func_80005148",  /* RDP DONE handler (case 0x65) */
    "func_80004B0C",  /* SP DONE inner (calls task->queue) */
    "func_80004C68",  /* RDP DONE inner (sets unk_1E=2, posts task->queue) */
    "func_80004E94",  /* case 0x67 task-launch trigger */
    "func_80004F08",  /* case 0x68 PreNMI handler */
    "func_80004F70",  /* case 0x66 VI handler */
    "func_80004E60",  /* task launcher wrapper */
    "func_800049D4",  /* osSpTaskLoad+StartGo */
    "func_80005194",  /* sched main loop entry */
    "func_8000183C",  /* gfx_sched main loop (thread 5) */
    "func_8002B330",  /* Game_Thread main */
};
static volatile uint64_t k_interesting_counts[INTERESTING_FN_COUNT];

void pkmnstadium_trace_entry(const char *func) {
    uint64_t idx = __atomic_fetch_add(&trace_ring_write_idx, 1, __ATOMIC_RELAXED);
    trace_ring[idx & (TRACE_RING_CAP - 1)] = func;

    /* Identity-compare against the interesting list. Cheap: 12 pointer
     * compares per recompiled function entry. The linker dedupes the
     * string literals so this is correct and fast. */
    for (int i = 0; i < INTERESTING_FN_COUNT; i++) {
        if (func == k_interesting_fns[i]) {
            __atomic_fetch_add(&k_interesting_counts[i], 1, __ATOMIC_RELAXED);
            break;
        }
    }
}

/* Public accessor used by debug_server's "interesting_fns" command. */
uint64_t pkmnstadium_interesting_fn_count(int idx) {
    if (idx < 0 || idx >= INTERESTING_FN_COUNT) return 0;
    return __atomic_load_n(&k_interesting_counts[idx], __ATOMIC_RELAXED);
}
const char* pkmnstadium_interesting_fn_name(int idx) {
    if (idx < 0 || idx >= INTERESTING_FN_COUNT) return NULL;
    return k_interesting_fns[idx];
}
int pkmnstadium_interesting_fn_total(void) { return INTERESTING_FN_COUNT; }

void pkmnstadium_trace_return(const char *func) {
    /* For "where are we stuck?" the entry log is what matters; returns
     * are recorded too in case we need to reconstruct a call stack. */
    uint64_t idx = __atomic_fetch_add(&trace_ring_write_idx, 1, __ATOMIC_RELAXED);
    trace_ring[idx & (TRACE_RING_CAP - 1)] = func;  /* same slot semantics */
}

/* Public queries used by debug_server.cpp. Returning const char* from a
 * shared ring is racy with concurrent writers, but for diagnostic
 * sampling we accept the small chance of a partial slot. */

uint64_t pkmnstadium_trace_write_idx(void) {
    return __atomic_load_n(&trace_ring_write_idx, __ATOMIC_RELAXED);
}

const char* pkmnstadium_trace_at(uint64_t idx) {
    return trace_ring[idx & (TRACE_RING_CAP - 1)];
}

uint32_t pkmnstadium_trace_capacity(void) {
    return TRACE_RING_CAP;
}
