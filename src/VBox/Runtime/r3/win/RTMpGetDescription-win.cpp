/* $Id$ */
/** @file
 * IPRT - Multiprocessor, RTMpGetDescription, edition for newer Windows versions.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/win/windows.h>
#include <iprt/mp.h>
#include "internal/iprt.h"

#include <iprt/string.h>
#include <iprt/utf16.h>
#include <iprt/err.h>


RTDECL(int) RTMpGetDescription(RTCPUID idCpu, char *pszBuf, size_t cbBuf)
{
    if (idCpu != NIL_RTCPUID && !RTMpIsCpuOnline(idCpu))
        return RTMpIsCpuPossible(idCpu)
             ? VERR_CPU_OFFLINE
             : VERR_CPU_NOT_FOUND;

    /*
     * Newer windows versions (at least 11), keeps info about the CPU in the
     * registry.
     *
     * 'Get-WmiObject win32_processor' queries this information for instance
     * (probably by way of cimwin32.dll and framedynos.dll).
     */
    HKEY    hKeyCpuRoot = NULL;
    LSTATUS lrc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor", 0 /* ulOptions*/,
                                KEY_ENUMERATE_SUB_KEYS | KEY_READ, &hKeyCpuRoot);
    AssertMsg(lrc == ERROR_SUCCESS, ("lrc=%u\n", lrc));
    if (lrc == ERROR_SUCCESS)
    {
        HKEY hKeyCpu = NULL;
        if (idCpu == NIL_RTCPUID)
        {
            lrc = RegOpenKeyExW(hKeyCpuRoot, L"0", 0 /* ulOptions*/, KEY_ENUMERATE_SUB_KEYS | KEY_READ, &hKeyCpu);
            AssertMsg(lrc == ERROR_SUCCESS, ("lrc=%u\n", lrc));
            /** @todo if (lrc != ERROR_SUCCESS) enumerate subkeys */
        }
        else
        {
            WCHAR wszCpuNo[32];
            RTUtf16Printf(wszCpuNo, RT_ELEMENTS(wszCpuNo), "%u", idCpu);
            lrc = RegOpenKeyExW(hKeyCpuRoot, wszCpuNo, 0 /* ulOptions*/, KEY_ENUMERATE_SUB_KEYS | KEY_READ, &hKeyCpu);
            AssertMsg(lrc == ERROR_SUCCESS, ("lrc=%u\n", lrc));
        }
        if (lrc == ERROR_SUCCESS)
        {
            WCHAR wszBuf[1536] = {0}; /* reasonable buffer size, right... */
            DWORD dwType = REG_NONE;
            DWORD cbData = sizeof(wszBuf) - sizeof(WCHAR);
            lrc = RegQueryValueExW(hKeyCpu, L"ProcessorNameString", NULL /*lpReserved*/, &dwType, (PBYTE)&wszBuf[0], &cbData);
            if (lrc == ERROR_SUCCESS)
            {
                if (dwType == REG_SZ)
                {
                    if (cbBuf != 0 && pszBuf)
                        return RTUtf16ToUtf8Ex(wszBuf, cbData / sizeof(WCHAR), &pszBuf, cbBuf, NULL);
                    return VERR_BUFFER_OVERFLOW;
                }
            }
        }
    }

    /* Fallback... */
    return RTStrCopy(pszBuf, cbBuf, "Unknown");
}
RT_EXPORT_SYMBOL(RTMpGetDescription);

