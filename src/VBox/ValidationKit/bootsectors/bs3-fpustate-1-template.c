/* $Id$ */
/** @file
 * BS3Kit - bs3-fpustate-1, C code template.
 */

/*
 * Copyright (C) 2007-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#include <VBox/VMMDevTesting.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


#ifdef BS3_INSTANTIATING_CMN

/**
 * Displays the differences between the two states.
 */
# define bs3FpuState1_Diff BS3_CMN_NM(bs3FpuState1_Diff)
BS3_DECL_NEAR(void) bs3FpuState1_Diff(X86FXSTATE const BS3_FAR *pExpected, X86FXSTATE const BS3_FAR *pChecking)
{
    unsigned i;

# define CHECK(a_Member, a_Fmt) \
        if (pExpected->a_Member != pChecking->a_Member) \
            Bs3TestPrintf("  " #a_Member ": " a_Fmt ", expected " a_Fmt "\n", pChecking->a_Member, pExpected->a_Member); \
        else do { } while (0)
    CHECK(FCW,          "%#RX16");
    CHECK(FSW,          "%#RX16");
    CHECK(FTW,          "%#RX16");
    CHECK(FOP,          "%#RX16");
    CHECK(FPUIP,        "%#RX32");
    CHECK(CS,           "%#RX16");
    CHECK(Rsrvd1,       "%#RX16");
    CHECK(FPUDP,        "%#RX32");
    CHECK(DS,           "%#RX16");
    CHECK(Rsrvd2,       "%#RX16");
    CHECK(MXCSR,        "%#RX32");
    CHECK(MXCSR_MASK,   "%#RX32");
# undef CHECK
    for (i = 0; i < RT_ELEMENTS(pExpected->aRegs); i++)
        if (   pChecking->aRegs[i].au64[0] != pExpected->aRegs[i].au64[0]
            || pChecking->aRegs[i].au64[1] != pExpected->aRegs[i].au64[1])
            Bs3TestPrintf("st%u: %.16Rhxs\n"
                          "exp: %.16Rhxs\n",
                          i, &pChecking->aRegs[i], &pExpected->aRegs[i]);
    for (i = 0; i < RT_ELEMENTS(pExpected->aXMM); i++)
        if (   pChecking->aXMM[i].au64[0] != pExpected->aXMM[i].au64[0]
            || pChecking->aXMM[i].au64[1] != pExpected->aXMM[i].au64[1])
            Bs3TestPrintf("xmm%u: %.16Rhxs\n"
                          " %sexp: %.16Rhxs\n",
                          i, &pChecking->aRegs[i], &pExpected->aRegs[i], i >= 10 ? " " : "");
}


#endif /* BS3_INSTANTIATING_CMN */


/*
 * Mode specific code.
 * Mode specific code.
 * Mode specific code.
 */
#ifdef BS3_INSTANTIATING_MODE
# if TMPL_MODE == BS3_MODE_PE32 \
  || TMPL_MODE == BS3_MODE_PP32 \
  || TMPL_MODE == BS3_MODE_PAE32 \
  || TMPL_MODE == BS3_MODE_LM64 \
  || TMPL_MODE == BS3_MODE_RM

/* Assembly helpers: */
BS3_DECL_NEAR(void) TMPL_NM(bs3FpuState1_InitState)(X86FXSTATE BS3_FAR *pFxState, void BS3_FAR *pvMmioReg);
BS3_DECL_NEAR(void) TMPL_NM(bs3FpuState1_Restore)(X86FXSTATE const BS3_FAR *pFxState);
BS3_DECL_NEAR(void) TMPL_NM(bs3FpuState1_Save)(X86FXSTATE BS3_FAR *pFxState);

BS3_DECL_NEAR(void) TMPL_NM(bs3FpuState1_FNStEnv)(void BS3_FAR *pvMmioReg);
BS3_DECL_NEAR(void) TMPL_NM(bs3FpuState1_MovDQU_Read)(void BS3_FAR *pvMmioReg);
BS3_DECL_NEAR(void) TMPL_NM(bs3FpuState1_MovDQU_Write)(void BS3_FAR *pvMmioReg);
BS3_DECL_NEAR(void) TMPL_NM(bs3FpuState1_FMul)(void BS3_FAR *pvMmioReg);


/**
 * Tests for FPU state corruption.
 *
 * First we don't do anything to quit guest context for a while.
 * Then we start testing weird MMIO accesses, some which amonger other things
 * forces the use of the FPU state or host FPU to do the emulation.  Both are a
 * little complicated in raw-mode and ring-0 contexts.
 *
 * We ASSUME FXSAVE/FXRSTOR support here.
 */
BS3_DECL_FAR(uint8_t) TMPL_NM(bs3FpuState1_Corruption)(uint8_t bMode)
{
    /* We don't need to test that many modes, probably.  */

    uint8_t             abBuf[sizeof(X86FXSTATE)*2 + 32];
    uint8_t BS3_FAR    *pbTmp = &abBuf[0x10 - (((uintptr_t)abBuf) & 0x0f)];
    X86FXSTATE BS3_FAR *pExpected = (X86FXSTATE BS3_FAR *)pbTmp;
    X86FXSTATE BS3_FAR *pChecking = pExpected + 1;
    uint32_t            iLoop;
    uint32_t            uStartTick;
    bool                fMmioReadback;
    bool                fReadBackError = false;
    BS3PTRUNION         MmioReg;


# undef  CHECK_STATE
# define CHECK_STATE(a_Instr) \
        do { \
            TMPL_NM(bs3FpuState1_Save)(pChecking); \
            if (Bs3MemCmp(pExpected, pChecking, sizeof(*pExpected)) != 0) \
            { \
                Bs3TestFailedF("State differs after " #a_Instr " (write) in loop #%RU32\n", iLoop); \
                bs3FpuState1_Diff(pExpected, pChecking); \
                Bs3PitDisable(); \
                return 1; \
            } \
        } while (0)

    /*
     * Setup the test.
     */

    /* Make this code executable in raw-mode.  A bit tricky. */
    ASMSetCR0(ASMGetCR0() | X86_CR0_WP);
    Bs3PitSetupAndEnablePeriodTimer(20);
    ASMIntEnable();
# if ARCH_BITS != 64
    ASMHalt();
# endif

    /* Figure out which MMIO region we'll be using so we can correctly initialize FPUDS. */
# if BS3_MODE_IS_RM_OR_V86(TMPL_MODE)
    MmioReg.pv = BS3_FP_MAKE(0xffff, VMMDEV_TESTING_MMIO_BASE - _1M + 16);
# elif BS3_MODE_IS_16BIT_CODE(TMPL_MODE)
    MmioReg.pv = BS3_FP_MAKE(BS3_SEL_VMMDEV_MMIO16, VMMDEV_TESTING_MMIO_BASE - _1M);
# else
    MmioReg.pv = (uint8_t *)VMMDEV_TESTING_MMIO_BASE;
# endif
    if (MmioReg.pu32[VMMDEV_TESTING_MMIO_OFF_NOP / sizeof(uint32_t)] == VMMDEV_TESTING_NOP_RET)
    {
        fMmioReadback = true;
        MmioReg.pb += VMMDEV_TESTING_MMIO_OFF_READBACK;
    }
    else
    {
        Bs3TestPrintf("VMMDev MMIO not found, using VGA instead\n");
        fMmioReadback = false;
        MmioReg.pv = Bs3XptrFlatToCurrent(0xa7800);
    }

    /* Make 100% sure we don't trap accessing the FPU state and that we can use fxsave/fxrstor. */
    g_usBs3TestStep = 1;
    ASMSetCR0((ASMGetCR0() & ~(X86_CR0_TS | X86_CR0_EM)) | X86_CR0_MP);
    ASMSetCR4(ASMGetCR4() | X86_CR4_OSFXSR /*| X86_CR4_OSXMMEEXCPT*/);

    /* Come up with a distinct state. We do that from assembly (will do FPU in R0/RC). */
    g_usBs3TestStep = 2;
    Bs3MemSet(abBuf, 0x42, sizeof(abBuf));
    TMPL_NM(bs3FpuState1_InitState)(pExpected, MmioReg.pb);


    /*
     * Test #1: Check that we can keep it consistent for a while.
     */
    g_usBs3TestStep = 3;
    uStartTick = g_cBs3PitTicks;
    for (iLoop = 0; iLoop < _16M; iLoop++)
    {
        CHECK_STATE(nop);
        if (   (iLoop & 0xffff) == 0xffff
            && g_cBs3PitTicks - uStartTick >= 20 * 20) /* 20 seconds*/
            break;
    }

    /*
     * Test #2: Use various FPU, SSE and weird instructions to do MMIO writes.
     *
     * We'll use the VMMDev readback register if possible, but make do
     * with VGA if not configured.
     */
    g_usBs3TestStep = 4;
    uStartTick = g_cBs3PitTicks;
    for (iLoop = 0; iLoop < _1M; iLoop++)
    {
        unsigned off;
        uint8_t  abCompare[64];
        uint8_t  abReadback[64];

        /* Macros  */
# undef  CHECK_READBACK_WRITE_RUN
# define CHECK_READBACK_WRITE_RUN(a_Instr, a_Worker, a_Type) \
            do { \
                off = (unsigned)(iLoop & (VMMDEV_TESTING_READBACK_SIZE / 2 - 1)); \
                if (off + sizeof(a_Type) > VMMDEV_TESTING_READBACK_SIZE) \
                    off = VMMDEV_TESTING_READBACK_SIZE - sizeof(a_Type); \
                a_Worker((a_Type *)&MmioReg.pb[off]); \
                if (fMmioReadback && (!fReadBackError || iLoop == 0)) \
                { \
                    a_Worker((a_Type *)&abCompare[0]); \
                    Bs3MemCpy(abReadback, &MmioReg.pb[off], sizeof(a_Type)); \
                    if (Bs3MemCmp(abReadback, abCompare, sizeof(a_Type)) != 0) \
                    { \
                        Bs3TestFailedF("Read back error for " #a_Instr " in loop #%RU32:\n%.*Rhxs expected:\n%.*Rhxs\n", \
                                       iLoop, sizeof(a_Type), abReadback, sizeof(a_Type), abCompare); \
                        fReadBackError = true; \
                    } \
                } \
            } while (0)

# undef  CHECK_READBACK_WRITE
# define CHECK_READBACK_WRITE(a_Instr, a_Worker, a_Type) \
            CHECK_READBACK_WRITE_RUN(a_Instr, a_Worker, a_Type); \
            CHECK_STATE(a_Instr)
# undef  CHECK_READBACK_WRITE_Z
# define CHECK_READBACK_WRITE_Z(a_Instr, a_Worker, a_Type) \
            do { \
                if (fMmioReadback && (!fReadBackError || iLoop == 0)) \
                { \
                    Bs3MemZero(&abCompare[0], sizeof(a_Type)); \
                    off = (unsigned)(iLoop & (VMMDEV_TESTING_READBACK_SIZE / 2 - 1)); \
                    if (off + sizeof(a_Type) > VMMDEV_TESTING_READBACK_SIZE) \
                        off = VMMDEV_TESTING_READBACK_SIZE - sizeof(a_Type); \
                    Bs3MemZero(&MmioReg.pb[off], sizeof(a_Type)); \
                } \
                CHECK_READBACK_WRITE(a_Instr, a_Worker, a_Type); \
            } while (0)

# undef  CHECK_READBACK_READ_RUN
# define CHECK_READBACK_READ_RUN(a_Instr, a_Worker, a_Type) \
            do { \
                off = (unsigned)(iLoop & (VMMDEV_TESTING_READBACK_SIZE / 2 - 1)); \
                if (off + sizeof(a_Type) > VMMDEV_TESTING_READBACK_SIZE) \
                    off = VMMDEV_TESTING_READBACK_SIZE - sizeof(a_Type); \
                a_Worker((a_Type *)&MmioReg.pb[off]); \
                TMPL_NM(bs3FpuState1_Save)(pChecking); \
            } while (0)
# undef  CHECK_READBACK_READ
# define CHECK_READBACK_READ(a_Instr, a_Worker, a_Type) \
            CHECK_READBACK_READ_RUN(a_Instr, a_Worker, a_Type); \
            CHECK_STATE(a_Instr)


        /* The tests. */
        CHECK_READBACK_WRITE_Z(SIDT,     ASMGetIDTR,                         RTIDTR);
        CHECK_READBACK_WRITE_Z(FNSTENV,  TMPL_NM(bs3FpuState1_FNStEnv),      X86FSTENV32P); /** @todo x86.h is missing types */
        CHECK_READBACK_WRITE(  MOVDQU,   TMPL_NM(bs3FpuState1_MovDQU_Write), X86XMMREG);
        CHECK_READBACK_READ(   MOVDQU,   TMPL_NM(bs3FpuState1_MovDQU_Read),  X86XMMREG);

        /* Using the FPU is a little complicated, but we really need to check these things. */
        CHECK_READBACK_READ_RUN(FMUL,    TMPL_NM(bs3FpuState1_FMul),         uint64_t);
        pExpected->FOP    = 0x7dc;
# if ARCH_BITS == 64
        pExpected->FPUDP  = (uint32_t) (uintptr_t)&MmioReg.pb[off];
        pExpected->DS     = (uint16_t)((uintptr_t)&MmioReg.pb[off] >> 32);
        pExpected->Rsrvd2 = (uint16_t)((uintptr_t)&MmioReg.pb[off] >> 48);
# elif BS3_MODE_IS_RM_OR_V86(TMPL_MODE)
        pExpected->FPUDP  = Bs3SelPtrToFlat(&MmioReg.pb[off]);
# else
        pExpected->FPUDP  = BS3_FP_OFF(&MmioReg.pb[off]);
# endif
        CHECK_STATE(FMUL);

        /* check for timeout every now an then. */
        if (   (iLoop & 0xfff) == 0xfff
            && g_cBs3PitTicks - uStartTick >= 20 * 20) /* 20 seconds*/
            break;
    }

    Bs3PitDisable();
    return 0;
}
# endif
#endif /* BS3_INSTANTIATING_MODE */

