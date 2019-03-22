/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Shared modules.
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
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
#include "VBGLR3Internal.h"
#include <iprt/mem.h>
#include <iprt/string.h>

/**
 * Registers a new shared module for the VM
 *
 * @returns IPRT status code.
 * @param   pszModuleName       Module name
 * @param   pszVersion          Module version
 * @param   GCBaseAddr          Module base address
 * @param   cbModule            Module size
 * @param   cRegions            Number of shared region descriptors
 * @param   pRegions            Shared region(s)
 */
VBGLR3DECL(int) VbglR3RegisterSharedModule(char *pszModuleName, char *pszVersion,
                                           RTGCPTR64  GCBaseAddr, uint32_t cbModule,
                                           unsigned cRegions, VMMDEVSHAREDREGIONDESC *pRegions)
{
    VMMDevSharedModuleRegistrationRequest *pReq;
    int rc;

    /* Sanity check. */
    AssertReturn(cRegions < VMMDEVSHAREDREGIONDESC_MAX, VERR_INVALID_PARAMETER);

    pReq = (VMMDevSharedModuleRegistrationRequest *)RTMemAllocZ(RT_OFFSETOF(VMMDevSharedModuleRegistrationRequest, aRegions[cRegions]));
    AssertReturn(pReq, VERR_NO_MEMORY);

    vmmdevInitRequest(&pReq->header, VMMDevReq_RegisterSharedModule);
    pReq->header.size   = RT_OFFSETOF(VMMDevSharedModuleRegistrationRequest, aRegions[cRegions]);
    pReq->GCBaseAddr    = GCBaseAddr;
    pReq->cbModule      = cbModule;
    pReq->cRegions      = cRegions;
#ifdef RT_OS_WINDOWS
# if ARCH_BITS == 32
    pReq->enmGuestOS    = VBOXOSFAMILY_Windows32;
# else
    pReq->enmGuestOS    = VBOXOSFAMILY_Windows64;
# endif
#else
    /** @todo */
    pReq->enmGuestOS    = VBOXOSFAMILY_Unknown;
#endif
    for (unsigned i = 0; i < cRegions; i++)
        pReq->aRegions[i] = pRegions[i];

    if (    RTStrCopy(pReq->szName, sizeof(pReq->szName), pszModuleName) != VINF_SUCCESS
        ||  RTStrCopy(pReq->szVersion, sizeof(pReq->szVersion), pszVersion) != VINF_SUCCESS)
    {
        rc = VERR_BUFFER_OVERFLOW;
        goto end;
    }

    rc = vbglR3GRPerform(&pReq->header);

end:
    RTMemFree(pReq);
    return rc;

}

/**
 * Unregisters a shared module for the VM
 *
 * @returns IPRT status code.
 * @param   pszModuleName       Module name
 * @param   pszVersion          Module version
 * @param   GCBaseAddr          Module base address
 * @param   cbModule            Module size
 */
VBGLR3DECL(int) VbglR3UnregisterSharedModule(char *pszModuleName, char *pszVersion, RTGCPTR64 GCBaseAddr, uint32_t cbModule)
{
    VMMDevSharedModuleUnregistrationRequest Req;

    vmmdevInitRequest(&Req.header, VMMDevReq_UnregisterSharedModule);
    Req.GCBaseAddr    = GCBaseAddr;
    Req.cbModule      = cbModule;

    if (    RTStrCopy(Req.szName, sizeof(Req.szName), pszModuleName) != VINF_SUCCESS
        ||  RTStrCopy(Req.szVersion, sizeof(Req.szVersion), pszVersion) != VINF_SUCCESS)
    {
        return VERR_BUFFER_OVERFLOW;
    }
    return vbglR3GRPerform(&Req.header);
}

/**
 * Checks registered modules for shared pages
 *
 * @returns IPRT status code.
 */
VBGLR3DECL(int) VbglR3CheckSharedModules()
{
    VMMDevSharedModuleCheckRequest Req;

    vmmdevInitRequest(&Req.header, VMMDevReq_CheckSharedModules);
    return vbglR3GRPerform(&Req.header);
}

/**
 * Checks if page sharing is enabled.
 *
 * @returns true/false enabled/disabled
 */
VBGLR3DECL(bool) VbglR3PageSharingIsEnabled()
{
    VMMDevPageSharingStatusRequest Req;

    vmmdevInitRequest(&Req.header, VMMDevReq_GetPageSharingStatus);
    int rc = vbglR3GRPerform(&Req.header);
    if (RT_SUCCESS(rc))
        return Req.fEnabled;
    return false;
}

/**
 * Checks if page sharing is enabled.
 *
 * @returns true/false enabled/disabled
 */
VBGLR3DECL(int) VbglR3PageIsShared(RTGCPTR pPage, bool *pfShared, uint64_t *puPageFlags)
{
#ifdef DEBUG
    VMMDevPageIsSharedRequest Req;

    vmmdevInitRequest(&Req.header, VMMDevReq_DebugIsPageShared);
    Req.GCPtrPage = pPage;
    int rc = vbglR3GRPerform(&Req.header);
    if (RT_SUCCESS(rc))
    {
        *pfShared    = Req.fShared;
        *puPageFlags = Req.uPageFlags;
    }
    return rc;
#else
    RT_NOREF3(pPage, pfShared, puPageFlags);
    return VERR_NOT_IMPLEMENTED;
#endif
}
