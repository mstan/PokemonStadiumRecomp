#include "librecomp/rsp.hpp"
#include "librecomp/rsp_vu_impl.hpp"
RspExitReason gbTowerMain_impl(uint8_t* rdram, RspContext* ctx) {
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
    // jal         0x1634
    r31 = 0x1040;
    // or          $5, $8, $zero
    r5 = r8 | 0;
    goto L_1634;
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
    // jal         0x166C
    r31 = 0x1048;
    // or          $5, $9, $zero
    r5 = r9 | 0;
    goto L_166C;
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
    // jal         0x1634
    r31 = 0x106C;
    // or          $5, $12, $zero
    r5 = r12 | 0;
    goto L_1634;
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
    // jal         0x1630
    r31 = 0x107C;
    // addiu       $5, $11, 0xA40
    r5 = RSP_ADD32(r11, 0XA40);
    goto L_1630;
    // addiu       $5, $11, 0xA40
    r5 = RSP_ADD32(r11, 0XA40);
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
    // jal         0x1634
    r31 = 0x10FC;
    // addu        $5, $9, $10
    r5 = RSP_ADD32(r9, r10);
    goto L_1634;
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
    // j           L_1634
    // addiu       $ra, $zero, 0x1168
    r31 = RSP_ADD32(0, 0X1168);
    goto L_1634;
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
    // jal         0x1634
    r31 = 0x113C;
    // addu        $5, $9, $10
    r5 = RSP_ADD32(r9, r10);
    goto L_1634;
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
    // jal         0x1634
    r31 = 0x1148;
    // addiu       $5, $5, 0x300
    r5 = RSP_ADD32(r5, 0X300);
    goto L_1634;
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
    // jal         0x1634
    r31 = 0x115C;
    // addu        $5, $9, $10
    r5 = RSP_ADD32(r9, r10);
    goto L_1634;
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
    // jal         0x1634
    r31 = 0x1168;
    // addiu       $5, $5, 0x300
    r5 = RSP_ADD32(r5, 0X300);
    goto L_1634;
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
    // jal         0x1644
    r31 = 0x1170;
    // nop

    goto L_1644;
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
    // ori         $17, $17, 0x81
    r17 = r17 | 0X81;
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
    // andi        $11, $9, 0x91
    r11 = r9 & 0X91;
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
    // jal         0x1630
    r31 = 0x11EC;
    // addu        $5, $5, $11
    r5 = RSP_ADD32(r5, r11);
    goto L_1630;
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
    // j           L_167C
    // addiu       $25, $zero, 0x118C
    r25 = RSP_ADD32(0, 0X118C);
    goto L_167C;
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
    // ori         $17, $17, 0xA1
    r17 = r17 | 0XA1;
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
    // andi        $11, $9, 0xB1
    r11 = r9 & 0XB1;
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
    // jal         0x1630
    r31 = 0x1284;
    // addu        $5, $5, $11
    r5 = RSP_ADD32(r5, r11);
    goto L_1630;
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
    // j           L_167C
    // addiu       $25, $zero, 0x1224
    r25 = RSP_ADD32(0, 0X1224);
    goto L_167C;
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
    // jal         0x1644
    r31 = 0x12C0;
    // lbu         $8, 0xE7F($zero)
    r8 = RSP_MEM_BU(0XE7F, 0);
    goto L_1644;
    // lbu         $8, 0xE7F($zero)
    r8 = RSP_MEM_BU(0XE7F, 0);
L_12C0:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x12C0;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x12C0 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // andi        $8, $8, 0x3
    r8 = r8 & 0X3;
    // sll         $8, $8, 1
    r8 = S32(U32(r8) << 1);
    // sll         $9, $8, 8
    r9 = S32(U32(r8) << 8);
    // or          $8, $8, $9
    r8 = r8 | r9;
    // mtc2        $8, $v0[0]
    rsp.MTC2<0>(r8, rsp.vpu.r[0]);
    // lw          $5, 0xFE0($zero)
    r5 = RSP_MEM_W_LOAD(0XFE0, 0);
    // lbu         $8, 0xA00($zero)
    r8 = RSP_MEM_BU(0XA00, 0);
    // addiu       $4, $zero, 0x0
    r4 = RSP_ADD32(0, 0X0);
    // addiu       $6, $zero, 0x9F
    r6 = RSP_ADD32(0, 0X9F);
    // addiu       $28, $zero, -0x480
    r28 = RSP_ADD32(0, -0X480);
    // vaddc       $v0, $v31, $v0[0]
    rsp.VADDC<8>(rsp.vpu.r[0], rsp.vpu.r[31], rsp.vpu.r[0]);
    // sqv         $v0[0], 0x0($zero)
    rsp.SQV<0>(rsp.vpu.r[0], 0, 0X0);
    // sqv         $v0[0], 0x10($zero)
    rsp.SQV<0>(rsp.vpu.r[0], 0, 0X1);
    // sqv         $v0[0], 0x20($zero)
    rsp.SQV<0>(rsp.vpu.r[0], 0, 0X2);
    // sqv         $v0[0], 0x30($zero)
    rsp.SQV<0>(rsp.vpu.r[0], 0, 0X3);
    // sqv         $v0[0], 0x40($zero)
    rsp.SQV<0>(rsp.vpu.r[0], 0, 0X4);
    // sqv         $v0[0], 0x50($zero)
    rsp.SQV<0>(rsp.vpu.r[0], 0, 0X5);
    // sqv         $v0[0], 0x60($zero)
    rsp.SQV<0>(rsp.vpu.r[0], 0, 0X6);
    // sqv         $v0[0], 0x70($zero)
    rsp.SQV<0>(rsp.vpu.r[0], 0, 0X7);
    // sqv         $v0[0], 0x80($zero)
    rsp.SQV<0>(rsp.vpu.r[0], 0, 0X8);
    // sqv         $v0[0], 0x90($zero)
    rsp.SQV<0>(rsp.vpu.r[0], 0, 0X9);
    // addiu       $5, $5, -0xA0
    r5 = RSP_ADD32(r5, -0XA0);
L_1318:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1318;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1318 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $5, $5, 0xA0
    r5 = RSP_ADD32(r5, 0XA0);
    // beq         $28, $zero, L_133C
    if (r28 == 0) {
        // andi        $10, $8, 0x1
        r10 = r8 & 0X1;
        goto L_133C;
    }
    // andi        $10, $8, 0x1
    r10 = r8 & 0X1;
    // beq         $10, $zero, L_1330
    if (r10 == 0) {
        // lbu         $8, 0xE88($28)
        r8 = RSP_MEM_BU(0XE88, r28);
        goto L_1330;
    }
    // lbu         $8, 0xE88($28)
    r8 = RSP_MEM_BU(0XE88, r28);
    // j           L_1318
    // addiu       $28, $28, 0x8
    r28 = RSP_ADD32(r28, 0X8);
    goto L_1318;
L_1330:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1330;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1330 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // addiu       $28, $28, 0x8
    r28 = RSP_ADD32(r28, 0X8);
    // j           L_166C
    // addiu       $ra, $zero, 0x1318
    r31 = RSP_ADD32(0, 0X1318);
    goto L_166C;
    // addiu       $ra, $zero, 0x1318
    r31 = RSP_ADD32(0, 0X1318);
L_133C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x133C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x133C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
L_1340:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1340;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1340 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // beq         $8, $zero, L_1404
    if (r8 == 0) {
        // addiu       $16, $16, -0x10
        r16 = RSP_ADD32(r16, -0X10);
        goto L_1404;
    }
    // addiu       $16, $16, -0x10
    r16 = RSP_ADD32(r16, -0X10);
    // bgez        $16, L_137C
    if (RSP_SIGNED(r16) >= 0) {
        // addiu       $7, $zero, 0x7
        r7 = RSP_ADD32(0, 0X7);
        goto L_137C;
    }
    // addiu       $7, $zero, 0x7
    r7 = RSP_ADD32(0, 0X7);
    // addu        $7, $7, $16
    r7 = RSP_ADD32(r7, r16);
    // addiu       $16, $zero, 0x0
    r16 = RSP_ADD32(0, 0X0);
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
    // sll         $28, $16, 3
    r28 = S32(U32(r16) << 3);
    // lbu         $9, 0xA00($28)
    r9 = RSP_MEM_BU(0XA00, r28);
    // lw          $5, 0xFEC($zero)
    r5 = RSP_MEM_W_LOAD(0XFEC, 0);
    // andi        $8, $19, 0xFE
    r8 = r19 & 0XFE;
    // sll         $8, $8, 4
    r8 = S32(U32(r8) << 4);
    // addiu       $8, $8, 0x4000
    r8 = RSP_ADD32(r8, 0X4000);
    // addu        $5, $5, $8
    r5 = RSP_ADD32(r5, r8);
    // addiu       $4, $zero, 0xF80
    r4 = RSP_ADD32(0, 0XF80);
    // jal         0x1634
    r31 = 0x13A4;
    // addiu       $6, $zero, 0x1F
    r6 = RSP_ADD32(0, 0X1F);
    goto L_1634;
    // addiu       $6, $zero, 0x1F
    r6 = RSP_ADD32(0, 0X1F);
L_13A4:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x13A4;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x13A4 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // bne         $9, $zero, L_13D0
    if (r9 != 0) {
        // srl         $10, $10, 2
        r10 = S32(U32(r10) >> 2);
        goto L_13D0;
    }
    // srl         $10, $10, 2
    r10 = S32(U32(r10) >> 2);
    // andi        $10, $19, 0x1
    r10 = r19 & 0X1;
    // sll         $10, $10, 4
    r10 = S32(U32(r10) << 4);
    // j           L_13F4
    // or          $29, $29, $10
    r29 = r29 | r10;
    goto L_13F4;
L_13D0:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x13D0;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x13D0 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // bltz        $7, L_13E4
    if (RSP_SIGNED(r7) < 0) {
        // xori        $15, $29, 0x10
        r15 = r29 ^ 0X10;
        goto L_13E4;
    }
    // xori        $15, $29, 0x10
    r15 = r29 ^ 0X10;
    // j           L_175C
    // addiu       $25, $zero, 0x13E4
    r25 = RSP_ADD32(0, 0X13E4);
    goto L_175C;
    // addiu       $25, $zero, 0x13E4
    r25 = RSP_ADD32(0, 0X13E4);
L_13E4:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x13E4;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x13E4 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
L_13F4:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x13F4;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x13F4 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // bltz        $7, L_1404
    if (RSP_SIGNED(r7) < 0) {
        // sll         $28, $16, 3
        r28 = S32(U32(r16) << 3);
        goto L_1404;
    }
    // sll         $28, $16, 3
    r28 = S32(U32(r16) << 3);
    // j           L_175C
    // addiu       $25, $zero, 0x1404
    r25 = RSP_ADD32(0, 0X1404);
    goto L_175C;
    // addiu       $25, $zero, 0x1404
    r25 = RSP_ADD32(0, 0X1404);
L_1404:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1404;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1404 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // bltz        $2, L_1340
    if (RSP_SIGNED(r2) < 0) {
        // addiu       $2, $2, 0x4
        r2 = RSP_ADD32(r2, 0X4);
        goto L_1340;
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
    // jal         0x1630
    r31 = 0x1424;
    // sw          $8, 0xFF8($zero)
    RSP_MEM_W_STORE(0XFF8, 0, r8);
    goto L_1630;
    // sw          $8, 0xFF8($zero)
    RSP_MEM_W_STORE(0XFF8, 0, r8);
L_1424:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1424;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1424 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // bne         $8, $4, L_1438
    if (r8 != r4) {
        // sqv         $v5[0], 0x0($8)
        rsp.SQV<0>(rsp.vpu.r[5], r8, 0X0);
        goto L_1438;
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
    // bgezal      $8, L_188C
    if (RSP_SIGNED(r8) >= 0) {
        r31 = 0x14AC;
    // addiu       $7, $zero, 0x28
        r7 = RSP_ADD32(0, 0X28);
        goto L_188C;
    }
    // addiu       $7, $zero, 0x28
    r7 = RSP_ADD32(0, 0X28);
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
    // addiu       $25, $zero, 0x14B0
    r25 = RSP_ADD32(0, 0X14B0);
L_14B0:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x14B0;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x14B0 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // bne         $7, $zero, L_18C8
    if (r7 != 0) {
        // addiu       $7, $7, -0x1
        r7 = RSP_ADD32(r7, -0X1);
        goto L_18C8;
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
    // addiu       $4, $zero, 0xF20
    r4 = RSP_ADD32(0, 0XF20);
    // addiu       $8, $4, 0x20
    r8 = RSP_ADD32(r4, 0X20);
    // j           L_19BC
    // addiu       $25, $zero, 0x14E8
    r25 = RSP_ADD32(0, 0X14E8);
    goto L_19BC;
    // addiu       $25, $zero, 0x14E8
    r25 = RSP_ADD32(0, 0X14E8);
L_14E8:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x14E8;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x14E8 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lw          $5, 0xFF0($zero)
    r5 = RSP_MEM_W_LOAD(0XFF0, 0);
    // addiu       $4, $zero, 0xA0
    r4 = RSP_ADD32(0, 0XA0);
    // addiu       $6, $zero, 0x5F
    r6 = RSP_ADD32(0, 0X5F);
    // jal         0x1634
    r31 = 0x14FC;
    // addiu       $5, $5, 0x540
    r5 = RSP_ADD32(r5, 0X540);
    goto L_1634;
    // addiu       $5, $5, 0x540
    r5 = RSP_ADD32(r5, 0X540);
L_14FC:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x14FC;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x14FC after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // or          $15, $4, $zero
    r15 = r4 | 0;
L_1500:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1500;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1500 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // beq         $2, $3, L_1608
    if (r2 == r3) {
        // addiu       $6, $zero, 0x9F
        r6 = RSP_ADD32(0, 0X9F);
        goto L_1608;
    }
    // addiu       $6, $zero, 0x9F
    r6 = RSP_ADD32(0, 0X9F);
    // addiu       $4, $zero, 0x0
    r4 = RSP_ADD32(0, 0X0);
    // jal         0x1630
    r31 = 0x1514;
    // or          $5, $29, $zero
    r5 = r29 | 0;
    goto L_1630;
    // or          $5, $29, $zero
    r5 = r29 | 0;
L_1514:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1514;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1514 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // addiu       $13, $18, 0x140
    r13 = RSP_ADD32(r18, 0X140);
    // addiu       $25, $zero, 0x0
    r25 = RSP_ADD32(0, 0X0);
    // or          $24, $18, $zero
    r24 = r18 | 0;
    // addiu       $ra, $zero, 0x1A14
    r31 = RSP_ADD32(0, 0X1A14);
L_1528:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1528;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1528 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // jr          $ra
    jump_target = r31;
    debug_file = __FILE__; debug_line = __LINE__;
    // andi        $12, $12, 0x18
    r12 = r12 & 0X18;
    goto do_indirect_jump;
L_152C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x152C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x152C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // andi        $12, $12, 0x18
    r12 = r12 & 0X18;
L_1530:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1530;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1530 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // andi        $8, $8, 0x6
    r8 = r8 & 0X6;
    // or          $8, $8, $12
    r8 = r8 | r12;
    // lhu         $8, 0xF20($8)
    r8 = RSP_MEM_HU_LOAD(0XF20, r8);
    // lbu         $11, 0x3($25)
    r11 = RSP_MEM_BU(0X3, r25);
    // andi        $9, $9, 0x6
    r9 = r9 & 0X6;
    // or          $9, $9, $12
    r9 = r9 | r12;
    // lhu         $9, 0xF20($9)
    r9 = RSP_MEM_HU_LOAD(0XF20, r9);
    // sh          $8, 0x200($24)
    RSP_MEM_H_STORE(0X200, r24, r8);
    // andi        $10, $10, 0x6
    r10 = r10 & 0X6;
    // or          $10, $10, $12
    r10 = r10 | r12;
    // lhu         $10, 0xF20($10)
    r10 = RSP_MEM_HU_LOAD(0XF20, r10);
    // sh          $9, 0x202($24)
    RSP_MEM_H_STORE(0X202, r24, r9);
    // andi        $11, $11, 0x6
    r11 = r11 & 0X6;
    // or          $11, $11, $12
    r11 = r11 | r12;
    // lhu         $11, 0xF20($11)
    r11 = RSP_MEM_HU_LOAD(0XF20, r11);
    // sh          $10, 0x204($24)
    RSP_MEM_H_STORE(0X204, r24, r10);
    // addiu       $25, $25, 0x4
    r25 = RSP_ADD32(r25, 0X4);
    // addiu       $24, $24, 0x8
    r24 = RSP_ADD32(r24, 0X8);
    // sh          $11, 0x1FE($24)
    RSP_MEM_H_STORE(0X1FE, r24, r11);
    // bgez        $12, L_1530
    if (RSP_SIGNED(r12) >= 0) {
        // addiu       $12, $12, -0x8000
        r12 = RSP_ADD32(r12, -0X8000);
        goto L_1530;
    }
    // addiu       $12, $12, -0x8000
    r12 = RSP_ADD32(r12, -0X8000);
    // bne         $24, $13, L_1528
    if (r24 != r13) {
        // lui         $6, 0xA00
        r6 = S32(U32(0XA00) << 16);
        goto L_1528;
    }
    // lui         $6, 0xA00
    r6 = S32(U32(0XA00) << 16);
    // ori         $6, $6, 0x102F
    r6 = r6 | 0X102F;
    // addiu       $16, $zero, 0x1
    r16 = RSP_ADD32(0, 0X1);
    // j           L_18D0
    // addiu       $25, $zero, 0x15A8
    r25 = RSP_ADD32(0, 0X15A8);
    goto L_18D0;
    // addiu       $25, $zero, 0x15A8
    r25 = RSP_ADD32(0, 0X15A8);
L_15A8:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x15A8;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x15A8 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // andi        $8, $28, 0x38
    r8 = r28 & 0X38;
    // bne         $8, $zero, L_15B8
    if (r8 != 0) {
        // addiu       $15, $15, -0x5
        r15 = RSP_ADD32(r15, -0X5);
        goto L_15B8;
    }
    // addiu       $15, $15, -0x5
    r15 = RSP_ADD32(r15, -0X5);
    // addiu       $15, $15, 0x5
    r15 = RSP_ADD32(r15, 0X5);
L_15B8:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x15B8;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x15B8 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // bne         $28, $zero, L_1500
    if (r28 != 0) {
        // addiu       $28, $28, 0x8
        r28 = RSP_ADD32(r28, 0X8);
        goto L_1500;
    }
    // addiu       $28, $28, 0x8
    r28 = RSP_ADD32(r28, 0X8);
    // jal         0x1644
    r31 = 0x15C8;
    // addiu       $7, $zero, 0x28
    r7 = RSP_ADD32(0, 0X28);
    goto L_1644;
    // addiu       $7, $zero, 0x28
    r7 = RSP_ADD32(0, 0X28);
L_15C8:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x15C8;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x15C8 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // addiu       $25, $zero, 0x15D8
    r25 = RSP_ADD32(0, 0X15D8);
L_15D8:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x15D8;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x15D8 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // beq         $2, $3, L_1608
    if (r2 == r3) {
        // addiu       $16, $zero, 0x0
        r16 = RSP_ADD32(0, 0X0);
        goto L_1608;
    }
    // addiu       $16, $zero, 0x0
    r16 = RSP_ADD32(0, 0X0);
    // bgtz        $24, L_15F4
    if (RSP_SIGNED(r24) > 0) {
        // addiu       $24, $24, -0x1
        r24 = RSP_ADD32(r24, -0X1);
        goto L_15F4;
    }
    // addiu       $24, $24, -0x1
    r24 = RSP_ADD32(r24, -0X1);
    // blez        $14, L_15F4
    if (RSP_SIGNED(r14) <= 0) {
        // addiu       $14, $14, -0x1
        r14 = RSP_ADD32(r14, -0X1);
        goto L_15F4;
    }
    // addiu       $14, $14, -0x1
    r14 = RSP_ADD32(r14, -0X1);
    // addiu       $16, $zero, 0x2
    r16 = RSP_ADD32(0, 0X2);
L_15F4:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x15F4;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x15F4 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // bgtz        $7, L_18CC
    if (RSP_SIGNED(r7) > 0) {
        // addiu       $7, $7, -0x1
        r7 = RSP_ADD32(r7, -0X1);
        goto L_18CC;
    }
    // addiu       $7, $7, -0x1
    r7 = RSP_ADD32(r7, -0X1);
    // addiu       $8, $zero, 0x0
    r8 = RSP_ADD32(0, 0X0);
    // j           L_1644
    // addiu       $ra, $zero, 0x1890
    r31 = RSP_ADD32(0, 0X1890);
    goto L_1644;
    // addiu       $ra, $zero, 0x1890
    r31 = RSP_ADD32(0, 0X1890);
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
    // mfc0        $1, DPC_CLOCK
    r1 = 0;
    // subu        $1, $1, $26
    r1 = RSP_SUB32(r1, r26);
    // sw          $1, 0xFFC($zero)
    RSP_MEM_W_STORE(0XFFC, 0, r1);
    // jal         0x1644
    r31 = 0x161C;
    // mfc0        $8, SP_STATUS
    r8 = 0;
    goto L_1644;
L_1618:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1618;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1618 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // andi        $8, $8, 0x400
    r8 = r8 & 0X400;
    // bne         $8, $zero, L_1618
    if (r8 != 0) {
        // ori         $8, $zero, 0x4000
        r8 = 0 | 0X4000;
        goto L_1618;
    }
    // ori         $8, $zero, 0x4000
    r8 = 0 | 0X4000;
    // mtc0        $8, SP_STATUS
    WRITE_SP_STATUS(r8);
    // break       0
    return RspExitReason::Broke;
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
    // ori         $1, $zero, 0xFFFF
    r1 = 0 | 0XFFFF;
L_1634:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1634;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1634 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
L_1640:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1640;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1640 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // beq         $1, $zero, L_1654
    if (r1 == 0) {
        // mfc0        $1, SP_DMA_BUSY
        r1 = 0;
        goto L_1654;
    }
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
    // mfc0        $1, SP_DMA_BUSY
    r1 = 0;
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
    // bne         $1, $zero, L_1648
    if (r1 != 0) {
        // mfc0        $1, SP_DMA_BUSY
        r1 = 0;
        goto L_1648;
    }
    // mfc0        $1, SP_DMA_BUSY
    r1 = 0;
    // jr          $ra
    jump_target = r31;
    debug_file = __FILE__; debug_line = __LINE__;
    // mfc0        $1, SP_DMA_FULL
    r1 = 0;
    goto do_indirect_jump;
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
    // mfc0        $1, SP_DMA_FULL
    r1 = 0;
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
    // bne         $1, $zero, L_1658
    if (r1 != 0) {
        // mfc0        $1, SP_DMA_FULL
        r1 = 0;
        goto L_1658;
    }
    // mfc0        $1, SP_DMA_FULL
    r1 = 0;
    // jr          $ra
    jump_target = r31;
    debug_file = __FILE__; debug_line = __LINE__;
    // nop

    goto do_indirect_jump;
    // nop

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
    // ori         $1, $zero, 0xFFFF
    r1 = 0 | 0XFFFF;
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
    // mtc0        $4, SP_MEM_ADDR
    SET_DMA_MEM(r4);
    // mtc0        $5, SP_DRAM_ADDR
    SET_DMA_DRAM(r5);
    // j           L_1640
    // mtc0        $6, SP_WR_LEN
    DO_DMA_WRITE(r6);
    goto L_1640;
    // mtc0        $6, SP_WR_LEN
    DO_DMA_WRITE(r6);
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
    // jal         0x1630
    r31 = 0x169C;
    // addu        $5, $5, $8
    r5 = RSP_ADD32(r5, r8);
    goto L_1630;
    // addu        $5, $5, $8
    r5 = RSP_ADD32(r5, r8);
L_169C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x169C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x169C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lbu         $12, 0xA07($28)
    r12 = RSP_MEM_BU(0XA07, r28);
    // andi        $8, $12, 0x3
    r8 = r12 & 0X3;
    // sll         $8, $8, 9
    r8 = S32(U32(r8) << 9);
    // mtc2        $8, $v17[0]
    rsp.MTC2<0>(r8, rsp.vpu.r[17]);
    // andi        $9, $12, 0xC
    r9 = r12 & 0XC;
    // sll         $9, $9, 7
    r9 = S32(U32(r9) << 7);
    // ori         $9, $9, 0x800
    r9 = r9 | 0X800;
    // mtc2        $9, $v2[2]
    rsp.MTC2<2>(r9, rsp.vpu.r[2]);
    // andi        $10, $12, 0x30
    r10 = r12 & 0X30;
    // sll         $10, $10, 5
    r10 = S32(U32(r10) << 5);
    // ori         $10, $10, 0x1000
    r10 = r10 | 0X1000;
    // mtc2        $10, $v2[4]
    rsp.MTC2<4>(r10, rsp.vpu.r[2]);
    // andi        $11, $12, 0xC0
    r11 = r12 & 0XC0;
    // sll         $11, $11, 3
    r11 = S32(U32(r11) << 3);
    // ori         $11, $11, 0x1800
    r11 = r11 | 0X1800;
    // mtc2        $11, $v2[6]
    rsp.MTC2<6>(r11, rsp.vpu.r[2]);
    // vaddc       $v17, $v31, $v17[0]
    rsp.VADDC<8>(rsp.vpu.r[17], rsp.vpu.r[31], rsp.vpu.r[17]);
    // addiu       $24, $18, 0x1
    r24 = RSP_ADD32(r18, 0X1);
L_16E4:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x16E4;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x16E4 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // bgez        $22, L_1754
    if (RSP_SIGNED(r22) >= 0) {
        // andi        $9, $20, 0x7FE
        r9 = r20 & 0X7FE;
        goto L_1754;
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
    // lpv         $v6[0], 0x0($24)
    rsp.LPV<0>(rsp.vpu.r[6], r24, 0X0);
    // vand        $v3, $v29, $v2[0]
    rsp.VAND<8>(rsp.vpu.r[3], rsp.vpu.r[29], rsp.vpu.r[2]);
    // vne         $v30, $v3, $v29
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[3], rsp.vpu.r[29]);
    // vmrg        $v4, $v17, $v2[3]
    rsp.VMRG<11>(rsp.vpu.r[4], rsp.vpu.r[17], rsp.vpu.r[2]);
    // vne         $v30, $v3, $v27
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[3], rsp.vpu.r[27]);
    // vmrg        $v4, $v4, $v2[1]
    rsp.VMRG<9>(rsp.vpu.r[4], rsp.vpu.r[4], rsp.vpu.r[2]);
    // vne         $v30, $v3, $v28
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[3], rsp.vpu.r[28]);
    // vmrg        $v4, $v4, $v2[2]
    rsp.VMRG<10>(rsp.vpu.r[4], rsp.vpu.r[4], rsp.vpu.r[2]);
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
    // bltz        $22, L_16E4
    if (RSP_SIGNED(r22) < 0) {
        // spv         $v4[0], 0x3F8($24)
        rsp.SPV<0>(rsp.vpu.r[4], r24, -0X1);
        goto L_16E4;
    }
    // spv         $v4[0], 0x3F8($24)
    rsp.SPV<0>(rsp.vpu.r[4], r24, -0X1);
L_1754:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1754;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1754 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // j           L_166C
    // or          $ra, $25, $zero
    r31 = r25 | 0;
    goto L_166C;
    // or          $ra, $25, $zero
    r31 = r25 | 0;
L_175C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x175C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x175C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // jal         0x1630
    r31 = 0x17A0;
    // addiu       $4, $zero, 0x0
    r4 = RSP_ADD32(0, 0X0);
    goto L_1630;
    // addiu       $4, $zero, 0x0
    r4 = RSP_ADD32(0, 0X0);
L_17A0:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x17A0;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x17A0 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // beq         $10, $zero, L_1878
    if (r10 == 0) {
        // addiu       $7, $7, -0x1
        r7 = RSP_ADD32(r7, -0X1);
        goto L_1878;
    }
    // addiu       $7, $7, -0x1
    r7 = RSP_ADD32(r7, -0X1);
    // lhu         $9, 0xF80($29)
    r9 = RSP_MEM_HU_LOAD(0XF80, r29);
    // lhu         $12, 0xA0C($28)
    r12 = RSP_MEM_HU_LOAD(0XA0C, r28);
    // sll         $10, $3, 8
    r10 = S32(U32(r3) << 8);
    // lpv         $v0[0], 0x0($8)
    rsp.LPV<0>(rsp.vpu.r[0], r8, 0X0);
    // mtc2        $10, $v3[0]
    rsp.MTC2<0>(r10, rsp.vpu.r[3]);
    // andi        $11, $3, 0x20
    r11 = r3 & 0X20;
    // andi        $10, $3, 0x10
    r10 = r3 & 0X10;
    // bne         $10, $zero, L_17E4
    if (r10 != 0) {
        // mtc2        $9, $v2[0]
        rsp.MTC2<0>(r9, rsp.vpu.r[2]);
        goto L_17E4;
    }
    // mtc2        $9, $v2[0]
    rsp.MTC2<0>(r9, rsp.vpu.r[2]);
    // srl         $12, $12, 8
    r12 = S32(U32(r12) >> 8);
L_17E4:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x17E4;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x17E4 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // andi        $9, $12, 0xC
    r9 = r12 & 0XC;
    // sll         $9, $9, 7
    r9 = S32(U32(r9) << 7);
    // mtc2        $9, $v17[0]
    rsp.MTC2<0>(r9, rsp.vpu.r[17]);
    // andi        $10, $12, 0x30
    r10 = r12 & 0X30;
    // sll         $10, $10, 5
    r10 = S32(U32(r10) << 5);
    // mtc2        $10, $v2[4]
    rsp.MTC2<4>(r10, rsp.vpu.r[2]);
    // andi        $9, $12, 0xC0
    r9 = r12 & 0XC0;
    // sll         $9, $9, 3
    r9 = S32(U32(r9) << 3);
    // mtc2        $9, $v2[6]
    rsp.MTC2<6>(r9, rsp.vpu.r[2]);
    // vaddc       $v17, $v31, $v17[0]
    rsp.VADDC<8>(rsp.vpu.r[17], rsp.vpu.r[31], rsp.vpu.r[17]);
    // beq         $11, $zero, L_1828
    if (r11 == 0) {
        // mtc2        $18, $v9[0]
        rsp.MTC2<0>(r18, rsp.vpu.r[9]);
        goto L_1828;
    }
    // mtc2        $18, $v9[0]
    rsp.MTC2<0>(r18, rsp.vpu.r[9]);
    // vand        $v5, $v26, $v2[0]
    rsp.VAND<8>(rsp.vpu.r[5], rsp.vpu.r[26], rsp.vpu.r[2]);
    // vne         $v30, $v5, $v26
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[5], rsp.vpu.r[26]);
    // vmrg        $v6, $v17, $v2[3]
    rsp.VMRG<11>(rsp.vpu.r[6], rsp.vpu.r[17], rsp.vpu.r[2]);
    // j           L_1838
    // vne         $v30, $v5, $v24
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[5], rsp.vpu.r[24]);
    goto L_1838;
    // vne         $v30, $v5, $v24
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[5], rsp.vpu.r[24]);
L_1828:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1828;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1828 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // vmrg        $v6, $v17, $v2[3]
    rsp.VMRG<11>(rsp.vpu.r[6], rsp.vpu.r[17], rsp.vpu.r[2]);
    // vne         $v30, $v5, $v28
    rsp.VNE<0>(rsp.vpu.r[30], rsp.vpu.r[5], rsp.vpu.r[28]);
