/* $Id$ */
/** @file
 * VBox XPDM Display driver, DirectDraw callbacks
 */

/*
 * Copyright (C) 2011-2024 Oracle and/or its affiliates.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "VBoxDisp.h"
#include "VBoxDispDDraw.h"
#include "VBoxDispMini.h"
#include <iprt/asm.h>

/* Called to check if our driver can create surface with requested attributes */
DWORD APIENTRY VBoxDispDDCanCreateSurface(PDD_CANCREATESURFACEDATA lpCanCreateSurface)
{
    LOGF_ENTER();

    PDD_SURFACEDESC lpDDS = lpCanCreateSurface->lpDDSurfaceDesc;

    if (lpDDS->ddsCaps.dwCaps & DDSCAPS_ZBUFFER)
    {
        LOG(("No Z-Bufer support"));
        lpCanCreateSurface->ddRVal = DDERR_UNSUPPORTED;
        return DDHAL_DRIVER_HANDLED;
    }
    if (lpDDS->ddsCaps.dwCaps & DDSCAPS_TEXTURE)
    {
        LOG(("No texture support"));
        lpCanCreateSurface->ddRVal = DDERR_UNSUPPORTED;
        return DDHAL_DRIVER_HANDLED;
    }
    if (lpCanCreateSurface->bIsDifferentPixelFormat && (lpDDS->ddpfPixelFormat.dwFlags & DDPF_FOURCC))
    {
        LOG(("FOURCC not supported"));
        lpCanCreateSurface->ddRVal = DDERR_UNSUPPORTED;
        return DDHAL_DRIVER_HANDLED;
    }

    lpCanCreateSurface->ddRVal = DD_OK;
    LOGF_LEAVE();
    return DDHAL_DRIVER_HANDLED;
}

/* Called to create DirectDraw surface.
 * Note: we always return DDHAL_DRIVER_NOTHANDLED, which asks DirectDraw memory manager
 * to perform actual memory allocation in our DDraw heap.
 */
DWORD APIENTRY VBoxDispDDCreateSurface(PDD_CREATESURFACEDATA lpCreateSurface)
{
    LOGF_ENTER();

    PDD_SURFACE_LOCAL pSurf = lpCreateSurface->lplpSList[0];

    if (pSurf->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        LOG(("primary surface"));
        pSurf->lpGbl->fpVidMem = 0;
    }
    else
    {
        LOG(("non primary surface"));
        pSurf->lpGbl->fpVidMem = DDHAL_PLEASEALLOC_BLOCKSIZE;
    }
    pSurf->lpGbl->dwReserved1 = 0;

    LPDDSURFACEDESC pDesc = lpCreateSurface->lpDDSurfaceDesc;

    if (pDesc->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED4)
    {
        pSurf->lpGbl->lPitch = RT_ALIGN_T(pSurf->lpGbl->wWidth/2, 32, LONG);
    }
    else
    if (pDesc->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8)
    {
        pSurf->lpGbl->lPitch = RT_ALIGN_T(pSurf->lpGbl->wWidth, 32, LONG);
    }
    else
    {
        pSurf->lpGbl->lPitch = pSurf->lpGbl->wWidth*(pDesc->ddpfPixelFormat.dwRGBBitCount/8);
    }

    pSurf->lpGbl->dwBlockSizeX = pSurf->lpGbl->lPitch;
    pSurf->lpGbl->dwBlockSizeY = pSurf->lpGbl->wHeight;

    pDesc->lPitch = pSurf->lpGbl->lPitch;
    pDesc->dwFlags |= DDSD_PITCH;

    LOGF_LEAVE();
    return DDHAL_DRIVER_NOTHANDLED;
}

/* Called to destroy DirectDraw surface,
 * in particular we should free vhwa resources allocated on VBoxDispDDCreateSurface.
 * Note: we're always returning DDHAL_DRIVER_NOTHANDLED because we rely on DirectDraw memory manager.
 */
DWORD APIENTRY VBoxDispDDDestroySurface(PDD_DESTROYSURFACEDATA lpDestroySurface)
{
    LOGF_ENTER();

    lpDestroySurface->ddRVal = DD_OK;

    LOGF_LEAVE();
    return DDHAL_DRIVER_NOTHANDLED;
}

