/* $Id$ */
/** @file
 * VBoxAcpi - VirtualBox ACPI manipulation functionality.
 */

/*
 * Copyright (C) 2009-2024 Oracle and/or its affiliates.
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
#include <iprt/cdefs.h>
#if !defined(IN_RING3)
# error Pure R3 code
#endif

#define LOG_GROUP LOG_GROUP_DEV_ACPI
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pgm.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/vmm/mm.h>
#include <iprt/acpi.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/buildconfig.h>
#include <iprt/string.h>
#include <iprt/file.h>

/* Statically compiled AML */
#include <vboxaml.hex>
#ifdef VBOX_WITH_TPM
# include <vboxssdt_tpm.hex>
#endif

#include "VBoxDD.h"

/** The CPU suffixes being used for processor object names. */
static const char g_achCpuSuff[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";

/**
 * Creates the SSDT exposing configured CPUs as processor objects.
 *
 * @returns VBox status code.
 * @param   pDevIns             The PDM device instance data.
 * @param   ppabAml             Where to store the pointer to the buffer containing the ACPI table on success.
 * @param   pcbAml              Where to store the size of the ACPI table in bytes on success.
 *
 * @note This replaces the old vbox-standard.dsl which was patched accordingly.
 */
static int acpiCreateCpuSsdt(PPDMDEVINS pDevIns, uint8_t **ppabAml, size_t *pcbAml)
{
    PCPDMDEVHLPR3 pHlp = pDevIns->pHlpR3;

    uint16_t cCpus;
    int rc = pHlp->pfnCFGMQueryU16Def(pDevIns->pCfg, "NumCPUs", &cCpus, 1);
    if (RT_FAILURE(rc))
        return rc;

    bool fShowCpu;
    rc = pHlp->pfnCFGMQueryBoolDef(pDevIns->pCfg, "ShowCpu", &fShowCpu, false);
    if (RT_FAILURE(rc))
        return rc;

    /* Don't expose any CPU object if we are not required to. */
    if (!fShowCpu)
        cCpus = 0;

    RTACPITBL hAcpiTbl;
    rc = RTAcpiTblCreate(&hAcpiTbl, ACPI_TABLE_HDR_SIGNATURE_SSDT, 1, "VBOX  ", "VBOXCPUT", 2, "VBOX", RTBldCfgRevision());
    if (RT_SUCCESS(rc))
    {
        RTAcpiTblScopeStart(hAcpiTbl, "\\_PR");
        for (uint16_t i = 0; i < cCpus; i++)
        {
            uint8_t const cCpuSuff = RT_ELEMENTS(g_achCpuSuff);
            RTAcpiTblProcessorStartF(hAcpiTbl, i /*bProcId*/, 0 /*u32PBlkAddr*/, 0 /*cbPBlk*/, "CP%c%c",
                                     i < cCpuSuff ? 'U' : 'V',
                                     g_achCpuSuff[i % cCpuSuff]);
            rc = RTAcpiTblProcessorFinalize(hAcpiTbl);
            if (RT_FAILURE(rc))
                break;
        }
        RTAcpiTblScopeFinalize(hAcpiTbl);

        rc = RTAcpiTblFinalize(hAcpiTbl);
        if (RT_SUCCESS(rc))
        {
            rc = RTAcpiTblDumpToBufferA(hAcpiTbl, RTACPITBLTYPE_AML, ppabAml, pcbAml);
            if (RT_FAILURE(rc))
                rc = PDMDEV_SET_ERROR(pDevIns, rc, N_("ACPI error: Failed to dump CPU SSDT"));
        }
        else
            rc = PDMDEV_SET_ERROR(pDevIns, rc, N_("ACPI error: Failed to finalize CPU SSDT"));

        RTAcpiTblDestroy(hAcpiTbl);
    }
    else
        rc = PDMDEV_SET_ERROR(pDevIns, rc, N_("ACPI error: Failed to create CPU SSDT"));

    return rc;
}


/**
 * Creates the SSDT exposing configured CPUs as processor objects - hotplug variant.
 *
 * @returns VBox status code.
 * @param   pDevIns             The PDM device instance data.
 * @param   ppabAml             Where to store the pointer to the buffer containing the ACPI table on success.
 * @param   pcbAml              Where to store the size of the ACPI table in bytes on success.
 *
 * @note This replaces the old vbox-cpuhotplug.dsl which was patched accordingly.
 */
static int acpiCreateCpuHotplugSsdt(PPDMDEVINS pDevIns, uint8_t **ppabAml, size_t *pcbAml)
{
    PCPDMDEVHLPR3 pHlp = pDevIns->pHlpR3;

    uint16_t cCpus;
    int rc = pHlp->pfnCFGMQueryU16Def(pDevIns->pCfg, "NumCPUs", &cCpus, 1);
    if (RT_FAILURE(rc))
        return rc;

    RTACPITBL hAcpiTbl;
    rc = RTAcpiTblCreate(&hAcpiTbl, ACPI_TABLE_HDR_SIGNATURE_SSDT, 1, "VBOX  ", "VBOXCPUT", 2, "VBOX", RTBldCfgRevision());
    if (RT_SUCCESS(rc))
    {
        uint8_t const cCpuSuff = RT_ELEMENTS(g_achCpuSuff);

        /* Declare externals */
        RTAcpiTblIfStart(hAcpiTbl);
            RTAcpiTblIntegerAppend(hAcpiTbl, 0);

            RTAcpiTblExternalAppend(hAcpiTbl, "CPUC", kAcpiObjType_Unknown, 0 /*cArgs*/);
            RTAcpiTblExternalAppend(hAcpiTbl, "CPUL", kAcpiObjType_Unknown, 0 /*cArgs*/);
            RTAcpiTblExternalAppend(hAcpiTbl, "CPEV", kAcpiObjType_Unknown, 0 /*cArgs*/);
            RTAcpiTblExternalAppend(hAcpiTbl, "CPET", kAcpiObjType_Unknown, 0 /*cArgs*/);

        RTAcpiTblIfFinalize(hAcpiTbl);

        /* Define two helper methods. */

        /* CPCK(Arg0) -> Boolean - Checks whether the CPU identified by the given indes is locked. */
        RTAcpiTblMethodStart(hAcpiTbl, "CPCK", 1 /*cArgs*/, RTACPI_METHOD_F_NOT_SERIALIZED, 0 /*uSyncLvl*/);
            RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Store);
                RTAcpiTblArgOpAppend(hAcpiTbl, 0);
                RTAcpiTblNameStringAppend(hAcpiTbl, "CPUC");

            RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Return);
                    RTAcpiTblBinaryOpAppend(hAcpiTbl, kAcpiBinaryOp_LEqual);
                        RTAcpiTblNameStringAppend(hAcpiTbl, "CPUL");
                        RTAcpiTblIntegerAppend(hAcpiTbl, 1);
        RTAcpiTblMethodFinalize(hAcpiTbl);

        /* CPLO(Arg0) -> Nothing - Unlocks the CPU identified by the given index. */
        RTAcpiTblMethodStart(hAcpiTbl, "CPLO", 1 /*cArgs*/, RTACPI_METHOD_F_NOT_SERIALIZED, 0 /*uSyncLvl*/);
            RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Store);
                RTAcpiTblArgOpAppend(hAcpiTbl, 0);
                RTAcpiTblNameStringAppend(hAcpiTbl, "CPUL");
        RTAcpiTblMethodFinalize(hAcpiTbl);

        /* Define all configured CPUs. */
        RTAcpiTblScopeStart(hAcpiTbl, "\\_SB");
        for (uint16_t i = 0; i < cCpus; i++)
        {
            RTAcpiTblDeviceStartF(hAcpiTbl, "SC%c%c", i < cCpuSuff ? 'K' : 'L', g_achCpuSuff[i % cCpuSuff]);

                RTAcpiTblNameAppend(hAcpiTbl, "_HID");
                    RTAcpiTblStringAppend(hAcpiTbl, "ACPI0004");
                RTAcpiTblNameAppend(hAcpiTbl, "_UID");
                    RTAcpiTblStringAppendF(hAcpiTbl, "SCKCP%c%c", i < cCpuSuff ? 'U' : 'V', g_achCpuSuff[i % cCpuSuff]);

                RTAcpiTblProcessorStartF(hAcpiTbl, i /*bProcId*/, 0 /*u32PBlkAddr*/, 0 /*cbPBlk*/, "CP%c%c",
                                         i < cCpuSuff ? 'U' : 'V',
                                         g_achCpuSuff[i % cCpuSuff]);

                    RTAcpiTblNameAppend(hAcpiTbl, "_HID");
                        RTAcpiTblStringAppend(hAcpiTbl, "ACPI0007");
                    RTAcpiTblNameAppend(hAcpiTbl, "_UID");
                        RTAcpiTblIntegerAppend(hAcpiTbl, i);
                    RTAcpiTblNameAppend(hAcpiTbl, "_PXM");
                        RTAcpiTblIntegerAppend(hAcpiTbl, 0);

                    uint8_t abBufApic[8] = { 0x00, 0x08, (uint8_t)i, (uint8_t)i, 0, 0, 0, 0};
                    RTAcpiTblNameAppend(hAcpiTbl, "APIC");
                        RTAcpiTblBufferAppend(hAcpiTbl, &abBufApic[0], sizeof(abBufApic));

                    /* _MAT Method. */
                    RTAcpiTblMethodStart(hAcpiTbl, "_MAT", 0 /*cArgs*/, RTACPI_METHOD_F_SERIALIZED, 0 /*uSyncLvl*/);
                        RTAcpiTblIfStart(hAcpiTbl);
                            RTAcpiTblNameStringAppend(hAcpiTbl, "CPCK");
                                RTAcpiTblIntegerAppend(hAcpiTbl, i);

                            RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Store);
                                RTAcpiTblIntegerAppend(hAcpiTbl, 1);
                                RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Index);
                                    RTAcpiTblNameStringAppend(hAcpiTbl, "APIC");
                                    RTAcpiTblIntegerAppend(hAcpiTbl, 4);
                                    RTAcpiTblNullNameAppend(hAcpiTbl);
 
                        RTAcpiTblIfFinalize(hAcpiTbl);
                        RTAcpiTblElseStart(hAcpiTbl);
                        RTAcpiTblElseFinalize(hAcpiTbl);

                        RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Return);
                            RTAcpiTblNameStringAppend(hAcpiTbl, "APIC");
                    RTAcpiTblMethodFinalize(hAcpiTbl);

                    /* _STA Method. */
                    RTAcpiTblMethodStart(hAcpiTbl, "_STA", 0 /*cArgs*/, RTACPI_METHOD_F_NOT_SERIALIZED, 0 /*uSyncLvl*/);
                        RTAcpiTblIfStart(hAcpiTbl);
                            RTAcpiTblNameStringAppend(hAcpiTbl, "CPCK");
                                RTAcpiTblIntegerAppend(hAcpiTbl, i);

                            RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Return);
                                RTAcpiTblIntegerAppend(hAcpiTbl, 0xf);
                        RTAcpiTblIfFinalize(hAcpiTbl);
                        RTAcpiTblElseStart(hAcpiTbl);
                            RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Return);
                                RTAcpiTblIntegerAppend(hAcpiTbl, 0x0);
                        RTAcpiTblElseFinalize(hAcpiTbl);
                    RTAcpiTblMethodFinalize(hAcpiTbl);

                    /* _EJ0 Method. */
                    RTAcpiTblMethodStart(hAcpiTbl, "_EJ0", 1 /*cArgs*/, RTACPI_METHOD_F_NOT_SERIALIZED, 0 /*uSyncLvl*/);
                        RTAcpiTblNameStringAppend(hAcpiTbl, "CPLO");
                            RTAcpiTblIntegerAppend(hAcpiTbl, i);
                        RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Return);
                    RTAcpiTblMethodFinalize(hAcpiTbl);

                RTAcpiTblProcessorFinalize(hAcpiTbl);

            rc = RTAcpiTblDeviceFinalize(hAcpiTbl);
            if (RT_FAILURE(rc))
                break;
        }

        if (RT_SUCCESS(rc))
        {
            RTAcpiTblScopeFinalize(hAcpiTbl);

            /* Now the _GPE scope where event processing takes place. */
            RTAcpiTblScopeStart(hAcpiTbl, "\\_GPE");
                RTAcpiTblMethodStart(hAcpiTbl, "_L01", 0 /*cArgs*/, RTACPI_METHOD_F_NOT_SERIALIZED, 0 /*uSyncLvl*/);

                    RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Store);
                        RTAcpiTblNameStringAppend(hAcpiTbl, "CPEV");
                        RTAcpiTblLocalOpAppend(hAcpiTbl, 0);

                    RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Store);
                        RTAcpiTblNameStringAppend(hAcpiTbl, "CPET");
                        RTAcpiTblLocalOpAppend(hAcpiTbl, 1);

                    for (uint16_t i = 0; i < cCpus; i++)
                    {
                        RTAcpiTblIfStart(hAcpiTbl);
                            RTAcpiTblBinaryOpAppend(hAcpiTbl, kAcpiBinaryOp_LEqual);
                                RTAcpiTblLocalOpAppend(hAcpiTbl, 0);
                                RTAcpiTblIntegerAppend(hAcpiTbl, i);

                            RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Notify);
                                RTAcpiTblNameStringAppendF(hAcpiTbl, "\\_SB.SC%c%c.CP%c%c",
                                                           i < cCpuSuff ? 'K' : 'L',
                                                           g_achCpuSuff[i % cCpuSuff],
                                                           i < cCpuSuff ? 'U' : 'V',
                                                           g_achCpuSuff[i % cCpuSuff]);
                                RTAcpiTblLocalOpAppend(hAcpiTbl, 1);
                        rc = RTAcpiTblIfFinalize(hAcpiTbl);
                        if (RT_FAILURE(rc))
                            break;
                    }

                RTAcpiTblMethodFinalize(hAcpiTbl);
            RTAcpiTblScopeFinalize(hAcpiTbl);
        }

        rc = RTAcpiTblFinalize(hAcpiTbl);
        if (RT_SUCCESS(rc))
        {
            rc = RTAcpiTblDumpToBufferA(hAcpiTbl, RTACPITBLTYPE_AML, ppabAml, pcbAml);
            if (RT_FAILURE(rc))
                rc = PDMDEV_SET_ERROR(pDevIns, rc, N_("ACPI error: Failed to dump CPU SSDT"));
        }
        else
            rc = PDMDEV_SET_ERROR(pDevIns, rc, N_("ACPI error: Failed to finalize CPU SSDT"));

        RTAcpiTblDestroy(hAcpiTbl);
    }
    else
        rc = PDMDEV_SET_ERROR(pDevIns, rc, N_("ACPI error: Failed to create CPU SSDT"));

    return rc;
}


