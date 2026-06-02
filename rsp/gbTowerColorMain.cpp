#include "librecomp/rsp.hpp"
#include "librecomp/rsp_vu_impl.hpp"
RspExitReason gbTowerColorMain_impl(uint8_t* rdram, RspContext* ctx) {
    uint32_t&                 r1 = ctx->r1;   uint32_t&  r2 = ctx->r2;   uint32_t&  r3 = ctx->r3;   uint32_t&  r4 = ctx->r4;   uint32_t&  r5 = ctx->r5;   uint32_t&  r6 = ctx->r6;   uint32_t&  r7 = ctx->r7;
    uint32_t&  r8 = ctx->r8;  uint32_t&  r9 = ctx->r9;   uint32_t& r10 = ctx->r10; uint32_t& r11 = ctx->r11; uint32_t& r12 = ctx->r12; uint32_t& r13 = ctx->r13; uint32_t& r14 = ctx->r14; uint32_t& r15 = ctx->r15;
    uint32_t& r16 = ctx->r16; uint32_t& r17 = ctx->r17; uint32_t& r18 = ctx->r18; uint32_t& r19 = ctx->r19; uint32_t& r20 = ctx->r20; uint32_t& r21 = ctx->r21; uint32_t& r22 = ctx->r22; uint32_t& r23 = ctx->r23;
    uint32_t& r24 = ctx->r24; uint32_t& r25 = ctx->r25; uint32_t& r26 = ctx->r26; uint32_t& r27 = ctx->r27; uint32_t& r28 = ctx->r28; uint32_t& r29 = ctx->r29; uint32_t& r30 = ctx->r30; uint32_t& r31 = ctx->r31;
    uint32_t& dma_mem_address = ctx->dma_mem_address; uint32_t& dma_dram_address = ctx->dma_dram_address; uint32_t& jump_target = ctx->jump_target;
    const char * debug_file = NULL; int debug_line = 0;
    RSP& rsp = ctx->rsp;
    r1 = 0xFC0;
    ctx->watchdog_count = 0;
    // mfc0        $26, DPC_CLOCK
    r26 = 0;
    // lqv         $v0[0], 0x7C0($zero)
    rsp.LQV<0>(rsp.vpu.r[0], 0, -0X4);
    // lqv         $v1[0], 0x7D0($zero)
    rsp.LQV<0>(rsp.vpu.r[1], 0, -0X3);
    // lqv         $v2[0], 0x7E0($zero)
    rsp.LQV<0>(rsp.vpu.r[2], 0, -0X2);
    // lqv         $v3[0], 0x7F0($zero)
    rsp.LQV<0>(rsp.vpu.r[3], 0, -0X1);
    // lw          $8, 0xFE0($zero)
    r8 = RSP_MEM_W_LOAD(0XFE0, 0);
    // lw          $9, 0xFEC($zero)
    r9 = RSP_MEM_W_LOAD(0XFEC, 0);
    // lw          $11, 0xFC8($zero)
    r11 = RSP_MEM_W_LOAD(0XFC8, 0);
    // lw          $12, 0xFF0($zero)
    r12 = RSP_MEM_W_LOAD(0XFF0, 0);
    // ori         $10, $zero, 0x6000
    r10 = 0 | 0X6000;
    // addiu       $1, $zero, 0x0
    r1 = RSP_ADD32(0, 0X0);
    // addiu       $4, $zero, 0x0
    r4 = RSP_ADD32(0, 0X0);
L_1030:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1030;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1030 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $10, $10, -0x1
    r10 = RSP_ADD32(r10, -0X1);
    // andi        $6, $10, 0xFFF
    r6 = r10 & 0XFFF;
    // jal         0x1648
    r31 = 0x1040;
    // or          $5, $8, $zero
    r5 = r8 | 0;
    goto L_1648;
    // or          $5, $8, $zero
    r5 = r8 | 0;
L_1040:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1040;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1040 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // jal         0x1680
    r31 = 0x1048;
    // or          $5, $9, $zero
    r5 = r9 | 0;
    goto L_1680;
    // or          $5, $9, $zero
    r5 = r9 | 0;
L_1048:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1048;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1048 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // xor         $10, $10, $6
    r10 = r10 ^ r6;
    // addiu       $6, $6, 0x1
    r6 = RSP_ADD32(r6, 0X1);
    // addu        $8, $8, $6
    r8 = RSP_ADD32(r8, r6);
    // bne         $10, $zero, L_1030
    if (r10 != 0) {
        // addu        $9, $9, $6
        r9 = RSP_ADD32(r9, r6);
        goto L_1030;
    }
    // addu        $9, $9, $6
    r9 = RSP_ADD32(r9, r6);
    // addiu       $4, $zero, 0xA00
    r4 = RSP_ADD32(0, 0XA00);
    // addiu       $6, $zero, 0x53F
    r6 = RSP_ADD32(0, 0X53F);
    // jal         0x1648
    r31 = 0x106C;
    // or          $5, $12, $zero
    r5 = r12 | 0;
    goto L_1648;
    // or          $5, $12, $zero
    r5 = r12 | 0;
L_106C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x106C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x106C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $4, $zero, 0x0
    r4 = RSP_ADD32(0, 0X0);
    // addiu       $6, $zero, 0xAF
    r6 = RSP_ADD32(0, 0XAF);
    // jal         0x1644
    r31 = 0x107C;
    // addiu       $5, $11, 0x9F0
    r5 = RSP_ADD32(r11, 0X9F0);
    goto L_1644;
    // addiu       $5, $11, 0x9F0
    r5 = RSP_ADD32(r11, 0X9F0);
L_107C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x107C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x107C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lui         $8, 0x1
    r8 = S32(U32(0X1) << 16);
    // mtc0        $8, SP_STATUS
    WRITE_SP_STATUS(r8);
    // sqv         $v0[0], 0x7C0($zero)
    rsp.SQV<0>(rsp.vpu.r[0], 0, -0X4);
    // sqv         $v1[0], 0x7D0($zero)
    rsp.SQV<0>(rsp.vpu.r[1], 0, -0X3);
    // sqv         $v2[0], 0x7E0($zero)
    rsp.SQV<0>(rsp.vpu.r[2], 0, -0X2);
    // sqv         $v3[0], 0x7F0($zero)
    rsp.SQV<0>(rsp.vpu.r[3], 0, -0X1);
    // addiu       $9, $9, 0x8
    r9 = RSP_ADD32(r9, 0X8);
    // sw          $9, 0xFE0($zero)
    RSP_MEM_W_STORE(0XFE0, 0, r9);
    // vxor        $v31, $v31, $v31
    rsp.VXOR<0>(rsp.vpu.r[31], rsp.vpu.r[31], rsp.vpu.r[31]);
    // lqv         $v26[0], 0x0($4)
    rsp.LQV<0>(rsp.vpu.r[26], r4, 0X0);
    // lqv         $v25[0], 0x10($4)
    rsp.LQV<0>(rsp.vpu.r[25], r4, 0X1);
    // lqv         $v24[0], 0x20($4)
    rsp.LQV<0>(rsp.vpu.r[24], r4, 0X2);
    // lqv         $v29[0], 0x30($4)
    rsp.LQV<0>(rsp.vpu.r[29], r4, 0X3);
    // lqv         $v27[0], 0x40($4)
    rsp.LQV<0>(rsp.vpu.r[27], r4, 0X4);
    // lqv         $v28[0], 0x50($4)
    rsp.LQV<0>(rsp.vpu.r[28], r4, 0X5);
    // lqv         $v22[0], 0x60($4)
    rsp.LQV<0>(rsp.vpu.r[22], r4, 0X6);
    // lqv         $v21[0], 0x70($4)
    rsp.LQV<0>(rsp.vpu.r[21], r4, 0X7);
    // lqv         $v19[0], 0x80($4)
    rsp.LQV<0>(rsp.vpu.r[19], r4, 0X8);
    // lqv         $v18[0], 0x90($4)
    rsp.LQV<0>(rsp.vpu.r[18], r4, 0X9);
    // lqv         $v15[0], 0xA0($4)
    rsp.LQV<0>(rsp.vpu.r[15], r4, 0XA);
    // addiu       $7, $zero, 0x13
    r7 = RSP_ADD32(0, 0X13);
L_10D0:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x10D0;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x10D0 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lw          $9, 0xFEC($zero)
    r9 = RSP_MEM_W_LOAD(0XFEC, 0);
    // addiu       $4, $zero, 0x200
    r4 = RSP_ADD32(0, 0X200);
    // lui         $6, 0x1000
    r6 = S32(U32(0X1000) << 16);
    // ori         $6, $6, 0x11FF
    r6 = r6 | 0X11FF;
    // andi        $8, $7, 0x3
    r8 = r7 & 0X3;
    // sll         $10, $8, 1
    r10 = S32(U32(r8) << 1);
    // addu        $10, $10, $8
    r10 = RSP_ADD32(r10, r8);
    // sll         $10, $10, 9
    r10 = S32(U32(r10) << 9);
    // addiu       $9, $9, 0x1000
    r9 = RSP_ADD32(r9, 0X1000);
    // jal         0x1648
    r31 = 0x10FC;
    // addu        $5, $9, $10
    r5 = RSP_ADD32(r9, r10);
    goto L_1648;
    // addu        $5, $9, $10
    r5 = RSP_ADD32(r9, r10);
L_10FC:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x10FC;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x10FC after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $4, $zero, 0x600
    r4 = RSP_ADD32(0, 0X600);
    // addiu       $8, $zero, 0x2A00
    r8 = RSP_ADD32(0, 0X2A00);
    // subu        $10, $8, $10
    r10 = RSP_SUB32(r8, r10);
    // addu        $5, $9, $10
    r5 = RSP_ADD32(r9, r10);
    // j           L_1648
    // addiu       $ra, $zero, 0x1168
    r31 = RSP_ADD32(0, 0X1168);
    goto L_1648;
    // addiu       $ra, $zero, 0x1168
    r31 = RSP_ADD32(0, 0X1168);
L_1114:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1114;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1114 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lw          $9, 0xFEC($zero)
    r9 = RSP_MEM_W_LOAD(0XFEC, 0);
    // addiu       $4, $zero, 0x200
    r4 = RSP_ADD32(0, 0X200);
    // addiu       $6, $zero, 0xFF
    r6 = RSP_ADD32(0, 0XFF);
    // andi        $8, $7, 0x3
    r8 = r7 & 0X3;
    // sll         $10, $8, 1
    r10 = S32(U32(r8) << 1);
    // addu        $10, $10, $8
    r10 = RSP_ADD32(r10, r8);
    // sll         $10, $10, 9
    r10 = S32(U32(r10) << 9);
    // addiu       $9, $9, 0x1200
    r9 = RSP_ADD32(r9, 0X1200);
    // jal         0x1648
    r31 = 0x113C;
    // addu        $5, $9, $10
    r5 = RSP_ADD32(r9, r10);
    goto L_1648;
    // addu        $5, $9, $10
    r5 = RSP_ADD32(r9, r10);
L_113C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x113C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x113C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $4, $zero, 0x400
    r4 = RSP_ADD32(0, 0X400);
    // jal         0x1648
    r31 = 0x1148;
    // addiu       $5, $5, 0x300
    r5 = RSP_ADD32(r5, 0X300);
    goto L_1648;
    // addiu       $5, $5, 0x300
    r5 = RSP_ADD32(r5, 0X300);
L_1148:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1148;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1148 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $4, $zero, 0x600
    r4 = RSP_ADD32(0, 0X600);
    // addiu       $8, $zero, 0x2A00
    r8 = RSP_ADD32(0, 0X2A00);
    // subu        $10, $8, $10
    r10 = RSP_SUB32(r8, r10);
    // jal         0x1648
    r31 = 0x115C;
    // addu        $5, $9, $10
    r5 = RSP_ADD32(r9, r10);
    goto L_1648;
    // addu        $5, $9, $10
    r5 = RSP_ADD32(r9, r10);
L_115C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x115C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x115C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $4, $zero, 0x800
    r4 = RSP_ADD32(0, 0X800);
    // jal         0x1648
    r31 = 0x1168;
    // addiu       $5, $5, 0x300
    r5 = RSP_ADD32(r5, 0X300);
    goto L_1648;
    // addiu       $5, $5, 0x300
    r5 = RSP_ADD32(r5, 0X300);
L_1168:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1168;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1168 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // jal         0x1658
    r31 = 0x1170;
    // nop

    goto L_1658;
    // nop

L_1170:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1170;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1170 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lbu         $16, 0xE79($zero)
    r16 = RSP_MEM_BU(0XE79, 0);
    // addiu       $28, $zero, 0x480
    r28 = RSP_ADD32(0, 0X480);
    // andi        $2, $7, 0x7
    r2 = r7 & 0X7;
    // xori        $3, $2, 0x7
    r3 = r2 ^ 0X7;
    // addiu       $29, $zero, -0x1
    r29 = RSP_ADD32(0, -0X1);
    // andi        $17, $7, 0x10
    r17 = r7 & 0X10;
    // ori         $17, $17, 0x80
    r17 = r17 | 0X80;
L_118C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x118C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x118C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // beq         $28, $zero, L_120C
    if (r28 == 0) {
        // andi        $8, $16, 0x7
        r8 = r16 & 0X7;
        goto L_120C;
    }
    // andi        $8, $16, 0x7
    r8 = r16 & 0X7;
    // beq         $8, $2, L_11A0
    if (r8 == r2) {
        // lbu         $16, 0x9F1($28)
        r16 = RSP_MEM_BU(0X9F1, r28);
        goto L_11A0;
    }
    // lbu         $16, 0x9F1($28)
    r16 = RSP_MEM_BU(0X9F1, r28);
    // bne         $8, $3, L_118C
    if (r8 != r3) {
        // addiu       $28, $28, -0x8
        r28 = RSP_ADD32(r28, -0X8);
        goto L_118C;
    }
L_11A0:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x11A0;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x11A0 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $28, $28, -0x8
    r28 = RSP_ADD32(r28, -0X8);
    // lbu         $9, 0xA00($28)
    r9 = RSP_MEM_BU(0XA00, r28);
    // lbu         $10, 0xA01($28)
    r10 = RSP_MEM_BU(0XA01, r28);
    // andi        $15, $8, 0x4
    r15 = r8 & 0X4;
    // sll         $15, $15, 8
    r15 = S32(U32(r15) << 8);
    // andi        $11, $9, 0x90
    r11 = r9 & 0X90;
    // bne         $11, $17, L_118C
    if (r11 != r17) {
        // andi        $11, $9, 0x8
        r11 = r9 & 0X8;
        goto L_118C;
    }
    // andi        $11, $9, 0x8
    r11 = r9 & 0X8;
    // sll         $11, $11, 5
    r11 = S32(U32(r11) << 5);
    // or          $11, $11, $10
    r11 = r11 | r10;
    // srl         $11, $11, 3
    r11 = S32(U32(r11) >> 3);
    // sll         $11, $11, 6
    r11 = S32(U32(r11) << 6);
    // beq         $11, $29, L_11EC
    if (r11 == r29) {
        // or          $29, $11, $zero
        r29 = r11 | 0;
        goto L_11EC;
    }
    // or          $29, $11, $zero
    r29 = r11 | 0;
    // lw          $5, 0xFEC($zero)
    r5 = RSP_MEM_W_LOAD(0XFEC, 0);
    // addiu       $4, $zero, 0xF80
    r4 = RSP_ADD32(0, 0XF80);
    // addiu       $6, $zero, 0x3F
    r6 = RSP_ADD32(0, 0X3F);
    // jal         0x1644
    r31 = 0x11EC;
    // addu        $5, $5, $11
    r5 = RSP_ADD32(r5, r11);
    goto L_1644;
    // addu        $5, $5, $11
    r5 = RSP_ADD32(r5, r11);
L_11EC:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x11EC;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x11EC after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lbu         $18, 0xA06($28)
    r18 = RSP_MEM_BU(0XA06, r28);
    // lbu         $19, 0xA03($28)
    r19 = RSP_MEM_BU(0XA03, r28);
    // srl         $21, $18, 3
    r21 = S32(U32(r18) >> 3);
    // sll         $21, $21, 1
    r21 = S32(U32(r21) << 1);
    // andi        $18, $18, 0x7
    r18 = r18 & 0X7;
    // xori        $18, $18, 0x7
    r18 = r18 ^ 0X7;
    // j           L_1690
    // addiu       $25, $zero, 0x118C
    r25 = RSP_ADD32(0, 0X118C);
    goto L_1690;
    // addiu       $25, $zero, 0x118C
    r25 = RSP_ADD32(0, 0X118C);
L_120C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x120C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x120C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lbu         $16, 0xE7A($zero)
    r16 = RSP_MEM_BU(0XE7A, 0);
    // addiu       $28, $zero, 0x480
    r28 = RSP_ADD32(0, 0X480);
    // andi        $2, $7, 0x7
    r2 = r7 & 0X7;
    // xori        $3, $2, 0x7
    r3 = r2 ^ 0X7;
    // andi        $17, $7, 0x10
    r17 = r7 & 0X10;
    // ori         $17, $17, 0xA0
    r17 = r17 | 0XA0;
L_1224:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1224;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1224 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // beq         $28, $zero, L_12A0
    if (r28 == 0) {
        // andi        $8, $16, 0x7
        r8 = r16 & 0X7;
        goto L_12A0;
    }
    // andi        $8, $16, 0x7
    r8 = r16 & 0X7;
    // beq         $8, $2, L_1238
    if (r8 == r2) {
        // lbu         $16, 0x9F2($28)
        r16 = RSP_MEM_BU(0X9F2, r28);
        goto L_1238;
    }
    // lbu         $16, 0x9F2($28)
    r16 = RSP_MEM_BU(0X9F2, r28);
    // bne         $8, $3, L_1224
    if (r8 != r3) {
        // addiu       $28, $28, -0x8
        r28 = RSP_ADD32(r28, -0X8);
        goto L_1224;
    }
L_1238:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1238;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1238 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $28, $28, -0x8
    r28 = RSP_ADD32(r28, -0X8);
    // lbu         $9, 0xA00($28)
    r9 = RSP_MEM_BU(0XA00, r28);
    // lbu         $10, 0xA02($28)
    r10 = RSP_MEM_BU(0XA02, r28);
    // andi        $15, $8, 0x4
    r15 = r8 & 0X4;
    // sll         $15, $15, 8
    r15 = S32(U32(r15) << 8);
    // andi        $11, $9, 0xB0
    r11 = r9 & 0XB0;
    // bne         $11, $17, L_1224
    if (r11 != r17) {
        // andi        $11, $9, 0x40
        r11 = r9 & 0X40;
        goto L_1224;
    }
    // andi        $11, $9, 0x40
    r11 = r9 & 0X40;
    // sll         $11, $11, 2
    r11 = S32(U32(r11) << 2);
    // or          $11, $11, $10
    r11 = r11 | r10;
    // srl         $11, $11, 3
    r11 = S32(U32(r11) >> 3);
    // sll         $11, $11, 6
    r11 = S32(U32(r11) << 6);
    // beq         $11, $29, L_1284
    if (r11 == r29) {
        // or          $29, $11, $zero
        r29 = r11 | 0;
        goto L_1284;
    }
    // or          $29, $11, $zero
    r29 = r11 | 0;
    // lw          $5, 0xFEC($zero)
    r5 = RSP_MEM_W_LOAD(0XFEC, 0);
    // addiu       $4, $zero, 0xF80
    r4 = RSP_ADD32(0, 0XF80);
    // addiu       $6, $zero, 0x3F
    r6 = RSP_ADD32(0, 0X3F);
    // jal         0x1644
    r31 = 0x1284;
    // addu        $5, $5, $11
    r5 = RSP_ADD32(r5, r11);
    goto L_1644;
    // addu        $5, $5, $11
    r5 = RSP_ADD32(r5, r11);
L_1284:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1284;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1284 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lbu         $18, 0xA03($28)
    r18 = RSP_MEM_BU(0XA03, r28);
    // addiu       $19, $zero, 0xA7
    r19 = RSP_ADD32(0, 0XA7);
    // subu        $22, $18, $19
    r22 = RSP_SUB32(r18, r19);
    // bgez        $22, L_1224
    if (RSP_SIGNED(r22) >= 0) {
        // addiu       $21, $zero, 0x0
        r21 = RSP_ADD32(0, 0X0);
        goto L_1224;
    }
    // addiu       $21, $zero, 0x0
    r21 = RSP_ADD32(0, 0X0);
    // j           L_1690
    // addiu       $25, $zero, 0x1224
    r25 = RSP_ADD32(0, 0X1224);
    goto L_1690;
    // addiu       $25, $zero, 0x1224
    r25 = RSP_ADD32(0, 0X1224);
L_12A0:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x12A0;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x12A0 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // beq         $7, $zero, L_12B8
    if (r7 == 0) {
        // andi        $8, $7, 0x10
        r8 = r7 & 0X10;
        goto L_12B8;
    }
    // andi        $8, $7, 0x10
    r8 = r7 & 0X10;
    // bne         $8, $zero, L_1114
    if (r8 != 0) {
        // addiu       $7, $7, -0x10
        r7 = RSP_ADD32(r7, -0X10);
        goto L_1114;
    }
    // addiu       $7, $7, -0x10
    r7 = RSP_ADD32(r7, -0X10);
    // j           L_10D0
    // addiu       $7, $7, 0x1F
    r7 = RSP_ADD32(r7, 0X1F);
    goto L_10D0;
    // addiu       $7, $7, 0x1F
    r7 = RSP_ADD32(r7, 0X1F);
L_12B8:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x12B8;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x12B8 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $2, $zero, -0x9C
    r2 = RSP_ADD32(0, -0X9C);
L_12BC:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x12BC;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x12BC after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lbu         $16, 0xF1C($2)
    r16 = RSP_MEM_BU(0XF1C, r2);
    // lbu         $18, 0xF1D($2)
    r18 = RSP_MEM_BU(0XF1D, r2);
    // lbu         $3, 0xF1F($2)
    r3 = RSP_MEM_BU(0XF1F, r2);
    // lbu         $19, 0xF1E($2)
    r19 = RSP_MEM_BU(0XF1E, r2);
    // addiu       $8, $16, -0x1
    r8 = RSP_ADD32(r16, -0X1);
    // sltiu       $8, $8, 0x9F
    r8 = r8 < 0X9F ? 1 : 0;
    // addiu       $9, $18, -0x1
    r9 = RSP_ADD32(r18, -0X1);
    // sltiu       $9, $9, 0xA7
    r9 = r9 < 0XA7 ? 1 : 0;
    // and         $8, $8, $9
    r8 = r8 & r9;
    // beq         $8, $zero, L_138C
    if (r8 == 0) {
        // addiu       $16, $16, -0x10
        r16 = RSP_ADD32(r16, -0X10);
        goto L_138C;
    }
    // addiu       $16, $16, -0x10
    r16 = RSP_ADD32(r16, -0X10);
    // bgez        $16, L_12F8
    if (RSP_SIGNED(r16) >= 0) {
        // addiu       $7, $zero, 0x7
        r7 = RSP_ADD32(0, 0X7);
        goto L_12F8;
    }
    // addiu       $7, $zero, 0x7
    r7 = RSP_ADD32(0, 0X7);
    // addu        $7, $7, $16
    r7 = RSP_ADD32(r7, r16);
    // addiu       $16, $zero, 0x0
    r16 = RSP_ADD32(0, 0X0);
L_12F8:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x12F8;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x12F8 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // sll         $28, $16, 3
    r28 = S32(U32(r16) << 3);
    // lbu         $9, 0xA00($28)
    r9 = RSP_MEM_BU(0XA00, r28);
    // lw          $5, 0xFEC($zero)
    r5 = RSP_MEM_W_LOAD(0XFEC, 0);
    // andi        $10, $3, 0x8
    r10 = r3 & 0X8;
    // sll         $10, $10, 9
    r10 = S32(U32(r10) << 9);
    // andi        $8, $19, 0xFE
    r8 = r19 & 0XFE;
    // sll         $8, $8, 4
    r8 = S32(U32(r8) << 4);
    // addu        $8, $8, $10
    r8 = RSP_ADD32(r8, r10);
    // addiu       $8, $8, 0x4000
    r8 = RSP_ADD32(r8, 0X4000);
    // addu        $5, $5, $8
    r5 = RSP_ADD32(r5, r8);
    // addiu       $4, $zero, 0xF80
    r4 = RSP_ADD32(0, 0XF80);
    // jal         0x1648
    r31 = 0x132C;
    // addiu       $6, $zero, 0x1F
    r6 = RSP_ADD32(0, 0X1F);
    goto L_1648;
    // addiu       $6, $zero, 0x1F
    r6 = RSP_ADD32(0, 0X1F);
L_132C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x132C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x132C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // andi        $10, $3, 0x40
    r10 = r3 & 0X40;
    // srl         $8, $10, 5
    r8 = S32(U32(r10) >> 5);
    // addiu       $8, $8, -0x1
    r8 = RSP_ADD32(r8, -0X1);
    // andi        $29, $8, 0xE
    r29 = r8 & 0XE;
    // sll         $20, $8, 1
    r20 = S32(U32(r8) << 1);
    // andi        $9, $9, 0x4
    r9 = r9 & 0X4;
    // bne         $9, $zero, L_1358
    if (r9 != 0) {
        // srl         $10, $10, 2
        r10 = S32(U32(r10) >> 2);
        goto L_1358;
    }
    // srl         $10, $10, 2
    r10 = S32(U32(r10) >> 2);
    // andi        $10, $19, 0x1
    r10 = r19 & 0X1;
    // sll         $10, $10, 4
    r10 = S32(U32(r10) << 4);
    // j           L_137C
    // or          $29, $29, $10
    r29 = r29 | r10;
    goto L_137C;
L_1358:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1358;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1358 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // or          $29, $29, $10
    r29 = r29 | r10;
    // bltz        $7, L_136C
    if (RSP_SIGNED(r7) < 0) {
        // xori        $15, $29, 0x10
        r15 = r29 ^ 0X10;
        goto L_136C;
    }
    // xori        $15, $29, 0x10
    r15 = r29 ^ 0X10;
    // j           L_1750
    // addiu       $25, $zero, 0x136C
    r25 = RSP_ADD32(0, 0X136C);
    goto L_1750;
    // addiu       $25, $zero, 0x136C
    r25 = RSP_ADD32(0, 0X136C);
L_136C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x136C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x136C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lbu         $16, 0xF1C($2)
    r16 = RSP_MEM_BU(0XF1C, r2);
    // or          $29, $15, $zero
    r29 = r15 | 0;
    // addiu       $7, $zero, 0x7
    r7 = RSP_ADD32(0, 0X7);
    // addiu       $16, $16, -0x8
    r16 = RSP_ADD32(r16, -0X8);
L_137C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x137C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x137C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // bltz        $7, L_138C
    if (RSP_SIGNED(r7) < 0) {
        // sll         $28, $16, 3
        r28 = S32(U32(r16) << 3);
        goto L_138C;
    }
    // sll         $28, $16, 3
    r28 = S32(U32(r16) << 3);
    // j           L_1750
    // addiu       $25, $zero, 0x138C
    r25 = RSP_ADD32(0, 0X138C);
    goto L_1750;
    // addiu       $25, $zero, 0x138C
    r25 = RSP_ADD32(0, 0X138C);
L_138C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x138C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x138C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // bltz        $2, L_12BC
    if (RSP_SIGNED(r2) < 0) {
        // addiu       $2, $2, 0x4
        r2 = RSP_ADD32(r2, 0X4);
        goto L_12BC;
    }
    // addiu       $2, $2, 0x4
    r2 = RSP_ADD32(r2, 0X4);
    // lw          $5, 0xFF8($zero)
    r5 = RSP_MEM_W_LOAD(0XFF8, 0);
    // addiu       $4, $zero, 0x800
    r4 = RSP_ADD32(0, 0X800);
    // addiu       $6, $zero, 0x1FF
    r6 = RSP_ADD32(0, 0X1FF);
    // addiu       $8, $5, 0x200
    r8 = RSP_ADD32(r5, 0X200);
    // jal         0x1644
    r31 = 0x13AC;
    // sw          $8, 0xFF8($zero)
    RSP_MEM_W_STORE(0XFF8, 0, r8);
    goto L_1644;
    // sw          $8, 0xFF8($zero)
    RSP_MEM_W_STORE(0XFF8, 0, r8);
L_13AC:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x13AC;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x13AC after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lbu         $9, 0xFFC($zero)
    r9 = RSP_MEM_BU(0XFFC, 0);
    // sll         $9, $9, 9
    r9 = S32(U32(r9) << 9);
    // mtc2        $9, $v4[0]
    rsp.MTC2<0>(r9, rsp.vpu.r[4]);
    // addu        $8, $4, $6
    r8 = RSP_ADD32(r4, r6);
    // addiu       $8, $8, 0x1
    r8 = RSP_ADD32(r8, 0X1);
L_13C0:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x13C0;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x13C0 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lqv         $v0[0], 0x7F0($8)
    rsp.LQV<0>(rsp.vpu.r[0], r8, -0X1);
    // addiu       $8, $8, -0x10
    r8 = RSP_ADD32(r8, -0X10);
    // vand        $v1, $v0, $v22[3]
    rsp.VAND<11>(rsp.vpu.r[1], rsp.vpu.r[0], rsp.vpu.r[22]);
    // vand        $v2, $v0, $v22[4]
    rsp.VAND<12>(rsp.vpu.r[2], rsp.vpu.r[0], rsp.vpu.r[22]);
    // vand        $v3, $v0, $v15[0]
    rsp.VAND<8>(rsp.vpu.r[3], rsp.vpu.r[0], rsp.vpu.r[15]);
    // vmudl       $v1, $v1, $v4[0]
    rsp.VMUDL<8>(rsp.vpu.r[1], rsp.vpu.r[1], rsp.vpu.r[4]);
    // vmudl       $v2, $v2, $v4[0]
    rsp.VMUDL<8>(rsp.vpu.r[2], rsp.vpu.r[2], rsp.vpu.r[4]);
    // vmudl       $v3, $v3, $v4[0]
    rsp.VMUDL<8>(rsp.vpu.r[3], rsp.vpu.r[3], rsp.vpu.r[4]);
    // vand        $v1, $v1, $v15[1]
    rsp.VAND<9>(rsp.vpu.r[1], rsp.vpu.r[1], rsp.vpu.r[15]);
    // vand        $v2, $v2, $v15[2]
    rsp.VAND<10>(rsp.vpu.r[2], rsp.vpu.r[2], rsp.vpu.r[15]);
    // vor         $v5, $v1, $v3
    rsp.VOR<0>(rsp.vpu.r[5], rsp.vpu.r[1], rsp.vpu.r[3]);
    // vor         $v5, $v5, $v2
    rsp.VOR<0>(rsp.vpu.r[5], rsp.vpu.r[5], rsp.vpu.r[2]);
    // vaddc       $v5, $v5, $v5
    rsp.VADDC<0>(rsp.vpu.r[5], rsp.vpu.r[5], rsp.vpu.r[5]);
    // vor         $v5, $v5, $v28[7]
    rsp.VOR<15>(rsp.vpu.r[5], rsp.vpu.r[5], rsp.vpu.r[28]);
    // bne         $8, $4, L_13C0
    if (r8 != r4) {
        // sqv         $v5[0], 0x0($8)
        rsp.SQV<0>(rsp.vpu.r[5], r8, 0X0);
        goto L_13C0;
    }
    // sqv         $v5[0], 0x0($8)
    rsp.SQV<0>(rsp.vpu.r[5], r8, 0X0);
    // lw          $8, 0xFF4($zero)
    r8 = RSP_MEM_W_LOAD(0XFF4, 0);
    // lbu         $7, 0xFF4($zero)
    r7 = RSP_MEM_BU(0XFF4, 0);
    // lhu         $9, 0xFFE($zero)
    r9 = RSP_MEM_HU_LOAD(0XFFE, 0);
    // lw          $29, 0xFE0($zero)
    r29 = RSP_MEM_W_LOAD(0XFE0, 0);
    // addiu       $2, $zero, 0x0
    r2 = RSP_ADD32(0, 0X0);
    // sll         $8, $8, 8
    r8 = S32(U32(r8) << 8);
    // srl         $3, $8, 8
    r3 = S32(U32(r8) >> 8);
    // andi        $19, $9, 0xF800
    r19 = r9 & 0XF800;
    // srl         $19, $19, 5
    r19 = S32(U32(r19) >> 5);
    // andi        $18, $9, 0x7FF
    r18 = r9 & 0X7FF;
    // addiu       $8, $7, -0x29
    r8 = RSP_ADD32(r7, -0X29);
    // bgezal      $8, L_185C
    if (RSP_SIGNED(r8) >= 0) {
        r31 = 0x1434;
    // addiu       $7, $zero, 0x28
        r7 = RSP_ADD32(0, 0X28);
        goto L_185C;
    }
    // addiu       $7, $zero, 0x28
    r7 = RSP_ADD32(0, 0X28);
L_1434:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1434;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1434 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $25, $zero, 0x1438
    r25 = RSP_ADD32(0, 0X1438);
L_1438:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1438;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1438 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // bne         $7, $zero, L_1898
    if (r7 != 0) {
        // addiu       $7, $7, -0x1
        r7 = RSP_ADD32(r7, -0X1);
        goto L_1898;
    }
    // addiu       $7, $7, -0x1
    r7 = RSP_ADD32(r7, -0X1);
    // addiu       $28, $zero, -0x478
    r28 = RSP_ADD32(0, -0X478);
    // lbu         $9, 0xFFD($zero)
    r9 = RSP_MEM_BU(0XFFD, 0);
    // sll         $10, $9, 2
    r10 = S32(U32(r9) << 2);
    // mtc2        $10, $v16[0]
    rsp.MTC2<0>(r10, rsp.vpu.r[16]);
    // sll         $10, $9, 5
    r10 = S32(U32(r9) << 5);
    // mtc2        $10, $v16[2]
    rsp.MTC2<2>(r10, rsp.vpu.r[16]);
    // sll         $10, $9, 3
    r10 = S32(U32(r9) << 3);
    // mtc2        $10, $v16[4]
    rsp.MTC2<4>(r10, rsp.vpu.r[16]);
    // addiu       $20, $zero, -0x1
    r20 = RSP_ADD32(0, -0X1);
L_1464:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1464;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1464 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // beq         $2, $3, L_161C
    if (r2 == r3) {
        // addiu       $6, $zero, 0x9F
        r6 = RSP_ADD32(0, 0X9F);
        goto L_161C;
    }
    // addiu       $6, $zero, 0x9F
    r6 = RSP_ADD32(0, 0X9F);
    // addiu       $4, $zero, 0x0
    r4 = RSP_ADD32(0, 0X0);
    // jal         0x1644
    r31 = 0x1478;
    // or          $5, $29, $zero
    r5 = r29 | 0;
    goto L_1644;
    // or          $5, $29, $zero
    r5 = r29 | 0;
L_1478:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1478;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1478 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $29, $29, 0xA0
    r29 = RSP_ADD32(r29, 0XA0);
    // lb          $8, 0xE78($28)
    r8 = RSP_MEM_B(0XE78, r28);
    // bltz        $8, L_14C4
    if (RSP_SIGNED(r8) < 0) {
        // addiu       $4, $zero, 0x100
        r4 = RSP_ADD32(0, 0X100);
        goto L_14C4;
    }
    // addiu       $4, $zero, 0x100
    r4 = RSP_ADD32(0, 0X100);
    // lui         $9, 0x0
    r9 = S32(U32(0X0) << 16);
    // beq         $20, $9, L_1564
    if (r20 == r9) {
        // or          $20, $9, $zero
        r20 = r9 | 0;
        goto L_1564;
    }
    // or          $20, $9, $zero
    r20 = r9 | 0;
    // addiu       $10, $4, 0xF0
    r10 = RSP_ADD32(r4, 0XF0);
    // lw          $5, 0xFF0($zero)
    r5 = RSP_MEM_W_LOAD(0XFF0, 0);
    // addiu       $6, $zero, 0xF
    r6 = RSP_ADD32(0, 0XF);
    // addiu       $25, $zero, 0x14AC
    r25 = RSP_ADD32(0, 0X14AC);
    // j           L_1980
    // addiu       $5, $5, 0x5A0
    r5 = RSP_ADD32(r5, 0X5A0);
    goto L_1980;
    // addiu       $5, $5, 0x5A0
    r5 = RSP_ADD32(r5, 0X5A0);
L_14AC:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x14AC;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x14AC after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lsv         $v0[0], 0x0($4)
    rsp.LSV<0>(rsp.vpu.r[0], r4, 0X0);
    // vaddc       $v0, $v31, $v0[0]
    rsp.VADDC<8>(rsp.vpu.r[0], rsp.vpu.r[31], rsp.vpu.r[0]);
L_14B4:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x14B4;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x14B4 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // sqv         $v0[0], 0x0($4)
    rsp.SQV<0>(rsp.vpu.r[0], r4, 0X0);
    // bne         $10, $4, L_14B4
    if (r10 != r4) {
        // addiu       $4, $4, 0x10
        r4 = RSP_ADD32(r4, 0X10);
        goto L_14B4;
    }
    // addiu       $4, $4, 0x10
    r4 = RSP_ADD32(r4, 0X10);
    // j           L_1564
    // lhu         $9, 0xE7C($28)
    r9 = RSP_MEM_HU_LOAD(0XE7C, r28);
    goto L_1564;
L_14C4:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x14C4;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x14C4 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lhu         $9, 0xE7C($28)
    r9 = RSP_MEM_HU_LOAD(0XE7C, r28);
    // xor         $9, $9, $20
    r9 = r9 ^ r20;
    // andi        $8, $9, 0xFF
    r8 = r9 & 0XFF;
    // beq         $8, $zero, L_1518
    if (r8 == 0) {
        // addiu       $6, $zero, 0x3F
        r6 = RSP_ADD32(0, 0X3F);
        goto L_1518;
    }
    // addiu       $6, $zero, 0x3F
    r6 = RSP_ADD32(0, 0X3F);
    // lw          $5, 0xFF0($zero)
    r5 = RSP_MEM_W_LOAD(0XFF0, 0);
    // xor         $20, $20, $8
    r20 = r20 ^ r8;
    // andi        $8, $20, 0xFF
    r8 = r20 & 0XFF;
    // sll         $8, $8, 6
    r8 = S32(U32(r8) << 6);
    // addiu       $8, $8, 0x560
    r8 = RSP_ADD32(r8, 0X560);
    // addu        $5, $5, $8
    r5 = RSP_ADD32(r5, r8);
    // j           L_1980
    // addiu       $25, $zero, 0x14F8
    r25 = RSP_ADD32(0, 0X14F8);
    goto L_1980;
    // addiu       $25, $zero, 0x14F8
    r25 = RSP_ADD32(0, 0X14F8);
L_14F8:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x14F8;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x14F8 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lqv         $v0[0], 0x0($4)
    rsp.LQV<0>(rsp.vpu.r[0], r4, 0X0);
    // lqv         $v1[0], 0x10($4)
    rsp.LQV<0>(rsp.vpu.r[1], r4, 0X1);
    // lqv         $v2[0], 0x20($4)
    rsp.LQV<0>(rsp.vpu.r[2], r4, 0X2);
    // lqv         $v3[0], 0x30($4)
    rsp.LQV<0>(rsp.vpu.r[3], r4, 0X3);
    // sqv         $v0[0], 0x80($4)
    rsp.SQV<0>(rsp.vpu.r[0], r4, 0X8);
    // sqv         $v1[0], 0x90($4)
    rsp.SQV<0>(rsp.vpu.r[1], r4, 0X9);
    // sqv         $v2[0], 0xA0($4)
    rsp.SQV<0>(rsp.vpu.r[2], r4, 0XA);
    // sqv         $v3[0], 0xB0($4)
    rsp.SQV<0>(rsp.vpu.r[3], r4, 0XB);
L_1518:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1518;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1518 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // andi        $8, $9, 0xFF00
    r8 = r9 & 0XFF00;
    // beq         $8, $zero, L_1564
    if (r8 == 0) {
        // addiu       $4, $zero, 0x140
        r4 = RSP_ADD32(0, 0X140);
        goto L_1564;
    }
    // addiu       $4, $zero, 0x140
    r4 = RSP_ADD32(0, 0X140);
    // lw          $5, 0xFF0($zero)
    r5 = RSP_MEM_W_LOAD(0XFF0, 0);
    // xor         $20, $20, $8
    r20 = r20 ^ r8;
    // andi        $8, $20, 0xFF00
    r8 = r20 & 0XFF00;
    // srl         $8, $8, 2
    r8 = S32(U32(r8) >> 2);
    // addiu       $8, $8, 0x2960
    r8 = RSP_ADD32(r8, 0X2960);
    // addu        $5, $5, $8
    r5 = RSP_ADD32(r5, r8);
    // j           L_1980
    // addiu       $25, $zero, 0x1544
    r25 = RSP_ADD32(0, 0X1544);
    goto L_1980;
    // addiu       $25, $zero, 0x1544
    r25 = RSP_ADD32(0, 0X1544);
L_1544:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1544;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1544 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lqv         $v0[0], 0x0($4)
    rsp.LQV<0>(rsp.vpu.r[0], r4, 0X0);
    // lqv         $v1[0], 0x10($4)
    rsp.LQV<0>(rsp.vpu.r[1], r4, 0X1);
    // lqv         $v2[0], 0x20($4)
    rsp.LQV<0>(rsp.vpu.r[2], r4, 0X2);
    // lqv         $v3[0], 0x30($4)
    rsp.LQV<0>(rsp.vpu.r[3], r4, 0X3);
    // sqv         $v0[0], 0x80($4)
    rsp.SQV<0>(rsp.vpu.r[0], r4, 0X8);
    // sqv         $v1[0], 0x90($4)
    rsp.SQV<0>(rsp.vpu.r[1], r4, 0X9);
    // sqv         $v2[0], 0xA0($4)
    rsp.SQV<0>(rsp.vpu.r[2], r4, 0XA);
    // sqv         $v3[0], 0xB0($4)
    rsp.SQV<0>(rsp.vpu.r[3], r4, 0XB);
L_1564:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1564;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1564 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $13, $18, 0x140
    r13 = RSP_ADD32(r18, 0X140);
    // addiu       $25, $zero, 0x0
    r25 = RSP_ADD32(0, 0X0);
    // or          $24, $18, $zero
    r24 = r18 | 0;
    // addiu       $12, $zero, 0x1
    r12 = RSP_ADD32(0, 0X1);
L_1574:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1574;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1574 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lbu         $8, 0x0($25)
    r8 = RSP_MEM_BU(0X0, r25);
    // lbu         $9, 0x1($25)
    r9 = RSP_MEM_BU(0X1, r25);
    // lbu         $10, 0x2($25)
    r10 = RSP_MEM_BU(0X2, r25);
    // lbu         $11, 0x3($25)
    r11 = RSP_MEM_BU(0X3, r25);
    // lhu         $8, 0x100($8)
    r8 = RSP_MEM_HU_LOAD(0X100, r8);
    // lhu         $9, 0x100($9)
    r9 = RSP_MEM_HU_LOAD(0X100, r9);
    // lhu         $10, 0x100($10)
    r10 = RSP_MEM_HU_LOAD(0X100, r10);
    // lhu         $11, 0x100($11)
    r11 = RSP_MEM_HU_LOAD(0X100, r11);
    // sh          $8, 0x200($24)
    RSP_MEM_H_STORE(0X200, r24, r8);
    // addiu       $25, $25, 0x4
    r25 = RSP_ADD32(r25, 0X4);
    // sh          $9, 0x202($24)
    RSP_MEM_H_STORE(0X202, r24, r9);
    // addiu       $24, $24, 0x8
    r24 = RSP_ADD32(r24, 0X8);
    // sh          $10, 0x1FC($24)
    RSP_MEM_H_STORE(0X1FC, r24, r10);
    // sh          $11, 0x1FE($24)
    RSP_MEM_H_STORE(0X1FE, r24, r11);
    // bgtz        $12, L_1574
    if (RSP_SIGNED(r12) > 0) {
        // addiu       $12, $12, -0x1
        r12 = RSP_ADD32(r12, -0X1);
        goto L_1574;
    }
    // addiu       $12, $12, -0x1
    r12 = RSP_ADD32(r12, -0X1);
    // bne         $24, $13, L_1574
    if (r24 != r13) {
        // lui         $6, 0xA00
        r6 = S32(U32(0XA00) << 16);
        goto L_1574;
    }
    // lui         $6, 0xA00
    r6 = S32(U32(0XA00) << 16);
    // ori         $6, $6, 0x102F
    r6 = r6 | 0X102F;
    // addiu       $16, $zero, 0x1
    r16 = RSP_ADD32(0, 0X1);
    // j           L_18A0
    // addiu       $25, $zero, 0x15CC
    r25 = RSP_ADD32(0, 0X15CC);
    goto L_18A0;
    // addiu       $25, $zero, 0x15CC
    r25 = RSP_ADD32(0, 0X15CC);
L_15CC:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x15CC;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x15CC after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // bne         $28, $zero, L_1464
    if (r28 != 0) {
        // addiu       $28, $28, 0x8
        r28 = RSP_ADD32(r28, 0X8);
        goto L_1464;
    }
    // addiu       $28, $28, 0x8
    r28 = RSP_ADD32(r28, 0X8);
    // jal         0x1658
    r31 = 0x15DC;
    // addiu       $7, $zero, 0x28
    r7 = RSP_ADD32(0, 0X28);
    goto L_1658;
    // addiu       $7, $zero, 0x28
    r7 = RSP_ADD32(0, 0X28);
L_15DC:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x15DC;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x15DC after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lbu         $14, 0xFDE($zero)
    r14 = RSP_MEM_BU(0XFDE, 0);
    // lbu         $24, 0xFDF($zero)
    r24 = RSP_MEM_BU(0XFDF, 0);
    // lw          $12, 0xFD8($zero)
    r12 = RSP_MEM_W_LOAD(0XFD8, 0);
    // addiu       $25, $zero, 0x15EC
    r25 = RSP_ADD32(0, 0X15EC);
L_15EC:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x15EC;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x15EC after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // beq         $2, $3, L_161C
    if (r2 == r3) {
        // addiu       $16, $zero, 0x0
        r16 = RSP_ADD32(0, 0X0);
        goto L_161C;
    }
    // addiu       $16, $zero, 0x0
    r16 = RSP_ADD32(0, 0X0);
    // bgtz        $24, L_1608
    if (RSP_SIGNED(r24) > 0) {
        // addiu       $24, $24, -0x1
        r24 = RSP_ADD32(r24, -0X1);
        goto L_1608;
    }
    // addiu       $24, $24, -0x1
    r24 = RSP_ADD32(r24, -0X1);
    // blez        $14, L_1608
    if (RSP_SIGNED(r14) <= 0) {
        // addiu       $14, $14, -0x1
        r14 = RSP_ADD32(r14, -0X1);
        goto L_1608;
    }
    // addiu       $14, $14, -0x1
    r14 = RSP_ADD32(r14, -0X1);
    // addiu       $16, $zero, 0x2
    r16 = RSP_ADD32(0, 0X2);
L_1608:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1608;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1608 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // bgtz        $7, L_189C
    if (RSP_SIGNED(r7) > 0) {
        // addiu       $7, $7, -0x1
        r7 = RSP_ADD32(r7, -0X1);
        goto L_189C;
    }
    // addiu       $7, $7, -0x1
    r7 = RSP_ADD32(r7, -0X1);
    // addiu       $8, $zero, 0x0
    r8 = RSP_ADD32(0, 0X0);
    // j           L_1658
    // addiu       $ra, $zero, 0x1860
    r31 = RSP_ADD32(0, 0X1860);
    goto L_1658;
    // addiu       $ra, $zero, 0x1860
    r31 = RSP_ADD32(0, 0X1860);
L_161C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x161C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x161C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // mfc0        $1, DPC_CLOCK
    r1 = 0;
    // subu        $1, $1, $26
    r1 = RSP_SUB32(r1, r26);
    // sw          $1, 0xFFC($zero)
    RSP_MEM_W_STORE(0XFFC, 0, r1);
    // jal         0x1658
    r31 = 0x1630;
    // mfc0        $8, SP_STATUS
    r8 = 0;
    goto L_1658;
L_162C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x162C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x162C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // mfc0        $8, SP_STATUS
    r8 = 0;
L_1630:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1630;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1630 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // andi        $8, $8, 0x400
    r8 = r8 & 0X400;
    // bne         $8, $zero, L_162C
    if (r8 != 0) {
        // ori         $8, $zero, 0x4000
        r8 = 0 | 0X4000;
        goto L_162C;
    }
    // ori         $8, $zero, 0x4000
    r8 = 0 | 0X4000;
    // mtc0        $8, SP_STATUS
    WRITE_SP_STATUS(r8);
    // break       0
    return RspExitReason::Broke;
L_1644:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1644;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1644 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // ori         $1, $zero, 0xFFFF
    r1 = 0 | 0XFFFF;
L_1648:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1648;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1648 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // mtc0        $4, SP_MEM_ADDR
    SET_DMA_MEM(r4);
    // mtc0        $5, SP_DRAM_ADDR
    SET_DMA_DRAM(r5);
    // mtc0        $6, SP_RD_LEN
    DO_DMA_READ(r6);
L_1654:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1654;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1654 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // beq         $1, $zero, L_1668
    if (r1 == 0) {
        // mfc0        $1, SP_DMA_BUSY
        r1 = 0;
        goto L_1668;
    }
L_1658:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1658;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1658 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // mfc0        $1, SP_DMA_BUSY
    r1 = 0;
L_165C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x165C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x165C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // bne         $1, $zero, L_165C
    if (r1 != 0) {
        // mfc0        $1, SP_DMA_BUSY
        r1 = 0;
        goto L_165C;
    }
    // mfc0        $1, SP_DMA_BUSY
    r1 = 0;
    // jr          $ra
    jump_target = r31;
    debug_file = __FILE__; debug_line = __LINE__;
    // mfc0        $1, SP_DMA_FULL
    r1 = 0;
    goto do_indirect_jump;
L_1668:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1668;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1668 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // mfc0        $1, SP_DMA_FULL
    r1 = 0;
L_166C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x166C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x166C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // bne         $1, $zero, L_166C
    if (r1 != 0) {
        // mfc0        $1, SP_DMA_FULL
        r1 = 0;
        goto L_166C;
    }
    // mfc0        $1, SP_DMA_FULL
    r1 = 0;
    // jr          $ra
    jump_target = r31;
    debug_file = __FILE__; debug_line = __LINE__;
    // nop

    goto do_indirect_jump;
    // nop

L_167C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x167C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x167C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // ori         $1, $zero, 0xFFFF
    r1 = 0 | 0XFFFF;
L_1680:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1680;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1680 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // mtc0        $4, SP_MEM_ADDR
    SET_DMA_MEM(r4);
    // mtc0        $5, SP_DRAM_ADDR
    SET_DMA_DRAM(r5);
    // j           L_1654
    // mtc0        $6, SP_WR_LEN
    DO_DMA_WRITE(r6);
    goto L_1654;
    // mtc0        $6, SP_WR_LEN
    DO_DMA_WRITE(r6);
L_1690:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1690;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1690 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lw          $5, 0xFE0($zero)
    r5 = RSP_MEM_W_LOAD(0XFE0, 0);
    // sll         $8, $28, 2
    r8 = S32(U32(r28) << 2);
    // addu        $8, $8, $28
    r8 = RSP_ADD32(r8, r28);
    // sll         $8, $8, 2
    r8 = S32(U32(r8) << 2);
    // addiu       $4, $zero, 0x8
    r4 = RSP_ADD32(0, 0X8);
    // addiu       $6, $zero, 0x9F
    r6 = RSP_ADD32(0, 0X9F);
    // jal         0x1644
    r31 = 0x16B0;
    // addu        $5, $5, $8
    r5 = RSP_ADD32(r5, r8);
    goto L_1644;
    // addu        $5, $5, $8
    r5 = RSP_ADD32(r5, r8);
L_16B0:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x16B0;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x16B0 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $24, $18, 0x1
    r24 = RSP_ADD32(r18, 0X1);
L_16B4:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x16B4;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x16B4 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lhu         $20, 0xF80($21)
    r20 = RSP_MEM_HU_LOAD(0XF80, r21);
    // subu        $22, $18, $19
    r22 = RSP_SUB32(r18, r19);
    // bgez        $22, L_1748
    if (RSP_SIGNED(r22) >= 0) {
        // andi        $9, $20, 0x7FE
        r9 = r20 & 0X7FE;
        goto L_1748;
    }
    // andi        $9, $20, 0x7FE
    r9 = r20 & 0X7FE;
    // xor         $9, $9, $15
    r9 = r9 ^ r15;
    // addiu       $9, $9, 0x200
    r9 = RSP_ADD32(r9, 0X200);
    // lsv         $v2[0], 0x0($9)
    rsp.LSV<0>(rsp.vpu.r[2], r9, 0X0);
    // andi        $8, $20, 0xB800
    r8 = r20 & 0XB800;
    // mtc2        $8, $v1[0]
    rsp.MTC2<0>(r8, rsp.vpu.r[1]);
    // mtc2        $22, $v5[0]
    rsp.MTC2<0>(r22, rsp.vpu.r[5]);
    // andi        $8, $20, 0x4000
    r8 = r20 & 0X4000;
    // beq         $8, $zero, L_1704
    if (r8 == 0) {
        // lpv         $v6[0], 0x0($24)
        rsp.LPV<0>(rsp.vpu.r[6], r24, 0X0);
        goto L_1704;
    }
    // lpv         $v6[0], 0x0($24)
    rsp.LPV<0>(rsp.vpu.r[6], r24, 0X0);
    // vand        $v3, $v26, $v2[0]
    rsp.VAND<8>(rsp.vpu.r[3], rsp.vpu.r[26], rsp.vpu.r[2]);
    // vne         $v30, $v3, $v26
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[3], rsp.vpu.r[26]);
    // vmrg        $v4, $v31, $v22[7]
    rsp.VMRG<15>(rsp.vpu.r[4], rsp.vpu.r[31], rsp.vpu.r[22]);
    // vne         $v30, $v3, $v25
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[3], rsp.vpu.r[25]);
    // vmrg        $v4, $v4, $v27[6]
    rsp.VMRG<14>(rsp.vpu.r[4], rsp.vpu.r[4], rsp.vpu.r[27]);
    // j           L_171C
    // vne         $v30, $v3, $v24
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[3], rsp.vpu.r[24]);
    goto L_171C;
    // vne         $v30, $v3, $v24
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[3], rsp.vpu.r[24]);
L_1704:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1704;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1704 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // vand        $v3, $v29, $v2[0]
    rsp.VAND<8>(rsp.vpu.r[3], rsp.vpu.r[29], rsp.vpu.r[2]);
    // vne         $v30, $v3, $v29
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[3], rsp.vpu.r[29]);
    // vmrg        $v4, $v31, $v22[7]
    rsp.VMRG<15>(rsp.vpu.r[4], rsp.vpu.r[31], rsp.vpu.r[22]);
    // vne         $v30, $v3, $v27
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[3], rsp.vpu.r[27]);
    // vmrg        $v4, $v4, $v27[6]
    rsp.VMRG<14>(rsp.vpu.r[4], rsp.vpu.r[4], rsp.vpu.r[27]);
    // vne         $v30, $v3, $v28
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[3], rsp.vpu.r[28]);
L_171C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x171C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x171C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // vmrg        $v4, $v4, $v27[5]
    rsp.VMRG<13>(rsp.vpu.r[4], rsp.vpu.r[4], rsp.vpu.r[27]);
    // vor         $v4, $v4, $v1[0]
    rsp.VOR<8>(rsp.vpu.r[4], rsp.vpu.r[4], rsp.vpu.r[1]);
    // addiu       $21, $21, 0x2
    r21 = RSP_ADD32(r21, 0X2);
    // andi        $21, $21, 0x3E
    r21 = r21 & 0X3E;
    // addiu       $18, $18, 0x8
    r18 = RSP_ADD32(r18, 0X8);
    // vge         $v30, $v21, $v5[0]
    rsp.VGE<8>(rsp.vpu.r[30], rsp.vpu.r[21], rsp.vpu.r[5]);
    // subu        $22, $18, $19
    r22 = RSP_SUB32(r18, r19);
    // vmrg        $v4, $v4, $v6
    rsp.VMRG<0>(rsp.vpu.r[4], rsp.vpu.r[4], rsp.vpu.r[6]);
    // addiu       $24, $24, 0x8
    r24 = RSP_ADD32(r24, 0X8);
    // bltz        $22, L_16B4
    if (RSP_SIGNED(r22) < 0) {
        // spv         $v4[0], 0x3F8($24)
        rsp.SPV<0>(rsp.vpu.r[4], r24, -0X1);
        goto L_16B4;
    }
    // spv         $v4[0], 0x3F8($24)
    rsp.SPV<0>(rsp.vpu.r[4], r24, -0X1);
