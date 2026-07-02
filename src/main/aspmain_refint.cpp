/*
 * aspmain_refint.cpp — reference RSP interpreter for the aspMain ucode.
 *
 * Ground-truth engine for the audio-crackle differential: an INDEPENDENT
 * decoder/executor for the combined rspboot+aspMain IMEM image
 * (aspmain_combined.bin), run against the same captured task inputs as
 * the RSPRecomp-generated rsp/aspMain.cpp. Vector-op semantics reuse the
 * same ares-derived RSP class the recompiled code links (that unit is
 * exonerated: SISD-vs-SIMD A/B both crackle), so a divergence between
 * this interpreter and the recompiled code isolates what RSPRecomp
 * EMITS: instruction decode, scalar ops, control flow + delay slots,
 * DMEM addressing, and the DMA engine — all implemented here from the
 * binary and hardware semantics, not from the generated C.
 *
 * Deliberately mirrored interface facts (not independent, by design —
 * they define the recomp's HLE environment, verified against
 * rsp_recomp.cpp and the generated output):
 *   - all branch/jump/link targets normalize as (addr | 0x1000) & 0x1FFF
 *     (the combined-image PC domain [0x1000, 0x2080));
 *   - every MFC0 reads 0 (SP_DMA_BUSY/FULL, SP_SEMAPHORE, SP_STATUS,
 *     DPC_STATUS are HLE'd to idle);
 *   - MTC0 SP_SEMAPHORE is a no-op; MTC0 SP_STATUS is recorded only;
 *   - DMA auto-increments SP_MEM_ADDR/SP_DRAM_ADDR after the transfer.
 *
 * Entry: psr_aspmain_refint_run() — the caller (aspmain_replay.cpp)
 * preloads the shared global `dmem` and a guest RDRAM, seeds registers
 * from the capture, and diffs the results against the recompiled run.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <vector>

#include "librecomp/rsp.hpp"
#include "librecomp/rsp_vu_impl.hpp"

extern uint8_t dmem[];

namespace {

constexpr uint32_t kPcBase = 0x1000;
constexpr uint32_t kImemBytes = 0x1080;
constexpr uint64_t kStepCap = 20000000ull;

uint8_t g_imem[kImemBytes];
bool g_imem_loaded = false;

struct RefDma {
    bool write;
    uint32_t dmem_addr, dram_addr, len_reg;
};

struct Ref {
    uint32_t r[32] = {};
    RSP vu{};
    uint32_t dma_mem = 0, dma_dram = 0;
    uint32_t sp_status_last = 0;
    uint8_t* rdram = nullptr;
    std::vector<RefDma> dmas;
    bool broke = false;
    bool error = false;
    char err[256] = {};
    uint64_t steps = 0;
};

inline uint32_t norm_pc(uint32_t a) { return (a | kPcBase) & 0x1FFFu; }

inline uint32_t fetch(Ref& s, uint32_t pc) {
    uint32_t off = pc - kPcBase;
    if (pc < kPcBase || off + 4 > kImemBytes) {
        if (!s.error) {
            snprintf(s.err, sizeof(s.err), "fetch outside IMEM: pc=0x%04X", pc);
            s.error = true;
        }
        return 0;
    }
    return ((uint32_t)g_imem[off] << 24) | ((uint32_t)g_imem[off + 1] << 16) |
           ((uint32_t)g_imem[off + 2] << 8) | (uint32_t)g_imem[off + 3];
}

// ── Independent scalar DMEM access: big-endian semantics over the host
// XOR-3 array, per-byte 4 KiB wrap, unaligned allowed as on hardware ──
inline uint8_t d_lb(uint32_t a) { return dmem[(a & 0xFFFu) ^ 3]; }
inline void d_sb(uint32_t a, uint8_t v) { dmem[(a & 0xFFFu) ^ 3] = v; }
inline uint16_t d_lh(uint32_t a) {
    return (uint16_t)((d_lb(a) << 8) | d_lb(a + 1));
}
inline void d_sh(uint32_t a, uint16_t v) {
    d_sb(a, (uint8_t)(v >> 8));
    d_sb(a + 1, (uint8_t)v);
}
inline uint32_t d_lw(uint32_t a) {
    return ((uint32_t)d_lb(a) << 24) | ((uint32_t)d_lb(a + 1) << 16) |
           ((uint32_t)d_lb(a + 2) << 8) | (uint32_t)d_lb(a + 3);
}
inline void d_sw(uint32_t a, uint32_t v) {
    d_sb(a, (uint8_t)(v >> 24));
    d_sb(a + 1, (uint8_t)(v >> 16));
    d_sb(a + 2, (uint8_t)(v >> 8));
    d_sb(a + 3, (uint8_t)v);
}

// Guest RDRAM byte access for the DMA engine (host XOR-3 layout, 24-bit
// physical addressing — matches how the runtime maps guest memory).
inline uint8_t rd_b(const uint8_t* rdram, uint32_t p) {
    return rdram[(p & 0xFFFFFFu) ^ 3];
}
inline void wr_b(uint8_t* rdram, uint32_t p, uint8_t v) {
    rdram[(p & 0xFFFFFFu) ^ 3] = v;
}

// ── DMA engine, implemented from the SP_RD_LEN/SP_WR_LEN hardware
// encoding: len[11:0] (8-byte-rounded block), count[19:12], skip[31:20]
// (8-byte aligned). Addresses auto-increment after the transfer. ──
void dma_read(Ref& s, uint32_t len_reg) {
    s.dmas.push_back({false, s.dma_mem, s.dma_dram, len_reg});
    const uint32_t block = (len_reg & 0xFF8u) + 8u;
    const uint32_t count = (len_reg >> 12) & 0xFFu;
    const uint32_t skip = (len_reg >> 20) & 0xFF8u;
    uint32_t cd = s.dma_mem & 0xFF8u;
    uint32_t cr = s.dma_dram & 0xFFFFF8u;
    for (uint32_t b = 0; b <= count; b++) {
        for (uint32_t i = 0; i < block; i++) d_sb(cd + i, rd_b(s.rdram, cr + i));
        cd = (cd + block) & 0xFFFu;
        cr = (cr + block) & 0xFFFFF8u;
        if (b != count) cr = (cr + skip) & 0xFFFFF8u;
    }
    s.dma_mem += (count + 1u) * block;
    s.dma_dram += (count + 1u) * block + count * skip;
}

void dma_write(Ref& s, uint32_t len_reg) {
    s.dmas.push_back({true, s.dma_mem, s.dma_dram, len_reg});
    const uint32_t block = (len_reg & 0xFF8u) + 8u;
    const uint32_t count = (len_reg >> 12) & 0xFFu;
    const uint32_t skip = (len_reg >> 20) & 0xFF8u;
    uint32_t cd = s.dma_mem & 0xFF8u;
    uint32_t cr = s.dma_dram & 0xFFFFF8u;
    for (uint32_t b = 0; b <= count; b++) {
        for (uint32_t i = 0; i < block; i++) wr_b(s.rdram, cr + i, d_lb(cd + i));
        cd = (cd + block) & 0xFFFu;
        cr = (cr + block) & 0xFFFFF8u;
        if (b != count) cr = (cr + skip) & 0xFFFFF8u;
    }
    s.dma_mem += (count + 1u) * block;
    s.dma_dram += (count + 1u) * block + count * skip;
}

// Dispatch a runtime element index to the compile-time template argument
// the VU methods require. `body(E)` is invoked with E as a
// std::integral_constant-like value usable as `E.value`.
template <typename F>
inline void dispatch_e16(uint32_t e, F&& body) {
    switch (e & 0xF) {
    case 0: body(std::integral_constant<u8, 0>{}); break;
    case 1: body(std::integral_constant<u8, 1>{}); break;
    case 2: body(std::integral_constant<u8, 2>{}); break;
    case 3: body(std::integral_constant<u8, 3>{}); break;
    case 4: body(std::integral_constant<u8, 4>{}); break;
    case 5: body(std::integral_constant<u8, 5>{}); break;
    case 6: body(std::integral_constant<u8, 6>{}); break;
    case 7: body(std::integral_constant<u8, 7>{}); break;
    case 8: body(std::integral_constant<u8, 8>{}); break;
    case 9: body(std::integral_constant<u8, 9>{}); break;
    case 10: body(std::integral_constant<u8, 10>{}); break;
    case 11: body(std::integral_constant<u8, 11>{}); break;
    case 12: body(std::integral_constant<u8, 12>{}); break;
    case 13: body(std::integral_constant<u8, 13>{}); break;
    case 14: body(std::integral_constant<u8, 14>{}); break;
    case 15: body(std::integral_constant<u8, 15>{}); break;
    }
}

struct Ctl {
    bool is_branch = false;
    bool taken = false;
    uint32_t target = 0;
    bool link = false;
    int link_reg = 31;
};

Ctl exec(Ref& s, uint32_t instr, uint32_t pc) {
    Ctl c{};
    if (instr == 0) return c;   // nop
    const uint32_t op = instr >> 26;
    const int rs = (instr >> 21) & 31;
    const int rt = (instr >> 16) & 31;
    const int rd = (instr >> 11) & 31;
    const int sa = (instr >> 6) & 31;
    const uint16_t imm = (uint16_t)instr;
    const int32_t simm = (int16_t)imm;
    uint32_t* R = s.r;
    auto setr = [&](int i, uint32_t v) { if (i != 0) R[i] = v; };
    auto btarget = [&](void) { return norm_pc(pc + 4 + ((uint32_t)simm << 2)); };

    switch (op) {
    case 0x00:   // SPECIAL
        switch (instr & 0x3F) {
        case 0x00: setr(rd, R[rt] << sa); break;                        // sll
        case 0x02: setr(rd, R[rt] >> sa); break;                        // srl
        case 0x03: setr(rd, (uint32_t)((int32_t)R[rt] >> sa)); break;   // sra
        case 0x04: setr(rd, R[rt] << (R[rs] & 31)); break;              // sllv
        case 0x06: setr(rd, R[rt] >> (R[rs] & 31)); break;              // srlv
        case 0x07: setr(rd, (uint32_t)((int32_t)R[rt] >> (R[rs] & 31))); break; // srav
        case 0x08:   // jr
            c.is_branch = c.taken = true;
            c.target = norm_pc(R[rs]);
            break;
        case 0x09:   // jalr
            c.is_branch = c.taken = c.link = true;
            c.link_reg = rd;
            c.target = norm_pc(R[rs]);
            break;
        case 0x0D: s.broke = true; break;                               // break
        case 0x20: case 0x21: setr(rd, R[rs] + R[rt]); break;           // add/addu
        case 0x22: case 0x23: setr(rd, R[rs] - R[rt]); break;           // sub/subu
        case 0x24: setr(rd, R[rs] & R[rt]); break;                      // and
        case 0x25: setr(rd, R[rs] | R[rt]); break;                      // or
        case 0x26: setr(rd, R[rs] ^ R[rt]); break;                      // xor
        case 0x27: setr(rd, ~(R[rs] | R[rt])); break;                   // nor
        case 0x2A: setr(rd, (uint32_t)((int32_t)R[rs] < (int32_t)R[rt])); break; // slt
        case 0x2B: setr(rd, (uint32_t)(R[rs] < R[rt])); break;          // sltu
        default:
            snprintf(s.err, sizeof(s.err), "unhandled SPECIAL funct 0x%02X pc=0x%04X",
                     instr & 0x3F, pc);
            s.error = true;
        }
        break;
    case 0x01:   // REGIMM
        switch (rt) {
        case 0x00: c.is_branch = true; c.taken = (int32_t)R[rs] < 0;  c.target = btarget(); break;  // bltz
        case 0x01: c.is_branch = true; c.taken = (int32_t)R[rs] >= 0; c.target = btarget(); break;  // bgez
        case 0x10: c.is_branch = true; c.taken = (int32_t)R[rs] < 0;  c.target = btarget(); c.link = true; break; // bltzal
        case 0x11: c.is_branch = true; c.taken = (int32_t)R[rs] >= 0; c.target = btarget(); c.link = true; break; // bgezal
        default:
            snprintf(s.err, sizeof(s.err), "unhandled REGIMM rt 0x%02X pc=0x%04X", rt, pc);
            s.error = true;
        }
        break;
    case 0x02:   // j
        c.is_branch = c.taken = true;
        c.target = norm_pc((instr & 0x03FFFFFFu) << 2);
        break;
    case 0x03:   // jal
        c.is_branch = c.taken = c.link = true;
        c.target = norm_pc((instr & 0x03FFFFFFu) << 2);
        break;
    case 0x04: c.is_branch = true; c.taken = R[rs] == R[rt]; c.target = btarget(); break;  // beq
    case 0x05: c.is_branch = true; c.taken = R[rs] != R[rt]; c.target = btarget(); break;  // bne
    case 0x06: c.is_branch = true; c.taken = (int32_t)R[rs] <= 0; c.target = btarget(); break; // blez
    case 0x07: c.is_branch = true; c.taken = (int32_t)R[rs] > 0;  c.target = btarget(); break; // bgtz
    case 0x08: case 0x09: setr(rt, R[rs] + (uint32_t)simm); break;   // addi/addiu
    case 0x0A: setr(rt, (uint32_t)((int32_t)R[rs] < simm)); break;   // slti
    case 0x0B: setr(rt, (uint32_t)(R[rs] < (uint32_t)simm)); break;  // sltiu
    case 0x0C: setr(rt, R[rs] & imm); break;                         // andi
    case 0x0D: setr(rt, R[rs] | imm); break;                         // ori
    case 0x0E: setr(rt, R[rs] ^ imm); break;                         // xori
    case 0x0F: setr(rt, (uint32_t)imm << 16); break;                 // lui
    case 0x10:   // COP0
        if (rs == 0x00) {
            // MFC0 — the recompiled environment HLEs every cop0 read to 0.
            setr(rt, 0);
        } else if (rs == 0x04) {
            // MTC0
            switch (rd) {
            case 0: s.dma_mem = R[rt]; break;        // SP_MEM_ADDR
            case 1: s.dma_dram = R[rt]; break;       // SP_DRAM_ADDR
            case 2: dma_read(s, R[rt]); break;       // SP_RD_LEN
            case 3: dma_write(s, R[rt]); break;      // SP_WR_LEN
            case 4: s.sp_status_last = R[rt]; break; // SP_STATUS (recorded)
            case 7: break;                            // SP_SEMAPHORE (no-op)
            default:
                snprintf(s.err, sizeof(s.err), "unhandled MTC0 rd=%d pc=0x%04X", rd, pc);
                s.error = true;
            }
        } else {
            snprintf(s.err, sizeof(s.err), "unhandled COP0 rs=0x%02X pc=0x%04X", rs, pc);
            s.error = true;
        }
        break;
    case 0x12: {  // COP2 — vector unit
        RSP& v = s.vu;
        if (instr & (1u << 25)) {
            const uint32_t e = (instr >> 21) & 0xF;
            const int vt = (instr >> 16) & 31;
            const int vs2 = (instr >> 11) & 31;
            const int vd = (instr >> 6) & 31;
            const uint8_t de = (uint8_t)((instr >> 11) & 31);
            const uint32_t funct = instr & 0x3F;
#define VOP3(NAME)                                                        \
    dispatch_e16(e, [&](auto E) {                                         \
        v.NAME<decltype(E)::value>(v.vpu.r[vd], v.vpu.r[vs2], v.vpu.r[vt]); \
    })
#define VOPD(NAME)                                                        \
    dispatch_e16(e, [&](auto E) {                                         \
        v.NAME<decltype(E)::value>(v.vpu.r[vd], (u8)(de & 7), v.vpu.r[vt]); \
    })
            switch (funct) {
            case 0x00: VOP3(VMULF); break;
            case 0x01: VOP3(VMULU); break;
            case 0x04: VOP3(VMUDL); break;
            case 0x05: VOP3(VMUDM); break;
            case 0x06: VOP3(VMUDN); break;
            case 0x07: VOP3(VMUDH); break;
            case 0x08: VOP3(VMACF); break;
            case 0x09: VOP3(VMACU); break;
            case 0x0C: VOP3(VMADL); break;
            case 0x0D: VOP3(VMADM); break;
            case 0x0E: VOP3(VMADN); break;
            case 0x0F: VOP3(VMADH); break;
            case 0x10: VOP3(VADD); break;
            case 0x11: VOP3(VSUB); break;
            case 0x13: VOP3(VABS); break;
            case 0x14: VOP3(VADDC); break;
            case 0x15: VOP3(VSUBC); break;
            case 0x1D:   // VSAR — e selects ACC hi/mid/lo (8/9/10)
                dispatch_e16(e, [&](auto E) {
                    v.VSAR<decltype(E)::value>(v.vpu.r[vd], v.vpu.r[vs2]);
                });
                break;
            case 0x20: VOP3(VLT); break;
            case 0x21: VOP3(VEQ); break;
            case 0x22: VOP3(VNE); break;
            case 0x23: VOP3(VGE); break;
            case 0x24: VOP3(VCL); break;
            case 0x25: VOP3(VCH); break;
            case 0x26: VOP3(VCR); break;
            case 0x27: VOP3(VMRG); break;
            case 0x28: VOP3(VAND); break;
            case 0x29: VOP3(VNAND); break;
            case 0x2A: VOP3(VOR); break;
            case 0x2B: VOP3(VNOR); break;
            case 0x2C: VOP3(VXOR); break;
            case 0x2D: VOP3(VNXOR); break;
            case 0x30: VOPD(VRCP); break;
            case 0x31: VOPD(VRCPL); break;
            case 0x32: VOPD(VRCPH); break;
            case 0x33: VOPD(VMOV); break;
            case 0x34: VOPD(VRSQ); break;
            case 0x35: VOPD(VRSQL); break;
            case 0x36: VOPD(VRSQH); break;
            case 0x37: v.VNOP(); break;   // vnop
            case 0x3F: v.VNOP(); break;   // vnull
            default:
                snprintf(s.err, sizeof(s.err), "unhandled vector funct 0x%02X pc=0x%04X",
                         funct, pc);
                s.error = true;
            }
#undef VOP3
#undef VOPD
        } else {
            const uint32_t e = (instr >> 7) & 0xF;
            switch (rs) {
            case 0x00: {   // MFC2
                uint32_t tmp = 0;
                dispatch_e16(e, [&](auto E) {
                    v.MFC2<decltype(E)::value>(tmp, v.vpu.r[rd]);
                });
                setr(rt, tmp);
                break;
            }
            case 0x02: {   // CFC2
                uint32_t tmp = 0;
                v.CFC2(tmp, (u8)(rd & 3));
                setr(rt, tmp);
                break;
            }
            case 0x04: {   // MTC2
                uint32_t tmp = R[rt];
                dispatch_e16(e, [&](auto E) {
                    v.MTC2<decltype(E)::value>(tmp, v.vpu.r[rd]);
                });
                break;
            }
            case 0x06: {   // CTC2
                uint32_t tmp = R[rt];
                v.CTC2(tmp, (u8)(rd & 3));
                break;
            }
            default:
                snprintf(s.err, sizeof(s.err), "unhandled COP2 rs=0x%02X pc=0x%04X", rs, pc);
                s.error = true;
            }
        }
        break;
    }
    case 0x20: setr(rt, (uint32_t)(int32_t)(int8_t)d_lb(R[rs] + simm)); break;   // lb
    case 0x21: setr(rt, (uint32_t)(int32_t)(int16_t)d_lh(R[rs] + simm)); break;  // lh
    case 0x23: setr(rt, d_lw(R[rs] + simm)); break;                              // lw
    case 0x24: setr(rt, d_lb(R[rs] + simm)); break;                              // lbu
    case 0x25: setr(rt, d_lh(R[rs] + simm)); break;                              // lhu
    case 0x28: d_sb(R[rs] + simm, (uint8_t)R[rt]); break;                        // sb
    case 0x29: d_sh(R[rs] + simm, (uint16_t)R[rt]); break;                       // sh
    case 0x2B: d_sw(R[rs] + simm, R[rt]); break;                                 // sw
    case 0x32: {  // LWC2 — vector loads
        const int vt = rt;
        const uint32_t e = (instr >> 7) & 0xF;
        const int8_t off7 =
            (int8_t)((instr & 0x40) ? (instr & 0x7F) | 0x80 : (instr & 0x7F));
        RSP& v = s.vu;
        const uint32_t funct = (instr >> 11) & 0x1F;
#define VLS(NAME)                                                         \
    dispatch_e16(e, [&](auto E) {                                         \
        v.NAME<decltype(E)::value>(v.vpu.r[vt], R[rs], off7);             \
    })
        switch (funct) {
        case 0x00: VLS(LBV); break;
        case 0x01: VLS(LSV); break;
        case 0x02: VLS(LLV); break;
        case 0x03: VLS(LDV); break;
        case 0x04: VLS(LQV); break;
        case 0x05: VLS(LRV); break;
        case 0x06: VLS(LPV); break;
        case 0x07: VLS(LUV); break;
        case 0x08: VLS(LHV); break;
        case 0x09: VLS(LFV); break;
        case 0x0B:
            dispatch_e16(e, [&](auto E) {
                v.LTV<decltype(E)::value>((u8)vt, R[rs], off7);
            });
            break;
        default:
            snprintf(s.err, sizeof(s.err), "unhandled LWC2 funct 0x%02X pc=0x%04X",
                     funct, pc);
            s.error = true;
        }
#undef VLS
        break;
    }
    case 0x3A: {  // SWC2 — vector stores
        const int vt = rt;
        const uint32_t e = (instr >> 7) & 0xF;
        const int8_t off7 =
            (int8_t)((instr & 0x40) ? (instr & 0x7F) | 0x80 : (instr & 0x7F));
        RSP& v = s.vu;
        const uint32_t funct = (instr >> 11) & 0x1F;
#define VSS(NAME)                                                         \
    dispatch_e16(e, [&](auto E) {                                         \
        v.NAME<decltype(E)::value>(v.vpu.r[vt], R[rs], off7);             \
    })
        switch (funct) {
        case 0x00: VSS(SBV); break;
        case 0x01: VSS(SSV); break;
        case 0x02: VSS(SLV); break;
        case 0x03: VSS(SDV); break;
        case 0x04: VSS(SQV); break;
        case 0x05: VSS(SRV); break;
        case 0x06: VSS(SPV); break;
        case 0x07: VSS(SUV); break;
        case 0x08: VSS(SHV); break;
        case 0x09: VSS(SFV); break;
        case 0x0A: VSS(SWV); break;
        case 0x0B:
            dispatch_e16(e, [&](auto E) {
                v.STV<decltype(E)::value>((u8)vt, R[rs], off7);
            });
            break;
        default:
            snprintf(s.err, sizeof(s.err), "unhandled SWC2 funct 0x%02X pc=0x%04X",
                     funct, pc);
            s.error = true;
        }
#undef VSS
        break;
    }
    default:
        snprintf(s.err, sizeof(s.err), "unhandled opcode 0x%02X pc=0x%04X", op, pc);
        s.error = true;
    }
    return c;
}

}  // namespace

// Runs the reference interpreter over the shared global `dmem` and the
// given guest RDRAM, starting from the recomp entry (repr PC 0x1000),
// with registers/VU seeded from the capture's RspContext. Writes a DMA
// trace (recomp_dma.txt-style) to `dma_txt_path` if non-null.
// Returns 0 = clean break, 1 = execution error, 2 = step cap (hang).
extern "C" int psr_aspmain_refint_run(uint8_t* rdram,
                                      const RspContext* seed,
                                      const char* imem_path,
                                      const char* dma_txt_path) {
    if (!g_imem_loaded) {
        FILE* f = fopen(imem_path, "rb");
        if (!f) {
            fprintf(stderr, "[refint] cannot open IMEM image %s\n", imem_path);
            return 1;
        }
        size_t got = fread(g_imem, 1, kImemBytes, f);
        fclose(f);
        if (got != kImemBytes) {
            fprintf(stderr, "[refint] IMEM image short: %zu bytes\n", got);
            return 1;
        }
        g_imem_loaded = true;
    }

    Ref s{};
    s.rdram = rdram;
    // Seed scalar registers r1..r31 + DMA addresses + the full VU state
    // from the capture (the same values the recompiled run starts from).
    const uint32_t* gpr = &seed->r1;
    for (int i = 1; i <= 31; i++) s.r[i] = gpr[i - 1];
    s.dma_mem = seed->dma_mem_address;
    s.dma_dram = seed->dma_dram_address;
    s.vu = seed->rsp;

    uint32_t pc = kPcBase;   // recomp entry (blob offset 0)
    while (!s.broke && !s.error && s.steps < kStepCap) {
        s.steps++;
        uint32_t instr = fetch(s, pc);
        if (s.error) break;
        Ctl c = exec(s, instr, pc);
        if (s.broke || s.error) break;
        if (c.is_branch) {
            if (c.link && c.link_reg != 0) {
                s.r[c.link_reg] = norm_pc(pc + 8);
            }
            // Delay slot — must not itself branch.
            uint32_t dinstr = fetch(s, pc + 4);
            if (s.error) break;
            Ctl dc = exec(s, dinstr, pc + 4);
            if (dc.is_branch) {
                fprintf(stderr, "[refint] branch in delay slot at pc=0x%04X\n", pc + 4);
                return 1;
            }
            if (s.broke || s.error) break;
            pc = c.taken ? c.target : norm_pc(pc + 8);
        } else {
            pc = norm_pc(pc + 4);
        }
    }

    if (dma_txt_path) {
        if (FILE* f = fopen(dma_txt_path, "w")) {
            fprintf(f, "# reference-interpreter aspMain DMA trace\n");
            unsigned idx = 0;
            for (const RefDma& d : s.dmas) {
                uint32_t block = (d.len_reg & 0xFF8u) + 8u;
                fprintf(f, "%3u %s dram=0x%06X dmem=0x%03X len=0x%X\n",
                        idx++, d.write ? "WR" : "RD",
                        d.dram_addr & 0xFFFFFF, d.dmem_addr & 0xFFF, block);
            }
            fclose(f);
        }
    }

    if (s.error) {
        fprintf(stderr, "[refint] ERROR after %llu steps: %s\n",
                (unsigned long long)s.steps, s.err);
        return 1;
    }
    if (!s.broke) {
        fprintf(stderr, "[refint] step cap (%llu) exceeded — ucode hang\n",
                (unsigned long long)kStepCap);
        return 2;
    }
    fprintf(stderr, "[refint] clean break after %llu steps, %zu DMAs\n",
            (unsigned long long)s.steps, s.dmas.size());
    return 0;
}
