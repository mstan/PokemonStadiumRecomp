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
#define INTERESTING_FN_COUNT 25
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
    "func_82100054",        /* (already above) */
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
