/* $Id$ */
/** @file
 * IPRT - CRT Strings, memcmp().
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
#include <iprt/string.h>
#include <iprt/types.h>


/**
 * Copies a memory.
 *
 * @returns 0 if pvDst and pvSrc are equal
 * @returns <0 if pvDst is 'smaller' than pvSrc.
 * @returns >0 if pvDst is 'larger' than pvSrc.
 *
 * @param   pvDst       Pointer to the target block.
 * @param   pvSrc       Pointer to the source block.
 * @param   cb          The size of the block.
 */
#ifdef _MSC_VER
# if _MSC_VER >= 1400
__checkReturn int __cdecl memcmp(__in_bcount_opt(_Size) const void * pvDst, __in_bcount_opt(_Size) const void * pvSrc, __in size_t cb)
# else
int __cdecl memcmp(const void *pvDst, const void *pvSrc, size_t cb)
# endif
#else
int memcmp(const void *pvDst, const void *pvSrc, size_t cb)
#endif
{
    register union
    {
        uint8_t const  *pu8;
        uint32_t const *pu32;
        void const     *pv;
    } uDst, uSrc;
    uDst.pv = pvDst;
    uSrc.pv = pvSrc;

    /* 32-bit word compare. */
    register size_t c = cb >> 2;
    while (c-- > 0)
    {
        /* ASSUMES int is at least 32-bit! */
        register int32_t iDiff = *uDst.pu32++ - *uSrc.pu32++;
        if (iDiff)
            return iDiff;
    }

    /* Remaining byte moves. */
    c = cb & 3;
    while (c-- > 0)
    {
        register int8_t iDiff = *uDst.pu8++ - *uSrc.pu8++;
        if (iDiff)
            return iDiff;
    }

    return 0;
}

