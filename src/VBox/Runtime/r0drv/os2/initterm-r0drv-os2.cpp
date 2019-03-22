/* $Id$ */
/** @file
 * IPRT - Initialization & Termination, Ring-0 Driver, OS/2.
 */

/*
 * Contributed by knut st. osmundsen.
 *
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
 *
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "the-os2-kernel.h"

#include "internal/initterm.h"
#include <iprt/errcore.h>
#include <iprt/assert.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Pointer to the 1st DOS variable table. */
PCDOSTABLE  g_pDosTable = NULL;
/** Pointer to the 2nd DOS variable table. */
PCDOSTABLE2 g_pDosTable2 = NULL;
/** Pointer to the global info segment. */
PGINFOSEG   g_pGIS = NULL;
/** Far 16:16 pointer to the local info segment.
 * IIRC this must be converted to a flat pointer when accessed to work correctly on SMP systems. */
RTFAR16     g_fpLIS = {0, 0};


DECLHIDDEN(int) rtR0InitNative(void)
{
    /*
     * Get the DOS Tables.
     */
    RTFAR16 fp;
    int rc = RTR0Os2DHQueryDOSVar(DHGETDOSV_DOSTABLES, 0, &fp);
    AssertMsgReturn(!rc, ("rc=%d\n", rc), VERR_INTERNAL_ERROR);
    g_pDosTable = (PCDOSTABLE)RTR0Os2Virt2Flat(fp);
    AssertReturn(g_pDosTable, VERR_INTERNAL_ERROR);
    g_pDosTable2 = (PCDOSTABLE2)((const uint8_t *)g_pDosTable + g_pDosTable->cul * sizeof(ULONG) + 1);

    /*
     * Get the addresses of the two info segments.
     */
    rc = RTR0Os2DHQueryDOSVar(DHGETDOSV_SYSINFOSEG, 0, &fp);
    AssertMsgReturn(!rc, ("rc=%d\n", rc), VERR_INTERNAL_ERROR);
    g_pGIS = (PGINFOSEG)RTR0Os2Virt2Flat(fp);
    rc = RTR0Os2DHQueryDOSVar(DHGETDOSV_LOCINFOSEG, 0, &fp);
    AssertMsgReturn(!rc, ("rc=%d\n", rc), VERR_INTERNAL_ERROR);
    g_fpLIS = fp;

    return VINF_SUCCESS;
}


DECLHIDDEN(void) rtR0TermNative(void)
{
    /* nothing to do here yet. */
}


/**
 * Converts a 16:16 pointer to a flat pointer.
 *
 * @returns Flat pointer (NULL if fp is NULL).
 * @param   fp      Far pointer to convert.
 */
RTR0DECL(void *) RTR0Os2Virt2Flat(RTFAR16 fp)
{
    return (void *)KernSelToFlat(((uint32_t)fp.sel << 16) | fp.off);
}

