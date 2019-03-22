/* $Id$ */
/** @file
 * IPRT - Memory Allocation, Extended Alloc Workers, Windows.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
#define RTMEM_NO_WRAP_TO_EF_APIS
#include <iprt/mem.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/param.h>
#include "../allocex.h"

#include <iprt/win/windows.h>


static int rtMemAllocExInRange(size_t cbAlloc, uint32_t fFlags, void **ppv, uintptr_t uAddr, uintptr_t uAddrLast)
{
    /*
     * Try with every possible address hint since the possible range is very limited.
     */
    DWORD     fPageProt = (fFlags & RTMEMALLOCEX_FLAGS_EXEC ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
    while (uAddr <= uAddrLast)
    {
        MEMORY_BASIC_INFORMATION MemInfo;
        SIZE_T cbRange = VirtualQuery((void *)uAddr, &MemInfo, sizeof(MemInfo));
        AssertReturn(cbRange == sizeof(MemInfo), VERR_NOT_SUPPORTED);
        Assert(MemInfo.RegionSize > 0);

        if (   MemInfo.State == MEM_FREE
            && MemInfo.RegionSize >= cbAlloc)
        {
            void *pv = VirtualAlloc((void *)uAddr, cbAlloc, MEM_RESERVE | MEM_COMMIT, fPageProt);
            if ((uintptr_t)pv == uAddr)
            {
                *ppv = pv;
                return VINF_SUCCESS;
            }
            AssertStmt(!pv, VirtualFree(pv, cbAlloc, MEM_RELEASE));
        }

        /* skip ahead */
        uintptr_t uAddrNext = (uintptr_t)MemInfo.BaseAddress + MemInfo.RegionSize;
        if (uAddrNext <= uAddr)
            break;
        uAddr += uAddrNext;
    }

    return VERR_NO_MEMORY;
}


DECLHIDDEN(int) rtMemAllocEx16BitReach(size_t cbAlloc, uint32_t fFlags, void **ppv)
{
    cbAlloc = RT_ALIGN_Z(cbAlloc, PAGE_SIZE);
    AssertReturn(cbAlloc <= _64K - PAGE_SIZE, VERR_NO_MEMORY);

    /* Seems this doesn't work on W7/64... */
    return rtMemAllocExInRange(cbAlloc, fFlags, ppv, PAGE_SIZE, _64K - cbAlloc);
}


DECLHIDDEN(int) rtMemAllocEx32BitReach(size_t cbAlloc, uint32_t fFlags, void **ppv)
{
    cbAlloc = RT_ALIGN_Z(cbAlloc, PAGE_SIZE);
    AssertReturn(cbAlloc <= _2G+_1G, VERR_NO_MEMORY);

    /*
     * Just try first.
     */
    DWORD     fPageProt = (fFlags & RTMEMALLOCEX_FLAGS_EXEC ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
    void *pv = VirtualAlloc(NULL, cbAlloc, MEM_RESERVE | MEM_COMMIT, fPageProt);
    if (!pv)
        return VERR_NO_MEMORY;
    if ((uintptr_t)pv + cbAlloc - 1 < _4G)
    {
        *ppv = pv;
        return VINF_SUCCESS;
    }
    VirtualFree(pv, cbAlloc, MEM_RELEASE);

    /*
     * No luck, do address scan based allocation.
     */
    return rtMemAllocExInRange(cbAlloc, fFlags, ppv, _64K, _4G - cbAlloc);
}


DECLHIDDEN(void) rtMemFreeExYyBitReach(void *pv, size_t cb, uint32_t fFlags)
{
    RT_NOREF_PV(fFlags);

    BOOL fRc = VirtualFree(pv, cb, MEM_RELEASE);
    Assert(fRc); RT_NOREF_PV(fRc);
}