L_1838:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1838;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1838 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // vmrg        $v6, $v6, $v2[2]
    rsp.VMRG<10>(rsp.vpu.r[6], rsp.vpu.r[6], rsp.vpu.r[2]);
    // vor         $v8, $v0, $v3[0]
    rsp.VOR<8>(rsp.vpu.r[8], rsp.vpu.r[0], rsp.vpu.r[3]);
    // veq         $v30, $v5, $v31[0]
    rsp.VEQ<8>(rsp.vpu.r[30], rsp.vpu.r[5], rsp.vpu.r[31]);
    // vor         $v6, $v6, $v22[6]
    rsp.VOR<14>(rsp.vpu.r[6], rsp.vpu.r[6], rsp.vpu.r[22]);
    // vand        $v8, $v8, $v27[0]
    rsp.VAND<8>(rsp.vpu.r[8], rsp.vpu.r[8], rsp.vpu.r[27]);
    // vand        $v7, $v0, $v22[5]
    rsp.VAND<13>(rsp.vpu.r[7], rsp.vpu.r[0], rsp.vpu.r[22]);
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
L_1878:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1878;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1878 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // bgez        $7, L_17A0
    if (RSP_SIGNED(r7) >= 0) {
        // addiu       $24, $24, -0x10
        r24 = RSP_ADD32(r24, -0X10);
        goto L_17A0;
    }
    // addiu       $24, $24, -0x10
    r24 = RSP_ADD32(r24, -0X10);
    // jal         0x1668
    r31 = 0x188C;
    // or          $ra, $25, $zero
    r31 = r25 | 0;
    goto L_1668;
    // or          $ra, $25, $zero
    r31 = r25 | 0;