L_1748:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1748;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1748 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // j           L_1680
    // or          $ra, $25, $zero
    r31 = r25 | 0;
    goto L_1680;
    // or          $ra, $25, $zero
    r31 = r25 | 0;
L_1750:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1750;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1750 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lw          $5, 0xFE0($zero)
    r5 = RSP_MEM_W_LOAD(0XFE0, 0);
    // sll         $8, $16, 2
    r8 = S32(U32(r16) << 2);
    // addu        $8, $8, $16
    r8 = RSP_ADD32(r8, r16);
    // sll         $8, $8, 5
    r8 = S32(U32(r8) << 5);
    // andi        $9, $18, 0xF8
    r9 = r18 & 0XF8;
    // addiu       $9, $9, -0x8
    r9 = RSP_ADD32(r9, -0X8);
    // addu        $8, $8, $9
    r8 = RSP_ADD32(r8, r9);
    // addu        $5, $5, $8
    r5 = RSP_ADD32(r5, r8);
    // sll         $8, $7, 3
    r8 = S32(U32(r7) << 3);
    // addu        $28, $28, $8
    r28 = RSP_ADD32(r28, r8);
    // sll         $24, $8, 1
    r24 = S32(U32(r8) << 1);
    // lui         $6, 0x900
    r6 = S32(U32(0X900) << 16);
    // sll         $8, $8, 9
    r8 = S32(U32(r8) << 9);
    // or          $6, $6, $8
    r6 = r6 | r8;
    // ori         $6, $6, 0xF
    r6 = r6 | 0XF;
    // jal         0x1644
    r31 = 0x1794;
    // addiu       $4, $zero, 0x0
    r4 = RSP_ADD32(0, 0X0);
    goto L_1644;
    // addiu       $4, $zero, 0x0
    r4 = RSP_ADD32(0, 0X0);
