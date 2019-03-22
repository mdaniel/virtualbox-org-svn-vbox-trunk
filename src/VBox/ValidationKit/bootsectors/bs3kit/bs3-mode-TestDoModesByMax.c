/* $Id$ */
/** @file
 * BS3Kit - Bs3TestDoModesByMax
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
#if TMPL_MODE == BS3_MODE_RM
# define BS3_USE_RM_TEXT_SEG 1 /* Real mode version in RMTEXT16 segment to save space. */
# include "bs3kit-template-header.h"
# include "bs3-cmn-test.h"
#else
# include "bs3kit-template-header.h"
# include "bs3-cmn-test.h"
#endif
#include "bs3-mode-TestDoModes.h"



/**
 * Warns about CPU modes that must be skipped.
 *
 * It will try not warn about modes for which there are no tests.
 *
 * @param   paEntries       The mode test entries.
 * @param   cEntries        The number of tests.
 * @param   bCpuType        The CPU type byte (see #BS3CPU_TYPE_MASK).
 * @param   fHavePae        Whether the CPU has PAE.
 * @param   fHaveLongMode   Whether the CPU does long mode.
 */
static void bs3TestWarnAboutSkippedModes(PCBS3TESTMODEBYMAXENTRY paEntries, unsigned cEntries,
                                         uint8_t bCpuType, bool fHavePae, bool fHaveLongMode)
{
    bool           fComplained286   = false;
    bool           fComplained386   = false;
    bool           fComplainedPAE   = false;
    bool           fComplainedAMD64 = false;
    unsigned       i;

    /*
     * Complaint run.
     */
    for (i = 0; i < cEntries; i++)
    {
        if (   !fComplained286
            &&  paEntries[i].pfnDoPE16)
        {
            if (bCpuType < BS3CPU_80286)
            {
                Bs3Printf("Only executing real-mode tests as no 80286+ CPU was detected.\n");
                break;
            }
            fComplained286 = true;
        }

        if (   !fComplained386
            &&  (   paEntries[i].fDoPE16_32
                 || paEntries[i].fDoPE16_V86
                 || paEntries[i].fDoPE32
                 || paEntries[i].fDoPE32_16
                 || paEntries[i].fDoPEV86
                 || paEntries[i].fDoPP16
                 || paEntries[i].fDoPP16_32
                 || paEntries[i].fDoPP16_V86
                 || paEntries[i].fDoPP32
                 || paEntries[i].fDoPP32_16
                 || paEntries[i].fDoPPV86) )
        {
            if (bCpuType < BS3CPU_80386)
            {
                Bs3Printf("80286 CPU: Only executing 16-bit protected and real mode tests.\n");
                break;
            }
            fComplained386 = true;
        }

        if (   !fComplainedPAE
            &&  (   paEntries[i].fDoPAE16
                 || paEntries[i].fDoPAE16_32
                 || paEntries[i].fDoPAE16_V86
                 || paEntries[i].fDoPAE32
                 || paEntries[i].fDoPAE32_16
                 || paEntries[i].fDoPAEV86) )
        {
            if (!fHavePae)
            {
                Bs3Printf("PAE and long mode tests will be skipped.\n");
                break;
            }
            fComplainedPAE = true;
        }

        if (   !fComplainedAMD64
            &&  (   paEntries[i].fDoLM16
                 || paEntries[i].fDoLM32
                 || paEntries[i].fDoLM64) )
        {
            if (!fHaveLongMode)
            {
                Bs3Printf("Long mode tests will be skipped.\n");
                break;
            }
            fComplainedAMD64 = true;
        }
    }
}

