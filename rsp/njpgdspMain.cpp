#include "librecomp/rsp.hpp"
#include "librecomp/rsp_vu_impl.hpp"
RspExitReason njpgdspMain(uint8_t* rdram, [[maybe_unused]] uint32_t ucode_addr) {
    uint32_t           r1 = 0,  r2 = 0,  r3 = 0,  r4 = 0,  r5 = 0,  r6 = 0,  r7 = 0;
    uint32_t  r8 = 0,  r9 = 0, r10 = 0, r11 = 0, r12 = 0, r13 = 0, r14 = 0, r15 = 0;
    uint32_t r16 = 0, r17 = 0, r18 = 0, r19 = 0, r20 = 0, r21 = 0, r22 = 0, r23 = 0;
    uint32_t r24 = 0, r25 = 0, r26 = 0, r27 = 0, r28 = 0, r29 = 0, r30 = 0, r31 = 0;
    uint32_t dma_mem_address = 0, dma_dram_address = 0, jump_target = 0;
    const char * debug_file = NULL; int debug_line = 0;
    RSP rsp{};
    r1 = 0xFC0;
    // lw          $22, 0x4($1)
    r22 = RSP_MEM_W_LOAD(0X4, r1);
    // lw          $23, 0x38($1)
    r23 = RSP_MEM_W_LOAD(0X38, r1);
    // lw          $24, 0x3C($1)
    r24 = RSP_MEM_W_LOAD(0X3C, r1);
    // andi        $22, $22, 0x1
    r22 = r22 & 0X1;
    // beq         $22, $zero, L_10B8
    if (r22 == 0) {
        // lqv         $v2[0], 0x20($zero)
        rsp.LQV<0>(rsp.vpu.r[2], 0, 0X2);
        goto L_10B8;
    }
    // lqv         $v2[0], 0x20($zero)
    rsp.LQV<0>(rsp.vpu.r[2], 0, 0X2);
    // lw          $3, 0x1E0($zero)
    r3 = RSP_MEM_W_LOAD(0X1E0, 0);
    // lw          $25, 0x1E4($zero)
    r25 = RSP_MEM_W_LOAD(0X1E4, 0);
    // lw          $26, 0x1E8($zero)
    r26 = RSP_MEM_W_LOAD(0X1E8, 0);
    // lw          $9, 0x1EC($zero)
    r9 = RSP_MEM_W_LOAD(0X1EC, 0);
    // lw          $10, 0x1F0($zero)
    r10 = RSP_MEM_W_LOAD(0X1F0, 0);
    // lw          $11, 0x1F4($zero)
    r11 = RSP_MEM_W_LOAD(0X1F4, 0);
    // j           L_1134
    // lw          $12, 0x1F8($zero)
    r12 = RSP_MEM_W_LOAD(0X1F8, 0);
    goto L_1134;
    // lw          $12, 0x1F8($zero)
    r12 = RSP_MEM_W_LOAD(0X1F8, 0);
L_10B8:
    // lw          $29, 0x34($1)
    r29 = RSP_MEM_W_LOAD(0X34, r1);
    // lw          $27, 0x30($1)
    r27 = RSP_MEM_W_LOAD(0X30, r1);
    // addi        $28, $zero, 0x1E0
    r28 = RSP_ADD32(0, 0X1E0);
    // jal         0x1B08
    r31 = 0x10CC;
    // addi        $29, $29, -0x1
    r29 = RSP_ADD32(r29, -0X1);
    goto L_1B08;
    // addi        $29, $29, -0x1
    r29 = RSP_ADD32(r29, -0X1);
L_10CC:
    // jal         0x1B58
    r31 = 0x10D4;
    // addi        $29, $zero, 0x7F
    r29 = RSP_ADD32(0, 0X7F);
    goto L_1B58;
    // addi        $29, $zero, 0x7F
    r29 = RSP_ADD32(0, 0X7F);
L_10D4:
    // lw          $27, 0x1EC($zero)
    r27 = RSP_MEM_W_LOAD(0X1EC, 0);
    // jal         0x1B08
    r31 = 0x10E0;
    // addi        $28, $zero, 0x60
    r28 = RSP_ADD32(0, 0X60);
    goto L_1B08;
    // addi        $28, $zero, 0x60
    r28 = RSP_ADD32(0, 0X60);
L_10E0:
    // jal         0x1B58
    r31 = 0x10E8;
    // lw          $27, 0x1F0($zero)
    r27 = RSP_MEM_W_LOAD(0X1F0, 0);
    goto L_1B58;
    // lw          $27, 0x1F0($zero)
    r27 = RSP_MEM_W_LOAD(0X1F0, 0);
L_10E8:
    // jal         0x1B08
    r31 = 0x10F0;
    // addi        $28, $zero, 0xE0
    r28 = RSP_ADD32(0, 0XE0);
    goto L_1B08;
    // addi        $28, $zero, 0xE0
    r28 = RSP_ADD32(0, 0XE0);
L_10F0:
    // jal         0x1B58
    r31 = 0x10F8;
    // lw          $27, 0x1F4($zero)
    r27 = RSP_MEM_W_LOAD(0X1F4, 0);
    goto L_1B58;
    // lw          $27, 0x1F4($zero)
    r27 = RSP_MEM_W_LOAD(0X1F4, 0);
L_10F8:
    // jal         0x1B08
    r31 = 0x1100;
    // addi        $28, $zero, 0x160
    r28 = RSP_ADD32(0, 0X160);
    goto L_1B08;
    // addi        $28, $zero, 0x160
    r28 = RSP_ADD32(0, 0X160);
L_1100:
    // jal         0x1B58
    r31 = 0x1108;
    // lw          $9, 0x1E8($zero)
    r9 = RSP_MEM_W_LOAD(0X1E8, 0);
    goto L_1B58;
    // lw          $9, 0x1E8($zero)
    r9 = RSP_MEM_W_LOAD(0X1E8, 0);
L_1108:
    // lw          $25, 0x1E0($zero)
    r25 = RSP_MEM_W_LOAD(0X1E0, 0);
    // bgtz        $9, L_1124
    if (RSP_SIGNED(r9) > 0) {
        // lw          $3, 0x1E4($zero)
        r3 = RSP_MEM_W_LOAD(0X1E4, 0);
        goto L_1124;
    }
    // lw          $3, 0x1E4($zero)
    r3 = RSP_MEM_W_LOAD(0X1E4, 0);
    // addi        $10, $zero, 0x1FF
    r10 = RSP_ADD32(0, 0X1FF);
    // addi        $11, $zero, 0xFF
    r11 = RSP_ADD32(0, 0XFF);
    // j           L_1130
    // addi        $12, $zero, 0x200
    r12 = RSP_ADD32(0, 0X200);
    goto L_1130;
    // addi        $12, $zero, 0x200
    r12 = RSP_ADD32(0, 0X200);
L_1124:
    // addi        $10, $zero, 0x2FF
    r10 = RSP_ADD32(0, 0X2FF);
    // addi        $11, $zero, 0x1FF
    r11 = RSP_ADD32(0, 0X1FF);
    // addi        $12, $zero, 0x300
    r12 = RSP_ADD32(0, 0X300);
L_1130:
    // sub         $26, $25, $12
    r26 = RSP_SUB32(r25, r12);
L_1134:
    // addi        $27, $25, 0x0
    r27 = RSP_ADD32(r25, 0X0);
    // addi        $28, $zero, 0x1E0
    r28 = RSP_ADD32(0, 0X1E0);
    // jal         0x1B08
    r31 = 0x1144;
    // add         $29, $zero, $10
    r29 = RSP_ADD32(0, r10);
    goto L_1B08;
    // add         $29, $zero, $10
    r29 = RSP_ADD32(0, r10);
L_1144:
    // add         $2, $zero, $3
    r2 = RSP_ADD32(0, r3);
    // lqv         $v3[0], 0x30($zero)
    rsp.LQV<0>(rsp.vpu.r[3], 0, 0X3);
    // vxor        $v0, $v0, $v0
    rsp.VXOR<0>(rsp.vpu.r[0], rsp.vpu.r[0], rsp.vpu.r[0]);
    // jal         0x1B58
    r31 = 0x1158;
    // add         $25, $25, $12
    r25 = RSP_ADD32(r25, r12);
    goto L_1B58;
    // add         $25, $25, $12
    r25 = RSP_ADD32(r25, r12);
L_1158:
    // beq         $2, $3, L_116C
    if (r2 == r3) {
        // addi        $27, $zero, 0xBE0
        r27 = RSP_ADD32(0, 0XBE0);
        goto L_116C;
    }
    // addi        $27, $zero, 0xBE0
    r27 = RSP_ADD32(0, 0XBE0);
    // addi        $28, $26, 0x0
    r28 = RSP_ADD32(r26, 0X0);
    // jal         0x1B30
    r31 = 0x116C;
    // add         $29, $zero, $11
    r29 = RSP_ADD32(0, r11);
    goto L_1B30;
    // add         $29, $zero, $11
    r29 = RSP_ADD32(0, r11);
L_116C:
    // addi        $3, $3, -0x1
    r3 = RSP_ADD32(r3, -0X1);
    // addi        $4, $zero, 0x1E0
    r4 = RSP_ADD32(0, 0X1E0);
    // addi        $5, $zero, 0x4E0
    r5 = RSP_ADD32(0, 0X4E0);
    // addi        $7, $zero, 0x3
    r7 = RSP_ADD32(0, 0X3);
    // addi        $8, $9, 0x2
    r8 = RSP_ADD32(r9, 0X2);
    // addi        $6, $zero, 0x60
    r6 = RSP_ADD32(0, 0X60);
    // lqv         $v1[0], 0x0($zero)
    rsp.LQV<0>(rsp.vpu.r[1], 0, 0X0);
    // lqv         $v16[0], 0x0($4)
    rsp.LQV<0>(rsp.vpu.r[16], r4, 0X0);
    // lqv         $v17[0], 0x10($4)
    rsp.LQV<0>(rsp.vpu.r[17], r4, 0X1);
    // lqv         $v18[0], 0x20($4)
    rsp.LQV<0>(rsp.vpu.r[18], r4, 0X2);
    // lqv         $v19[0], 0x30($4)
    rsp.LQV<0>(rsp.vpu.r[19], r4, 0X3);
    // lqv         $v20[0], 0x40($4)
    rsp.LQV<0>(rsp.vpu.r[20], r4, 0X4);
    // lqv         $v21[0], 0x50($4)
    rsp.LQV<0>(rsp.vpu.r[21], r4, 0X5);
    // lqv         $v22[0], 0x60($4)
    rsp.LQV<0>(rsp.vpu.r[22], r4, 0X6);
    // lqv         $v23[0], 0x70($4)
    rsp.LQV<0>(rsp.vpu.r[23], r4, 0X7);
    // addi        $4, $4, 0x80
    r4 = RSP_ADD32(r4, 0X80);
L_11AC:
    // lqv         $v5[0], 0x0($6)
    rsp.LQV<0>(rsp.vpu.r[5], r6, 0X0);
    // lqv         $v6[0], 0x10($6)
    rsp.LQV<0>(rsp.vpu.r[6], r6, 0X1);
    // lqv         $v7[0], 0x20($6)
    rsp.LQV<0>(rsp.vpu.r[7], r6, 0X2);
    // lqv         $v8[0], 0x30($6)
    rsp.LQV<0>(rsp.vpu.r[8], r6, 0X3);
    // lqv         $v9[0], 0x40($6)
    rsp.LQV<0>(rsp.vpu.r[9], r6, 0X4);
    // lqv         $v10[0], 0x50($6)
    rsp.LQV<0>(rsp.vpu.r[10], r6, 0X5);
    // lqv         $v11[0], 0x60($6)
    rsp.LQV<0>(rsp.vpu.r[11], r6, 0X6);
    // lqv         $v12[0], 0x70($6)
    rsp.LQV<0>(rsp.vpu.r[12], r6, 0X7);
    // addi        $7, $7, -0x1
    r7 = RSP_ADD32(r7, -0X1);
L_11D0:
    // sqv         $v24[0], 0x0($5)
    rsp.SQV<0>(rsp.vpu.r[24], r5, 0X0);
    // vmudh       $v16, $v16, $v5
    rsp.VMUDH<0>(rsp.vpu.r[16], rsp.vpu.r[16], rsp.vpu.r[5]);
    // sqv         $v25[0], 0x10($5)
    rsp.SQV<0>(rsp.vpu.r[25], r5, 0X1);
    // vmudh       $v17, $v17, $v6
    rsp.VMUDH<0>(rsp.vpu.r[17], rsp.vpu.r[17], rsp.vpu.r[6]);
    // sqv         $v26[0], 0x20($5)
    rsp.SQV<0>(rsp.vpu.r[26], r5, 0X2);
    // vmudh       $v18, $v18, $v7
    rsp.VMUDH<0>(rsp.vpu.r[18], rsp.vpu.r[18], rsp.vpu.r[7]);
    // sqv         $v27[0], 0x30($5)
    rsp.SQV<0>(rsp.vpu.r[27], r5, 0X3);
    // vmudh       $v19, $v19, $v8
    rsp.VMUDH<0>(rsp.vpu.r[19], rsp.vpu.r[19], rsp.vpu.r[8]);
    // sqv         $v28[0], 0x40($5)
    rsp.SQV<0>(rsp.vpu.r[28], r5, 0X4);
    // vmudh       $v20, $v20, $v9
    rsp.VMUDH<0>(rsp.vpu.r[20], rsp.vpu.r[20], rsp.vpu.r[9]);
    // sqv         $v29[0], 0x50($5)
    rsp.SQV<0>(rsp.vpu.r[29], r5, 0X5);
    // vmudh       $v21, $v21, $v10
    rsp.VMUDH<0>(rsp.vpu.r[21], rsp.vpu.r[21], rsp.vpu.r[10]);
    // sqv         $v30[0], 0x60($5)
    rsp.SQV<0>(rsp.vpu.r[30], r5, 0X6);
    // vmudh       $v22, $v22, $v11
    rsp.VMUDH<0>(rsp.vpu.r[22], rsp.vpu.r[22], rsp.vpu.r[11]);
    // sqv         $v31[0], 0x70($5)
    rsp.SQV<0>(rsp.vpu.r[31], r5, 0X7);
    // vmudh       $v23, $v23, $v12
    rsp.VMUDH<0>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[12]);
    // vmudn       $v24, $v16, $v1[0]
    rsp.VMUDN<8>(rsp.vpu.r[24], rsp.vpu.r[16], rsp.vpu.r[1]);
    // vmudn       $v25, $v17, $v1[0]
    rsp.VMUDN<8>(rsp.vpu.r[25], rsp.vpu.r[17], rsp.vpu.r[1]);
    // lqv         $v16[0], 0x0($4)
    rsp.LQV<0>(rsp.vpu.r[16], r4, 0X0);
    // vmudn       $v26, $v18, $v1[0]
    rsp.VMUDN<8>(rsp.vpu.r[26], rsp.vpu.r[18], rsp.vpu.r[1]);
    // lqv         $v17[0], 0x10($4)
    rsp.LQV<0>(rsp.vpu.r[17], r4, 0X1);
    // vmudn       $v27, $v19, $v1[0]
    rsp.VMUDN<8>(rsp.vpu.r[27], rsp.vpu.r[19], rsp.vpu.r[1]);
    // lqv         $v18[0], 0x20($4)
    rsp.LQV<0>(rsp.vpu.r[18], r4, 0X2);
    // vmudn       $v28, $v20, $v1[0]
    rsp.VMUDN<8>(rsp.vpu.r[28], rsp.vpu.r[20], rsp.vpu.r[1]);
    // lqv         $v19[0], 0x30($4)
    rsp.LQV<0>(rsp.vpu.r[19], r4, 0X3);
    // vmudn       $v29, $v21, $v1[0]
    rsp.VMUDN<8>(rsp.vpu.r[29], rsp.vpu.r[21], rsp.vpu.r[1]);
    // lqv         $v20[0], 0x40($4)
    rsp.LQV<0>(rsp.vpu.r[20], r4, 0X4);
    // vmudn       $v30, $v22, $v1[0]
    rsp.VMUDN<8>(rsp.vpu.r[30], rsp.vpu.r[22], rsp.vpu.r[1]);
    // lqv         $v21[0], 0x50($4)
    rsp.LQV<0>(rsp.vpu.r[21], r4, 0X5);
    // vmudn       $v31, $v23, $v1[0]
    rsp.VMUDN<8>(rsp.vpu.r[31], rsp.vpu.r[23], rsp.vpu.r[1]);
    // lqv         $v22[0], 0x60($4)
    rsp.LQV<0>(rsp.vpu.r[22], r4, 0X6);
    // lqv         $v23[0], 0x70($4)
    rsp.LQV<0>(rsp.vpu.r[23], r4, 0X7);
    // addi        $4, $4, 0x80
    r4 = RSP_ADD32(r4, 0X80);
    // addi        $8, $8, -0x1
    r8 = RSP_ADD32(r8, -0X1);
    // bgtz        $8, L_11D0
    if (RSP_SIGNED(r8) > 0) {
        // addi        $5, $5, 0x80
        r5 = RSP_ADD32(r5, 0X80);
        goto L_11D0;
    }
    // addi        $5, $5, 0x80
    r5 = RSP_ADD32(r5, 0X80);
    // addi        $8, $zero, 0x1
    r8 = RSP_ADD32(0, 0X1);
    // bgtz        $7, L_11AC
    if (RSP_SIGNED(r7) > 0) {
        // addi        $6, $6, 0x80
        r6 = RSP_ADD32(r6, 0X80);
        goto L_11AC;
    }
    // addi        $6, $6, 0x80
    r6 = RSP_ADD32(r6, 0X80);
    // sqv         $v24[0], 0x0($5)
    rsp.SQV<0>(rsp.vpu.r[24], r5, 0X0);
    // sqv         $v25[0], 0x10($5)
    rsp.SQV<0>(rsp.vpu.r[25], r5, 0X1);
    // sqv         $v26[0], 0x20($5)
    rsp.SQV<0>(rsp.vpu.r[26], r5, 0X2);
    // sqv         $v27[0], 0x30($5)
    rsp.SQV<0>(rsp.vpu.r[27], r5, 0X3);
    // sqv         $v28[0], 0x40($5)
    rsp.SQV<0>(rsp.vpu.r[28], r5, 0X4);
    // sqv         $v29[0], 0x50($5)
    rsp.SQV<0>(rsp.vpu.r[29], r5, 0X5);
    // sqv         $v30[0], 0x60($5)
    rsp.SQV<0>(rsp.vpu.r[30], r5, 0X6);
    // sqv         $v31[0], 0x70($5)
    rsp.SQV<0>(rsp.vpu.r[31], r5, 0X7);
    // addi        $7, $9, 0x4
    r7 = RSP_ADD32(r9, 0X4);
    // addi        $4, $zero, 0x560
    r4 = RSP_ADD32(0, 0X560);
    // addi        $5, $zero, 0x860
    r5 = RSP_ADD32(0, 0X860);