L_188C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x188C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x188C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
L_1890:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1890;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1890 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // addiu       $4, $4, -0x10
    r4 = RSP_ADD32(r4, -0X10);
    // bne         $4, $zero, L_189C
    if (r4 != 0) {
        // sqv         $v31[0], 0x200($4)
        rsp.SQV<0>(rsp.vpu.r[31], r4, 0X20);
        goto L_189C;
    }
    // sqv         $v31[0], 0x200($4)
    rsp.SQV<0>(rsp.vpu.r[31], r4, 0X20);
    // addiu       $4, $zero, 0x200
    r4 = RSP_ADD32(0, 0X200);
    // addu        $5, $5, $2
    r5 = RSP_ADD32(r5, r2);
L_18B0:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x18B0;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x18B0 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // jal         0x166C
    r31 = 0x18B8;
    // addu        $2, $2, $19
    r2 = RSP_ADD32(r2, r19);
    goto L_166C;
    // addu        $2, $2, $19
    r2 = RSP_ADD32(r2, r19);
L_18B8:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x18B8;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x18B8 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // bne         $8, $zero, L_18B0
    if (r8 != 0) {
        // addiu       $8, $8, -0x1
        r8 = RSP_ADD32(r8, -0X1);
        goto L_18B0;
    }
    // addiu       $8, $8, -0x1
    r8 = RSP_ADD32(r8, -0X1);
    // jr          $25
    jump_target = r25;
    debug_file = __FILE__; debug_line = __LINE__;
    // addiu       $16, $zero, 0x0
    r16 = RSP_ADD32(0, 0X0);
    goto do_indirect_jump;
