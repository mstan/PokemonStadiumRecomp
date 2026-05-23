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
    FILE *f = fopen("F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_error.log", "a");
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
        FILE *f = fopen("F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_error.log", "a"); \
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
        /* Big-endian read of w0,w1 from RDRAM. */
        uint32_t w0 = ((uint32_t)rdram[off]   << 24) |
                      ((uint32_t)rdram[off+1] << 16) |
                      ((uint32_t)rdram[off+2] << 8)  |
                      ((uint32_t)rdram[off+3]);
        uint32_t w1 = ((uint32_t)rdram[off+4] << 24) |
                      ((uint32_t)rdram[off+5] << 16) |
                      ((uint32_t)rdram[off+6] << 8)  |
                      ((uint32_t)rdram[off+7]);
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
extern int32_t recomp_resolve_via_data_context(uint32_t link_vaddr,
                                                  uint32_t data_ctx_addr);
extern int recomp_addr_in_loaded_variant(uint32_t bucket, uint32_t addr);
extern int32_t recomp_resolve_synthetic_fragment(uint32_t addr);

static __thread uint32_t s_memmap_get_input_stack[16];
static __thread int s_memmap_get_sp = 0;

void pkmnstadium_memmap_get_enter(uint32_t input) {
    if (s_memmap_get_sp < 16) s_memmap_get_input_stack[s_memmap_get_sp] = input;
    s_memmap_get_sp++;
}

