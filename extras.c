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
#define INTERESTING_FN_COUNT 48
static const char* const k_interesting_fns[INTERESTING_FN_COUNT] = {
    "func_80005084",        /* SP DONE handler (case 0x64) */
    "func_80005148",        /* RDP DONE handler (case 0x65) */
    "func_80004B0C",        /* SP DONE inner (calls task->queue) */
    "func_80004C68",        /* RDP DONE inner (sets unk_1E=2, posts task->queue) */
    "func_80004E94",        /* case 0x67 task-launch trigger */
    "func_80004F08",        /* case 0x68 PreNMI handler */
    "func_80004F70",        /* case 0x66 VI handler */
    "func_80004E60",        /* task launcher wrapper */
    "func_800049D4",        /* osSpTaskLoad+StartGo */
    "func_80005194",        /* sched main loop entry */
    "func_8000183C",        /* gfx_sched main loop (thread 5) */
    "Game_Thread",          /* Game_Thread main (named symbol) */
    "fragment17_entry",     /* intro fragment entry */
    "fragment36_entry",     /* title-screen fragment entry */
    "func_800290E4",        /* FRAGMENT_LOAD_AND_CALL caller for intro */
    "func_80029310",        /* runs intro then sets state=TITLE_SCREEN */
    "func_800293CC",        /* TITLE_SCREEN handler — calls fragment36 */
    "Cont_ReadInputs",      /* controller poll consumer */
    "func_821009B4",        /* fragment36 main loop */
    "func_82100054",        /* fragment36 exit-state computer */
    "func_82100AB8",        /* fragment36 setup */
    "func_82100C98",        /* fragment36 main entry (called by func_800293CC) */
    "fragment37_entry",     /* AREA_SELECT fragment entry */
    "func_80029828",        /* STATE_AREA_SELECT handler */
    "func_8003AD58",        /* audio synth voice-process */
    "func_8003C204",        /* offset->ptr lazy-resolver in libnaudio */
    "func_8003979C",        /* candidate wave-bank loader */
    "func_80037340",        /* audio init wrapper */
    "func_80038B68",        /* audio config setup */
    "n_alAudioFrame",       /* per-frame synth */
    "func_80039D58",        /* sound-bank parser path A (sets unk_08C/090) */
    "func_80039F28",        /* sound-bank parser path B (sets unk_090) */
    "func_8003A10C",        /* sound-bank parser path C (sets unk_090) */
    "func_8003BD2C",        /* SoundBank loader (resolves wave_list, basenote, detune) */
    "func_8003B214",        /* called inside func_8003AD58 right before unk_2C read */
    "func_8003B2B4",        /* called inside func_8003AD58 (env update) */
    "n_alInit",             /* libnaudio init (vram 0x80042920) */
    "n_alSynNew",           /* libnaudio synth init (vram 0x800491C0) */
    "func_800069F0",        /* per-frame fade ticker — increments unk_13, transitions unk_11 */
    "func_80006FE8",        /* gfx setup that calls func_800069F0 */
    "func_80007778",        /* gfx submit / vsync wait inside func_821005EC */
    "func_821005EC",        /* fragment36 per-frame work — called from fade wait loop */
    "func_82100B1C",        /* fragment36 fade-out wait */
    "func_800076C0",        /* fragment36 cleanup callee */
    "func_8000D2B4",        /* fragment36 cleanup callee */
    "func_8004FF20",        /* fragment36 cleanup callee */
    "func_80005EAC",        /* fragment36 cleanup callee */
    "main_pool_pop_state",  /* fragment36 last cleanup */
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

/* ------------------------------------------------------------------ */
/* func_80019D18 lookup-or-allocate diagnostic.                       */
/*                                                                    */
/* Stadium's func_80019D18(idx) does table lookup against the         */
/* container at *(0x800AC814) and returns NULL when func_8000484C     */
/* fails (idx >= count, or downstream func_800047C4 returns 0).       */
/* The menu-load crash at func_86B0027C dereferences this NULL.       */
/*                                                                    */
/* These hooks are inserted via game.toml [[patches.hook]] entries    */
/* — entry hook saves the input idx, exit hook logs (idx, v0) when    */
/* v0 == 0. Lets us see exactly which idx Stadium asked for.          */
/* ------------------------------------------------------------------ */

static __thread uint32_t s_lookup_a0_stack[16];
static __thread int s_lookup_a0_sp = 0;

void pkmnstadium_lookup_enter(uint32_t a0) {
    if (s_lookup_a0_sp < 16) {
        s_lookup_a0_stack[s_lookup_a0_sp] = a0;
    }
    s_lookup_a0_sp++;
}

void pkmnstadium_lookup_exit(uint32_t v0) {
    s_lookup_a0_sp--;
    if (s_lookup_a0_sp < 0 || s_lookup_a0_sp >= 16) return;
    uint32_t a0 = s_lookup_a0_stack[s_lookup_a0_sp];
    if (v0 == 0) {
        fprintf(stderr,
            "[lookup] MISS func_80019D18 a0=0x%X (=%u, idx_minus_1=0x%X)\n",
            a0, a0, a0 - 1);
        fflush(stderr);
    }
}

/* func_80003DC4(rom_start, rom_end, 0, 0) — the PERS-SZP wrapper
 * loader. Logs every call's args + first 8 bytes of the destination
 * (the wrapper magic) so we can correlate which entries actually
 * land valid data and which return NULL. */
static __thread uint32_t s_pers_a0_stack[16];
static __thread uint32_t s_pers_a1_stack[16];
static __thread int s_pers_sp = 0;

void pkmnstadium_pers_enter(uint32_t a0, uint32_t a1) {
    if (s_pers_sp < 16) {
        s_pers_a0_stack[s_pers_sp] = a0;
        s_pers_a1_stack[s_pers_sp] = a1;
    }
    s_pers_sp++;
}

/* Pre-Util_InitMainPools hook: activate Stadium's expansion-pak
 * code path so it allocates the 6 MiB main_pool instead of the
 * 4 MiB one. See game.toml's hook on Util_InitMainPools entry.
 *
 * Why this is needed:
 *   Util_InitMainPools chooses POOL_END_4MB unless BOTH
 *   gExpansionRAMStart > 0 AND osMemSize > 0x600000. Our runtime
 *   correctly reports osMemSize = 8 MiB, but `gExpansionRAMStart`
 *   is Stadium-specific (in BSS, init to 0) and has NO writer
 *   in the recompiled binary — the 6 MiB path is unreachable
 *   dead code on a freshly-booted Stadium image. Real hardware
 *   doesn't hit this because Stadium's working set fits in the
 *   4 MiB pool there; in our runtime, marginally larger
 *   allocations from extra recompiler overhead push us past
 *   the 3-MiB-usable cap and the title-screen pokemon_models
 *   loader hits an alloc failure.
 *
 * `gExpansionRAMStart` is at 0x80068B90. We set it to 1 BEFORE
 * Util_InitMainPools' first instruction reads it — that takes
 * Stadium down the 6 MiB code path that the original devs wrote
 * but evidently never tested.
 *
 * This is Stadium-specific by symbol name, but the pattern
 * applies to many N64 games: an expansion-pak global that
 * gates a larger memory region is sometimes left zeroed because
 * the original developer relied on a side effect that doesn't
 * carry over to the recomp environment.
 */
void pkmnstadium_force_expansion_ram(uint8_t* rdram) {
    /* gExpansionRAMStart at kseg0 0x80068B90 → physical 0x00068B90.
     * MEM_LOAD_BE32 / store via XOR-3 byte order. */
    uint32_t paddr = 0x00068B90u;
    static int s_logged = 0;
    if (!s_logged) {
        s_logged = 1;
        rdram[(paddr + 0) ^ 3] = 0;
        rdram[(paddr + 1) ^ 3] = 0;
        rdram[(paddr + 2) ^ 3] = 0;
        rdram[(paddr + 3) ^ 3] = 1;
        fprintf(stderr,
            "[pool] forced gExpansionRAMStart=1 to activate "
            "POOL_END_6MB path in Util_InitMainPools\n");
        fflush(stderr);
    }
}

/* main_pool_alloc(size, side) diagnostic. Logs every call's
 * (size, result) plus the running sMemPool.available so we can
 * see when/why the pool exhausts. sMemPool is the Stadium static
 * MainPool struct at 0x800A6070, with available field at +0x1C. */

#define MEM_LOAD_BE32(rdram, paddr) (\
    ((uint32_t)(rdram)[((paddr) + 0) ^ 3] << 24) | \
    ((uint32_t)(rdram)[((paddr) + 1) ^ 3] << 16) | \
    ((uint32_t)(rdram)[((paddr) + 2) ^ 3] <<  8) | \
    ((uint32_t)(rdram)[((paddr) + 3) ^ 3]))