L_18C8:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x18C8;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x18C8 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
L_18CC:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x18CC;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x18CC after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
L_18D0:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x18D0;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x18D0 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // jal         0x1630
    r31 = 0x18DC;
    // addiu       $4, $zero, 0x700
    r4 = RSP_ADD32(0, 0X700);
    goto L_1630;
    // addiu       $4, $zero, 0x700
    r4 = RSP_ADD32(0, 0X700);
L_18DC:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x18DC;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x18DC after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
L_18E4:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x18E4;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x18E4 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // bgez        $6, L_18E4
    if (RSP_SIGNED(r6) >= 0) {
        // sh          $11, 0x1FE($17)
        RSP_MEM_H_STORE(0X1FE, r17, r11);
        goto L_18E4;
    }
    // sh          $11, 0x1FE($17)
    RSP_MEM_H_STORE(0X1FE, r17, r11);
    // andi        $8, $16, 0x1
    r8 = r16 & 0X1;
    // beq         $8, $zero, L_194C
    if (r8 == 0) {
        // addiu       $16, $16, -0x1
        r16 = RSP_ADD32(r16, -0X1);
        goto L_194C;
    }
    // addiu       $16, $16, -0x1
    r16 = RSP_ADD32(r16, -0X1);
    // addiu       $17, $17, 0x140
    r17 = RSP_ADD32(r17, 0X140);
    // j           L_18E4
    // addiu       $6, $zero, 0x2F
    r6 = RSP_ADD32(0, 0X2F);
    goto L_18E4;
    // addiu       $6, $zero, 0x2F
    r6 = RSP_ADD32(0, 0X2F);