/**
 * Loads an AML file if present in CFGM
 *
 * @returns VBox status code
 * @param   pDevIns        The device instance
 * @param   pcszCfgName    The configuration key holding the file path
 * @param   pcszSignature  The signature to check for
 * @param   ppabAmlCode     Where to store the pointer to the AML code on success.
 * @param   pcbAmlCode     Where to store the number of bytes of the AML code on success.
 */
static int acpiAmlLoadExternal(PPDMDEVINS pDevIns, const char *pcszCfgName, const char *pcszSignature,
                               uint8_t **ppabAmlCode, size_t *pcbAmlCode)
{
    PCPDMDEVHLPR3 pHlp = pDevIns->pHlpR3;

    char *pszAmlFilePath = NULL;
    int rc = pHlp->pfnCFGMQueryStringAlloc(pDevIns->pCfg, pcszCfgName, &pszAmlFilePath);
    if (RT_SUCCESS(rc))
    {
        /* Load from file. */
        RTFILE hFileAml = NIL_RTFILE;
        rc = RTFileOpen(&hFileAml, pszAmlFilePath, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc))
        {
            /*
             * An AML file contains the raw DSDT or SSDT thus the size of the file
             * is equal to the size of the DSDT or SSDT.
             */
            uint64_t cbAmlFile = 0;
            rc = RTFileQuerySize(hFileAml, &cbAmlFile);

            /* Don't use AML files over 32MiB. */
            if (   RT_SUCCESS(rc)
                && cbAmlFile <= _32M)
            {
                size_t const cbAmlCode = (size_t)cbAmlFile;
                uint8_t *pabAmlCode = (uint8_t *)RTMemAllocZ(cbAmlCode);
                if (pabAmlCode)
                {
                    rc = RTFileReadAt(hFileAml, 0, pabAmlCode, cbAmlCode, NULL);

                    /*
                     * We fail if reading failed or the identifier at the
                     * beginning is wrong.
                     */
                    if (   RT_FAILURE(rc)
                        || strncmp((const char *)pabAmlCode, pcszSignature, 4))
                    {
                        RTMemFree(pabAmlCode);
                        pabAmlCode = NULL;

                        /* Return error if file header check failed */
                        if (RT_SUCCESS(rc))
                            rc = VERR_PARSE_ERROR;
                    }
                    else
                    {
                        *ppabAmlCode = pabAmlCode;
                        *pcbAmlCode = cbAmlCode;
                        rc = VINF_SUCCESS;
                    }
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else if (RT_SUCCESS(rc))
                rc = VERR_OUT_OF_RANGE;

            RTFileClose(hFileAml);
        }
        PDMDevHlpMMHeapFree(pDevIns, pszAmlFilePath);
    }

    return rc;
}


/** No docs, lazy coder. */
int acpiPrepareDsdt(PPDMDEVINS pDevIns,  void **ppvPtr, size_t *pcbDsdt)
{
    uint8_t *pabAmlCodeDsdt = NULL;
    size_t cbAmlCodeDsdt = 0;
    int rc = acpiAmlLoadExternal(pDevIns, "DsdtFilePath", "DSDT", &pabAmlCodeDsdt, &cbAmlCodeDsdt);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        /* Use the compiled in AML code */
        cbAmlCodeDsdt = sizeof(AmlCode);
        pabAmlCodeDsdt = (uint8_t *)RTMemDup(AmlCode, cbAmlCodeDsdt);
        if (pabAmlCodeDsdt)
            rc = VINF_SUCCESS;
        else
            rc = VERR_NO_MEMORY;
    }
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to read \"DsdtFilePath\""));

    if (RT_SUCCESS(rc))
    {
        *ppvPtr = pabAmlCodeDsdt;
        *pcbDsdt = cbAmlCodeDsdt;
    }
    return rc;
}