L_1298:
    // addi        $7, $7, -0x1
    r7 = RSP_ADD32(r7, -0X1);
    // lqv         $v24[0], 0x0($4)
    rsp.LQV<0>(rsp.vpu.r[24], r4, 0X0);
    // lqv         $v25[0], 0x10($4)
    rsp.LQV<0>(rsp.vpu.r[25], r4, 0X1);
    // lqv         $v26[0], 0x20($4)
    rsp.LQV<0>(rsp.vpu.r[26], r4, 0X2);
    // lqv         $v27[0], 0x30($4)
    rsp.LQV<0>(rsp.vpu.r[27], r4, 0X3);
    // lqv         $v28[0], 0x40($4)
    rsp.LQV<0>(rsp.vpu.r[28], r4, 0X4);
    // lqv         $v29[0], 0x50($4)
    rsp.LQV<0>(rsp.vpu.r[29], r4, 0X5);
    // lqv         $v30[0], 0x60($4)
    rsp.LQV<0>(rsp.vpu.r[30], r4, 0X6);
    // lqv         $v31[0], 0x70($4)
    rsp.LQV<0>(rsp.vpu.r[31], r4, 0X7);
    // ssv         $v24[0], 0x0($5)
    rsp.SSV<0>(rsp.vpu.r[24], r5, 0X0);
    // ssv         $v24[2], 0x10($5)
    rsp.SSV<2>(rsp.vpu.r[24], r5, 0X8);
    // ssv         $v24[4], 0x2($5)
    rsp.SSV<4>(rsp.vpu.r[24], r5, 0X1);
    // ssv         $v24[6], 0x4($5)
    rsp.SSV<6>(rsp.vpu.r[24], r5, 0X2);
    // ssv         $v24[8], 0x12($5)
    rsp.SSV<8>(rsp.vpu.r[24], r5, 0X9);
    // ssv         $v24[10], 0x20($5)
    rsp.SSV<10>(rsp.vpu.r[24], r5, 0X10);
    // ssv         $v24[12], 0x30($5)
    rsp.SSV<12>(rsp.vpu.r[24], r5, 0X18);
    // ssv         $v24[14], 0x22($5)
    rsp.SSV<14>(rsp.vpu.r[24], r5, 0X11);
    // ssv         $v25[0], 0x14($5)
    rsp.SSV<0>(rsp.vpu.r[25], r5, 0XA);
    // ssv         $v25[2], 0x6($5)
    rsp.SSV<2>(rsp.vpu.r[25], r5, 0X3);
    // ssv         $v25[4], 0x8($5)
    rsp.SSV<4>(rsp.vpu.r[25], r5, 0X4);
    // ssv         $v25[6], 0x16($5)
    rsp.SSV<6>(rsp.vpu.r[25], r5, 0XB);
    // ssv         $v25[8], 0x24($5)
    rsp.SSV<8>(rsp.vpu.r[25], r5, 0X12);
    // ssv         $v25[10], 0x32($5)
    rsp.SSV<10>(rsp.vpu.r[25], r5, 0X19);
    // ssv         $v25[12], 0x40($5)
    rsp.SSV<12>(rsp.vpu.r[25], r5, 0X20);
    // ssv         $v25[14], 0x50($5)
    rsp.SSV<14>(rsp.vpu.r[25], r5, 0X28);
    // ssv         $v26[0], 0x42($5)
    rsp.SSV<0>(rsp.vpu.r[26], r5, 0X21);
    // ssv         $v26[2], 0x34($5)
    rsp.SSV<2>(rsp.vpu.r[26], r5, 0X1A);
    // ssv         $v26[4], 0x26($5)
    rsp.SSV<4>(rsp.vpu.r[26], r5, 0X13);
    // ssv         $v26[6], 0x18($5)
    rsp.SSV<6>(rsp.vpu.r[26], r5, 0XC);
    // ssv         $v26[8], 0xA($5)
    rsp.SSV<8>(rsp.vpu.r[26], r5, 0X5);
    // ssv         $v26[10], 0xC($5)
    rsp.SSV<10>(rsp.vpu.r[26], r5, 0X6);
    // ssv         $v26[12], 0x1A($5)
    rsp.SSV<12>(rsp.vpu.r[26], r5, 0XD);
    // ssv         $v26[14], 0x28($5)
    rsp.SSV<14>(rsp.vpu.r[26], r5, 0X14);
    // ssv         $v27[0], 0x36($5)
    rsp.SSV<0>(rsp.vpu.r[27], r5, 0X1B);
    // ssv         $v27[2], 0x44($5)
    rsp.SSV<2>(rsp.vpu.r[27], r5, 0X22);
    // ssv         $v27[4], 0x52($5)
    rsp.SSV<4>(rsp.vpu.r[27], r5, 0X29);
    // ssv         $v27[6], 0x60($5)
    rsp.SSV<6>(rsp.vpu.r[27], r5, 0X30);
    // ssv         $v27[8], 0x70($5)
    rsp.SSV<8>(rsp.vpu.r[27], r5, 0X38);
    // ssv         $v27[10], 0x62($5)
    rsp.SSV<10>(rsp.vpu.r[27], r5, 0X31);
    // ssv         $v27[12], 0x54($5)
    rsp.SSV<12>(rsp.vpu.r[27], r5, 0X2A);
    // ssv         $v27[14], 0x46($5)
    rsp.SSV<14>(rsp.vpu.r[27], r5, 0X23);
    // ssv         $v28[0], 0x38($5)
    rsp.SSV<0>(rsp.vpu.r[28], r5, 0X1C);
    // ssv         $v28[2], 0x2A($5)
    rsp.SSV<2>(rsp.vpu.r[28], r5, 0X15);
    // ssv         $v28[4], 0x1C($5)
    rsp.SSV<4>(rsp.vpu.r[28], r5, 0XE);
    // ssv         $v28[6], 0xE($5)
    rsp.SSV<6>(rsp.vpu.r[28], r5, 0X7);
    // ssv         $v28[8], 0x1E($5)
    rsp.SSV<8>(rsp.vpu.r[28], r5, 0XF);
    // ssv         $v28[10], 0x2C($5)
    rsp.SSV<10>(rsp.vpu.r[28], r5, 0X16);
    // ssv         $v28[12], 0x3A($5)
    rsp.SSV<12>(rsp.vpu.r[28], r5, 0X1D);
    // ssv         $v28[14], 0x48($5)
    rsp.SSV<14>(rsp.vpu.r[28], r5, 0X24);
    // ssv         $v29[0], 0x56($5)
    rsp.SSV<0>(rsp.vpu.r[29], r5, 0X2B);
    // ssv         $v29[2], 0x64($5)
    rsp.SSV<2>(rsp.vpu.r[29], r5, 0X32);
    // ssv         $v29[4], 0x72($5)
    rsp.SSV<4>(rsp.vpu.r[29], r5, 0X39);
    // ssv         $v29[6], 0x74($5)
    rsp.SSV<6>(rsp.vpu.r[29], r5, 0X3A);
    // ssv         $v29[8], 0x66($5)
    rsp.SSV<8>(rsp.vpu.r[29], r5, 0X33);
    // ssv         $v29[10], 0x58($5)
    rsp.SSV<10>(rsp.vpu.r[29], r5, 0X2C);
    // ssv         $v29[12], 0x4A($5)
    rsp.SSV<12>(rsp.vpu.r[29], r5, 0X25);
    // ssv         $v29[14], 0x3C($5)
    rsp.SSV<14>(rsp.vpu.r[29], r5, 0X1E);
    // ssv         $v30[0], 0x2E($5)
    rsp.SSV<0>(rsp.vpu.r[30], r5, 0X17);
    // ssv         $v30[2], 0x3E($5)
    rsp.SSV<2>(rsp.vpu.r[30], r5, 0X1F);
    // ssv         $v30[4], 0x4C($5)
    rsp.SSV<4>(rsp.vpu.r[30], r5, 0X26);
    // ssv         $v30[6], 0x5A($5)
    rsp.SSV<6>(rsp.vpu.r[30], r5, 0X2D);
    // ssv         $v30[8], 0x68($5)
    rsp.SSV<8>(rsp.vpu.r[30], r5, 0X34);
    // ssv         $v30[10], 0x76($5)
    rsp.SSV<10>(rsp.vpu.r[30], r5, 0X3B);
    // ssv         $v30[12], 0x78($5)
    rsp.SSV<12>(rsp.vpu.r[30], r5, 0X3C);
    // ssv         $v30[14], 0x6A($5)
    rsp.SSV<14>(rsp.vpu.r[30], r5, 0X35);
    // ssv         $v31[0], 0x5C($5)
    rsp.SSV<0>(rsp.vpu.r[31], r5, 0X2E);
    // ssv         $v31[2], 0x4E($5)
    rsp.SSV<2>(rsp.vpu.r[31], r5, 0X27);
    // ssv         $v31[4], 0x5E($5)
    rsp.SSV<4>(rsp.vpu.r[31], r5, 0X2F);
    // ssv         $v31[6], 0x6C($5)
    rsp.SSV<6>(rsp.vpu.r[31], r5, 0X36);
    // ssv         $v31[8], 0x7A($5)
    rsp.SSV<8>(rsp.vpu.r[31], r5, 0X3D);
    // ssv         $v31[10], 0x7C($5)
    rsp.SSV<10>(rsp.vpu.r[31], r5, 0X3E);
    // ssv         $v31[12], 0x6E($5)
    rsp.SSV<12>(rsp.vpu.r[31], r5, 0X37);
    // ssv         $v31[14], 0x7E($5)
    rsp.SSV<14>(rsp.vpu.r[31], r5, 0X3F);
    // addi        $4, $4, 0x80
    r4 = RSP_ADD32(r4, 0X80);
    // bgtz        $7, L_1298
    if (RSP_SIGNED(r7) > 0) {
        // addi        $5, $5, 0x80
        r5 = RSP_ADD32(r5, 0X80);
        goto L_1298;
    }
    // addi        $5, $5, 0x80
    r5 = RSP_ADD32(r5, 0X80);
    // jal         0x1B58
    r31 = 0x13D0;
    // add         $26, $26, $12
    r26 = RSP_ADD32(r26, r12);
    goto L_1B58;
    // add         $26, $26, $12
    r26 = RSP_ADD32(r26, r12);
