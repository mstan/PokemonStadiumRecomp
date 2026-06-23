/*
 * extras.c — game-specific runtime hooks for PokemonStadiumRecomp.
 *
 * Hosts the recomp_unhandled_* entry points that the engine emits
 * for instructions it can't translate at compile time. These are
 * NOT stubs: they are real implementations that abort loudly with
 * full diagnostic context
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
#include <string.h>

#include "app_paths.h" /* psr_app_file_c: runtime files next to the exe */

#ifdef _WIN32
/* Defined here (top of file) rather than before its sole use site
 * because <windows.h> typedefs `small` as a macro, which collides
 * with local variables of the same name elsewhere in this file
 * (e.g. pkmnstadium_geo_render_call_log's `int small`). Including
 * up top forces all subsequent code to use renamed locals if
 * needed; current code uses a non-conflicting name. */
#  include <windows.h>
#  include <dbghelp.h>
#  pragma comment(lib, "dbghelp.lib")
#endif

/* Pull in the real recomp_context layout (gpr/fpr/cop0 fields) for
 * hand-written recomp wrappers below. recomp.h is C-compatible. */
#include "recomp.h"

/* librecomp overlay-table maintenance. Releases the recompiler's
 * section registration for a Stadium fragment id when the game clears
 * it (Memmap_ClearFragmentMemmap), resetting section_addresses[] to the
 * link literal. Defined in lib/N64ModernRuntime/librecomp/src/overlays.cpp. */
extern void recomp_unregister_runtime_fragment(uint32_t id);
extern int32_t* section_addresses;

/* Read a 32-bit big-endian value from rdram. rdram on the host
 * side stores BE words in word-swapped (per-byte XOR-3) order, so
 * to read the original big-endian word we recombine bytes via the
 * usual (byte_index ^ 3) indexing. */
#define MEM_LOAD_BE32(rdram, paddr) (\
    ((uint32_t)(rdram)[((paddr) + 0) ^ 3] << 24) | \
    ((uint32_t)(rdram)[((paddr) + 1) ^ 3] << 16) | \
    ((uint32_t)(rdram)[((paddr) + 2) ^ 3] <<  8) | \
    ((uint32_t)(rdram)[((paddr) + 3) ^ 3]))


static void unhandled_abort(const char *kind, uint32_t pc, const char *detail) {
    /* Persistent-file copy: stderr in headless runs gets buffered and
     * the abort() can wipe it before the parent process reads. The
     * file is the source of truth for post-mortem inspection. */
    char errlog_path[1024];
    FILE *f = fopen(psr_app_file_c("last_error.log", errlog_path, sizeof errlog_path), "a");
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
/* shims but not these. These are NOT stubs — they abort loudly with  */
/* full context if reached, surfacing the missing implementation as a  */
/* clearly-named                                                       */
/* runtime failure rather than silent wrong behavior.                 */
/*                                                                    */
/* Real implementations would either model the libultra behavior      */
/* against context state, or escalate to an in-process emulator       */
/* handler. For first boot we want loud failure to identify what      */
/* code paths actually need real impls.                               */
/* ------------------------------------------------------------------ */

#define UNIMPL_LIBULTRA(name)                                                  \
    void name##_recomp(uint8_t *rdram, recomp_context *ctx) {                  \
        char errlog_path[1024];                                               \
        FILE *f = fopen(psr_app_file_c("last_error.log", errlog_path,         \
                                       sizeof errlog_path), "a");             \
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
UNIMPL_LIBULTRA(osLeoDiskInit)
UNIMPL_LIBULTRA(__osSetSR)
UNIMPL_LIBULTRA(__osSetCause)
UNIMPL_LIBULTRA(__osSetCount)
UNIMPL_LIBULTRA(__osSetCompare)
UNIMPL_LIBULTRA(rmonPrintf)

/* ------------------------------------------------------------------ */
/* TRACE_ENTRY / TRACE_RETURN ring (powered by game.toml             */
/* trace_mode = true). See include/trace.h.                          */
/* ------------------------------------------------------------------ */

/* 256K entries (2 MiB of const char*) — power of two, wraps cheaply.
 * The old 4096 cap was overrun by audio/render in <1 frame, so a crash
 * dump could not reach back to a constructor that ran a few frames
 * earlier. 256K spans many frames of full per-function tracing, which is
 * what the gallery null-vtable RE (issues #9/#16) needs. Only allocated/
 * written when PSR_TRACE_HOOKS is on; harmless otherwise. */
#define TRACE_RING_CAP (256u * 1024u)

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
static const char* const k_interesting_fns[] = {
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
    "func_8000D5C0",        /* GB Tower helper thread */
    "func_8000D678",        /* GB Tower main thread */
    "func_81200AA8",        /* GB Tower Transfer Pak init thread */
    "func_812011D0",        /* GB Tower init continuation */
    "func_81202210",        /* GB Tower framebuffer helper */
    "func_8120241C",        /* GB Tower framebuffer helper */
    "func_81202EA8",        /* GB Tower display-list helper */
    "func_81202FCC",        /* GB Tower display-list submit helper */
    "func_812033F4",        /* GB Tower startup / state driver */
    "func_81203F3C",        /* GB Tower update helper */
    "func_81204A84",        /* GB Tower state transition helper */
    "func_8120572C",        /* GB Tower per-frame state machine */
    "func_81206D9C",        /* GB Tower main-thread init */
    "func_81206E64",        /* GB Tower main-thread tick */
    "func_81206F38",        /* GB Tower helper-thread tick */
    "func_81209078",        /* GB Tower audio buffer pump */
    "func_81209870",        /* GB Tower GB CPU/emulator init/reset */
    "func_81209EB4",        /* GB Tower GB CPU step entry */
    "func_8120A660",        /* GB Tower GB CPU memory fetch helper */
    "func_8120A69C",        /* GB Tower GB CPU memory fetch helper */
    "func_8120A6A0",        /* GB Tower GB CPU memory fetch helper */
    "func_8120A73C",        /* GB Tower GB CPU dispatch loop */
    "static_9_81209FBC",    /* GB Tower GB CPU step exit continuation */
    "static_9_8120ACC0",    /* GB Tower GB CPU yielded memory-map setup */
    "static_9_8120ACD4",    /* GB Tower GB CPU yielded memory-map loop */
    "static_9_8120ACDC",    /* GB Tower GB CPU yielded memory-map resume */
    "func_8120B490",        /* GB Tower GB CPU save/yield helper */
    "func_8120B4B8",        /* GB Tower GB CPU restore/resume helper */
    "func_8120B4D8",        /* GB Tower GB CPU resume continuation */
    "func_8120B4E4",        /* GB Tower GB CPU resume continuation */
    "static_9_8120B424",    /* GB Tower GB CPU scanline/step continuation */
    "func_81208828",        /* GB Tower audio sample fill */
    "func_81209374",        /* GB Tower audio sample fill */
    "osGbSetNextBuffer",    /* GB Tower direct AI buffer submit */
};
#define INTERESTING_FN_COUNT ((int)(sizeof(k_interesting_fns) / sizeof(k_interesting_fns[0])))
static volatile uint64_t k_interesting_counts[sizeof(k_interesting_fns) / sizeof(k_interesting_fns[0])];

/* Voluntary-preemption fast-path entry. Declared here so we don't need
 * to pull in C++ headers from a C TU. Defined in
 * lib/N64ModernRuntime/ultramodern/src/scheduler_tick.cpp. Hot path is
 * one relaxed atomic load + branch; returns immediately when no yield
 * is pending. See scheduler_tick.hpp for the design rationale. */
extern void ultramodern_scheduler_tick(void);

static void pkmnstadium_asset_wait_trace(const char *func,
                                         const recomp_context *ctx,
                                         int is_return) {
    if (ctx == NULL || strcmp(func, "func_800484E0") != 0) {
        return;
    }

    enum { ASSET_WAIT_CALLER_CAP = 64 };
    static volatile uint32_t caller_pc[ASSET_WAIT_CALLER_CAP];
    static volatile uint64_t caller_count[ASSET_WAIT_CALLER_CAP];
    static volatile uint32_t next_slot = 0;

    uint32_t ra = (uint32_t)ctx->r31;
    uint32_t slot = ASSET_WAIT_CALLER_CAP;
    for (uint32_t i = 0; i < ASSET_WAIT_CALLER_CAP; i++) {
        if (__atomic_load_n(&caller_pc[i], __ATOMIC_RELAXED) == ra) {
            slot = i;
            break;
        }
    }

    if (slot == ASSET_WAIT_CALLER_CAP) {
        uint32_t new_slot = __atomic_fetch_add(&next_slot, 1, __ATOMIC_RELAXED);
        slot = new_slot & (ASSET_WAIT_CALLER_CAP - 1);
        __atomic_store_n(&caller_pc[slot], ra, __ATOMIC_RELAXED);
        __atomic_store_n(&caller_count[slot], 0, __ATOMIC_RELAXED);
        fprintf(stderr, "[asset-wait] new caller ra=0x%08X slot=%u\n", ra, slot);
        fflush(stderr);
    }

    if (!is_return) {
        return;
    }

    uint64_t count = __atomic_add_fetch(&caller_count[slot], 1, __ATOMIC_RELAXED);
    if (count <= 8 || (count & (count - 1)) == 0) {
        fprintf(stderr,
                "[asset-wait] caller=0x%08X count=%llu ret_v0=0x%08X\n",
                ra,
                (unsigned long long)count,
                (uint32_t)ctx->r2);
        fflush(stderr);
    }
}

static int pkmnstadium_is_gb_fragment_trace_func(const char *func) {
    if (func == NULL) {
        return 0;
    }

    return strncmp(func, "func_812", 8) == 0 ||
           strncmp(func, "static_9_812", 12) == 0 ||
           strcmp(func, "fragment1_entry") == 0 ||
           strncmp(func, "func_878", 8) == 0 ||
           strncmp(func, "static_14_878", 13) == 0 ||
           strcmp(func, "fragment2_entry") == 0;
}

static void pkmnstadium_gb_fragment_trace(const char *func,
                                          const recomp_context *ctx,
                                          int is_return) {
    if (!pkmnstadium_is_gb_fragment_trace_func(func)) {
        return;
    }

    static int trace_mode = -1;
    if (trace_mode < 0) {
        const char *env = getenv("PSR_GB_FRAG_TRACE");
        if (env == NULL || env[0] == '\0' || strcmp(env, "0") == 0) {
            trace_mode = 0;
        } else if (strcmp(env, "full") == 0 || strcmp(env, "FULL") == 0) {
            trace_mode = 2;
        } else {
            trace_mode = 1;
        }
    }
    if (trace_mode == 0) {
        return;
    }
    if (trace_mode == 1 &&
        (strcmp(func, "func_812015E0") == 0 ||
         strcmp(func, "func_81201560") == 0 ||
         strcmp(func, "func_8120157C") == 0 ||
         strcmp(func, "func_81201598") == 0)) {
        return;
    }

    static volatile uint64_t event_count = 0;
    uint64_t event = __atomic_add_fetch(&event_count, 1, __ATOMIC_RELAXED);

    /* The current GB Tower crash happens shortly after fragment 0x81200000
     * registration. Keep the first large window complete; after that, avoid
     * unbounded disk growth while still proving whether execution continues.
     */
    if (event > 250000 && (event & (event - 1)) != 0) {
        return;
    }

    char trace_path[1024];
    FILE *f = fopen(
        psr_app_file_c("gb_frag_trace.log", trace_path, sizeof trace_path),
        event == 1 ? "w" : "a");
    if (f == NULL) {
        return;
    }

    fprintf(f,
        "%c #%llu %s",
        is_return ? 'R' : 'E',
        (unsigned long long)event,
        func);
    if (ctx != NULL) {
        fprintf(f,
            " ra=0x%08X sp=0x%08X v0=0x%08X v1=0x%08X "
            "a0=0x%08X a1=0x%08X a2=0x%08X a3=0x%08X "
            "gp=0x%08X t7=0x%08X t8=0x%08X t9=0x%08X "
            "s0=0x%08X s1=0x%08X s6=0x%08X",
            (uint32_t)ctx->r31,
            (uint32_t)ctx->r29,
            (uint32_t)ctx->r2,
            (uint32_t)ctx->r3,
            (uint32_t)ctx->r4,
            (uint32_t)ctx->r5,
            (uint32_t)ctx->r6,
            (uint32_t)ctx->r7,
            (uint32_t)ctx->r28,
            (uint32_t)ctx->r15,
            (uint32_t)ctx->r24,
            (uint32_t)ctx->r25,
            (uint32_t)ctx->r16,
            (uint32_t)ctx->r17,
            (uint32_t)ctx->r22);
    }
    fputc('\n', f);
    fclose(f);
}

static void pkmnstadium_trace_entry_common(const char *func,
                                           const recomp_context *ctx) {
    /* DIAG (dev/register-quit-softlock): skip libnaudio per-audio-frame
     * functions (e.g. _n_loadOutputBuffer) from the trace ring — they flood
     * it at audio rate and bury the control-flow we care about. Still run the
     * load-bearing preemption tick. Remove with the TEMP trace block. */
    if (func[0] == '_' && func[1] == 'n' && func[2] == '_') {
        ultramodern_scheduler_tick();
        return;
    }
    /* Issue #9 geo probe (only active when funcs_85.c is built with the hook,
     * via -DPSR_GEO_PROBE=ON). process_geo_layout(pool=a0, segptr=a1) is the geo
     * interpreter; a1 (ctx->r5) is the geo SOURCE pointer. Log it for TOP-LEVEL
     * model-processing calls (caller in func_80019328/func_80019420 land,
     * ra>=0x80019000) — recursive sub-layout calls (callers are geo handlers
     * <0x80019000) are skipped to keep the log readable. Classify segptr vs the
     * CURRENT frag-239 instance (gFragments[239]) so we can see whether the very
     * first processing of a model already gets a graph-pool segptr (Case A: bad
     * reload) or only a later one does (Case B: double-processing). */
    if (ctx != NULL && func[0] == 'p' && __builtin_strcmp(func, "process_geo_layout") == 0) {
        uint32_t ra  = (uint32_t)ctx->r31;
        if (ra >= 0x80019000u && ra < 0x8001A000u) {  /* top-level model-processing call */
            extern unsigned char* recomp_runtime_get_rdram(void);
            unsigned char* rd = recomp_runtime_get_rdram();
            uint32_t seg = (uint32_t)ctx->r5;
            if (rd != NULL) {
                /* gFragments[239] = {u32 vaddr, u32 size} at 0x800A58F0 + 239*8 */
                uint32_t fe = 0x800A58F0u + 239u * 8u;
                #define RD_BE32(va) ( ((uint32_t)rd[(((va)&0x1FFFFFFFu)+0)^3]<<24) | \
                                      ((uint32_t)rd[(((va)&0x1FFFFFFFu)+1)^3]<<16) | \
                                      ((uint32_t)rd[(((va)&0x1FFFFFFFu)+2)^3]<<8)  | \
                                      ((uint32_t)rd[(((va)&0x1FFFFFFFu)+3)^3]) )
                uint32_t fbase = RD_BE32(fe);
                uint32_t fsize = RD_BE32(fe + 4u);
                uint32_t fend  = fbase + fsize;
                uint32_t segw  = (seg >= 0x80000000u && (seg & 0x1FFFFFFFu) < 0x00800000u) ? RD_BE32(seg) : 0;
                const char* cls = "OTHER";
                if (seg >= fbase && seg < fend) cls = "IN_FRAG239";
                else if (seg == fend) cls = "FRAG_END";
                else if (seg == fend + 4u) cls = "FRAG_END+4";
                else if (seg >= fend && seg < fend + 0x40000u) cls = "PAST_FRAG_POOL";
                #undef RD_BE32
                fprintf(stderr, "[geoseg] seg=0x%08X caller=0x%08X frag239=[0x%08X,0x%08X) cls=%s firstword=0x%08X\n",
                        seg, ra, fbase, fend, cls, segw);
                fflush(stderr);
            }
        }
    }
    /* DIAG (dev/register-quit-softlock): log ENTRY of the registration-Quit
     * return chain + the pool-free path, so enter/return pairing pins which
     * function is entered last without returning. Remove with the TEMP trace. */
    if (ctx != NULL && (__builtin_strcmp(func, "func_80029008") == 0 ||
                        __builtin_strcmp(func, "func_80029984") == 0 ||
                        __builtin_strcmp(func, "func_80029BC0") == 0 ||
                        __builtin_strcmp(func, "main_pool_try_free") == 0 ||
                        __builtin_strcmp(func, "main_pool_free") == 0 ||
                        __builtin_strcmp(func, "func_84203E6C") == 0)) {
        fprintf(stderr,
            "[jrdiag] %s ENTER: ra(r31)=0x%08X sp(r29)=0x%08X a0=0x%08X\n",
            func, (uint32_t)ctx->r31, (uint32_t)ctx->r29, (uint32_t)ctx->r4);
        fflush(stderr);
    }
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

    pkmnstadium_asset_wait_trace(func, ctx, 0);
    pkmnstadium_gb_fragment_trace(func, ctx, 0);

    /* Voluntary-preemption check. Almost always returns immediately
     * after a single relaxed atomic load. Fires the slow path only
     * when the ultramodern host monitor has decided the current game
     * thread has been running for >200ms without a context switch —
     * at which point we drain pending external messages and yield to
     * any queued thread, letting predicate-flipping threads make
     * progress. Retires the free-battle-modal, petit-cup, and
     * asset-pending-bypass per-site hacks. */
    ultramodern_scheduler_tick();
}