/** No docs, lazy coder. */
int acpiCleanupDsdt(PPDMDEVINS pDevIns, void *pvPtr)
{
    RT_NOREF1(pDevIns);
    if (pvPtr)
        RTMemFree(pvPtr);
    return VINF_SUCCESS;
}

/** No docs, lazy coder. */
int acpiPrepareSsdt(PPDMDEVINS pDevIns, void **ppvPtr, size_t *pcbSsdt)
{
    PCPDMDEVHLPR3 pHlp = pDevIns->pHlpR3;

    uint8_t *pabAmlCodeSsdt = NULL;
    size_t   cbAmlCodeSsdt = 0;
    int rc = acpiAmlLoadExternal(pDevIns, "SsdtFilePath", "SSDT", &pabAmlCodeSsdt, &cbAmlCodeSsdt);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        bool fCpuHotPlug = false;
        rc = pHlp->pfnCFGMQueryBoolDef(pDevIns->pCfg, "CpuHotPlug", &fCpuHotPlug, false);
        if (RT_SUCCESS(rc))
        {
            if (fCpuHotPlug)
                rc = acpiCreateCpuHotplugSsdt(pDevIns, &pabAmlCodeSsdt, &cbAmlCodeSsdt);
            else
                rc = acpiCreateCpuSsdt(pDevIns, &pabAmlCodeSsdt, &cbAmlCodeSsdt);
        }
    }
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to read \"SsdtFilePath\""));

    if (RT_SUCCESS(rc))
    {
        *ppvPtr = pabAmlCodeSsdt;
        *pcbSsdt = cbAmlCodeSsdt;
    }
    return rc;
}

