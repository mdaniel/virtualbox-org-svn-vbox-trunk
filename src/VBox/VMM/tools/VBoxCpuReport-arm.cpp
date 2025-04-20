/* $Id$ */
/** @file
 * VBoxCpuReport - Produces the basis for a CPU DB entry, x86 specifics.
 */

/*
 * Copyright (C) 2013-2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/ctype.h>
#include <iprt/message.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/sort.h>

#include <VBox/err.h>
#include <VBox/vmm/cpum.h>
#include <VBox/sup.h>

#include "VBoxCpuReport.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct PARTNUMINFO
{
    uint32_t        uPartNum;
    CPUMMICROARCH   enmMicroarch;
    const char     *pszName;
    const char     *pszFullName;
    CPUMCORETYPE    enmCoreType;
} PARTNUMINFO;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static struct
{
    RTCPUSET            bmMembers;
    uint32_t            cCores;
    uint32_t            cSysRegVals;
    SUPARMSYSREGVAL     aSysRegVals[256];
}                       g_aVariations[RTCPUSET_MAX_CPUS];
static uint32_t         g_cVariations    = 0;
static uint32_t         g_cCores         = 0;

static uint32_t         g_cCmnSysRegVals = 0;
static SUPARMSYSREGVAL  g_aCmnSysRegVals[256];


/** ARM CPU info by part number. */
static PARTNUMINFO const g_aPartNumDbArm[] =
{
    { 0xfff,    kCpumMicroarch_Unknown,             "TODO",                 "TODO" },
};

/** Broadcom CPU info by part number. */
static PARTNUMINFO const g_aPartNumDbBroadcom[] =
{
    { 0xfff,    kCpumMicroarch_Unknown,             "TODO",                 "TODO" },
};

/** Qualcomm CPU info by part number. */
static PARTNUMINFO const g_aPartNumDbQualcomm[] =
{
    { 0x0d4b,   kCpumMicroarch_Qualcomm_Kyro,       "Snapdragon 8cx Gen 3", "Snapdragon 8cx Gen 3 (Kryo Prime)",   kCpumCoreType_Efficiency  }, /* Guessing which part */    /*MIDR_EL1=0x410FD4B0*/
    { 0x0d4c,   kCpumMicroarch_Qualcomm_Kyro,       "Snapdragon 8cx Gen 3", "Snapdragon 8cx Gen 3 (Kryo Gold)",    kCpumCoreType_Performance }, /* is for which core... */   /*MIDR_EL1=0x410FD4C0*/
    { 0x1001,   kCpumMicroarch_Qualcomm_Oryon,      "Snapdragon X",         "Snapdragon X (Oryon var 1)",          kCpumCoreType_Unknown     }, /*MIDR_EL1=0x511f0011 (perf?)*/
    { 0x2001,   kCpumMicroarch_Qualcomm_Oryon,      "Snapdragon X",         "Snapdragon X (Oryon var 2)",          kCpumCoreType_Unknown     }, /*MIDR_EL1=0x512f0011 (eff?)*/
};

/** Apple CPU info by part number. */
static PARTNUMINFO const g_aPartNumDbApple[] =
{
    { 0x022,    kCpumMicroarch_Apple_M1,            "Apple M1",             "Apple M1 (Icestorm)",          kCpumCoreType_Efficiency  },
    { 0x023,    kCpumMicroarch_Apple_M1,            "Apple M1",             "Apple M1 (Firestorm)",         kCpumCoreType_Performance },
    { 0x024,    kCpumMicroarch_Apple_M1,            "Apple M1 Pro",         "Apple M1 Pro (Icestorm)",      kCpumCoreType_Efficiency  },
    { 0x025,    kCpumMicroarch_Apple_M1,            "Apple M1 Pro",         "Apple M1 Pro (Firestorm)",     kCpumCoreType_Performance },
    { 0x028,    kCpumMicroarch_Apple_M1,            "Apple M1 Max",         "Apple M1 Max (Icestorm)",      kCpumCoreType_Efficiency  },
    { 0x029,    kCpumMicroarch_Apple_M1,            "Apple M1 Max",         "Apple M1 Max (Firestorm)",     kCpumCoreType_Performance },
    { 0x032,    kCpumMicroarch_Apple_M2,            "Apple M2",             "Apple M2 (Blizzard)",          kCpumCoreType_Efficiency  },
    { 0x033,    kCpumMicroarch_Apple_M2,            "Apple M2",             "Apple M2 (Avalanche)",         kCpumCoreType_Performance },
    { 0x034,    kCpumMicroarch_Apple_M2,            "Apple M2 Pro",         "Apple M2 Pro (Blizzard)",      kCpumCoreType_Efficiency  },
    { 0x035,    kCpumMicroarch_Apple_M2,            "Apple M2 Pro",         "Apple M2 Pro (Avalanche)",     kCpumCoreType_Performance },
    { 0x038,    kCpumMicroarch_Apple_M2,            "Apple M2 Max",         "Apple M2 Max (Blizzard)",      kCpumCoreType_Efficiency  },
    { 0x039,    kCpumMicroarch_Apple_M2,            "Apple M2 Max",         "Apple M2 Max (Avalanche)",     kCpumCoreType_Performance },
};

