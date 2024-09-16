/* $Id$ */
/** @file
 * IPRT - No-CRT - Basic allocators, Windows.
 */

/*
 * Copyright (C) 2006-2024 Oracle and/or its affiliates.
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
#include "internal/iprt.h"
#include <iprt/mem.h>

#include <iprt/nt/nt-and-windows.h>


#undef RTMemTmpFree
RTDECL(void) RTMemTmpFree(void *pv)
{
    HeapFree(GetProcessHeap(), 0, pv);
}


#undef RTMemFree
RTDECL(void) RTMemFree(void *pv)
{
    HeapFree(GetProcessHeap(), 0, pv);
}


#undef RTMemTmpAllocTag
RTDECL(void *) RTMemTmpAllocTag(size_t cb, const char *pszTag)
{
    RT_NOREF(pszTag);
    return HeapAlloc(GetProcessHeap(), 0, cb);
}


#undef RTMemTmpAllocZTag
RTDECL(void *) RTMemTmpAllocZTag(size_t cb, const char *pszTag)
{
    RT_NOREF(pszTag);
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb);
}


#undef RTMemAllocTag
RTDECL(void *) RTMemAllocTag(size_t cb, const char *pszTag)
{
    RT_NOREF(pszTag);
    return HeapAlloc(GetProcessHeap(), 0, cb);
}


#undef RTMemAllocZTag
RTDECL(void *) RTMemAllocZTag(size_t cb, const char *pszTag)
{
    RT_NOREF(pszTag);
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb);
}


#undef RTMemAllocVarTag
RTDECL(void *) RTMemAllocVarTag(size_t cbUnaligned, const char *pszTag)
{
    return RTMemAllocTag(cbUnaligned, pszTag);
}


#undef RTMemAllocZVarTag
RTDECL(void *) RTMemAllocZVarTag(size_t cbUnaligned, const char *pszTag)
{
    return RTMemAllocZTag(cbUnaligned, pszTag);
}


#undef RTMemReallocTag
RTDECL(void *) RTMemReallocTag(void *pvOld, size_t cbNew, const char *pszTag)
{
    RT_NOREF(pszTag);
    if (pvOld)
        return HeapReAlloc(GetProcessHeap(), 0, pvOld, cbNew);
    return HeapAlloc(GetProcessHeap(), 0, cbNew);
}