L_1794:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1794;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1794 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lbu         $10, 0xA00($28)
    r10 = RSP_MEM_BU(0XA00, r28);
    // andi        $8, $18, 0x7
    r8 = r18 & 0X7;
    // addu        $8, $8, $24
    r8 = RSP_ADD32(r8, r24);
    // addiu       $28, $28, -0x8
    r28 = RSP_ADD32(r28, -0X8);
    // andi        $10, $10, 0x2
    r10 = r10 & 0X2;
    // beq         $10, $zero, L_1848
    if (r10 == 0) {
        // addiu       $7, $7, -0x1
        r7 = RSP_ADD32(r7, -0X1);
        goto L_1848;
    }
    // addiu       $7, $7, -0x1
    r7 = RSP_ADD32(r7, -0X1);
    // lhu         $9, 0xF80($29)
    r9 = RSP_MEM_HU_LOAD(0XF80, r29);
    // sll         $10, $3, 8
    r10 = S32(U32(r3) << 8);
    // lpv         $v0[0], 0x0($8)
    rsp.LPV<0>(rsp.vpu.r[0], r8, 0X0);
    // mtc2        $10, $v3[0]
    rsp.MTC2<0>(r10, rsp.vpu.r[3]);
    // andi        $11, $3, 0x20
    r11 = r3 & 0X20;
    // mtc2        $9, $v2[0]
    rsp.MTC2<0>(r9, rsp.vpu.r[2]);
    // vaddc       $v17, $v31, $v27[6]
    rsp.VADDC<14>(rsp.vpu.r[17], rsp.vpu.r[31], rsp.vpu.r[27]);
    // andi        $10, $3, 0x7
    r10 = r3 & 0X7;
    // sll         $10, $10, 11
    r10 = S32(U32(r10) << 11);
    // mtc2        $10, $v4[0]
    rsp.MTC2<0>(r10, rsp.vpu.r[4]);
    // beq         $11, $zero, L_17F4
    if (r11 == 0) {
        // mtc2        $18, $v9[0]
        rsp.MTC2<0>(r18, rsp.vpu.r[9]);
        goto L_17F4;
    }
    // mtc2        $18, $v9[0]
    rsp.MTC2<0>(r18, rsp.vpu.r[9]);
    // vand        $v5, $v26, $v2[0]
    rsp.VAND<8>(rsp.vpu.r[5], rsp.vpu.r[26], rsp.vpu.r[2]);
    // vne         $v30, $v5, $v26
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[5], rsp.vpu.r[26]);
    // vmrg        $v6, $v17, $v22[7]
    rsp.VMRG<15>(rsp.vpu.r[6], rsp.vpu.r[17], rsp.vpu.r[22]);
    // j           L_1804
    // vne         $v30, $v5, $v24
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[5], rsp.vpu.r[24]);
    goto L_1804;
    // vne         $v30, $v5, $v24
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[5], rsp.vpu.r[24]);
L_17F4:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x17F4;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x17F4 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // vand        $v5, $v29, $v2[0]
    rsp.VAND<8>(rsp.vpu.r[5], rsp.vpu.r[29], rsp.vpu.r[2]);
    // vne         $v30, $v5, $v29
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[5], rsp.vpu.r[29]);
    // vmrg        $v6, $v17, $v22[7]
    rsp.VMRG<15>(rsp.vpu.r[6], rsp.vpu.r[17], rsp.vpu.r[22]);
    // vne         $v30, $v5, $v28
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[5], rsp.vpu.r[28]);
L_1804:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1804;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1804 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // vmrg        $v6, $v6, $v27[5]
    rsp.VMRG<13>(rsp.vpu.r[6], rsp.vpu.r[6], rsp.vpu.r[27]);
    // vor         $v8, $v0, $v3[0]
    rsp.VOR<8>(rsp.vpu.r[8], rsp.vpu.r[0], rsp.vpu.r[3]);
    // veq         $v30, $v5, $v31[0]
    rsp.VEQ<8>(rsp.vpu.r[30], rsp.vpu.r[5], rsp.vpu.r[31]);
    // vor         $v6, $v6, $v22[6]
    rsp.VOR<14>(rsp.vpu.r[6], rsp.vpu.r[6], rsp.vpu.r[22]);
    // vand        $v8, $v8, $v27[0]
    rsp.VAND<8>(rsp.vpu.r[8], rsp.vpu.r[8], rsp.vpu.r[27]);
    // vor         $v6, $v6, $v4[0]
    rsp.VOR<8>(rsp.vpu.r[6], rsp.vpu.r[6], rsp.vpu.r[4]);
    // vand        $v7, $v0, $v15[3]
    rsp.VAND<11>(rsp.vpu.r[7], rsp.vpu.r[0], rsp.vpu.r[15]);
    // vmrg        $v6, $v0, $v6
    rsp.VMRG<0>(rsp.vpu.r[6], rsp.vpu.r[0], rsp.vpu.r[6]);
    // veq         $v30, $v8, $v31[0]
    rsp.VEQ<8>(rsp.vpu.r[30], rsp.vpu.r[8], rsp.vpu.r[31]);
    // vmrg        $v1, $v6, $v0
    rsp.VMRG<0>(rsp.vpu.r[1], rsp.vpu.r[6], rsp.vpu.r[0]);
    // veq         $v30, $v7, $v31[0]
    rsp.VEQ<8>(rsp.vpu.r[30], rsp.vpu.r[7], rsp.vpu.r[31]);
    // vmrg        $v1, $v6, $v1
    rsp.VMRG<0>(rsp.vpu.r[1], rsp.vpu.r[6], rsp.vpu.r[1]);
    // vlt         $v30, $v19, $v9[0]
    rsp.VLT<8>(rsp.vpu.r[30], rsp.vpu.r[19], rsp.vpu.r[9]);
    // vmrg        $v1, $v1, $v0
    rsp.VMRG<0>(rsp.vpu.r[1], rsp.vpu.r[1], rsp.vpu.r[0]);
    // vge         $v30, $v18, $v9[0]
    rsp.VGE<8>(rsp.vpu.r[30], rsp.vpu.r[18], rsp.vpu.r[9]);
    // vmrg        $v1, $v1, $v0
    rsp.VMRG<0>(rsp.vpu.r[1], rsp.vpu.r[1], rsp.vpu.r[0]);
    // spv         $v1[0], 0x0($8)
    rsp.SPV<0>(rsp.vpu.r[1], r8, 0X0);