/** Ampere CPU info by part number. */
static PARTNUMINFO const g_aPartNumDbAmpere[] =
{
    { 0xfff,    kCpumMicroarch_Unknown,             "TODO",                 "TODO" },
};


/** Looks up a register entry in an array. */
static SUPARMSYSREGVAL *lookupSysReg(SUPARMSYSREGVAL *paSysRegVals, uint32_t const cSysRegVals, uint32_t const idReg)
{
    for (uint32_t i = 0; i < cSysRegVals; i++)
        if (paSysRegVals[i].idReg == idReg)
            return &paSysRegVals[i];
    return NULL;
}


/** Looks up a register value in g_aSysRegVals. */
static uint64_t getSysRegVal(uint32_t idReg, uint64_t uNotFoundValue = 0, uint32_t iVar = 0)
{
    for (uint32_t i = 0; i < g_cCmnSysRegVals; i++)
        if (g_aCmnSysRegVals[i].idReg == idReg)
            return g_aCmnSysRegVals[i].uValue;

    if (iVar < g_cVariations)
        for (uint32_t i = 0; i < g_aVariations[iVar].cSysRegVals; i++)
            if (g_aVariations[iVar].aSysRegVals[i].idReg == idReg)
                return g_aVariations[iVar].aSysRegVals[i].uValue;

    return uNotFoundValue;
}


/**
 * Translates system register ID to a string, returning NULL if we can't.
 */
