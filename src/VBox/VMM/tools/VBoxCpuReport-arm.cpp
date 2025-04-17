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
} PARTNUMINFO;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static uint32_t        g_cSysRegVals = 0;
static SUPARMSYSREGVAL g_aSysRegVals[4096];

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
    { 0xfff,    kCpumMicroarch_Unknown,             "TODO",                 "TODO" },
};

/** Apple CPU info by part number. */
static PARTNUMINFO const g_aPartNumDbApple[] =
{
    { 0x022,    kCpumMicroarch_Apple_M1,            "Apple M1",             "Apple M1 (Icestorm)" },
    { 0x023,    kCpumMicroarch_Apple_M1,            "Apple M1",             "Apple M1 (Firestorm)" },
    { 0x024,    kCpumMicroarch_Apple_M1,            "Apple M1 Pro",         "Apple M1 Pro (Icestorm)" },
    { 0x025,    kCpumMicroarch_Apple_M1,            "Apple M1 Pro",         "Apple M1 Pro (Firestorm)" },
    { 0x028,    kCpumMicroarch_Apple_M1,            "Apple M1 Max",         "Apple M1 Max (Icestorm)" },
    { 0x029,    kCpumMicroarch_Apple_M1,            "Apple M1 Max",         "Apple M1 Max (Firestorm)" },
    { 0x032,    kCpumMicroarch_Apple_M2,            "Apple M2",             "Apple M2 (Blizzard)" },
    { 0x033,    kCpumMicroarch_Apple_M2,            "Apple M2",             "Apple M2 (Avalanche)" },
    { 0x034,    kCpumMicroarch_Apple_M2,            "Apple M2 Pro",         "Apple M2 Pro (Blizzard)" },
    { 0x035,    kCpumMicroarch_Apple_M2,            "Apple M2 Pro",         "Apple M2 Pro (Avalanche)" },
    { 0x038,    kCpumMicroarch_Apple_M2,            "Apple M2 Max",         "Apple M2 Max (Blizzard)" },
    { 0x039,    kCpumMicroarch_Apple_M2,            "Apple M2 Max",         "Apple M2 Max (Avalanche)" },
};

/** Ampere CPU info by part number. */
static PARTNUMINFO const g_aPartNumDbAmpere[] =
{
    { 0xfff,    kCpumMicroarch_Unknown,             "TODO",                 "TODO" },
};


/** Looks up a register value in g_aSysRegVals. */
static uint64_t getSysRegVal(uint32_t idReg, uint64_t uNotFoundValue = 0)
{
    for (uint32_t i = 0; i < g_cSysRegVals; i++)
        if (g_aSysRegVals[i].idReg == idReg)
            return g_aSysRegVals[i].uValue;
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
        uint32_t cRegAvailable = 0;
        rc = SUPR3ArmQuerySysRegs(SUP_ARM_SYS_REG_F_INC_ZERO_REG_VAL | SUP_ARM_SYS_REG_F_EXTENDED,
                                  RT_ELEMENTS(g_aSysRegVals), &g_cSysRegVals, &cRegAvailable, g_aSysRegVals);
        vbCpuRepDebug("SUPR3ArmQuerySysRegs -> %Rrc (%u/%u regs)\n", rc, g_cSysRegVals, cRegAvailable);
        if (RT_FAILURE(rc))
            return RTMsgErrorRc(rc, "SUPR3ArmQuerySysRegs failed: %Rrc", rc);
        if (cRegAvailable > g_cSysRegVals)
            return RTMsgErrorRc(rc,
                                "SUPR3ArmQuerySysRegs claims there are %u more registers availble.\n"
                                "Increase size of g_aSysRegVals to at least %u entries and retry!",
                                cRegAvailable - g_cSysRegVals, cRegAvailable);
        RTSortShell(g_aSysRegVals, g_cSysRegVals, sizeof(g_aSysRegVals[0]), sysRegValSortCmp, NULL);
        return rc;
    }
    return RTMsgErrorRc(rc, "Unable to initialize the support library (%Rrc).", rc);
    //vbCpuRepDebug("warning: Unable to initialize the support library (%Rrc).\n", rc);
    /** @todo On Linux we can query the registers exposed to ring-3... */
}


/**
 * Populate the system register array and output it.
 */
static int produceSysRegArray(const char *pszNameC, const char *pszCpuDesc)
{
    RT_NOREF(pszNameC, pszCpuDesc);

    /* Output the array. */
    vbCpuRepPrintf("\n"
                   "/*\n"
                   " * System Register Values for %s.\n"
                   " */\n"
                   "static SUPARMSYSREGVAL const g_aSysRegVals_%s[] =\n"
                   "{\n",
                   pszCpuDesc, pszNameC);
    for (uint32_t i = 0; i < g_cSysRegVals; i++)
    {
        uint32_t const     idReg = g_aSysRegVals[i].idReg;
        uint32_t const     uOp0  = ARMV8_AARCH64_SYSREG_OP0_GET(idReg);
        uint32_t const     uOp1  = ARMV8_AARCH64_SYSREG_OP1_GET(idReg);
        uint32_t const     uCRn  = ARMV8_AARCH64_SYSREG_CRN_GET(idReg);
        uint32_t const     uCRm  = ARMV8_AARCH64_SYSREG_CRM_GET(idReg);
        uint32_t const     uOp2  = ARMV8_AARCH64_SYSREG_OP2_GET(idReg);
        const char * const pszNm = sysRegNoToName(idReg);

        vbCpuRepPrintf("    { UINT64_C(%#018RX64), ARMV8_AARCH64_SYSREG_ID_CREATE(%u, %u,%2u,%2u, %u), %#x },%s%s%s\n",
                       g_aSysRegVals[i].uValue, uOp0, uOp1, uCRn, uCRm, uOp2, g_aSysRegVals[i].fFlags,
                       pszNm ? " /* " : "", pszNm, pszNm ? " */" : "");
    }
    vbCpuRepPrintf("};\n"
                   "\n");
    return VINF_SUCCESS;
}


int produceCpuReport(void)
{
    /*
     * Figure out the processor name via the host OS and command line first...
     */
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

    /*
     * Now that we've got the ID register values, figure out the vendor,
     * microarch, cpu name and description..
     */
    uint64_t const uMIdReg      = getSysRegVal(ARMV8_AARCH64_SYSREG_MIDR_EL1);

    uint8_t const  bImplementer = (uint8_t)(uMIdReg >> 24);
    uint8_t const  bRevision    = (uint8_t)(uMIdReg & 0x7);
    uint16_t const uPartNum     = (uint16_t)((uMIdReg >> 4) & 0xfff);

    /** @todo move this to CPUM or IPRT...   */
    CPUMCPUVENDOR       enmVendor;
    const char         *pszVendor;
    PARTNUMINFO const  *paPartNums;
    size_t              cPartNums;
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
    const char   *pszCpuDesc   = NULL;
    CPUMMICROARCH enmMicroarch = kCpumMicroarch_Invalid;
    for (size_t i = 0; i < cPartNums; i++)
        if (paPartNums[i].uPartNum == uPartNum)
        {
            enmMicroarch = paPartNums[i].enmMicroarch;
            pszCpuDesc   = paPartNums[i].pszFullName;
            if (!g_pszCpuNameOverride)
                pszCpuName = paPartNums[i].pszName;
            break;
        }
    if (enmMicroarch == kCpumMicroarch_Invalid)
        return RTMsgErrorRc(VERR_UNSUPPORTED_CPU, "%s part number not found: %#x (%s)", pszVendor, uPartNum, pszCpuName);

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