/* Called before first DDLock/after last DDUnlock to map/unmap surface memory from given process address space
 * We go easy way and map whole framebuffer and offscreen DirectDraw heap every time.
 */
DWORD APIENTRY VBoxDispDDMapMemory(PDD_MAPMEMORYDATA lpMapMemory)
{
    PVBOXDISPDEV pDev = (PVBOXDISPDEV) lpMapMemory->lpDD->dhpdev;
    VIDEO_SHARE_MEMORY smem;
    int rc;
    LOGF_ENTER();

    lpMapMemory->ddRVal = DDERR_GENERIC;

    memset(&smem, 0, sizeof(smem));
    smem.ProcessHandle = lpMapMemory->hProcess;

    if (lpMapMemory->bMap)
    {
        VIDEO_SHARE_MEMORY_INFORMATION  smemInfo;

        smem.ViewSize = pDev->layout.offDDrawHeap + pDev->layout.cbDDrawHeap;

        rc = VBoxDispMPShareVideoMemory(pDev->hDriver, &smem, &smemInfo);
        VBOX_WARNRC_RETV(rc, DDHAL_DRIVER_HANDLED);

        lpMapMemory->fpProcess = (FLATPTR) smemInfo.VirtualAddress;
    }
    else
    {
        smem.RequestedVirtualAddress = (PVOID) lpMapMemory->fpProcess;

        rc = VBoxDispMPUnshareVideoMemory(pDev->hDriver, &smem);
        VBOX_WARNRC_RETV(rc, DDHAL_DRIVER_HANDLED);
    }


    lpMapMemory->ddRVal = DD_OK;
    LOGF_LEAVE();
    return DDHAL_DRIVER_HANDLED;
}

/* Lock specified area of surface */
DWORD APIENTRY VBoxDispDDLock(PDD_LOCKDATA lpLock)
{
    PVBOXDISPDEV pDev = (PVBOXDISPDEV) lpLock->lpDD->dhpdev;
    LOGF_ENTER();

    DD_SURFACE_LOCAL* pSurf = lpLock->lpDDSurface;

    lpLock->ddRVal = DD_OK;

    /* We only care about primary surface as we'd have to report dirty rectangles to the host in the DDUnlock*/
    if (pSurf->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        pDev->ddpsLock.bLocked = TRUE;

        if (lpLock->bHasRect)
        {
            pDev->ddpsLock.rect = lpLock->rArea;
        }
        else
        {
            pDev->ddpsLock.rect.left = 0;
            pDev->ddpsLock.rect.top = 0;
            pDev->ddpsLock.rect.right = pDev->mode.ulWidth;
            pDev->ddpsLock.rect.bottom = pDev->mode.ulHeight;
        }
    }

    LOGF_LEAVE();
    return DDHAL_DRIVER_NOTHANDLED;
}

/* Unlock previously locked surface */
DWORD APIENTRY VBoxDispDDUnlock(PDD_UNLOCKDATA lpUnlock)
{
    PVBOXDISPDEV pDev = (PVBOXDISPDEV) lpUnlock->lpDD->dhpdev;
    LOGF_ENTER();

    lpUnlock->ddRVal = DD_OK;

    if (pDev->ddpsLock.bLocked)
    {
        pDev->ddpsLock.bLocked = FALSE;

        if (pDev->hgsmi.bSupported && VBoxVBVABufferBeginUpdate(&pDev->vbvaCtx, &pDev->hgsmi.ctx))
        {
            vbvaReportDirtyRect(pDev, &pDev->ddpsLock.rect);

            if (pDev->vbvaCtx.pVBVA->hostFlags.u32HostEvents & VBOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET)
            {
                vrdpReset(pDev);
                pDev->vbvaCtx.pVBVA->hostFlags.u32HostEvents &= ~VBOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET;
            }

            if (pDev->vbvaCtx.pVBVA->hostFlags.u32HostEvents & VBVA_F_MODE_VRDP)
            {
                vrdpReportDirtyRect(pDev, &pDev->ddpsLock.rect);
            }

            VBoxVBVABufferEndUpdate(&pDev->vbvaCtx);
        }
    }

    LOGF_LEAVE();
    return DDHAL_DRIVER_NOTHANDLED;
}