L_13D0:
    // beq         $3, $zero, L_13E4
    if (r3 == 0) {
        // addi        $27, $25, 0x0
        r27 = RSP_ADD32(r25, 0X0);
        goto L_13E4;
    }
    // addi        $27, $25, 0x0
    r27 = RSP_ADD32(r25, 0X0);
    // addi        $28, $zero, 0x1E0
    r28 = RSP_ADD32(0, 0X1E0);
    // jal         0x1B08
    r31 = 0x13E4;
    // add         $29, $zero, $10
    r29 = RSP_ADD32(0, r10);
    goto L_1B08;
    // add         $29, $zero, $10
    r29 = RSP_ADD32(0, r10);
L_13E4:
    // addi        $7, $9, 0x4
    r7 = RSP_ADD32(r9, 0X4);
    // addi        $4, $zero, 0x860
    r4 = RSP_ADD32(0, 0X860);
    // addi        $5, $zero, 0x4E0
    r5 = RSP_ADD32(0, 0X4E0);
    // addi        $21, $zero, 0xDE0
    r21 = RSP_ADD32(0, 0XDE0);
    // lqv         $v19[0], 0x30($4)
    rsp.LQV<0>(rsp.vpu.r[19], r4, 0X3);
    // lqv         $v21[0], 0x50($4)
    rsp.LQV<0>(rsp.vpu.r[21], r4, 0X5);
    // lqv         $v23[0], 0x70($4)
    rsp.LQV<0>(rsp.vpu.r[23], r4, 0X7);
    // lqv         $v17[0], 0x10($4)
    rsp.LQV<0>(rsp.vpu.r[17], r4, 0X1);
    // lqv         $v16[0], 0x0($4)
    rsp.LQV<0>(rsp.vpu.r[16], r4, 0X0);
    // lqv         $v20[0], 0x40($4)
    rsp.LQV<0>(rsp.vpu.r[20], r4, 0X4);
    // lqv         $v22[0], 0x60($4)
    rsp.LQV<0>(rsp.vpu.r[22], r4, 0X6);
    // lqv         $v18[0], 0x20($4)
    rsp.LQV<0>(rsp.vpu.r[18], r4, 0X2);
    // addi        $4, $4, 0x80
    r4 = RSP_ADD32(r4, 0X80);
