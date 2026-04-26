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

UNIMPL_LIBULTRA(osEPiWriteIo)
UNIMPL_LIBULTRA(osPfsIsPlug)
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