/** No docs, lazy coder. */
int acpiCleanupSsdt(PPDMDEVINS pDevIns, void *pvPtr)
{
    RT_NOREF1(pDevIns);
    if (pvPtr)
        RTMemFree(pvPtr);
    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_TPM
/** No docs, lazy coder. */
int acpiPrepareTpmSsdt(PPDMDEVINS pDevIns, void **ppvPtr, size_t *pcbSsdt)
{
    uint8_t *pabAmlCodeSsdt = NULL;
    size_t   cbAmlCodeSsdt = 0;
    int rc = acpiAmlLoadExternal(pDevIns, "SsdtTpmFilePath", "SSDT", &pabAmlCodeSsdt, &cbAmlCodeSsdt);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        rc = VINF_SUCCESS;
        cbAmlCodeSsdt  = sizeof(AmlCodeSsdtTpm);
        pabAmlCodeSsdt = (uint8_t *)RTMemDup(AmlCodeSsdtTpm, sizeof(AmlCodeSsdtTpm));
        if (!pabAmlCodeSsdt)
            rc = VERR_NO_MEMORY;
    }
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to read \"SsdtFilePath\""));

    if (RT_SUCCESS(rc))
    {
        *ppvPtr = pabAmlCodeSsdt;
        *pcbSsdt = cbAmlCodeSsdt;
    }
    return rc;
}

/** No docs, lazy coder. */
int acpiCleanupTpmSsdt(PPDMDEVINS pDevIns, void *pvPtr)
{
    RT_NOREF1(pDevIns);
    if (pvPtr)
        RTMemFree(pvPtr);
    return VINF_SUCCESS;
}
#endif