L_194C:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x194C;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x194C after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // blez        $16, L_1994
    if (RSP_SIGNED(r16) <= 0) {
        // sw          $5, 0xFF8($zero)
        RSP_MEM_W_STORE(0XFF8, 0, r5);
        goto L_1994;
    }
    // sw          $5, 0xFF8($zero)
    RSP_MEM_W_STORE(0XFF8, 0, r5);
    // or          $5, $12, $zero
    r5 = r12 | 0;
    // addiu       $6, $19, -0x1
    r6 = RSP_ADD32(r19, -0X1);
    // jal         0x1630
    r31 = 0x1968;
    // addiu       $4, $zero, 0x480
    r4 = RSP_ADD32(0, 0X480);
    goto L_1630;
    // addiu       $4, $zero, 0x480
    r4 = RSP_ADD32(0, 0X480);
L_1968:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1968;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1968 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
L_1974:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1974;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1974 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // bne         $4, $8, L_1974
    if (r4 != r8) {
        // sqv         $v0[0], 0x7F0($9)
        rsp.SQV<0>(rsp.vpu.r[0], r9, -0X1);
        goto L_1974;
    }
    // sqv         $v0[0], 0x7F0($9)
    rsp.SQV<0>(rsp.vpu.r[0], r9, -0X1);
