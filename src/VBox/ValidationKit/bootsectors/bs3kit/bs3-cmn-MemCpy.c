/* $Id$ */
/** @file
 * BS3Kit - Bs3MemCpy
 */

/*
 * Copyright (C) 2007-2019 Oracle Corporation
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

#include "bs3kit-template-header.h"

#undef Bs3MemCpy
BS3_CMN_DEF(void BS3_FAR *, Bs3MemCpy,(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy))
{
#if 1
    const size_t BS3_FAR   *pBigSrc = (const size_t BS3_FAR *)pvSrc;
    size_t BS3_FAR         *pBigDst = (size_t *)pvDst;
    size_t                  cBig = cbToCopy / sizeof(size_t);
    while (cBig-- > 0)
        *pBigDst++ = *pBigSrc++;

    switch (cbToCopy % sizeof(size_t))
    {
#if TMPL_BITS >= 64
        case 7: ((uint8_t BS3_FAR *)pBigDst)[6] = ((const uint8_t BS3_FAR *)pBigSrc)[6];
        case 6: ((uint8_t BS3_FAR *)pBigDst)[5] = ((const uint8_t BS3_FAR *)pBigSrc)[5];
        case 5: ((uint8_t BS3_FAR *)pBigDst)[4] = ((const uint8_t BS3_FAR *)pBigSrc)[4];
        case 4: ((uint8_t BS3_FAR *)pBigDst)[3] = ((const uint8_t BS3_FAR *)pBigSrc)[3];
#endif
#if TMPL_BITS >= 32
        case 3: ((uint8_t BS3_FAR *)pBigDst)[2] = ((const uint8_t BS3_FAR *)pBigSrc)[2];
        case 2: ((uint8_t BS3_FAR *)pBigDst)[1] = ((const uint8_t BS3_FAR *)pBigSrc)[1];
#endif
        case 1: ((uint8_t BS3_FAR *)pBigDst)[0] = ((const uint8_t BS3_FAR *)pBigSrc)[0];
        case 0:
            break;
    }

#else
    size_t          cLargeRounds;
    BS3CPTRUNION    uSrc;
    BS3PTRUNION     uDst;
    uSrc.pv = pvSrc;
    uDst.pv = pvDst;

    cLargeRounds = cbToCopy / sizeof(*uSrc.pcb);
    while (cLargeRounds-- > 0)
        *uDst.pcb++ = *uSrc.pcb++;

    cbToCopy %= sizeof(*uSrc.pcb);
    while (cbToCopy-- > 0)
        *uDst.pb++ = *uSrc.pb++;

#endif

    return pvDst;
}