L_1848:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1848;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1848 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addu        $29, $29, $20
    r29 = RSP_ADD32(r29, r20);
    // bgez        $7, L_1794
    if (RSP_SIGNED(r7) >= 0) {
        // addiu       $24, $24, -0x10
        r24 = RSP_ADD32(r24, -0X10);
        goto L_1794;
    }
    // addiu       $24, $24, -0x10
    r24 = RSP_ADD32(r24, -0X10);
    // jal         0x167C
    r31 = 0x185C;
    // or          $ra, $25, $zero
    r31 = r25 | 0;
    goto L_167C;
    // or          $ra, $25, $zero
    r31 = r25 | 0;
L_185C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x185C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x185C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // or          $25, $ra, $zero
    r25 = r31 | 0;
L_1860:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1860;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1860 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // or          $4, $19, $zero
    r4 = r19 | 0;
    // addiu       $6, $19, -0x1
    r6 = RSP_ADD32(r19, -0X1);
    // lw          $5, 0xFE8($zero)
    r5 = RSP_MEM_W_LOAD(0XFE8, 0);
L_186C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x186C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x186C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $4, $4, -0x10
    r4 = RSP_ADD32(r4, -0X10);
    // bne         $4, $zero, L_186C
    if (r4 != 0) {
        // sqv         $v31[0], 0x200($4)
        rsp.SQV<0>(rsp.vpu.r[31], r4, 0X20);
        goto L_186C;
    }
    // sqv         $v31[0], 0x200($4)
    rsp.SQV<0>(rsp.vpu.r[31], r4, 0X20);
    // addiu       $4, $zero, 0x200
    r4 = RSP_ADD32(0, 0X200);
    // addu        $5, $5, $2
    r5 = RSP_ADD32(r5, r2);