#undef Bs3TestDoModesByMax
BS3_MODE_DEF(void, Bs3TestDoModesByMax,(PCBS3TESTMODEBYMAXENTRY paEntries, size_t cEntries))
{
    bool const      fVerbose         = true;
    bool const      fDoV86Modes      = true;
    bool const      fDoWeirdV86Modes = true;
    uint16_t const  uCpuDetected  = g_uBs3CpuDetected;
    uint8_t const   bCpuType      = uCpuDetected & BS3CPU_TYPE_MASK;
    bool const      fHavePae      = RT_BOOL(uCpuDetected & BS3CPU_F_PAE);
    bool const      fHaveLongMode = RT_BOOL(uCpuDetected & BS3CPU_F_LONG_MODE);
    unsigned        i;

#if 1 /* debug. */
    Bs3Printf("Bs3TestDoModes: uCpuDetected=%#x fHavePae=%d fHaveLongMode=%d\n", uCpuDetected, fHavePae, fHaveLongMode);
#endif
    bs3TestWarnAboutSkippedModes(paEntries, cEntries, bCpuType, fHavePae, fHaveLongMode);

    /*
     * The real run.
     */
    for (i = 0; i < cEntries; i++)
    {
        const char *pszFmtStr = "Error #%u (%#x) in %s!\n";
        bool        fSkipped  = true;
        uint8_t     bErrNo;

        if (paEntries[i].pszSubTest != NULL)
            Bs3TestSub(paEntries[i].pszSubTest);

#define PRE_DO_CALL(a_szModeName) do { if (fVerbose) Bs3TestPrintf("...%s\n", a_szModeName); } while (0)
#define CHECK_RESULT(a_szModeName) \
            do { \
                if (bErrNo != BS3TESTDOMODE_SKIPPED) \
                { \
                    /*Bs3Printf("bErrNo=%#x %s\n", bErrNo, a_szModeName);*/ \
                    fSkipped = false; \
                    if (bErrNo != 0) \
                        Bs3TestFailedF(pszFmtStr, bErrNo, bErrNo, a_szModeName); \
                } \
            } while (0)

        if (paEntries[i].fDoRM)
        {
            PRE_DO_CALL(g_szBs3ModeName_rm);
            bErrNo = TMPL_NM(Bs3TestCallDoerInRM)(CONV_TO_RM_FAR16(paEntries[i].pfnDoRM));
            CHECK_RESULT(g_szBs3ModeName_rm);
        }

        if (bCpuType < BS3CPU_80286)
        {
            if (fSkipped)
                Bs3TestSkipped(NULL);
            continue;
        }

        /*
         * Unpaged prot mode.
         */
        if (paEntries[i].fDoPE16)
        {
            PRE_DO_CALL(g_szBs3ModeName_pe16);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPE16)(CONV_TO_PROT_FAR16(paEntries[i].pfnDoPE16));
            CHECK_RESULT(g_szBs3ModeName_pe16);
        }
        if (bCpuType < BS3CPU_80386)
        {
            if (fSkipped)
                Bs3TestSkipped(NULL);
            continue;
        }

        if (paEntries[i].fDoPE16_32)
        {
            PRE_DO_CALL(g_szBs3ModeName_pe16_32);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPE16_32)(CONV_TO_FLAT(paEntries[i].pfnDoPE16_32), BS3_MODE_PE16_32);
            CHECK_RESULT(g_szBs3ModeName_pe16_32);
        }

        if (paEntries[i].fDoPE16_V86 && fDoWeirdV86Modes)
        {
            PRE_DO_CALL(g_szBs3ModeName_pe16_v86);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPE16_32)(CONV_TO_FLAT(paEntries[i].pfnDoPE16_32), BS3_MODE_PE16_V86);
            CHECK_RESULT(g_szBs3ModeName_pe16_v86);
        }

        if (paEntries[i].fDoPE32)
        {
            PRE_DO_CALL(g_szBs3ModeName_pe32);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPE32)(CONV_TO_FLAT(paEntries[i].pfnDoPE32), BS3_MODE_PE32);
            CHECK_RESULT(g_szBs3ModeName_pe32);
        }

        if (paEntries[i].fDoPE32_16)
        {
            PRE_DO_CALL(g_szBs3ModeName_pe32_16);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPE32)(CONV_TO_FLAT(paEntries[i].pfnDoPE32), BS3_MODE_PE32_16);
            CHECK_RESULT(g_szBs3ModeName_pe32_16);
        }

        if (paEntries[i].fDoPEV86 && fDoV86Modes)
        {
            PRE_DO_CALL(g_szBs3ModeName_pev86);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPE32)(CONV_TO_FLAT(paEntries[i].pfnDoPE32), BS3_MODE_PEV86);
            CHECK_RESULT(g_szBs3ModeName_pev86);
        }

        /*
         * Paged protected mode.
         */
        if (paEntries[i].fDoPP16)
        {
            PRE_DO_CALL(g_szBs3ModeName_pp16);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPP16_32)(CONV_TO_FLAT(paEntries[i].pfnDoPP16_32), BS3_MODE_PP16);
            CHECK_RESULT(g_szBs3ModeName_pp16);
        }

        if (paEntries[i].fDoPP16_32)
        {
            PRE_DO_CALL(g_szBs3ModeName_pp16_32);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPP16_32)(CONV_TO_FLAT(paEntries[i].pfnDoPP16_32), BS3_MODE_PP16_32);
            CHECK_RESULT(g_szBs3ModeName_pp16_32);
        }

        if (paEntries[i].fDoPP16_V86 && fDoWeirdV86Modes)
        {
            PRE_DO_CALL(g_szBs3ModeName_pp16_v86);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPP16_32)(CONV_TO_FLAT(paEntries[i].pfnDoPP16_32), BS3_MODE_PP16_V86);
            CHECK_RESULT(g_szBs3ModeName_pp16_v86);
        }

        if (paEntries[i].fDoPP32)
        {
            PRE_DO_CALL(g_szBs3ModeName_pp32);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPP32)(CONV_TO_FLAT(paEntries[i].pfnDoPP32), BS3_MODE_PP32);
            CHECK_RESULT(g_szBs3ModeName_pp32);
        }

        if (paEntries[i].fDoPP32_16)
        {
            PRE_DO_CALL(g_szBs3ModeName_pp32_16);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPP32)(CONV_TO_FLAT(paEntries[i].pfnDoPP32), BS3_MODE_PP32_16);
            CHECK_RESULT(g_szBs3ModeName_pp32_16);
        }

        if (paEntries[i].fDoPPV86 && fDoV86Modes)
        {
            PRE_DO_CALL(g_szBs3ModeName_ppv86);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPP32)(CONV_TO_FLAT(paEntries[i].pfnDoPP32), BS3_MODE_PPV86);
            CHECK_RESULT(g_szBs3ModeName_ppv86);
        }

        /*
         * Protected mode with PAE paging.
         */
        if (!fHavePae)
        {
            if (fSkipped)
                Bs3TestSkipped(NULL);
            continue;
        }

        if (paEntries[i].fDoPAE16)
        {
            PRE_DO_CALL(g_szBs3ModeName_pae16);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPAE16_32)(CONV_TO_FLAT(paEntries[i].pfnDoPAE16_32), BS3_MODE_PAE16);
            CHECK_RESULT(g_szBs3ModeName_pae16);
        }

        if (paEntries[i].fDoPAE16_32)
        {
            PRE_DO_CALL(g_szBs3ModeName_pae16_32);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPAE16_32)(CONV_TO_FLAT(paEntries[i].pfnDoPAE16_32), BS3_MODE_PAE16_32);
            CHECK_RESULT(g_szBs3ModeName_pae16_32);
        }

        if (paEntries[i].fDoPAE16_V86 && fDoWeirdV86Modes)
        {
            PRE_DO_CALL(g_szBs3ModeName_pae16_v86);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPAE16_32)(CONV_TO_FLAT(paEntries[i].pfnDoPAE16_32), BS3_MODE_PAE16_V86);
            CHECK_RESULT(g_szBs3ModeName_pae16_v86);
        }

        if (paEntries[i].fDoPAE32)
        {
            PRE_DO_CALL(g_szBs3ModeName_pae32);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPAE32)(CONV_TO_FLAT(paEntries[i].pfnDoPAE32), BS3_MODE_PAE32);
            CHECK_RESULT(g_szBs3ModeName_pae32);
        }

        if (paEntries[i].fDoPAE32_16)
        {
            PRE_DO_CALL(g_szBs3ModeName_pae32_16);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPAE32)(CONV_TO_FLAT(paEntries[i].pfnDoPAE32), BS3_MODE_PAE32_16);
            CHECK_RESULT(g_szBs3ModeName_pae32_16);
        }

        if (paEntries[i].fDoPAEV86 && fDoV86Modes)
        {
            PRE_DO_CALL(g_szBs3ModeName_paev86);
            bErrNo = TMPL_NM(Bs3TestCallDoerInPAE32)(CONV_TO_FLAT(paEntries[i].pfnDoPAE32), BS3_MODE_PAEV86);
            CHECK_RESULT(g_szBs3ModeName_paev86);
        }

        /*
         * Long mode.
         */
        if (!fHaveLongMode)
        {
            if (fSkipped)
                Bs3TestSkipped(NULL);
            continue;
        }

        if (paEntries[i].fDoLM16)
        {
            PRE_DO_CALL(g_szBs3ModeName_lm16);
            bErrNo = TMPL_NM(Bs3TestCallDoerInLM64)(CONV_TO_FLAT(paEntries[i].pfnDoLM64), BS3_MODE_LM16);
            CHECK_RESULT(g_szBs3ModeName_lm16);
        }

        if (paEntries[i].fDoLM32)
        {
            PRE_DO_CALL(g_szBs3ModeName_lm32);
            bErrNo = TMPL_NM(Bs3TestCallDoerInLM64)(CONV_TO_FLAT(paEntries[i].pfnDoLM64), BS3_MODE_LM32);
            CHECK_RESULT(g_szBs3ModeName_lm32);
        }

        if (paEntries[i].fDoLM64)
        {
            PRE_DO_CALL(g_szBs3ModeName_lm64);
            bErrNo = TMPL_NM(Bs3TestCallDoerInLM64)(CONV_TO_FLAT(paEntries[i].pfnDoLM64), BS3_MODE_LM64);
            CHECK_RESULT(g_szBs3ModeName_lm64);
        }

        if (fSkipped)
            Bs3TestSkipped("skipped\n");
    }
    Bs3TestSubDone();
}