static const char *sysRegNoToName(uint32_t idReg)
{
    switch (idReg)
    {
        /* The stuff here is copied from SUPDrv.cpp and trimmed down to the reads: */
#define READ_SYS_REG_NAMED(a_Op0, a_Op1, a_CRn, a_CRm, a_Op2, a_SysRegName) \
        case ARMV8_AARCH64_SYSREG_ID_CREATE(a_Op0, a_Op1, a_CRn, a_CRm, a_Op2): return #a_SysRegName
#define READ_SYS_REG__TODO(a_Op0, a_Op1, a_CRn, a_CRm, a_Op2, a_SysRegName) \
        case ARMV8_AARCH64_SYSREG_ID_CREATE(a_Op0, a_Op1, a_CRn, a_CRm, a_Op2): return #a_SysRegName
#define READ_SYS_REG_UNDEF(a_Op0, a_Op1, a_CRn, a_CRm, a_Op2) \
        case ARMV8_AARCH64_SYSREG_ID_CREATE(a_Op0, a_Op1, a_CRn, a_CRm, a_Op2): return NULL

        READ_SYS_REG_NAMED(3, 0, 0, 0, 0, MIDR_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 0, 5, MPIDR_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 0, 6, REVIDR_EL1);
        READ_SYS_REG__TODO(3, 1, 0, 0, 0, CCSIDR_EL1);
        READ_SYS_REG__TODO(3, 1, 0, 0, 1, CLIDR_EL1);
        READ_SYS_REG__TODO(3, 1, 0, 0, 7, AIDR_EL1);
        READ_SYS_REG_NAMED(3, 3, 0, 0, 7, DCZID_EL0);
        READ_SYS_REG_NAMED(3, 3,14, 0, 0, CNTFRQ_EL0);


        READ_SYS_REG_NAMED(3, 0, 0, 4, 0, ID_AA64PFR0_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 4, 1, ID_AA64PFR1_EL1);
        READ_SYS_REG_UNDEF(3, 0, 0, 4, 2);
        READ_SYS_REG_UNDEF(3, 0, 0, 4, 3);
        READ_SYS_REG_NAMED(3, 0, 0, 4, 4, ID_AA64ZFR0_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 4, 5, ID_AA64SMFR0_EL1);
        READ_SYS_REG_UNDEF(3, 0, 0, 4, 6);
        READ_SYS_REG_UNDEF(3, 0, 0, 4, 7);

        READ_SYS_REG_NAMED(3, 0, 0, 5, 0, ID_AA64DFR0_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 5, 1, ID_AA64DFR1_EL1);
        READ_SYS_REG_UNDEF(3, 0, 0, 5, 2);
        READ_SYS_REG_UNDEF(3, 0, 0, 5, 3);
        READ_SYS_REG_NAMED(3, 0, 0, 5, 4, ID_AA64AFR0_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 5, 5, ID_AA64AFR1_EL1);
        READ_SYS_REG_UNDEF(3, 0, 0, 5, 6);
        READ_SYS_REG_UNDEF(3, 0, 0, 5, 7);

        READ_SYS_REG_NAMED(3, 0, 0, 6, 0, ID_AA64ISAR0_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 6, 1, ID_AA64ISAR1_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 6, 2, ID_AA64ISAR2_EL1);
        READ_SYS_REG__TODO(3, 0, 0, 6, 3, ID_AA64ISAR3_EL1);
        READ_SYS_REG_UNDEF(3, 0, 0, 6, 4);
        READ_SYS_REG_UNDEF(3, 0, 0, 6, 5);
        READ_SYS_REG_UNDEF(3, 0, 0, 6, 6);
        READ_SYS_REG_UNDEF(3, 0, 0, 6, 7);

        READ_SYS_REG_NAMED(3, 0, 0, 7, 0, ID_AA64MMFR0_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 7, 1, ID_AA64MMFR1_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 7, 2, ID_AA64MMFR2_EL1);
        READ_SYS_REG__TODO(3, 0, 0, 7, 3, ID_AA64MMFR3_EL1);
        READ_SYS_REG__TODO(3, 0, 0, 7, 4, ID_AA64MMFR4_EL1);
        READ_SYS_REG_UNDEF(3, 0, 0, 7, 5);
        READ_SYS_REG_UNDEF(3, 0, 0, 7, 6);
        READ_SYS_REG_UNDEF(3, 0, 0, 7, 7);

        READ_SYS_REG_NAMED(3, 0, 0, 1, 0, ID_PFR0_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 1, 1, ID_PFR1_EL1);

        READ_SYS_REG_NAMED(3, 0, 0, 1, 2, ID_DFR0_EL1);

        READ_SYS_REG_NAMED(3, 0, 0, 1, 3, ID_AFR0_EL1);

        READ_SYS_REG_NAMED(3, 0, 0, 1, 4, ID_MMFR0_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 1, 5, ID_MMFR1_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 1, 6, ID_MMFR2_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 1, 7, ID_MMFR3_EL1);

        READ_SYS_REG_NAMED(3, 0, 0, 2, 0, ID_ISAR0_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 2, 1, ID_ISAR1_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 2, 2, ID_ISAR2_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 2, 3, ID_ISAR3_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 2, 4, ID_ISAR4_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 2, 5, ID_ISAR5_EL1);

        READ_SYS_REG_NAMED(3, 0, 0, 2, 6, ID_MMFR4_EL1);

        READ_SYS_REG_NAMED(3, 0, 0, 2, 7, ID_ISAR6_EL1);

        READ_SYS_REG_NAMED(3, 0, 0, 3, 0, MVFR0_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 3, 1, MVFR1_EL1);
        READ_SYS_REG_NAMED(3, 0, 0, 3, 2, MVFR2_EL1);

        READ_SYS_REG_NAMED(3, 0, 0, 3, 4, ID_PFR2_EL1);

        READ_SYS_REG_NAMED(3, 0, 0, 3, 5, ID_DFR1_EL1);

        READ_SYS_REG_NAMED(3, 0, 0, 3, 6, ID_MMFR5_EL1);

        READ_SYS_REG__TODO(3, 1, 0, 0, 2, CCSIDR2_EL1); /*?*/

        READ_SYS_REG_NAMED(3, 0, 5, 3, 0, ERRIDR_EL1);

        READ_SYS_REG__TODO(3, 1, 0, 0, 4, GMID_EL1);

        READ_SYS_REG__TODO(3, 0, 10, 4, 4, MPAMIDR_EL1);
        READ_SYS_REG__TODO(3, 0, 10, 4, 5, MPAMBWIDR_EL1);

        READ_SYS_REG__TODO(3, 0, 9, 10, 7, PMBIDR_EL1);
        READ_SYS_REG__TODO(3, 0, 9,  8, 7, PMSIDR_EL1);

        READ_SYS_REG__TODO(3, 0, 9, 11, 7, TRBIDR_EL1);

        READ_SYS_REG__TODO(2, 1, 0, 8, 7, TRCIDR0); /*?*/
        READ_SYS_REG__TODO(2, 1, 0, 9, 7, TRCIDR1);
        READ_SYS_REG__TODO(2, 1, 0,10, 7, TRCIDR2);
        READ_SYS_REG__TODO(2, 1, 0,11, 7, TRCIDR3);
        READ_SYS_REG__TODO(2, 1, 0,12, 7, TRCIDR4);
        READ_SYS_REG__TODO(2, 1, 0,13, 7, TRCIDR5);
        READ_SYS_REG__TODO(2, 1, 0,14, 7, TRCIDR6);
        READ_SYS_REG__TODO(2, 1, 0,15, 7, TRCIDR7);
        READ_SYS_REG__TODO(2, 1, 0, 0, 6, TRCIDR8);
        READ_SYS_REG__TODO(2, 1, 0, 1, 6, TRCIDR9);
        READ_SYS_REG__TODO(2, 1, 0, 2, 6, TRCIDR10);
        READ_SYS_REG__TODO(2, 1, 0, 3, 6, TRCIDR11);
        READ_SYS_REG__TODO(2, 1, 0, 4, 6, TRCIDR12);
        READ_SYS_REG__TODO(2, 1, 0, 5, 6, TRCIDR13);

#undef READ_SYS_REG_NAMED
#undef READ_SYS_REG__TODO
#undef READ_SYS_REG_UNDEF
    }
    return NULL;
}