L_1418:
    // addi        $7, $7, -0x1
    r7 = RSP_ADD32(r7, -0X1);
    // vmulf       $v10, $v19, $v2[2]
    rsp.VMULF<10>(rsp.vpu.r[10], rsp.vpu.r[19], rsp.vpu.r[2]);
    // vmacf       $v10, $v21, $v2[4]
    rsp.VMACF<12>(rsp.vpu.r[10], rsp.vpu.r[21], rsp.vpu.r[2]);
    // vmulf       $v11, $v23, $v2[0]
    rsp.VMULF<8>(rsp.vpu.r[11], rsp.vpu.r[23], rsp.vpu.r[2]);
    // sqv         $v24[0], 0x0($5)
    rsp.SQV<0>(rsp.vpu.r[24], r5, 0X0);
    // vmacf       $v11, $v17, $v2[5]
    rsp.VMACF<13>(rsp.vpu.r[11], rsp.vpu.r[17], rsp.vpu.r[2]);
    // sqv         $v31[0], 0x70($5)
    rsp.SQV<0>(rsp.vpu.r[31], r5, 0X7);
    // vmulf       $v8, $v17, $v2[0]
    rsp.VMULF<8>(rsp.vpu.r[8], rsp.vpu.r[17], rsp.vpu.r[2]);
    // sqv         $v27[0], 0x30($5)
    rsp.SQV<0>(rsp.vpu.r[27], r5, 0X3);
    // vmacf       $v8, $v23, $v2[1]
    rsp.VMACF<9>(rsp.vpu.r[8], rsp.vpu.r[23], rsp.vpu.r[2]);
    // sqv         $v28[0], 0x40($5)
    rsp.SQV<0>(rsp.vpu.r[28], r5, 0X4);
    // vmulf       $v9, $v21, $v2[2]
    rsp.VMULF<10>(rsp.vpu.r[9], rsp.vpu.r[21], rsp.vpu.r[2]);
    // sqv         $v25[0], 0x10($5)
    rsp.SQV<0>(rsp.vpu.r[25], r5, 0X1);
    // vmacf       $v9, $v19, $v2[3]
    rsp.VMACF<11>(rsp.vpu.r[9], rsp.vpu.r[19], rsp.vpu.r[2]);
    // sqv         $v30[0], 0x60($5)
    rsp.SQV<0>(rsp.vpu.r[30], r5, 0X6);
    // vmulf       $v6, $v16, $v3[0]
    rsp.VMULF<8>(rsp.vpu.r[6], rsp.vpu.r[16], rsp.vpu.r[3]);
    // sqv         $v26[0], 0x20($5)
    rsp.SQV<0>(rsp.vpu.r[26], r5, 0X2);
    // vmacf       $v6, $v20, $v3[1]
    rsp.VMACF<9>(rsp.vpu.r[6], rsp.vpu.r[20], rsp.vpu.r[3]);
    // sqv         $v29[0], 0x50($5)
    rsp.SQV<0>(rsp.vpu.r[29], r5, 0X5);
    // vsub        $v5, $v11, $v10
    rsp.VSUB<0>(rsp.vpu.r[5], rsp.vpu.r[11], rsp.vpu.r[10]);
    // vsub        $v4, $v8, $v9
    rsp.VSUB<0>(rsp.vpu.r[4], rsp.vpu.r[8], rsp.vpu.r[9]);
    // vadd        $v12, $v8, $v9
    rsp.VADD<0>(rsp.vpu.r[12], rsp.vpu.r[8], rsp.vpu.r[9]);
    // vadd        $v15, $v11, $v10
    rsp.VADD<0>(rsp.vpu.r[15], rsp.vpu.r[11], rsp.vpu.r[10]);
    // vmulf       $v13, $v5, $v3[0]
    rsp.VMULF<8>(rsp.vpu.r[13], rsp.vpu.r[5], rsp.vpu.r[3]);
    // vmacf       $v13, $v4, $v3[1]
    rsp.VMACF<9>(rsp.vpu.r[13], rsp.vpu.r[4], rsp.vpu.r[3]);
    // vmulf       $v14, $v5, $v3[0]
    rsp.VMULF<8>(rsp.vpu.r[14], rsp.vpu.r[5], rsp.vpu.r[3]);
    // vmacf       $v14, $v4, $v3[0]
    rsp.VMACF<8>(rsp.vpu.r[14], rsp.vpu.r[4], rsp.vpu.r[3]);
    // vmulf       $v4, $v16, $v3[0]
    rsp.VMULF<8>(rsp.vpu.r[4], rsp.vpu.r[16], rsp.vpu.r[3]);
    // vmacf       $v4, $v20, $v3[0]
    rsp.VMACF<8>(rsp.vpu.r[4], rsp.vpu.r[20], rsp.vpu.r[3]);
    // vmulf       $v5, $v22, $v3[2]
    rsp.VMULF<10>(rsp.vpu.r[5], rsp.vpu.r[22], rsp.vpu.r[3]);
    // vmacf       $v5, $v18, $v3[4]
    rsp.VMACF<12>(rsp.vpu.r[5], rsp.vpu.r[18], rsp.vpu.r[3]);
    // vmulf       $v7, $v18, $v3[2]
    rsp.VMULF<10>(rsp.vpu.r[7], rsp.vpu.r[18], rsp.vpu.r[3]);
    // vmacf       $v7, $v22, $v3[3]
    rsp.VMACF<11>(rsp.vpu.r[7], rsp.vpu.r[22], rsp.vpu.r[3]);
    // nop

    // vadd        $v8, $v4, $v5
    rsp.VADD<0>(rsp.vpu.r[8], rsp.vpu.r[4], rsp.vpu.r[5]);
    // vsub        $v11, $v4, $v5
    rsp.VSUB<0>(rsp.vpu.r[11], rsp.vpu.r[4], rsp.vpu.r[5]);
    // vadd        $v9, $v6, $v7
    rsp.VADD<0>(rsp.vpu.r[9], rsp.vpu.r[6], rsp.vpu.r[7]);
    // vsub        $v10, $v6, $v7
    rsp.VSUB<0>(rsp.vpu.r[10], rsp.vpu.r[6], rsp.vpu.r[7]);
    // vadd        $v16, $v8, $v15
    rsp.VADD<0>(rsp.vpu.r[16], rsp.vpu.r[8], rsp.vpu.r[15]);
    // vsub        $v23, $v8, $v15
    rsp.VSUB<0>(rsp.vpu.r[23], rsp.vpu.r[8], rsp.vpu.r[15]);
    // vadd        $v19, $v11, $v12
    rsp.VADD<0>(rsp.vpu.r[19], rsp.vpu.r[11], rsp.vpu.r[12]);
    // vsub        $v20, $v11, $v12
    rsp.VSUB<0>(rsp.vpu.r[20], rsp.vpu.r[11], rsp.vpu.r[12]);
    // vadd        $v17, $v9, $v14
    rsp.VADD<0>(rsp.vpu.r[17], rsp.vpu.r[9], rsp.vpu.r[14]);
    // vsub        $v22, $v9, $v14
    rsp.VSUB<0>(rsp.vpu.r[22], rsp.vpu.r[9], rsp.vpu.r[14]);
    // vadd        $v18, $v10, $v13
    rsp.VADD<0>(rsp.vpu.r[18], rsp.vpu.r[10], rsp.vpu.r[13]);
    // vsub        $v21, $v10, $v13
    rsp.VSUB<0>(rsp.vpu.r[21], rsp.vpu.r[10], rsp.vpu.r[13]);
    // stv         $v16[0], 0x0($21)
    rsp.STV<0>(16, r21, 0X0);
    // stv         $v16[2], 0x10($21)
    rsp.STV<2>(16, r21, 0X1);
    // stv         $v16[4], 0x20($21)
    rsp.STV<4>(16, r21, 0X2);
    // stv         $v16[6], 0x30($21)
    rsp.STV<6>(16, r21, 0X3);
    // stv         $v16[8], 0x40($21)
    rsp.STV<8>(16, r21, 0X4);
    // stv         $v16[10], 0x50($21)
    rsp.STV<10>(16, r21, 0X5);
    // stv         $v16[12], 0x60($21)
    rsp.STV<12>(16, r21, 0X6);
    // stv         $v16[14], 0x70($21)
    rsp.STV<14>(16, r21, 0X7);
    // ltv         $v24[0], 0x0($21)
    rsp.LTV<0>(24, r21, 0X0);
    // ltv         $v24[14], 0x10($21)
    rsp.LTV<14>(24, r21, 0X1);
    // ltv         $v24[12], 0x20($21)
    rsp.LTV<12>(24, r21, 0X2);
    // ltv         $v24[10], 0x30($21)
    rsp.LTV<10>(24, r21, 0X3);
    // ltv         $v24[8], 0x40($21)
    rsp.LTV<8>(24, r21, 0X4);
    // ltv         $v24[6], 0x50($21)
    rsp.LTV<6>(24, r21, 0X5);
    // ltv         $v24[4], 0x60($21)
    rsp.LTV<4>(24, r21, 0X6);
    // ltv         $v24[2], 0x70($21)
    rsp.LTV<2>(24, r21, 0X7);
    // vmulf       $v10, $v27, $v2[2]
    rsp.VMULF<10>(rsp.vpu.r[10], rsp.vpu.r[27], rsp.vpu.r[2]);
    // vmacf       $v10, $v29, $v2[4]
    rsp.VMACF<12>(rsp.vpu.r[10], rsp.vpu.r[29], rsp.vpu.r[2]);
    // vmulf       $v11, $v31, $v2[0]
    rsp.VMULF<8>(rsp.vpu.r[11], rsp.vpu.r[31], rsp.vpu.r[2]);
    // vmacf       $v11, $v25, $v2[5]
    rsp.VMACF<13>(rsp.vpu.r[11], rsp.vpu.r[25], rsp.vpu.r[2]);
    // vmulf       $v8, $v25, $v2[0]
    rsp.VMULF<8>(rsp.vpu.r[8], rsp.vpu.r[25], rsp.vpu.r[2]);
    // vmacf       $v8, $v31, $v2[1]
    rsp.VMACF<9>(rsp.vpu.r[8], rsp.vpu.r[31], rsp.vpu.r[2]);
    // vmulf       $v9, $v29, $v2[2]
    rsp.VMULF<10>(rsp.vpu.r[9], rsp.vpu.r[29], rsp.vpu.r[2]);
    // vmacf       $v9, $v27, $v2[3]
    rsp.VMACF<11>(rsp.vpu.r[9], rsp.vpu.r[27], rsp.vpu.r[2]);
    // vmulf       $v6, $v24, $v3[0]
    rsp.VMULF<8>(rsp.vpu.r[6], rsp.vpu.r[24], rsp.vpu.r[3]);
    // vmacf       $v6, $v28, $v3[1]
    rsp.VMACF<9>(rsp.vpu.r[6], rsp.vpu.r[28], rsp.vpu.r[3]);
    // vsub        $v5, $v11, $v10
    rsp.VSUB<0>(rsp.vpu.r[5], rsp.vpu.r[11], rsp.vpu.r[10]);
    // vsub        $v4, $v8, $v9
    rsp.VSUB<0>(rsp.vpu.r[4], rsp.vpu.r[8], rsp.vpu.r[9]);
    // vadd        $v12, $v8, $v9
    rsp.VADD<0>(rsp.vpu.r[12], rsp.vpu.r[8], rsp.vpu.r[9]);
    // vadd        $v15, $v11, $v10
    rsp.VADD<0>(rsp.vpu.r[15], rsp.vpu.r[11], rsp.vpu.r[10]);
    // vmulf       $v13, $v5, $v3[0]
    rsp.VMULF<8>(rsp.vpu.r[13], rsp.vpu.r[5], rsp.vpu.r[3]);
    // vmacf       $v13, $v4, $v3[1]
    rsp.VMACF<9>(rsp.vpu.r[13], rsp.vpu.r[4], rsp.vpu.r[3]);
    // vmulf       $v14, $v5, $v3[0]
    rsp.VMULF<8>(rsp.vpu.r[14], rsp.vpu.r[5], rsp.vpu.r[3]);
    // lqv         $v19[0], 0x30($4)
    rsp.LQV<0>(rsp.vpu.r[19], r4, 0X3);
    // vmacf       $v14, $v4, $v3[0]
    rsp.VMACF<8>(rsp.vpu.r[14], rsp.vpu.r[4], rsp.vpu.r[3]);
    // lqv         $v21[0], 0x50($4)
    rsp.LQV<0>(rsp.vpu.r[21], r4, 0X5);
    // vmulf       $v4, $v24, $v3[0]
    rsp.VMULF<8>(rsp.vpu.r[4], rsp.vpu.r[24], rsp.vpu.r[3]);
    // lqv         $v23[0], 0x70($4)
    rsp.LQV<0>(rsp.vpu.r[23], r4, 0X7);
    // vmacf       $v4, $v28, $v3[0]
    rsp.VMACF<8>(rsp.vpu.r[4], rsp.vpu.r[28], rsp.vpu.r[3]);
    // lqv         $v17[0], 0x10($4)
    rsp.LQV<0>(rsp.vpu.r[17], r4, 0X1);
    // vmulf       $v5, $v30, $v3[2]
    rsp.VMULF<10>(rsp.vpu.r[5], rsp.vpu.r[30], rsp.vpu.r[3]);
    // lqv         $v16[0], 0x0($4)
    rsp.LQV<0>(rsp.vpu.r[16], r4, 0X0);
    // vmacf       $v5, $v26, $v3[4]
    rsp.VMACF<12>(rsp.vpu.r[5], rsp.vpu.r[26], rsp.vpu.r[3]);
    // lqv         $v20[0], 0x40($4)
    rsp.LQV<0>(rsp.vpu.r[20], r4, 0X4);
    // vmulf       $v7, $v26, $v3[2]
    rsp.VMULF<10>(rsp.vpu.r[7], rsp.vpu.r[26], rsp.vpu.r[3]);
    // lqv         $v22[0], 0x60($4)
    rsp.LQV<0>(rsp.vpu.r[22], r4, 0X6);
    // vmacf       $v7, $v30, $v3[3]
    rsp.VMACF<11>(rsp.vpu.r[7], rsp.vpu.r[30], rsp.vpu.r[3]);
    // lqv         $v18[0], 0x20($4)
    rsp.LQV<0>(rsp.vpu.r[18], r4, 0X2);
    // nop

    // vadd        $v8, $v4, $v5
    rsp.VADD<0>(rsp.vpu.r[8], rsp.vpu.r[4], rsp.vpu.r[5]);
    // vsub        $v11, $v4, $v5
    rsp.VSUB<0>(rsp.vpu.r[11], rsp.vpu.r[4], rsp.vpu.r[5]);
    // vadd        $v9, $v6, $v7
    rsp.VADD<0>(rsp.vpu.r[9], rsp.vpu.r[6], rsp.vpu.r[7]);
    // vsub        $v10, $v6, $v7
    rsp.VSUB<0>(rsp.vpu.r[10], rsp.vpu.r[6], rsp.vpu.r[7]);
    // vmulf       $v24, $v8, $v1[1]
    rsp.VMULF<9>(rsp.vpu.r[24], rsp.vpu.r[8], rsp.vpu.r[1]);
    // vmacf       $v24, $v15, $v1[1]
    rsp.VMACF<9>(rsp.vpu.r[24], rsp.vpu.r[15], rsp.vpu.r[1]);
    // vmacf       $v31, $v15, $v1[2]
    rsp.VMACF<10>(rsp.vpu.r[31], rsp.vpu.r[15], rsp.vpu.r[1]);
    // vmulf       $v27, $v11, $v1[1]
    rsp.VMULF<9>(rsp.vpu.r[27], rsp.vpu.r[11], rsp.vpu.r[1]);
    // vmacf       $v27, $v12, $v1[1]
    rsp.VMACF<9>(rsp.vpu.r[27], rsp.vpu.r[12], rsp.vpu.r[1]);
    // vmacf       $v28, $v12, $v1[2]
    rsp.VMACF<10>(rsp.vpu.r[28], rsp.vpu.r[12], rsp.vpu.r[1]);
    // vmulf       $v25, $v9, $v1[1]
    rsp.VMULF<9>(rsp.vpu.r[25], rsp.vpu.r[9], rsp.vpu.r[1]);
    // vmacf       $v25, $v14, $v1[1]
    rsp.VMACF<9>(rsp.vpu.r[25], rsp.vpu.r[14], rsp.vpu.r[1]);
    // vmacf       $v30, $v14, $v1[2]
    rsp.VMACF<10>(rsp.vpu.r[30], rsp.vpu.r[14], rsp.vpu.r[1]);
    // vmulf       $v26, $v10, $v1[1]
    rsp.VMULF<9>(rsp.vpu.r[26], rsp.vpu.r[10], rsp.vpu.r[1]);
    // vmacf       $v26, $v13, $v1[1]
    rsp.VMACF<9>(rsp.vpu.r[26], rsp.vpu.r[13], rsp.vpu.r[1]);
    // vmacf       $v29, $v13, $v1[2]
    rsp.VMACF<10>(rsp.vpu.r[29], rsp.vpu.r[13], rsp.vpu.r[1]);
    // addi        $4, $4, 0x80
    r4 = RSP_ADD32(r4, 0X80);
    // bgtz        $7, L_1418
    if (RSP_SIGNED(r7) > 0) {
        // addi        $5, $5, 0x80
        r5 = RSP_ADD32(r5, 0X80);
        goto L_1418;
    }
    // addi        $5, $5, 0x80
    r5 = RSP_ADD32(r5, 0X80);
    // sqv         $v24[0], 0x0($5)
    rsp.SQV<0>(rsp.vpu.r[24], r5, 0X0);
    // sqv         $v31[0], 0x70($5)
    rsp.SQV<0>(rsp.vpu.r[31], r5, 0X7);
    // sqv         $v27[0], 0x30($5)
    rsp.SQV<0>(rsp.vpu.r[27], r5, 0X3);
    // sqv         $v28[0], 0x40($5)
    rsp.SQV<0>(rsp.vpu.r[28], r5, 0X4);
    // sqv         $v25[0], 0x10($5)
    rsp.SQV<0>(rsp.vpu.r[25], r5, 0X1);
    // sqv         $v30[0], 0x60($5)
    rsp.SQV<0>(rsp.vpu.r[30], r5, 0X6);
    // sqv         $v26[0], 0x20($5)
    rsp.SQV<0>(rsp.vpu.r[26], r5, 0X2);
    // sqv         $v29[0], 0x50($5)
    rsp.SQV<0>(rsp.vpu.r[29], r5, 0X5);
    // bgtz        $9, L_17E4
    if (RSP_SIGNED(r9) > 0) {
        // nop
    
        goto L_17E4;
    }
    // nop

    // addi        $4, $zero, 0x560
    r4 = RSP_ADD32(0, 0X560);
    // vxor        $v19, $v19, $v19
    rsp.VXOR<0>(rsp.vpu.r[19], rsp.vpu.r[19], rsp.vpu.r[19]);
    // addi        $20, $4, 0x100
    r20 = RSP_ADD32(r4, 0X100);
    // vxor        $v20, $v20, $v20
    rsp.VXOR<0>(rsp.vpu.r[20], rsp.vpu.r[20], rsp.vpu.r[20]);
    // addi        $5, $zero, 0xBC0
    r5 = RSP_ADD32(0, 0XBC0);
    // vxor        $v21, $v21, $v21
    rsp.VXOR<0>(rsp.vpu.r[21], rsp.vpu.r[21], rsp.vpu.r[21]);
    // lqv         $v7[0], 0x0($20)
    rsp.LQV<0>(rsp.vpu.r[7], r20, 0X0);
    // vxor        $v22, $v22, $v22
    rsp.VXOR<0>(rsp.vpu.r[22], rsp.vpu.r[22], rsp.vpu.r[22]);
    // llv         $v19[0], 0xC($zero)
    rsp.LLV<0>(rsp.vpu.r[19], 0, 0X3);
    // llv         $v20[4], 0xC($zero)
    rsp.LLV<4>(rsp.vpu.r[20], 0, 0X3);
    // llv         $v21[8], 0xC($zero)
    rsp.LLV<8>(rsp.vpu.r[21], 0, 0X3);
    // llv         $v22[12], 0xC($zero)
    rsp.LLV<12>(rsp.vpu.r[22], 0, 0X3);
    // addi        $7, $zero, 0x8
    r7 = RSP_ADD32(0, 0X8);
    // vmudh       $v24, $v19, $v7[0]
    rsp.VMUDH<8>(rsp.vpu.r[24], rsp.vpu.r[19], rsp.vpu.r[7]);
    // lqv         $v8[0], 0x80($20)
    rsp.LQV<0>(rsp.vpu.r[8], r20, 0X8);
    // vmadh       $v24, $v20, $v7[1]
    rsp.VMADH<9>(rsp.vpu.r[24], rsp.vpu.r[20], rsp.vpu.r[7]);
    // lqv         $v1[0], 0x0($zero)
    rsp.LQV<0>(rsp.vpu.r[1], 0, 0X0);
    // vmadh       $v24, $v21, $v7[2]
    rsp.VMADH<10>(rsp.vpu.r[24], rsp.vpu.r[21], rsp.vpu.r[7]);
    // lqv         $v4[0], 0x10($zero)
    rsp.LQV<0>(rsp.vpu.r[4], 0, 0X1);
    // vmadh       $v24, $v22, $v7[3]
    rsp.VMADH<11>(rsp.vpu.r[24], rsp.vpu.r[22], rsp.vpu.r[7]);
    // vmudh       $v23, $v19, $v7[4]
    rsp.VMUDH<12>(rsp.vpu.r[23], rsp.vpu.r[19], rsp.vpu.r[7]);
    // vmadh       $v23, $v20, $v7[5]
    rsp.VMADH<13>(rsp.vpu.r[23], rsp.vpu.r[20], rsp.vpu.r[7]);
    // vmadh       $v23, $v21, $v7[6]
    rsp.VMADH<14>(rsp.vpu.r[23], rsp.vpu.r[21], rsp.vpu.r[7]);
    // vmadh       $v23, $v22, $v7[7]
    rsp.VMADH<15>(rsp.vpu.r[23], rsp.vpu.r[22], rsp.vpu.r[7]);
    // vmudh       $v26, $v19, $v8[0]
    rsp.VMUDH<8>(rsp.vpu.r[26], rsp.vpu.r[19], rsp.vpu.r[8]);
    // lqv         $v6[0], 0x0($4)
    rsp.LQV<0>(rsp.vpu.r[6], r4, 0X0);
    // vmadh       $v26, $v20, $v8[1]
    rsp.VMADH<9>(rsp.vpu.r[26], rsp.vpu.r[20], rsp.vpu.r[8]);
    // lqv         $v5[0], 0x80($4)
    rsp.LQV<0>(rsp.vpu.r[5], r4, 0X8);
    // vmadh       $v26, $v21, $v8[2]
    rsp.VMADH<10>(rsp.vpu.r[26], rsp.vpu.r[21], rsp.vpu.r[8]);
    // vmadh       $v26, $v22, $v8[3]
    rsp.VMADH<11>(rsp.vpu.r[26], rsp.vpu.r[22], rsp.vpu.r[8]);
    // vmudh       $v25, $v19, $v8[4]
    rsp.VMUDH<12>(rsp.vpu.r[25], rsp.vpu.r[19], rsp.vpu.r[8]);
    // vmadh       $v25, $v20, $v8[5]
    rsp.VMADH<13>(rsp.vpu.r[25], rsp.vpu.r[20], rsp.vpu.r[8]);
    // vmadh       $v25, $v21, $v8[6]
    rsp.VMADH<14>(rsp.vpu.r[25], rsp.vpu.r[21], rsp.vpu.r[8]);
    // addi        $4, $4, 0x10
    r4 = RSP_ADD32(r4, 0X10);
    // vmadh       $v25, $v22, $v8[7]
    rsp.VMADH<15>(rsp.vpu.r[25], rsp.vpu.r[22], rsp.vpu.r[8]);
    // addi        $20, $20, 0x10
    r20 = RSP_ADD32(r20, 0X10);