L_1994:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1994;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1994 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // j           L_166C
    // addu        $2, $2, $19
    r2 = RSP_ADD32(r2, r19);
    goto L_166C;
    // addu        $2, $2, $19
    r2 = RSP_ADD32(r2, r19);
    // jal         0x1630
    r31 = 0x19B8;
    // addu        $8, $4, $6
    r8 = RSP_ADD32(r4, r6);
    goto L_1630;
    // addu        $8, $4, $6
    r8 = RSP_ADD32(r4, r6);
L_19B8:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x19B8;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x19B8 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
L_19BC:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x19BC;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x19BC after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
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
    // bne         $8, $4, L_19BC
    if (r8 != r4) {
        // sqv         $v3[0], 0x0($8)
        rsp.SQV<0>(rsp.vpu.r[3], r8, 0X0);
        goto L_19BC;
    }
    // sqv         $v3[0], 0x0($8)
    rsp.SQV<0>(rsp.vpu.r[3], r8, 0X0);
    // jr          $25
    jump_target = r25;
    debug_file = __FILE__; debug_line = __LINE__;
    // nop

    goto do_indirect_jump;
    // nop

L_1A14:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1A14;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1A14 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // lbu         $14, 0x0($15)
    r14 = RSP_MEM_BU(0X0, r15);
    // jal         0x152C
    r31 = 0x1A20;
    // srl         $12, $14, 3
    r12 = S32(U32(r14) >> 3);
    goto L_152C;
    // srl         $12, $14, 3
    r12 = S32(U32(r14) >> 3);