static __thread uint32_t s_pool_size_stack[16];
static __thread int s_pool_sp = 0;

void pkmnstadium_pool_alloc_enter(uint32_t size) {
    if (s_pool_sp < 16) s_pool_size_stack[s_pool_sp] = size;
    s_pool_sp++;
}

/* Audio synth voice-table read regression detector.
 *
 * Originally added to chase a NULL-deref crash in func_8003AD58
 * (Stadium's per-voice synth). Diagnosed 2026-04-29 to a
 * use-after-free: fragment36 loaded its SoundBank into a 1 MiB
 * main_pool buffer and the fade-out wait was being bypassed, so
 * main_pool was popped while the synth still held voice pointers
 * into the freed bank.
 *
 * Fixed by letting func_82100B1C run naturally (the fade ticks
 * voices to silence before pool-pop). This hook is kept as a
 * regression detector — if a future change reintroduces the bug,
 * SUSPECT log lines and a struct dump fire so we can re-diagnose
 * fast. */
void pkmnstadium_audio_diag(uint8_t* rdram, uint32_t s0_vaddr,
                             uint32_t s1_vaddr,
                             uint32_t unk_2C_field, uint32_t unk_28_field,
                             uint16_t v1_index)
{
    uint32_t target = unk_2C_field + (uint32_t)v1_index * 4;
    int suspect = (target < 0x80000000u) || (target >= 0x80800000u) || (unk_2C_field == 0);
    static __thread int s_n_logged = 0;
    if (!suspect && s_n_logged > 5) return;
    s_n_logged++;
    fprintf(stderr,
        "[audio-crash] %s s0(unk_090)=0x%08X unk_28=0x%08X unk_2C=0x%08X "
        "idx=%u target=0x%08X\n",
        suspect ? "*** SUSPECT ***" : "ok",
        s0_vaddr, unk_28_field, unk_2C_field,
        (unsigned)v1_index, target);
    if (suspect) {
        /* Dump 0x40 bytes of the struct + 0x40 of s1 (the parent
         * unk_D_800FC7D0). XOR-3 byte-order to print BE-as-bytes. */
        uint32_t paddr = s0_vaddr & 0x1FFFFFFFu;
        if (paddr + 0x40 < (1u << 30)) {
            fprintf(stderr, "  [s0 struct dump @ 0x%08X]:", s0_vaddr);
            for (int i = 0; i < 0x40; i++) {
                if ((i & 0xF) == 0) fprintf(stderr, "\n    +0x%02X:", i);
                fprintf(stderr, " %02X", rdram[(paddr + i) ^ 3]);
            }
            fprintf(stderr, "\n");
        }
        uint32_t s1_paddr = s1_vaddr & 0x1FFFFFFFu;
        if (s1_paddr + 0x40 < (1u << 30)) {
            fprintf(stderr, "  [s1 struct dump @ 0x%08X +0x80..0xC0]:", s1_vaddr);
            for (int i = 0x80; i < 0xC0; i++) {
                if ((i & 0xF) == 0) fprintf(stderr, "\n    +0x%02X:", i);
                fprintf(stderr, " %02X", rdram[(s1_paddr + i) ^ 3]);
            }
            fprintf(stderr, "\n");
        }
        fflush(stderr);
    }
}