L_1880:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1880;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1880 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // jal         0x1680
    r31 = 0x1888;
    // addu        $2, $2, $19
    r2 = RSP_ADD32(r2, r19);
    goto L_1680;
    // addu        $2, $2, $19
    r2 = RSP_ADD32(r2, r19);
L_1888:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1888;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1888 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addu        $5, $5, $19
    r5 = RSP_ADD32(r5, r19);
    // bne         $8, $zero, L_1880
    if (r8 != 0) {
        // addiu       $8, $8, -0x1
        r8 = RSP_ADD32(r8, -0X1);
        goto L_1880;
    }
    // addiu       $8, $8, -0x1
    r8 = RSP_ADD32(r8, -0X1);
    // jr          $25
    jump_target = r25;
    debug_file = __FILE__; debug_line = __LINE__;
    // addiu       $16, $zero, 0x0
    r16 = RSP_ADD32(0, 0X0);
    goto do_indirect_jump;
L_1898:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1898;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1898 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $16, $zero, 0x0
    r16 = RSP_ADD32(0, 0X0);
L_189C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x189C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x189C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $6, $zero, 0xFF
    r6 = RSP_ADD32(0, 0XFF);
L_18A0:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x18A0;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x18A0 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lw          $5, 0xFF8($zero)
    r5 = RSP_MEM_W_LOAD(0XFF8, 0);
    // jal         0x1644
    r31 = 0x18AC;
    // addiu       $4, $zero, 0x700
    r4 = RSP_ADD32(0, 0X700);
    goto L_1644;
    // addiu       $4, $zero, 0x700
    r4 = RSP_ADD32(0, 0X700);