L_1A20:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1A20;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1A20 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // jal         0x152C
    r31 = 0x1A28;
    // srl         $12, $14, 1
    r12 = S32(U32(r14) >> 1);
    goto L_152C;
    // srl         $12, $14, 1
    r12 = S32(U32(r14) >> 1);
L_1A28:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1A28;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1A28 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // jal         0x152C
    r31 = 0x1A30;
    // sll         $12, $14, 1
    r12 = S32(U32(r14) << 1);
    goto L_152C;
    // sll         $12, $14, 1
    r12 = S32(U32(r14) << 1);
L_1A30:
    ctx->pc_trail[ctx->pc_trail_idx & 31] = 0x1A30;
    ctx->pc_trail_idx++;
    if (++ctx->watchdog_count > 100000000ULL) {
        fprintf(stderr, "[rsp watchdog] hung at PC 0x1A30 after %llu transitions; PC trail (oldest..newest):\n", (unsigned long long)ctx->watchdog_count);
        for (uint32_t i = 0; i < 32; i++) {
            uint32_t pos = (ctx->pc_trail_idx + i) & 31;
            fprintf(stderr, "  [%2u] PC=0x%04X\n", i, ctx->pc_trail[pos]);
        }
        fprintf(stderr, "[rsp watchdog] gprs: r1=%08X r2=%08X r3=%08X r25=%08X r26=%08X r27=%08X r28=%08X r29=%08X r30=%08X r31=%08X jt=%08X dma_mem=%08X dma_dram=%08X\n",
            ctx->r1, ctx->r2, ctx->r3, ctx->r25, ctx->r26, ctx->r27, ctx->r28, ctx->r29, ctx->r30, ctx->r31, ctx->jump_target, ctx->dma_mem_address, ctx->dma_dram_address);
        return RspExitReason::Watchdog;
    }
    // sll         $12, $14, 3
    r12 = S32(U32(r14) << 3);
    // addiu       $15, $15, 0x1
    r15 = RSP_ADD32(r15, 0X1);
    // j           L_152C
    // addiu       $ra, $zero, 0x1A14
    r31 = RSP_ADD32(0, 0X1A14);
    goto L_152C;
    // addiu       $ra, $zero, 0x1A14
    r31 = RSP_ADD32(0, 0X1A14);
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
        case 0x12C0: goto L_12C0;
        case 0x1318: goto L_1318;
        case 0x1330: goto L_1330;
        case 0x133C: goto L_133C;
        case 0x1340: goto L_1340;
        case 0x137C: goto L_137C;
        case 0x13A4: goto L_13A4;
        case 0x13D0: goto L_13D0;
        case 0x13E4: goto L_13E4;
        case 0x13F4: goto L_13F4;
        case 0x1404: goto L_1404;
        case 0x1424: goto L_1424;
        case 0x1438: goto L_1438;
        case 0x14AC: goto L_14AC;
        case 0x14B0: goto L_14B0;
        case 0x14E8: goto L_14E8;
        case 0x14FC: goto L_14FC;
        case 0x1500: goto L_1500;
        case 0x1514: goto L_1514;
        case 0x1528: goto L_1528;
        case 0x152C: goto L_152C;
        case 0x1530: goto L_1530;
        case 0x15A8: goto L_15A8;
        case 0x15B8: goto L_15B8;
        case 0x15C8: goto L_15C8;
        case 0x15D8: goto L_15D8;
        case 0x15F4: goto L_15F4;
        case 0x1608: goto L_1608;
        case 0x1618: goto L_1618;
        case 0x161C: goto L_161C;
        case 0x1630: goto L_1630;
        case 0x1634: goto L_1634;
        case 0x1640: goto L_1640;
        case 0x1644: goto L_1644;
        case 0x1648: goto L_1648;
        case 0x1654: goto L_1654;
        case 0x1658: goto L_1658;
        case 0x1668: goto L_1668;
        case 0x166C: goto L_166C;
        case 0x167C: goto L_167C;
        case 0x169C: goto L_169C;
        case 0x16E4: goto L_16E4;
        case 0x1754: goto L_1754;
        case 0x175C: goto L_175C;
        case 0x17A0: goto L_17A0;
        case 0x17E4: goto L_17E4;
        case 0x1828: goto L_1828;
        case 0x1838: goto L_1838;
        case 0x1878: goto L_1878;
        case 0x188C: goto L_188C;
        case 0x1890: goto L_1890;
        case 0x189C: goto L_189C;
        case 0x18B0: goto L_18B0;
        case 0x18B8: goto L_18B8;
        case 0x18C8: goto L_18C8;
        case 0x18CC: goto L_18CC;
        case 0x18D0: goto L_18D0;
        case 0x18DC: goto L_18DC;
        case 0x18E4: goto L_18E4;
        case 0x194C: goto L_194C;
        case 0x1968: goto L_1968;
        case 0x1974: goto L_1974;
        case 0x1994: goto L_1994;
        case 0x19B8: goto L_19B8;
        case 0x19BC: goto L_19BC;
        case 0x1A14: goto L_1A14;
        case 0x1A20: goto L_1A20;
        case 0x1A28: goto L_1A28;
        case 0x1A30: goto L_1A30;
    }
    fprintf(stderr, "Unhandled jump target 0x%04X in microcode gbTowerMain, coming from [%s:%d]\n", jump_target, debug_file, debug_line);
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

RspExitReason gbTowerMain(uint8_t* rdram, [[maybe_unused]] uint32_t ucode_addr) {
    static thread_local RspContext persistent_ctx{};
    // Pre-task hook: if a runtime registered a hook keyed by
    // this ucode's name, call it here. Lets game-specific code
    // replicate parts of rspboot's setup that the static
    // recompilation can't infer (initial GPRs, DMA-engine
    // residue, pre-loaded command data in DMEM). Inline
    // null-check by the std::unordered_map lookup — typical
    // cost is one branch when no hook is registered.
    recomp::rsp::run_pre_task_hook(rdram, &persistent_ctx, "gbTowerMain", ucode_addr);
    return gbTowerMain_impl(rdram, &persistent_ctx);
}