L_1698:
    // vadd        $v10, $v6, $v4[7]
    rsp.VADD<15>(rsp.vpu.r[10], rsp.vpu.r[6], rsp.vpu.r[4]);
    // sqv         $v28[0], 0x0($5)
    rsp.SQV<0>(rsp.vpu.r[28], r5, 0X0);
    // vadd        $v9, $v5, $v4[7]
    rsp.VADD<15>(rsp.vpu.r[9], rsp.vpu.r[5], rsp.vpu.r[4]);
    // sqv         $v27[0], 0x10($5)
    rsp.SQV<0>(rsp.vpu.r[27], r5, 0X1);
    // vmudm       $v15, $v26, $v4[0]
    rsp.VMUDM<8>(rsp.vpu.r[15], rsp.vpu.r[26], rsp.vpu.r[4]);
    // addi        $5, $5, 0x20
    r5 = RSP_ADD32(r5, 0X20);
    // vmudm       $v16, $v25, $v4[0]
    rsp.VMUDM<8>(rsp.vpu.r[16], rsp.vpu.r[25], rsp.vpu.r[4]);
    // vmudm       $v17, $v24, $v4[1]
    rsp.VMUDM<9>(rsp.vpu.r[17], rsp.vpu.r[24], rsp.vpu.r[4]);
    // vmudm       $v18, $v23, $v4[1]
    rsp.VMUDM<9>(rsp.vpu.r[18], rsp.vpu.r[23], rsp.vpu.r[4]);
    // vmudm       $v11, $v26, $v4[2]
    rsp.VMUDM<10>(rsp.vpu.r[11], rsp.vpu.r[26], rsp.vpu.r[4]);
    // vmudm       $v12, $v25, $v4[2]
    rsp.VMUDM<10>(rsp.vpu.r[12], rsp.vpu.r[25], rsp.vpu.r[4]);
    // vmudm       $v13, $v24, $v4[3]
    rsp.VMUDM<11>(rsp.vpu.r[13], rsp.vpu.r[24], rsp.vpu.r[4]);
    // vmudm       $v14, $v23, $v4[3]
    rsp.VMUDM<11>(rsp.vpu.r[14], rsp.vpu.r[23], rsp.vpu.r[4]);
    // vadd        $v15, $v15, $v26
    rsp.VADD<0>(rsp.vpu.r[15], rsp.vpu.r[15], rsp.vpu.r[26]);
    // vadd        $v16, $v16, $v25
    rsp.VADD<0>(rsp.vpu.r[16], rsp.vpu.r[16], rsp.vpu.r[25]);
    // vadd        $v17, $v17, $v11
    rsp.VADD<0>(rsp.vpu.r[17], rsp.vpu.r[17], rsp.vpu.r[11]);
    // vadd        $v18, $v18, $v12
    rsp.VADD<0>(rsp.vpu.r[18], rsp.vpu.r[18], rsp.vpu.r[12]);
    // vadd        $v13, $v13, $v24
    rsp.VADD<0>(rsp.vpu.r[13], rsp.vpu.r[13], rsp.vpu.r[24]);
    // vadd        $v14, $v14, $v23
    rsp.VADD<0>(rsp.vpu.r[14], rsp.vpu.r[14], rsp.vpu.r[23]);
    // vadd        $v15, $v15, $v10
    rsp.VADD<0>(rsp.vpu.r[15], rsp.vpu.r[15], rsp.vpu.r[10]);
    // vadd        $v16, $v16, $v9
    rsp.VADD<0>(rsp.vpu.r[16], rsp.vpu.r[16], rsp.vpu.r[9]);
    // vsub        $v17, $v10, $v17
    rsp.VSUB<0>(rsp.vpu.r[17], rsp.vpu.r[10], rsp.vpu.r[17]);
    // vsub        $v18, $v9, $v18
    rsp.VSUB<0>(rsp.vpu.r[18], rsp.vpu.r[9], rsp.vpu.r[18]);
    // vadd        $v13, $v13, $v10
    rsp.VADD<0>(rsp.vpu.r[13], rsp.vpu.r[13], rsp.vpu.r[10]);
    // vadd        $v14, $v14, $v9
    rsp.VADD<0>(rsp.vpu.r[14], rsp.vpu.r[14], rsp.vpu.r[9]);
    // vge         $v15, $v15, $v0
    rsp.VGE<0>(rsp.vpu.r[15], rsp.vpu.r[15], rsp.vpu.r[0]);
    // lqv         $v6[0], 0x0($4)
    rsp.LQV<0>(rsp.vpu.r[6], r4, 0X0);
    // vge         $v16, $v16, $v0
    rsp.VGE<0>(rsp.vpu.r[16], rsp.vpu.r[16], rsp.vpu.r[0]);
    // lqv         $v5[0], 0x80($4)
    rsp.LQV<0>(rsp.vpu.r[5], r4, 0X8);
    // vge         $v17, $v17, $v0
    rsp.VGE<0>(rsp.vpu.r[17], rsp.vpu.r[17], rsp.vpu.r[0]);
    // lqv         $v7[0], 0x0($20)
    rsp.LQV<0>(rsp.vpu.r[7], r20, 0X0);
    // vge         $v18, $v18, $v0
    rsp.VGE<0>(rsp.vpu.r[18], rsp.vpu.r[18], rsp.vpu.r[0]);
    // lqv         $v8[0], 0x80($20)
    rsp.LQV<0>(rsp.vpu.r[8], r20, 0X8);
    // vge         $v13, $v13, $v0
    rsp.VGE<0>(rsp.vpu.r[13], rsp.vpu.r[13], rsp.vpu.r[0]);
    // vge         $v14, $v14, $v0
    rsp.VGE<0>(rsp.vpu.r[14], rsp.vpu.r[14], rsp.vpu.r[0]);
    // vlt         $v15, $v15, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[15], rsp.vpu.r[15], rsp.vpu.r[4]);
    // vlt         $v16, $v16, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[16], rsp.vpu.r[16], rsp.vpu.r[4]);
    // vlt         $v17, $v17, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[17], rsp.vpu.r[17], rsp.vpu.r[4]);
    // vlt         $v18, $v18, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[18], rsp.vpu.r[18], rsp.vpu.r[4]);
    // vlt         $v13, $v13, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[13], rsp.vpu.r[13], rsp.vpu.r[4]);
    // vlt         $v14, $v14, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[14], rsp.vpu.r[14], rsp.vpu.r[4]);
    // vmudm       $v15, $v15, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[15], rsp.vpu.r[15], rsp.vpu.r[4]);
    // vmudm       $v16, $v16, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[16], rsp.vpu.r[16], rsp.vpu.r[4]);
    // vmudm       $v17, $v17, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[17], rsp.vpu.r[17], rsp.vpu.r[4]);
    // vmudm       $v18, $v18, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[18], rsp.vpu.r[18], rsp.vpu.r[4]);
    // vmudm       $v13, $v13, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[13], rsp.vpu.r[13], rsp.vpu.r[4]);
    // vmudm       $v14, $v14, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[14], rsp.vpu.r[14], rsp.vpu.r[4]);
    // vmudn       $v15, $v15, $v1[3]
    rsp.VMUDN<11>(rsp.vpu.r[15], rsp.vpu.r[15], rsp.vpu.r[1]);
    // vmudn       $v16, $v16, $v1[3]
    rsp.VMUDN<11>(rsp.vpu.r[16], rsp.vpu.r[16], rsp.vpu.r[1]);
    // vmudh       $v17, $v17, $v1[4]
    rsp.VMUDH<12>(rsp.vpu.r[17], rsp.vpu.r[17], rsp.vpu.r[1]);
    // vmudh       $v18, $v18, $v1[4]
    rsp.VMUDH<12>(rsp.vpu.r[18], rsp.vpu.r[18], rsp.vpu.r[1]);
    // vmudh       $v13, $v13, $v1[5]
    rsp.VMUDH<13>(rsp.vpu.r[13], rsp.vpu.r[13], rsp.vpu.r[1]);
    // vmudh       $v14, $v14, $v1[5]
    rsp.VMUDH<13>(rsp.vpu.r[14], rsp.vpu.r[14], rsp.vpu.r[1]);
    // vmudh       $v24, $v19, $v7[0]
    rsp.VMUDH<8>(rsp.vpu.r[24], rsp.vpu.r[19], rsp.vpu.r[7]);
    // vmadh       $v24, $v20, $v7[1]
    rsp.VMADH<9>(rsp.vpu.r[24], rsp.vpu.r[20], rsp.vpu.r[7]);
    // vmadh       $v24, $v21, $v7[2]
    rsp.VMADH<10>(rsp.vpu.r[24], rsp.vpu.r[21], rsp.vpu.r[7]);
    // vmadh       $v24, $v22, $v7[3]
    rsp.VMADH<11>(rsp.vpu.r[24], rsp.vpu.r[22], rsp.vpu.r[7]);
    // vor         $v15, $v15, $v17
    rsp.VOR<0>(rsp.vpu.r[15], rsp.vpu.r[15], rsp.vpu.r[17]);
    // vor         $v16, $v16, $v18
    rsp.VOR<0>(rsp.vpu.r[16], rsp.vpu.r[16], rsp.vpu.r[18]);
    // vmudh       $v23, $v19, $v7[4]
    rsp.VMUDH<12>(rsp.vpu.r[23], rsp.vpu.r[19], rsp.vpu.r[7]);
    // vmadh       $v23, $v20, $v7[5]
    rsp.VMADH<13>(rsp.vpu.r[23], rsp.vpu.r[20], rsp.vpu.r[7]);
    // vmadh       $v23, $v21, $v7[6]
    rsp.VMADH<14>(rsp.vpu.r[23], rsp.vpu.r[21], rsp.vpu.r[7]);
    // vmadh       $v23, $v22, $v7[7]
    rsp.VMADH<15>(rsp.vpu.r[23], rsp.vpu.r[22], rsp.vpu.r[7]);
    // vor         $v28, $v15, $v13
    rsp.VOR<0>(rsp.vpu.r[28], rsp.vpu.r[15], rsp.vpu.r[13]);
    // vor         $v27, $v16, $v14
    rsp.VOR<0>(rsp.vpu.r[27], rsp.vpu.r[16], rsp.vpu.r[14]);
    // vmudh       $v26, $v19, $v8[0]
    rsp.VMUDH<8>(rsp.vpu.r[26], rsp.vpu.r[19], rsp.vpu.r[8]);
    // addi        $7, $7, -0x1
    r7 = RSP_ADD32(r7, -0X1);
    // vmadh       $v26, $v20, $v8[1]
    rsp.VMADH<9>(rsp.vpu.r[26], rsp.vpu.r[20], rsp.vpu.r[8]);
    // addi        $4, $4, 0x10
    r4 = RSP_ADD32(r4, 0X10);
    // vmadh       $v26, $v21, $v8[2]
    rsp.VMADH<10>(rsp.vpu.r[26], rsp.vpu.r[21], rsp.vpu.r[8]);
    // addi        $20, $20, 0x10
    r20 = RSP_ADD32(r20, 0X10);
    // vmadh       $v26, $v22, $v8[3]
    rsp.VMADH<11>(rsp.vpu.r[26], rsp.vpu.r[22], rsp.vpu.r[8]);
    // vor         $v28, $v28, $v1[6]
    rsp.VOR<14>(rsp.vpu.r[28], rsp.vpu.r[28], rsp.vpu.r[1]);
    // vor         $v27, $v27, $v1[6]
    rsp.VOR<14>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[1]);
    // vmudh       $v25, $v19, $v8[4]
    rsp.VMUDH<12>(rsp.vpu.r[25], rsp.vpu.r[19], rsp.vpu.r[8]);
    // vmadh       $v25, $v20, $v8[5]
    rsp.VMADH<13>(rsp.vpu.r[25], rsp.vpu.r[20], rsp.vpu.r[8]);
    // vmadh       $v25, $v21, $v8[6]
    rsp.VMADH<14>(rsp.vpu.r[25], rsp.vpu.r[21], rsp.vpu.r[8]);
    // bgtz        $7, L_1698
    if (RSP_SIGNED(r7) > 0) {
        // vmadh       $v25, $v22, $v8[7]
        rsp.VMADH<15>(rsp.vpu.r[25], rsp.vpu.r[22], rsp.vpu.r[8]);
        goto L_1698;
    }
    // vmadh       $v25, $v22, $v8[7]
    rsp.VMADH<15>(rsp.vpu.r[25], rsp.vpu.r[22], rsp.vpu.r[8]);
    // sqv         $v28[0], 0x0($5)
    rsp.SQV<0>(rsp.vpu.r[28], r5, 0X0);
    // sqv         $v27[0], 0x10($5)
    rsp.SQV<0>(rsp.vpu.r[27], r5, 0X1);
    // j           L_1A80
    // nop

    goto L_1A80;
    // nop