L_18AC:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x18AC;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x18AC after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // andi        $6, $6, 0xFF
    r6 = r6 & 0XFF;
    // addiu       $17, $18, -0x60
    r17 = RSP_ADD32(r18, -0X60);
L_18B4:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x18B4;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x18B4 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lbu         $8, 0x0($4)
    r8 = RSP_MEM_BU(0X0, r4);
    // lbu         $9, 0x1($4)
    r9 = RSP_MEM_BU(0X1, r4);
    // lbu         $10, 0x2($4)
    r10 = RSP_MEM_BU(0X2, r4);
    // lbu         $11, 0x3($4)
    r11 = RSP_MEM_BU(0X3, r4);
    // sll         $8, $8, 1
    r8 = S32(U32(r8) << 1);
    // lhu         $8, 0x800($8)
    r8 = RSP_MEM_HU_LOAD(0X800, r8);
    // sll         $9, $9, 1
    r9 = S32(U32(r9) << 1);
    // lhu         $9, 0x800($9)
    r9 = RSP_MEM_HU_LOAD(0X800, r9);
    // sll         $10, $10, 1
    r10 = S32(U32(r10) << 1);
    // lhu         $10, 0x800($10)
    r10 = RSP_MEM_HU_LOAD(0X800, r10);
    // sll         $11, $11, 1
    r11 = S32(U32(r11) << 1);
    // lhu         $11, 0x800($11)
    r11 = RSP_MEM_HU_LOAD(0X800, r11);
    // addiu       $17, $17, 0x8
    r17 = RSP_ADD32(r17, 0X8);
    // sh          $8, 0x1F8($17)
    RSP_MEM_H_STORE(0X1F8, r17, r8);
    // addiu       $4, $4, 0x4
    r4 = RSP_ADD32(r4, 0X4);
    // sh          $9, 0x1FA($17)
    RSP_MEM_H_STORE(0X1FA, r17, r9);
    // addiu       $6, $6, -0x4
    r6 = RSP_ADD32(r6, -0X4);
    // sh          $10, 0x1FC($17)
    RSP_MEM_H_STORE(0X1FC, r17, r10);
    // bgez        $6, L_18B4
    if (RSP_SIGNED(r6) >= 0) {
        // sh          $11, 0x1FE($17)
        RSP_MEM_H_STORE(0X1FE, r17, r11);
        goto L_18B4;
    }
    // sh          $11, 0x1FE($17)
    RSP_MEM_H_STORE(0X1FE, r17, r11);
    // andi        $8, $16, 0x1
    r8 = r16 & 0X1;
    // beq         $8, $zero, L_191C
    if (r8 == 0) {
        // addiu       $16, $16, -0x1
        r16 = RSP_ADD32(r16, -0X1);
        goto L_191C;
    }
    // addiu       $16, $16, -0x1
    r16 = RSP_ADD32(r16, -0X1);
    // addiu       $17, $17, 0x140
    r17 = RSP_ADD32(r17, 0X140);
    // j           L_18B4
    // addiu       $6, $zero, 0x2F
    r6 = RSP_ADD32(0, 0X2F);
    goto L_18B4;
    // addiu       $6, $zero, 0x2F
    r6 = RSP_ADD32(0, 0X2F);
