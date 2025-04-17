/* $Id$ */
/** @file
 * VBoxCpuReport internal header file.
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

#ifndef VMM_INCLUDED_SRC_tools_VBoxCpuReport_h
#define VMM_INCLUDED_SRC_tools_VBoxCpuReport_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/sup.h>
#include <VBox/vmm/cpum.h>
#include <iprt/stream.h>

RT_C_DECLS_BEGIN

typedef struct VBCPUREPMSRACCESSORS
{
    /** Wheter MSR prober can read/modify/restore MSRs more or less
     *  atomically, without allowing other code to be executed. */
    bool                    fAtomic;
    /** @copydoc SUPR3MsrProberRead  */
    DECLCALLBACKMEMBER(int, pfnMsrProberRead,(uint32_t uMsr, RTCPUID idCpu, uint64_t *puValue, bool *pfGp));
    /** @copydoc SUPR3MsrProberWrite  */
    DECLCALLBACKMEMBER(int, pfnMsrProberWrite,(uint32_t uMsr, RTCPUID idCpu, uint64_t uValue, bool *pfGp));
    /** @copydoc SUPR3MsrProberModify */
    DECLCALLBACKMEMBER(int, pfnMsrProberModify,(uint32_t uMsr, RTCPUID idCpu, uint64_t fAndMask, uint64_t fOrMask,
                                                PSUPMSRPROBERMODIFYRESULT pResult));
    /** Termination callback, optional. */
    DECLCALLBACKMEMBER(void, pfnTerm,(void));
} VBCPUREPMSRACCESSORS;
typedef VBCPUREPMSRACCESSORS *PVBCPUREPMSRACCESSORS;

extern PRTSTREAM        g_pReportOut;
extern PRTSTREAM        g_pDebugOut;
extern CPUMCPUVENDOR    g_enmVendor;
extern CPUMMICROARCH    g_enmMicroarch;
extern const char      *g_pszCpuNameOverride;
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
extern bool             g_fNoMsrs;
#endif

extern void vbCpuRepDebug(const char *pszMsg, ...);
extern void vbCpuRepFileHdr(const char *pszName, const char *pszNameC);
extern void vbCpuRepPrintf(const char *pszMsg, ...);
extern const char *vbCpuVendorToString(CPUMCPUVENDOR enmCpuVendor);

extern int  produceCpuReport(void); /* arch specific */

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
extern int  probeMsrs(bool fHacking, const char *pszNameC, const char *pszCpuDesc, char *pszMsrMask, size_t cbMsrMask);
extern int  VbCpuRepMsrProberInitSupDrv(PVBCPUREPMSRACCESSORS pMsrAccessors);
extern int  VbCpuRepMsrProberInitPlatform(PVBCPUREPMSRACCESSORS pMsrAccessors);
#endif

RT_C_DECLS_END

#endif /* !VMM_INCLUDED_SRC_tools_VBoxCpuReport_h */