L_17E4:
    // addi        $4, $zero, 0x560
    r4 = RSP_ADD32(0, 0X560);
    // vxor        $v9, $v9, $v9
    rsp.VXOR<0>(rsp.vpu.r[9], rsp.vpu.r[9], rsp.vpu.r[9]);
    // addi        $20, $4, 0x200
    r20 = RSP_ADD32(r4, 0X200);
    // vxor        $v10, $v10, $v10
    rsp.VXOR<0>(rsp.vpu.r[10], rsp.vpu.r[10], rsp.vpu.r[10]);
    // addi        $5, $zero, 0xBC0
    r5 = RSP_ADD32(0, 0XBC0);
    // vxor        $v11, $v11, $v11
    rsp.VXOR<0>(rsp.vpu.r[11], rsp.vpu.r[11], rsp.vpu.r[11]);
    // addi        $7, $zero, 0x2
    r7 = RSP_ADD32(0, 0X2);
    // vxor        $v12, $v12, $v12
    rsp.VXOR<0>(rsp.vpu.r[12], rsp.vpu.r[12], rsp.vpu.r[12]);
    // addi        $8, $zero, 0x4
    r8 = RSP_ADD32(0, 0X4);
    // lqv         $v8[0], 0x80($20)
    rsp.LQV<0>(rsp.vpu.r[8], r20, 0X8);
    // lqv         $v7[0], 0x0($20)
    rsp.LQV<0>(rsp.vpu.r[7], r20, 0X0);
    // llv         $v9[0], 0xC($zero)
    rsp.LLV<0>(rsp.vpu.r[9], 0, 0X3);
    // llv         $v10[4], 0xC($zero)
    rsp.LLV<4>(rsp.vpu.r[10], 0, 0X3);
    // llv         $v11[8], 0xC($zero)
    rsp.LLV<8>(rsp.vpu.r[11], 0, 0X3);
    // llv         $v12[12], 0xC($zero)
    rsp.LLV<12>(rsp.vpu.r[12], 0, 0X3);
    // lqv         $v1[0], 0x0($zero)
    rsp.LQV<0>(rsp.vpu.r[1], 0, 0X0);
    // lqv         $v4[0], 0x10($zero)
    rsp.LQV<0>(rsp.vpu.r[4], 0, 0X1);
    // addi        $20, $20, 0x10
    r20 = RSP_ADD32(r20, 0X10);