L_191C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x191C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x191C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $5, $5, 0x100
    r5 = RSP_ADD32(r5, 0X100);
    // blez        $16, L_1964
    if (RSP_SIGNED(r16) <= 0) {
        // sw          $5, 0xFF8($zero)
        RSP_MEM_W_STORE(0XFF8, 0, r5);
        goto L_1964;
    }
    // sw          $5, 0xFF8($zero)
    RSP_MEM_W_STORE(0XFF8, 0, r5);
    // or          $5, $12, $zero
    r5 = r12 | 0;
    // addiu       $6, $19, -0x1
    r6 = RSP_ADD32(r19, -0X1);
    // jal         0x1644
    r31 = 0x1938;
    // addiu       $4, $zero, 0x480
    r4 = RSP_ADD32(0, 0X480);
    goto L_1644;
    // addiu       $4, $zero, 0x480
    r4 = RSP_ADD32(0, 0X480);
L_1938:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1938;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1938 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addu        $12, $12, $19
    r12 = RSP_ADD32(r12, r19);
    // addu        $8, $4, $19
    r8 = RSP_ADD32(r4, r19);
    // addiu       $9, $zero, 0x200
    r9 = RSP_ADD32(0, 0X200);
L_1944:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1944;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1944 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lqv         $v0[0], 0x0($4)
    rsp.LQV<0>(rsp.vpu.r[0], r4, 0X0);
    // lqv         $v1[0], 0x0($9)
    rsp.LQV<0>(rsp.vpu.r[1], r9, 0X0);
    // addiu       $4, $4, 0x10
    r4 = RSP_ADD32(r4, 0X10);
    // addiu       $9, $9, 0x10
    r9 = RSP_ADD32(r9, 0X10);
    // vne         $v30, $v0, $v31
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[0], rsp.vpu.r[31]);
    // vmrg        $v0, $v0, $v1
    rsp.VMRG<0>(rsp.vpu.r[0], rsp.vpu.r[0], rsp.vpu.r[1]);
    // bne         $4, $8, L_1944
    if (r4 != r8) {
        // sqv         $v0[0], 0x7F0($9)
        rsp.SQV<0>(rsp.vpu.r[0], r9, -0X1);
        goto L_1944;
    }
    // sqv         $v0[0], 0x7F0($9)
    rsp.SQV<0>(rsp.vpu.r[0], r9, -0X1);
