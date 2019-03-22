/* $Id$ */
/** @file
 * VBox HDD container test utility - I/O data generator.
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOGGROUP LOGGROUP_DEFAULT
#include <iprt/log.h>
#include <iprt/errcore.h>
#include <iprt/mem.h>
#include <iprt/rand.h>
#include <iprt/assert.h>

#include "VDIoRnd.h"

/**
 * I/O random data generator instance data.
 */
typedef struct VDIORND
{
    /** Pointer to the buffer holding the random data. */
    uint8_t *pbPattern;
    /** Size of the buffer. */
    size_t   cbPattern;
    /** RNG */
    RTRAND   hRand;
} VDIORND;

int VDIoRndCreate(PPVDIORND ppIoRnd, size_t cbPattern, uint64_t uSeed)
{
    PVDIORND pIoRnd = NULL;
    int rc = VINF_SUCCESS;

    AssertPtrReturn(ppIoRnd, VERR_INVALID_POINTER);

    pIoRnd = (PVDIORND)RTMemAllocZ(sizeof(VDIORND));
    if (pIoRnd)
        pIoRnd->pbPattern = (uint8_t *)RTMemPageAllocZ(cbPattern);

    if (   pIoRnd
        && pIoRnd->pbPattern)
    {
        pIoRnd->cbPattern = cbPattern;

        rc = RTRandAdvCreateParkMiller(&pIoRnd->hRand);
        if (RT_SUCCESS(rc))
        {
            RTRandAdvSeed(pIoRnd->hRand, uSeed);
            RTRandAdvBytes(pIoRnd->hRand, pIoRnd->pbPattern, cbPattern);
        }
        else
        {
            RTMemPageFree(pIoRnd->pbPattern, cbPattern);
            RTMemFree(pIoRnd);
        }
    }
    else
    {
        if (pIoRnd)
            RTMemFree(pIoRnd);
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
        *ppIoRnd = pIoRnd;

    return rc;
}

void VDIoRndDestroy(PVDIORND pIoRnd)
{
    AssertPtrReturnVoid(pIoRnd);

    RTRandAdvDestroy(pIoRnd->hRand);
    RTMemPageFree(pIoRnd->pbPattern, pIoRnd->cbPattern);
    RTMemFree(pIoRnd);
}

int VDIoRndGetBuffer(PVDIORND pIoRnd, void **ppv, size_t cb)
{
    AssertPtrReturn(pIoRnd, VERR_INVALID_POINTER);
    AssertPtrReturn(ppv, VERR_INVALID_POINTER);
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);

    if (cb > pIoRnd->cbPattern - 512)
        return VERR_INVALID_PARAMETER;

    *ppv = pIoRnd->pbPattern + RT_ALIGN_64(RTRandAdvU64Ex(pIoRnd->hRand, 0, pIoRnd->cbPattern - cb - 512), 512);
    return VINF_SUCCESS;
}


uint32_t VDIoRndGetU32Ex(PVDIORND pIoRnd, uint32_t uMin, uint32_t uMax)
{
    return RTRandAdvU32Ex(pIoRnd->hRand, uMin, uMax);
}

