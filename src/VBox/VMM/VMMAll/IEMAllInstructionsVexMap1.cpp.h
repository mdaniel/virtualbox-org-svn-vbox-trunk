/* $Id$ */
/** @file
 * IEM - Instruction Decoding and Emulation.
 *
 * @remarks IEMAllInstructionsTwoByte0f.cpp.h is a legacy mirror of this file.
 *          Any update here is likely needed in that file too.
 */

/*
 * Copyright (C) 2011-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @name VEX Opcode Map 1
 * @{
 */


/*  Opcode VEX.0F 0x00 - invalid */
/*  Opcode VEX.0F 0x01 - invalid */
/*  Opcode VEX.0F 0x02 - invalid */
/*  Opcode VEX.0F 0x03 - invalid */
/*  Opcode VEX.0F 0x04 - invalid */
/*  Opcode VEX.0F 0x05 - invalid */
/*  Opcode VEX.0F 0x06 - invalid */
/*  Opcode VEX.0F 0x07 - invalid */
/*  Opcode VEX.0F 0x08 - invalid */
/*  Opcode VEX.0F 0x09 - invalid */
/*  Opcode VEX.0F 0x0a - invalid */

/** Opcode VEX.0F 0x0b. */
FNIEMOP_DEF(iemOp_vud2)
{
    IEMOP_MNEMONIC(vud2, "vud2");
    return IEMOP_RAISE_INVALID_OPCODE();
}

/*  Opcode VEX.0F 0x0c - invalid */
/*  Opcode VEX.0F 0x0d - invalid */
/*  Opcode VEX.0F 0x0e - invalid */
/*  Opcode VEX.0F 0x0f - invalid */


/**
 * @opcode      0x10
 * @oppfx       none
 * @opcpuid     avx
 * @opgroup     og_avx_simdfp_datamove
 * @opxcpttype  4UA
 * @optest      op1=1 op2=2 -> op1=2
 * @optest      op1=0 op2=-22 -> op1=-22
 */