L_1964:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1964;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1964 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $4, $zero, 0x200
    r4 = RSP_ADD32(0, 0X200);
    // lw          $5, 0xFE8($zero)
    r5 = RSP_MEM_W_LOAD(0XFE8, 0);
    // addiu       $6, $19, -0x1
    r6 = RSP_ADD32(r19, -0X1);
    // or          $ra, $25, $zero
    r31 = r25 | 0;
    // addu        $5, $5, $2
    r5 = RSP_ADD32(r5, r2);
    // j           L_1680
    // addu        $2, $2, $19
    r2 = RSP_ADD32(r2, r19);
    goto L_1680;
    // addu        $2, $2, $19
    r2 = RSP_ADD32(r2, r19);
L_1980:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1980;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1980 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // jal         0x1644
    r31 = 0x1988;
    // addu        $8, $4, $6
    r8 = RSP_ADD32(r4, r6);
    goto L_1644;
    // addu        $8, $4, $6
    r8 = RSP_ADD32(r4, r6);
L_1988:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1988;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1988 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $8, $8, 0x1
    r8 = RSP_ADD32(r8, 0X1);
L_198C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x198C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x198C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lqv         $v0[0], 0x7F0($8)
    rsp.LQV<0>(rsp.vpu.r[0], r8, -0X1);
    // addiu       $8, $8, -0x10
    r8 = RSP_ADD32(r8, -0X10);
    // vand        $v4, $v0, $v22[0]
    rsp.VAND<8>(rsp.vpu.r[4], rsp.vpu.r[0], rsp.vpu.r[22]);
    // vand        $v3, $v0, $v22[1]
    rsp.VAND<9>(rsp.vpu.r[3], rsp.vpu.r[0], rsp.vpu.r[22]);
    // vmudn       $v1, $v0, $v27[1]
    rsp.VMUDN<9>(rsp.vpu.r[1], rsp.vpu.r[0], rsp.vpu.r[27]);
    // vand        $v5, $v0, $v22[2]
    rsp.VAND<10>(rsp.vpu.r[5], rsp.vpu.r[0], rsp.vpu.r[22]);
    // vmudl       $v2, $v4, $v27[1]
    rsp.VMUDL<9>(rsp.vpu.r[2], rsp.vpu.r[4], rsp.vpu.r[27]);
    // vmudl       $v3, $v3, $v16[0]
    rsp.VMUDL<8>(rsp.vpu.r[3], rsp.vpu.r[3], rsp.vpu.r[16]);
    // vor         $v4, $v1, $v2
    rsp.VOR<0>(rsp.vpu.r[4], rsp.vpu.r[1], rsp.vpu.r[2]);
    // vmudn       $v5, $v5, $v16[2]
    rsp.VMUDN<10>(rsp.vpu.r[5], rsp.vpu.r[5], rsp.vpu.r[16]);
    // vmudn       $v3, $v3, $v27[4]
    rsp.VMUDN<12>(rsp.vpu.r[3], rsp.vpu.r[3], rsp.vpu.r[27]);
    // vmudl       $v4, $v4, $v16[1]
    rsp.VMUDL<9>(rsp.vpu.r[4], rsp.vpu.r[4], rsp.vpu.r[16]);
    // vmudl       $v5, $v5, $v24[6]
    rsp.VMUDL<14>(rsp.vpu.r[5], rsp.vpu.r[5], rsp.vpu.r[24]);
    // vand        $v3, $v3, $v22[3]
    rsp.VAND<11>(rsp.vpu.r[3], rsp.vpu.r[3], rsp.vpu.r[22]);
    // vand        $v4, $v4, $v22[4]
    rsp.VAND<12>(rsp.vpu.r[4], rsp.vpu.r[4], rsp.vpu.r[22]);
    // vor         $v5, $v5, $v28[7]
    rsp.VOR<15>(rsp.vpu.r[5], rsp.vpu.r[5], rsp.vpu.r[28]);
    // vor         $v3, $v3, $v4
    rsp.VOR<0>(rsp.vpu.r[3], rsp.vpu.r[3], rsp.vpu.r[4]);
    // vor         $v3, $v3, $v5
    rsp.VOR<0>(rsp.vpu.r[3], rsp.vpu.r[3], rsp.vpu.r[5]);
    // bne         $8, $4, L_198C
    if (r8 != r4) {
        // sqv         $v3[0], 0x0($8)
        rsp.SQV<0>(rsp.vpu.r[3], r8, 0X0);
        goto L_198C;
    }
    // sqv         $v3[0], 0x0($8)
    rsp.SQV<0>(rsp.vpu.r[3], r8, 0X0);
    // jr          $25
    jump_target = r25;
    debug_file = __FILE__; debug_line = __LINE__;
    // nop

    goto do_indirect_jump;
    // nop

    return RspExitReason::ImemOverrun;
do_indirect_jump:
    switch ((jump_target | 0x1000) & 0X1FFF) { 
        case 0x1030: goto L_1030;
        case 0x1040: goto L_1040;
        case 0x1048: goto L_1048;
        case 0x106C: goto L_106C;
        case 0x107C: goto L_107C;
        case 0x10D0: goto L_10D0;
        case 0x10FC: goto L_10FC;
        case 0x1114: goto L_1114;
        case 0x113C: goto L_113C;
        case 0x1148: goto L_1148;
        case 0x115C: goto L_115C;
        case 0x1168: goto L_1168;
        case 0x1170: goto L_1170;
        case 0x118C: goto L_118C;
        case 0x11A0: goto L_11A0;
        case 0x11EC: goto L_11EC;
        case 0x120C: goto L_120C;
        case 0x1224: goto L_1224;
        case 0x1238: goto L_1238;
        case 0x1284: goto L_1284;
        case 0x12A0: goto L_12A0;
        case 0x12B8: goto L_12B8;
        case 0x12BC: goto L_12BC;
        case 0x12F8: goto L_12F8;
        case 0x132C: goto L_132C;
        case 0x1358: goto L_1358;
        case 0x136C: goto L_136C;
        case 0x137C: goto L_137C;
        case 0x138C: goto L_138C;
        case 0x13AC: goto L_13AC;
        case 0x13C0: goto L_13C0;
        case 0x1434: goto L_1434;
        case 0x1438: goto L_1438;
        case 0x1464: goto L_1464;
        case 0x1478: goto L_1478;
        case 0x14AC: goto L_14AC;
        case 0x14B4: goto L_14B4;
        case 0x14C4: goto L_14C4;
        case 0x14F8: goto L_14F8;
        case 0x1518: goto L_1518;
        case 0x1544: goto L_1544;
        case 0x1564: goto L_1564;
        case 0x1574: goto L_1574;
        case 0x15CC: goto L_15CC;
        case 0x15DC: goto L_15DC;
        case 0x15EC: goto L_15EC;
        case 0x1608: goto L_1608;
        case 0x161C: goto L_161C;
        case 0x162C: goto L_162C;
        case 0x1630: goto L_1630;
        case 0x1644: goto L_1644;
        case 0x1648: goto L_1648;
        case 0x1654: goto L_1654;
        case 0x1658: goto L_1658;
        case 0x165C: goto L_165C;
        case 0x1668: goto L_1668;
        case 0x166C: goto L_166C;
        case 0x167C: goto L_167C;
        case 0x1680: goto L_1680;
        case 0x1690: goto L_1690;
        case 0x16B0: goto L_16B0;
        case 0x16B4: goto L_16B4;
        case 0x1704: goto L_1704;
        case 0x171C: goto L_171C;
        case 0x1748: goto L_1748;
        case 0x1750: goto L_1750;
        case 0x1794: goto L_1794;
        case 0x17F4: goto L_17F4;
        case 0x1804: goto L_1804;
        case 0x1848: goto L_1848;
        case 0x185C: goto L_185C;
        case 0x1860: goto L_1860;
        case 0x186C: goto L_186C;
        case 0x1880: goto L_1880;
        case 0x1888: goto L_1888;
        case 0x1898: goto L_1898;
        case 0x189C: goto L_189C;
        case 0x18A0: goto L_18A0;
        case 0x18AC: goto L_18AC;
        case 0x18B4: goto L_18B4;
        case 0x191C: goto L_191C;
        case 0x1938: goto L_1938;
        case 0x1944: goto L_1944;
        case 0x1964: goto L_1964;
        case 0x1980: goto L_1980;
        case 0x1988: goto L_1988;
        case 0x198C: goto L_198C;
    }
    fprintf(stderr, "Unhandled jump target 0x%04X in microcode gbTowerColorMain, coming from [%s:%d]\n", jump_target, debug_file, debug_line);
    fprintf(stderr, "PC trail (oldest..newest):\n");
    for (uint32_t i = 0; i < 32; i++) {
        uint32_t pos = (ctx->pc_trail_idx + i) & 31;
        fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
    }
    fprintf(stderr, "Register dump: r0  = %08X r1  = %08X r2  = %08X r3  = %08X r4  = %08X r5  = %08X r6  = %08X r7  = %08X\n"
           "               r8  = %08X r9  = %08X r10 = %08X r11 = %08X r12 = %08X r13 = %08X r14 = %08X r15 = %08X\n"
           "               r16 = %08X r17 = %08X r18 = %08X r19 = %08X r20 = %08X r21 = %08X r22 = %08X r23 = %08X\n"
           "               r24 = %08X r25 = %08X r26 = %08X r27 = %08X r28 = %08X r29 = %08X r30 = %08X r31 = %08X\n",
           0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15, r16,
           r17, r18, r19, r20, r21, r22, r23, r24, r25, r26, r27, r28, r29, r30, r31);
    fflush(stderr);
    return RspExitReason::UnhandledJumpTarget;
}

RspExitReason gbTowerColorMain(uint8_t* rdram, [[maybe_unused]] uint32_t ucode_addr) {
    static thread_local RspContext persistent_ctx{};
    // Pre-task hook: if a runtime registered a hook keyed by
    // this ucode's name, call it here. Lets game-specific code
    // replicate parts of rspboot's setup that the static
    // recompilation can't infer (initial GPRs, DMA-engine
    // residue, pre-loaded command data in DMEM). Inline
    // null-check by the std::unordered_map lookup — typical
    // cost is one branch when no hook is registered.
    recomp::rsp::run_pre_task_hook(rdram, &persistent_ctx, "gbTowerColorMain", ucode_addr);
    return gbTowerColorMain_impl(rdram, &persistent_ctx);
}
