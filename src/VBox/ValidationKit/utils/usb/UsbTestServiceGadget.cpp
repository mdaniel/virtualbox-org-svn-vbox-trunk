/* $Id$ */
/** @file
 * UsbTestServ - Remote USB test configuration and execution server, USB gadget host API.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
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

#include <iprt/asm.h>
#include <iprt/cdefs.h>
#include <iprt/ctype.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/types.h>

#include "UsbTestServiceGadgetInternal.h"


/*********************************************************************************************************************************
*   Constants And Macros, Structures and Typedefs                                                                                *
*********************************************************************************************************************************/

/**
 * Internal UTS gadget host instance data.
 */
typedef struct UTSGADGETINT
{
    /** Reference counter. */
    volatile uint32_t         cRefs;
    /** Pointer to the gadget class callback table. */
    PCUTSGADGETCLASSIF        pClassIf;
    /** The gadget host handle. */
    UTSGADGETHOST             hGadgetHost;
    /** Class specific instance data - variable in size. */
    uint8_t                   abClassInst[1];
} UTSGADGETINT;
/** Pointer to the internal gadget host instance data. */
typedef UTSGADGETINT *PUTSGADGETINT;


/*********************************************************************************************************************************
*   Global variables                                                                                                             *
*********************************************************************************************************************************/

/** Known gadget host interfaces. */
static const PCUTSGADGETCLASSIF g_apUtsGadgetClass[] =
{
    &g_UtsGadgetClassTest
};


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/**
 * Destroys a gadget instance.
 *
 * @returns nothing.
 * @param   pThis    The gadget instance.
 */
static void utsGadgetDestroy(PUTSGADGETINT pThis)
{
    pThis->pClassIf->pfnTerm((PUTSGADGETCLASSINT)&pThis->abClassInst[0]);
    RTMemFree(pThis);
}


DECLHIDDEN(int) utsGadgetCreate(UTSGADGETHOST hGadgetHost, UTSGADGETCLASS enmClass,
                                PCUTSGADGETCFGITEM paCfg, PUTSGADET phGadget)
{
    int rc = VINF_SUCCESS;
    PCUTSGADGETCLASSIF pClassIf = NULL;

    /* Get the interface. */
    for (unsigned i = 0; i < RT_ELEMENTS(g_apUtsGadgetClass); i++)
    {
        if (g_apUtsGadgetClass[i]->enmClass == enmClass)
        {
            pClassIf = g_apUtsGadgetClass[i];
            break;
        }
    }

    if (RT_LIKELY(pClassIf))
    {
        PUTSGADGETINT pThis = (PUTSGADGETINT)RTMemAllocZ(RT_OFFSETOF(UTSGADGETINT, abClassInst[pClassIf->cbClass]));
        if (RT_LIKELY(pThis))
        {
            pThis->cRefs       = 1;
            pThis->hGadgetHost = hGadgetHost;
            pThis->pClassIf    = pClassIf;
            rc = pClassIf->pfnInit((PUTSGADGETCLASSINT)&pThis->abClassInst[0], paCfg);
            if (RT_SUCCESS(rc))
            {
                /* Connect the gadget to the host. */
                rc = utsGadgetHostGadgetConnect(pThis->hGadgetHost, pThis);
                if (RT_SUCCESS(rc))
                    *phGadget = pThis;
            }
            else
                RTMemFree(pThis);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    return rc;
}


DECLHIDDEN(uint32_t) utsGadgetRetain(UTSGADGET hGadget)
{
    PUTSGADGETINT pThis = hGadget;

    AssertPtrReturn(pThis, 0);

    return ASMAtomicIncU32(&pThis->cRefs);
}


DECLHIDDEN(uint32_t) utsGadgetRelease(UTSGADGET hGadget)
{
    PUTSGADGETINT pThis = hGadget;

    AssertPtrReturn(pThis, 0);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    if (!cRefs)
        utsGadgetDestroy(pThis);

    return cRefs;
}


DECLHIDDEN(uint32_t) utsGadgetGetBusId(UTSGADGET hGadget)
{
    PUTSGADGETINT pThis = hGadget;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    return pThis->pClassIf->pfnGetBusId((PUTSGADGETCLASSINT)&pThis->abClassInst[0]);
}


DECLHIDDEN(uint32_t) utsGadgetGetDevId(UTSGADGET hGadget)
{
    PUTSGADGETINT pThis = hGadget;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    return 1; /** @todo Current assumption which is true on Linux with dummy_hcd. */
}


DECLHIDDEN(int) utsGadgetConnect(UTSGADGET hGadget)
{
    PUTSGADGETINT pThis = hGadget;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    int rc = pThis->pClassIf->pfnConnect((PUTSGADGETCLASSINT)&pThis->abClassInst[0]);
    if (RT_SUCCESS(rc))
        rc = utsGadgetHostGadgetConnect(pThis->hGadgetHost, hGadget);

    return rc;
}


DECLHIDDEN(int) utsGadgetDisconnect(UTSGADGET hGadget)
{
    PUTSGADGETINT pThis = hGadget;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    int rc = utsGadgetHostGadgetDisconnect(pThis->hGadgetHost, hGadget);
    if (RT_SUCCESS(rc))
        rc = pThis->pClassIf->pfnDisconnect((PUTSGADGETCLASSINT)&pThis->abClassInst[0]);

    return rc;
}