L_182C:
    // vmudh       $v16, $v9, $v8[0]
    rsp.VMUDH<8>(rsp.vpu.r[16], rsp.vpu.r[9], rsp.vpu.r[8]);
    // addi        $8, $8, -0x1
    r8 = RSP_ADD32(r8, -0X1);
    // vmadh       $v16, $v10, $v8[1]
    rsp.VMADH<9>(rsp.vpu.r[16], rsp.vpu.r[10], rsp.vpu.r[8]);
    // lqv         $v6[0], 0x0($4)
    rsp.LQV<0>(rsp.vpu.r[6], r4, 0X0);
    // vmadh       $v16, $v11, $v8[2]
    rsp.VMADH<10>(rsp.vpu.r[16], rsp.vpu.r[11], rsp.vpu.r[8]);
    // lqv         $v5[0], 0x80($4)
    rsp.LQV<0>(rsp.vpu.r[5], r4, 0X8);
    // vmadh       $v16, $v12, $v8[3]
    rsp.VMADH<11>(rsp.vpu.r[16], rsp.vpu.r[12], rsp.vpu.r[8]);
    // addi        $4, $4, 0x10
    r4 = RSP_ADD32(r4, 0X10);
    // vor         $v24, $v24, $v26
    rsp.VOR<0>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[26]);
    // vor         $v23, $v23, $v25
    rsp.VOR<0>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[25]);
    // vmudh       $v15, $v9, $v8[4]
    rsp.VMUDH<12>(rsp.vpu.r[15], rsp.vpu.r[9], rsp.vpu.r[8]);
    // vmadh       $v15, $v10, $v8[5]
    rsp.VMADH<13>(rsp.vpu.r[15], rsp.vpu.r[10], rsp.vpu.r[8]);
    // vmadh       $v15, $v11, $v8[6]
    rsp.VMADH<14>(rsp.vpu.r[15], rsp.vpu.r[11], rsp.vpu.r[8]);
    // vmadh       $v15, $v12, $v8[7]
    rsp.VMADH<15>(rsp.vpu.r[15], rsp.vpu.r[12], rsp.vpu.r[8]);
    // vadd        $v18, $v6, $v4[7]
    rsp.VADD<15>(rsp.vpu.r[18], rsp.vpu.r[6], rsp.vpu.r[4]);
    // vadd        $v17, $v5, $v4[7]
    rsp.VADD<15>(rsp.vpu.r[17], rsp.vpu.r[5], rsp.vpu.r[4]);
    // vmudh       $v14, $v9, $v7[0]
    rsp.VMUDH<8>(rsp.vpu.r[14], rsp.vpu.r[9], rsp.vpu.r[7]);
    // vmadh       $v14, $v10, $v7[1]
    rsp.VMADH<9>(rsp.vpu.r[14], rsp.vpu.r[10], rsp.vpu.r[7]);
    // vmadh       $v14, $v11, $v7[2]
    rsp.VMADH<10>(rsp.vpu.r[14], rsp.vpu.r[11], rsp.vpu.r[7]);
    // vmadh       $v14, $v12, $v7[3]
    rsp.VMADH<11>(rsp.vpu.r[14], rsp.vpu.r[12], rsp.vpu.r[7]);
    // vor         $v30, $v24, $v28
    rsp.VOR<0>(rsp.vpu.r[30], rsp.vpu.r[24], rsp.vpu.r[28]);
    // vor         $v29, $v23, $v27
    rsp.VOR<0>(rsp.vpu.r[29], rsp.vpu.r[23], rsp.vpu.r[27]);
    // vmudh       $v13, $v9, $v7[4]
    rsp.VMUDH<12>(rsp.vpu.r[13], rsp.vpu.r[9], rsp.vpu.r[7]);
    // vmadh       $v13, $v10, $v7[5]
    rsp.VMADH<13>(rsp.vpu.r[13], rsp.vpu.r[10], rsp.vpu.r[7]);
    // vmadh       $v13, $v11, $v7[6]
    rsp.VMADH<14>(rsp.vpu.r[13], rsp.vpu.r[11], rsp.vpu.r[7]);
    // vmadh       $v13, $v12, $v7[7]
    rsp.VMADH<15>(rsp.vpu.r[13], rsp.vpu.r[12], rsp.vpu.r[7]);
    // vor         $v30, $v30, $v1[6]
    rsp.VOR<14>(rsp.vpu.r[30], rsp.vpu.r[30], rsp.vpu.r[1]);
    // vor         $v29, $v29, $v1[6]
    rsp.VOR<14>(rsp.vpu.r[29], rsp.vpu.r[29], rsp.vpu.r[1]);
    // vmudm       $v24, $v16, $v4[0]
    rsp.VMUDM<8>(rsp.vpu.r[24], rsp.vpu.r[16], rsp.vpu.r[4]);
    // vmudm       $v23, $v15, $v4[0]
    rsp.VMUDM<8>(rsp.vpu.r[23], rsp.vpu.r[15], rsp.vpu.r[4]);
    // vmudm       $v26, $v14, $v4[1]
    rsp.VMUDM<9>(rsp.vpu.r[26], rsp.vpu.r[14], rsp.vpu.r[4]);
    // vmudm       $v25, $v13, $v4[1]
    rsp.VMUDM<9>(rsp.vpu.r[25], rsp.vpu.r[13], rsp.vpu.r[4]);
    // vmudm       $v21, $v16, $v4[2]
    rsp.VMUDM<10>(rsp.vpu.r[21], rsp.vpu.r[16], rsp.vpu.r[4]);
    // vmudm       $v22, $v15, $v4[2]
    rsp.VMUDM<10>(rsp.vpu.r[22], rsp.vpu.r[15], rsp.vpu.r[4]);
    // vmudm       $v28, $v14, $v4[3]
    rsp.VMUDM<11>(rsp.vpu.r[28], rsp.vpu.r[14], rsp.vpu.r[4]);
    // vmudm       $v27, $v13, $v4[3]
    rsp.VMUDM<11>(rsp.vpu.r[27], rsp.vpu.r[13], rsp.vpu.r[4]);
    // vadd        $v24, $v24, $v16
    rsp.VADD<0>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[16]);
    // vadd        $v23, $v23, $v15
    rsp.VADD<0>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[15]);
    // vadd        $v26, $v26, $v21
    rsp.VADD<0>(rsp.vpu.r[26], rsp.vpu.r[26], rsp.vpu.r[21]);
    // sqv         $v30[0], 0x0($5)
    rsp.SQV<0>(rsp.vpu.r[30], r5, 0X0);
    // vadd        $v25, $v25, $v22
    rsp.VADD<0>(rsp.vpu.r[25], rsp.vpu.r[25], rsp.vpu.r[22]);
    // sqv         $v29[0], 0x10($5)
    rsp.SQV<0>(rsp.vpu.r[29], r5, 0X1);
    // vadd        $v28, $v28, $v14
    rsp.VADD<0>(rsp.vpu.r[28], rsp.vpu.r[28], rsp.vpu.r[14]);
    // addi        $5, $5, 0x20
    r5 = RSP_ADD32(r5, 0X20);
    // vadd        $v27, $v27, $v13
    rsp.VADD<0>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[13]);
    // vadd        $v24, $v24, $v18
    rsp.VADD<0>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[18]);
    // vadd        $v23, $v23, $v17
    rsp.VADD<0>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[17]);
    // vsub        $v26, $v18, $v26
    rsp.VSUB<0>(rsp.vpu.r[26], rsp.vpu.r[18], rsp.vpu.r[26]);
    // vsub        $v25, $v17, $v25
    rsp.VSUB<0>(rsp.vpu.r[25], rsp.vpu.r[17], rsp.vpu.r[25]);
    // vadd        $v28, $v28, $v18
    rsp.VADD<0>(rsp.vpu.r[28], rsp.vpu.r[28], rsp.vpu.r[18]);
    // vadd        $v27, $v27, $v17
    rsp.VADD<0>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[17]);
    // vge         $v24, $v24, $v0
    rsp.VGE<0>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[0]);
    // vge         $v23, $v23, $v0
    rsp.VGE<0>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[0]);
    // vge         $v26, $v26, $v0
    rsp.VGE<0>(rsp.vpu.r[26], rsp.vpu.r[26], rsp.vpu.r[0]);
    // vge         $v25, $v25, $v0
    rsp.VGE<0>(rsp.vpu.r[25], rsp.vpu.r[25], rsp.vpu.r[0]);
    // vge         $v28, $v28, $v0
    rsp.VGE<0>(rsp.vpu.r[28], rsp.vpu.r[28], rsp.vpu.r[0]);
    // vge         $v27, $v27, $v0
    rsp.VGE<0>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[0]);
    // vlt         $v24, $v24, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[4]);
    // vlt         $v23, $v23, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[4]);
    // vlt         $v26, $v26, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[26], rsp.vpu.r[26], rsp.vpu.r[4]);
    // vlt         $v25, $v25, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[25], rsp.vpu.r[25], rsp.vpu.r[4]);
    // vlt         $v28, $v28, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[28], rsp.vpu.r[28], rsp.vpu.r[4]);
    // vlt         $v27, $v27, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[4]);
    // vmudm       $v24, $v24, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[4]);
    // lqv         $v6[0], 0x0($4)
    rsp.LQV<0>(rsp.vpu.r[6], r4, 0X0);
    // vmudm       $v23, $v23, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[4]);
    // lqv         $v5[0], 0x80($4)
    rsp.LQV<0>(rsp.vpu.r[5], r4, 0X8);
    // vmudm       $v26, $v26, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[26], rsp.vpu.r[26], rsp.vpu.r[4]);
    // addi        $4, $4, 0x10
    r4 = RSP_ADD32(r4, 0X10);
    // vmudm       $v25, $v25, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[25], rsp.vpu.r[25], rsp.vpu.r[4]);
    // vmudm       $v28, $v28, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[28], rsp.vpu.r[28], rsp.vpu.r[4]);
    // vmudm       $v27, $v27, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[4]);
    // vmudn       $v24, $v24, $v1[3]
    rsp.VMUDN<11>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[1]);
    // vmudn       $v23, $v23, $v1[3]
    rsp.VMUDN<11>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[1]);
    // vmudh       $v26, $v26, $v1[4]
    rsp.VMUDH<12>(rsp.vpu.r[26], rsp.vpu.r[26], rsp.vpu.r[1]);
    // vmudh       $v25, $v25, $v1[4]
    rsp.VMUDH<12>(rsp.vpu.r[25], rsp.vpu.r[25], rsp.vpu.r[1]);
    // vmudh       $v28, $v28, $v1[5]
    rsp.VMUDH<13>(rsp.vpu.r[28], rsp.vpu.r[28], rsp.vpu.r[1]);
    // vmudh       $v27, $v27, $v1[5]
    rsp.VMUDH<13>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[1]);
    // vadd        $v18, $v6, $v4[7]
    rsp.VADD<15>(rsp.vpu.r[18], rsp.vpu.r[6], rsp.vpu.r[4]);
    // vadd        $v17, $v5, $v4[7]
    rsp.VADD<15>(rsp.vpu.r[17], rsp.vpu.r[5], rsp.vpu.r[4]);
    // vor         $v24, $v24, $v26
    rsp.VOR<0>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[26]);
    // vor         $v23, $v23, $v25
    rsp.VOR<0>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[25]);
    // vmudm       $v20, $v16, $v4[0]
    rsp.VMUDM<8>(rsp.vpu.r[20], rsp.vpu.r[16], rsp.vpu.r[4]);
    // vmudm       $v19, $v15, $v4[0]
    rsp.VMUDM<8>(rsp.vpu.r[19], rsp.vpu.r[15], rsp.vpu.r[4]);
    // vor         $v30, $v24, $v28
    rsp.VOR<0>(rsp.vpu.r[30], rsp.vpu.r[24], rsp.vpu.r[28]);
    // vor         $v29, $v23, $v27
    rsp.VOR<0>(rsp.vpu.r[29], rsp.vpu.r[23], rsp.vpu.r[27]);
    // vmudm       $v26, $v14, $v4[1]
    rsp.VMUDM<9>(rsp.vpu.r[26], rsp.vpu.r[14], rsp.vpu.r[4]);
    // vmudm       $v25, $v13, $v4[1]
    rsp.VMUDM<9>(rsp.vpu.r[25], rsp.vpu.r[13], rsp.vpu.r[4]);
    // vmudm       $v21, $v16, $v4[2]
    rsp.VMUDM<10>(rsp.vpu.r[21], rsp.vpu.r[16], rsp.vpu.r[4]);
    // vor         $v30, $v30, $v1[6]
    rsp.VOR<14>(rsp.vpu.r[30], rsp.vpu.r[30], rsp.vpu.r[1]);
    // vor         $v29, $v29, $v1[6]
    rsp.VOR<14>(rsp.vpu.r[29], rsp.vpu.r[29], rsp.vpu.r[1]);
    // vmudm       $v22, $v15, $v4[2]
    rsp.VMUDM<10>(rsp.vpu.r[22], rsp.vpu.r[15], rsp.vpu.r[4]);
    // vmudm       $v28, $v14, $v4[3]
    rsp.VMUDM<11>(rsp.vpu.r[28], rsp.vpu.r[14], rsp.vpu.r[4]);
    // vmudm       $v27, $v13, $v4[3]
    rsp.VMUDM<11>(rsp.vpu.r[27], rsp.vpu.r[13], rsp.vpu.r[4]);
    // vadd        $v24, $v20, $v16
    rsp.VADD<0>(rsp.vpu.r[24], rsp.vpu.r[20], rsp.vpu.r[16]);
    // vadd        $v23, $v19, $v15
    rsp.VADD<0>(rsp.vpu.r[23], rsp.vpu.r[19], rsp.vpu.r[15]);
    // vadd        $v26, $v26, $v21
    rsp.VADD<0>(rsp.vpu.r[26], rsp.vpu.r[26], rsp.vpu.r[21]);
    // sqv         $v30[0], 0x0($5)
    rsp.SQV<0>(rsp.vpu.r[30], r5, 0X0);
    // vadd        $v25, $v25, $v22
    rsp.VADD<0>(rsp.vpu.r[25], rsp.vpu.r[25], rsp.vpu.r[22]);
    // sqv         $v29[0], 0x10($5)
    rsp.SQV<0>(rsp.vpu.r[29], r5, 0X1);
    // vadd        $v28, $v28, $v14
    rsp.VADD<0>(rsp.vpu.r[28], rsp.vpu.r[28], rsp.vpu.r[14]);
    // addi        $5, $5, 0x20
    r5 = RSP_ADD32(r5, 0X20);
    // vadd        $v27, $v27, $v13
    rsp.VADD<0>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[13]);
    // vadd        $v24, $v24, $v18
    rsp.VADD<0>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[18]);
    // vadd        $v23, $v23, $v17
    rsp.VADD<0>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[17]);
    // vsub        $v26, $v18, $v26
    rsp.VSUB<0>(rsp.vpu.r[26], rsp.vpu.r[18], rsp.vpu.r[26]);
    // vsub        $v25, $v17, $v25
    rsp.VSUB<0>(rsp.vpu.r[25], rsp.vpu.r[17], rsp.vpu.r[25]);
    // vadd        $v28, $v28, $v18
    rsp.VADD<0>(rsp.vpu.r[28], rsp.vpu.r[28], rsp.vpu.r[18]);
    // vadd        $v27, $v27, $v17
    rsp.VADD<0>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[17]);
    // vge         $v24, $v24, $v0
    rsp.VGE<0>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[0]);
    // vge         $v23, $v23, $v0
    rsp.VGE<0>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[0]);
    // vge         $v26, $v26, $v0
    rsp.VGE<0>(rsp.vpu.r[26], rsp.vpu.r[26], rsp.vpu.r[0]);
    // vge         $v25, $v25, $v0
    rsp.VGE<0>(rsp.vpu.r[25], rsp.vpu.r[25], rsp.vpu.r[0]);
    // vge         $v28, $v28, $v0
    rsp.VGE<0>(rsp.vpu.r[28], rsp.vpu.r[28], rsp.vpu.r[0]);
    // vge         $v27, $v27, $v0
    rsp.VGE<0>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[0]);
    // vlt         $v24, $v24, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[4]);
    // vlt         $v23, $v23, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[4]);
    // vlt         $v26, $v26, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[26], rsp.vpu.r[26], rsp.vpu.r[4]);
    // vlt         $v25, $v25, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[25], rsp.vpu.r[25], rsp.vpu.r[4]);
    // vlt         $v28, $v28, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[28], rsp.vpu.r[28], rsp.vpu.r[4]);
    // vlt         $v27, $v27, $v4[4]
    rsp.VLT<12>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[4]);
    // vmudm       $v24, $v24, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[4]);
    // lqv         $v7[0], 0x0($20)
    rsp.LQV<0>(rsp.vpu.r[7], r20, 0X0);
    // vmudm       $v23, $v23, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[4]);
    // lqv         $v8[0], 0x80($20)
    rsp.LQV<0>(rsp.vpu.r[8], r20, 0X8);
    // vmudm       $v26, $v26, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[26], rsp.vpu.r[26], rsp.vpu.r[4]);
    // addi        $20, $20, 0x10
    r20 = RSP_ADD32(r20, 0X10);
    // vmudm       $v25, $v25, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[25], rsp.vpu.r[25], rsp.vpu.r[4]);
    // vmudm       $v28, $v28, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[28], rsp.vpu.r[28], rsp.vpu.r[4]);
    // vmudm       $v27, $v27, $v4[6]
    rsp.VMUDM<14>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[4]);
    // vmudn       $v24, $v24, $v1[3]
    rsp.VMUDN<11>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[1]);
    // vmudn       $v23, $v23, $v1[3]
    rsp.VMUDN<11>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[1]);
    // vmudh       $v26, $v26, $v1[4]
    rsp.VMUDH<12>(rsp.vpu.r[26], rsp.vpu.r[26], rsp.vpu.r[1]);
    // vmudh       $v25, $v25, $v1[4]
    rsp.VMUDH<12>(rsp.vpu.r[25], rsp.vpu.r[25], rsp.vpu.r[1]);
    // vmudh       $v28, $v28, $v1[5]
    rsp.VMUDH<13>(rsp.vpu.r[28], rsp.vpu.r[28], rsp.vpu.r[1]);
    // bgtz        $8, L_182C
    if (RSP_SIGNED(r8) > 0) {
        // vmudh       $v27, $v27, $v1[5]
        rsp.VMUDH<13>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[1]);
        goto L_182C;
    }
    // vmudh       $v27, $v27, $v1[5]
    rsp.VMUDH<13>(rsp.vpu.r[27], rsp.vpu.r[27], rsp.vpu.r[1]);
    // addi        $7, $7, -0x1
    r7 = RSP_ADD32(r7, -0X1);
    // addi        $8, $zero, 0x4
    r8 = RSP_ADD32(0, 0X4);
    // bgtz        $7, L_182C
    if (RSP_SIGNED(r7) > 0) {
        // addi        $4, $4, 0x80
        r4 = RSP_ADD32(r4, 0X80);
        goto L_182C;
    }
    // addi        $4, $4, 0x80
    r4 = RSP_ADD32(r4, 0X80);
    // vor         $v24, $v24, $v26
    rsp.VOR<0>(rsp.vpu.r[24], rsp.vpu.r[24], rsp.vpu.r[26]);
    // vor         $v23, $v23, $v25
    rsp.VOR<0>(rsp.vpu.r[23], rsp.vpu.r[23], rsp.vpu.r[25]);
    // vor         $v30, $v24, $v28
    rsp.VOR<0>(rsp.vpu.r[30], rsp.vpu.r[24], rsp.vpu.r[28]);
    // vor         $v29, $v23, $v27
    rsp.VOR<0>(rsp.vpu.r[29], rsp.vpu.r[23], rsp.vpu.r[27]);
    // vor         $v30, $v30, $v1[6]
    rsp.VOR<14>(rsp.vpu.r[30], rsp.vpu.r[30], rsp.vpu.r[1]);
    // vor         $v29, $v29, $v1[6]
    rsp.VOR<14>(rsp.vpu.r[29], rsp.vpu.r[29], rsp.vpu.r[1]);
    // sqv         $v30[0], 0x0($5)
    rsp.SQV<0>(rsp.vpu.r[30], r5, 0X0);
    // sqv         $v29[0], 0x10($5)
    rsp.SQV<0>(rsp.vpu.r[29], r5, 0X1);