/* func_8003C204 lazy-resolver argument log.
 *
 * Signature: void func_8003C204(u8* arr, u32 base, u32 count) — adds
 * `base` to each non-zero entry of `arr[0..count]`. This is libnaudio's
 * offset->absolute-pointer fixup. In disasm/src/38BB0.c we see it
 * called for soundbank fields unk_04/unk_08/unk_0C, but not for
 * unk_28/unk_2C — which are exactly the fields func_8003AD58 derefs
 * and crashes on.
 *
 * Phase 1 question: do we ever see func_8003C204 called against the
 * struct that ends up as arg0->unk_090 of the synth voice? If yes,
 * our hypothesis (unk_2C never resolved) is wrong. If no, the
 * hypothesis stands and the next question is "where SHOULD it be
 * called and why isn't it."
 *
 * Capture the first N invocations' (arr, base, count). Triplet is
 * cheap; first 64 calls fits easily. */
#define RESOLVER_LOG_CAP 64
static volatile int s_resolver_n = 0;
typedef struct { uint32_t arr; uint32_t base; uint32_t count; } resolver_log_entry;
static resolver_log_entry s_resolver_log[RESOLVER_LOG_CAP];

void pkmnstadium_resolver_log(uint32_t arr, uint32_t base, uint32_t count) {
    int idx = __atomic_fetch_add(&s_resolver_n, 1, __ATOMIC_RELAXED);
    if (idx < RESOLVER_LOG_CAP) {
        s_resolver_log[idx].arr = arr;
        s_resolver_log[idx].base = base;
        s_resolver_log[idx].count = count;
    }
}