/** @callback_impl{FNRTSORTCMP} */
static DECLCALLBACK(int) sysRegValSortCmp(void const *pvElement1, void const *pvElement2, void *pvUser)
{
    RT_NOREF(pvUser);
    PCSUPARMSYSREGVAL const pElm1 = (PCSUPARMSYSREGVAL)pvElement1;
    PCSUPARMSYSREGVAL const pElm2 = (PCSUPARMSYSREGVAL)pvElement2;
    return pElm1->idReg < pElm2->idReg ? -1 : pElm1->idReg > pElm2->idReg ? 1 : 0;
}


/**
 * Populates g_aSysRegVals and g_cSysRegVals
 */
static int populateSystemRegisters(void)
{
    /*
     * First try using the support driver.
     */
    int rc = SUPR3Init(NULL);
    if (RT_SUCCESS(rc))
    {
        /*
         * Get the registers for online each CPU in the system, sorting them.
         */
        RTCPUSET  OnlineSet;
        int const idxLast = RTCpuLastIndex(RTMpGetOnlineSet(&OnlineSet));
        for (int idxCpu = 0, iVar = 0; idxCpu <= idxLast; idxCpu++)
            if (RTCpuSetIsMemberByIndex(&OnlineSet, idxCpu))
            {
                RTCPUID const idCpu         = RTMpCpuIdFromSetIndex(idxCpu);
                uint32_t      cRegAvailable = 0;
                g_aVariations[iVar].cSysRegVals = 0;
                rc = SUPR3ArmQuerySysRegs(idCpu,
                                          SUP_ARM_SYS_REG_F_INC_ZERO_REG_VAL | SUP_ARM_SYS_REG_F_EXTENDED,
                                          RT_ELEMENTS(g_aVariations[iVar].aSysRegVals),
                                          &g_aVariations[iVar].cSysRegVals,
                                          &cRegAvailable,
                                          g_aVariations[iVar].aSysRegVals);
                vbCpuRepDebug("SUPR3ArmQuerySysRegs(%u/%u) -> %Rrc (%u/%u regs)\n",
                              idCpu, idxCpu, rc, g_aVariations[iVar].cSysRegVals, cRegAvailable);
                if (RT_FAILURE(rc))
                    return RTMsgErrorRc(rc, "SUPR3ArmQuerySysRegs failed: %Rrc", rc);
                if (cRegAvailable > g_aVariations[iVar].cSysRegVals)
                    return RTMsgErrorRc(rc,
                                        "SUPR3ArmQuerySysRegs claims there are %u more registers availble.\n"
                                        "Increase size of g_aSysRegVals to at least %u entries and retry!",
                                        cRegAvailable - g_aVariations[iVar].cSysRegVals, cRegAvailable);
                /* Sort it. */
                RTSortShell(g_aVariations[iVar].aSysRegVals, g_aVariations[iVar].cSysRegVals,
                            sizeof(g_aVariations[iVar].aSysRegVals[0]), sysRegValSortCmp, NULL);

                /* Sanitize the MP affinity register. */
                SUPARMSYSREGVAL *pReg = lookupSysReg(g_aVariations[iVar].aSysRegVals, g_aVariations[iVar].cSysRegVals,
                                                     ARMV8_AARCH64_SYSREG_MPIDR_EL1);
                if (pReg)
                {
                    pReg->uValue &= ~UINT64_C(0xff00ffffff); /* Zero the Aff3, Aff2, Aff1 & Aff0 fields. */
                    pReg->fFlags = 1;
                }

                /* Check if it's the same as an existing variation. */
                int iVarMatch;
                for (iVarMatch = iVar - 1; iVarMatch >= 0; iVarMatch--)
                    if (   g_aVariations[iVarMatch].cSysRegVals == g_aVariations[iVar].cSysRegVals
                        && memcmp(&g_aVariations[iVarMatch].aSysRegVals,
                                  &g_aVariations[iVar].aSysRegVals, g_aVariations[iVar].cSysRegVals) == 0)
                        break;
                if (iVarMatch >= 0)
                {
                    /* Add to existing */
                    vbCpuRepDebug("CPU %u/%u is same as variant #%u\n", idCpu, idxCpu, iVarMatch);
                    g_aVariations[iVarMatch].cCores += 1;
                    RTCpuSetAddByIndex(&g_aVariations[iVarMatch].bmMembers, idxCpu);
                }
                else
                {
                    vbCpuRepDebug("CPU %u/%u is a new variant #%u\n", idCpu, idxCpu, iVar);
                    g_aVariations[iVar].cCores = 1;
                    RTCpuSetEmpty(&g_aVariations[iVar].bmMembers);
                    RTCpuSetAddByIndex(&g_aVariations[iVar].bmMembers, idxCpu);

                    /* Set remaining entries to 0xffff to guard against trouble below when
                       finding common register values. */
                    for (uint32_t i = g_aVariations[iVar].cSysRegVals; i < RT_ELEMENTS(g_aVariations[iVar].aSysRegVals); i++)
                    {
                        g_aVariations[iVar].aSysRegVals[i].idReg  = UINT32_MAX;
                        g_aVariations[iVar].aSysRegVals[i].uValue = 0;
                        g_aVariations[iVar].aSysRegVals[i].fFlags = 0;
                    }

                    g_cVariations = ++iVar;
                }
                g_cCores += 1;
            }
        vbCpuRepDebug("Detected %u variants across %u online CPUs\n", g_cVariations, g_cCores);

        /*
         * Now, destill similar register values and unique ones.
         * This isn't too complicated since the arrays have been sorted.
         */
        g_cCmnSysRegVals = 0;

        uint32_t cMaxRegs = g_aVariations[0].cSysRegVals;
        for (unsigned i = 0; i < g_cVariations; i++)
            cMaxRegs = RT_MAX(cMaxRegs, g_aVariations[i].cSysRegVals);

        struct
        {
            unsigned idxSrc;
            unsigned idxDst;
        } aState[RTCPUSET_MAX_CPUS] = { {0, 0} };

        for (;;)
        {
            /* Find the min & max register value. */
            uint32_t idRegMax = 0;
            uint32_t idRegMin = UINT32_MAX;
            for (unsigned iVar = 0; iVar < g_cVariations; iVar++)
            {
                unsigned const idxSrc = aState[iVar].idxSrc;

                uint32_t const idReg  = idxSrc < g_aVariations[iVar].cSysRegVals
                                      ? g_aVariations[iVar].aSysRegVals[idxSrc].idReg : UINT32_MAX;
                idRegMax = RT_MAX(idRegMax, idReg);
                idRegMin = RT_MIN(idRegMin, idReg);
            }
            if (idRegMin == UINT32_MAX)
                break;

            /* Advance all arrays till we've reached idRegMax. */
            unsigned cMatchedMax = 0;
            for (unsigned iVar = 0; iVar < g_cVariations; iVar++)
            {
                unsigned idxSrc = aState[iVar].idxSrc;
                unsigned idxDst = aState[iVar].idxDst;
                while (   idxSrc < g_aVariations[iVar].cSysRegVals
                       && g_aVariations[iVar].aSysRegVals[idxSrc].idReg < idRegMax)
                    g_aVariations[iVar].aSysRegVals[idxDst++] = g_aVariations[iVar].aSysRegVals[idxSrc++];
                cMatchedMax += idxSrc < g_aVariations[iVar].cSysRegVals
                            && g_aVariations[iVar].aSysRegVals[idxSrc].idReg == idRegMax;
                aState[iVar].idxSrc = idxSrc;
                aState[iVar].idxDst = idxDst;
            }
            if (idRegMax == UINT32_MAX)
                break;

            if (cMatchedMax == g_cVariations)
            {
                /* Check if all the values match. */
                uint64_t const uValue0  = g_aVariations[0].aSysRegVals[aState[0].idxSrc].uValue;
                uint32_t const fFlags0  = g_aVariations[0].aSysRegVals[aState[0].idxSrc].fFlags;
                unsigned       cMatches = 1;
                for (unsigned iVar = 1; iVar < g_cVariations; iVar++)
                {
                    unsigned const idxSrc = aState[iVar].idxSrc;
                    Assert(idxSrc < g_aVariations[iVar].cSysRegVals);
                    Assert(g_aVariations[iVar].aSysRegVals[idxSrc].idReg == idRegMax);
                    cMatches += g_aVariations[iVar].aSysRegVals[idxSrc].uValue == uValue0
                             && g_aVariations[iVar].aSysRegVals[idxSrc].fFlags == fFlags0;
                }
                if (cMatches == g_cVariations)
                {
                    g_aCmnSysRegVals[g_cCmnSysRegVals++] = g_aVariations[0].aSysRegVals[aState[0].idxSrc];
                    for (unsigned iVar = 0; iVar < g_cVariations; iVar++)
                        aState[iVar].idxSrc += 1;
                    continue;
                }
                vbCpuRepDebug("%#x: missed #2\n", idRegMax);
            }
            else
                vbCpuRepDebug("%#x: missed #1\n", idRegMax);

            for (unsigned iVar = 0; iVar < g_cVariations; iVar++)
            {
                Assert(aState[iVar].idxSrc < g_aVariations[iVar].cSysRegVals);
                g_aVariations[iVar].aSysRegVals[aState[iVar].idxDst++]
                    = g_aVariations[iVar].aSysRegVals[aState[iVar].idxSrc++];
            }
        }
        vbCpuRepDebug("Common register values: %u\n", g_cCmnSysRegVals);

        /* Anything left in any of the arrays are considered unique and needs to be moved up. */
        for (unsigned iVar = 0; iVar < g_cVariations; iVar++)
        {
            unsigned idxSrc = aState[iVar].idxSrc;
            unsigned idxDst = aState[iVar].idxDst;
            Assert(idxDst <= idxSrc);
            Assert(idxSrc == g_aVariations[iVar].cSysRegVals);
            while (idxSrc < g_aVariations[iVar].cSysRegVals)
                g_aVariations[iVar].aSysRegVals[idxDst++] = g_aVariations[iVar].aSysRegVals[idxSrc++];
            g_aVariations[iVar].cSysRegVals = idxDst;
            vbCpuRepDebug("Var #%u register values: %u\n", iVar, idxDst);
        }
        return rc;
    }
    return RTMsgErrorRc(rc, "Unable to initialize the support library (%Rrc).", rc);
    //vbCpuRepDebug("warning: Unable to initialize the support library (%Rrc).\n", rc);
    /** @todo On Linux we can query the registers exposed to ring-3... */
}