L_1A80:
    // jal         0x1B58
    r31 = 0x1A88;
    // nop

    goto L_1B58;
    // nop

L_1A88:
    // mfc0        $22, SP_STATUS
    r22 = 0;
    // beq         $3, $zero, L_1ADC
    if (r3 == 0) {
        // add         $25, $25, $12
        r25 = RSP_ADD32(r25, r12);
        goto L_1ADC;
    }
    // add         $25, $25, $12
    r25 = RSP_ADD32(r25, r12);
    // andi        $22, $22, 0x80
    r22 = r22 & 0X80;
    // beq         $22, $zero, L_1158
    if (r22 == 0) {
        // nop
    
        goto L_1158;
    }
    // nop

    // sub         $25, $25, $12
    r25 = RSP_SUB32(r25, r12);
    // sw          $3, 0x1E0($zero)
    RSP_MEM_W_STORE(0X1E0, 0, r3);
    // sw          $25, 0x1E4($zero)
    RSP_MEM_W_STORE(0X1E4, 0, r25);
    // sw          $26, 0x1E8($zero)
    RSP_MEM_W_STORE(0X1E8, 0, r26);
    // sw          $9, 0x1EC($zero)
    RSP_MEM_W_STORE(0X1EC, 0, r9);
    // sw          $10, 0x1F0($zero)
    RSP_MEM_W_STORE(0X1F0, 0, r10);
    // sw          $11, 0x1F4($zero)
    RSP_MEM_W_STORE(0X1F4, 0, r11);
    // sw          $12, 0x1F8($zero)
    RSP_MEM_W_STORE(0X1F8, 0, r12);
    // addi        $27, $zero, 0x0
    r27 = RSP_ADD32(0, 0X0);
    // addi        $28, $23, 0x0
    r28 = RSP_ADD32(r23, 0X0);
    // jal         0x1B30
    r31 = 0x1AD0;
    // addi        $29, $24, -0x1
    r29 = RSP_ADD32(r24, -0X1);
    goto L_1B30;
    // addi        $29, $24, -0x1
    r29 = RSP_ADD32(r24, -0X1);
L_1AD0:
    // ori         $21, $zero, 0x1000
    r21 = 0 | 0X1000;
    // jal         0x1B58
    r31 = 0x1ADC;
    // mtc0        $21, SP_STATUS
    goto L_1B58;
    // mtc0        $21, SP_STATUS
L_1ADC:
    // addi        $27, $zero, 0xBE0
    r27 = RSP_ADD32(0, 0XBE0);
    // addi        $28, $26, 0x0
    r28 = RSP_ADD32(r26, 0X0);
    // jal         0x1B30
    r31 = 0x1AEC;
    // add         $29, $zero, $11
    r29 = RSP_ADD32(0, r11);
    goto L_1B30;
    // add         $29, $zero, $11
    r29 = RSP_ADD32(0, r11);
L_1AEC:
    // jal         0x1B58
    r31 = 0x1AF4;
    // addi        $21, $zero, 0x4000
    r21 = RSP_ADD32(0, 0X4000);
    goto L_1B58;
    // addi        $21, $zero, 0x4000
    r21 = RSP_ADD32(0, 0X4000);
L_1AF4:
    // mtc0        $21, SP_STATUS
    // break       0
    return RspExitReason::Broke;
    // nop

L_1B00:
    // j           L_1B00
    // nop

    goto L_1B00;
    // nop

L_1B08:
    // mfc0        $30, SP_SEMAPHORE
    r30 = 0;
L_1B0C:
    // bne         $30, $zero, L_1B0C
    if (r30 != 0) {
        // mfc0        $30, SP_SEMAPHORE
        r30 = 0;
        goto L_1B0C;
    }
    // mfc0        $30, SP_SEMAPHORE
    r30 = 0;
    // mfc0        $30, SP_DMA_FULL
    r30 = 0;
L_1B18:
    // bne         $30, $zero, L_1B18
    if (r30 != 0) {
        // mfc0        $30, SP_DMA_FULL
        r30 = 0;
        goto L_1B18;
    }
    // mfc0        $30, SP_DMA_FULL
    r30 = 0;
    // mtc0        $28, SP_MEM_ADDR
    SET_DMA_MEM(r28);
    // mtc0        $27, SP_DRAM_ADDR
    SET_DMA_DRAM(r27);
    // jr          $ra
    jump_target = r31;
    debug_file = __FILE__; debug_line = __LINE__;
    // mtc0        $29, SP_RD_LEN
    DO_DMA_READ(r29);
    goto do_indirect_jump;
    // mtc0        $29, SP_RD_LEN
    DO_DMA_READ(r29);
L_1B30:
    // mfc0        $30, SP_SEMAPHORE
    r30 = 0;
L_1B34:
    // bne         $30, $zero, L_1B34
    if (r30 != 0) {
        // mfc0        $30, SP_SEMAPHORE
        r30 = 0;
        goto L_1B34;
    }
    // mfc0        $30, SP_SEMAPHORE
    r30 = 0;
    // mfc0        $30, SP_DMA_FULL
    r30 = 0;
L_1B40:
    // bne         $30, $zero, L_1B40
    if (r30 != 0) {
        // mfc0        $30, SP_DMA_FULL
        r30 = 0;
        goto L_1B40;
    }
    // mfc0        $30, SP_DMA_FULL
    r30 = 0;
    // mtc0        $27, SP_MEM_ADDR
    SET_DMA_MEM(r27);
    // mtc0        $28, SP_DRAM_ADDR
    SET_DMA_DRAM(r28);
    // jr          $ra
    jump_target = r31;
    debug_file = __FILE__; debug_line = __LINE__;
    // mtc0        $29, SP_WR_LEN
    DO_DMA_WRITE(r29);
    goto do_indirect_jump;
    // mtc0        $29, SP_WR_LEN
    DO_DMA_WRITE(r29);
L_1B58:
    // mfc0        $30, SP_DMA_BUSY
    r30 = 0;
L_1B5C:
    // bne         $30, $zero, L_1B5C
    if (r30 != 0) {
        // mfc0        $30, SP_DMA_BUSY
        r30 = 0;
        goto L_1B5C;
    }
    // mfc0        $30, SP_DMA_BUSY
    r30 = 0;
    // jr          $ra
    jump_target = r31;
    debug_file = __FILE__; debug_line = __LINE__;
    // mtc0        $zero, SP_SEMAPHORE
    goto do_indirect_jump;
    // mtc0        $zero, SP_SEMAPHORE
    // nop

    return RspExitReason::ImemOverrun;
do_indirect_jump:
    switch ((jump_target | 0x1000) & 0X1FFF) { 
        case 0x10D4: goto L_10D4;
        case 0x10CC: goto L_10CC;
        case 0x10F8: goto L_10F8;
        case 0x10F0: goto L_10F0;
        case 0x10E8: goto L_10E8;
        case 0x10E0: goto L_10E0;
        case 0x1108: goto L_1108;
        case 0x1100: goto L_1100;
        case 0x1144: goto L_1144;
        case 0x1158: goto L_1158;
        case 0x116C: goto L_116C;
        case 0x13D0: goto L_13D0;
        case 0x13E4: goto L_13E4;
        case 0x1A88: goto L_1A88;
        case 0x1AD0: goto L_1AD0;
        case 0x1ADC: goto L_1ADC;
        case 0x1AEC: goto L_1AEC;
        case 0x1AF4: goto L_1AF4;
    }
    printf("Unhandled jump target 0x%04X in microcode njpgdspMain, coming from [%s:%d]\n", jump_target, debug_file, debug_line);
    printf("Register dump: r0  = %08X r1  = %08X r2  = %08X r3  = %08X r4  = %08X r5  = %08X r6  = %08X r7  = %08X\n"
           "               r8  = %08X r9  = %08X r10 = %08X r11 = %08X r12 = %08X r13 = %08X r14 = %08X r15 = %08X\n"
           "               r16 = %08X r17 = %08X r18 = %08X r19 = %08X r20 = %08X r21 = %08X r22 = %08X r23 = %08X\n"
           "               r24 = %08X r25 = %08X r26 = %08X r27 = %08X r28 = %08X r29 = %08X r30 = %08X r31 = %08X\n",
           0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15, r16,
           r17, r18, r19, r20, r21, r22, r23, r24, r25, r26, r27, r28, r29, r30, r31);
    return RspExitReason::UnhandledJumpTarget;
}