/* Public accessors (for debug_server "resolver_log" cmd). */
int pkmnstadium_resolver_log_total(void) {
    int n = __atomic_load_n(&s_resolver_n, __ATOMIC_RELAXED);
    return n < RESOLVER_LOG_CAP ? n : RESOLVER_LOG_CAP;
}
void pkmnstadium_resolver_log_get(int idx, uint32_t* arr, uint32_t* base, uint32_t* count) {
    if (idx < 0 || idx >= RESOLVER_LOG_CAP) {
        if (arr)   *arr = 0;
        if (base)  *base = 0;
        if (count) *count = 0;
        return;
    }
    if (arr)   *arr   = s_resolver_log[idx].arr;
    if (base)  *base  = s_resolver_log[idx].base;
    if (count) *count = s_resolver_log[idx].count;
}

/* func_8003BD2C SoundBank-loader entry snapshot.
 *
 * Dumps the first 0x40 bytes of the struct at arg0 BEFORE the loader
 * runs. Combined with the post-resolve snapshot from
 * pkmnstadium_audio_diag (taken at synth read time), tells us whether
 * the bank was valid at load-time and got overwritten between then
 * and synth read, OR whether the load-time data was already garbage.
 *
 * No behavior change. Just records the snapshot keyed by sb_addr;
 * indexed lookup at synth-read time would let us diff. For Phase 1
 * we just stderr-log so the diff is eyeballed. */
#define SB_SNAPSHOT_CAP 16
typedef struct {
    uint32_t addr;
    uint8_t  bytes[0x40];
} sb_snapshot;
static volatile int s_sb_n = 0;
static sb_snapshot s_sb_snapshots[SB_SNAPSHOT_CAP];

void pkmnstadium_sb_loader_enter(uint8_t* rdram, uint32_t sb_vaddr) {
    int idx = __atomic_fetch_add(&s_sb_n, 1, __ATOMIC_RELAXED);
    if (idx >= SB_SNAPSHOT_CAP) return;
    s_sb_snapshots[idx].addr = sb_vaddr;
    uint32_t paddr = sb_vaddr & 0x1FFFFFFFu;
    if (paddr + 0x40 >= (1u << 30)) return;
    for (int i = 0; i < 0x40; i++) {
        s_sb_snapshots[idx].bytes[i] = rdram[(paddr + i) ^ 3];
    }
    /* Print one-line summary: first 8 bytes + content at offsets 0x24/0x28/0x2C. */
    fprintf(stderr,
        "[sb-load] arg0=0x%08X first8: %02X%02X%02X%02X %02X%02X%02X%02X "
        "off24: %02X%02X%02X%02X off28: %02X%02X%02X%02X off2C: %02X%02X%02X%02X\n",
        sb_vaddr,
        s_sb_snapshots[idx].bytes[0], s_sb_snapshots[idx].bytes[1],
        s_sb_snapshots[idx].bytes[2], s_sb_snapshots[idx].bytes[3],
        s_sb_snapshots[idx].bytes[4], s_sb_snapshots[idx].bytes[5],
        s_sb_snapshots[idx].bytes[6], s_sb_snapshots[idx].bytes[7],
        s_sb_snapshots[idx].bytes[0x24], s_sb_snapshots[idx].bytes[0x25],
        s_sb_snapshots[idx].bytes[0x26], s_sb_snapshots[idx].bytes[0x27],
        s_sb_snapshots[idx].bytes[0x28], s_sb_snapshots[idx].bytes[0x29],
        s_sb_snapshots[idx].bytes[0x2A], s_sb_snapshots[idx].bytes[0x2B],
        s_sb_snapshots[idx].bytes[0x2C], s_sb_snapshots[idx].bytes[0x2D],
        s_sb_snapshots[idx].bytes[0x2E], s_sb_snapshots[idx].bytes[0x2F]);
    fflush(stderr);
}