static void printSysRegArray(const char *pszNameC, uint32_t cSysRegVals, SUPARMSYSREGVAL const *paSysRegVals,
                             const char *pszCpuDesc, uint32_t iVariation = UINT32_MAX)
{
    if (!g_cCmnSysRegVals)
        return;

    vbCpuRepPrintf("\n"
                   "/*\n");
    if (iVariation == UINT32_MAX)
        vbCpuRepPrintf(" * Common system register values for %s.\n"
                       " */\n"
                       "static SUPARMSYSREGVAL const g_aCmnSysRegVals_%s[] =\n"
                       "{\n",
                       pszCpuDesc, pszNameC);
    else
    {
        vbCpuRepPrintf(" * System register values for %s, variation #%u.\n"
                       " * %u CPUs shares this variant: ",
                       pszCpuDesc, iVariation,
                       g_aVariations[iVariation].cCores);
        int iLast = RTCpuLastIndex(&g_aVariations[iVariation].bmMembers);
        for (int i = 0, cPrinted = 0; i <= iLast; i++)
            if (RTCpuSetIsMemberByIndex(&g_aVariations[iVariation].bmMembers, i))
                vbCpuRepPrintf(cPrinted++ == 0 ? "%u" : ", %u", i);
        vbCpuRepPrintf("\n"
                       " */\n"
                       "static SUPARMSYSREGVAL const g_aVar%uSysRegVals_%s[] =\n"
                       "{\n",
                       iVariation, pszNameC);
    }
    for (uint32_t i = 0; i < cSysRegVals; i++)
    {
        uint32_t const     idReg = paSysRegVals[i].idReg;
        uint32_t const     uOp0  = ARMV8_AARCH64_SYSREG_OP0_GET(idReg);
        uint32_t const     uOp1  = ARMV8_AARCH64_SYSREG_OP1_GET(idReg);
        uint32_t const     uCRn  = ARMV8_AARCH64_SYSREG_CRN_GET(idReg);
        uint32_t const     uCRm  = ARMV8_AARCH64_SYSREG_CRM_GET(idReg);
        uint32_t const     uOp2  = ARMV8_AARCH64_SYSREG_OP2_GET(idReg);
        const char * const pszNm = sysRegNoToName(idReg);

        vbCpuRepPrintf("    { UINT64_C(%#018RX64), ARMV8_AARCH64_SYSREG_ID_CREATE(%u, %u,%2u,%2u, %u), %#x },%s%s%s\n",
                paSysRegVals[i].uValue, uOp0, uOp1, uCRn, uCRm, uOp2, paSysRegVals[i].fFlags,
                pszNm ? " /* " : "", pszNm, pszNm ? " */" : "");
    }
    vbCpuRepPrintf("};\n"
                   "\n");
}