uint32_t pkmnstadium_memmap_get_exit(uint32_t game_result, uint32_t data_ctx) {
    s_memmap_get_sp--;
    int idx = (s_memmap_get_sp >= 0 && s_memmap_get_sp < 16) ? s_memmap_get_sp : 0;
    uint32_t input = s_memmap_get_input_stack[idx];

    /* Path 2 synthetic resolver (highest priority). If the input is in
     * the per-variant synthetic-vram pool (0xA0000000..0xC0000000), it
     * was emitted by a recompiled pattern variant whose section.ram_addr
     * we assigned a unique synthetic identity. Resolve via the parallel
     * recomp_synthetic_fragments[] table, bypassing the game's native
     * gFragments[id] entirely. The native game path returned the input
     * unchanged here (game_result == input) because the input is outside
     * the 0x81000000..0x90000000 range the game recognizes — we just
     * substitute our resolution.
     *
     * recomp_resolve_synthetic_fragment aborts deterministically if the
     * slot is empty. Returning 0 here means "addr wasn't in the
     * synthetic pool", in which case we fall through to the existing
     * 0x8FF00000-bucket data-context logic. */
    {
        int32_t synth = recomp_resolve_synthetic_fragment(input);
        if (synth != 0) {
            return (uint32_t)synth;
        }
    }

    /* Bounded scope: only the known-ambiguous 0x8FF00000 bucket. */
    if ((input & 0xFFF00000u) != 0x8FF00000u) return game_result;
    if (input < 0x81000000u || input >= 0x90000000u) return game_result;

    /* Step 1: trust the game's answer when it lands inside a registered
     * variant of this bucket. */
    if (recomp_addr_in_loaded_variant(input & 0xFFF00000u, game_result)) {
        return game_result;
    }

    /* Step 2: data-context resolution. Walker's current data pointer
     * lives in the variant currently being walked. */
    int32_t resolved = recomp_resolve_via_data_context(input, data_ctx);

    static volatile int s_n_logged = 0;
    int log_idx = __atomic_fetch_add(&s_n_logged, 1, __ATOMIC_RELAXED);
    if (log_idx < 16) {
        fprintf(stderr,
            "[memmap-ctx] in=0x%08X game-result=0x%08X "
            "data-ctx=0x%08X resolved=0x%08X\n",
            input, game_result, data_ctx, (uint32_t)resolved);
        fflush(stderr);
    }

    if (resolved != 0) return (uint32_t)resolved;
    return game_result;
}

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
                                  uint32_t size, uint32_t game_state) {
    static int s_n = 0;
    static int s_n_id0 = 0;
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

void pkmnstadium_segmap_clear_enter(uint32_t id, uint32_t game_state) {
    static int s_n = 0;
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

void pkmnstadium_aload_enter(uint32_t archive, uint32_t file_number) {
    if (s_aload_sp < 8) {
        s_aload_arg_archive[s_aload_sp] = archive;
        s_aload_arg_file_number[s_aload_sp] = file_number;
    }
    s_aload_sp++;
    static int s_n = 0;
    s_n++;
    if (s_n <= 96) {
        fprintf(stderr,
            "[aload] enter archive=0x%08X file_number=%d depth=%d\n",
            archive, (int32_t)file_number, s_aload_sp);
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

    /* Stadium-side stop above is necessary but not sufficient: the
     * libnaudio synth maintains its OWN voice pool (N_PVoice array)
     * via n_syn->auxBus->sources. Even after Stadium stops feeding
     * the high-level voice, those low-level N_PVoices keep getting
     * pulled by n_alAuxBusPull → n_alEnvmixerPull → n_alAdpcmPull
     * because their em_motion is still AL_PLAYING. n_alAdpcmPull
     * then reads f->dc_table and friends from soon-to-be-popped
     * pool memory, computes overflow nOver of ~3.7B samples, emits
     * a CLEARBUFF cmd whose count wraps DMEM and zeroes the dispatch
     * table — the post-title audio bug 2026-05-06.
     *
     * Walk libnaudio's voice pool and force em_motion=AL_STOPPED so
     * n_alEnvmixerPull's "if (em_motion != AL_PLAYING) return" gate
     * kicks them out before any wavetable deref.
     *
     * Field offsets (verified against recompiled n_alAuxBusPull and
     * the N_PVoice struct in n_synthInternals.h):
     *   n_syn at vaddr 0x80078584 (pointer-to-struct, NOT inline)
     *     +0x34 auxBus*
     *   N_ALAuxBus
     *     +0x14 sourceCount (s32)
     *     +0x1C sources (N_PVoice**)
     *   N_PVoice
     *     +0x84 em_motion (s32; 0 = AL_STOPPED, 1 = AL_PLAYING)
     */
    {
        uint32_t n_syn_var_paddr = 0x00078584u;
        uint32_t n_syn_vaddr = MEM_LOAD_BE32(rdram, n_syn_var_paddr);
        if (n_syn_vaddr < 0x80000000u || n_syn_vaddr >= 0x80800000u) {
            fprintf(stderr,
                "[audio-stop] n_syn pointer not in RDRAM (0x%08X), "
                "libnaudio voices not stopped\n", n_syn_vaddr);
            fflush(stderr);
            return;
        }
        uint32_t n_syn_paddr = n_syn_vaddr & 0x1FFFFFFFu;
        uint32_t auxBus_vaddr = MEM_LOAD_BE32(rdram, n_syn_paddr + 0x34);
        if (auxBus_vaddr < 0x80000000u || auxBus_vaddr >= 0x80800000u) {
            fprintf(stderr,
                "[audio-stop] auxBus ptr not in RDRAM (0x%08X), skipped\n",
                auxBus_vaddr);
            fflush(stderr);
            return;
        }
        uint32_t auxBus_paddr = auxBus_vaddr & 0x1FFFFFFFu;
        uint32_t source_count = MEM_LOAD_BE32(rdram, auxBus_paddr + 0x14);
        uint32_t sources_vaddr = MEM_LOAD_BE32(rdram, auxBus_paddr + 0x1C);
        if (source_count == 0 || source_count > 64 ||
            sources_vaddr < 0x80000000u || sources_vaddr >= 0x80800000u) {
            fprintf(stderr,
                "[audio-stop] auxBus sourceCount=%u sources=0x%08X "
                "(skipping libnaudio stop)\n",
                source_count, sources_vaddr);
            fflush(stderr);
            return;
        }
        uint32_t sources_paddr = sources_vaddr & 0x1FFFFFFFu;
        uint32_t stopped = 0;
        for (uint32_t i = 0; i < source_count; i++) {
            uint32_t pvoice_vaddr = MEM_LOAD_BE32(rdram, sources_paddr + i*4);
            if (pvoice_vaddr < 0x80000000u || pvoice_vaddr >= 0x80800000u) continue;
            uint32_t pvoice_paddr = pvoice_vaddr & 0x1FFFFFFFu;
            /* Write 0 (AL_STOPPED) to em_motion at +0x84. */
            for (int b = 0; b < 4; b++) {
                rdram[(pvoice_paddr + 0x84 + b) ^ 3] = 0;
            }
            stopped++;
        }
        static int s_lib_logged = 0;
        if (!s_lib_logged) {
            s_lib_logged = 1;
            fprintf(stderr,
                "[audio-stop] libnaudio: zeroed em_motion on %u of %u "
                "N_PVoice(s) via n_syn->auxBus->sources at 0x%08X\n",
                stopped, source_count, sources_vaddr);
            fflush(stderr);
        }
    }
}

/* Thin glue for the generic libnaudio voice UAF protector in librecomp.
 *
 * Hooked at main_pool_pop_state entry. Reads sMemPool to compute the
 * about-to-be-freed range [saved.L, current.L), then forwards to
 * librecomp_audio_uaf_silence_voices_in_range which walks the libnaudio
 * voice array (registered at startup in main.cpp) and zeros em_motion
 * on any voice whose dc_table falls in the freed range.
 *
 * sMemPool layout (vaddr 0x800A6070, see pkmnstadium_pool_pop_log):
 *   +0x28 listHeadL (current bump pointer, low side)
 *   +0x30 mainState* (pointer to MainPoolState)
 * MainPoolState (whatever state_v points to):
 *   +0x04 listHeadL (saved low-side bump pointer to pop back to)
 *
 * Single hook subsumes per-scene voice-stop hooks (e.g. the post-title
 * fragment36 hook at func_8004FF20) for any pop that would orphan an
 * active voice's wavetable. Stays out of the way otherwise — silenced=0
 * is logged-quietly inside the librecomp helper. */
extern int librecomp_audio_uaf_silence_voices_in_range(
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

/* ====================================================================
 * PSR_TEMP_PATCH: free-battle-modal-softlock — see TEMP_PATCHES.md
 *
 * Stadium-specific Option B fix for func_8000771C cooperative-scheduler
 * deadlock. See game.toml [patches].ignored entry for full context.
 *
 * Original `func_8000771C` is a tight predicate-only busy-wait:
 *
 *   void func_8000771C(void) {
 *       while (func_80001C90() == 0) {}
 *   }
 *
 * On real N64, hardware DP/SP/AI/VI interrupts preempt the busy-wait
 * to deliver completion events that eventually flip the predicate.
 * ultramodern's cooperative scheduler can't preempt — so when the game
 * thread runs this loop, the audio scheduler thread (priority 3, lower
 * than Game_Thread) sits in running_queue waiting for its turn that
 * never comes. external_messages accumulate undrained. The DONE that
 * would flip D_800846C0.queue.validCount > 0 (the loop exit
 * condition) never gets posted.
 *
 * Workaround: between predicate checks, temporarily lower this
 * thread's priority below the audio scheduler's so check_running_queue
 * (called by yield_self_1ms) actually swaps to the audio scheduler.
 * Once it runs, it drains external_messages, dispatches msg=0x65
 * (DP-complete) → func_80005148 → func_80004C68 → osSendMesg DONE
 * to D_800846C0.queue, flipping validCount. We then resume, see the
 * predicate satisfied, restore priority, and return.
 *
 * yield_self_1ms also calls wait_for_external_message_timed which
 * directly drains pending externals via do_send under cooperative
 * single-current-game-thread invariants — safe to call from here
 * because we ARE the cooperatively-current thread.
 *
 * This is the targeted Option B fix; the proper Option C runtime fix
 * lives in N64ModernRuntime/ultramodern (a queue-ops mutex + external-
 * pump host thread) and will obsolete this entry once shipped.
 * See memory: project_free_battle_modal_softlock_2026_05_08.
 * ==================================================================== */

/* Recompiler-generated wrapper for the original predicate body
 * (func_80001C90 reads D_80083CA0.unk_AA0 and D_800846C0.queue.validCount,
 * returns 1 if predicate satisfied). */
extern void func_80001C90(uint8_t* rdram, recomp_context* ctx);

/* ultramodern primitives. Marked extern "C" in their .cpp definitions
 * so they're callable from this C translation unit. RDRAM_ARG / NULLPTR
 * macros expand to the corresponding C types per ultra64.h. */
extern void osSetThreadPri(uint8_t* rdram, int32_t /*PTR(OSThread)*/ thread, int32_t pri);
extern int32_t osGetThreadPri(uint8_t* rdram, int32_t /*PTR(OSThread)*/ thread);
extern void yield_self_1ms(uint8_t* rdram);

/* Note: NOT named with _recomp suffix — user-config ignored functions
 * (game.toml [patches].ignored) only mark `ignored = true` and skip
 * emission; they don't get renamed. Callers in generated/*.c still
 * reference the bare `func_8000771C` symbol, so our replacement
 * matches that name. (Distinct from the N64Recomp::ignored_funcs
 * built-in set which DOES rename, used for libultra translation.) */
void func_8000771C(uint8_t* rdram, recomp_context* ctx) {
    /* Save current priority, drop below audio scheduler (priority 3)
     * so check_running_queue swaps to it on yield. Stadium creates
     * Game_Thread with a higher priority; without lowering, the swap
     * never happens. */
    int32_t saved_pri = osGetThreadPri(rdram, 0 /* PTR(OSThread) NULL = self */);
    osSetThreadPri(rdram, 0, 1);

    while (1) {
        func_80001C90(rdram, ctx);
        /* func_80001C90 returns its result in $v0 = ctx->r2.
         * Predicate satisfied (any non-zero) → exit loop. */
        if (ctx->r2 != 0) {
            break;
        }
        /* Yield: drain external_messages (delivers any pending DP/SP/
         * VI completions to their target queues, possibly waking
         * blocked recv'ers) + check_running_queue (swap to higher-
         * priority queued thread; with our priority dropped to 1,
         * any other thread queued will preempt us). */
        yield_self_1ms(rdram);
    }

    osSetThreadPri(rdram, 0, saved_pri);
}

/* ====================================================================
 * PSR_TEMP_PATCH: petit-cup-softlock — see TEMP_PATCHES.md
 *
 * Stadium-specific Option B fix for the cooperative-scheduler deadlock
 * inside func_80003680 (JPEG decoder). After several pages of work
 * loading quantization/Huffman tables and pixel buffers, func_80003680
 * busy-waits on the same predicate as func_8000771C:
 *
 *     L_80003770:
 *         func_80001C90();          // 0x80003770 jal 0x80001C90
 *         if (v0 == 0) goto L_80003770;  // 0x80003778 beq $v0,$0,L
 *
 * The loop is INLINED inside func_80003680 (not wrappable like
 * func_8000771C). Replacing the whole 200+-line JPEG function in
 * extras.c is brittle, so we attach a [[patches.hook]] hook in
 * game.toml that fires before the BEQ at vram 0x80003778 and runs
 * this helper. The helper inspects ctx->r2 (the func_80001C90 return
 * in $v0); if non-zero, we're about to exit the loop and the helper
 * is a no-op. If zero, we're about to loop back — we drop priority
 * below the audio scheduler (priority 3), yield_self_1ms (drains
 * externals + swaps to highest-pri queued), then restore priority.
 *
 * Reproduced symptom (pre-fix): selecting Petit Cup at Free Battle's
 * cup-confirm screen freezes Game_Thread inside this loop; thread
 * dump shows pkmnstadium_trace_entry → func_80001C90 → func_80003680
 * → func_80003C80 → func_80003DC4 (Stadium asset loader chain).
 *
 * Same proper-fix layer as func_8000771C: a host-monitor "no context-
 * switch in N seconds" flag in N64ModernRuntime/ultramodern would
 * obsolete both per-site patches in one move.
 * See memory: project_petit_cup_softlock_optionB_2026_05_09.
 * ==================================================================== */

void pkmnstadium_petit_cup_yield(uint8_t* rdram, recomp_context* ctx) {
    /* func_80001C90 just ran; ctx->r2 holds the predicate result.
     * Non-zero → loop will exit on the BEQ; nothing to do. */
    if (ctx->r2 != 0) return;

    /* Loop will continue. Drop priority below the audio scheduler so
     * yield_self_1ms's check_running_queue actually swaps. */
    int32_t saved_pri = osGetThreadPri(rdram, 0 /* PTR(OSThread) NULL = self */);
    osSetThreadPri(rdram, 0, 1);
    yield_self_1ms(rdram);
    osSetThreadPri(rdram, 0, saved_pri);
}

/* Fragment 57's Game Pak Check UI uses several large texture panels
 * split into adjacent Vtx quads. RT64 can expose the shared edges as
 * one-pixel lines, so overlap internal tile edges by one screen pixel.
 */
#define FRAG57_SECTION_INDEX 239
#define FRAG57_VTX_SIZE 0x40u

static inline uint32_t pkmnstadium_frag57_addr(uint32_t offset) {
    uint32_t base = (uint32_t)section_addresses[FRAG57_SECTION_INDEX];
    if (base == 0) return 0;
    return base + offset;
}

static inline int16_t pkmnstadium_vtx_x(uint8_t* rdram, uint32_t vtx_addr, int idx) {
    return MEM_H(0, vtx_addr + (uint32_t)idx * 0x10u);
}

static inline int16_t pkmnstadium_vtx_y(uint8_t* rdram, uint32_t vtx_addr, int idx) {
    return MEM_H(2, vtx_addr + (uint32_t)idx * 0x10u);
}

static inline void pkmnstadium_set_vtx_x(uint8_t* rdram, uint32_t vtx_addr, int idx, int16_t value) {
    MEM_H(0, vtx_addr + (uint32_t)idx * 0x10u) = value;
}

static inline void pkmnstadium_set_vtx_y(uint8_t* rdram, uint32_t vtx_addr, int idx, int16_t value) {
    MEM_H(2, vtx_addr + (uint32_t)idx * 0x10u) = value;
}

/* Live-test 2026-05-23: at the user's render scale +1 still showed
 * residual yellow row-seam dashes on every Game Pak Check card and
 * on the main-menu icon panels. +2 cleared them; bumped here.
 */
#define FRAG57_SEAM_OVERLAP 2

static void pkmnstadium_overlap_vtx_tile(uint8_t* rdram, uint32_t vtx_addr, int overlap_left, int overlap_top) {
    if (overlap_left) {
        int16_t x = pkmnstadium_vtx_x(rdram, vtx_addr, 0);
        pkmnstadium_set_vtx_x(rdram, vtx_addr, 0, x - FRAG57_SEAM_OVERLAP);
        pkmnstadium_set_vtx_x(rdram, vtx_addr, 1, x - FRAG57_SEAM_OVERLAP);
    }

    if (overlap_top) {
        int16_t y = pkmnstadium_vtx_y(rdram, vtx_addr, 0);
        pkmnstadium_set_vtx_y(rdram, vtx_addr, 0, y + FRAG57_SEAM_OVERLAP);
        pkmnstadium_set_vtx_y(rdram, vtx_addr, 3, y + FRAG57_SEAM_OVERLAP);
    }
}

static void pkmnstadium_overlap_vtx_grid_row_major(uint8_t* rdram, uint32_t start, int rows, int cols) {
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            uint32_t vtx_addr = pkmnstadium_frag57_addr(start + (uint32_t)(row * cols + col) * FRAG57_VTX_SIZE);
            pkmnstadium_overlap_vtx_tile(rdram, vtx_addr, col > 0, row > 0);
        }
    }
}

static void pkmnstadium_overlap_vtx_grid_col_major(uint8_t* rdram, uint32_t start, int rows, int cols) {
    for (int col = 0; col < cols; col++) {
        for (int row = 0; row < rows; row++) {
            uint32_t vtx_addr = pkmnstadium_frag57_addr(start + (uint32_t)(col * rows + row) * FRAG57_VTX_SIZE);
            pkmnstadium_overlap_vtx_tile(rdram, vtx_addr, col > 0, row > 0);
        }
    }
}

void pkmnstadium_patch_fragment57_ui_seams(uint8_t* rdram) {
    uint32_t sentinel = pkmnstadium_frag57_addr(0x7048);
    if (sentinel == 0 || pkmnstadium_vtx_y(rdram, sentinel, 0) != -16) {
        return;
    }

    /* Four controller-port cards and the OK box are vertical strips. */
    pkmnstadium_overlap_vtx_grid_col_major(rdram, 0x7008, 13, 1);
    pkmnstadium_overlap_vtx_grid_col_major(rdram, 0x8680, 3, 1);

    /* Warning/header panel: two rows by fourteen columns. */
    pkmnstadium_overlap_vtx_grid_row_major(rdram, 0x7748, 2, 14);

    /* Other fragment 57 registration/info panels use two-column grids
     * with column-major Vtx storage; patch them while the fragment is hot.
     */
    pkmnstadium_overlap_vtx_grid_col_major(rdram, 0x8870, 8, 2);
    pkmnstadium_overlap_vtx_grid_col_major(rdram, 0x9140, 4, 2);
    pkmnstadium_overlap_vtx_grid_col_major(rdram, 0x95D0, 5, 2);
    pkmnstadium_overlap_vtx_grid_col_major(rdram, 0x9B70, 3, 2);
}

static void pkmnstadium_append_display_list(uint8_t* rdram, uint32_t display_list_addr) {
    uint32_t dl = MEM_W(0, 0x800A7420u);
    MEM_W(0, 0x800A7420u) = dl + 8;
    MEM_W(0, dl) = 0xDE000000u;
    MEM_W(4, dl) = display_list_addr;
}

static void pkmnstadium_append_env_color(uint8_t* rdram, uint8_t alpha) {
    uint32_t dl = MEM_W(0, 0x800A7420u);
    MEM_W(0, 0x800A7420u) = dl + 8;
    MEM_W(0, dl) = 0xFB000000u;
    MEM_W(4, dl) = 0xFFFFFF00u | alpha;
}

/* Fragment 57 draws a decorative five-strip overlay behind selected
 * Game Pak status text. On RT64 that layer leaks stale/yellow TMEM
 * rows across the card. The card background is already drawn just
 * before this function and the real text is drawn after it, so omit
 * the overlay instead of trying to preserve a broken transparent layer.
 */
void func_82D022E8(uint8_t* rdram, recomp_context* ctx) {
    uint8_t alpha = (uint8_t)(ctx->r4 & 0xFF);
    gpr saved_sp = ctx->r29;
    gpr saved_ra = ctx->r31;
    gpr saved_s0 = ctx->r16;

    ctx->r29 = ADD32(ctx->r29, -0x30);

    pkmnstadium_append_display_list(rdram, 0x8006F518u);
    pkmnstadium_append_env_color(rdram, alpha);
    pkmnstadium_append_display_list(rdram, 0x8006F630u);

    ctx->r16 = saved_s0;
    ctx->r31 = saved_ra;
    ctx->r29 = saved_sp;
}