/* Game_DoCopyProtection diagnostic. Returns -0x10 (= 0xFFFFFFF0)
 * if the copy-protection check trips. We see 0xFFFFFFF0 stored at
 * what might be gLastGameState; this hook tells us definitively
 * whether the function is responsible. */
static __thread uint32_t s_copyprot_state_stack[16];
static __thread int s_copyprot_sp = 0;
void pkmnstadium_copyprot_enter(uint32_t state) {
    if (s_copyprot_sp < 16) s_copyprot_state_stack[s_copyprot_sp] = state;
    s_copyprot_sp++;
}
/* Logs fragment36's view of (buttonPressed & 0x9000) and D_82100EC8
 * each loop iteration. Reveals whether the press-detection check
 * sees the rising edge. */
static __thread int s_frag36_iter = 0;
/* Cont_ReadInputs entry/exit — log rate and any rising-edge consumption.
 * Logs buttonDown BEFORE write (= prev) and AFTER (= current). */
static __thread uint32_t s_cri_count = 0;
static __thread uint32_t s_cri_last_logged = 0;
void pkmnstadium_cri_enter(uint32_t prev_buttondown_via_cont0_word) {
    s_cri_count++;
    if (s_cri_count <= 5 || (s_cri_count % 100) == 0) {
        fprintf(stderr, "[cri] entry #%u\n", s_cri_count);
        fflush(stderr);
    }
}
void pkmnstadium_cri_exit(uint32_t cur_buttondown_via_cont0_word, uint32_t buttonpressed) {
    if (buttonpressed != 0 || (s_cri_count - s_cri_last_logged) >= 200) {
        s_cri_last_logged = s_cri_count;
        fprintf(stderr,
            "[cri] #%u current=0x%X buttonPressed=0x%X\n",
            s_cri_count, cur_buttondown_via_cont0_word & 0xFFFF,
            buttonpressed & 0xFFFF);
        fflush(stderr);
    }
}

/* process_geo_layout dispatch-site diagnostic.
 *
 * Inside process_geo_layout, the dispatch is:
 *   t9 = GeoLayoutJumpTable[cmd_byte];   // 4 bytes per entry
 *   jalr t9;                              // call dispatched handler
 *
 * If cmd_byte is out of range (table has 40 entries, indices 0..39),
 * the table read goes off the end into adjacent .rodata. The
 * recompiler then can't find a matching function for the bogus
 * address. This hook fires right before the jalr; if t9 is small
 * (< 0x80000000), log cmd_byte + t9 + the cmd-stream pointer + a
 * dump of nearby cmd bytes so we can identify the malformed layout.
 */
void pkmnstadium_geo_dispatch_log(uint8_t* rdram, uint32_t cmd_byte,
                                    uint32_t fn_ptr, uint32_t cmd_stream_addr) {
    if (fn_ptr >= 0x80000000u) return;
    static int s_logged = 0;
    s_logged++;
    if (s_logged > 5) return;  // first few are enough to identify
    fprintf(stderr,
        "[geo-dispatch] SUSPECT cmd_byte=0x%02X (=%u) -> fn=0x%08X "
        "cmd_stream=0x%08X\n",
        cmd_byte & 0xFF, cmd_byte & 0xFF, fn_ptr, cmd_stream_addr);
    /* Dump 16 bytes of the cmd stream around its current position. */
    if (cmd_stream_addr >= 0x80000000u && cmd_stream_addr < 0x80800000u) {
        uint32_t paddr = cmd_stream_addr & 0x1FFFFFFFu;
        if (paddr >= 8 && paddr + 16 < (1u << 30)) {
            fprintf(stderr, "  cmd_stream context [%08X-8..+8]:",
                cmd_stream_addr);
            for (int i = -8; i < 16; i++) {
                fprintf(stderr, " %02X", rdram[(paddr + i) ^ 3]);
                if (i == -1) fprintf(stderr, " |");
            }
            fprintf(stderr, "\n");
        }
    }
    fflush(stderr);
}

