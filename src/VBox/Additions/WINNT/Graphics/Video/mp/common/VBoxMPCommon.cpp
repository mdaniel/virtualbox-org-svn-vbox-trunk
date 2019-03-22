/* $Id$ */
/** @file
 * VBox Miniport common utils
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

#include "VBoxMPCommon.h"
#include <VBoxVideoVBE.h>

int VBoxMPCmnMapAdapterMemory(PVBOXMP_COMMON pCommon, void **ppv, uint32_t ulOffset, uint32_t ulSize)
{
    PVBOXMP_DEVEXT pPEXT = VBoxCommonToPrimaryExt(pCommon);

    LOGF(("0x%08X[0x%X]", ulOffset, ulSize));

    if (!ulSize)
    {
        WARN(("Illegal length 0!"));
        return ERROR_INVALID_PARAMETER;
    }

    PHYSICAL_ADDRESS FrameBuffer;
    FrameBuffer.QuadPart = VBoxCommonFromDeviceExt(pPEXT)->phVRAM.QuadPart + ulOffset;

    PVOID VideoRamBase = NULL;
    ULONG VideoRamLength = ulSize;
    VP_STATUS Status;
#ifndef VBOX_WITH_WDDM
    ULONG inIoSpace = 0;

    Status = VideoPortMapMemory(pPEXT, FrameBuffer, &VideoRamLength, &inIoSpace, &VideoRamBase);
#else
    NTSTATUS ntStatus = pPEXT->u.primary.DxgkInterface.DxgkCbMapMemory(pPEXT->u.primary.DxgkInterface.DeviceHandle,
            FrameBuffer,
            VideoRamLength,
            FALSE, /* IN BOOLEAN InIoSpace */
            FALSE, /* IN BOOLEAN MapToUserMode */
            MmNonCached, /* IN MEMORY_CACHING_TYPE CacheType */
            &VideoRamBase /*OUT PVOID *VirtualAddress*/
            );
    Assert(ntStatus == STATUS_SUCCESS);
    /* this is what VideoPortMapMemory returns according to the docs */
    Status = ntStatus == STATUS_SUCCESS ? NO_ERROR : ERROR_INVALID_PARAMETER;
#endif

    if (Status == NO_ERROR)
    {
        *ppv = VideoRamBase;
    }

    LOGF(("rc = %d", Status));

    return (Status==NO_ERROR) ? VINF_SUCCESS:VERR_INVALID_PARAMETER;
}

void VBoxMPCmnUnmapAdapterMemory(PVBOXMP_COMMON pCommon, void **ppv)
{
    LOGF_ENTER();

    PVBOXMP_DEVEXT pPEXT = VBoxCommonToPrimaryExt(pCommon);

    if (*ppv)
    {
#ifndef VBOX_WITH_WDDM
        VP_STATUS Status;
        Status = VideoPortUnmapMemory(pPEXT, *ppv, NULL);
        VBOXMP_WARN_VPS(Status);
#else
        NTSTATUS ntStatus;
        ntStatus = pPEXT->u.primary.DxgkInterface.DxgkCbUnmapMemory(pPEXT->u.primary.DxgkInterface.DeviceHandle, *ppv);
        Assert(ntStatus == STATUS_SUCCESS);
#endif
    }

    *ppv = NULL;

    LOGF_LEAVE();
}

bool VBoxMPCmnSyncToVideoIRQ(PVBOXMP_COMMON pCommon, PFNVIDEOIRQSYNC pfnSync, void *pvUser)
{
    PVBOXMP_DEVEXT pPEXT = VBoxCommonToPrimaryExt(pCommon);
    PMINIPORT_SYNCHRONIZE_ROUTINE pfnSyncMiniport = (PMINIPORT_SYNCHRONIZE_ROUTINE) pfnSync;

#ifndef VBOX_WITH_WDDM
    return !!VideoPortSynchronizeExecution(pPEXT, VpMediumPriority, pfnSyncMiniport, pvUser);
#else
    BOOLEAN fRet;
    DXGKCB_SYNCHRONIZE_EXECUTION pfnDxgkCbSync = pPEXT->u.primary.DxgkInterface.DxgkCbSynchronizeExecution;
    HANDLE hDev = pPEXT->u.primary.DxgkInterface.DeviceHandle;
    NTSTATUS ntStatus = pfnDxgkCbSync(hDev, pfnSyncMiniport, pvUser, 0, &fRet);
    AssertReturn(ntStatus == STATUS_SUCCESS, false);
    return !!fRet;
#endif
}

bool VBoxMPCmnUpdatePointerShape(PVBOXMP_COMMON pCommon, PVIDEO_POINTER_ATTRIBUTES pAttrs, uint32_t cbLength)
{
    const uint32_t fFlags = pAttrs->Enable & 0x0000FFFF;
    const uint32_t cHotX = (pAttrs->Enable >> 16) & 0xFF;
    const uint32_t cHotY = (pAttrs->Enable >> 24) & 0xFF;
    const uint32_t cWidth = pAttrs->Width;
    const uint32_t cHeight = pAttrs->Height;
    uint8_t *pPixels = &pAttrs->Pixels[0];

    int rc = VBoxHGSMIUpdatePointerShape(&pCommon->guestCtx,
                                         fFlags, cHotX, cHotY,
                                         cWidth, cHeight, pPixels,
                                         cbLength - sizeof(VIDEO_POINTER_ATTRIBUTES));
    return RT_SUCCESS(rc);
}
