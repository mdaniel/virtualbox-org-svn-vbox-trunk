/* $Id$ */
/** @file
 * IPRT - RTSystemGetNativeArch, Windows ring-3.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
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
#include <iprt/system.h>
#include "internal/iprt.h"

#include <iprt/nt/nt-and-windows.h>

#include "internal-r3-win.h"


typedef BOOL (WINAPI *PFNISWOW64PROCESS2)(HANDLE, USHORT *, USHORT *);


static uint32_t rtSystemNativeToArchVal(USHORT uMachine)
{
    switch (uMachine)
    {
        case IMAGE_FILE_MACHINE_I386:       return RT_ARCH_VAL_X86;
        case IMAGE_FILE_MACHINE_AMD64:      return RT_ARCH_VAL_AMD64;
        case IMAGE_FILE_MACHINE_ARM:        return RT_ARCH_VAL_ARM32;
        case IMAGE_FILE_MACHINE_ARM64:      return RT_ARCH_VAL_ARM64;
        default:
            AssertMsgFailed(("Unknown value: uMachine=%#x\n", uMachine));
            return 0;
    }
}


RTDECL(uint32_t) RTSystemGetNativeArch(void)
{
    Assert(g_hModKernel32 != NULL);

    /*
     * We try IsWow64Process2 first.
     */
    static PFNISWOW64PROCESS2 s_pfnIsWow64Process2 = NULL;
    PFNISWOW64PROCESS2 pfnIsWow64Process2 = s_pfnIsWow64Process2;
    if (!pfnIsWow64Process2)
        s_pfnIsWow64Process2 = pfnIsWow64Process2 = (PFNISWOW64PROCESS2)GetProcAddress(g_hModKernel32, "IsWow64Process2");
    if (s_pfnIsWow64Process2)
    {
        USHORT uProcess = 0;
        USHORT uNative  = 0;
        if (pfnIsWow64Process2(GetCurrentProcess(), &uProcess, &uNative))
            return rtSystemNativeToArchVal(uNative);
        AssertFailed();
    }

    /*
     * The fallback is KUSER_SHARED_DATA::NativeProcessorArchitecture.
     * This works for NT4 and later.
     */
    if (g_enmWinVer >= kRTWinOSType_NT4)
    {
        KUSER_SHARED_DATA volatile *pUserSharedData = (KUSER_SHARED_DATA volatile *)MM_SHARED_USER_DATA_VA;
        return rtSystemNativeToArchVal(pUserSharedData->NativeProcessorArchitecture);
    }
    return RT_ARCH_VAL;
}