/* Util_ConvertAddrToVirtAddr exit diagnostic.
 *
 * Catches any (input → output) pair where output is a non-NULL value
 * smaller than 0x80000000 — that means a segment/fragment was missing
 * its registration and the function returned the input pass-through,
 * which downstream callers will use as a function pointer or memory
 * address and crash on. */
void pkmnstadium_addr_translate_log(uint32_t in, uint32_t out) {
    int suspect = (out != 0 && out < 0x80000000u);
    if (!suspect) return;
    static int s_n = 0;
    s_n++;
    if (s_n <= 8 || (s_n & 31) == 0) {
        fprintf(stderr,
            "[addr-xlate] #%d SUSPECT in=0x%08X -> out=0x%08X\n",
            s_n, in, out);
        fflush(stderr);
    }
}

/* func_80010FDC entry diagnostic.
 *
 * Stadium hits a get_function lookup miss at 0x00000E00 inside this
 * function — `arg1 = Util_ConvertAddrToVirtAddr(arg1); arg1(0, arg0);`
 * The result of the segment translation is being called as a function,
 * but the translated value is too small to be a real function address.
 *
 * Log arg1 BEFORE conversion (the seg-addr from the geo layout) and
 * AFTER conversion (what got called), plus arg0 (the GraphNode) and
 * arg2 (data ptr). Tells us which geo layout source has the bad
 * function-pointer reference. */
void pkmnstadium_geo_render_call_log(uint32_t arg0_node, uint32_t arg1_seg,
                                       uint32_t arg2_data) {
    static int s_n = 0;
    s_n++;
    /* Log first 16 calls + any whose input is "small" (likely to
     * translate to a near-NULL function pointer). */
    int small = (arg1_seg != 0 && arg1_seg < 0x80000000u) &&
                ((arg1_seg & 0xFF000000u) == 0);
    if (s_n <= 16 || small) {
        fprintf(stderr,
            "[geo-render] #%d node=0x%08X arg1(seg)=0x%08X arg2=0x%08X%s\n",
            s_n, arg0_node, arg1_seg, arg2_data,
            small ? " <-- SUSPECT (low addr)" : "");
        fflush(stderr);
    }
}

/* Stop all libnaudio voices by clearing each voice's unk_038
 * (sequence-stream pointer). func_8003AD58 (the per-voice synth)
 * skips voices with unk_038 == NULL, so this is sufficient to
 * prevent the synth from dereferencing the voices' unk_090 bank
 * pointer.
 *
 * Hooked at func_8004FF20 entry — fragment36 calls func_8004FF20
 * right before main_pool_pop_state('TITL'), which frees the 1 MiB
 * SoundBank buffer at fragment36.c:351. Without this, the synth
 * keeps voicing the freed bank across the state transition and
 * the audio thread crashes.
 *
 * Why not just call func_8004FCD8 (the libnaudio audio-stop)?
 * Because that's a recompiled function and hooks can't easily
 * cross-call recompiled funcs. Direct rdram writes do the same job.
 *
 * D_800FC7D0 (vaddr 0x800FC7D0) holds a pointer to the voice array;
 * D_800FC7CC (vaddr 0x800FC7CC) holds the voice count;
 * sizeof(unk_D_800FC7D0) = 0x150; unk_038 sits at offset 0x38.
 */