/**
 * Populate the system register array and output it.
 */
static int produceSysRegArray(const char *pszNameC, const char *pszCpuDesc)
{
    printSysRegArray(pszNameC, g_cCmnSysRegVals, g_aCmnSysRegVals, pszCpuDesc);
    for (uint32_t iVar = 0; iVar < g_cVariations; iVar++)
        printSysRegArray(pszNameC, g_aVariations[iVar].cSysRegVals, g_aVariations[iVar].aSysRegVals, pszCpuDesc, iVar);
    return VINF_SUCCESS;
}


int produceCpuReport(void)
{
    /*
     * Figure out the processor name via the host OS and command line first...
     */
    /** @todo HKLM/Hardware/...   */
    char szDetectedCpuName[256] = {0};
    int rc = RTMpGetDescription(NIL_RTCPUID, szDetectedCpuName, sizeof(szDetectedCpuName));
    if (RT_SUCCESS(rc))
        vbCpuRepDebug("szDetectedCpuName: %s\n", szDetectedCpuName);
    if (RT_FAILURE(rc) || strcmp(szDetectedCpuName, "Unknown") == 0)
        szDetectedCpuName[0] = '\0';

    const char *pszCpuName = g_pszCpuNameOverride ? g_pszCpuNameOverride : RTStrStrip(szDetectedCpuName);
    if (strlen(pszCpuName) >= sizeof(szDetectedCpuName))
        return RTMsgErrorRc(VERR_FILENAME_TOO_LONG, "CPU name is too long: %zu chars, max %zu: %s",
                            strlen(pszCpuName), sizeof(szDetectedCpuName) - 1, pszCpuName);

    /*
     * Get the system registers first so we can try identify the CPU.
     */
    rc = populateSystemRegisters();
    if (RT_FAILURE(rc))
        return rc;

    /** @todo update the remainder of the file to the variation scheme...   */

    /*
     * Now that we've got the ID register values, figure out the vendor,
     * microarch, cpu name and description..
     */
    uint64_t const uMIdReg      = getSysRegVal(ARMV8_AARCH64_SYSREG_MIDR_EL1);

    uint8_t const  bImplementer = (uint8_t )((uMIdReg >> 24) & 0xff);
    uint8_t  const bVariant     = (uint8_t )((uMIdReg >> 20) & 0xf);
    uint16_t const uPartNum     = (uint16_t)((uMIdReg >>  4) & 0xfff);
    uint8_t const  bRevision    = (uint8_t )( uMIdReg        & 0x7);
    uint16_t const uPartNumEx   = uPartNum | ((uint16_t)bVariant << 12);

    /** @todo move this to CPUM or IPRT...   */
    CPUMCPUVENDOR      enmVendor;
    const char        *pszVendor;
    PARTNUMINFO const *paPartNums;
    size_t             cPartNums;
    uint32_t           uPartNumSearch = uPartNum;
    switch (bImplementer)
    {
        case 0x41:
            enmVendor  = CPUMCPUVENDOR_ARM;
            pszVendor  = "ARM";
            paPartNums = g_aPartNumDbArm;
            cPartNums  = RT_ELEMENTS(g_aPartNumDbArm);
            break;

        case 0x42:
            enmVendor  = CPUMCPUVENDOR_BROADCOM;
            pszVendor  = "Broadcom";
            paPartNums = g_aPartNumDbBroadcom;
            cPartNums  = RT_ELEMENTS(g_aPartNumDbBroadcom);
            break;

        case 0x51:
            enmVendor  = CPUMCPUVENDOR_QUALCOMM;
            pszVendor  = "Qualcomm";
            paPartNums = g_aPartNumDbQualcomm;
            cPartNums  = RT_ELEMENTS(g_aPartNumDbQualcomm);
            uPartNumSearch = uPartNumEx; /* include the variant in the search */
            break;

        case 0x61:
            enmVendor  = CPUMCPUVENDOR_APPLE;
            pszVendor  = "Apple";
            paPartNums = g_aPartNumDbApple;
            cPartNums  = RT_ELEMENTS(g_aPartNumDbApple);
            break;

        case 0xc0:
            enmVendor  = CPUMCPUVENDOR_AMPERE;
            pszVendor  = "Ampere";
            paPartNums = g_aPartNumDbAmpere;
            cPartNums  = RT_ELEMENTS(g_aPartNumDbAmpere);
            break;

        default:
            return RTMsgErrorRc(VERR_UNSUPPORTED_CPU, "Unknown ARM implementer: %#x (%s)", bImplementer, pszCpuName);
    }

    /* Look up the part number in the vendor table: */
    const char    *pszCpuDesc   = NULL;
    CPUMMICROARCH  enmMicroarch = kCpumMicroarch_Invalid;
    for (size_t i = 0; i < cPartNums; i++)
        if (paPartNums[i].uPartNum == uPartNumSearch)
        {
            enmMicroarch = paPartNums[i].enmMicroarch;
            pszCpuDesc   = paPartNums[i].pszFullName;
            if (!g_pszCpuNameOverride)
                pszCpuName = paPartNums[i].pszName;
            break;
        }
    if (enmMicroarch == kCpumMicroarch_Invalid)
        return RTMsgErrorRc(VERR_UNSUPPORTED_CPU, "%s part number not found: %#x (MIDR_EL1=%#x%s%s)",
                            pszVendor, uPartNum, uMIdReg, *pszCpuName ? " " : "", pszCpuName);

    /*
     * Sanitize the name.
     */
    char   szName[sizeof(szDetectedCpuName)];
    size_t offSrc = 0;
    size_t offDst = 0;
    for (;;)
    {
        char ch = pszCpuName[offSrc++];
        if (!RT_C_IS_SPACE(ch))
            szName[offDst++] = ch;
        else
        {
            while (RT_C_IS_SPACE((ch = pszCpuName[offSrc])))
                offSrc++;
            if (offDst > 0 && ch != '\0')
                szName[offDst++] = ' ';
        }
        if (!ch)
            break;
    }
    RTStrPurgeEncoding(szName);
    pszCpuName = szName;
    vbCpuRepDebug("Name: %s\n", pszCpuName);

    /*
     * Make it C/C++ acceptable.
     */
    static const char s_szNamePrefix[] = "ARM_";
    char szNameC[sizeof(s_szNamePrefix) + sizeof(szDetectedCpuName)];
    strcpy(szNameC, s_szNamePrefix);
    /** @todo Move to common function... */
    offDst = sizeof(s_szNamePrefix) - 1;
    offSrc = 0;
    for (;;)
    {
        char ch = pszCpuName[offSrc++];
        if (!RT_C_IS_ALNUM(ch) && ch != '_' && ch != '\0')
            ch = '_';
        if (ch == '_' && offDst > 0 && szNameC[offDst - 1] == '_')
            offDst--;
        szNameC[offDst++] = ch;
        if (!ch)
            break;
    }
    while (offDst > 1 && szNameC[offDst - 1] == '_')
        szNameC[--offDst] = '\0';

    vbCpuRepDebug("NameC: %s\n", szNameC);

    /*
     * Print a file header, if we're not outputting to stdout (assumption being
     * that stdout is used while hacking the reporter and too much output is
     * unwanted).
     */
    if (g_pReportOut)
        vbCpuRepFileHdr(pszCpuName, szNameC);

    /*
     * Produce the array of system (id) register values.
     */
    rc = produceSysRegArray(szNameC, pszCpuDesc);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Emit the database entry.
     */
    vbCpuRepPrintf("\n"
                   "/**\n"
                   " * Database entry for %s.\n"
                   " */\n"
                   "static CPUMDBENTRYARM const g_Entry_%s = \n"
                   "{\n"
                   "    {\n"
                   "        /*.pszName      = */ \"%s\",\n"
                   "        /*.pszFullName  = */ \"%s\",\n"
                   "        /*.enmVendor    = */ CPUMCPUVENDOR_%s,\n"
                   "        /*.enmMicroarch = */ kCpumMicroarch_%s,\n"
                   "        /*.fFlags       = */ 0,\n"
                   "    },\n"
                   "    /*.bImplementer     = */ %#04x,\n"
                   "    /*.bRevision        = */ %#04x,\n"
                   "    /*.uPartNum         = */ %#04x,\n"
                   "    /*.cSysRegVals      = */ ZERO_ALONE(RT_ELEMENTS(g_aSysRegVals_%s)),\n"
                   "    /*.paSysRegVals     = */ NULL_ALONE(g_aSysRegVals_%s),\n"
                   "};\n"
                   "\n"
                   "#endif /* !VBOX_CPUDB_%s_h */\n"
                   "\n",
                   pszCpuDesc,
                   szNameC,
                   pszCpuName,
                   pszCpuDesc,
                   CPUMCpuVendorName(enmVendor),
                   CPUMMicroarchName(enmMicroarch),
                   bImplementer,
                   bRevision,
                   uPartNum,
                   szNameC,
                   szNameC,
                   szNameC);

    return VINF_SUCCESS;
}

