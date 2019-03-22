/* $Id$ */
/** @file
 * IPRT - Memory Allocation, Ring-0 Driver, Haiku.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
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
#include "the-haiku-kernel.h"
#include "internal/iprt.h"
#include <iprt/mem.h>
#include <iprt/log.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/thread.h>
#include "r0drv/alloc-r0drv.h"


/**
 * OS specific allocation function.
 */
int rtR0MemAllocEx(size_t cb, uint32_t fFlags, PRTMEMHDR *ppHdr)
{
    if (RT_UNLIKELY(fFlags & RTMEMHDR_FLAG_ANY_CTX))
        return VERR_NOT_SUPPORTED;

    PRTMEMHDR pHdr = (PRTMEMHDR)malloc(cb + sizeof(*pHdr));
    if (RT_UNLIKELY(!pHdr))
    {
        LogRel(("rtR0MemAllocEx(%u, %#x) failed\n",(unsigned)cb + sizeof(*pHdr), fFlags));
        return VERR_NO_MEMORY;
    }

    pHdr->u32Magic  = RTMEMHDR_MAGIC;
    pHdr->fFlags    = fFlags;
    pHdr->cb        = cb;
    pHdr->cbReq     = cb;
    *ppHdr = pHdr;
    return VINF_SUCCESS;
}


/**
 * OS specific free function.
 */
void rtR0MemFree(PRTMEMHDR pHdr)
{
    pHdr->u32Magic += 1;
    free(pHdr);
}


RTR0DECL(void *) RTMemContAlloc(PRTCCPHYS pPhys, size_t cb) RT_NO_THROW_DEF
{
    /*
     * Validate input.
     */
    AssertPtr(pPhys);
    Assert(cb > 0);
    RT_ASSERT_PREEMPTIBLE();

    /*
     * Allocate the memory and ensure that the API is still providing
     * memory that's always below 4GB.
     */
    cb = RT_ALIGN_Z(cb, PAGE_SIZE);
    void *pv;
    area_id area = create_area("VirtualBox Contig Alloc", &pv, B_ANY_KERNEL_ADDRESS, cb, B_32_BIT_CONTIGUOUS,
                               B_READ_AREA | B_WRITE_AREA);
    if (area >= 0)
    {
        physical_entry physMap[2];
        if (get_memory_map(pv, cb, physMap, 2)>= B_OK)
        {
            *pPhys = physMap[0].address;
            return pv;
        }
        delete_area(area);
        AssertMsgFailed(("Cannot get_memory_map for contig alloc! cb=%u\n",(unsigned)cb));
    }
    else
    AssertMsgFailed(("Cannot create_area for contig alloc! cb=%u error=0x%08lx\n",(unsigned)cb, area));
    return NULL;
}


RTR0DECL(void) RTMemContFree(void *pv, size_t cb) RT_NO_THROW_DEF
{
    RT_ASSERT_PREEMPTIBLE();
    if (pv)
    {
        Assert(cb > 0);

        area_id area = area_for(pv);
        if (area >= B_OK)
        delete_area(area);
        else
        AssertMsgFailed(("Cannot find area to delete! cb=%u error=0x%08lx\n",(unsigned)cb, area));
    }
}