void pkmnstadium_audio_stop_voices(uint8_t* rdram) {
    uint32_t count_paddr = 0x000FC7CCu;
    uint32_t count =
        ((uint32_t)rdram[(count_paddr + 0) ^ 3] << 24) |
        ((uint32_t)rdram[(count_paddr + 1) ^ 3] << 16) |
        ((uint32_t)rdram[(count_paddr + 2) ^ 3] <<  8) |
        ((uint32_t)rdram[(count_paddr + 3) ^ 3]);
    uint32_t arr_ptr_paddr = 0x000FC7D0u;
    uint32_t arr_vaddr =
        ((uint32_t)rdram[(arr_ptr_paddr + 0) ^ 3] << 24) |
        ((uint32_t)rdram[(arr_ptr_paddr + 1) ^ 3] << 16) |
        ((uint32_t)rdram[(arr_ptr_paddr + 2) ^ 3] <<  8) |
        ((uint32_t)rdram[(arr_ptr_paddr + 3) ^ 3]);
    if (arr_vaddr == 0 || arr_vaddr < 0x80000000u || arr_vaddr >= 0x80800000u) {
        fprintf(stderr,
            "[audio-stop] D_800FC7D0 array ptr out of range (0x%08X), skipped\n",
            arr_vaddr);
        fflush(stderr);
        return;
    }
    if (count == 0 || count > 256) {
        fprintf(stderr,
            "[audio-stop] D_800FC7CC voice count out of range (%u), skipped\n",
            count);
        fflush(stderr);
        return;
    }
    uint32_t arr_paddr = arr_vaddr & 0x1FFFFFFFu;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t voice_paddr = arr_paddr + i * 0x150u;
        for (int b = 0; b < 4; b++) {
            rdram[(voice_paddr + 0x38 + b) ^ 3] = 0;
        }
    }
    static int s_logged = 0;
    if (!s_logged) {
        s_logged = 1;
        fprintf(stderr,
            "[audio-stop] cleared unk_038 for %u voice(s) at "
            "0x%08X (size 0x150 each) — synth will skip these "
            "until the next sequence-load reassigns them\n",
            count, arr_vaddr);
        fflush(stderr);
    }
}

/* miniEkansInitCam entry diagnostic.
 *
 * The Ekans minigame (auto-attract path) crashes in miniUpdateCamera
 * writing to D_87906054->unk_24.fovy where D_87906054 is NULL.
 * D_87906054 is assigned at miniEkansInitCam entry as
 * D_87906054 = D_87906050->unk_00.unk_0C, so either D_87906050 is
 * NULL or its unk_0C child-list head field is NULL.
 *
 * Dump both at entry to localize. */
void pkmnstadium_mini_ekans_init_cam_enter(uint8_t* rdram) {
    uint32_t d50_addr = 0x87906050u;
    uint32_t d50_paddr = d50_addr & 0x1FFFFFFFu;
    uint32_t d50_val =
        ((uint32_t)rdram[(d50_paddr + 0) ^ 3] << 24) |
        ((uint32_t)rdram[(d50_paddr + 1) ^ 3] << 16) |
        ((uint32_t)rdram[(d50_paddr + 2) ^ 3] <<  8) |
        ((uint32_t)rdram[(d50_paddr + 3) ^ 3]);
    fprintf(stderr,
        "[ekans-cam] D_87906050(@0x%08X) holds = 0x%08X\n",
        d50_addr, d50_val);
    if (d50_val == 0 || d50_val < 0x80000000u || d50_val >= 0x80800000u) {
        fprintf(stderr, "  (no further dump — D_87906050 outside RAM)\n");
        fflush(stderr);
        return;
    }
    /* Dump first 0x20 bytes of *D_87906050 (the GraphNode-or-similar root). */
    uint32_t struct_paddr = d50_val & 0x1FFFFFFFu;
    fprintf(stderr, "  [*D_87906050 @ 0x%08X]:", d50_val);
    for (int i = 0; i < 0x20; i++) {
        if ((i & 0xF) == 0) fprintf(stderr, "\n    +0x%02X:", i);
        fprintf(stderr, " %02X", rdram[(struct_paddr + i) ^ 3]);
    }
    fprintf(stderr, "\n");
    /* Read unk_00.unk_0C — that's what gets stored into D_87906054.
     * If the struct layout matches, unk_0C is at offset 0xC inside
     * unk_00; unk_00 is at offset 0 of the parent; so 0x0C absolute. */
    uint32_t child_val =
        ((uint32_t)rdram[(struct_paddr + 0xC) ^ 3] << 24) |
        ((uint32_t)rdram[(struct_paddr + 0xD) ^ 3] << 16) |
        ((uint32_t)rdram[(struct_paddr + 0xE) ^ 3] <<  8) |
        ((uint32_t)rdram[(struct_paddr + 0xF) ^ 3]);
    fprintf(stderr,
        "  D_87906050->unk_00.unk_0C = 0x%08X  (will be stored into D_87906054)\n",
        child_val);
    fflush(stderr);
}