void pkmnstadium_trace_entry(const char *func) {
    pkmnstadium_trace_entry_common(func, NULL);
}

void pkmnstadium_trace_entry_ctx(const char *func, const void *ctx) {
    pkmnstadium_trace_entry_common(func, (const recomp_context *)ctx);
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
/* GB Tower state-machine trace.                                      */
/*                                                                    */
/* The embedded GB runtime is a two-thread fragment with most control */
/* flow in func_8120572C. These hooks record the branch checkpoints    */
/* that decide whether the GB core advances and whether a framebuffer */
/* display list is submitted. They are diagnostics only and are        */
/* queried through debug_server's gbtower_trace command.              */
/* ------------------------------------------------------------------ */

struct gbtower_trace_ev {
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

#define GBTOWER_TRACE_CAP 32768
static struct gbtower_trace_ev s_gbtower_trace[GBTOWER_TRACE_CAP];
static volatile uint64_t s_gbtower_trace_seq = 0;

static int vaddr_to_paddr32(uint32_t vaddr, uint32_t* out_paddr) {
    uint32_t paddr;
    if ((vaddr >= 0x80000000u && vaddr < 0x80800000u) ||
        (vaddr >= 0xA0000000u && vaddr < 0xA0800000u)) {
        paddr = vaddr & 0x7FFFFFu;
    } else if (vaddr < 0x800000u) {
        paddr = vaddr;
    } else {
        return 0;
    }
    if (paddr >= 0x800000u) {
        return 0;
    }
    *out_paddr = paddr;
    return 1;
}

static uint8_t load_be8_v(uint8_t* rdram, uint32_t vaddr) {
    uint32_t paddr = 0;
    if (!vaddr_to_paddr32(vaddr, &paddr)) {
        return 0;
    }
    return rdram[paddr ^ 3u];
}

static uint16_t load_be16_v(uint8_t* rdram, uint32_t vaddr) {
    uint32_t paddr = 0;
    if (!vaddr_to_paddr32(vaddr, &paddr) || paddr + 1u >= 0x800000u) {
        return 0;
    }
    return (uint16_t)(((uint16_t)rdram[paddr ^ 3u] << 8) |
                      (uint16_t)rdram[(paddr + 1u) ^ 3u]);
}

static uint32_t load_be32_v(uint8_t* rdram, uint32_t vaddr) {
    uint32_t paddr = 0;
    if (!vaddr_to_paddr32(vaddr, &paddr) || paddr + 3u >= 0x800000u) {
        return 0;
    }
    return MEM_LOAD_BE32(rdram, paddr);
}

static uint32_t frag9_runtime_vaddr(uint32_t link_offset) {
    uint32_t base = 0x81200000u;
    if (section_addresses != NULL && section_addresses[9] != 0) {
        base = (uint32_t)section_addresses[9];
    }
    return base + link_offset;
}

static uint32_t frag9_load32(uint8_t* rdram, uint32_t link_offset) {
    return load_be32_v(rdram, frag9_runtime_vaddr(link_offset));
}

static uint8_t frag9_load8(uint8_t* rdram, uint32_t link_offset) {
    return load_be8_v(rdram, frag9_runtime_vaddr(link_offset));
}

void pkmnstadium_gbtower_state_trace(uint8_t* rdram, uint32_t tag,
                                     uint32_t s0, const void* raw_ctx) {
    static int trace_cpu_tags = -1;
    if (trace_cpu_tags < 0) {
        const char* env = getenv("PSR_GBTOWER_TRACE_CPU");
        trace_cpu_tags = (env != NULL && env[0] != '\0' && env[0] != '0') ? 1 : 0;
    }
    if (!trace_cpu_tags && (tag == 0x8120A73Cu || tag == 0x81209FB4u)) {
        return;
    }

    const recomp_context* ctx = (const recomp_context*)raw_ctx;
    if (s0 == 0) {
        s0 = frag9_load32(rdram, 0x2B2C0u);
    }
    uint64_t seq = __atomic_fetch_add(&s_gbtower_trace_seq, 1, __ATOMIC_RELAXED);
    struct gbtower_trace_ev* ev = &s_gbtower_trace[seq % GBTOWER_TRACE_CAP];
    memset(ev, 0, sizeof(*ev));

    ev->seq = seq;
    ev->tag = tag;
    ev->s0 = s0;
    if (ctx != NULL) {
        ev->ra = (uint32_t)ctx->r31;
        ev->sp = (uint32_t)ctx->r29;
        ev->r2 = (uint32_t)ctx->r2;
        ev->r3 = (uint32_t)ctx->r3;
        ev->r4 = (uint32_t)ctx->r4;
        ev->r5 = (uint32_t)ctx->r5;
        ev->r6 = (uint32_t)ctx->r6;
        ev->r7 = (uint32_t)ctx->r7;
        ev->r8 = (uint32_t)ctx->r8;
        ev->r9 = (uint32_t)ctx->r9;
        ev->r10 = (uint32_t)ctx->r10;
        ev->r11 = (uint32_t)ctx->r11;
        ev->r12 = (uint32_t)ctx->r12;
        ev->r13 = (uint32_t)ctx->r13;
        ev->r14 = (uint32_t)ctx->r14;
        ev->r15 = (uint32_t)ctx->r15;
        ev->r16 = (uint32_t)ctx->r16;
        ev->r17 = (uint32_t)ctx->r17;
        ev->r18 = (uint32_t)ctx->r18;
        ev->r19 = (uint32_t)ctx->r19;
        ev->r20 = (uint32_t)ctx->r20;
        ev->r21 = (uint32_t)ctx->r21;
        ev->r22 = (uint32_t)ctx->r22;
        ev->r23 = (uint32_t)ctx->r23;
        ev->r24 = (uint32_t)ctx->r24;
        ev->r25 = (uint32_t)ctx->r25;
        ev->ctx_r2_byte = load_be8_v(rdram, (uint32_t)ctx->r2);
        uint32_t op = ev->ctx_r2_byte & 0xFFu;
        ev->op_table0 = load_be32_v(rdram, (uint32_t)ctx->r28 + 0x4B88u + op * 8u);
        ev->op_handler = load_be32_v(rdram, (uint32_t)ctx->r28 + 0x4B8Cu + op * 8u);
    }

    ev->state = frag9_load32(rdram, 0x2C4DCu);
    ev->phase = frag9_load8(rdram, 0x2C4FCu);
    ev->g_c4e0 = frag9_load32(rdram, 0x2C4E0u);
    ev->g_c4e4 = frag9_load32(rdram, 0x2C4E4u);
    ev->g_c4e8 = frag9_load32(rdram, 0x2C4E8u);
    ev->g_c4ec = frag9_load32(rdram, 0x2C4ECu);
    ev->g_c4f0 = frag9_load32(rdram, 0x2C4F0u);
    ev->g_c4f4 = frag9_load32(rdram, 0x2C4F4u);
    ev->g_c4f8 = frag9_load32(rdram, 0x2C4F8u);
    ev->g_c4fc = frag9_load32(rdram, 0x2C4FCu);
    ev->g_c4e2 = frag9_load8(rdram, 0x2C4E2u);
    ev->g_c4f6 = frag9_load8(rdram, 0x2C4F6u);
    ev->g_c4f7 = frag9_load8(rdram, 0x2C4F7u);
    ev->g_c4f9 = frag9_load8(rdram, 0x2C4F9u);
    ev->g_b2b8 = frag9_load32(rdram, 0x2B2B8u);

    if (s0 != 0) {
        ev->s_ctrl0 = load_be32_v(rdram, s0 + 0x5DC4u);
        ev->s_ctrl1 = load_be32_v(rdram, s0 + 0x5DC8u);
        ev->s_ctrl2 = load_be32_v(rdram, s0 + 0x5DCCu);
        ev->s_5dd0  = load_be32_v(rdram, s0 + 0x5DD0u);
        ev->s_hdr0  = load_be32_v(rdram, s0 + 0x5CA8u);
        ev->s_hdr1  = load_be32_v(rdram, s0 + 0x5DA4u);
        ev->s_hdr2  = load_be32_v(rdram, s0 + 0x5DA8u);
        ev->s_ready0 = load_be32_v(rdram, s0 + 0x549Cu);
        ev->s_ready1 = load_be32_v(rdram, s0 + 0x559Cu);
        ev->s_pad0 = load_be32_v(rdram, s0 + 0x53E8u);
        ev->gb_pc = load_be16_v(rdram, s0 + 0x53E8u);
        ev->gb_pc_base = load_be32_v(rdram, s0 + 0x55ACu + ((ev->gb_pc >> 10) & 0x3Cu));
        ev->gb_pc_addr = ev->gb_pc_base + ev->gb_pc;
        ev->gb_pc_byte = load_be8_v(rdram, ev->gb_pc_addr);
        ev->io_ff00_03 = load_be32_v(rdram, s0 + 0x100u);
        ev->io_ff04_07 = load_be32_v(rdram, s0 + 0x104u);
        ev->io_ff0c_0f = load_be32_v(rdram, s0 + 0x10Cu);
        ev->io_ff40_43 = load_be32_v(rdram, s0 + 0x140u);
        ev->io_ff44_47 = load_be32_v(rdram, s0 + 0x144u);
        ev->io_ff4c_4f = load_be32_v(rdram, s0 + 0x14Cu);
        ev->io_ff50_53 = load_be32_v(rdram, s0 + 0x150u);
        ev->io_ff54_57 = load_be32_v(rdram, s0 + 0x154u);
        ev->hram_ff80_83 = load_be32_v(rdram, s0 + 0x180u);
        ev->hram_ff84_87 = load_be32_v(rdram, s0 + 0x184u);
        ev->hram_ff88_8b = load_be32_v(rdram, s0 + 0x188u);
        ev->hram_ffd4_d7 = load_be32_v(rdram, s0 + 0x1D4u);
        ev->hram_fffc_ff = load_be32_v(rdram, s0 + 0x1FCu);
        ev->h53d6 = load_be16_v(rdram, s0 + 0x53D6u);
        ev->h53d8 = load_be16_v(rdram, s0 + 0x53D8u);
        ev->h53da = load_be16_v(rdram, s0 + 0x53DAu);
        ev->h53dc = load_be16_v(rdram, s0 + 0x53DCu);
        ev->h53de = load_be16_v(rdram, s0 + 0x53DEu);
        ev->w5388 = load_be32_v(rdram, s0 + 0x5388u);
        ev->w538c = load_be32_v(rdram, s0 + 0x538Cu);
        ev->w53b8 = load_be32_v(rdram, s0 + 0x53B8u);
        ev->w53c0 = load_be32_v(rdram, s0 + 0x53C0u);
        ev->w53c4 = load_be32_v(rdram, s0 + 0x53C4u);
        ev->w53c8 = load_be32_v(rdram, s0 + 0x53C8u);
        ev->w53cc = load_be32_v(rdram, s0 + 0x53CCu);
        ev->w53d0 = load_be32_v(rdram, s0 + 0x53D0u);
        ev->w53f0 = load_be32_v(rdram, s0 + 0x53F0u);
        ev->w53f2_53f5 = load_be32_v(rdram, s0 + 0x53F2u);
        ev->saved_5c44 = load_be32_v(rdram, s0 + 0x5C44u);
        ev->saved_5c48 = load_be32_v(rdram, s0 + 0x5C48u);
        ev->saved_5c4c = load_be32_v(rdram, s0 + 0x5C4Cu);
        ev->saved_5c50 = load_be32_v(rdram, s0 + 0x5C50u);
        ev->saved_5c54 = load_be32_v(rdram, s0 + 0x5C54u);
        ev->w5390 = load_be32_v(rdram, s0 + 0x5390u);
        ev->w5394 = load_be32_v(rdram, s0 + 0x5394u);
        ev->w5398 = load_be32_v(rdram, s0 + 0x5398u);
        ev->w53a4 = load_be32_v(rdram, s0 + 0x53A4u);
        ev->w53b4 = load_be32_v(rdram, s0 + 0x53B4u);
        ev->w53bc = load_be32_v(rdram, s0 + 0x53BCu);
        ev->rom_0140 = load_be32_v(rdram, ev->w53bc + 0x140u);
        ev->rom_0144 = load_be32_v(rdram, ev->w53bc + 0x144u);
        ev->rom_0148 = load_be32_v(rdram, ev->w53bc + 0x148u);
        ev->rom_014c = load_be32_v(rdram, ev->w53bc + 0x14Cu);
        ev->rom_type = load_be8_v(rdram, ev->w53bc + 0x147u);
        ev->cart_type_flags = frag9_load32(rdram, 0x0D90Cu + ev->rom_type * 4u);
        ev->w5484 = load_be32_v(rdram, s0 + 0x5484u);
        ev->w5488 = load_be32_v(rdram, s0 + 0x5488u);
        ev->w548c = load_be32_v(rdram, s0 + 0x548Cu);
        ev->w55ac = load_be32_v(rdram, s0 + 0x55ACu);
        ev->w55b0 = load_be32_v(rdram, s0 + 0x55B0u);
        ev->w55b4 = load_be32_v(rdram, s0 + 0x55B4u);
        ev->w55b8 = load_be32_v(rdram, s0 + 0x55B8u);
        ev->w55bc = load_be32_v(rdram, s0 + 0x55BCu);
        ev->w55c0 = load_be32_v(rdram, s0 + 0x55C0u);
        ev->w55c4 = load_be32_v(rdram, s0 + 0x55C4u);
        ev->w55c8 = load_be32_v(rdram, s0 + 0x55C8u);
        ev->w55ec = load_be32_v(rdram, s0 + 0x55ECu);
        ev->w55f0 = load_be32_v(rdram, s0 + 0x55F0u);
        ev->w55f4 = load_be32_v(rdram, s0 + 0x55F4u);
        ev->w55f8 = load_be32_v(rdram, s0 + 0x55F8u);
        ev->w55fc = load_be32_v(rdram, s0 + 0x55FCu);
        ev->w5600 = load_be32_v(rdram, s0 + 0x5600u);
        ev->w5604 = load_be32_v(rdram, s0 + 0x5604u);
        ev->w5608 = load_be32_v(rdram, s0 + 0x5608u);
        ev->w576c = load_be32_v(rdram, s0 + 0x576Cu);
        ev->w5d64 = load_be32_v(rdram, s0 + 0x5D64u);
        ev->w5d68 = load_be32_v(rdram, s0 + 0x5D68u);
        ev->w5d6c = load_be32_v(rdram, s0 + 0x5D6Cu);
        ev->w5d70 = load_be32_v(rdram, s0 + 0x5D70u);
        ev->w5d74 = load_be32_v(rdram, s0 + 0x5D74u);
        ev->w5d78 = load_be32_v(rdram, s0 + 0x5D78u);
        ev->w5d7c = load_be32_v(rdram, s0 + 0x5D7Cu);
    }
}

uint64_t pkmnstadium_gbtower_trace_seq(void) {
    return __atomic_load_n(&s_gbtower_trace_seq, __ATOMIC_RELAXED);
}

uint32_t pkmnstadium_gbtower_trace_cap(void) {
    return GBTOWER_TRACE_CAP;
}

void pkmnstadium_gbtower_trace_get(uint32_t i, struct gbtower_trace_ev* out) {
    if (out == NULL) {
        return;
    }
    *out = s_gbtower_trace[i % GBTOWER_TRACE_CAP];
}

/* ------------------------------------------------------------------ */
/* Memmap segment/fragment binding ring.                              */
/*                                                                    */
/* Always-on ring capturing every mutation of gSegments[]/gFragments[]*/
/* in memmap.c. Built to diagnose the menu cursor/icon corruption:    */
/* RSP segment 1 resolves icon-texture loads into a STALE/wrong       */
/* fragment's code section (e.g. fragment 31 "transfer pak checker"   */
/* left bound after the Game Pak Check screen, while the Poke Cup      */
/* Registration menu reads seg1+offset as a texture). Each entry       */
/* records (seq, kind, id, vaddr, size, caller PC, gCurrentGameState) */
/* so the post-mortem shows the exact bind timeline across a screen    */
/* transition: which code bound segment N to what, and whether a menu  */
/* re-bound it or inherited a previous screen's value.                */
/*                                                                    */
/* Hooked at the entry of the four memmap mutators via game.toml:     */
/*   kind 0 = Memmap_SetSegmentMap(id, vaddr, size)                   */
/*   kind 1 = Memmap_ClearSegmentMemmap(id)                           */
/*   kind 2 = Memmap_SetFragmentMap(id, vaddr, size)                  */
/*   kind 3 = Memmap_ClearFragmentMemmap(id)                          */
/* gCurrentGameState lives at vaddr 0x80075668 (phys 0x00075668). */
struct memmap_ev {
    uint64_t seq;
    uint32_t kind;
    uint32_t id;
    uint32_t vaddr;
    uint32_t size;
    uint32_t caller;
    uint32_t game_state;
};
#define MEMMAP_RING_CAP 2048
static volatile struct memmap_ev s_memmap_ring[MEMMAP_RING_CAP];
static volatile uint64_t s_memmap_seq = 0;

static void memmap_ring_push(uint32_t kind, uint32_t id, uint32_t vaddr,
                             uint32_t size, uint32_t caller, uint32_t game_state) {
    uint64_t s = __atomic_fetch_add(&s_memmap_seq, 1, __ATOMIC_RELAXED);
    uint32_t i = (uint32_t)(s % MEMMAP_RING_CAP);
    s_memmap_ring[i].seq = s;
    s_memmap_ring[i].kind = kind;
    s_memmap_ring[i].id = id;
    s_memmap_ring[i].vaddr = vaddr;
    s_memmap_ring[i].size = size;
    s_memmap_ring[i].caller = caller;
    s_memmap_ring[i].game_state = game_state;
}

/* Segment-map events (kind 0/1) are pushed from the existing
 * pkmnstadium_segmap_set_enter / _clear_enter handlers below — they
 * already receive game_state and now also caller — so we do NOT add a
 * second hook on Memmap_SetSegmentMap/ClearSegmentMemmap (one already
 * exists at the same vram and the recompiler rejects duplicates).
 * Only the fragment-map mutators get fresh hooks; those handlers have
 * rdram and read gCurrentGameState from it. */
void pkmnstadium_memmap_set_fragment(uint8_t* rdram, uint32_t id, uint32_t vaddr,
                                     uint32_t size, uint32_t caller) {
    memmap_ring_push(2u, id, vaddr, size, caller, MEM_LOAD_BE32(rdram, 0x00075668u));
}
void pkmnstadium_memmap_clear_fragment(uint8_t* rdram, uint32_t id, uint32_t caller) {
    memmap_ring_push(3u, id, 0u, 0u, caller, MEM_LOAD_BE32(rdram, 0x00075668u));
    /* Functional fix (not just diagnostic): mirror the game's fragment
     * clear into the recompiler's overlay table so RELOC_HI16/LO16 for
     * this fragment's section fall back to the link literal once it is
     * no longer resident. Without this the stale runtime base makes
     * fragment-id-extraction math compute a wrong (negative) id that
     * indexes gFragments[]/gSegments[] OOB — the menu cursor/icon
     * corruption. See recomp::overlays::unregister_runtime_fragment. */
    recomp_unregister_runtime_fragment(id);
}

uint64_t pkmnstadium_memmap_seq(void) {
    return __atomic_load_n(&s_memmap_seq, __ATOMIC_RELAXED);
}
uint32_t pkmnstadium_memmap_cap(void) { return MEMMAP_RING_CAP; }
void pkmnstadium_memmap_get(uint32_t i, uint64_t* seq, uint32_t* kind,
                            uint32_t* id, uint32_t* vaddr, uint32_t* size,
                            uint32_t* caller, uint32_t* gs) {
    uint32_t idx = i % MEMMAP_RING_CAP;
    *seq    = s_memmap_ring[idx].seq;
    *kind   = s_memmap_ring[idx].kind;
    *id     = s_memmap_ring[idx].id;
    *vaddr  = s_memmap_ring[idx].vaddr;
    *size   = s_memmap_ring[idx].size;
    *caller = s_memmap_ring[idx].caller;
    *gs     = s_memmap_ring[idx].game_state;
}

/* ------------------------------------------------------------------ */
/* Archive-load + fragment-relocate ring (cursor/icon corruption root).*/
/*                                                                    */
/* The memmap ring (above) PROVED the corruption: an OOB               */
/* Memmap_SetFragmentMap(id=-15) writes into gFragments[-15], which    */
/* aliases &gSegments[1] (gFragments base 0x800A58F0 - 15*8 =          */
/* 0x800A5878), resetting seg 1 from the icon texture bank to          */
/* fragment-31 CODE; the cursor then samples MIPS code -> sparkle.     */
/* The id traces back through func_800043BC(id, Fragment*) to          */
/* func_8000484C's `func_800043BC(archive->unk_02, frag)` — i.e. the   */
/* fragment id is BinArchive->unk_02 (the u16 at decompressed offset   */
/* 0x02). On real HW the same gFragments[-15] aliasing exists, so the  */
/* shipped game cannot legitimately pass -15; therefore EITHER unk_02  */
/* is corrupt in our build (decompression / wrong-source divergence),  */
/* OR the relocate branch is taken when HW would take the in-place     */
/* (unk_00 & 1) or cached (file.unk_08) branch instead.                */
/*                                                                    */
/* This always-on ring captures both ends of the chain so the          */
/* post-mortem can distinguish those root causes:                      */
/*   kind 0 = func_8000484C entry (archive consume):                   */
/*     a=archive vaddr  b=file_number                                  */
/*     hdr0=(unk_00<<16 | unk_02)   hdr1=unk_04 (compressed source)    */
/*     hdr2=total_size   hdr3=num_files                                */
/*     extra0=file[fn].offset  extra1=file[fn].size                    */
/*     extra2=file[fn].unk_08 (decompressed-cache ptr)                 */
/*   kind 1 = func_800043BC entry (relocate dispatch):                 */
/*     a=id (raw s32 that indexes gFragments)  b=Fragment vaddr        */
/*     hdr0=magic[0:4] ('FRAG')  hdr1=magic[4:8] ('MENT')              */
/*     hdr2=relocOffset  hdr3=sizeInRam                                */
/*                                                                    */
/* Capturing both unk_02 (stored, hdr0&0xFFFF on kind 0) AND id        */
/* (consumed, kind 1 a) pins whether a u16->s32 sign-extension turns a */
/* stored 0xFFF1 into -15, vs whether the stored byte itself is wrong. */
/* These are game.toml entry hooks, so rdram is librecomp's            */
/* word-swapped store -> read via MEM_LOAD_BE32 (XOR-3 indexing).      */
struct arcload_ev {
    uint64_t seq;
    uint32_t kind;
    uint32_t a;
    uint32_t b;
    uint32_t hdr0, hdr1, hdr2, hdr3;
    uint32_t extra0, extra1, extra2;
    uint32_t caller;
    uint32_t game_state;
};
#define ARCLOAD_RING_CAP 2048
static volatile struct arcload_ev s_arcload_ring[ARCLOAD_RING_CAP];
static volatile uint64_t s_arcload_seq = 0;

/* Non-evicting smoking-gun: the first relocate dispatch whose fragment
 * id is outside [0,239] (the OOB gFragments[] write), paired with the
 * BinArchive consume frame that produced it. Kept separately so ring
 * eviction can never lose the one event that matters. */
static volatile int s_badfrag_captured = 0;
static volatile struct arcload_ev s_badfrag_reloc;
static volatile struct arcload_ev s_badfrag_aload;
/* Per-thread last consume frame, so a synchronous relocate dispatch
 * (func_800043BC is called from inside func_8000484C) correlates to its
 * source archive without racing other threads. */
static __thread struct arcload_ev s_last_aload;
static __thread int s_have_last_aload = 0;

static uint64_t arcload_ring_push(uint32_t kind, uint32_t a, uint32_t b,
                                  uint32_t h0, uint32_t h1, uint32_t h2, uint32_t h3,
                                  uint32_t e0, uint32_t e1, uint32_t e2,
                                  uint32_t caller, uint32_t gs) {
    uint64_t s = __atomic_fetch_add(&s_arcload_seq, 1, __ATOMIC_RELAXED);
    uint32_t i = (uint32_t)(s % ARCLOAD_RING_CAP);
    s_arcload_ring[i].seq = s;
    s_arcload_ring[i].kind = kind;
    s_arcload_ring[i].a = a;
    s_arcload_ring[i].b = b;
    s_arcload_ring[i].hdr0 = h0;
    s_arcload_ring[i].hdr1 = h1;
    s_arcload_ring[i].hdr2 = h2;
    s_arcload_ring[i].hdr3 = h3;
    s_arcload_ring[i].extra0 = e0;
    s_arcload_ring[i].extra1 = e1;
    s_arcload_ring[i].extra2 = e2;
    s_arcload_ring[i].caller = caller;
    s_arcload_ring[i].game_state = gs;
    return s;
}

uint64_t pkmnstadium_arcload_seq(void) {
    return __atomic_load_n(&s_arcload_seq, __ATOMIC_RELAXED);
}
uint32_t pkmnstadium_arcload_cap(void) { return ARCLOAD_RING_CAP; }
void pkmnstadium_arcload_get(uint32_t i, uint64_t* seq, uint32_t* kind,
                             uint32_t* a, uint32_t* b,
                             uint32_t* h0, uint32_t* h1, uint32_t* h2, uint32_t* h3,
                             uint32_t* e0, uint32_t* e1, uint32_t* e2,
                             uint32_t* caller, uint32_t* gs) {
    uint32_t idx = i % ARCLOAD_RING_CAP;
    *seq    = s_arcload_ring[idx].seq;
    *kind   = s_arcload_ring[idx].kind;
    *a      = s_arcload_ring[idx].a;
    *b      = s_arcload_ring[idx].b;
    *h0     = s_arcload_ring[idx].hdr0;
    *h1     = s_arcload_ring[idx].hdr1;
    *h2     = s_arcload_ring[idx].hdr2;
    *h3     = s_arcload_ring[idx].hdr3;
    *e0     = s_arcload_ring[idx].extra0;
    *e1     = s_arcload_ring[idx].extra1;
    *e2     = s_arcload_ring[idx].extra2;
    *caller = s_arcload_ring[idx].caller;
    *gs     = s_arcload_ring[idx].game_state;
}

/* Returns 1 if a bad-id relocate was captured; fills the relocate event
 * plus the source-archive (consume) event it was correlated to. */
int pkmnstadium_arcload_badfrag(uint32_t* r_id, uint32_t* r_frag,
                                uint32_t* r_magic0, uint32_t* r_magic1,
                                uint32_t* r_relocoff, uint32_t* r_sizeram,
                                uint32_t* r_caller, uint32_t* r_gs,
                                uint32_t* a_archive, uint32_t* a_unk00unk02,
                                uint32_t* a_unk04, uint32_t* a_total,
                                uint32_t* a_nfiles, uint32_t* a_filenum,
                                uint32_t* a_foff, uint32_t* a_fsize,
                                uint32_t* a_funk08, uint32_t* a_caller) {
    if (!__atomic_load_n(&s_badfrag_captured, __ATOMIC_ACQUIRE)) return 0;
    *r_id       = s_badfrag_reloc.a;
    *r_frag     = s_badfrag_reloc.b;
    *r_magic0   = s_badfrag_reloc.hdr0;
    *r_magic1   = s_badfrag_reloc.hdr1;
    *r_relocoff = s_badfrag_reloc.hdr2;
    *r_sizeram  = s_badfrag_reloc.hdr3;
    *r_caller   = s_badfrag_reloc.caller;
    *r_gs       = s_badfrag_reloc.game_state;
    *a_archive     = s_badfrag_aload.a;
    *a_unk00unk02  = s_badfrag_aload.hdr0;
    *a_unk04       = s_badfrag_aload.hdr1;
    *a_total       = s_badfrag_aload.hdr2;
    *a_nfiles      = s_badfrag_aload.hdr3;
    *a_filenum     = s_badfrag_aload.b;
    *a_foff        = s_badfrag_aload.extra0;
    *a_fsize       = s_badfrag_aload.extra1;
    *a_funk08      = s_badfrag_aload.extra2;
    *a_caller      = s_badfrag_aload.caller;
    return 1;
}

/* func_800043BC(s32 id, Fragment* addr) entry. id == archive->unk_02 is
 * forwarded to Memmap_RelocateFragment -> Memmap_SetFragmentMap; an id
 * outside [0,239] is the OOB write that clobbers gSegments[]. */
void pkmnstadium_relocfrag_enter(uint8_t* rdram, uint32_t id,
                                 uint32_t fragment, uint32_t caller) {
    uint32_t fo = fragment & 0x7FFFFFu;
    uint32_t m0 = 0, m1 = 0, ro = 0, sr = 0;
    if (fragment >= 0x80000000u && (fo + 0x20u) <= 0x800000u) {
        m0 = MEM_LOAD_BE32(rdram, fo + 0x08u);  /* magic[0:4] -> 'FRAG' */
        m1 = MEM_LOAD_BE32(rdram, fo + 0x0Cu);  /* magic[4:8] -> 'MENT' */
        ro = MEM_LOAD_BE32(rdram, fo + 0x14u);  /* relocOffset */
        sr = MEM_LOAD_BE32(rdram, fo + 0x1Cu);  /* sizeInRam   */
    }
    uint32_t gs = MEM_LOAD_BE32(rdram, 0x00075668u);
    arcload_ring_push(1u, id, fragment, m0, m1, ro, sr, 0u, 0u, 0u, caller, gs);

    if (((int32_t)id < 0 || id >= 240u) &&
        !__atomic_load_n(&s_badfrag_captured, __ATOMIC_ACQUIRE)) {
        s_badfrag_reloc.kind = 1u;
        s_badfrag_reloc.a = id;
        s_badfrag_reloc.b = fragment;
        s_badfrag_reloc.hdr0 = m0;
        s_badfrag_reloc.hdr1 = m1;
        s_badfrag_reloc.hdr2 = ro;
        s_badfrag_reloc.hdr3 = sr;
        s_badfrag_reloc.caller = caller;
        s_badfrag_reloc.game_state = gs;
        if (s_have_last_aload) {
            s_badfrag_aload = s_last_aload;
        } else {
            s_badfrag_aload.a = 0u;  /* archive unknown */
        }
        __atomic_store_n(&s_badfrag_captured, 1, __ATOMIC_RELEASE);
        fprintf(stderr,
            "[arcload] BAD FRAGMENT id=0x%08X (%d) frag=0x%08X magic=%c%c%c%c%c%c%c%c "
            "sizeInRam=0x%X  <- archive=0x%08X unk_00=0x%04X unk_02=0x%04X "
            "num_files=%u unk_04=0x%08X file=%d\n",
            id, (int32_t)id, fragment,
            (char)((m0>>24)&0xFF),(char)((m0>>16)&0xFF),(char)((m0>>8)&0xFF),(char)(m0&0xFF),
            (char)((m1>>24)&0xFF),(char)((m1>>16)&0xFF),(char)((m1>>8)&0xFF),(char)(m1&0xFF),
            sr, s_badfrag_aload.a, (s_badfrag_aload.hdr0>>16)&0xFFFF,
            s_badfrag_aload.hdr0&0xFFFF, s_badfrag_aload.hdr3, s_badfrag_aload.hdr1,
            (int32_t)s_badfrag_aload.b);
        fflush(stderr);
    }
}

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

/* n_alLoadParam voice descriptor probe. Hooked just before the
 * crashing load `lw $t5, 0x8($v1)` at MIPS PC 0x8004FAE4. Logs
 * (a3, v0, v1) where v1 is the about-to-be-derefed pointer. Prints
 * to stderr when v1 falls outside RDRAM [0x80000000, 0x80800000) —
 * i.e., the case that would trip the SEH access violation. Also
 * keeps a ring of the last 64 calls so a post-mortem can show the
 * recent context (good calls preceding the bad one). */
#define VOICE_RING_CAP 64
struct voice_ev { uint32_t a3, v0, v1; };
static volatile struct voice_ev s_voice_ring[VOICE_RING_CAP];
static volatile uint64_t s_voice_seq = 0;
static __thread int s_voice_logged_bad = 0;

void pkmnstadium_audio_voice_check(uint32_t a3, uint32_t v0, uint32_t v1) {
    uint64_t s = s_voice_seq++;
    s_voice_ring[s % VOICE_RING_CAP].a3 = a3;
    s_voice_ring[s % VOICE_RING_CAP].v0 = v0;
    s_voice_ring[s % VOICE_RING_CAP].v1 = v1;
    /* RDRAM is [0x80000000, 0x80800000). Flag anything else. */
    if ((v1 < 0x80000000u || v1 >= 0x80800000u) && !s_voice_logged_bad) {
        s_voice_logged_bad = 1;
        fprintf(stderr,
            "[voice_check] BAD v1=0x%08X  a3=0x%08X  v0=0x%08X  seq=%llu\n",
            v1, a3, v0, (unsigned long long)s);
        fflush(stderr);
    }
}

/* n_alAdpcmPull aClearBuffer-overflow probe.
 *
 * Hooked at MIPS PC 0x8004F908 — the START of the inlined
 * `aClearBuffer(ptr++, startZero + *outp, nOver << 1)` block in
 * n_alAdpcmPull. At this point:
 *   $s0 (r16) = filter (N_PVoice*)
 *   $s2 (r18) = nOver        (samples to clear)
 *   $v1 (r3)  = startZero    (s32, may be 0 if !decoded)
 *   $t3 (r11) = outp         (s16* pointer)
 *
 * The captured cmd buffer (audio_task_corrupt.bin from a 2026-05-06
 * Press-Start-past-title repro) contains CLEARBUFF cmds with
 * count = nOver << 1 ≈ 0x0E58XXXX (~120M samples worth) emitted by
 * specific voice slots. The post-task DMEM check then catches a
 * zeroed dispatch table because the RSP handler (L_119C) clears
 * 24800 bytes via DMEM addressing that wraps every 0x1000.
 *
 * Across 3 captured frames, the same 4 voice slots are pathological;
 * d shrinks by ~0x240 and c grows by ~0x240 per frame, consistent
 * with f->dc_memin accumulating past the sample's end.
 *
 * Goal: identify which N_PVoice (and which dc_table) is in this
 * runaway state. On first observation of nOver > 0x400, dump the
 * full N_PVoice + ALWaveTable so we can see which field is wrong.
 *
 * N_PVoice layout (n_synthInternals.h, verified against recomp):
 *   0x00 ALLink node (8 bytes)
 *   0x08 vvoice*
 *   0x0C dc_state*
 *   0x10 dc_lstate*
 *   0x14 dc_loop.start  (s32)
 *   0x18 dc_loop.end    (s32)
 *   0x1C dc_loop.count  (s32)
 *   0x20 dc_table*  (ALWaveTable*)
 *   0x24 dc_bookSize
 *   0x28 dc_dma func ptr
 *   0x2C dc_dmaState
 *   0x30 dc_sample
 *   0x34 dc_lastsam
 *   0x38 dc_first
 *   0x3C dc_memin
 *
 * ALWaveTable layout (libaudio.h):
 *   0x00 base*
 *   0x04 len   (s32)
 *   0x08 type, flags (u8 each)
 *   0x0C waveInfo.adpcmWave.loop*
 *   0x10 waveInfo.adpcmWave.book*
 */
/* Forward decl — defined further down. */
static void pkmnstadium_wt_dump_for_filter(uint32_t filter_vaddr);

void pkmnstadium_adpcm_overflow_dump(uint8_t* rdram,
                                     uint32_t filter_vaddr,
                                     uint32_t nOver,
                                     uint32_t startZero,
                                     uint32_t outp_vaddr) {
    /* Threshold: a normal frame is FIXED_SAMPLE=184 samples, so a
     * realistic nOver is at most a few hundred. 0x400 (1024) is a
     * generous "definitely too big" threshold. */
    if (nOver <= 0x400u) return;
    /* Throttle: dump up to N distinct filter pointers, then go silent.
     * We want to see whether multiple voices are pathological — if it's
     * always the same N_PVoice, the bug is voice-specific; if different
     * filters appear across runs, the bug is at the SET_WAVETABLE
     * upstream call. */
    enum { DUMP_CAP = 8 };
    static uint32_t s_dumped_filters[DUMP_CAP];
    static int s_dumped_count = 0;
    for (int i = 0; i < s_dumped_count; i++) {
        if (s_dumped_filters[i] == filter_vaddr) return;
    }
    if (s_dumped_count < DUMP_CAP) {
        s_dumped_filters[s_dumped_count++] = filter_vaddr;
    } else {
        return;
    }

    fprintf(stderr,
        "[adpcm-overflow] filter=0x%08X nOver=0x%X startZero=0x%X outp=0x%08X\n",
        filter_vaddr, nOver, startZero, outp_vaddr);
    /* Show the matching SET_WAVETABLE history so we can trace this
     * voice's recent wavetable assignments. */
    pkmnstadium_wt_dump_for_filter(filter_vaddr);

    /* Read *outp (s16, sign-extended in the recomp). */
    if (outp_vaddr >= 0x80000000u && outp_vaddr < 0x80800000u) {
        uint32_t outp_paddr = outp_vaddr & 0x1FFFFFFFu;
        int16_t outp_value = (int16_t)(
            ((uint16_t)rdram[(outp_paddr + 0) ^ 3] << 8) |
            ((uint16_t)rdram[(outp_paddr + 1) ^ 3]));
        fprintf(stderr, "  *outp = %d (0x%04X)\n",
            (int)outp_value, (unsigned)(uint16_t)outp_value);
    }

    if (filter_vaddr >= 0x80000000u && filter_vaddr < 0x80800000u) {
        uint32_t f_paddr = filter_vaddr & 0x1FFFFFFFu;
        uint32_t loop_start = MEM_LOAD_BE32(rdram, f_paddr + 0x14);
        uint32_t loop_end   = MEM_LOAD_BE32(rdram, f_paddr + 0x18);
        uint32_t loop_count = MEM_LOAD_BE32(rdram, f_paddr + 0x1C);
        uint32_t dc_table   = MEM_LOAD_BE32(rdram, f_paddr + 0x20);
        uint32_t dc_bookSz  = MEM_LOAD_BE32(rdram, f_paddr + 0x24);
        uint32_t dc_sample  = MEM_LOAD_BE32(rdram, f_paddr + 0x30);
        uint32_t dc_lastsam = MEM_LOAD_BE32(rdram, f_paddr + 0x34);
        uint32_t dc_first   = MEM_LOAD_BE32(rdram, f_paddr + 0x38);
        uint32_t dc_memin   = MEM_LOAD_BE32(rdram, f_paddr + 0x3C);
        uint32_t dc_state   = MEM_LOAD_BE32(rdram, f_paddr + 0x0C);
        fprintf(stderr,
            "  N_PVoice fields:\n"
            "    dc_state    = 0x%08X\n"
            "    dc_loop.start  = 0x%08X (%d)\n"
            "    dc_loop.end    = 0x%08X (%d)\n"
            "    dc_loop.count  = 0x%08X (%d)\n"
            "    dc_table    = 0x%08X\n"
            "    dc_bookSize = 0x%08X\n"
            "    dc_sample   = 0x%08X (%d)\n"
            "    dc_lastsam  = 0x%08X (%d)\n"
            "    dc_first    = 0x%08X\n"
            "    dc_memin    = 0x%08X (%d)\n",
            dc_state,
            loop_start, (int32_t)loop_start,
            loop_end,   (int32_t)loop_end,
            loop_count, (int32_t)loop_count,
            dc_table, dc_bookSz,
            dc_sample, (int32_t)dc_sample,
            dc_lastsam, (int32_t)dc_lastsam,
            dc_first,
            dc_memin, (int32_t)dc_memin);

        /* Dump dc_table struct if pointer is in RAM. ROM-resident
         * tables (kseg1 at 0xB0...) are not currently supported by
         * this dump because rdram only covers DRAM. */
        if (dc_table >= 0x80000000u && dc_table < 0x80800000u) {
            uint32_t t_paddr = dc_table & 0x1FFFFFFFu;
            uint32_t t_base = MEM_LOAD_BE32(rdram, t_paddr + 0x00);
            uint32_t t_len  = MEM_LOAD_BE32(rdram, t_paddr + 0x04);
            uint32_t t_type_flags = MEM_LOAD_BE32(rdram, t_paddr + 0x08);
            uint32_t t_loop = MEM_LOAD_BE32(rdram, t_paddr + 0x0C);
            uint32_t t_book = MEM_LOAD_BE32(rdram, t_paddr + 0x10);
            fprintf(stderr,
                "  dc_table fields:\n"
                "    base       = 0x%08X\n"
                "    len        = 0x%08X (%d)\n"
                "    type/flags = 0x%08X\n"
                "    .loop      = 0x%08X\n"
                "    .book      = 0x%08X\n"
                "    sample_end = base+len = 0x%08X\n"
                "    memin_overshoot = 0x%X (memin past end)\n",
                t_base, t_len, (int32_t)t_len, t_type_flags,
                t_loop, t_book,
                t_base + t_len,
                (uint32_t)(dc_memin - (t_base + t_len)));
        } else {
            fprintf(stderr,
                "  dc_table = 0x%08X (not in RDRAM, possibly ROM/kseg1)\n",
                dc_table);
        }
    }
    fflush(stderr);
}

/* n_alLoadParam(SET_WAVETABLE) call probe.
 *
 * Hooked at the function entry (PC 0x8004F95C). Captures
 * (filter=$a0, paramID=$a1, param=$a2) for every n_alLoadParam call
 * with paramID == AL_FILTER_SET_WAVETABLE (5). The ring stores the
 * last N calls so [adpcm-overflow] can match a corrupt voice back to
 * the SET_WAVETABLE call that gave it its (bad) wavetable.
 *
 * Note on "valid" wavetable contents: Stadium uses ROM-offset base
 * pointers (e.g. base=0x01928ADC) — sound bank wavetables stored in
 * the bank's loaded copy reference sample data still resident in
 * ROM. So "base must be a kseg0/kseg1 pointer" is WRONG; we accept
 * ROM offsets too. We don't pre-filter here — every SET_WAVETABLE
 * call goes into the ring, and the dump is only taken on demand
 * from pkmnstadium_adpcm_overflow_dump.
 */
#define WAVETABLE_RING_CAP 64
struct wavetable_ev {
    uint64_t seq;
    uint32_t filter;
    uint32_t param;
    uint32_t first8_hi;  /* base */
    uint32_t first8_lo;  /* len */
    uint32_t second8_hi; /* type/flags packed */
    uint32_t second8_lo; /* loop ptr */
};
static volatile struct wavetable_ev s_wt_ring[WAVETABLE_RING_CAP];
static volatile uint64_t s_wt_seq = 0;

void pkmnstadium_set_wavetable_probe(uint8_t* rdram,
                                     uint32_t filter,
                                     uint32_t paramID,
                                     uint32_t param) {
    if (paramID != 5u) return;
    uint64_t s = s_wt_seq++;
    int idx = (int)(s % WAVETABLE_RING_CAP);
    s_wt_ring[idx].seq    = s;
    s_wt_ring[idx].filter = filter;
    s_wt_ring[idx].param  = param;
    s_wt_ring[idx].first8_hi  = 0;
    s_wt_ring[idx].first8_lo  = 0;
    s_wt_ring[idx].second8_hi = 0;
    s_wt_ring[idx].second8_lo = 0;
    if (param >= 0x80000000u && param < 0x80800000u) {
        uint32_t paddr = param & 0x1FFFFFFFu;
        s_wt_ring[idx].first8_hi  = MEM_LOAD_BE32(rdram, paddr + 0);
        s_wt_ring[idx].first8_lo  = MEM_LOAD_BE32(rdram, paddr + 4);
        s_wt_ring[idx].second8_hi = MEM_LOAD_BE32(rdram, paddr + 8);
        s_wt_ring[idx].second8_lo = MEM_LOAD_BE32(rdram, paddr + 12);
    }
}

/* Dump the wavetable ring's entries that match `filter`. Caller
 * holds the ring's "live" state lightly (no locking; readers may
 * see partial writes). Used by pkmnstadium_adpcm_overflow_dump to
 * trace a runaway voice back to its SET_WAVETABLE history. */
static void pkmnstadium_wt_dump_for_filter(uint32_t filter_vaddr) {
    int n = 0;
    fprintf(stderr,
        "  SET_WAVETABLE history for filter=0x%08X:\n", filter_vaddr);
    /* Walk newest-first. */
    uint64_t cur_seq = s_wt_seq;
    for (uint64_t back = 0; back < WAVETABLE_RING_CAP && back < cur_seq; back++) {
        uint64_t s = cur_seq - 1 - back;
        int idx = (int)(s % WAVETABLE_RING_CAP);
        if (s_wt_ring[idx].filter != filter_vaddr) continue;
        if (s_wt_ring[idx].seq != s) continue;
        fprintf(stderr,
            "    seq=%llu param=0x%08X first16=%08X %08X %08X %08X\n",
            (unsigned long long)s, s_wt_ring[idx].param,
            s_wt_ring[idx].first8_hi, s_wt_ring[idx].first8_lo,
            s_wt_ring[idx].second8_hi, s_wt_ring[idx].second8_lo);
        if (++n >= 4) break;
    }
    if (n == 0) {
        fprintf(stderr, "    (no SET_WAVETABLE calls for this filter in ring)\n");
    }
}

/* main_pool_pop_state caller probe.
 *
 * The post-title audio UAF (2026-05-06) traced to a 1 MiB pool alloc
 * landing at 0x8018A1B0 — overlapping wavetable structs that voices
 * still pointed at. For the bump-allocator to land there again, a
 * main_pool_pop_state must have run between the original
 * SET_WAVETABLE and the load2 alloc. This probe logs every pop's
 * caller PC + gCurrentGameState + pool-top before/after so the
 * post-mortem shows which Stadium function freed the bank-resident
 * memory without first stopping the voices.
 *
 * Hook fires at function entry (PC 0x80002838). At entry:
 *   $a0 = arg (push/pop state's arg byte; 0 = unconditional, else
 *               must match a tag in the saved state)
 *   $ra = caller PC
 * sMemPool fields read from rdram at vaddr 0x800A6070 + offsets:
 *   +0x1C available, +0x28 listHeadL, +0x2C listHeadR, +0x30 mainState
 * MainPoolState fields at *mainState:
 *   +0x00 freeSpace, +0x04 listHeadL, +0x08 listHeadR, +0x0C prev
 * gCurrentGameState lives at vaddr 0x80075668.
 *
 * Always-on; logs every pop. Output volume is bounded by the actual
 * number of pops (not many). */
void pkmnstadium_pool_pop_log(uint8_t* rdram,
                              uint32_t arg,
                              uint32_t caller_pc) {
    uint32_t pool_paddr = 0x000A6070u;
    uint32_t avail   = MEM_LOAD_BE32(rdram, pool_paddr + 0x1C);
    uint32_t headL   = MEM_LOAD_BE32(rdram, pool_paddr + 0x28);
    uint32_t headR   = MEM_LOAD_BE32(rdram, pool_paddr + 0x2C);
    uint32_t state_p = MEM_LOAD_BE32(rdram, pool_paddr + 0x30);
    uint32_t game_state = MEM_LOAD_BE32(rdram, 0x00075668u);

    uint32_t s_free = 0, s_lL = 0, s_lR = 0;
    if (state_p >= 0x80000000u && state_p < 0x80800000u) {
        uint32_t sp = state_p & 0x1FFFFFFFu;
        s_free = MEM_LOAD_BE32(rdram, sp + 0x00);
        s_lL   = MEM_LOAD_BE32(rdram, sp + 0x04);
        s_lR   = MEM_LOAD_BE32(rdram, sp + 0x08);
    }
    fprintf(stderr,
        "[pool-pop] arg=0x%X caller=0x%08X gs=0x%08X avail=0x%X "
        "live(L=0x%08X R=0x%08X) pop_to(state=0x%08X free=0x%X "
        "L=0x%08X R=0x%08X)\n",
        arg, caller_pc, game_state, avail,
        headL, headR, state_p, s_free, s_lL, s_lR);
    fflush(stderr);
}

/* n_alFxPull / n_alMainBusPull diagnostic ring for the quick-battle
 * NULL+0xBC crash 2026-05-03. The crash is a write through $v0=0xB8
 * inside n_alMainBusPull right after `jalr $t9` to the bus filter
 * handler (resolved via RDRAM dump to be n_alFxPull at 0x800500C0).
 *
 * Hooks:
 *   - n_alFxPull entry (0x800500C0): record (a0=sampleOffset, a1=ptr)
 *   - n_alFxPull exit  (0x80050398, the `jr $ra`): record (v0=ret)
 *     and stderr-shout if v0 < 0x80000000 (caught the bad-return)
 *   - n_alMainBusPull pre-crash (0x80050D14, the `sw $s7, 0x4($v0)`):
 *     record (v0, s7, a1) and stderr-shout if v0 < 0x80000000
 *
 * Each ring keeps the last 32 events so a post-mortem after a crash
 * shows the recent context (good calls preceding the bad one). */
#define AFX_RING_CAP 32
struct afx_ev { uint32_t a, b, c; };
static volatile struct afx_ev s_afx_in_ring[AFX_RING_CAP];
static volatile struct afx_ev s_afx_out_ring[AFX_RING_CAP];
static volatile struct afx_ev s_mbp_pre_ring[AFX_RING_CAP];
static volatile uint64_t s_afx_in_seq = 0, s_afx_out_seq = 0, s_mbp_pre_seq = 0;
static __thread int s_afx_logged_bad = 0;
static __thread int s_mbp_logged_bad = 0;

void pkmnstadium_afxpull_enter(uint32_t a0, uint32_t a1) {
    uint64_t s = s_afx_in_seq++;
    s_afx_in_ring[s % AFX_RING_CAP].a = a0;
    s_afx_in_ring[s % AFX_RING_CAP].b = a1;
    s_afx_in_ring[s % AFX_RING_CAP].c = (uint32_t)(s & 0xFFFFFFFF);
}

void pkmnstadium_afxpull_exit(uint32_t v0, uint32_t s0) {
    uint64_t s = s_afx_out_seq++;
    s_afx_out_ring[s % AFX_RING_CAP].a = v0;
    s_afx_out_ring[s % AFX_RING_CAP].b = s0;
    s_afx_out_ring[s % AFX_RING_CAP].c = (uint32_t)(s & 0xFFFFFFFF);
    if ((v0 < 0x80000000u || v0 >= 0x80800000u) && !s_afx_logged_bad) {
        s_afx_logged_bad = 1;
        fprintf(stderr,
            "[afxpull_exit] BAD v0=0x%08X  s0=0x%08X  seq=%llu  "
            "(expected Acmd* in [0x80000000, 0x80800000))\n",
            v0, s0, (unsigned long long)s);
        fflush(stderr);
    }
}

void pkmnstadium_mbp_precrash(uint32_t v0, uint32_t s7, uint32_t a1) {
    uint64_t s = s_mbp_pre_seq++;
    s_mbp_pre_ring[s % AFX_RING_CAP].a = v0;
    s_mbp_pre_ring[s % AFX_RING_CAP].b = s7;
    s_mbp_pre_ring[s % AFX_RING_CAP].c = a1;
    if ((v0 < 0x80000000u || v0 >= 0x80800000u) && !s_mbp_logged_bad) {
        s_mbp_logged_bad = 1;
        fprintf(stderr,
            "[mbp_precrash] BAD v0=0x%08X  s7=0x%08X  a1=0x%08X  seq=%llu  "
            "(about to sw $s7,0x4($v0) and SEH)\n",
            v0, s7, a1, (unsigned long long)s);
        fflush(stderr);
    }
}

/* Bus dispatch in n_alMainBusPull: who actually gets called?
 * 0x80050CF4: jalr $t9 → handler(rdram, ctx); after, $v0 has return.
 * mbp_dispatch fires BEFORE the call: $t8 = n_syn ptr, $t9 = handler.
 * mbp_dispatch_ret fires AFTER the call: $v0 = handler's return.
 * Both conditional-shout if the handler ptr or return is implausible. */
struct dispatch_ev { uint32_t t8, t9, v0; };
static volatile struct dispatch_ev s_mbp_disp_ring[AFX_RING_CAP];
static volatile uint64_t s_mbp_disp_seq = 0;
static __thread int s_mbp_disp_logged_bad = 0;

void pkmnstadium_mbp_dispatch(uint32_t t8, uint32_t t9) {
    uint64_t s = s_mbp_disp_seq++;
    uint32_t idx = s % AFX_RING_CAP;
    s_mbp_disp_ring[idx].t8 = t8;
    s_mbp_disp_ring[idx].t9 = t9;
    s_mbp_disp_ring[idx].v0 = 0xDEADBEEFu;  /* will be overwritten by _ret */
    if ((t9 < 0x80000000u || t9 >= 0x80800000u) && !s_mbp_disp_logged_bad) {
        s_mbp_disp_logged_bad = 1;
        fprintf(stderr,
            "[mbp_dispatch] BAD handler t9=0x%08X  n_syn=0x%08X  seq=%llu  "
            "(about to jalr to garbage)\n",
            t9, t8, (unsigned long long)s);
        fflush(stderr);
    }
}

void pkmnstadium_mbp_dispatch_ret(uint32_t v0) {
    /* Most recent dispatch entry is at (s_mbp_disp_seq - 1) % CAP. */
    uint64_t s = s_mbp_disp_seq;
    if (s == 0) return;
    uint32_t idx = (s - 1) % AFX_RING_CAP;
    s_mbp_disp_ring[idx].v0 = v0;
    if ((v0 < 0x80000000u || v0 >= 0x80800000u) && !s_mbp_disp_logged_bad) {
        s_mbp_disp_logged_bad = 1;
        fprintf(stderr,
            "[mbp_dispatch_ret] BAD v0=0x%08X  handler t9=0x%08X  seq=%llu  "
            "(handler returned non-RDRAM ptr)\n",
            v0, s_mbp_disp_ring[idx].t9, (unsigned long long)(s - 1));
        fflush(stderr);
    }
}

/* n_alMainBusPull entry/exit probes — recursion-depth tracker.
 *
 * The dispatch site at 0x80050CF4 always shows t9=0x800500C0 (= n_alMainBusPull
 * itself) per the mbp_disp ring. So this is a recursive-descent dispatcher.
 * At seq 3436 it returned v0=0xB8, which can only be a recursive frame's
 * end-state. We need to see HOW DEEP the recursion goes and WHAT each frame
 * returns to triangulate which frame produces the bogus pointer first.
 *
 * Entry probe stamps (depth, a0, a1) and increments depth.
 * Exit probe stamps (depth, v0_out, s0_out) and decrements depth.
 * Both stamp the same seq counter so we can correlate back to mbp_disp seq.
 */
struct mbp_chain_ev {
    uint32_t depth_in;       /* depth at entry (1 = outermost) */
    uint32_t a0_in;
    uint32_t a1_in;
    uint32_t v0_in;          /* $v0 inherited from caller */
    uint32_t depth_out;      /* depth at exit (matches entry depth) */
    uint32_t v0_out;
    uint32_t s0_out;
    uint32_t kind;           /* 0 = entry, 1 = exit */
};
#define MBP_CHAIN_RING_CAP 256
static volatile struct mbp_chain_ev s_mbp_chain_ring[MBP_CHAIN_RING_CAP];
static volatile uint64_t s_mbp_chain_seq = 0;
static __thread int s_mbp_depth = 0;
static __thread int s_mbp_chain_logged_bad = 0;

void pkmnstadium_mbp_entry(uint32_t a0, uint32_t a1, uint32_t v0) {
    s_mbp_depth++;
    uint64_t s = s_mbp_chain_seq++;
    uint32_t idx = s % MBP_CHAIN_RING_CAP;
    s_mbp_chain_ring[idx].kind = 0;
    s_mbp_chain_ring[idx].depth_in = (uint32_t)s_mbp_depth;
    s_mbp_chain_ring[idx].a0_in = a0;
    s_mbp_chain_ring[idx].a1_in = a1;
    s_mbp_chain_ring[idx].v0_in = v0;
    s_mbp_chain_ring[idx].depth_out = 0;
    s_mbp_chain_ring[idx].v0_out = 0;
    s_mbp_chain_ring[idx].s0_out = 0;
}

void pkmnstadium_mbp_exit(uint32_t v0, uint32_t s0) {
    uint64_t s = s_mbp_chain_seq++;
    uint32_t idx = s % MBP_CHAIN_RING_CAP;
    s_mbp_chain_ring[idx].kind = 1;
    s_mbp_chain_ring[idx].depth_in = 0;
    s_mbp_chain_ring[idx].a0_in = 0;
    s_mbp_chain_ring[idx].a1_in = 0;
    s_mbp_chain_ring[idx].v0_in = 0;
    s_mbp_chain_ring[idx].depth_out = (uint32_t)s_mbp_depth;
    s_mbp_chain_ring[idx].v0_out = v0;
    s_mbp_chain_ring[idx].s0_out = s0;
    if ((v0 < 0x80000000u || v0 >= 0x80800000u) && !s_mbp_chain_logged_bad) {
        s_mbp_chain_logged_bad = 1;
        fprintf(stderr,
            "[mbp_chain] BAD exit v0=0x%08X s0=0x%08X depth=%d seq=%llu\n",
            v0, s0, s_mbp_depth, (unsigned long long)s);
        fflush(stderr);
    }
    s_mbp_depth--;
    if (s_mbp_depth < 0) s_mbp_depth = 0;
}

uint64_t pkmnstadium_mbp_chain_seq(void) { return s_mbp_chain_seq; }
uint32_t pkmnstadium_mbp_chain_cap(void) { return MBP_CHAIN_RING_CAP; }
void pkmnstadium_mbp_chain_get(uint32_t i, uint32_t* kind,
                               uint32_t* depth_in, uint32_t* a0, uint32_t* a1,
                               uint32_t* v0_in, uint32_t* depth_out,
                               uint32_t* v0_out, uint32_t* s0_out) {
    uint32_t idx = i % MBP_CHAIN_RING_CAP;
    *kind = s_mbp_chain_ring[idx].kind;
    *depth_in = s_mbp_chain_ring[idx].depth_in;
    *a0 = s_mbp_chain_ring[idx].a0_in;
    *a1 = s_mbp_chain_ring[idx].a1_in;
    *v0_in = s_mbp_chain_ring[idx].v0_in;
    *depth_out = s_mbp_chain_ring[idx].depth_out;
    *v0_out = s_mbp_chain_ring[idx].v0_out;
    *s0_out = s_mbp_chain_ring[idx].s0_out;
}

/* G_DL CALL target snapshot rings — distinguish wrong-pointer (A) vs
 * submit-too-early race (B) for the white-screen RT64 freeze.
 *
 * SUBMIT ring: filled by pokestadium_gdl_submit_snapshot, called once
 *   per top-level DL submission (from RT64Context::send_dl) for each
 *   G_DL push=1 (CALL) target found while walking the top-level DL.
 *   Captures (submit_seq, target_vaddr, head_bytes[16]) — what the
 *   target buffer looks like AT SUBMIT TIME.
 *
 * WALK ring: filled by pokestadium_gdl_walk_snapshot, called from
 *   RT64::Interpreter::processDisplayLists at the moment of each
 *   G_DL push=1 walk. Captures the same target's head bytes AS THE
 *   INTERPRETER SEES THEM.
 *
 * Post-mortem comparison: if submit_bytes == walk_bytes for a given
 * (submit_seq, target_vaddr) pair AND those bytes don't look like a
 * sane DL, that's hypothesis A (wrong-pointer / Stadium emitted G_DL
 * pointing into a non-DL buffer). If submit_bytes != walk_bytes,
 * Stadium overwrote the buffer between submit and walk — hypothesis B
 * (submit-too-early race).
 */
struct gdl_snap_ev {
    uint64_t submit_seq;     /* Stadium send_dl_gfx counter at submit time */
    uint32_t target_vaddr;   /* the G_DL target (kseg0 vaddr from w1) */
    uint32_t parent_vaddr;   /* the G_DL cmd's own vaddr (where it lives) */
    uint8_t  head[16];       /* first 16 bytes at target */
};
#define GDL_SNAP_RING_CAP 1024
static volatile struct gdl_snap_ev s_gdl_submit_ring[GDL_SNAP_RING_CAP];
static volatile uint64_t s_gdl_submit_seq = 0;
static volatile struct gdl_snap_ev s_gdl_walk_ring[GDL_SNAP_RING_CAP];
static volatile uint64_t s_gdl_walk_seq = 0;

/* Walk a top-level DL starting at `dl_vaddr` (kseg0). For each G_DL
 * with push=1 (CALL), snapshot the first 16 bytes at the target.
 * Stops on G_ENDDL or after MAX_CMDS. `rdram` is the host base ptr
 * to RDRAM (from RT64Context::send_dl: app->core.RDRAM).
 *
 * `submit_seq` is provided by the caller (= Stadium's send_dl_gfx
 * counter) so submit and walk events can be correlated.
 */
void pkmnstadium_gdl_submit_snapshot(uint8_t* rdram, uint32_t dl_vaddr,
                                     uint64_t submit_seq) {
    if (rdram == NULL) return;
    uint32_t dl_off = dl_vaddr & 0x7FFFFF;
    enum { MAX_CMDS = 8192 };
    for (uint32_t i = 0; i < MAX_CMDS; i++) {
        uint32_t off = dl_off + i * 8;
        if (off + 8 > 0x800000) break;
        /* RDRAM stores words in the host byte order used by RT64's
         * DisplayList view, so decode the command words little-endian. */
        uint32_t w0 = ((uint32_t)rdram[off]) |
                      ((uint32_t)rdram[off + 1] << 8) |
                      ((uint32_t)rdram[off + 2] << 16) |
                      ((uint32_t)rdram[off + 3] << 24);
        uint32_t w1 = ((uint32_t)rdram[off + 4]) |
                      ((uint32_t)rdram[off + 5] << 8) |
                      ((uint32_t)rdram[off + 6] << 16) |
                      ((uint32_t)rdram[off + 7] << 24);
        uint8_t op = (w0 >> 24) & 0xFF;
        if (op == 0xDF) break;  /* G_ENDDL */
        if (op == 0xDE) {
            uint8_t push = (w0 >> 16) & 0xFF;
            if (push == 1) {
                /* CALL — snapshot target's first 16 bytes. */
                uint32_t tgt_off = w1 & 0x7FFFFF;
                if (tgt_off + 16 <= 0x800000) {
                    uint64_t s = s_gdl_submit_seq++;
                    uint32_t idx = s % GDL_SNAP_RING_CAP;
                    s_gdl_submit_ring[idx].submit_seq = submit_seq;
                    s_gdl_submit_ring[idx].target_vaddr = w1;
                    s_gdl_submit_ring[idx].parent_vaddr = 0x80000000u | off;
                    for (int k = 0; k < 16; k++) {
                        s_gdl_submit_ring[idx].head[k] = rdram[tgt_off + k];
                    }
                }
            }
            /* push=0 BRANCH — don't follow (would diverge); just continue. */
        }
    }
}

/* Snapshot taken from RT64::Interpreter when it walks a G_DL push=1.
 * `head_ptr` is host-side ptr to the target's first byte.
 */
void pkmnstadium_gdl_walk_snapshot(uint32_t target_vaddr,
                                   uint32_t parent_vaddr,
                                   const uint8_t* head_ptr,
                                   uint64_t submit_seq) {
    if (head_ptr == NULL) return;
    uint64_t s = s_gdl_walk_seq++;
    uint32_t idx = s % GDL_SNAP_RING_CAP;
    s_gdl_walk_ring[idx].submit_seq = submit_seq;
    s_gdl_walk_ring[idx].target_vaddr = target_vaddr;
    s_gdl_walk_ring[idx].parent_vaddr = parent_vaddr;
    for (int k = 0; k < 16; k++) {
        s_gdl_walk_ring[idx].head[k] = head_ptr[k];
    }
}

uint64_t pkmnstadium_gdl_submit_seq(void) { return s_gdl_submit_seq; }
uint64_t pkmnstadium_gdl_walk_seq(void)   { return s_gdl_walk_seq; }
uint32_t pkmnstadium_gdl_ring_cap(void)   { return GDL_SNAP_RING_CAP; }
void pkmnstadium_gdl_submit_get(uint32_t i, uint64_t* submit_seq,
                                uint32_t* target_vaddr, uint32_t* parent_vaddr,
                                uint8_t out_head[16]) {
    uint32_t idx = i % GDL_SNAP_RING_CAP;
    *submit_seq = s_gdl_submit_ring[idx].submit_seq;
    *target_vaddr = s_gdl_submit_ring[idx].target_vaddr;
    *parent_vaddr = s_gdl_submit_ring[idx].parent_vaddr;
    for (int k = 0; k < 16; k++) out_head[k] = s_gdl_submit_ring[idx].head[k];
}
void pkmnstadium_gdl_walk_get(uint32_t i, uint64_t* submit_seq,
                              uint32_t* target_vaddr, uint32_t* parent_vaddr,
                              uint8_t out_head[16]) {
    uint32_t idx = i % GDL_SNAP_RING_CAP;
    *submit_seq = s_gdl_walk_ring[idx].submit_seq;
    *target_vaddr = s_gdl_walk_ring[idx].target_vaddr;
    *parent_vaddr = s_gdl_walk_ring[idx].parent_vaddr;
    for (int k = 0; k < 16; k++) out_head[k] = s_gdl_walk_ring[idx].head[k];
}

/* Public getters so post_mortem can serialize the rings as JSON. */
uint64_t pkmnstadium_afx_in_seq(void)   { return s_afx_in_seq; }
uint64_t pkmnstadium_afx_out_seq(void)  { return s_afx_out_seq; }
uint64_t pkmnstadium_mbp_pre_seq(void)  { return s_mbp_pre_seq; }
uint64_t pkmnstadium_mbp_disp_seq(void) { return s_mbp_disp_seq; }
uint32_t pkmnstadium_afx_ring_cap(void) { return AFX_RING_CAP; }

void pkmnstadium_afx_in_get(uint32_t i, uint32_t* a, uint32_t* b, uint32_t* c) {
    *a = s_afx_in_ring[i % AFX_RING_CAP].a;
    *b = s_afx_in_ring[i % AFX_RING_CAP].b;
    *c = s_afx_in_ring[i % AFX_RING_CAP].c;
}
void pkmnstadium_afx_out_get(uint32_t i, uint32_t* a, uint32_t* b, uint32_t* c) {
    *a = s_afx_out_ring[i % AFX_RING_CAP].a;
    *b = s_afx_out_ring[i % AFX_RING_CAP].b;
    *c = s_afx_out_ring[i % AFX_RING_CAP].c;
}
void pkmnstadium_mbp_pre_get(uint32_t i, uint32_t* a, uint32_t* b, uint32_t* c) {
    *a = s_mbp_pre_ring[i % AFX_RING_CAP].a;
    *b = s_mbp_pre_ring[i % AFX_RING_CAP].b;
    *c = s_mbp_pre_ring[i % AFX_RING_CAP].c;
}
void pkmnstadium_mbp_disp_get(uint32_t i, uint32_t* t8, uint32_t* t9, uint32_t* v0) {
    *t8 = s_mbp_disp_ring[i % AFX_RING_CAP].t8;
    *t9 = s_mbp_disp_ring[i % AFX_RING_CAP].t9;
    *v0 = s_mbp_disp_ring[i % AFX_RING_CAP].v0;
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

/* DELETED 2026-05-23: pkmnstadium_force_expansion_ram was a hook on
 * Util_InitMainPools entry that wrote gExpansionRAMStart=1 once before
 * the global was read. Moved to src/main/main.cpp's GameEntry
 * on_init_callback — runtime-init facts belong at the game-init layer,
 * not as a recompile-time hook on a specific MIPS function. */

/* main_pool_alloc(size, side) diagnostic. Logs every call's
 * (size, result) plus the running sMemPool.available so we can
 * see when/why the pool exhausts. sMemPool is the Stadium static
 * MainPool struct at 0x800A6070, with available field at +0x1C. */

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

/* __amDMA entry diagnostic — log addr/len/D_800FC820 flag state.
 * Helps figure out whether the OOB DMAs are coming from ROM-path
 * calls (correct path, but with bad addr) or RAM-path-supposed-to-
 * be-RAM calls (where the flag isn't set when it should be). */
void pkmnstadium_amdma_log(uint8_t* rdram, uint32_t addr, uint32_t len) {
    static int s_n = 0;
    s_n++;
    /* Read D_800FC820 from RAM. */
    uint32_t flag_paddr = 0x000FC820u;
    uint32_t flag =
        ((uint32_t)rdram[(flag_paddr + 0) ^ 3] << 24) |
        ((uint32_t)rdram[(flag_paddr + 1) ^ 3] << 16) |
        ((uint32_t)rdram[(flag_paddr + 2) ^ 3] <<  8) |
        ((uint32_t)rdram[(flag_paddr + 3) ^ 3]);
    /* Suspect: addr looks like it'll go OOB on the ROM path
     * (rom_base = 0x10000000, rom_size = 0x2000000, so any phys
     *  > 0x12000000 is OOB). */
    int suspect = (addr > 0x02000000u) || ((flag & 0x80000000u) == 0 && addr >= 0x80000000u);
    if (s_n <= 16 || suspect) {
        if (s_n <= 256) {  /* hard cap to avoid spam */
            fprintf(stderr,
                "[amdma] #%d addr=0x%08X len=0x%X D_800FC820=0x%08X (RAM_path=%d)%s\n",
                s_n, addr, len, flag, (flag & 0x80000000u) ? 1 : 0,
                suspect ? " <-- SUSPECT" : "");
            fflush(stderr);
        }
    }
}

/* func_8003BD2C SoundBank loader diagnostic.
 *
 * Audio crashes in __amDMA reading from ROM addresses past end of
 * cart (e.g. 0x18C73F84 vs ROM size 0x2000000). Those addresses
 * come from wave_list[i].base after func_8003BD2C does
 * `wave_list[i].base += wave_tables_offset`. Either the SoundBank's
 * stored wave_list[i].base is corrupt, or arg1 (wave_tables_offset)
 * is bogus, or both.
 *
 * Entry hook: log (soundbank addr, wave_tables_offset).
 * Exit hook: read first 4 entries' resolved wave_list[i].base values
 * for sanity. wave_list pointer is at +0x28 of SoundBank;
 * wave_list[i] is a Wave* (4 bytes); Wave.base is at offset 0 of Wave. */
static __thread uint32_t s_sb_loader_sb_stack[8];
static __thread int s_sb_loader_sp = 0;

void pkmnstadium_sb_loader_exit_log(uint8_t* rdram, uint32_t soundbank);

void pkmnstadium_sb_loader_args_log(uint8_t* rdram, uint32_t soundbank,
                                      uint32_t wave_tables_offset) {
    if (s_sb_loader_sp < 8) s_sb_loader_sb_stack[s_sb_loader_sp] = soundbank;
    s_sb_loader_sp++;
    static int s_n = 0;
    s_n++;
    if (s_n > 16) return;
    fprintf(stderr,
        "[sb-loader] enter sb=0x%08X wave_tables_offset=0x%08X\n",
        soundbank, wave_tables_offset);
    fflush(stderr);
}

void pkmnstadium_sb_loader_exit_log_tls(uint8_t* rdram) {
    s_sb_loader_sp--;
    int idx = (s_sb_loader_sp >= 0 && s_sb_loader_sp < 8) ? s_sb_loader_sp : 0;
    uint32_t soundbank = s_sb_loader_sb_stack[idx];
    pkmnstadium_sb_loader_exit_log(rdram, soundbank);
}

void pkmnstadium_sb_loader_exit_log(uint8_t* rdram, uint32_t soundbank) {
    static int s_n = 0;
    s_n++;
    if (s_n > 16) return;
    if (soundbank < 0x80000000u || soundbank >= 0x80800000u) {
        fprintf(stderr, "[sb-loader] exit sb=0x%08X out-of-range\n", soundbank);
        fflush(stderr);
        return;
    }
    /* Read SoundBank fields: wave_list ptr at offset 0x28, count at offset 0x00. */
    uint32_t sb_paddr = soundbank & 0x1FFFFFFFu;
#define RD_BE32(paddr) (((uint32_t)rdram[((paddr) + 0) ^ 3] << 24) | \
                        ((uint32_t)rdram[((paddr) + 1) ^ 3] << 16) | \
                        ((uint32_t)rdram[((paddr) + 2) ^ 3] <<  8) | \
                        ((uint32_t)rdram[((paddr) + 3) ^ 3]))
    /* SoundBank layout (disasm/include/sound.h):
     *   0x00 char header_name[16]
     *   0x10 u32 flags
     *   0x14 char wbk_name[12]
     *   0x20 s32 count
     *   0x24 char* basenote
     *   0x28 f32* detune
     *   0x2C ALWaveTable** wave_list */
    uint32_t count = RD_BE32(sb_paddr + 0x20);
    uint32_t wave_list = RD_BE32(sb_paddr + 0x2C);
    fprintf(stderr,
        "[sb-loader] exit sb=0x%08X count=%u wave_list=0x%08X",
        soundbank, count, wave_list);
    if (wave_list >= 0x80000000u && wave_list < 0x80800000u) {
        uint32_t wl_paddr = wave_list & 0x1FFFFFFFu;
        uint32_t n = count > 4 ? 4 : count;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t wave_ptr = RD_BE32(wl_paddr + i * 4);
            uint32_t base = 0;
            if (wave_ptr >= 0x80000000u && wave_ptr < 0x80800000u) {
                base = RD_BE32((wave_ptr & 0x1FFFFFFFu) + 0);
            }
            fprintf(stderr, " wl[%u]=0x%08X(base=0x%08X)",
                i, wave_ptr, base);
        }
    }
    fprintf(stderr, "\n");
#undef RD_BE32
    fflush(stderr);
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

/* Data-context-driven Memmap_GetFragmentVaddr override.
 *
 * Pokemon Stadium's pattern fragments share one game-side fragment id
 * (e.g. 0xEF for stadium_models at link-vram 0x8FF00000). gFragments[id]
 * is a single-pointer table: it tracks whichever variant was registered
 * most recently. In the original game this is fine because only one
 * variant is materialized in RDRAM at a time; in the recompiler,
 * multiple variants are host-resident concurrently, so gFragments[id]
 * becomes ambiguous.
 *
 * The original game maintains an implicit invariant: while
 * process_geo_layout walks variant X's data, gFragments[X.id] points
 * to X's buffer, and embedded 0x8FF0XXXX literals in X's command
 * stream resolve back into X. To restore that semantics without the
 * single-pointer constraint, we use the walker's CURRENT COMMAND
 * POINTER (gGeoLayoutCommand at vaddr 0x800ABE00) as the data
 * context: it points into exactly one variant's RDRAM buffer, and
 * embedded literals should resolve against that variant.
 *
 * Resolution strategy:
 *
 *  1. If the game's native answer (gFragments[id] + offset) lies
 *     inside a registered variant of this bucket, trust it. Common
 *     case for steady-state walks where gFragments[id] points to the
 *     correct variant.
 *
 *  2. Otherwise: use the walker's current data context. Find the
 *     variant containing gGeoLayoutCommand and resolve against it.
 *
 *  3. If neither succeeds (top-level call with no data context, AND
 *     no native variant covers the offset): fall through to the game's
 *     native answer. This deliberately does NOT pick a variant by
 *     heuristic — the underlying issue is a missing variant load in
 *     the game-state orchestration, and a heuristic pick would silently
 *     mask it. The lookup-miss crash in this path IS the diagnostic.
 *
 * Bounded scope: only fires for inputs in 0x8FF00000-range. Other
 * fragment buckets (single-variant) get the game's native answer
 * untouched.
 */
/* DELETED 2026-05-23: pkmnstadium_memmap_get_enter / pkmnstadium_memmap_get_exit
 * were Stadium-specific bridges from Memmap_GetFragmentVaddr to the
 * data-context resolution helpers in librecomp. Their orchestration
 * logic (TLS-stack input save at entry + 3-step resolve at exit) was
 * promoted to librecomp on 2026-05-23 as librecomp_fragment_input_push
 * / librecomp_fragment_resolve_exit — the game.toml hooks now call
 * those directly. Pattern is generic across any game with pattern-bucket
 * fragments; see lib/N64ModernRuntime/librecomp/src/overlays.cpp. */

/* Segment-map setter/clear diagnostic.
 *
 * Memmap_SetSegmentMap / Memmap_ClearSegmentMemmap are the only
 * writers of gSegments[16] (the segment-id -> vaddr table consulted
 * by Memmap_GetSegmentVaddr). The autonomous attract-demo crash
 * shows process_geo_layout reading variant-384 bytes that decode as
 * a stream of segment-6 references, while gSegments[6].vaddr is
 * NULL at that moment — meaning either segment 6 was never mapped
 * before attract, or it was mapped and then cleared too early.
 *
 * These hooks fire at function entry. Caller attribution is via a
 * host backtrace (DbgHelp on Windows): N64Recomp does NOT populate
 * ctx->r31 for direct C-call JAL targets that don't read $ra
 * themselves, so the MIPS return address is not available. The
 * recompiled C caller frame IS available, which resolves to a
 * funcs_NN.c:line whose adjacent comment is the calling MIPS PC.
 *
 * Output is rate-limited to first 64 events of any id, but id==6
 * ALWAYS logs, is tagged <SEG6>, and gets a host backtrace. */
#ifdef _WIN32
static void pkmnstadium_segmap_dump_host_backtrace(void) {
    HANDLE proc = GetCurrentProcess();
    SymInitialize(proc, NULL, TRUE);
    void* frames[16];
    USHORT n = CaptureStackBackTrace(0, 16, frames, NULL);
    char symbuf[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO* sym = (SYMBOL_INFO*)symbuf;
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;
    IMAGEHLP_LINE64 line; memset(&line, 0, sizeof(line));
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    for (USHORT i = 0; i < n && i < 12; i++) {
        DWORD64 disp64 = 0; DWORD disp32 = 0;
        const char* name = "?"; const char* file = "?"; DWORD lineno = 0;
        if (SymFromAddr(proc, (DWORD64)frames[i], &disp64, sym)) name = sym->Name;
        if (SymGetLineFromAddr64(proc, (DWORD64)frames[i], &disp32, &line)) {
            file = line.FileName; lineno = line.LineNumber;
        }
        fprintf(stderr, "  #%02u %s (%s:%lu)\n", i, name, file, lineno);
    }
}
#else
static void pkmnstadium_segmap_dump_host_backtrace(void) {}
#endif

void pkmnstadium_segmap_set_enter(uint32_t id, uint32_t vaddr,
                                  uint32_t size, uint32_t game_state,
                                  uint32_t caller) {
    static int s_n = 0;
    static int s_n_id0 = 0;
    /* Push into the queryable memmap ring (kind 0), EXCEPT id=0 which is
     * the RDRAM identity remap that fires every frame (~70/s) — capturing
     * it would evict the meaningful id>=1 binds from the 2048-entry ring
     * within ~30s. id=0 is never a texture-asset segment anyway. */
    if (id != 0u) memmap_ring_push(0u, id, vaddr, size, caller, game_state);
    s_n++;
    /* id=0 (RDRAM identity remap) fires every frame; cap separately
     * so it doesn't flood out the interesting non-id=0 events. */
    if (id == 0) {
        s_n_id0++;
        if (s_n_id0 > 8) return;
    }
    fprintf(stderr,
        "[segmap-set] #%d id=%u vaddr=0x%08X size=0x%X gs=0x%08X%s\n",
        s_n, id, vaddr, size, game_state,
        id == 6 ? "  <SEG6>" : "");
    if (id == 6) pkmnstadium_segmap_dump_host_backtrace();
    fflush(stderr);
}

/* func_80004258 (= ASSET_LOAD inner) entry diagnostic.
 * Args: r4=id (segment id), r5=rom_start, r6=rom_end, r7=arg3.
 * Body calls func_80003DC4(rom_start, rom_end, arg3, 0); if it
 * returns non-NULL AND id > 0, calls Memmap_SetSegmentMap(id, ...).
 * If we see id=6 entry events but no [segmap-set] for id=6, the
 * inner loader returned NULL (or id was passed as 0). If we don't
 * see id=6 entry at all, the call site (fragment75:96) is being
 * skipped earlier in the call chain. */
void pkmnstadium_asset_load_enter(uint32_t id, uint32_t rom_start,
                                  uint32_t rom_end, uint32_t arg3,
                                  uint32_t game_state) {
    static int s_n = 0;
    s_n++;
    if (s_n > 256 && id != 6) return;
    fprintf(stderr,
        "[asset-load] #%d id=%u rom=0x%08X..0x%08X arg3=0x%X gs=0x%08X%s\n",
        s_n, id, rom_start, rom_end, arg3, game_state,
        id == 6 ? "  <SEG6-LOAD>" : "");
    fflush(stderr);
}

void pkmnstadium_segmap_clear_enter(uint32_t id, uint32_t game_state,
                                    uint32_t caller) {
    static int s_n = 0;
    memmap_ring_push(1u, id, 0u, 0u, caller, game_state);
    s_n++;
    fprintf(stderr,
        "[segmap-clear] #%d id=%u gs=0x%08X%s\n",
        s_n, id, game_state,
        id == 6 ? "  <SEG6>" : "");
    if (id == 6) pkmnstadium_segmap_dump_host_backtrace();
    fflush(stderr);
}

/* Asset-loader chain diagnostic (ASSET_LOAD2 / func_800044F4
 * / func_8000484C / func_800047C4) — the path that loads
 * stadium_models for fragment62. The autonomous attract-demo
 * crash traces back to process_geo_layout walking an
 * uninitialized buffer that came out of this chain.
 *
 * Per-call thread-local 1-deep saved args, plus exit prints. */
static __thread uint32_t s_load2_arg_rom_start[8];
static __thread uint32_t s_load2_arg_rom_end[8];
static __thread int s_load2_arg2_arg3[8];
static __thread int s_load2_sp = 0;

void pkmnstadium_load2_enter(uint32_t rom_start, uint32_t rom_end,
                               uint32_t arg2, uint32_t arg3) {
    if (s_load2_sp < 8) {
        s_load2_arg_rom_start[s_load2_sp] = rom_start;
        s_load2_arg_rom_end[s_load2_sp]   = rom_end;
        s_load2_arg2_arg3[s_load2_sp]     = (int)((arg2 << 8) | arg3);
    }
    s_load2_sp++;
    static int s_n = 0;
    s_n++;
    if (s_n <= 64) {
        fprintf(stderr,
            "[load2] enter rom=0x%08X..0x%08X arg2=%u arg3=%u depth=%d\n",
            rom_start, rom_end, arg2, arg3, s_load2_sp);
        fflush(stderr);
    }
}
void pkmnstadium_load2_exit(uint32_t ret) {
    s_load2_sp--;
    int idx = s_load2_sp >= 0 && s_load2_sp < 8 ? s_load2_sp : 0;
    static int s_n = 0;
    s_n++;
    if (s_n <= 64) {
        fprintf(stderr,
            "[load2] exit  rom=0x%08X..0x%08X arg2=%u arg3=%u -> 0x%08X\n",
            s_load2_arg_rom_start[idx],
            s_load2_arg_rom_end[idx],
            (s_load2_arg2_arg3[idx] >> 8) & 0xFF,
            s_load2_arg2_arg3[idx] & 0xFF,
            ret);
        fflush(stderr);
    }
}

static __thread uint32_t s_aload_arg_archive[8];
static __thread uint32_t s_aload_arg_file_number[8];
static __thread int s_aload_sp = 0;

void pkmnstadium_aload_enter(uint8_t* rdram, uint32_t archive,
                             uint32_t file_number, uint32_t caller) {
    if (s_aload_sp < 8) {
        s_aload_arg_archive[s_aload_sp] = archive;
        s_aload_arg_file_number[s_aload_sp] = file_number;
    }
    s_aload_sp++;

    /* Read the BinArchive header (and the indexed BinArchiveFile entry)
     * straight from RDRAM. `archive` is a kseg0 vaddr; mask to the 8 MiB
     * physical offset and bounds-check each read. */
    uint32_t ao = archive & 0x7FFFFFu;
    uint32_t raw = 0, unk04 = 0, total = 0, nfiles = 0;
    uint32_t foff = 0, fsz = 0, funk08 = 0;
    if (archive >= 0x80000000u && (ao + 0x10u) <= 0x800000u) {
        raw    = MEM_LOAD_BE32(rdram, ao + 0x00u);  /* unk_00<<16 | unk_02 */
        unk04  = MEM_LOAD_BE32(rdram, ao + 0x04u);  /* compressed source   */
        total  = MEM_LOAD_BE32(rdram, ao + 0x08u);
        nfiles = MEM_LOAD_BE32(rdram, ao + 0x0Cu);
        if ((int32_t)file_number >= 0 && file_number < nfiles) {
            uint32_t feo = ao + 0x10u + (file_number * 0x10u);
            if ((feo + 0x10u) <= 0x800000u) {
                foff   = MEM_LOAD_BE32(rdram, feo + 0x00u);
                fsz    = MEM_LOAD_BE32(rdram, feo + 0x04u);
                funk08 = MEM_LOAD_BE32(rdram, feo + 0x08u);  /* decompressed cache */
            }
        }
    }
    uint32_t gs = MEM_LOAD_BE32(rdram, 0x00075668u);
    uint64_t s = arcload_ring_push(0u, archive, file_number, raw, unk04,
                                   total, nfiles, foff, fsz, funk08, caller, gs);

    /* Remember this frame so a synchronous relocate dispatch from inside
     * this func_8000484C call (the FRAGMENT-magic branch) can be paired
     * with the archive that produced it. */
    s_last_aload.seq = s; s_last_aload.kind = 0u;
    s_last_aload.a = archive; s_last_aload.b = file_number;
    s_last_aload.hdr0 = raw; s_last_aload.hdr1 = unk04;
    s_last_aload.hdr2 = total; s_last_aload.hdr3 = nfiles;
    s_last_aload.extra0 = foff; s_last_aload.extra1 = fsz;
    s_last_aload.extra2 = funk08; s_last_aload.caller = caller;
    s_last_aload.game_state = gs;
    s_have_last_aload = 1;

    static int s_n = 0;
    s_n++;
    if (s_n <= 96) {
        fprintf(stderr,
            "[aload] enter archive=0x%08X file=%d depth=%d unk_00=0x%04X "
            "unk_02=0x%04X num_files=%u unk_04=0x%08X\n",
            archive, (int32_t)file_number, s_aload_sp,
            (raw >> 16) & 0xFFFFu, raw & 0xFFFFu, nfiles, unk04);
        fflush(stderr);
    }
}
void pkmnstadium_aload_exit(uint32_t ret) {
    s_aload_sp--;
    int idx = s_aload_sp >= 0 && s_aload_sp < 8 ? s_aload_sp : 0;
    static int s_n = 0;
    s_n++;
    if (s_n <= 96) {
        fprintf(stderr,
            "[aload] exit  archive=0x%08X file_number=%d -> 0x%08X\n",
            s_aload_arg_archive[idx],
            (int32_t)s_aload_arg_file_number[idx], ret);
        fflush(stderr);
    }
}

void pkmnstadium_persload_enter(uint32_t archive, uint32_t file_ent) {
    static int s_n = 0;
    s_n++;
    if (s_n <= 32) {
        fprintf(stderr,
            "[persload] enter archive=0x%08X file_ent=0x%08X\n",
            archive, file_ent);
        fflush(stderr);
    }
}
void pkmnstadium_persload_exit(uint32_t ret) {
    static int s_n = 0;
    s_n++;
    if (s_n <= 32) {
        fprintf(stderr,
            "[persload] exit  -> 0x%08X\n", ret);
        fflush(stderr);
    }
}

/* process_geo_layout entry diagnostic — log (pool, segptr) so we
 * can identify the geo-data source for each call. The crashing call
 * passed a segptr that translates via Memmap_GetFragmentVaddr to a
 * pool address holding garbage; this hook lets us pin the
 * fragment-vaddr source. */
extern void recomp_diag_dump_variant_candidates_for_offset(
    void* rdram, uint32_t pattern_id, uint32_t offset);

void pkmnstadium_geo_entry_log(void* rdram, uint32_t pool_arg, uint32_t segptr) {
    static int s_n = 0;
    s_n++;
    int log_now = (s_n <= 32) || (segptr == 0);
    if (!log_now) return;
    fprintf(stderr,
        "[geo-entry] #%d pool=0x%08X segptr=0x%08X\n",
        s_n, pool_arg, segptr);

    /* Option C variant-selection probe: when segptr is in pattern
     * bucket 0x8FF00000 (= a fragment-vaddr literal that needs
     * runtime resolution), dump every loaded variant of id=0xEF
     * whose size covers the requested offset and peek at the bytes
     * at runtime_base + offset. Shows which variants have plausible
     * geo data at the requested offset — narrows "which variant
     * does fragment62 actually want?" to a small set. One-shot per
     * unique segptr. */
    if ((segptr & 0xFFF00000u) == 0x8FF00000u &&
        segptr >= 0x81000000u && segptr < 0x90000000u)
    {
        static uint32_t s_probed[32] = {0};
        static int s_n_probed = 0;
        int already = 0;
        for (int i = 0; i < s_n_probed && i < 32; i++) {
            if (s_probed[i] == segptr) { already = 1; break; }
        }
        if (!already && s_n_probed < 32) {
            s_probed[s_n_probed++] = segptr;
            const uint32_t pattern_id = ((segptr & 0x0FF00000u) >> 0x14) - 0x10;
            const uint32_t offset = segptr & 0x000FFFFFu;
            recomp_diag_dump_variant_candidates_for_offset(
                rdram, pattern_id, offset);
        }
    }
#ifdef _WIN32
    /* Capture a host backtrace for two cases:
     *   (a) NULL segptr — white-screen attract bug.
     *   (b) Pattern-bucket segptr (0x8FF0XXXX) — Bug 1 origin trace.
     * Backtrace identifies the caller of process_geo_layout that
     * passed the bad segptr; the recompiled C function name maps
     * to a MIPS function via funcs_NN.c, and the line number
     * resolves to a `// 0x...:` comment with the originating
     * MIPS PC. One-shot per unique segptr (re-using s_probed). */
    int do_bt = (segptr == 0);
    if ((segptr & 0xFFF00000u) == 0x8FF00000u &&
        segptr >= 0x81000000u && segptr < 0x90000000u) {
        static uint32_t s_bt_done[32] = {0};
        static int s_bt_n = 0;
        int seen = 0;
        for (int i = 0; i < s_bt_n && i < 32; i++) {
            if (s_bt_done[i] == segptr) { seen = 1; break; }
        }
        if (!seen && s_bt_n < 32) {
            s_bt_done[s_bt_n++] = segptr;
            do_bt = 1;
        }
    }
    if (do_bt) {
        fprintf(stderr,
            "[geo-entry-origin] segptr=0x%08X pool=0x%08X — host backtrace:\n",
            segptr, pool_arg);
        HANDLE proc = GetCurrentProcess();
        SymInitialize(proc, NULL, TRUE);
        void* frames[24];
        USHORT n = CaptureStackBackTrace(0, 24, frames, NULL);
        char symbuf[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* sym = (SYMBOL_INFO*)symbuf;
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        IMAGEHLP_LINE64 line; memset(&line, 0, sizeof(line));
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        for (USHORT i = 0; i < n && i < 18; i++) {
            DWORD64 disp64 = 0; DWORD disp32 = 0;
            const char* name = "?"; const char* file = "?"; DWORD lineno = 0;
            if (SymFromAddr(proc, (DWORD64)frames[i], &disp64, sym)) name = sym->Name;
            if (SymGetLineFromAddr64(proc, (DWORD64)frames[i], &disp32, &line)) {
                file = line.FileName; lineno = line.LineNumber;
            }
            fprintf(stderr, "  #%02u %s (%s:%lu)\n", i, name, file, lineno);
        }
    }
#endif
    fflush(stderr);
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
 * address and crash on.
 *
 * Also runs the Option-C variant-selection census: when input is in
 * the pattern bucket 0x8FF00000, fire the structural-parse probe
 * once per unique input. Builds a per-literal report of which
 * variant fragment62 (or any other caller) actually wants. */
void pkmnstadium_addr_translate_log(void* rdram, uint32_t in, uint32_t out) {
    /* Option C census: any 0x8FF0XXXX input gets one-shot probed.
     * Bug 1 origin trace: ALSO dump a host backtrace for the first
     * occurrence so we can identify the recompiled C function (and
     * thus the originating MIPS PC) that produced the literal. */
    if ((in & 0xFFF00000u) == 0x8FF00000u &&
        in >= 0x81000000u && in < 0x90000000u) {
        static uint32_t s_probed[64] = {0};
        static int s_n_probed = 0;
        int already = 0;
        for (int i = 0; i < s_n_probed && i < 64; i++) {
            if (s_probed[i] == in) { already = 1; break; }
        }
        if (!already && s_n_probed < 64) {
            s_probed[s_n_probed++] = in;
            const uint32_t pattern_id = ((in & 0x0FF00000u) >> 0x14) - 0x10;
            const uint32_t offset = in & 0x000FFFFFu;
            recomp_diag_dump_variant_candidates_for_offset(
                rdram, pattern_id, offset);
#ifdef _WIN32
            /* Bug 1 origin trace: identify the recompiled C function
             * (and thus the MIPS handler) that called
             * Util_ConvertAddrToVirtAddr with this segptr. The frame
             * directly above this hook is Util_ConvertAddrToVirtAddr
             * itself; the next frame up is the recompiled caller —
             * its name maps to a MIPS function via funcs_NN.c, and
             * the line number resolves to a `// 0x...:` comment with
             * the originating MIPS PC. */
            fprintf(stderr,
                "[addr-xlate-origin] in=0x%08X (pattern_id=0x%X off=0x%X) "
                "host backtrace:\n", in, pattern_id, offset);
            HANDLE proc = GetCurrentProcess();
            SymInitialize(proc, NULL, TRUE);
            void* frames[24];
            USHORT n = CaptureStackBackTrace(0, 24, frames, NULL);
            char symbuf[sizeof(SYMBOL_INFO) + 256];
            SYMBOL_INFO* sym = (SYMBOL_INFO*)symbuf;
            sym->SizeOfStruct = sizeof(SYMBOL_INFO);
            sym->MaxNameLen = 255;
            IMAGEHLP_LINE64 line; memset(&line, 0, sizeof(line));
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            for (USHORT i = 0; i < n && i < 18; i++) {
                DWORD64 disp64 = 0; DWORD disp32 = 0;
                const char* name = "?"; const char* file = "?"; DWORD lineno = 0;
                if (SymFromAddr(proc, (DWORD64)frames[i], &disp64, sym)) name = sym->Name;
                if (SymGetLineFromAddr64(proc, (DWORD64)frames[i], &disp32, &line)) {
                    file = line.FileName; lineno = line.LineNumber;
                }
                fprintf(stderr, "  #%02u %s (%s:%lu)\n", i, name, file, lineno);
            }
            fflush(stderr);
#endif
        }
    }

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
    int low_addr = (arg1_seg != 0 && arg1_seg < 0x80000000u) &&
                   ((arg1_seg & 0xFF000000u) == 0);
    if (s_n <= 16 || low_addr) {
        fprintf(stderr,
            "[geo-render] #%d node=0x%08X arg1(seg)=0x%08X arg2=0x%08X%s\n",
            s_n, arg0_node, arg1_seg, arg2_data,
            low_addr ? " <-- SUSPECT (low addr)" : "");
        fflush(stderr);
    }
}

/* Thin glue for the generic voice-UAF protector in librecomp.
 *
 * Hooked at main_pool_pop_state entry. Reads sMemPool to compute the
 * about-to-be-freed range [saved.L, current.L), then forwards to:
 *   - librecomp_audio_uaf_silence_voices_in_range: libnaudio side
 *     (walks N_ALSynth's pAllocList + pLameList, range-checks dc_table)
 *   - librecomp_audio_uaf_silence_secondary_in_range: Stadium-side
 *     (walks D_800FC7D0 array, chains through unk_090 -> +0x2C to
 *      reach the wavetable-pointer-array base, range-checks it)
 *
 * Both layouts are registered at startup in src/main/main.cpp. This
 * single hook covers any pool pop that would orphan an active voice's
 * wavetable — at either the libnaudio level OR the Stadium-side synth
 * level. Retires the prior per-scene func_8004FF20 hook (was:
 * pkmnstadium_audio_stop_voices, deleted) which stopped Stadium voices
 * unconditionally at one specific cleanup; the generic range-checked
 * approach handles every pop, not just fragment36's.
 *
 * sMemPool layout (vaddr 0x800A6070, see pkmnstadium_pool_pop_log):
 *   +0x28 listHeadL (current bump pointer, low side)
 *   +0x30 mainState* (pointer to MainPoolState)
 * MainPoolState (whatever state_v points to):
 *   +0x04 listHeadL (saved low-side bump pointer to pop back to) */
extern int librecomp_audio_uaf_silence_voices_in_range(
    uint8_t* rdram, uint32_t start_vaddr, uint32_t end_vaddr);
extern int librecomp_audio_uaf_silence_secondary_in_range(
    uint8_t* rdram, uint32_t start_vaddr, uint32_t end_vaddr);

void pkmnstadium_pool_pop_silence_voices(uint8_t* rdram) {
    uint32_t pool_p = 0x000A6070u;
    uint32_t cur_L  = MEM_LOAD_BE32(rdram, pool_p + 0x28);
    uint32_t state_v = MEM_LOAD_BE32(rdram, pool_p + 0x30);
    if (state_v < 0x80000000u || state_v >= 0x80800000u) return;
    if (cur_L  < 0x80000000u || cur_L  >= 0x80800000u) return;
    uint32_t state_p = state_v & 0x1FFFFFFFu;
    uint32_t saved_L = MEM_LOAD_BE32(rdram, state_p + 0x04);
    if (saved_L < 0x80000000u || saved_L >= 0x80800000u) return;
    if (saved_L >= cur_L) return;  /* nothing to free */
    librecomp_audio_uaf_silence_voices_in_range(rdram, saved_L, cur_L);
    librecomp_audio_uaf_silence_secondary_in_range(rdram, saved_L, cur_L);
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

static void pkmnstadium_trace_return_common(const char *func,
                                            const recomp_context *ctx) {
    /* For "where are we stuck?" the entry log is what matters; returns
     * are recorded too in case we need to reconstruct a call stack. */
    /* DIAG: skip libnaudio (_n_*) returns — see pkmnstadium_trace_entry_common. */
    if (func[0] == '_' && func[1] == 'n' && func[2] == '_') {
        return;
    }
    /* DIAG (dev/register-quit-softlock): at func_84203E6C's return, log the
     * jr-dispatch decision inputs. TRACE_RETURN fires in BOTH the tailcall and
     * normal-return branches, so tailcall_pending tells us which was taken; r31
     * ($ra = jr target) vs host_return_target tells us WHY. Remove with the
     * TEMP trace block. */
    if (ctx != NULL && (__builtin_strcmp(func, "func_84203E6C") == 0 ||
                        __builtin_strcmp(func, "func_80029008") == 0 ||
                        __builtin_strcmp(func, "func_80029984") == 0 ||
                        __builtin_strcmp(func, "func_80029BC0") == 0 ||
                        __builtin_strcmp(func, "main_pool_try_free") == 0 ||
                        __builtin_strcmp(func, "main_pool_free") == 0)) {
        fprintf(stderr,
            "[jrdiag] %s RETURN: ra(r31)=0x%08X host_return_target=0x%08X "
            "tailcall_pending=%u tailcall_target=0x%08X sp(r29)=0x%08X\n",
            func, (uint32_t)ctx->r31, ctx->host_return_target,
            ctx->tailcall_pending, ctx->tailcall_target, (uint32_t)ctx->r29);
        fflush(stderr);
    }
    uint64_t idx = __atomic_fetch_add(&trace_ring_write_idx, 1, __ATOMIC_RELAXED);
    trace_ring[idx & (TRACE_RING_CAP - 1)] = func;  /* same slot semantics */
    pkmnstadium_asset_wait_trace(func, ctx, 1);
    pkmnstadium_gb_fragment_trace(func, ctx, 1);
}

void pkmnstadium_trace_return(const char *func) {
    pkmnstadium_trace_return_common(func, NULL);
}

void pkmnstadium_trace_return_ctx(const char *func, const void *ctx) {
    pkmnstadium_trace_return_common(func, (const recomp_context *)ctx);
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
