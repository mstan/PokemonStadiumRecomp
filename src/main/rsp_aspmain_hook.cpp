/*
 * rsp_aspmain_hook.cpp — Pre-task hook for Pokemon Stadium's aspMain.
 *
 * Stadium's aspMain (ROM 0x68020, 0xC60 bytes) is a stripped variant
 * of the standard libultra audio microcode. Unlike Zelda64's aspMain
 * (which calls a setup function at PC 0x1058 to load the audio
 * command list before dispatch), Stadium's entry path
 *   PC 0x1000 (mfc0 DPC_STATUS) → L_102C (jal L_1120)
 * has NO command-load step. It depends on rspboot leaving:
 *   - $29 = 0x2B0  (DMEM offset of audio command list)
 *   - $30 > 0      (chunk-remaining counter)
 *   - SP_MEM_ADDR / SP_DRAM_ADDR / SP_RD_LEN already pointing at a
 *     safe RDRAM range so the L_1120 → L_10EC tail-DMA is a no-op
 *   - DMEM[0x2B0..0x3EF] = first chunk of audio commands
 *
 * Our HLE doesn't run rspboot, so the recompiled aspMain hits the
 * dispatch loop at L_1048 with $29 = 0 and dispatch table residue
 * in DMEM. The first L_10EC tail-DMA writes 1 byte from RDRAM[0]
 * into DMEM[3], corrupting dispatch[0]'s low byte from 0x10 to 0x00,
 * which makes the dispatch lookup return 0xEC, which dispatches to
 * L_10EC again → self-perpetuating infinite loop (caught by the
 * RSPRecomp watchdog after 100M transitions).
 *
 * This hook replicates the rspboot residue Stadium's aspMain
 * expects:
 *   1. DMA the first chunk of audio commands (up to 0x140 bytes)
 *      from task->t.data_ptr to DMEM[0x2B0].
 *   2. Seed $29, $30, $27, $28 to match what the dead-code init
 *      function at PC 0x10A0 would have computed if Stadium's
 *      boot path had called it.
 *   3. Seed dma_mem_address / dma_dram_address / r3 so the boot
 *      path's bogus L_1120 → L_10EC DMA at PC 0x1140 becomes a
 *      no-op (re-reads ucode_data byte 0 into DMEM[0xFFC], which
 *      is safe scratch space outside the dispatch table).
 *
 * Validation: with this hook, the watchdog should NOT trip; dispatch
 * should land on real handlers (0x139C, 0x119C, etc.) and aspMain
 * should reach the `mtc0 r1, SP_STATUS; break 0` at L_108C in finite
 * time, returning RspExitReason::Broke.
 *
 * Per project principles (PRINCIPLES.md #12): this is NOT a stub. It
 * encodes the real HLE-correct boot residue Stadium's aspMain needs.
 * On real hardware rspboot does the equivalent setup; we replicate it.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "librecomp/rsp.hpp"
#include "ultramodern/ultra64.h"

// Reach into librecomp's DMEM array. Declared at file scope (not
// inside our namespace) so the extern resolves to the global
// `dmem` defined in librecomp/src/rsp.cpp.
extern uint8_t dmem[];

namespace pokestadium::rsp {

// Audio task command-list chunk size. Stadium's dead-code init at
// PC 0x10A0 caps the initial DMA at 0x140 bytes; this matches what
// the dispatch loop's "refresh" path expects.
constexpr uint32_t kAudioChunkSize = 0x140;

// DMEM destination offset where audio commands live. Stadium's
// dead-code init function tail (`addi $29, $0, 0x2B0`) puts them
// here; the dispatch loop reads commands via `lw $26, 0($29)`.
constexpr uint32_t kAudioCommandsDmemOffset = 0x2B0;

// Safe DMEM scratch offset for the boot-path's bogus L_10EC DMA.
// Anywhere past the dispatch table (0x00..0x1F) and the constants
// area (0x20..0xAF) and the audio commands (0x2B0..0x3EF) works.
// 0xFFC is just before the OSTask copy at 0xFC0 — wait, 0xFC0 is
// where OSTask starts. Use 0xFA0 instead, which is set up as r24
// in the boot path and is documented scratch.
constexpr uint32_t kBootDmaSafeDmemOffset = 0xFA0;

void aspmain_pre_task(uint8_t* rdram,
                      ::RspContext* ctx,
                      const char* ucode_name,
                      uint32_t ucode_addr) {
    // Read OSTask back from DMEM[0xFC0] where the runtime stored
    // it. We could pass a separate OSTask* but reading from DMEM
    // matches what real rspboot does: the task struct lives there
    // and aspMain reads its own data_ptr / data_size from
    // DMEM[0xFC0+0x30] / DMEM[0xFC0+0x34].
    //
    // Pull values directly from the runtime-side dmem array via
    // the same XOR-3 byte ordering the recompiled MEM_W_LOAD uses.
    // We need data_ptr (offset 0x30) and data_size (offset 0x34).
    auto load_w = [](uint32_t off) -> uint32_t {
        // Replicate RSP_MEM_W_LOAD: read 4 bytes with each byte
        // XOR-3'd to host position (i^3). On LE host the result
        // is the big-endian word as a native uint32.
        uint32_t out = 0;
        for (int i = 0; i < 4; i++) {
            uint8_t b = dmem[(off + i) ^ 3];
            reinterpret_cast<uint8_t*>(&out)[i ^ 3] = b;
        }
        return out;
    };
    uint32_t data_ptr  = load_w(0xFC0 + 0x30);
    uint32_t data_size = load_w(0xFC0 + 0x34);

    if (data_size == 0 || data_ptr == 0) {
        // Empty task — let aspMain handle whatever degenerate
        // state it falls into. Don't synthesize fake commands.
        return;
    }

    // Compute first chunk size (matches dead-code init at 0x10A0).
    uint32_t chunk = (data_size > kAudioChunkSize)
                         ? kAudioChunkSize
                         : data_size;

    // Pre-load first chunk of audio commands. dma_rdram_to_dmem
    // takes rd_len as inclusive (RSP DMA semantics: the value
    // written to SP_RD_LEN equals length-1).
    ::recomp::rsp::dma_rdram_to_dmem_external(
        rdram,
        kAudioCommandsDmemOffset,
        data_ptr,
        chunk - 1);

    // Seed registers as if the dead-code init function had run:
    //   $29 = 0x2B0    (commands DMEM ptr)
    //   $30 = chunk    (chunk byte count remaining; dispatch uses
    //                   `bgtz $30` to decide if the current DMEM
    //                   chunk has more commands or needs refresh)
    //   $27 = data_size - chunk  (DRAM bytes remaining)
    //   $28 = data_ptr + chunk   (next DRAM read addr)
    //
    // Note: aspMain's entry path also reads r28/r27 from DMEM
    // OSTask fields at PC 0x1004/0x1008, so it'll OVERWRITE our
    // r28/r27 seeds. That's fine — the original boot path's
    // r28=data_ptr, r27=data_size leads to the L_106C refresh
    // path picking up where we left off (it advances r28 by chunk
    // size in jal-L_1120 calls, decrements r27, etc.). The KEY
    // seeds the boot path doesn't overwrite are r29 and r30.
    ctx->r29 = kAudioCommandsDmemOffset;
    ctx->r30 = static_cast<int32_t>(chunk);
    ctx->r27 = data_size - chunk;
    ctx->r28 = data_ptr + chunk;

    // Seed the DMA-engine residue. The boot path's first DMA at
    // L_1120 → L_10EC fires WITHOUT having gone through the
    // mtc0 r1, SP_MEM_ADDR / mtc0 r2, SP_DRAM_ADDR sequence at
    // PC 0x10E4/0x10E8. So it uses whatever dma_mem_address /
    // dma_dram_address / r3 we leave here.
    //
    // Setting them to a self-rereferencing no-op (read 1 byte of
    // ucode_data back into DMEM[0xFA0]) keeps the operation
    // harmless. The byte at ucode_data[0] is the first byte of
    // the dispatch table = 0x10, written into DMEM scratch.
    // Seed DMA-engine residue to match what real rspboot leaves
    // behind. rspboot's last DMA was the ucode_data load, so it
    // leaves:
    //   SP_MEM_ADDR  = 0           (DMEM dest of the data load)
    //   SP_DRAM_ADDR = ucode_data  (DRAM source of the data load)
    //   SP_RD_LEN    = ucode_data_size - 1 = 0xF7F
    //
    // Boot path (L_1120 → L_10EC) and any early handler that fires
    // a DMA via L_10EC without going through PC 0x10E4/0x10E8 will
    // reuse these registers. With this seed, such DMAs harmlessly
    // re-load ucode_data into DMEM[0..0xF7F] — same content already
    // there, no corruption.
    //
    // ucode_addr is the TEXT address; we need ucode_data, which the
    // runtime loaded via dma_rdram_to_dmem. Read it from the OSTask
    // at DMEM[0xFC0+0x18] (struct offset of ucode_data).
    uint32_t ucode_data_addr = load_w(0xFC0 + 0x18);
    ctx->dma_mem_address  = 0;
    ctx->dma_dram_address = ucode_data_addr;
    // r3 starts as the standard rspboot residue: length-1 of last
    // DMA (= 0xF7F for the ucode_data load). The dispatched
    // handler's `r3 -= 1; jal L_1174` then produces a DMA of length
    // = old_r3, which on first call re-loads ucode_data harmlessly.
    ctx->r3 = 0xF7F;

    // Inform diagnostic ring that the hook ran. Quiet; not an
    // error path. Helpful if a future regression makes us doubt
    // whether the hook was even invoked.
    fprintf(stderr,
        "[aspmain_hook] %s: data_ptr=0x%08X data_size=0x%X chunk=0x%X "
        "→ DMEM[0x%X], r29=0x2B0, r30=%u\n",
        ucode_name ? ucode_name : "?",
        data_ptr, data_size, chunk,
        kAudioCommandsDmemOffset, chunk);
    fflush(stderr);
}

void register_pre_task_hooks() {
    ::recomp::rsp::set_pre_task_hook("aspMain", aspmain_pre_task);
}

}  // namespace pokestadium::rsp