/* Log fragment36's main-entry return value (= next gCurrentGameState). */
void pkmnstadium_frag36_exit(uint32_t v0) {
    fprintf(stderr, "[frag36] EXIT — return value (next state) = 0x%X\n", v0);
    fflush(stderr);
}

void pkmnstadium_frag36_check(uint32_t btn_pressed_word, uint32_t input_enable) {
    s_frag36_iter++;
    /* Only log nonzero btn or first 5 + every 50th iter to avoid flood */
    if (btn_pressed_word != 0 || s_frag36_iter <= 5 || (s_frag36_iter % 50) == 0) {
        fprintf(stderr,
            "[frag36] iter=%d btnPressed=0x%X (& 0x9000 = 0x%X) D_82100EC8=%u\n",
            s_frag36_iter, btn_pressed_word & 0xFFFF,
            btn_pressed_word & 0x9000, input_enable);
        fflush(stderr);
    }
}

void pkmnstadium_copyprot_exit(uint32_t v0) {
    s_copyprot_sp--;
    if (s_copyprot_sp < 0 || s_copyprot_sp >= 16) return;
    uint32_t in_state = s_copyprot_state_stack[s_copyprot_sp];
    if (v0 != in_state) {
        fprintf(stderr,
            "[copyprot] TRIPPED: input state=0x%X output=0x%X\n",
            in_state, v0);
        fflush(stderr);
    }
}

void pkmnstadium_pool_alloc_exit(uint8_t* rdram, uint32_t v0) {
    s_pool_sp--;
    if (s_pool_sp < 0 || s_pool_sp >= 16) return;
    uint32_t size = s_pool_size_stack[s_pool_sp];
    /* sMemPool.available at 0x800A608C */
    uint32_t avail = MEM_LOAD_BE32(rdram, 0x000A608Cu);
    uint32_t pool_start = MEM_LOAD_BE32(rdram, 0x000A6090u);  /* +0x20 */
    uint32_t pool_end   = MEM_LOAD_BE32(rdram, 0x000A6094u);  /* +0x24 */
    uint32_t listL      = MEM_LOAD_BE32(rdram, 0x000A6098u);  /* +0x28 */
    uint32_t listR      = MEM_LOAD_BE32(rdram, 0x000A609Cu);  /* +0x2C */
    /* Log full state on FAILURE; otherwise log a one-line summary
     * once per ~32 KiB of total alloc activity. */
    static __thread uint64_t s_total_allocd = 0;
    static __thread uint64_t s_last_logged = 0;
    s_total_allocd += size;
    if (v0 == 0) {
        fprintf(stderr,
            "[pool] ALLOC FAIL size=0x%X (=%u) avail=0x%X "
            "start=0x%08X end=0x%08X listL=0x%08X listR=0x%08X\n",
            size, size, avail, pool_start, pool_end, listL, listR);
        fflush(stderr);
    } else if (s_total_allocd - s_last_logged >= 0x8000) {
        s_last_logged = s_total_allocd;
        fprintf(stderr,
            "[pool] ok size=0x%X result=0x%08X avail=0x%X "
            "(total alloc'd: 0x%llX)\n",
            size, v0, avail, (unsigned long long)s_total_allocd);
        fflush(stderr);
    }
}

void pkmnstadium_pers_exit(uint8_t* rdram, uint32_t v0) {
    s_pers_sp--;
    if (s_pers_sp < 0 || s_pers_sp >= 16) return;
    uint32_t a0 = s_pers_a0_stack[s_pers_sp];
    uint32_t a1 = s_pers_a1_stack[s_pers_sp];
    /* If a destination buffer was allocated and validated, v0 is the
     * descriptor pointer (kseg0). Read the descriptor's first 8 bytes
     * — should be "PERS-SZP" or "PRES-???" if validation passed. If
     * v0 is 0, the function returned failure. */
    char magic[9] = {0};
    if (v0 != 0) {
        uint32_t paddr = v0 & 0x1FFFFFFFu;
        if (paddr + 8 <= 1024u * 1024u * 1024u) {
            for (int i = 0; i < 8; i++) {
                uint8_t b = rdram[(paddr + i) ^ 3];
                magic[i] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
            }
        }
    }
    fprintf(stderr,
        "[pers] func_80003DC4(rom=0x%08X..0x%08X) -> 0x%08X magic='%s'\n",
        a0, a1, v0, magic);
    fflush(stderr);
}

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