FNIEMOP_DEF(iemOp_vmovups_Vps_Wps)
{
    IEMOP_MNEMONIC2(VEX_RM, VMOVUPS, vmovups, Vps_WO, Wps, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);
    Assert(pVCpu->iem.s.uVexLength <= 1);
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register, register.
         */
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();
        if (pVCpu->iem.s.uVexLength == 0)
            IEM_MC_COPY_YREG_U128_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
                                           (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        else
            IEM_MC_COPY_YREG_U256_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
                                           (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else if (pVCpu->iem.s.uVexLength == 0)
    {
        /*
         * 128-bit: Memory, register.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(RTUINT128U,                uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();

        IEM_MC_FETCH_MEM_U128(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_STORE_YREG_U128_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * 256-bit: Memory, register.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(RTUINT256U,                uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();

        IEM_MC_FETCH_MEM_U256(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_STORE_YREG_U256_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/**
 * @opcode      0x10
 * @oppfx       0x66
 * @opcpuid     avx
 * @opgroup     og_avx_simdfp_datamove
 * @opxcpttype  4UA
 * @optest      op1=1 op2=2 -> op1=2
 * @optest      op1=0 op2=-22 -> op1=-22
 */
FNIEMOP_DEF(iemOp_vmovupd_Vpd_Wpd)
{
    IEMOP_MNEMONIC2(VEX_RM, VMOVUPD, vmovupd, Vpd_WO, Wpd, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);
    Assert(pVCpu->iem.s.uVexLength <= 1);
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register, register.
         */
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();
        if (pVCpu->iem.s.uVexLength == 0)
            IEM_MC_COPY_YREG_U128_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
                                           (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        else
            IEM_MC_COPY_YREG_U256_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
                                           (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else if (pVCpu->iem.s.uVexLength == 0)
    {
        /*
         * 128-bit: Memory, register.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(RTUINT128U,                uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();

        IEM_MC_FETCH_MEM_U128(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_STORE_YREG_U128_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * 256-bit: Memory, register.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(RTUINT256U,                uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();

        IEM_MC_FETCH_MEM_U256(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_STORE_YREG_U256_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


FNIEMOP_DEF(iemOp_vmovss_Vss_Hss_Wss)
{
    Assert(pVCpu->iem.s.uVexLength <= 1);
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /**
         * @opcode      0x10
         * @oppfx       0xf3
         * @opcodesub   11 mr/reg
         * @opcpuid     avx
         * @opgroup     og_avx_simdfp_datamerge
         * @opxcpttype  5
         * @optest      op1=1 op2=0  op3=2    -> op1=2
         * @optest      op1=0 op2=0  op3=-22  -> op1=0xffffffea
         * @optest      op1=3 op2=-1 op3=0x77 -> op1=-4294967177
         * @optest      op1=3 op2=-2 op3=0x77 -> op1=-8589934473
         * @note        HssHi refers to bits 127:32.
         */
        IEMOP_MNEMONIC3(VEX_RVM, VMOVSS, vmovss, Vss_WO, HssHi, Uss, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE | IEMOPHINT_IGNORES_VEX_L);
        IEMOP_HLP_DONE_VEX_DECODING();
        IEM_MC_BEGIN(0, 0);

        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();
        IEM_MC_MERGE_YREG_U32_U96_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
                                           (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB /*U32*/,
                                            IEM_GET_EFFECTIVE_VVVV(pVCpu) /*Hss*/);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /**
         * @opdone
         * @opcode      0x10
         * @oppfx       0xf3
         * @opcodesub   11 mr/reg
         * @opcpuid     avx
         * @opgroup     og_avx_simdfp_datamove
         * @opxcpttype  5
         * @opfunction  iemOp_vmovss_Vss_Hss_Wss
         * @optest      op1=1 op2=2 -> op1=2
         * @optest      op1=0 op2=-22 -> op1=-22
         */
        IEMOP_MNEMONIC2(VEX_XM, VMOVSS, vmovss, VssZx_WO, Md, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE | IEMOPHINT_IGNORES_VEX_L);
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint32_t,                  uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();

        IEM_MC_FETCH_MEM_U32(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_STORE_YREG_U32_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }

    return VINF_SUCCESS;
}


FNIEMOP_DEF(iemOp_vmovsd_Vsd_Hsd_Wsd)
{
    Assert(pVCpu->iem.s.uVexLength <= 1);
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /**
         * @opcode      0x10
         * @oppfx       0xf2
         * @opcodesub   11 mr/reg
         * @opcpuid     avx
         * @opgroup     og_avx_simdfp_datamerge
         * @opxcpttype  5
         * @optest      op1=1 op2=0  op3=2    -> op1=2
         * @optest      op1=0 op2=0  op3=-22  -> op1=0xffffffffffffffea
         * @optest      op1=3 op2=-1 op3=0x77 ->
         *              op1=0xffffffffffffffff0000000000000077
         * @optest      op1=3 op2=0x42 op3=0x77 -> op1=0x420000000000000077
         */
        IEMOP_MNEMONIC3(VEX_RVM, VMOVSD, vmovsd, Vsd_WO, HsdHi, Usd, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE | IEMOPHINT_IGNORES_VEX_L);
        IEMOP_HLP_DONE_VEX_DECODING();
        IEM_MC_BEGIN(0, 0);

        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();
        IEM_MC_MERGE_YREG_U64_U64_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
                                           (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB /*U32*/,
                                           IEM_GET_EFFECTIVE_VVVV(pVCpu) /*Hss*/);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /**
         * @opdone
         * @opcode      0x10
         * @oppfx       0xf2
         * @opcodesub   11 mr/reg
         * @opcpuid     avx
         * @opgroup     og_avx_simdfp_datamove
         * @opxcpttype  5
         * @opfunction  iemOp_vmovsd_Vsd_Hsd_Wsd
         * @optest      op1=1 op2=2 -> op1=2
         * @optest      op1=0 op2=-22 -> op1=-22
         */
        IEMOP_MNEMONIC2(VEX_XM, VMOVSD, vmovsd, VsdZx_WO, Mq, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE | IEMOPHINT_IGNORES_VEX_L);
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint64_t,                  uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();

        IEM_MC_FETCH_MEM_U64(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_STORE_YREG_U64_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }

    return VINF_SUCCESS;
}


/**
 * @opcode      0x11
 * @oppfx       none
 * @opcpuid     avx
 * @opgroup     og_avx_simdfp_datamove
 * @opxcpttype  4UA
 * @optest      op1=1 op2=2 -> op1=2
 * @optest      op1=0 op2=-22 -> op1=-22
 */
FNIEMOP_DEF(iemOp_vmovups_Wps_Vps)
{
    IEMOP_MNEMONIC2(VEX_MR, VMOVUPS, vmovups, Wps_WO, Vps, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);
    Assert(pVCpu->iem.s.uVexLength <= 1);
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register, register.
         */
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();
        if (pVCpu->iem.s.uVexLength == 0)
            IEM_MC_COPY_YREG_U128_ZX_VLMAX((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,
                                           ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        else
            IEM_MC_COPY_YREG_U256_ZX_VLMAX((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,
                                           ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else if (pVCpu->iem.s.uVexLength == 0)
    {
        /*
         * 128-bit: Memory, register.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(RTUINT128U,                uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_READ();

        IEM_MC_FETCH_YREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U128(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * 256-bit: Memory, register.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(RTUINT256U,                uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_READ();

        IEM_MC_FETCH_YREG_U256(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U256(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/**
 * @opcode      0x11
 * @oppfx       0x66
 * @opcpuid     avx
 * @opgroup     og_avx_simdfp_datamove
 * @opxcpttype  4UA
 * @optest      op1=1 op2=2 -> op1=2
 * @optest      op1=0 op2=-22 -> op1=-22
 */
FNIEMOP_DEF(iemOp_vmovupd_Wpd_Vpd)
{
    IEMOP_MNEMONIC2(VEX_MR, VMOVUPD, vmovupd, Wpd_WO, Vpd, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);
    Assert(pVCpu->iem.s.uVexLength <= 1);
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register, register.
         */
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();
        if (pVCpu->iem.s.uVexLength == 0)
            IEM_MC_COPY_YREG_U128_ZX_VLMAX((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,
                                           ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        else
            IEM_MC_COPY_YREG_U256_ZX_VLMAX((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,
                                           ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else if (pVCpu->iem.s.uVexLength == 0)
    {
        /*
         * 128-bit: Memory, register.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(RTUINT128U,                uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_READ();

        IEM_MC_FETCH_YREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U128(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * 256-bit: Memory, register.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(RTUINT256U,                uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_READ();

        IEM_MC_FETCH_YREG_U256(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U256(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


FNIEMOP_DEF(iemOp_vmovss_Wss_Hss_Vss)
{
    Assert(pVCpu->iem.s.uVexLength <= 1);
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /**
         * @opcode      0x11
         * @oppfx       0xf3
         * @opcodesub   11 mr/reg
         * @opcpuid     avx
         * @opgroup     og_avx_simdfp_datamerge
         * @opxcpttype  5
         * @optest      op1=1 op2=0  op3=2    -> op1=2
         * @optest      op1=0 op2=0  op3=-22  -> op1=0xffffffea
         * @optest      op1=3 op2=-1 op3=0x77 -> op1=-4294967177
         * @optest      op1=3 op2=0x42 op3=0x77 -> op1=0x4200000077
         */
        IEMOP_MNEMONIC3(VEX_MVR, VMOVSS, vmovss, Uss_WO, HssHi, Vss, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE | IEMOPHINT_IGNORES_VEX_L);
        IEMOP_HLP_DONE_VEX_DECODING();
        IEM_MC_BEGIN(0, 0);

        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();
        IEM_MC_MERGE_YREG_U32_U96_ZX_VLMAX((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB /*U32*/,
                                           ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
                                           IEM_GET_EFFECTIVE_VVVV(pVCpu) /*Hss*/);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /**
         * @opdone
         * @opcode      0x11
         * @oppfx       0xf3
         * @opcodesub   11 mr/reg
         * @opcpuid     avx
         * @opgroup     og_avx_simdfp_datamove
         * @opxcpttype  5
         * @opfunction  iemOp_vmovss_Vss_Hss_Wss
         * @optest      op1=1 op2=2 -> op1=2
         * @optest      op1=0 op2=-22 -> op1=-22
         */
        IEMOP_MNEMONIC2(VEX_MR, VMOVSS, vmovss, Md_WO, Vss, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE | IEMOPHINT_IGNORES_VEX_L);
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint32_t,                  uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_READ();

        IEM_MC_FETCH_YREG_U32(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U32(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }

    return VINF_SUCCESS;
}


FNIEMOP_DEF(iemOp_vmovsd_Wsd_Hsd_Vsd)
{
    Assert(pVCpu->iem.s.uVexLength <= 1);
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /**
         * @opcode      0x11
         * @oppfx       0xf2
         * @opcodesub   11 mr/reg
         * @opcpuid     avx
         * @opgroup     og_avx_simdfp_datamerge
         * @opxcpttype  5
         * @optest      op1=1 op2=0  op3=2    -> op1=2
         * @optest      op1=0 op2=0  op3=-22  -> op1=0xffffffffffffffea
         * @optest      op1=3 op2=-1 op3=0x77 ->
         *              op1=0xffffffffffffffff0000000000000077
         * @optest      op2=0x42 op3=0x77 -> op1=0x420000000000000077
         */
        IEMOP_MNEMONIC3(VEX_MVR, VMOVSD, vmovsd, Usd_WO, HsdHi, Vsd, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE | IEMOPHINT_IGNORES_VEX_L);
        IEMOP_HLP_DONE_VEX_DECODING();
        IEM_MC_BEGIN(0, 0);

        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();
        IEM_MC_MERGE_YREG_U64_U64_ZX_VLMAX((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,
                                           ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
                                           IEM_GET_EFFECTIVE_VVVV(pVCpu) /*Hss*/);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /**
         * @opdone
         * @opcode      0x11
         * @oppfx       0xf2
         * @opcodesub   11 mr/reg
         * @opcpuid     avx
         * @opgroup     og_avx_simdfp_datamove
         * @opxcpttype  5
         * @opfunction  iemOp_vmovsd_Wsd_Hsd_Vsd
         * @optest      op1=1 op2=2 -> op1=2
         * @optest      op1=0 op2=-22 -> op1=-22
         */
        IEMOP_MNEMONIC2(VEX_MR, VMOVSD, vmovsd, Mq_WO, Vsd, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE | IEMOPHINT_IGNORES_VEX_L);
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint64_t,                  uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_READ();

        IEM_MC_FETCH_YREG_U64(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }

    return VINF_SUCCESS;
}


FNIEMOP_DEF(iemOp_vmovlps_Vq_Hq_Mq__vmovhlps)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /**
         * @opcode      0x12
         * @opcodesub   11 mr/reg
         * @oppfx       none
         * @opcpuid     avx
         * @opgroup     og_avx_simdfp_datamerge
         * @opxcpttype  7LZ
         * @optest         op2=0x2200220122022203
         *                 op3=0x3304330533063307
         *              -> op1=0x22002201220222033304330533063307
         * @optest      op2=-1  op3=-42 -> op1=-42
         * @note        op3 and op2 are only the 8-byte high XMM register halfs.
         */
        IEMOP_MNEMONIC3(VEX_RVM, VMOVHLPS, vmovhlps, Vq_WO, HqHi, UqHi, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE | IEMOPHINT_VEX_L_ZERO);

        IEMOP_HLP_DONE_VEX_DECODING_L0();
        IEM_MC_BEGIN(0, 0);

        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();
        IEM_MC_MERGE_YREG_U64HI_U64_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
                                             (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,
                                             IEM_GET_EFFECTIVE_VVVV(pVCpu) /*Hq*/);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /**
         * @opdone
         * @opcode      0x12
         * @opcodesub   !11 mr/reg
         * @oppfx       none
         * @opcpuid     avx
         * @opgroup     og_avx_simdfp_datamove
         * @opxcpttype  5LZ
         * @opfunction  iemOp_vmovlps_Vq_Hq_Mq__vmovhlps
         * @optest      op1=1 op2=0 op3=0 -> op1=0
         * @optest      op1=0 op2=-1 op3=-1 -> op1=-1
         * @optest      op1=1 op2=2 op3=3 -> op1=0x20000000000000003
         * @optest      op2=-1 op3=0x42 -> op1=0xffffffffffffffff0000000000000042
         */
        IEMOP_MNEMONIC3(VEX_RVM_MEM, VMOVLPS, vmovlps, Vq_WO, HqHi, Mq, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE | IEMOPHINT_VEX_L_ZERO);

        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint64_t,                  uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_L0();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();

        IEM_MC_FETCH_MEM_U64(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_MERGE_YREG_U64LOCAL_U64_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
                                                uSrc,
                                                IEM_GET_EFFECTIVE_VVVV(pVCpu) /*Hq*/);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/**
 * @opcode      0x12
 * @opcodesub   !11 mr/reg
 * @oppfx       0x66
 * @opcpuid     avx
 * @opgroup     og_avx_pcksclr_datamerge
 * @opxcpttype  5LZ
 * @optest      op2=0 op3=2 -> op1=2
 * @optest      op2=0x22 op3=0x33 -> op1=0x220000000000000033
 * @optest      op2=0xfffffff0fffffff1 op3=0xeeeeeee8eeeeeee9
 *              -> op1=0xfffffff0fffffff1eeeeeee8eeeeeee9
 */
FNIEMOP_DEF(iemOp_vmovlpd_Vq_Hq_Mq)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_MNEMONIC3(VEX_RVM_MEM, VMOVLPD, vmovlpd, Vq_WO, HqHi, Mq, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE | IEMOPHINT_VEX_L_ZERO);

        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint64_t,                  uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_L0();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE();

        IEM_MC_FETCH_MEM_U64(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_MERGE_YREG_U64LOCAL_U64_ZX_VLMAX(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
                                                uSrc,
                                                IEM_GET_EFFECTIVE_VVVV(pVCpu) /*Hq*/);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
        return VINF_SUCCESS;
    }

    /**
     * @opdone
     * @opmnemonic  udvex660f12m3
     * @opcode      0x12
     * @opcodesub   11 mr/reg
     * @oppfx       0x66
     * @opunused    immediate
     * @opcpuid     avx
     * @optest      ->
     */
    return IEMOP_RAISE_INVALID_OPCODE();
}


/**
 * @opcode      0x12
 * @oppfx       0xf3
 * @opcpuid     avx
 * @opgroup     og_avx_pcksclr_datamove
 * @opxcpttype  4
 * @optest      vex.l==0 / op1=-1 op2=0xdddddddd00000002eeeeeeee00000001
 *              -> op1=0x00000002000000020000000100000001
 * @optest      vex.l==1 /
 *                 op2=0xbbbbbbbb00000004cccccccc00000003dddddddd00000002eeeeeeee00000001
 *              -> op1=0x0000000400000004000000030000000300000002000000020000000100000001
 */
FNIEMOP_DEF(iemOp_vmovsldup_Vx_Wx)
{
    IEMOP_MNEMONIC2(VEX_RM, VMOVSLDUP, vmovsldup, Vx_WO, Wx, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);
    Assert(pVCpu->iem.s.uVexLength <= 1);
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register, register.
         */
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        if (pVCpu->iem.s.uVexLength == 0)
        {
            IEM_MC_BEGIN(2, 0);
            IEM_MC_ARG(PRTUINT128U,                 puDst, 0);
            IEM_MC_ARG(PCRTUINT128U,                puSrc, 1);

            IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
            IEM_MC_PREPARE_AVX_USAGE();

            IEM_MC_REF_XREG_U128_CONST(puSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
            IEM_MC_REF_XREG_U128(puDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
            IEM_MC_CALL_SSE_AIMPL_2(iemAImpl_movsldup, puDst, puSrc);
            IEM_MC_CLEAR_YREG_128_UP(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            IEM_MC_BEGIN(3, 0);
            IEM_MC_IMPLICIT_AVX_AIMPL_ARGS();
            IEM_MC_ARG_CONST(uint8_t,   iYRegDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, 1);
            IEM_MC_ARG_CONST(uint8_t,   iYRegSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 2);

            IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
            IEM_MC_PREPARE_AVX_USAGE();
            IEM_MC_CALL_AVX_AIMPL_2(iemAImpl_vmovsldup_256_rr, iYRegDst, iYRegSrc);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
    }
    else
    {
        /*
         * Register, memory.
         */
        if (pVCpu->iem.s.uVexLength == 0)
        {
            IEM_MC_BEGIN(2, 2);
            IEM_MC_LOCAL(RTUINT128U,                uSrc);
            IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
            IEM_MC_ARG(PRTUINT128U,                 puDst, 0);
            IEM_MC_ARG_LOCAL_REF(PCRTUINT128U,      puSrc, uSrc, 1);

            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
            IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
            IEM_MC_PREPARE_AVX_USAGE();

            IEM_MC_FETCH_MEM_U128(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
            IEM_MC_REF_XREG_U128(puDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
            IEM_MC_CALL_SSE_AIMPL_2(iemAImpl_movsldup, puDst, puSrc);
            IEM_MC_CLEAR_YREG_128_UP(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            IEM_MC_BEGIN(3, 2);
            IEM_MC_LOCAL(RTUINT256U,            uSrc);
            IEM_MC_LOCAL(RTGCPTR,               GCPtrEffSrc);
            IEM_MC_IMPLICIT_AVX_AIMPL_ARGS();
            IEM_MC_ARG_CONST(uint8_t,   iYRegDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, 1);
            IEM_MC_ARG_LOCAL_REF(PCRTUINT256U,  puSrc, uSrc, 2);

            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
            IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
            IEM_MC_PREPARE_AVX_USAGE();

            IEM_MC_FETCH_MEM_U256(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
            IEM_MC_CALL_AVX_AIMPL_2(iemAImpl_vmovsldup_256_rm, iYRegDst, puSrc);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
    }
    return VINF_SUCCESS;
}


/**
 * @opcode      0x12
 * @oppfx       0xf2
 * @opcpuid     avx
 * @opgroup     og_avx_pcksclr_datamove
 * @opxcpttype  5
 * @optest      vex.l==0 / op2=0xddddddddeeeeeeee2222222211111111
 *              ->         op1=0x22222222111111112222222211111111
 * @optest      vex.l==1 / op2=0xbbbbbbbbcccccccc4444444433333333ddddddddeeeeeeee2222222211111111
 *              ->         op1=0x4444444433333333444444443333333322222222111111112222222211111111
 * @oponly
 */
FNIEMOP_DEF(iemOp_vmovddup_Vx_Wx)
{
    IEMOP_MNEMONIC2(VEX_RM, VMOVDDUP, vmovddup, Vx_WO, Wx, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register, register.
         */
        IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
        if (pVCpu->iem.s.uVexLength == 0)
        {
            IEM_MC_BEGIN(2, 0);
            IEM_MC_ARG(PRTUINT128U,                 puDst, 0);
            IEM_MC_ARG(uint64_t,                    uSrc, 1);

            IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
            IEM_MC_PREPARE_AVX_USAGE();

            IEM_MC_FETCH_XREG_U64(uSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
            IEM_MC_REF_XREG_U128(puDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
            IEM_MC_CALL_SSE_AIMPL_2(iemAImpl_movddup, puDst, uSrc);
            IEM_MC_CLEAR_YREG_128_UP(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            IEM_MC_BEGIN(3, 0);
            IEM_MC_IMPLICIT_AVX_AIMPL_ARGS();
            IEM_MC_ARG_CONST(uint8_t,   iYRegDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, 1);
            IEM_MC_ARG_CONST(uint8_t,   iYRegSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 2);

            IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
            IEM_MC_PREPARE_AVX_USAGE();
            IEM_MC_CALL_AVX_AIMPL_2(iemAImpl_vmovddup_256_rr, iYRegDst, iYRegSrc);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
    }
    else
    {
        /*
         * Register, memory.
         */
        if (pVCpu->iem.s.uVexLength == 0)
        {
            IEM_MC_BEGIN(2, 2);
            IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
            IEM_MC_ARG(PRTUINT128U,                 puDst, 0);
            IEM_MC_ARG(uint64_t,                    uSrc, 1);

            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
            IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
            IEM_MC_PREPARE_AVX_USAGE();

            IEM_MC_FETCH_MEM_U64(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
            IEM_MC_REF_XREG_U128(puDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
            IEM_MC_CALL_SSE_AIMPL_2(iemAImpl_movddup, puDst, uSrc);
            IEM_MC_CLEAR_YREG_128_UP(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            IEM_MC_BEGIN(3, 2);
            IEM_MC_LOCAL(RTUINT256U,            uSrc);
            IEM_MC_LOCAL(RTGCPTR,               GCPtrEffSrc);
            IEM_MC_IMPLICIT_AVX_AIMPL_ARGS();
            IEM_MC_ARG_CONST(uint8_t,   iYRegDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, 1);
            IEM_MC_ARG_LOCAL_REF(PCRTUINT256U,  puSrc, uSrc, 2);

            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_VEX_DECODING_NO_VVVV();
            IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
            IEM_MC_PREPARE_AVX_USAGE();

            IEM_MC_FETCH_MEM_U256(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
            IEM_MC_CALL_AVX_AIMPL_2(iemAImpl_vmovddup_256_rm, iYRegDst, puSrc);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
    }
    return VINF_SUCCESS;
}


/**
 * @opcode      0x13
 * @opcodesub   !11 mr/reg
 * @oppfx       none
 * @opcpuid     avx
 * @opgroup     og_avx_simdfp_datamove
 * @opxcpttype  5
 * @optest      op1=1 op2=2 -> op1=2
 * @optest      op1=0 op2=-42 -> op1=-42
 */
FNIEMOP_DEF(iemOp_vmovlps_Mq_Vq)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_MNEMONIC2(VEX_MR_MEM, VMOVLPS, vmovlps, Mq_WO, Vq, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE | IEMOPHINT_VEX_L_ZERO);

        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint64_t,                  uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_VEX_DECODING_L0_AND_NO_VVVV();
        IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT();
        IEM_MC_ACTUALIZE_AVX_STATE_FOR_READ();

        IEM_MC_FETCH_YREG_U64(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
        return VINF_SUCCESS;
    }

    /**
     * @opdone
     * @opmnemonic  udvex0f13m3
     * @opcode      0x13
     * @opcodesub   11 mr/reg
     * @oppfx       none
     * @opunused    immediate
     * @opcpuid     avx
     * @optest      ->
     */
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode VEX.66.0F 0x13 - vmovlpd Mq, Vq */
FNIEMOP_STUB(iemOp_vmovlpd_Mq_Vq);
//FNIEMOP_DEF(iemOp_vmovlpd_Mq_Vq)
//{
//    IEMOP_MNEMONIC(vmovlpd_Mq_Vq, "movlpd Mq,Vq");
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//#if 0
//        /*
//         * Register, register.
//         */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(0, 1);
//        IEM_MC_LOCAL(uint64_t,                  uSrc);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//        IEM_MC_FETCH_XREG_U64(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_STORE_XREG_U64((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, uSrc);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//#else
//        return IEMOP_RAISE_INVALID_OPCODE();
//#endif
//    }
//    else
//    {
//        /*
//         * Memory, register.
//         */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(uint64_t,                  uSrc);
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();
//
//        IEM_MC_FETCH_XREG_U64(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}

/*  Opcode VEX.F3.0F 0x13 - invalid */
/*  Opcode VEX.F2.0F 0x13 - invalid */

/** Opcode VEX.0F 0x14 - vunpcklps Vx, Hx, Wx*/
FNIEMOP_STUB(iemOp_vunpcklps_Vx_Hx_Wx);
/** Opcode VEX.66.0F 0x14 - vunpcklpd Vx,Hx,Wx   */
FNIEMOP_STUB(iemOp_vunpcklpd_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0x14 - invalid */
/*  Opcode VEX.F2.0F 0x14 - invalid */
/** Opcode VEX.0F 0x15 - vunpckhps Vx, Hx, Wx   */
FNIEMOP_STUB(iemOp_vunpckhps_Vx_Hx_Wx);
/** Opcode VEX.66.0F 0x15 - vunpckhpd Vx,Hx,Wx   */
FNIEMOP_STUB(iemOp_vunpckhpd_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0x15 - invalid */
/*  Opcode VEX.F2.0F 0x15 - invalid */
/** Opcode VEX.0F 0x16 - vmovhpsv1 Vdq, Hq, Mq vmovlhps Vdq, Hq, Uq   */
FNIEMOP_STUB(iemOp_vmovhpsv1_Vdq_Hq_Mq__vmovlhps_Vdq_Hq_Uq);  //NEXT
/** Opcode VEX.66.0F 0x16 - vmovhpdv1 Vdq, Hq, Mq   */
FNIEMOP_STUB(iemOp_vmovhpdv1_Vdq_Hq_Mq);  //NEXT
/** Opcode VEX.F3.0F 0x16 - vmovshdup Vx, Wx   */
FNIEMOP_STUB(iemOp_vmovshdup_Vx_Wx); //NEXT
/*  Opcode VEX.F2.0F 0x16 - invalid */
/** Opcode VEX.0F 0x17 - vmovhpsv1 Mq, Vq   */
FNIEMOP_STUB(iemOp_vmovhpsv1_Mq_Vq);  //NEXT
/** Opcode VEX.66.0F 0x17 - vmovhpdv1 Mq, Vq   */
FNIEMOP_STUB(iemOp_vmovhpdv1_Mq_Vq);  //NEXT
/*  Opcode VEX.F3.0F 0x17 - invalid */
/*  Opcode VEX.F2.0F 0x17 - invalid */


/*  Opcode VEX.0F 0x18 - invalid */
/*  Opcode VEX.0F 0x19 - invalid */
/*  Opcode VEX.0F 0x1a - invalid */
/*  Opcode VEX.0F 0x1b - invalid */
/*  Opcode VEX.0F 0x1c - invalid */
/*  Opcode VEX.0F 0x1d - invalid */
/*  Opcode VEX.0F 0x1e - invalid */
/*  Opcode VEX.0F 0x1f - invalid */

/*  Opcode VEX.0F 0x20 - invalid */
/*  Opcode VEX.0F 0x21 - invalid */
/*  Opcode VEX.0F 0x22 - invalid */
/*  Opcode VEX.0F 0x23 - invalid */
/*  Opcode VEX.0F 0x24 - invalid */
/*  Opcode VEX.0F 0x25 - invalid */
/*  Opcode VEX.0F 0x26 - invalid */
/*  Opcode VEX.0F 0x27 - invalid */

/** Opcode VEX.0F 0x28 - vmovaps Vps, Wps */
FNIEMOP_STUB(iemOp_vmovaps_Vps_Wps);
//FNIEMOP_DEF(iemOp_vmovaps_Vps_Wps)
//{
//    IEMOP_MNEMONIC(vmovaps_Vps_Wps, "vmovaps Vps,Wps");
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(0, 0);
//        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//        IEM_MC_COPY_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
//                              (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Register, memory.
//         */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(RTUINT128U,                uSrc); /** @todo optimize this one day... */
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//
//        IEM_MC_FETCH_MEM_U128_ALIGN_SSE(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
//        IEM_MC_STORE_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}

/** Opcode VEX.66.0F 0x28 - vmovapd Vpd, Wpd */
FNIEMOP_STUB(iemOp_vmovapd_Vpd_Wpd);
//FNIEMOP_DEF(iemOp_vmovapd_Vpd_Wpd)
//{
//    IEMOP_MNEMONIC(vmovapd_Wpd_Wpd, "vmovapd Wpd,Wpd");
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(0, 0);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//        IEM_MC_COPY_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
//                              (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Register, memory.
//         */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(RTUINT128U,                uSrc); /** @todo optimize this one day... */
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//
//        IEM_MC_FETCH_MEM_U128_ALIGN_SSE(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
//        IEM_MC_STORE_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}

/*  Opcode VEX.F3.0F 0x28 - invalid */
/*  Opcode VEX.F2.0F 0x28 - invalid */

/** Opcode VEX.0F 0x29 - vmovaps Wps, Vps */
FNIEMOP_STUB(iemOp_vmovaps_Wps_Vps);
//FNIEMOP_DEF(iemOp_vmovaps_Wps_Vps)
//{
//    IEMOP_MNEMONIC(vmovaps_Wps_Vps, "vmovaps Wps,Vps");
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(0, 0);
//        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//        IEM_MC_COPY_XREG_U128((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,
//                              ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Memory, register.
//         */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(RTUINT128U,                uSrc); /** @todo optimize this one day... */
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();
//
//        IEM_MC_FETCH_XREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_STORE_MEM_U128_ALIGN_SSE(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}

/** Opcode VEX.66.0F 0x29 - vmovapd Wpd,Vpd */
FNIEMOP_STUB(iemOp_vmovapd_Wpd_Vpd);
//FNIEMOP_DEF(iemOp_vmovapd_Wpd_Vpd)
//{
//    IEMOP_MNEMONIC(vmovapd_Wpd_Vpd, "movapd Wpd,Vpd");
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(0, 0);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//        IEM_MC_COPY_XREG_U128((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,
//                              ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Memory, register.
//         */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(RTUINT128U,                uSrc); /** @todo optimize this one day... */
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();
//
//        IEM_MC_FETCH_XREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_STORE_MEM_U128_ALIGN_SSE(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}

/*  Opcode VEX.F3.0F 0x29 - invalid */
/*  Opcode VEX.F2.0F 0x29 - invalid */


/** Opcode VEX.0F 0x2a - invalid */
/** Opcode VEX.66.0F 0x2a - invalid */
/** Opcode VEX.F3.0F 0x2a - vcvtsi2ss Vss, Hss, Ey */
FNIEMOP_STUB(iemOp_vcvtsi2ss_Vss_Hss_Ey);
/** Opcode VEX.F2.0F 0x2a - vcvtsi2sd Vsd, Hsd, Ey */
FNIEMOP_STUB(iemOp_vcvtsi2sd_Vsd_Hsd_Ey);


/** Opcode VEX.0F 0x2b - vmovntps Mps, Vps */
FNIEMOP_STUB(iemOp_vmovntps_Mps_Vps);
//FNIEMOP_DEF(iemOp_vmovntps_Mps_Vps)
//{
//    IEMOP_MNEMONIC(vmovntps_Mps_Vps, "movntps Mps,Vps");
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * memory, register.
//         */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(RTUINT128U,                uSrc); /** @todo optimize this one day... */
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//
//        IEM_MC_FETCH_XREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_STORE_MEM_U128_ALIGN_SSE(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    /* The register, register encoding is invalid. */
//    else
//        return IEMOP_RAISE_INVALID_OPCODE();
//    return VINF_SUCCESS;
//}

/** Opcode VEX.66.0F 0x2b - vmovntpd Mpd, Vpd */
FNIEMOP_STUB(iemOp_vmovntpd_Mpd_Vpd);
//FNIEMOP_DEF(iemOp_vmovntpd_Mpd_Vpd)
//{
//    IEMOP_MNEMONIC(vmovntpd_Mpd_Vpd, "movntpd Mdq,Vpd");
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * memory, register.
//         */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(RTUINT128U,                uSrc); /** @todo optimize this one day... */
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//
//        IEM_MC_FETCH_XREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_STORE_MEM_U128_ALIGN_SSE(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    /* The register, register encoding is invalid. */
//    else
//        return IEMOP_RAISE_INVALID_OPCODE();
//    return VINF_SUCCESS;
//}
/*  Opcode VEX.F3.0F 0x2b - invalid */
/*  Opcode VEX.F2.0F 0x2b - invalid */


/*  Opcode VEX.0F 0x2c - invalid */
/*  Opcode VEX.66.0F 0x2c - invalid */
/** Opcode VEX.F3.0F 0x2c - vcvttss2si Gy, Wss */
FNIEMOP_STUB(iemOp_vcvttss2si_Gy_Wss);
/** Opcode VEX.F2.0F 0x2c - vcvttsd2si Gy, Wsd */
FNIEMOP_STUB(iemOp_vcvttsd2si_Gy_Wsd);

/*  Opcode VEX.0F 0x2d - invalid */
/*  Opcode VEX.66.0F 0x2d - invalid */
/** Opcode VEX.F3.0F 0x2d - vcvtss2si Gy, Wss */
FNIEMOP_STUB(iemOp_vcvtss2si_Gy_Wss);
/** Opcode VEX.F2.0F 0x2d - vcvtsd2si Gy, Wsd */
FNIEMOP_STUB(iemOp_vcvtsd2si_Gy_Wsd);

/** Opcode VEX.0F 0x2e - vucomiss Vss, Wss */
FNIEMOP_STUB(iemOp_vucomiss_Vss_Wss);
/** Opcode VEX.66.0F 0x2e - vucomisd Vsd, Wsd */
FNIEMOP_STUB(iemOp_vucomisd_Vsd_Wsd);
/*  Opcode VEX.F3.0F 0x2e - invalid */
/*  Opcode VEX.F2.0F 0x2e - invalid */

/** Opcode VEX.0F 0x2f - vcomiss Vss, Wss */
FNIEMOP_STUB(iemOp_vcomiss_Vss_Wss);
/** Opcode VEX.66.0F 0x2f - vcomisd Vsd, Wsd */
FNIEMOP_STUB(iemOp_vcomisd_Vsd_Wsd);
/*  Opcode VEX.F3.0F 0x2f - invalid */
/*  Opcode VEX.F2.0F 0x2f - invalid */

/*  Opcode VEX.0F 0x30 - invalid */
/*  Opcode VEX.0F 0x31 - invalid */
/*  Opcode VEX.0F 0x32 - invalid */
/*  Opcode VEX.0F 0x33 - invalid */
/*  Opcode VEX.0F 0x34 - invalid */
/*  Opcode VEX.0F 0x35 - invalid */
/*  Opcode VEX.0F 0x36 - invalid */
/*  Opcode VEX.0F 0x37 - invalid */
/*  Opcode VEX.0F 0x38 - invalid */
/*  Opcode VEX.0F 0x39 - invalid */
/*  Opcode VEX.0F 0x3a - invalid */
/*  Opcode VEX.0F 0x3b - invalid */
/*  Opcode VEX.0F 0x3c - invalid */
/*  Opcode VEX.0F 0x3d - invalid */
/*  Opcode VEX.0F 0x3e - invalid */
/*  Opcode VEX.0F 0x3f - invalid */
/*  Opcode VEX.0F 0x40 - invalid */
/*  Opcode VEX.0F 0x41 - invalid */
/*  Opcode VEX.0F 0x42 - invalid */
/*  Opcode VEX.0F 0x43 - invalid */
/*  Opcode VEX.0F 0x44 - invalid */
/*  Opcode VEX.0F 0x45 - invalid */
/*  Opcode VEX.0F 0x46 - invalid */
/*  Opcode VEX.0F 0x47 - invalid */
/*  Opcode VEX.0F 0x48 - invalid */
/*  Opcode VEX.0F 0x49 - invalid */
/*  Opcode VEX.0F 0x4a - invalid */
/*  Opcode VEX.0F 0x4b - invalid */
/*  Opcode VEX.0F 0x4c - invalid */
/*  Opcode VEX.0F 0x4d - invalid */
/*  Opcode VEX.0F 0x4e - invalid */
/*  Opcode VEX.0F 0x4f - invalid */

/** Opcode VEX.0F 0x50 - vmovmskps Gy, Ups */
FNIEMOP_STUB(iemOp_vmovmskps_Gy_Ups);
/** Opcode VEX.66.0F 0x50 - vmovmskpd Gy,Upd */
FNIEMOP_STUB(iemOp_vmovmskpd_Gy_Upd);
/*  Opcode VEX.F3.0F 0x50 - invalid */
/*  Opcode VEX.F2.0F 0x50 - invalid */

/** Opcode VEX.0F 0x51 - vsqrtps Vps, Wps */
FNIEMOP_STUB(iemOp_vsqrtps_Vps_Wps);
/** Opcode VEX.66.0F 0x51 - vsqrtpd Vpd, Wpd */
FNIEMOP_STUB(iemOp_vsqrtpd_Vpd_Wpd);
/** Opcode VEX.F3.0F 0x51 - vsqrtss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vsqrtss_Vss_Hss_Wss);
/** Opcode VEX.F2.0F 0x51 - vsqrtsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vsqrtsd_Vsd_Hsd_Wsd);

/** Opcode VEX.0F 0x52 - vrsqrtps Vps, Wps */
FNIEMOP_STUB(iemOp_vrsqrtps_Vps_Wps);
/*  Opcode VEX.66.0F 0x52 - invalid */
/** Opcode VEX.F3.0F 0x52 - vrsqrtss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vrsqrtss_Vss_Hss_Wss);
/*  Opcode VEX.F2.0F 0x52 - invalid */

/** Opcode VEX.0F 0x53 - vrcpps Vps, Wps */
FNIEMOP_STUB(iemOp_vrcpps_Vps_Wps);
/*  Opcode VEX.66.0F 0x53 - invalid */
/** Opcode VEX.F3.0F 0x53 - vrcpss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vrcpss_Vss_Hss_Wss);
/*  Opcode VEX.F2.0F 0x53 - invalid */

/** Opcode VEX.0F 0x54 - vandps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vandps_Vps_Hps_Wps);
/** Opcode VEX.66.0F 0x54 - vandpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vandpd_Vpd_Hpd_Wpd);
/*  Opcode VEX.F3.0F 0x54 - invalid */
/*  Opcode VEX.F2.0F 0x54 - invalid */

/** Opcode VEX.0F 0x55 - vandnps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vandnps_Vps_Hps_Wps);
/** Opcode VEX.66.0F 0x55 - vandnpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vandnpd_Vpd_Hpd_Wpd);
/*  Opcode VEX.F3.0F 0x55 - invalid */
/*  Opcode VEX.F2.0F 0x55 - invalid */

/** Opcode VEX.0F 0x56 - vorps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vorps_Vps_Hps_Wps);
/** Opcode VEX.66.0F 0x56 - vorpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vorpd_Vpd_Hpd_Wpd);
/*  Opcode VEX.F3.0F 0x56 - invalid */
/*  Opcode VEX.F2.0F 0x56 - invalid */

/** Opcode VEX.0F 0x57 - vxorps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vxorps_Vps_Hps_Wps);
/** Opcode VEX.66.0F 0x57 - vxorpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vxorpd_Vpd_Hpd_Wpd);
/*  Opcode VEX.F3.0F 0x57 - invalid */
/*  Opcode VEX.F2.0F 0x57 - invalid */

/** Opcode VEX.0F 0x58 - vaddps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vaddps_Vps_Hps_Wps);
/** Opcode VEX.66.0F 0x58 - vaddpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vaddpd_Vpd_Hpd_Wpd);
/** Opcode VEX.F3.0F 0x58 - vaddss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vaddss_Vss_Hss_Wss);
/** Opcode VEX.F2.0F 0x58 - vaddsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vaddsd_Vsd_Hsd_Wsd);

/** Opcode VEX.0F 0x59 - vmulps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vmulps_Vps_Hps_Wps);
/** Opcode VEX.66.0F 0x59 - vmulpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vmulpd_Vpd_Hpd_Wpd);
/** Opcode VEX.F3.0F 0x59 - vmulss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vmulss_Vss_Hss_Wss);
/** Opcode VEX.F2.0F 0x59 - vmulsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vmulsd_Vsd_Hsd_Wsd);

/** Opcode VEX.0F 0x5a - vcvtps2pd Vpd, Wps */
FNIEMOP_STUB(iemOp_vcvtps2pd_Vpd_Wps);
/** Opcode VEX.66.0F 0x5a - vcvtpd2ps Vps, Wpd */
FNIEMOP_STUB(iemOp_vcvtpd2ps_Vps_Wpd);
/** Opcode VEX.F3.0F 0x5a - vcvtss2sd Vsd, Hx, Wss */
FNIEMOP_STUB(iemOp_vcvtss2sd_Vsd_Hx_Wss);
/** Opcode VEX.F2.0F 0x5a - vcvtsd2ss Vss, Hx, Wsd */
FNIEMOP_STUB(iemOp_vcvtsd2ss_Vss_Hx_Wsd);

/** Opcode VEX.0F 0x5b - vcvtdq2ps Vps, Wdq */
FNIEMOP_STUB(iemOp_vcvtdq2ps_Vps_Wdq);
/** Opcode VEX.66.0F 0x5b - vcvtps2dq Vdq, Wps */
FNIEMOP_STUB(iemOp_vcvtps2dq_Vdq_Wps);
/** Opcode VEX.F3.0F 0x5b - vcvttps2dq Vdq, Wps */
FNIEMOP_STUB(iemOp_vcvttps2dq_Vdq_Wps);
/*  Opcode VEX.F2.0F 0x5b - invalid */

/** Opcode VEX.0F 0x5c - vsubps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vsubps_Vps_Hps_Wps);
/** Opcode VEX.66.0F 0x5c - vsubpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vsubpd_Vpd_Hpd_Wpd);
/** Opcode VEX.F3.0F 0x5c - vsubss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vsubss_Vss_Hss_Wss);
/** Opcode VEX.F2.0F 0x5c - vsubsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vsubsd_Vsd_Hsd_Wsd);

/** Opcode VEX.0F 0x5d - vminps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vminps_Vps_Hps_Wps);
/** Opcode VEX.66.0F 0x5d - vminpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vminpd_Vpd_Hpd_Wpd);
/** Opcode VEX.F3.0F 0x5d - vminss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vminss_Vss_Hss_Wss);
/** Opcode VEX.F2.0F 0x5d - vminsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vminsd_Vsd_Hsd_Wsd);

/** Opcode VEX.0F 0x5e - vdivps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vdivps_Vps_Hps_Wps);
/** Opcode VEX.66.0F 0x5e - vdivpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vdivpd_Vpd_Hpd_Wpd);
/** Opcode VEX.F3.0F 0x5e - vdivss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vdivss_Vss_Hss_Wss);
/** Opcode VEX.F2.0F 0x5e - vdivsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vdivsd_Vsd_Hsd_Wsd);

/** Opcode VEX.0F 0x5f - vmaxps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vmaxps_Vps_Hps_Wps);
/** Opcode VEX.66.0F 0x5f - vmaxpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vmaxpd_Vpd_Hpd_Wpd);
/** Opcode VEX.F3.0F 0x5f - vmaxss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vmaxss_Vss_Hss_Wss);
/** Opcode VEX.F2.0F 0x5f - vmaxsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vmaxsd_Vsd_Hsd_Wsd);


///**
// * Common worker for SSE2 instructions on the forms:
// *      pxxxx xmm1, xmm2/mem128
// *
// * The 2nd operand is the first half of a register, which in the memory case
// * means a 32-bit memory access for MMX and 128-bit aligned 64-bit or 128-bit
// * memory accessed for MMX.
// *
// * Exceptions type 4.
// */
//FNIEMOP_DEF_1(iemOpCommonSse_LowLow_To_Full, PCIEMOPMEDIAF1L1, pImpl)
//{
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if (!pImpl->pfnU64)
//        return IEMOP_RAISE_INVALID_OPCODE();
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        /** @todo testcase: REX.B / REX.R and MMX register indexing. Ignored? */
//        /** @todo testcase: REX.B / REX.R and segment register indexing. Ignored? */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(2, 0);
//        IEM_MC_ARG(uint64_t *,          pDst, 0);
//        IEM_MC_ARG(uint32_t const *,    pSrc, 1);
//        IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
//        IEM_MC_PREPARE_FPU_USAGE();
//        IEM_MC_REF_MREG_U64(pDst, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
//        IEM_MC_REF_MREG_U32_CONST(pSrc, bRm & X86_MODRM_RM_MASK);
//        IEM_MC_CALL_MMX_AIMPL_2(pImpl->pfnU64, pDst, pSrc);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Register, memory.
//         */
//        IEM_MC_BEGIN(2, 2);
//        IEM_MC_ARG(uint64_t *,                  pDst,       0);
//        IEM_MC_LOCAL(uint32_t,                  uSrc);
//        IEM_MC_ARG_LOCAL_REF(uint32_t const *,  pSrc, uSrc, 1);
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
//        IEM_MC_FETCH_MEM_U32(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
//
//        IEM_MC_PREPARE_FPU_USAGE();
//        IEM_MC_REF_MREG_U64(pDst, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
//        IEM_MC_CALL_MMX_AIMPL_2(pImpl->pfnU64, pDst, pSrc);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}


/*  Opcode VEX.0F 0x60 - invalid */

/** Opcode VEX.66.0F 0x60 - vpunpcklbw Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpunpcklbw_Vx_Hx_Wx);
//FNIEMOP_DEF(iemOp_vpunpcklbw_Vx_Hx_Wx)
//{
//    IEMOP_MNEMONIC(vpunpcklbw, "vpunpcklbw Vx, Hx, Wx");
//    return FNIEMOP_CALL_1(iemOpCommonSse_LowLow_To_Full, &g_iemAImpl_punpcklbw);
//}

/*  Opcode VEX.F3.0F 0x60 - invalid */


/*  Opcode VEX.0F 0x61 - invalid */

/** Opcode VEX.66.0F 0x61 - vpunpcklwd Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpunpcklwd_Vx_Hx_Wx);
//FNIEMOP_DEF(iemOp_vpunpcklwd_Vx_Hx_Wx)
//{
//    IEMOP_MNEMONIC(vpunpcklwd, "vpunpcklwd Vx, Hx, Wx");
//    return FNIEMOP_CALL_1(iemOpCommonSse_LowLow_To_Full, &g_iemAImpl_punpcklwd);
//}

/*  Opcode VEX.F3.0F 0x61 - invalid */


/*  Opcode VEX.0F 0x62 - invalid */

/** Opcode VEX.66.0F 0x62 - vpunpckldq Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpunpckldq_Vx_Hx_Wx);
//FNIEMOP_DEF(iemOp_vpunpckldq_Vx_Hx_Wx)
//{
//    IEMOP_MNEMONIC(vpunpckldq, "vpunpckldq Vx, Hx, Wx");
//    return FNIEMOP_CALL_1(iemOpCommonSse_LowLow_To_Full, &g_iemAImpl_punpckldq);
//}

/*  Opcode VEX.F3.0F 0x62 - invalid */



/*  Opcode VEX.0F 0x63 - invalid */
/** Opcode VEX.66.0F 0x63 - vpacksswb Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpacksswb_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0x63 - invalid */

/*  Opcode VEX.0F 0x64 - invalid */
/** Opcode VEX.66.0F 0x64 - vpcmpgtb Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpcmpgtb_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0x64 - invalid */

/*  Opcode VEX.0F 0x65 - invalid */
/** Opcode VEX.66.0F 0x65 - vpcmpgtw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpcmpgtw_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0x65 - invalid */

/*  Opcode VEX.0F 0x66 - invalid */
/** Opcode VEX.66.0F 0x66 - vpcmpgtd Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpcmpgtd_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0x66 - invalid */

/*  Opcode VEX.0F 0x67 - invalid */
/** Opcode VEX.66.0F 0x67 - vpackuswb Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpackuswb_Vx_Hx_W);
/*  Opcode VEX.F3.0F 0x67 - invalid */


///**
// * Common worker for SSE2 instructions on the form:
// *      pxxxx xmm1, xmm2/mem128
// *
// * The 2nd operand is the second half of a register, which in the memory case
// * means a 64-bit memory access for MMX, and for SSE a 128-bit aligned access
// * where it may read the full 128 bits or only the upper 64 bits.
// *
// * Exceptions type 4.
// */
//FNIEMOP_DEF_1(iemOpCommonSse_HighHigh_To_Full, PCIEMOPMEDIAF1H1, pImpl)
//{
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(2, 0);
//        IEM_MC_ARG(PRTUINT128U,          pDst, 0);
//        IEM_MC_ARG(PCRTUINT128U,         pSrc, 1);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_PREPARE_SSE_USAGE();
//        IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_REF_XREG_U128_CONST(pSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
//        IEM_MC_CALL_SSE_AIMPL_2(pImpl->pfnU128, pDst, pSrc);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Register, memory.
//         */
//        IEM_MC_BEGIN(2, 2);
//        IEM_MC_ARG(PRTUINT128U,                 pDst,       0);
//        IEM_MC_LOCAL(RTUINT128U,                uSrc);
//        IEM_MC_ARG_LOCAL_REF(PCRTUINT128U,      pSrc, uSrc, 1);
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_FETCH_MEM_U128_ALIGN_SSE(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc); /* Most CPUs probably only right high qword */
//
//        IEM_MC_PREPARE_SSE_USAGE();
//        IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_CALL_SSE_AIMPL_2(pImpl->pfnU128, pDst, pSrc);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}


/*  Opcode VEX.0F 0x68 - invalid */

/** Opcode VEX.66.0F 0x68 - vpunpckhbw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpunpckhbw_Vx_Hx_Wx);
//FNIEMOP_DEF(iemOp_vpunpckhbw_Vx_Hx_Wx)
//{
//    IEMOP_MNEMONIC(vpunpckhbw, "vpunpckhbw Vx, Hx, Wx");
//    return FNIEMOP_CALL_1(iemOpCommonSse_HighHigh_To_Full, &g_iemAImpl_punpckhbw);
//}
/*  Opcode VEX.F3.0F 0x68 - invalid */


/*  Opcode VEX.0F 0x69 - invalid */

/** Opcode VEX.66.0F 0x69 - vpunpckhwd Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpunpckhwd_Vx_Hx_Wx);
//FNIEMOP_DEF(iemOp_vpunpckhwd_Vx_Hx_Wx)
//{
//    IEMOP_MNEMONIC(vpunpckhwd, "vpunpckhwd Vx, Hx, Wx");
//    return FNIEMOP_CALL_1(iemOpCommonSse_HighHigh_To_Full, &g_iemAImpl_punpckhwd);
//
//}
/*  Opcode VEX.F3.0F 0x69 - invalid */


/*  Opcode VEX.0F 0x6a - invalid */

/** Opcode VEX.66.0F 0x6a - vpunpckhdq Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpunpckhdq_Vx_Hx_W);
//FNIEMOP_DEF(iemOp_vpunpckhdq_Vx_Hx_W)
//{
//    IEMOP_MNEMONIC(vpunpckhdq, "vpunpckhdq Vx, Hx, W");
//    return FNIEMOP_CALL_1(iemOpCommonSse_HighHigh_To_Full, &g_iemAImpl_punpckhdq);
//}
/*  Opcode VEX.F3.0F 0x6a - invalid */


/*  Opcode VEX.0F 0x6b - invalid */
/** Opcode VEX.66.0F 0x6b - vpackssdw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpackssdw_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0x6b - invalid */


/*  Opcode VEX.0F 0x6c - invalid */

/** Opcode VEX.66.0F 0x6c - vpunpcklqdq Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpunpcklqdq_Vx_Hx_Wx);
//FNIEMOP_DEF(iemOp_vpunpcklqdq_Vx_Hx_Wx)
//{
//    IEMOP_MNEMONIC(vpunpcklqdq, "vpunpcklqdq Vx, Hx, Wx");
//    return FNIEMOP_CALL_1(iemOpCommonSse_LowLow_To_Full, &g_iemAImpl_punpcklqdq);
//}

/*  Opcode VEX.F3.0F 0x6c - invalid */
/*  Opcode VEX.F2.0F 0x6c - invalid */


/*  Opcode VEX.0F 0x6d - invalid */

/** Opcode VEX.66.0F 0x6d - vpunpckhqdq Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpunpckhqdq_Vx_Hx_W);
//FNIEMOP_DEF(iemOp_vpunpckhqdq_Vx_Hx_W)
//{
//    IEMOP_MNEMONIC(punpckhqdq, "punpckhqdq");
//    return FNIEMOP_CALL_1(iemOpCommonSse_HighHigh_To_Full, &g_iemAImpl_punpckhqdq);
//}

/*  Opcode VEX.F3.0F 0x6d - invalid */


/*  Opcode VEX.0F 0x6e - invalid */

/** Opcode VEX.66.0F 0x6e - vmovd/q Vy, Ey */
FNIEMOP_STUB(iemOp_vmovd_q_Vy_Ey);
//FNIEMOP_DEF(iemOp_vmovd_q_Vy_Ey)
//{
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
//        IEMOP_MNEMONIC(vmovdq_Wq_Eq, "vmovq Wq,Eq");
//    else
//        IEMOP_MNEMONIC(vmovdq_Wd_Ed, "vmovd Wd,Ed");
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /* XMM, greg*/
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(0, 1);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//        if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
//        {
//            IEM_MC_LOCAL(uint64_t, u64Tmp);
//            IEM_MC_FETCH_GREG_U64(u64Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
//            IEM_MC_STORE_XREG_U64_ZX_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Tmp);
//        }
//        else
//        {
//            IEM_MC_LOCAL(uint32_t, u32Tmp);
//            IEM_MC_FETCH_GREG_U32(u32Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
//            IEM_MC_STORE_XREG_U32_ZX_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Tmp);
//        }
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /* XMM, [mem] */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT(); /** @todo order */
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 1);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//        if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
//        {
//            IEM_MC_LOCAL(uint64_t, u64Tmp);
//            IEM_MC_FETCH_MEM_U64(u64Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
//            IEM_MC_STORE_XREG_U64_ZX_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Tmp);
//        }
//        else
//        {
//            IEM_MC_LOCAL(uint32_t, u32Tmp);
//            IEM_MC_FETCH_MEM_U32(u32Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
//            IEM_MC_STORE_XREG_U32_ZX_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Tmp);
//        }
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}

/*  Opcode VEX.F3.0F 0x6e - invalid */


/*  Opcode VEX.0F 0x6f - invalid */

/** Opcode VEX.66.0F 0x6f - vmovdqa Vx, Wx */
FNIEMOP_STUB(iemOp_vmovdqa_Vx_Wx);
//FNIEMOP_DEF(iemOp_vmovdqa_Vx_Wx)
//{
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    IEMOP_MNEMONIC(vmovdqa_Vdq_Wdq, "movdqa Vdq,Wdq");
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(0, 0);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//        IEM_MC_COPY_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
//                              (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Register, memory.
//         */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(RTUINT128U, u128Tmp);
//        IEM_MC_LOCAL(RTGCPTR,    GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//        IEM_MC_FETCH_MEM_U128_ALIGN_SSE(u128Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
//        IEM_MC_STORE_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u128Tmp);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}

/** Opcode VEX.F3.0F 0x6f - vmovdqu Vx, Wx */
FNIEMOP_STUB(iemOp_vmovdqu_Vx_Wx);
//FNIEMOP_DEF(iemOp_vmovdqu_Vx_Wx)
//{
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    IEMOP_MNEMONIC(vmovdqu_Vdq_Wdq, "movdqu Vdq,Wdq");
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(0, 0);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//        IEM_MC_COPY_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
//                              (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Register, memory.
//         */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(RTUINT128U, u128Tmp);
//        IEM_MC_LOCAL(RTGCPTR,    GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//        IEM_MC_FETCH_MEM_U128(u128Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
//        IEM_MC_STORE_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u128Tmp);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}


/*  Opcode VEX.0F 0x70 - invalid */

/** Opcode VEX.66.0F 0x70 - vpshufd Vx, Wx, Ib */
FNIEMOP_STUB(iemOp_vpshufd_Vx_Wx_Ib);
//FNIEMOP_DEF(iemOp_vpshufd_Vx_Wx_Ib)
//{
//    IEMOP_MNEMONIC(vpshufd_Vx_Wx_Ib, "vpshufd Vx,Wx,Ib");
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        uint8_t bEvil; IEM_OPCODE_GET_NEXT_U8(&bEvil);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//
//        IEM_MC_BEGIN(3, 0);
//        IEM_MC_ARG(PRTUINT128U,         pDst, 0);
//        IEM_MC_ARG(PCRTUINT128U,        pSrc, 1);
//        IEM_MC_ARG_CONST(uint8_t,       bEvilArg, /*=*/ bEvil, 2);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_PREPARE_SSE_USAGE();
//        IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_REF_XREG_U128_CONST(pSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
//        IEM_MC_CALL_SSE_AIMPL_3(iemAImpl_pshufd, pDst, pSrc, bEvilArg);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Register, memory.
//         */
//        IEM_MC_BEGIN(3, 2);
//        IEM_MC_ARG(PRTUINT128U,                 pDst,       0);
//        IEM_MC_LOCAL(RTUINT128U,                uSrc);
//        IEM_MC_ARG_LOCAL_REF(PCRTUINT128U,      pSrc, uSrc, 1);
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        uint8_t bEvil; IEM_OPCODE_GET_NEXT_U8(&bEvil);
//        IEM_MC_ARG_CONST(uint8_t,               bEvilArg, /*=*/ bEvil, 2);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//
//        IEM_MC_FETCH_MEM_U128_ALIGN_SSE(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
//        IEM_MC_PREPARE_SSE_USAGE();
//        IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_CALL_SSE_AIMPL_3(iemAImpl_pshufd, pDst, pSrc, bEvilArg);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}

/** Opcode VEX.F3.0F 0x70 - vpshufhw Vx, Wx, Ib */
FNIEMOP_STUB(iemOp_vpshufhw_Vx_Wx_Ib);
//FNIEMOP_DEF(iemOp_vpshufhw_Vx_Wx_Ib)
//{
//    IEMOP_MNEMONIC(vpshufhw_Vx_Wx_Ib, "vpshufhw Vx,Wx,Ib");
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        uint8_t bEvil; IEM_OPCODE_GET_NEXT_U8(&bEvil);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//
//        IEM_MC_BEGIN(3, 0);
//        IEM_MC_ARG(PRTUINT128U,         pDst, 0);
//        IEM_MC_ARG(PCRTUINT128U,        pSrc, 1);
//        IEM_MC_ARG_CONST(uint8_t,       bEvilArg, /*=*/ bEvil, 2);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_PREPARE_SSE_USAGE();
//        IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_REF_XREG_U128_CONST(pSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
//        IEM_MC_CALL_SSE_AIMPL_3(iemAImpl_pshufhw, pDst, pSrc, bEvilArg);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Register, memory.
//         */
//        IEM_MC_BEGIN(3, 2);
//        IEM_MC_ARG(PRTUINT128U,                 pDst,       0);
//        IEM_MC_LOCAL(RTUINT128U,                uSrc);
//        IEM_MC_ARG_LOCAL_REF(PCRTUINT128U,      pSrc, uSrc, 1);
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        uint8_t bEvil; IEM_OPCODE_GET_NEXT_U8(&bEvil);
//        IEM_MC_ARG_CONST(uint8_t,               bEvilArg, /*=*/ bEvil, 2);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//
//        IEM_MC_FETCH_MEM_U128_ALIGN_SSE(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
//        IEM_MC_PREPARE_SSE_USAGE();
//        IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_CALL_SSE_AIMPL_3(iemAImpl_pshufhw, pDst, pSrc, bEvilArg);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}

/** Opcode VEX.F2.0F 0x70 - vpshuflw Vx, Wx, Ib */
FNIEMOP_STUB(iemOp_vpshuflw_Vx_Wx_Ib);
//FNIEMOP_DEF(iemOp_vpshuflw_Vx_Wx_Ib)
//{
//    IEMOP_MNEMONIC(vpshuflw_Vx_Wx_Ib, "vpshuflw Vx,Wx,Ib");
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        uint8_t bEvil; IEM_OPCODE_GET_NEXT_U8(&bEvil);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//
//        IEM_MC_BEGIN(3, 0);
//        IEM_MC_ARG(PRTUINT128U,         pDst, 0);
//        IEM_MC_ARG(PCRTUINT128U,        pSrc, 1);
//        IEM_MC_ARG_CONST(uint8_t,       bEvilArg, /*=*/ bEvil, 2);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_PREPARE_SSE_USAGE();
//        IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_REF_XREG_U128_CONST(pSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
//        IEM_MC_CALL_SSE_AIMPL_3(iemAImpl_pshuflw, pDst, pSrc, bEvilArg);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Register, memory.
//         */
//        IEM_MC_BEGIN(3, 2);
//        IEM_MC_ARG(PRTUINT128U,                 pDst,       0);
//        IEM_MC_LOCAL(RTUINT128U,                uSrc);
//        IEM_MC_ARG_LOCAL_REF(PCRTUINT128U,      pSrc, uSrc, 1);
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        uint8_t bEvil; IEM_OPCODE_GET_NEXT_U8(&bEvil);
//        IEM_MC_ARG_CONST(uint8_t,               bEvilArg, /*=*/ bEvil, 2);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//
//        IEM_MC_FETCH_MEM_U128_ALIGN_SSE(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
//        IEM_MC_PREPARE_SSE_USAGE();
//        IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_CALL_SSE_AIMPL_3(iemAImpl_pshuflw, pDst, pSrc, bEvilArg);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}


/*  Opcode VEX.0F 0x71 11/2 - invalid. */
/** Opcode VEX.66.0F 0x71 11/2. */
FNIEMOP_STUB_1(iemOp_VGrp12_vpsrlw_Hx_Ux_Ib, uint8_t, bRm);

/*  Opcode VEX.0F 0x71 11/4 - invalid */
/** Opcode VEX.66.0F 0x71 11/4. */
FNIEMOP_STUB_1(iemOp_VGrp12_vpsraw_Hx_Ux_Ib, uint8_t, bRm);

/*  Opcode VEX.0F 0x71 11/6 - invalid */
/** Opcode VEX.66.0F 0x71 11/6. */
FNIEMOP_STUB_1(iemOp_VGrp12_vpsllw_Hx_Ux_Ib, uint8_t, bRm);


/**
 * VEX Group 12 jump table for register variant.
 */
IEM_STATIC const PFNIEMOPRM g_apfnVexGroup12RegReg[] =
{
    /* /0 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /1 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /2 */ iemOp_InvalidWithRMNeedImm8,   iemOp_VGrp12_vpsrlw_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /3 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /4 */ iemOp_InvalidWithRMNeedImm8,   iemOp_VGrp12_vpsraw_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /5 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /6 */ iemOp_InvalidWithRMNeedImm8,   iemOp_VGrp12_vpsllw_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /7 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8)
};
AssertCompile(RT_ELEMENTS(g_apfnVexGroup12RegReg) == 8*4);


/** Opcode VEX.0F 0x71. */
FNIEMOP_DEF(iemOp_VGrp12)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        /* register, register */
        return FNIEMOP_CALL_1(g_apfnVexGroup12RegReg[  ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) * 4
                                                     + pVCpu->iem.s.idxPrefix], bRm);
    return FNIEMOP_CALL_1(iemOp_InvalidWithRMNeedImm8, bRm);
}


/*  Opcode VEX.0F 0x72 11/2 - invalid. */
/** Opcode VEX.66.0F 0x72 11/2. */
FNIEMOP_STUB_1(iemOp_VGrp13_vpsrld_Hx_Ux_Ib, uint8_t, bRm);

/*  Opcode VEX.0F 0x72 11/4 - invalid. */
/** Opcode VEX.66.0F 0x72 11/4. */
FNIEMOP_STUB_1(iemOp_VGrp13_vpsrad_Hx_Ux_Ib, uint8_t, bRm);

/*  Opcode VEX.0F 0x72 11/6 - invalid. */
/** Opcode VEX.66.0F 0x72 11/6. */
FNIEMOP_STUB_1(iemOp_VGrp13_vpslld_Hx_Ux_Ib, uint8_t, bRm);


/**
 * Group 13 jump table for register variant.
 */
IEM_STATIC const PFNIEMOPRM g_apfnVexGroup13RegReg[] =
{
    /* /0 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /1 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /2 */ iemOp_InvalidWithRMNeedImm8,   iemOp_VGrp13_vpsrld_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /3 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /4 */ iemOp_InvalidWithRMNeedImm8,   iemOp_VGrp13_vpsrad_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /5 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /6 */ iemOp_InvalidWithRMNeedImm8,   iemOp_VGrp13_vpslld_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /7 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8)
};
AssertCompile(RT_ELEMENTS(g_apfnVexGroup13RegReg) == 8*4);

/** Opcode VEX.0F 0x72. */
FNIEMOP_DEF(iemOp_VGrp13)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        /* register, register */
        return FNIEMOP_CALL_1(g_apfnVexGroup13RegReg[ ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) * 4
                                                     + pVCpu->iem.s.idxPrefix], bRm);
    return FNIEMOP_CALL_1(iemOp_InvalidWithRMNeedImm8, bRm);
}


/*  Opcode VEX.0F 0x73 11/2 - invalid. */
/** Opcode VEX.66.0F 0x73 11/2. */
FNIEMOP_STUB_1(iemOp_VGrp14_vpsrlq_Hx_Ux_Ib, uint8_t, bRm);

/** Opcode VEX.66.0F 0x73 11/3. */
FNIEMOP_STUB_1(iemOp_VGrp14_vpsrldq_Hx_Ux_Ib, uint8_t, bRm);

/*  Opcode VEX.0F 0x73 11/6 - invalid. */
/** Opcode VEX.66.0F 0x73 11/6. */
FNIEMOP_STUB_1(iemOp_VGrp14_vpsllq_Hx_Ux_Ib, uint8_t, bRm);

/** Opcode VEX.66.0F 0x73 11/7. */
FNIEMOP_STUB_1(iemOp_VGrp14_vpslldq_Hx_Ux_Ib, uint8_t, bRm);

/**
 * Group 14 jump table for register variant.
 */
IEM_STATIC const PFNIEMOPRM g_apfnVexGroup14RegReg[] =
{
    /* /0 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /1 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /2 */ iemOp_InvalidWithRMNeedImm8, iemOp_VGrp14_vpsrlq_Hx_Ux_Ib,  iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /3 */ iemOp_InvalidWithRMNeedImm8, iemOp_VGrp14_vpsrldq_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /4 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /5 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /6 */ iemOp_InvalidWithRMNeedImm8, iemOp_VGrp14_vpsllq_Hx_Ux_Ib,  iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /7 */ iemOp_InvalidWithRMNeedImm8, iemOp_VGrp14_vpslldq_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
};
AssertCompile(RT_ELEMENTS(g_apfnVexGroup14RegReg) == 8*4);


/** Opcode VEX.0F 0x73. */
FNIEMOP_DEF(iemOp_VGrp14)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        /* register, register */
        return FNIEMOP_CALL_1(g_apfnVexGroup14RegReg[ ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) * 4
                                                     + pVCpu->iem.s.idxPrefix], bRm);
    return FNIEMOP_CALL_1(iemOp_InvalidWithRMNeedImm8, bRm);
}


///**
// * Common worker for SSE2 instructions on the forms:
// *      pxxx    xmm1, xmm2/mem128
// *
// * Proper alignment of the 128-bit operand is enforced.
// * Exceptions type 4. SSE2 cpuid checks.
// */
//FNIEMOP_DEF_1(iemOpCommonSse2_FullFull_To_Full, PCIEMOPMEDIAF2, pImpl)
//{
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(2, 0);
//        IEM_MC_ARG(PRTUINT128U,          pDst, 0);
//        IEM_MC_ARG(PCRTUINT128U,         pSrc, 1);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_PREPARE_SSE_USAGE();
//        IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_REF_XREG_U128_CONST(pSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
//        IEM_MC_CALL_SSE_AIMPL_2(pImpl->pfnU128, pDst, pSrc);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Register, memory.
//         */
//        IEM_MC_BEGIN(2, 2);
//        IEM_MC_ARG(PRTUINT128U,                 pDst,       0);
//        IEM_MC_LOCAL(RTUINT128U,                uSrc);
//        IEM_MC_ARG_LOCAL_REF(PCRTUINT128U,      pSrc, uSrc, 1);
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_FETCH_MEM_U128_ALIGN_SSE(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
//
//        IEM_MC_PREPARE_SSE_USAGE();
//        IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_CALL_SSE_AIMPL_2(pImpl->pfnU128, pDst, pSrc);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}


/*  Opcode VEX.0F 0x74 - invalid */

/** Opcode VEX.66.0F 0x74 - vpcmpeqb Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpcmpeqb_Vx_Hx_Wx);
//FNIEMOP_DEF(iemOp_vpcmpeqb_Vx_Hx_Wx)
//{
//    IEMOP_MNEMONIC(vpcmpeqb, "vpcmpeqb");
//    return FNIEMOP_CALL_1(iemOpCommonSse2_FullFull_To_Full, &g_iemAImpl_pcmpeqb);
//}

/*  Opcode VEX.F3.0F 0x74 - invalid */
/*  Opcode VEX.F2.0F 0x74 - invalid */


/*  Opcode VEX.0F 0x75 - invalid */

/** Opcode VEX.66.0F 0x75 - vpcmpeqw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpcmpeqw_Vx_Hx_Wx);
//FNIEMOP_DEF(iemOp_vpcmpeqw_Vx_Hx_Wx)
//{
//    IEMOP_MNEMONIC(vpcmpeqw, "vpcmpeqw");
//    return FNIEMOP_CALL_1(iemOpCommonSse2_FullFull_To_Full, &g_iemAImpl_pcmpeqw);
//}

/*  Opcode VEX.F3.0F 0x75 - invalid */
/*  Opcode VEX.F2.0F 0x75 - invalid */


/*  Opcode VEX.0F 0x76 - invalid */

/** Opcode VEX.66.0F 0x76 - vpcmpeqd Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpcmpeqd_Vx_Hx_Wx);
//FNIEMOP_DEF(iemOp_vpcmpeqd_Vx_Hx_Wx)
//{
//    IEMOP_MNEMONIC(vpcmpeqd, "vpcmpeqd");
//    return FNIEMOP_CALL_1(iemOpCommonSse2_FullFull_To_Full, &g_iemAImpl_pcmpeqd);
//}

/*  Opcode VEX.F3.0F 0x76 - invalid */
/*  Opcode VEX.F2.0F 0x76 - invalid */


/** Opcode VEX.0F 0x77 - vzeroupperv vzeroallv */
FNIEMOP_STUB(iemOp_vzeroupperv__vzeroallv);
/*  Opcode VEX.66.0F 0x77 - invalid */
/*  Opcode VEX.F3.0F 0x77 - invalid */
/*  Opcode VEX.F2.0F 0x77 - invalid */

/*  Opcode VEX.0F 0x78 - invalid */
/*  Opcode VEX.66.0F 0x78 - invalid */
/*  Opcode VEX.F3.0F 0x78 - invalid */
/*  Opcode VEX.F2.0F 0x78 - invalid */

/*  Opcode VEX.0F 0x79 - invalid */
/*  Opcode VEX.66.0F 0x79 - invalid */
/*  Opcode VEX.F3.0F 0x79 - invalid */
/*  Opcode VEX.F2.0F 0x79 - invalid */

/*  Opcode VEX.0F 0x7a - invalid */
/*  Opcode VEX.66.0F 0x7a - invalid */
/*  Opcode VEX.F3.0F 0x7a - invalid */
/*  Opcode VEX.F2.0F 0x7a - invalid */

/*  Opcode VEX.0F 0x7b - invalid */
/*  Opcode VEX.66.0F 0x7b - invalid */
/*  Opcode VEX.F3.0F 0x7b - invalid */
/*  Opcode VEX.F2.0F 0x7b - invalid */

/*  Opcode VEX.0F 0x7c - invalid */
/** Opcode VEX.66.0F 0x7c - vhaddpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vhaddpd_Vpd_Hpd_Wpd);
/*  Opcode VEX.F3.0F 0x7c - invalid */
/** Opcode VEX.F2.0F 0x7c - vhaddps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vhaddps_Vps_Hps_Wps);

/*  Opcode VEX.0F 0x7d - invalid */
/** Opcode VEX.66.0F 0x7d - vhsubpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vhsubpd_Vpd_Hpd_Wpd);
/*  Opcode VEX.F3.0F 0x7d - invalid */
/** Opcode VEX.F2.0F 0x7d - vhsubps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vhsubps_Vps_Hps_Wps);


/*  Opcode VEX.0F 0x7e - invalid */

/** Opcode VEX.66.0F 0x7e - vmovd_q Ey, Vy */
FNIEMOP_STUB(iemOp_vmovd_q_Ey_Vy);
//FNIEMOP_DEF(iemOp_vmovd_q_Ey_Vy)
//{
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
//        IEMOP_MNEMONIC(vmovq_Eq_Wq, "vmovq Eq,Wq");
//    else
//        IEMOP_MNEMONIC(vmovd_Ed_Wd, "vmovd Ed,Wd");
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /* greg, XMM */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(0, 1);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();
//        if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
//        {
//            IEM_MC_LOCAL(uint64_t, u64Tmp);
//            IEM_MC_FETCH_XREG_U64(u64Tmp, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//            IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u64Tmp);
//        }
//        else
//        {
//            IEM_MC_LOCAL(uint32_t, u32Tmp);
//            IEM_MC_FETCH_XREG_U32(u32Tmp, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//            IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u32Tmp);
//        }
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /* [mem], XMM */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 1);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();
//        if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
//        {
//            IEM_MC_LOCAL(uint64_t, u64Tmp);
//            IEM_MC_FETCH_XREG_U64(u64Tmp, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//            IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, u64Tmp);
//        }
//        else
//        {
//            IEM_MC_LOCAL(uint32_t, u32Tmp);
//            IEM_MC_FETCH_XREG_U32(u32Tmp, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//            IEM_MC_STORE_MEM_U32(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, u32Tmp);
//        }
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}

/** Opcode VEX.F3.0F 0x7e - vmovq Vq, Wq */
FNIEMOP_STUB(iemOp_vmovq_Vq_Wq);
/*  Opcode VEX.F2.0F 0x7e - invalid */


/*  Opcode VEX.0F 0x7f - invalid */

/** Opcode VEX.66.0F 0x7f - vmovdqa Wx,Vx */
FNIEMOP_STUB(iemOp_vmovdqa_Wx_Vx);
//FNIEMOP_DEF(iemOp_vmovdqa_Wx_Vx)
//{
//    IEMOP_MNEMONIC(vmovdqa_Wdq_Vdq, "vmovdqa Wx,Vx");
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(0, 0);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//        IEM_MC_COPY_XREG_U128((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,
//                              ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Register, memory.
//         */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(RTUINT128U, u128Tmp);
//        IEM_MC_LOCAL(RTGCPTR,    GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();
//
//        IEM_MC_FETCH_XREG_U128(u128Tmp, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_STORE_MEM_U128_ALIGN_SSE(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, u128Tmp);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}

/** Opcode VEX.F3.0F 0x7f - vmovdqu Wx,Vx */
FNIEMOP_STUB(iemOp_vmovdqu_Wx_Vx);
//FNIEMOP_DEF(iemOp_vmovdqu_Wx_Vx)
//{
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    IEMOP_MNEMONIC(vmovdqu_Wdq_Vdq, "vmovdqu Wx,Vx");
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(0, 0);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//        IEM_MC_COPY_XREG_U128((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,
//                              ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Register, memory.
//         */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(RTUINT128U, u128Tmp);
//        IEM_MC_LOCAL(RTGCPTR,    GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();
//
//        IEM_MC_FETCH_XREG_U128(u128Tmp, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_STORE_MEM_U128(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, u128Tmp);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}

/*  Opcode VEX.F2.0F 0x7f - invalid */


/*  Opcode VEX.0F 0x80 - invalid  */
/*  Opcode VEX.0F 0x81 - invalid  */
/*  Opcode VEX.0F 0x82 - invalid  */
/*  Opcode VEX.0F 0x83 - invalid  */
/*  Opcode VEX.0F 0x84 - invalid  */
/*  Opcode VEX.0F 0x85 - invalid  */
/*  Opcode VEX.0F 0x86 - invalid  */
/*  Opcode VEX.0F 0x87 - invalid  */
/*  Opcode VEX.0F 0x88 - invalid  */
/*  Opcode VEX.0F 0x89 - invalid  */
/*  Opcode VEX.0F 0x8a - invalid  */
/*  Opcode VEX.0F 0x8b - invalid  */
/*  Opcode VEX.0F 0x8c - invalid  */
/*  Opcode VEX.0F 0x8d - invalid  */
/*  Opcode VEX.0F 0x8e - invalid  */
/*  Opcode VEX.0F 0x8f - invalid  */
/*  Opcode VEX.0F 0x90 - invalid  */
/*  Opcode VEX.0F 0x91 - invalid  */
/*  Opcode VEX.0F 0x92 - invalid  */
/*  Opcode VEX.0F 0x93 - invalid  */
/*  Opcode VEX.0F 0x94 - invalid  */
/*  Opcode VEX.0F 0x95 - invalid  */
/*  Opcode VEX.0F 0x96 - invalid  */
/*  Opcode VEX.0F 0x97 - invalid  */
/*  Opcode VEX.0F 0x98 - invalid  */
/*  Opcode VEX.0F 0x99 - invalid  */
/*  Opcode VEX.0F 0x9a - invalid  */
/*  Opcode VEX.0F 0x9b - invalid  */
/*  Opcode VEX.0F 0x9c - invalid  */
/*  Opcode VEX.0F 0x9d - invalid  */
/*  Opcode VEX.0F 0x9e - invalid  */
/*  Opcode VEX.0F 0x9f - invalid  */
/*  Opcode VEX.0F 0xa0 - invalid  */
/*  Opcode VEX.0F 0xa1 - invalid  */
/*  Opcode VEX.0F 0xa2 - invalid  */
/*  Opcode VEX.0F 0xa3 - invalid  */
/*  Opcode VEX.0F 0xa4 - invalid  */
/*  Opcode VEX.0F 0xa5 - invalid  */
/*  Opcode VEX.0F 0xa6 - invalid  */
/*  Opcode VEX.0F 0xa7 - invalid  */
/*  Opcode VEX.0F 0xa8 - invalid  */
/*  Opcode VEX.0F 0xa9 - invalid  */
/*  Opcode VEX.0F 0xaa - invalid  */
/*  Opcode VEX.0F 0xab - invalid  */
/*  Opcode VEX.0F 0xac - invalid  */
/*  Opcode VEX.0F 0xad - invalid  */


/*  Opcode VEX.0F 0xae mem/0 - invalid. */
/*  Opcode VEX.0F 0xae mem/1 - invalid. */

/**
 * @ opmaps      grp15
 * @ opcode      !11/2
 * @ oppfx       none
 * @ opcpuid     sse
 * @ opgroup     og_sse_mxcsrsm
 * @ opxcpttype  5
 * @ optest      op1=0      -> mxcsr=0
 * @ optest      op1=0x2083 -> mxcsr=0x2083
 * @ optest      op1=0xfffffffe -> value.xcpt=0xd
 * @ optest      op1=0x2083 cr0|=ts -> value.xcpt=0x7
 * @ optest      op1=0x2083 cr0|=em -> value.xcpt=0x6
 * @ optest      op1=0x2083 cr0|=mp -> mxcsr=0x2083
 * @ optest      op1=0x2083 cr4&~=osfxsr -> value.xcpt=0x6
 * @ optest      op1=0x2083 cr0|=ts,em -> value.xcpt=0x6
 * @ optest      op1=0x2083 cr0|=em cr4&~=osfxsr -> value.xcpt=0x6
 * @ optest      op1=0x2083 cr0|=ts,em cr4&~=osfxsr -> value.xcpt=0x6
 * @ optest      op1=0x2083 cr0|=ts,em,mp cr4&~=osfxsr -> value.xcpt=0x6
 */
FNIEMOP_STUB_1(iemOp_VGrp15_vldmxcsr, uint8_t, bRm);
//FNIEMOP_DEF_1(iemOp_VGrp15_vldmxcsr, uint8_t, bRm)
//{
//    IEMOP_MNEMONIC1(M_MEM, VLDMXCSR, vldmxcsr, MdRO, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);
//    if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fSse)
//        return IEMOP_RAISE_INVALID_OPCODE();
//
//    IEM_MC_BEGIN(2, 0);
//    IEM_MC_ARG(uint8_t,         iEffSeg,                                 0);
//    IEM_MC_ARG(RTGCPTR,         GCPtrEff,                                1);
//    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEff, bRm, 0);
//    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//    IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();
//    IEM_MC_ASSIGN(iEffSeg, pVCpu->iem.s.iEffSeg);
//    IEM_MC_CALL_CIMPL_2(iemCImpl_ldmxcsr, iEffSeg, GCPtrEff);
//    IEM_MC_END();
//    return VINF_SUCCESS;
//}


/**
 * @opmaps      vexgrp15
 * @opcode      !11/3
 * @oppfx       none
 * @opcpuid     avx
 * @opgroup     og_avx_mxcsrsm
 * @opxcpttype  5
 * @optest      mxcsr=0      -> op1=0
 * @optest      mxcsr=0x2083 -> op1=0x2083
 * @optest      mxcsr=0x2084 cr0|=ts -> value.xcpt=0x7
 * @optest      !amd / mxcsr=0x2085 cr0|=em -> op1=0x2085
 * @optest       amd / mxcsr=0x2085 cr0|=em -> value.xcpt=0x6
 * @optest      mxcsr=0x2086 cr0|=mp -> op1=0x2086
 * @optest      mxcsr=0x2087 cr4&~=osfxsr -> op1=0x2087
 * @optest      mxcsr=0x208f cr4&~=osxsave -> value.xcpt=0x6
 * @optest      mxcsr=0x2087 cr4&~=osfxsr,osxsave -> value.xcpt=0x6
 * @optest      !amd / mxcsr=0x2088 cr0|=ts,em -> value.xcpt=0x7
 * @optest      amd  / mxcsr=0x2088 cr0|=ts,em -> value.xcpt=0x6
 * @optest      !amd / mxcsr=0x2089 cr0|=em cr4&~=osfxsr -> op1=0x2089
 * @optest      amd  / mxcsr=0x2089 cr0|=em cr4&~=osfxsr -> value.xcpt=0x6
 * @optest      !amd / mxcsr=0x208a cr0|=ts,em cr4&~=osfxsr -> value.xcpt=0x7
 * @optest      amd  / mxcsr=0x208a cr0|=ts,em cr4&~=osfxsr -> value.xcpt=0x6
 * @optest      !amd / mxcsr=0x208b cr0|=ts,em,mp cr4&~=osfxsr -> value.xcpt=0x7
 * @optest      amd  / mxcsr=0x208b cr0|=ts,em,mp cr4&~=osfxsr -> value.xcpt=0x6
 * @optest      !amd / mxcsr=0x208c xcr0&~=all_avx -> value.xcpt=0x6
 * @optest      amd  / mxcsr=0x208c xcr0&~=all_avx -> op1=0x208c
 * @optest      !amd / mxcsr=0x208d xcr0&~=all_avx_sse -> value.xcpt=0x6
 * @optest      amd  / mxcsr=0x208d xcr0&~=all_avx_sse -> op1=0x208d
 * @optest      !amd / mxcsr=0x208e xcr0&~=all_avx cr0|=ts -> value.xcpt=0x6
 * @optest      amd  / mxcsr=0x208e xcr0&~=all_avx cr0|=ts -> value.xcpt=0x7
 * @optest      mxcsr=0x2082 cr0|=ts cr4&~=osxsave -> value.xcpt=0x6
 * @optest      mxcsr=0x2081 xcr0&~=all_avx cr0|=ts cr4&~=osxsave
 *              -> value.xcpt=0x6
 * @remarks     AMD Jaguar CPU (f0x16,m0,s1) \#UD when CR0.EM is set.  It also
 *              doesn't seem to check XCR0[2:1] != 11b.  This does not match the
 *              APMv4 rev 3.17 page 509.
 * @todo        Test this instruction on AMD Ryzen.
 */
FNIEMOP_DEF_1(iemOp_VGrp15_vstmxcsr,  uint8_t, bRm)
{
    IEMOP_MNEMONIC1(VEX_M_MEM, VSTMXCSR, vstmxcsr, Md_WO, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);
    IEM_MC_BEGIN(2, 0);
    IEM_MC_ARG(uint8_t,         iEffSeg,                                 0);
    IEM_MC_ARG(RTGCPTR,         GCPtrEff,                                1);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEff, bRm, 0);
    IEMOP_HLP_DONE_VEX_DECODING_L0_AND_NO_VVVV();
    IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();
    IEM_MC_ASSIGN(iEffSeg, pVCpu->iem.s.iEffSeg);
    IEM_MC_CALL_CIMPL_2(iemCImpl_vstmxcsr, iEffSeg, GCPtrEff);
    IEM_MC_END();
    return VINF_SUCCESS;
}

/*  Opcode VEX.0F 0xae mem/4 - invalid. */
/*  Opcode VEX.0F 0xae mem/5 - invalid. */
/*  Opcode VEX.0F 0xae mem/6 - invalid. */
/*  Opcode VEX.0F 0xae mem/7 - invalid. */

/*  Opcode VEX.0F 0xae 11b/0 - invalid. */
/*  Opcode VEX.0F 0xae 11b/1 - invalid. */
/*  Opcode VEX.0F 0xae 11b/2 - invalid. */
/*  Opcode VEX.0F 0xae 11b/3 - invalid. */
/*  Opcode VEX.0F 0xae 11b/4 - invalid. */
/*  Opcode VEX.0F 0xae 11b/5 - invalid. */
/*  Opcode VEX.0F 0xae 11b/6 - invalid. */
/*  Opcode VEX.0F 0xae 11b/7 - invalid. */

/**
 * Vex group 15 jump table for memory variant.
 */
IEM_STATIC const PFNIEMOPRM g_apfnVexGroup15MemReg[] =
{   /* pfx:  none,                          066h,                           0f3h,                           0f2h */
    /* /0 */ iemOp_InvalidWithRM,           iemOp_InvalidWithRM,            iemOp_InvalidWithRM,            iemOp_InvalidWithRM,
    /* /1 */ iemOp_InvalidWithRM,           iemOp_InvalidWithRM,            iemOp_InvalidWithRM,            iemOp_InvalidWithRM,
    /* /2 */ iemOp_VGrp15_vldmxcsr,         iemOp_InvalidWithRM,            iemOp_InvalidWithRM,            iemOp_InvalidWithRM,
    /* /3 */ iemOp_VGrp15_vstmxcsr,         iemOp_InvalidWithRM,            iemOp_InvalidWithRM,            iemOp_InvalidWithRM,
    /* /4 */ iemOp_InvalidWithRM,           iemOp_InvalidWithRM,            iemOp_InvalidWithRM,            iemOp_InvalidWithRM,
    /* /5 */ iemOp_InvalidWithRM,           iemOp_InvalidWithRM,            iemOp_InvalidWithRM,            iemOp_InvalidWithRM,
    /* /6 */ iemOp_InvalidWithRM,           iemOp_InvalidWithRM,            iemOp_InvalidWithRM,            iemOp_InvalidWithRM,
    /* /7 */ iemOp_InvalidWithRM,           iemOp_InvalidWithRM,            iemOp_InvalidWithRM,            iemOp_InvalidWithRM,
};
AssertCompile(RT_ELEMENTS(g_apfnVexGroup15MemReg) == 8*4);


/** Opcode vex. 0xae. */
FNIEMOP_DEF(iemOp_VGrp15)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        /* register, register */
        return FNIEMOP_CALL_1(iemOp_InvalidWithRM, bRm);

    /* memory, register */
    return FNIEMOP_CALL_1(g_apfnVexGroup15MemReg[ ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) * 4
                                                 + pVCpu->iem.s.idxPrefix], bRm);
}


/*  Opcode VEX.0F 0xaf - invalid. */

/*  Opcode VEX.0F 0xb0 - invalid. */
/*  Opcode VEX.0F 0xb1 - invalid. */
/*  Opcode VEX.0F 0xb2 - invalid. */
/*  Opcode VEX.0F 0xb2 - invalid. */
/*  Opcode VEX.0F 0xb3 - invalid. */
/*  Opcode VEX.0F 0xb4 - invalid. */
/*  Opcode VEX.0F 0xb5 - invalid. */
/*  Opcode VEX.0F 0xb6 - invalid. */
/*  Opcode VEX.0F 0xb7 - invalid. */
/*  Opcode VEX.0F 0xb8 - invalid. */
/*  Opcode VEX.0F 0xb9 - invalid. */
/*  Opcode VEX.0F 0xba - invalid. */
/*  Opcode VEX.0F 0xbb - invalid. */
/*  Opcode VEX.0F 0xbc - invalid. */
/*  Opcode VEX.0F 0xbd - invalid. */
/*  Opcode VEX.0F 0xbe - invalid. */
/*  Opcode VEX.0F 0xbf - invalid. */

/*  Opcode VEX.0F 0xc0 - invalid. */
/*  Opcode VEX.66.0F 0xc0 - invalid. */
/*  Opcode VEX.F3.0F 0xc0 - invalid. */
/*  Opcode VEX.F2.0F 0xc0 - invalid. */

/*  Opcode VEX.0F 0xc1 - invalid. */
/*  Opcode VEX.66.0F 0xc1 - invalid. */
/*  Opcode VEX.F3.0F 0xc1 - invalid. */
/*  Opcode VEX.F2.0F 0xc1 - invalid. */

/** Opcode VEX.0F 0xc2 - vcmpps Vps,Hps,Wps,Ib */
FNIEMOP_STUB(iemOp_vcmpps_Vps_Hps_Wps_Ib);
/** Opcode VEX.66.0F 0xc2 - vcmppd Vpd,Hpd,Wpd,Ib */
FNIEMOP_STUB(iemOp_vcmppd_Vpd_Hpd_Wpd_Ib);
/** Opcode VEX.F3.0F 0xc2 - vcmpss Vss,Hss,Wss,Ib */
FNIEMOP_STUB(iemOp_vcmpss_Vss_Hss_Wss_Ib);
/** Opcode VEX.F2.0F 0xc2 - vcmpsd Vsd,Hsd,Wsd,Ib */
FNIEMOP_STUB(iemOp_vcmpsd_Vsd_Hsd_Wsd_Ib);

/*  Opcode VEX.0F 0xc3 - invalid */
/*  Opcode VEX.66.0F 0xc3 - invalid */
/*  Opcode VEX.F3.0F 0xc3 - invalid */
/*  Opcode VEX.F2.0F 0xc3 - invalid */

/*  Opcode VEX.0F 0xc4 - invalid */
/** Opcode VEX.66.0F 0xc4 - vpinsrw Vdq,Hdq,Ry/Mw,Ib */
FNIEMOP_STUB(iemOp_vpinsrw_Vdq_Hdq_RyMw_Ib);
/*  Opcode VEX.F3.0F 0xc4 - invalid */
/*  Opcode VEX.F2.0F 0xc4 - invalid */

/*  Opcode VEX.0F 0xc5 - invlid */
/** Opcode VEX.66.0F 0xc5 - vpextrw Gd, Udq, Ib */
FNIEMOP_STUB(iemOp_vpextrw_Gd_Udq_Ib);
/*  Opcode VEX.F3.0F 0xc5 - invalid */
/*  Opcode VEX.F2.0F 0xc5 - invalid */

/** Opcode VEX.0F 0xc6 - vshufps Vps,Hps,Wps,Ib */
FNIEMOP_STUB(iemOp_vshufps_Vps_Hps_Wps_Ib);
/** Opcode VEX.66.0F 0xc6 - vshufpd Vpd,Hpd,Wpd,Ib */
FNIEMOP_STUB(iemOp_vshufpd_Vpd_Hpd_Wpd_Ib);
/*  Opcode VEX.F3.0F 0xc6 - invalid */
/*  Opcode VEX.F2.0F 0xc6 - invalid */

/*  Opcode VEX.0F 0xc7 - invalid */
/*  Opcode VEX.66.0F 0xc7 - invalid */
/*  Opcode VEX.F3.0F 0xc7 - invalid */
/*  Opcode VEX.F2.0F 0xc7 - invalid */

/*  Opcode VEX.0F 0xc8 - invalid */
/*  Opcode VEX.0F 0xc9 - invalid */
/*  Opcode VEX.0F 0xca - invalid */
/*  Opcode VEX.0F 0xcb - invalid */
/*  Opcode VEX.0F 0xcc - invalid */
/*  Opcode VEX.0F 0xcd - invalid */
/*  Opcode VEX.0F 0xce - invalid */
/*  Opcode VEX.0F 0xcf - invalid */


/*  Opcode VEX.0F 0xd0 - invalid */
/** Opcode VEX.66.0F 0xd0 - vaddsubpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vaddsubpd_Vpd_Hpd_Wpd);
/*  Opcode VEX.F3.0F 0xd0 - invalid */
/** Opcode VEX.F2.0F 0xd0 - vaddsubps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vaddsubps_Vps_Hps_Wps);

/*  Opcode VEX.0F 0xd1 - invalid */
/** Opcode VEX.66.0F 0xd1 - vpsrlw Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsrlw_Vx_Hx_W);
/*  Opcode VEX.F3.0F 0xd1 - invalid */
/*  Opcode VEX.F2.0F 0xd1 - invalid */

/*  Opcode VEX.0F 0xd2 - invalid */
/** Opcode VEX.66.0F 0xd2 - vpsrld Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsrld_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xd2 - invalid */
/*  Opcode VEX.F2.0F 0xd2 - invalid */

/*  Opcode VEX.0F 0xd3 - invalid */
/** Opcode VEX.66.0F 0xd3 - vpsrlq Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsrlq_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xd3 - invalid */
/*  Opcode VEX.F2.0F 0xd3 - invalid */

/*  Opcode VEX.0F 0xd4 - invalid */
/** Opcode VEX.66.0F 0xd4 - vpaddq Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpaddq_Vx_Hx_W);
/*  Opcode VEX.F3.0F 0xd4 - invalid */
/*  Opcode VEX.F2.0F 0xd4 - invalid */

/*  Opcode VEX.0F 0xd5 - invalid */
/** Opcode VEX.66.0F 0xd5 - vpmullw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpmullw_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xd5 - invalid */
/*  Opcode VEX.F2.0F 0xd5 - invalid */

/*  Opcode VEX.0F 0xd6 - invalid */

/**
 * @ opcode      0xd6
 * @ oppfx       0x66
 * @ opcpuid     sse2
 * @ opgroup     og_sse2_pcksclr_datamove
 * @ opxcpttype  none
 * @ optest      op1=-1 op2=2 -> op1=2
 * @ optest      op1=0 op2=-42 -> op1=-42
 */
FNIEMOP_STUB(iemOp_vmovq_Wq_Vq);
//FNIEMOP_DEF(iemOp_vmovq_Wq_Vq)
//{
//    IEMOP_MNEMONIC2(MR, VMOVQ, vmovq, WqZxReg, Vq, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /*
//         * Register, register.
//         */
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(uint64_t,                  uSrc);
//
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
//
//        IEM_MC_FETCH_XREG_U64(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_STORE_XREG_U64_ZX_U128((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, uSrc);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    else
//    {
//        /*
//         * Memory, register.
//         */
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(uint64_t,                  uSrc);
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();
//
//        IEM_MC_FETCH_XREG_U64(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//    }
//    return VINF_SUCCESS;
//}

/*  Opcode VEX.F3.0F 0xd6 - invalid */
/*  Opcode VEX.F2.0F 0xd6 - invalid */


/*  Opcode VEX.0F 0xd7 - invalid */

/** Opcode VEX.66.0F 0xd7 -  */
FNIEMOP_STUB(iemOp_vpmovmskb_Gd_Ux);
//FNIEMOP_DEF(iemOp_vpmovmskb_Gd_Ux)
//{
//    /* Note! Taking the lazy approch here wrt the high 32-bits of the GREG. */
//    /** @todo testcase: Check that the instruction implicitly clears the high
//     *        bits in 64-bit mode.  The REX.W is first necessary when VLMAX > 256
//     *        and opcode modifications are made to work with the whole width (not
//     *        just 128). */
//    IEMOP_MNEMONIC(vpmovmskb_Gd_Nq, "vpmovmskb Gd, Ux");
//    /* Docs says register only. */
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT)) /** @todo test that this is registers only. */
//    {
//        IEMOP_HLP_DECODED_NL_2(OP_PMOVMSKB, IEMOPFORM_RM_REG, OP_PARM_Gd, OP_PARM_Vdq, DISOPTYPE_SSE | DISOPTYPE_HARMLESS);
//        IEM_MC_BEGIN(2, 0);
//        IEM_MC_ARG(uint64_t *,           pDst, 0);
//        IEM_MC_ARG(PCRTUINT128U,         pSrc, 1);
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_PREPARE_SSE_USAGE();
//        IEM_MC_REF_GREG_U64(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_REF_XREG_U128_CONST(pSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
//        IEM_MC_CALL_SSE_AIMPL_2(iemAImpl_pmovmskb_u128, pDst, pSrc);
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//        return VINF_SUCCESS;
//    }
//    return IEMOP_RAISE_INVALID_OPCODE();
//}

/*  Opcode VEX.F3.0F 0xd7 - invalid */
/*  Opcode VEX.F2.0F 0xd7 - invalid */


/*  Opcode VEX.0F 0xd8 - invalid */
/** Opcode VEX.66.0F 0xd8 - vpsubusb Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsubusb_Vx_Hx_W);
/*  Opcode VEX.F3.0F 0xd8 - invalid */
/*  Opcode VEX.F2.0F 0xd8 - invalid */

/*  Opcode VEX.0F 0xd9 - invalid */
/** Opcode VEX.66.0F 0xd9 - vpsubusw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsubusw_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xd9 - invalid */
/*  Opcode VEX.F2.0F 0xd9 - invalid */

/*  Opcode VEX.0F 0xda - invalid */
/** Opcode VEX.66.0F 0xda - vpminub Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpminub_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xda - invalid */
/*  Opcode VEX.F2.0F 0xda - invalid */

/*  Opcode VEX.0F 0xdb - invalid */
/** Opcode VEX.66.0F 0xdb - vpand Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpand_Vx_Hx_W);
/*  Opcode VEX.F3.0F 0xdb - invalid */
/*  Opcode VEX.F2.0F 0xdb - invalid */

/*  Opcode VEX.0F 0xdc - invalid */
/** Opcode VEX.66.0F 0xdc - vpaddusb Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpaddusb_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xdc - invalid */
/*  Opcode VEX.F2.0F 0xdc - invalid */

/*  Opcode VEX.0F 0xdd - invalid */
/** Opcode VEX.66.0F 0xdd - vpaddusw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpaddusw_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xdd - invalid */
/*  Opcode VEX.F2.0F 0xdd - invalid */

/*  Opcode VEX.0F 0xde - invalid */
/** Opcode VEX.66.0F 0xde - vpmaxub Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpmaxub_Vx_Hx_W);
/*  Opcode VEX.F3.0F 0xde - invalid */
/*  Opcode VEX.F2.0F 0xde - invalid */

/*  Opcode VEX.0F 0xdf - invalid */
/** Opcode VEX.66.0F 0xdf - vpandn Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpandn_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xdf - invalid */
/*  Opcode VEX.F2.0F 0xdf - invalid */

/*  Opcode VEX.0F 0xe0 - invalid */
/** Opcode VEX.66.0F 0xe0 - vpavgb Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpavgb_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xe0 - invalid */
/*  Opcode VEX.F2.0F 0xe0 - invalid */

/*  Opcode VEX.0F 0xe1 - invalid */
/** Opcode VEX.66.0F 0xe1 - vpsraw Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsraw_Vx_Hx_W);
/*  Opcode VEX.F3.0F 0xe1 - invalid */
/*  Opcode VEX.F2.0F 0xe1 - invalid */

/*  Opcode VEX.0F 0xe2 - invalid */
/** Opcode VEX.66.0F 0xe2 - vpsrad Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsrad_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xe2 - invalid */
/*  Opcode VEX.F2.0F 0xe2 - invalid */

/*  Opcode VEX.0F 0xe3 - invalid */
/** Opcode VEX.66.0F 0xe3 - vpavgw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpavgw_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xe3 - invalid */
/*  Opcode VEX.F2.0F 0xe3 - invalid */

/*  Opcode VEX.0F 0xe4 - invalid */
/** Opcode VEX.66.0F 0xe4 - vpmulhuw Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpmulhuw_Vx_Hx_W);
/*  Opcode VEX.F3.0F 0xe4 - invalid */
/*  Opcode VEX.F2.0F 0xe4 - invalid */

/*  Opcode VEX.0F 0xe5 - invalid */
/** Opcode VEX.66.0F 0xe5 - vpmulhw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpmulhw_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xe5 - invalid */
/*  Opcode VEX.F2.0F 0xe5 - invalid */

/*  Opcode VEX.0F 0xe6 - invalid */
/** Opcode VEX.66.0F 0xe6 - vcvttpd2dq Vx, Wpd */
FNIEMOP_STUB(iemOp_vcvttpd2dq_Vx_Wpd);
/** Opcode VEX.F3.0F 0xe6 - vcvtdq2pd Vx, Wpd */
FNIEMOP_STUB(iemOp_vcvtdq2pd_Vx_Wpd);
/** Opcode VEX.F2.0F 0xe6 - vcvtpd2dq Vx, Wpd */
FNIEMOP_STUB(iemOp_vcvtpd2dq_Vx_Wpd);


/* Opcode VEX.0F 0xe7 - invalid */

/** Opcode VEX.66.0F 0xe7 - vmovntdq Mx, Vx */
FNIEMOP_STUB(iemOp_vmovntdq_Mx_Vx);
//FNIEMOP_DEF(iemOp_vmovntdq_Mx_Vx)
//{
//    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
//    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
//    {
//        /* Register, memory. */
//        IEMOP_MNEMONIC(vmovntdq_Mx_Vx, "vmovntdq Mx,Vx");
//        IEM_MC_BEGIN(0, 2);
//        IEM_MC_LOCAL(RTUINT128U,                uSrc);
//        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
//
//        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
//        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
//        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
//        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();
//
//        IEM_MC_FETCH_XREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
//        IEM_MC_STORE_MEM_U128_ALIGN_SSE(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);
//
//        IEM_MC_ADVANCE_RIP();
//        IEM_MC_END();
//        return VINF_SUCCESS;
//    }
//
//    /* The register, register encoding is invalid. */
//    return IEMOP_RAISE_INVALID_OPCODE();
//}

/*  Opcode VEX.F3.0F 0xe7 - invalid */
/*  Opcode VEX.F2.0F 0xe7 - invalid */


/*  Opcode VEX.0F 0xe8 - invalid */
/** Opcode VEX.66.0F 0xe8 - vpsubsb Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsubsb_Vx_Hx_W);
/*  Opcode VEX.F3.0F 0xe8 - invalid */
/*  Opcode VEX.F2.0F 0xe8 - invalid */

/*  Opcode VEX.0F 0xe9 - invalid */
/** Opcode VEX.66.0F 0xe9 - vpsubsw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsubsw_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xe9 - invalid */
/*  Opcode VEX.F2.0F 0xe9 - invalid */

/*  Opcode VEX.0F 0xea - invalid */
/** Opcode VEX.66.0F 0xea - vpminsw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpminsw_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xea - invalid */
/*  Opcode VEX.F2.0F 0xea - invalid */

/*  Opcode VEX.0F 0xeb - invalid */
/** Opcode VEX.66.0F 0xeb - vpor Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpor_Vx_Hx_W);
/*  Opcode VEX.F3.0F 0xeb - invalid */
/*  Opcode VEX.F2.0F 0xeb - invalid */

/*  Opcode VEX.0F 0xec - invalid */
/** Opcode VEX.66.0F 0xec - vpaddsb Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpaddsb_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xec - invalid */
/*  Opcode VEX.F2.0F 0xec - invalid */

/*  Opcode VEX.0F 0xed - invalid */
/** Opcode VEX.66.0F 0xed - vpaddsw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpaddsw_Vx_Hx_Wx);
/*  Opcode VEX.F3.0F 0xed - invalid */
/*  Opcode VEX.F2.0F 0xed - invalid */

/*  Opcode VEX.0F 0xee - invalid */
/** Opcode VEX.66.0F 0xee - vpmaxsw Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpmaxsw_Vx_Hx_W);
/*  Opcode VEX.F3.0F 0xee - invalid */
/*  Opcode VEX.F2.0F 0xee - invalid */


/*  Opcode VEX.0F 0xef - invalid */

/** Opcode VEX.66.0F 0xef - vpxor Vx, Hx, Wx */
FNIEMOP_DEF(iemOp_vpxor_Vx_Hx_Wx)
{
    IEMOP_MNEMONIC(vpxor, "vpxor");
    return FNIEMOP_CALL_1(iemOpCommonSse2_FullFull_To_Full, &g_iemAImpl_pxor);
}

/*  Opcode VEX.F3.0F 0xef - invalid */
/*  Opcode VEX.F2.0F 0xef - invalid */

/*  Opcode VEX.0F 0xf0 - invalid */
/*  Opcode VEX.66.0F 0xf0 - invalid */
/** Opcode VEX.F2.0F 0xf0 - vlddqu Vx, Mx */
FNIEMOP_STUB(iemOp_vlddqu_Vx_Mx);

/*  Opcode VEX.0F 0xf1 - invalid */
/** Opcode VEX.66.0F 0xf1 - vpsllw Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsllw_Vx_Hx_W);
/*  Opcode VEX.F2.0F 0xf1 - invalid */

/*  Opcode VEX.0F 0xf2 - invalid */
/** Opcode VEX.66.0F 0xf2 - vpslld Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpslld_Vx_Hx_Wx);
/*  Opcode VEX.F2.0F 0xf2 - invalid */

/*  Opcode VEX.0F 0xf3 - invalid */
/** Opcode VEX.66.0F 0xf3 - vpsllq Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsllq_Vx_Hx_Wx);
/*  Opcode VEX.F2.0F 0xf3 - invalid */

/*  Opcode VEX.0F 0xf4 - invalid */
/** Opcode VEX.66.0F 0xf4 - vpmuludq Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpmuludq_Vx_Hx_W);
/*  Opcode VEX.F2.0F 0xf4 - invalid */

/*  Opcode VEX.0F 0xf5 - invalid */
/** Opcode VEX.66.0F 0xf5 - vpmaddwd Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpmaddwd_Vx_Hx_Wx);
/*  Opcode VEX.F2.0F 0xf5 - invalid */

/*  Opcode VEX.0F 0xf6 - invalid */
/** Opcode VEX.66.0F 0xf6 - vpsadbw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsadbw_Vx_Hx_Wx);
/*  Opcode VEX.F2.0F 0xf6 - invalid */

/*  Opcode VEX.0F 0xf7 - invalid */
/** Opcode VEX.66.0F 0xf7 - vmaskmovdqu Vdq, Udq */
FNIEMOP_STUB(iemOp_vmaskmovdqu_Vdq_Udq);
/*  Opcode VEX.F2.0F 0xf7 - invalid */

/*  Opcode VEX.0F 0xf8 - invalid */
/** Opcode VEX.66.0F 0xf8 - vpsubb Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsubb_Vx_Hx_W);
/*  Opcode VEX.F2.0F 0xf8 - invalid */

/*  Opcode VEX.0F 0xf9 - invalid */
/** Opcode VEX.66.0F 0xf9 - vpsubw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsubw_Vx_Hx_Wx);
/*  Opcode VEX.F2.0F 0xf9 - invalid */

/*  Opcode VEX.0F 0xfa - invalid */
/** Opcode VEX.66.0F 0xfa - vpsubd Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsubd_Vx_Hx_Wx);
/*  Opcode VEX.F2.0F 0xfa - invalid */

/*  Opcode VEX.0F 0xfb - invalid */
/** Opcode VEX.66.0F 0xfb - vpsubq Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsubq_Vx_Hx_W);
/*  Opcode VEX.F2.0F 0xfb - invalid */

/*  Opcode VEX.0F 0xfc - invalid */
/** Opcode VEX.66.0F 0xfc - vpaddb Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpaddb_Vx_Hx_Wx);
/*  Opcode VEX.F2.0F 0xfc - invalid */

/*  Opcode VEX.0F 0xfd - invalid */
/** Opcode VEX.66.0F 0xfd - vpaddw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpaddw_Vx_Hx_Wx);
/*  Opcode VEX.F2.0F 0xfd - invalid */

/*  Opcode VEX.0F 0xfe - invalid */
/** Opcode VEX.66.0F 0xfe - vpaddd Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpaddd_Vx_Hx_W);
/*  Opcode VEX.F2.0F 0xfe - invalid */


/** Opcode **** 0x0f 0xff - UD0 */
FNIEMOP_DEF(iemOp_vud0)
{
    IEMOP_MNEMONIC(vud0, "vud0");
    if (pVCpu->iem.s.enmCpuVendor == CPUMCPUVENDOR_INTEL)
    {
        uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm); RT_NOREF(bRm);
#ifndef TST_IEM_CHECK_MC
        RTGCPTR      GCPtrEff;
        VBOXSTRICTRC rcStrict = iemOpHlpCalcRmEffAddr(pVCpu, bRm, 0, &GCPtrEff);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
#endif
        IEMOP_HLP_DONE_DECODING();
    }
    return IEMOP_RAISE_INVALID_OPCODE();
}



/**
 * VEX opcode map \#1.
 *
 * @sa  g_apfnTwoByteMap
 */
IEM_STATIC const PFNIEMOP g_apfnVexMap1[] =
{
    /*          no prefix,                  066h prefix                 f3h prefix,                 f2h prefix */
    /* 0x00 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x01 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x02 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x03 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x04 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x05 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x06 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x07 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x08 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x09 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x0a */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x0b */  IEMOP_X4(iemOp_vud2), /* ?? */
    /* 0x0c */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x0d */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x0e */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x0f */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0x10 */  iemOp_vmovups_Vps_Wps,      iemOp_vmovupd_Vpd_Wpd,      iemOp_vmovss_Vss_Hss_Wss,   iemOp_vmovsd_Vsd_Hsd_Wsd,
    /* 0x11 */  iemOp_vmovups_Wps_Vps,      iemOp_vmovupd_Wpd_Vpd,      iemOp_vmovss_Wss_Hss_Vss,   iemOp_vmovsd_Wsd_Hsd_Vsd,
    /* 0x12 */  iemOp_vmovlps_Vq_Hq_Mq__vmovhlps, iemOp_vmovlpd_Vq_Hq_Mq, iemOp_vmovsldup_Vx_Wx,    iemOp_vmovddup_Vx_Wx,
    /* 0x13 */  iemOp_vmovlps_Mq_Vq,        iemOp_vmovlpd_Mq_Vq,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x14 */  iemOp_vunpcklps_Vx_Hx_Wx,   iemOp_vunpcklpd_Vx_Hx_Wx,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x15 */  iemOp_vunpckhps_Vx_Hx_Wx,   iemOp_vunpckhpd_Vx_Hx_Wx,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x16 */  iemOp_vmovhpsv1_Vdq_Hq_Mq__vmovlhps_Vdq_Hq_Uq, iemOp_vmovhpdv1_Vdq_Hq_Mq, iemOp_vmovshdup_Vx_Wx, iemOp_InvalidNeedRM,
    /* 0x17 */  iemOp_vmovhpsv1_Mq_Vq,      iemOp_vmovhpdv1_Mq_Vq,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x18 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x19 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x1a */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x1b */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x1c */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x1d */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x1e */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x1f */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0x20 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x21 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x22 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x23 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x24 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x25 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x26 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x27 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x28 */  iemOp_vmovaps_Vps_Wps,      iemOp_vmovapd_Vpd_Wpd,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x29 */  iemOp_vmovaps_Wps_Vps,      iemOp_vmovapd_Wpd_Vpd,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x2a */  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_vcvtsi2ss_Vss_Hss_Ey, iemOp_vcvtsi2sd_Vsd_Hsd_Ey,
    /* 0x2b */  iemOp_vmovntps_Mps_Vps,     iemOp_vmovntpd_Mpd_Vpd,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x2c */  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_vcvttss2si_Gy_Wss,    iemOp_vcvttsd2si_Gy_Wsd,
    /* 0x2d */  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_vcvtss2si_Gy_Wss,     iemOp_vcvtsd2si_Gy_Wsd,
    /* 0x2e */  iemOp_vucomiss_Vss_Wss,     iemOp_vucomisd_Vsd_Wsd,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x2f */  iemOp_vcomiss_Vss_Wss,      iemOp_vcomisd_Vsd_Wsd,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,

    /* 0x30 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x31 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x32 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x33 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x34 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x35 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x36 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x37 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x38 */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x39 */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x3a */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x3b */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x3c */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x3d */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x3e */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x3f */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */

    /* 0x40 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x41 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x42 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x43 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x44 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x45 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x46 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x47 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x48 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x49 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x4a */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x4b */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x4c */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x4d */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x4e */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x4f */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0x50 */  iemOp_vmovmskps_Gy_Ups,     iemOp_vmovmskpd_Gy_Upd,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x51 */  iemOp_vsqrtps_Vps_Wps,      iemOp_vsqrtpd_Vpd_Wpd,      iemOp_vsqrtss_Vss_Hss_Wss,  iemOp_vsqrtsd_Vsd_Hsd_Wsd,
    /* 0x52 */  iemOp_vrsqrtps_Vps_Wps,     iemOp_InvalidNeedRM,        iemOp_vrsqrtss_Vss_Hss_Wss, iemOp_InvalidNeedRM,
    /* 0x53 */  iemOp_vrcpps_Vps_Wps,       iemOp_InvalidNeedRM,        iemOp_vrcpss_Vss_Hss_Wss,   iemOp_InvalidNeedRM,
    /* 0x54 */  iemOp_vandps_Vps_Hps_Wps,   iemOp_vandpd_Vpd_Hpd_Wpd,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x55 */  iemOp_vandnps_Vps_Hps_Wps,  iemOp_vandnpd_Vpd_Hpd_Wpd,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x56 */  iemOp_vorps_Vps_Hps_Wps,    iemOp_vorpd_Vpd_Hpd_Wpd,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x57 */  iemOp_vxorps_Vps_Hps_Wps,   iemOp_vxorpd_Vpd_Hpd_Wpd,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x58 */  iemOp_vaddps_Vps_Hps_Wps,   iemOp_vaddpd_Vpd_Hpd_Wpd,   iemOp_vaddss_Vss_Hss_Wss,   iemOp_vaddsd_Vsd_Hsd_Wsd,
    /* 0x59 */  iemOp_vmulps_Vps_Hps_Wps,   iemOp_vmulpd_Vpd_Hpd_Wpd,   iemOp_vmulss_Vss_Hss_Wss,   iemOp_vmulsd_Vsd_Hsd_Wsd,
    /* 0x5a */  iemOp_vcvtps2pd_Vpd_Wps,    iemOp_vcvtpd2ps_Vps_Wpd,    iemOp_vcvtss2sd_Vsd_Hx_Wss, iemOp_vcvtsd2ss_Vss_Hx_Wsd,
    /* 0x5b */  iemOp_vcvtdq2ps_Vps_Wdq,    iemOp_vcvtps2dq_Vdq_Wps,    iemOp_vcvttps2dq_Vdq_Wps,   iemOp_InvalidNeedRM,
    /* 0x5c */  iemOp_vsubps_Vps_Hps_Wps,   iemOp_vsubpd_Vpd_Hpd_Wpd,   iemOp_vsubss_Vss_Hss_Wss,   iemOp_vsubsd_Vsd_Hsd_Wsd,
    /* 0x5d */  iemOp_vminps_Vps_Hps_Wps,   iemOp_vminpd_Vpd_Hpd_Wpd,   iemOp_vminss_Vss_Hss_Wss,   iemOp_vminsd_Vsd_Hsd_Wsd,
    /* 0x5e */  iemOp_vdivps_Vps_Hps_Wps,   iemOp_vdivpd_Vpd_Hpd_Wpd,   iemOp_vdivss_Vss_Hss_Wss,   iemOp_vdivsd_Vsd_Hsd_Wsd,
    /* 0x5f */  iemOp_vmaxps_Vps_Hps_Wps,   iemOp_vmaxpd_Vpd_Hpd_Wpd,   iemOp_vmaxss_Vss_Hss_Wss,   iemOp_vmaxsd_Vsd_Hsd_Wsd,

    /* 0x60 */  iemOp_InvalidNeedRM,        iemOp_vpunpcklbw_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x61 */  iemOp_InvalidNeedRM,        iemOp_vpunpcklwd_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x62 */  iemOp_InvalidNeedRM,        iemOp_vpunpckldq_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x63 */  iemOp_InvalidNeedRM,        iemOp_vpacksswb_Vx_Hx_Wx,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x64 */  iemOp_InvalidNeedRM,        iemOp_vpcmpgtb_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x65 */  iemOp_InvalidNeedRM,        iemOp_vpcmpgtw_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x66 */  iemOp_InvalidNeedRM,        iemOp_vpcmpgtd_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x67 */  iemOp_InvalidNeedRM,        iemOp_vpackuswb_Vx_Hx_W,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x68 */  iemOp_InvalidNeedRM,        iemOp_vpunpckhbw_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x69 */  iemOp_InvalidNeedRM,        iemOp_vpunpckhwd_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6a */  iemOp_InvalidNeedRM,        iemOp_vpunpckhdq_Vx_Hx_W,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6b */  iemOp_InvalidNeedRM,        iemOp_vpackssdw_Vx_Hx_Wx,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6c */  iemOp_InvalidNeedRM,        iemOp_vpunpcklqdq_Vx_Hx_Wx, iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6d */  iemOp_InvalidNeedRM,        iemOp_vpunpckhqdq_Vx_Hx_W,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6e */  iemOp_InvalidNeedRM,        iemOp_vmovd_q_Vy_Ey,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6f */  iemOp_InvalidNeedRM,        iemOp_vmovdqa_Vx_Wx,        iemOp_vmovdqu_Vx_Wx,        iemOp_InvalidNeedRM,

    /* 0x70 */  iemOp_InvalidNeedRM,        iemOp_vpshufd_Vx_Wx_Ib,     iemOp_vpshufhw_Vx_Wx_Ib,    iemOp_vpshuflw_Vx_Wx_Ib,
    /* 0x71 */  iemOp_InvalidNeedRM,        iemOp_VGrp12,               iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x72 */  iemOp_InvalidNeedRM,        iemOp_VGrp13,               iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x73 */  iemOp_InvalidNeedRM,        iemOp_VGrp14,               iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x74 */  iemOp_InvalidNeedRM,        iemOp_vpcmpeqb_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x75 */  iemOp_InvalidNeedRM,        iemOp_vpcmpeqw_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x76 */  iemOp_InvalidNeedRM,        iemOp_vpcmpeqd_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x77 */  iemOp_vzeroupperv__vzeroallv, iemOp_InvalidNeedRM,      iemOp_InvalidNeedRM,       iemOp_InvalidNeedRM,
    /* 0x78 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x79 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x7a */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x7b */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x7c */  iemOp_InvalidNeedRM,        iemOp_vhaddpd_Vpd_Hpd_Wpd,  iemOp_InvalidNeedRM,        iemOp_vhaddps_Vps_Hps_Wps,
    /* 0x7d */  iemOp_InvalidNeedRM,        iemOp_vhsubpd_Vpd_Hpd_Wpd,  iemOp_InvalidNeedRM,        iemOp_vhsubps_Vps_Hps_Wps,
    /* 0x7e */  iemOp_InvalidNeedRM,        iemOp_vmovd_q_Ey_Vy,        iemOp_vmovq_Vq_Wq,          iemOp_InvalidNeedRM,
    /* 0x7f */  iemOp_InvalidNeedRM,        iemOp_vmovdqa_Wx_Vx,        iemOp_vmovdqu_Wx_Vx,        iemOp_InvalidNeedRM,

    /* 0x80 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x81 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x82 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x83 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x84 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x85 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x86 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x87 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x88 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x89 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x8a */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x8b */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x8c */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x8d */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x8e */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x8f */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0x90 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x91 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x92 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x93 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x94 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x95 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x96 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x97 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x98 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x99 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x9a */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x9b */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x9c */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x9d */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x9e */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x9f */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0xa0 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa1 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa2 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa3 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa4 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa5 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa6 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa7 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa8 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa9 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xaa */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xab */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xac */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xad */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xae */  IEMOP_X4(iemOp_VGrp15),
    /* 0xaf */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0xb0 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb1 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb2 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb3 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb4 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb5 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb6 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb7 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb8 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb9 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xba */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xbb */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xbc */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xbd */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xbe */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xbf */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0xc0 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xc1 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xc2 */  iemOp_vcmpps_Vps_Hps_Wps_Ib, iemOp_vcmppd_Vpd_Hpd_Wpd_Ib, iemOp_vcmpss_Vss_Hss_Wss_Ib, iemOp_vcmpsd_Vsd_Hsd_Wsd_Ib,
    /* 0xc3 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xc4 */  iemOp_InvalidNeedRM,        iemOp_vpinsrw_Vdq_Hdq_RyMw_Ib, iemOp_InvalidNeedRMImm8, iemOp_InvalidNeedRMImm8,
    /* 0xc5 */  iemOp_InvalidNeedRM,        iemOp_vpextrw_Gd_Udq_Ib,       iemOp_InvalidNeedRMImm8,    iemOp_InvalidNeedRMImm8,
    /* 0xc6 */  iemOp_vshufps_Vps_Hps_Wps_Ib, iemOp_vshufpd_Vpd_Hpd_Wpd_Ib, iemOp_InvalidNeedRMImm8,iemOp_InvalidNeedRMImm8,
    /* 0xc7 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xc8 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xc9 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xca */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xcb */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xcc */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xcd */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xce */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xcf */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0xd0 */  iemOp_InvalidNeedRM,        iemOp_vaddsubpd_Vpd_Hpd_Wpd, iemOp_InvalidNeedRM,       iemOp_vaddsubps_Vps_Hps_Wps,
    /* 0xd1 */  iemOp_InvalidNeedRM,        iemOp_vpsrlw_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd2 */  iemOp_InvalidNeedRM,        iemOp_vpsrld_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd3 */  iemOp_InvalidNeedRM,        iemOp_vpsrlq_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd4 */  iemOp_InvalidNeedRM,        iemOp_vpaddq_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd5 */  iemOp_InvalidNeedRM,        iemOp_vpmullw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd6 */  iemOp_InvalidNeedRM,        iemOp_vmovq_Wq_Vq,          iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd7 */  iemOp_InvalidNeedRM,        iemOp_vpmovmskb_Gd_Ux,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd8 */  iemOp_InvalidNeedRM,        iemOp_vpsubusb_Vx_Hx_W,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd9 */  iemOp_InvalidNeedRM,        iemOp_vpsubusw_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xda */  iemOp_InvalidNeedRM,        iemOp_vpminub_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xdb */  iemOp_InvalidNeedRM,        iemOp_vpand_Vx_Hx_W,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xdc */  iemOp_InvalidNeedRM,        iemOp_vpaddusb_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xdd */  iemOp_InvalidNeedRM,        iemOp_vpaddusw_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xde */  iemOp_InvalidNeedRM,        iemOp_vpmaxub_Vx_Hx_W,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xdf */  iemOp_InvalidNeedRM,        iemOp_vpandn_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,

    /* 0xe0 */  iemOp_InvalidNeedRM,        iemOp_vpavgb_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe1 */  iemOp_InvalidNeedRM,        iemOp_vpsraw_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe2 */  iemOp_InvalidNeedRM,        iemOp_vpsrad_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe3 */  iemOp_InvalidNeedRM,        iemOp_vpavgw_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe4 */  iemOp_InvalidNeedRM,        iemOp_vpmulhuw_Vx_Hx_W,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe5 */  iemOp_InvalidNeedRM,        iemOp_vpmulhw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe6 */  iemOp_InvalidNeedRM,        iemOp_vcvttpd2dq_Vx_Wpd,    iemOp_vcvtdq2pd_Vx_Wpd,     iemOp_vcvtpd2dq_Vx_Wpd,
    /* 0xe7 */  iemOp_InvalidNeedRM,        iemOp_vmovntdq_Mx_Vx,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe8 */  iemOp_InvalidNeedRM,        iemOp_vpsubsb_Vx_Hx_W,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe9 */  iemOp_InvalidNeedRM,        iemOp_vpsubsw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xea */  iemOp_InvalidNeedRM,        iemOp_vpminsw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xeb */  iemOp_InvalidNeedRM,        iemOp_vpor_Vx_Hx_W,         iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xec */  iemOp_InvalidNeedRM,        iemOp_vpaddsb_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xed */  iemOp_InvalidNeedRM,        iemOp_vpaddsw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xee */  iemOp_InvalidNeedRM,        iemOp_vpmaxsw_Vx_Hx_W,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xef */  iemOp_InvalidNeedRM,        iemOp_vpxor_Vx_Hx_Wx,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,

    /* 0xf0 */  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_vlddqu_Vx_Mx,
    /* 0xf1 */  iemOp_InvalidNeedRM,        iemOp_vpsllw_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf2 */  iemOp_InvalidNeedRM,        iemOp_vpslld_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf3 */  iemOp_InvalidNeedRM,        iemOp_vpsllq_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf4 */  iemOp_InvalidNeedRM,        iemOp_vpmuludq_Vx_Hx_W,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf5 */  iemOp_InvalidNeedRM,        iemOp_vpmaddwd_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf6 */  iemOp_InvalidNeedRM,        iemOp_vpsadbw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf7 */  iemOp_InvalidNeedRM,        iemOp_vmaskmovdqu_Vdq_Udq,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf8 */  iemOp_InvalidNeedRM,        iemOp_vpsubb_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf9 */  iemOp_InvalidNeedRM,        iemOp_vpsubw_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfa */  iemOp_InvalidNeedRM,        iemOp_vpsubd_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfb */  iemOp_InvalidNeedRM,        iemOp_vpsubq_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfc */  iemOp_InvalidNeedRM,        iemOp_vpaddb_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfd */  iemOp_InvalidNeedRM,        iemOp_vpaddw_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfe */  iemOp_InvalidNeedRM,        iemOp_vpaddd_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xff */  IEMOP_X4(iemOp_vud0) /* ?? */
};
AssertCompile(RT_ELEMENTS(g_apfnVexMap1) == 1024);
/** @}  */


