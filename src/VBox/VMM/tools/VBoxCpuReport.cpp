/* $Id$ */
/** @file
 * VBoxCpuReport - Produces the basis for a CPU DB entry.
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
#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/errcore.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/symlink.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include <VBox/vmm/cpum.h>
#include <VBox/sup.h>
#include <VBox/version.h>

#include "VBoxCpuReport.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The alternative report stream. */
PRTSTREAM        g_pReportOut;
/** The alternative debug stream. */
PRTSTREAM        g_pDebugOut;
/** The CPU vendor.  Used by the MSR code. */
CPUMCPUVENDOR    g_enmVendor = CPUMCPUVENDOR_INVALID;
/** The CPU microarchitecture.  Used by the MSR code. */
CPUMMICROARCH    g_enmMicroarch = kCpumMicroarch_Invalid;
/** Overrides the detected CPU name.
 * This is main for non-x86 hosts where the processor name string isn't
 * part of the silicone. */
const char      *g_pszCpuNameOverride = NULL;



void vbCpuRepDebug(const char *pszMsg, ...)
{
    va_list va;

    /* Always print a copy of the report to standard error. */
    va_start(va, pszMsg);
    RTStrmPrintfV(g_pStdErr, pszMsg, va);
    va_end(va);
    RTStrmFlush(g_pStdErr);

    /* Alternatively, also print to a log file. */
    if (g_pDebugOut)
    {
        va_start(va, pszMsg);
        RTStrmPrintfV(g_pDebugOut, pszMsg, va);
        va_end(va);
        RTStrmFlush(g_pDebugOut);
    }

    /* Give the output device a chance to write / display it. */
    RTThreadSleep(1);
}


void vbCpuRepPrintf(const char *pszMsg, ...)
{
    va_list va;

    /* Output to report file, if requested. */
    if (g_pReportOut)
    {
        va_start(va, pszMsg);
        RTStrmPrintfV(g_pReportOut, pszMsg, va);
        va_end(va);
        RTStrmFlush(g_pReportOut);
    }

    /* Always print a copy of the report to standard out. */
    va_start(va, pszMsg);
    RTStrmPrintfV(g_pStdOut, pszMsg, va);
    va_end(va);
    RTStrmFlush(g_pStdOut);
}


/** Prints the file header. */
void vbCpuRepFileHdr(const char *pszName, const char *pszNameC)
{
    RTTIMESPEC Now;
    char       szNow[64];
    RTTimeSpecToString(RTTimeNow(&Now), szNow, sizeof(szNow));
    char *pchDot = strchr(szNow, '.');
    if (pchDot)
        strcpy(pchDot, "Z");

    vbCpuRepPrintf("/* $" "Id" "$ */\n"
                   "/** @file\n"
                   " * CPU database entry \"%s\".\n"
                   " * Generated at %s by VBoxCpuReport v%sr%s on %s.%s.\n"
                   " */\n"
                   "\n"
                   "/*\n"
                   " * Copyright (C) 2013-" VBOX_C_YEAR " Oracle and/or its affiliates.\n"
                   " *\n"
                   " * This file is part of VirtualBox base platform packages, as\n"
                   " * available from https://www.virtualbox.org.\n"
                   " *\n"
                   " * This program is free software; you can redistribute it and/or\n"
                   " * modify it under the terms of the GNU General Public License\n"
                   " * as published by the Free Software Foundation, in version 3 of the\n"
                   " * License.\n"
                   " *\n"
                   " * This program is distributed in the hope that it will be useful, but\n"
                   " * WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                   " * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
                   " * General Public License for more details.\n"
                   " *\n"
                   " * You should have received a copy of the GNU General Public License\n"
                   " * along with this program; if not, see <https://www.gnu.org/licenses>.\n"
                   " *\n"
                   " * SPDX-License-Identifier: GPL-3.0-only\n"
                   " */\n"
                   "\n"
                   "#ifndef VBOX_CPUDB_%s_h\n"
                   "#define VBOX_CPUDB_%s_h\n"
                   "#ifndef RT_WITHOUT_PRAGMA_ONCE\n"
                   "# pragma once\n"
                   "#endif\n"
                   "\n",
                   pszName,
                   szNow, RTBldCfgVersion(), RTBldCfgRevisionStr(), RTBldCfgTarget(), RTBldCfgTargetArch(),
                   pszNameC, pszNameC);
}



const char *vbCpuVendorToString(CPUMCPUVENDOR enmCpuVendor)
{
    switch (enmCpuVendor)
    {
        case CPUMCPUVENDOR_INTEL:       return "Intel";
        case CPUMCPUVENDOR_AMD:         return "AMD";
        case CPUMCPUVENDOR_VIA:         return "VIA";
        case CPUMCPUVENDOR_CYRIX:       return "Cyrix";
        case CPUMCPUVENDOR_SHANGHAI:    return "Shanghai";
        case CPUMCPUVENDOR_HYGON:       return "Hygon";

        case CPUMCPUVENDOR_ARM:         return "ARM";
        case CPUMCPUVENDOR_BROADCOM:    return "Broadcom";
        case CPUMCPUVENDOR_QUALCOMM:    return "Qualecomm";
        case CPUMCPUVENDOR_APPLE:       return "Apple";
        case CPUMCPUVENDOR_AMPERE:      return "Ampere";

        case CPUMCPUVENDOR_INVALID:
        case CPUMCPUVENDOR_UNKNOWN:
        case CPUMCPUVENDOR_32BIT_HACK:
            break;
    }
    return "invalid-cpu-vendor";
}


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Argument parsing?
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--cpu-name",  'c', RTGETOPT_REQ_STRING },
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
        { "--msrs-only", 'm', RTGETOPT_REQ_NOTHING },
        { "--msrs-dev",  'd', RTGETOPT_REQ_NOTHING },
        { "--no-msrs",   'n', RTGETOPT_REQ_NOTHING },
#endif
        { "--output",    'o', RTGETOPT_REQ_STRING  },
        { "--log",       'l', RTGETOPT_REQ_STRING  },
    };
    RTGETOPTSTATE State;
    RTGetOptInit(&State, argc, argv, &s_aOptions[0], RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    enum
    {
        kCpuReportOp_Normal,
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
        kCpuReportOp_MsrsOnly,
        kCpuReportOp_MsrsHacking
#else
        kCpuReportOp_Dummy
#endif
    } enmOp = kCpuReportOp_Normal;
    g_pReportOut = NULL;
    g_pDebugOut  = NULL;
    const char *pszOutput   = NULL;
    const char *pszDebugOut = NULL;

    int iOpt;
    RTGETOPTUNION ValueUnion;
    while ((iOpt = RTGetOpt(&State, &ValueUnion)) != 0)
    {
        switch (iOpt)
        {
            case 'c':
                g_pszCpuNameOverride = *ValueUnion.psz ? ValueUnion.psz : NULL;
                break;

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
            case 'm':
                enmOp = kCpuReportOp_MsrsOnly;
                break;

            case 'd':
                enmOp = kCpuReportOp_MsrsHacking;
                break;

            case 'n':
                g_fNoMsrs = true;
                break;
#endif

            case 'o':
                pszOutput = ValueUnion.psz;
                break;

            case 'l':
                pszDebugOut = ValueUnion.psz;
                break;

            case 'h':
            {
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
                const char * const pszArchOps = "[-m|--msrs-only] [-d|--msrs-dev] [-n|--no-msrs] ";
#else
                const char * const pszArchOps = "";
#endif
                RTPrintf("Usage: VBoxCpuReport %s[-c|--cpu-name <name>] [-h|--help] [-V|--version] [-o filename.h] [-l debug.log]\n",
                         pszArchOps);
                RTPrintf("Internal tool for gathering information to the VMM CPU database.\n");
                return RTEXITCODE_SUCCESS;
            }
            case 'V':
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return RTEXITCODE_SUCCESS;
            default:
                return RTGetOptPrintError(iOpt, &ValueUnion);
        }
    }

    /*
     * Open the alternative debug log stream.
     */
    if (pszDebugOut)
    {
        if (RTFileExists(pszDebugOut) && !RTSymlinkExists(pszDebugOut))
        {
            char szOld[RTPATH_MAX];
            rc = RTStrCopy(szOld, sizeof(szOld), pszDebugOut);
            if (RT_SUCCESS(rc))
                rc = RTStrCat(szOld, sizeof(szOld), ".old");
            if (RT_SUCCESS(rc))
                RTFileRename(pszDebugOut, szOld, RTFILEMOVE_FLAGS_REPLACE);
        }
        rc = RTStrmOpen(pszDebugOut, "w", &g_pDebugOut);
        if (RT_FAILURE(rc))
        {
            RTMsgError("Error opening '%s': %Rrc", pszDebugOut, rc);
            g_pDebugOut = NULL;
        }
    }

    /*
     * Do the requested job.
     */
    rc = VERR_INTERNAL_ERROR;
    switch (enmOp)
    {
        case kCpuReportOp_Normal:
            /* switch output file. */
            if (pszOutput)
            {
                if (RTFileExists(pszOutput) && !RTSymlinkExists(pszOutput))
                {
                    char szOld[RTPATH_MAX];
                    rc = RTStrCopy(szOld, sizeof(szOld), pszOutput);
                    if (RT_SUCCESS(rc))
                        rc = RTStrCat(szOld, sizeof(szOld), ".old");
                    if (RT_SUCCESS(rc))
                        RTFileRename(pszOutput, szOld, RTFILEMOVE_FLAGS_REPLACE);
                }
                rc = RTStrmOpen(pszOutput, "w", &g_pReportOut);
                if (RT_FAILURE(rc))
                {
                    RTMsgError("Error opening '%s': %Rrc", pszOutput, rc);
                    break;
                }
            }
            rc = produceCpuReport();
            break;
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
        case kCpuReportOp_MsrsOnly:
        case kCpuReportOp_MsrsHacking:
            rc = probeMsrs(enmOp == kCpuReportOp_MsrsHacking, NULL, NULL, NULL, 0);
            break;
#else
        case kCpuReportOp_Dummy:
            break;
#endif
    }

    /*
     * Close the output files.
     */
    if (g_pReportOut)
    {
        RTStrmClose(g_pReportOut);
        g_pReportOut = NULL;
    }

    if (g_pDebugOut)
    {
        RTStrmClose(g_pDebugOut);
        g_pDebugOut = NULL;
    }

    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

