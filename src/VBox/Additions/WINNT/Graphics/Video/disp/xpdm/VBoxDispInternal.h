/* $Id$ */
/** @file
 * VBox XPDM Display driver, internal header
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

#ifndef VBOXDISPINTERNAL_H
#define VBOXDISPINTERNAL_H
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#define LOG_GROUP LOG_GROUP_DRV_DISPLAY
#include <VBox/log.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/win/windef.h>
#include <wingdi.h>
#include <winddi.h>
#include <ntddvdeo.h>
#undef CO_E_NOTINITIALIZED
#include <winerror.h>
#include <devioctl.h>
#define VBOX_VIDEO_LOG_NAME "VBoxDisp"
#include "common/VBoxVideoLog.h"
#include "common/xpdm/VBoxVideoPortAPI.h"
#include "common/xpdm/VBoxVideoIOCTL.h"
#include <HGSMI.h>
#include <VBoxVideo.h>
#include <VBoxVideoGuest.h>
#include <VBoxDisplay.h>

typedef struct _VBOXDISPDEV *PVBOXDISPDEV;

#ifdef VBOX_WITH_VIDEOHWACCEL
# include "VBoxDispVHWA.h"
#endif

/* 4bytes tag passed to EngAllocMem.
 * Note: chars are reverse order.
 */
#define MEM_ALLOC_TAG 'bvDD'

/* Helper macros */
#define VBOX_WARN_WINERR(_winerr)                     \
    do {                                              \
        if ((_winerr) != NO_ERROR)                    \
        {                                             \
            WARN(("winerr(%#x)!=NO_ERROR", _winerr)); \
        }                                             \
    } while (0)

#define VBOX_CHECK_WINERR_RETRC(_winerr, _rc)         \
    do {                                              \
        if ((_winerr) != NO_ERROR)                    \
        {                                             \
            WARN(("winerr(%#x)!=NO_ERROR", _winerr)); \
            return (_rc);                             \
        }                                             \
    } while (0)

#define VBOX_WARNRC_RETV(_rc, _ret)            \
    do {                                       \
        if (RT_FAILURE(_rc))                   \
        {                                      \
            WARN(("RT_FAILURE rc(%#x)", _rc)); \
            return (_ret);                     \
        }                                      \
    } while (0)

#define VBOX_WARNRC_RETRC(_rc) VBOX_WARNRC_RETV(_rc, _rc)

#define VBOX_WARNRC(_rc)                       \
    do {                                       \
        if (RT_FAILURE(_rc))                   \
        {                                      \
            WARN(("RT_FAILURE rc(%#x)", _rc)); \
        }                                      \
    } while (0)

#define VBOX_WARNRC_NOBP(_rc)                       \
    do {                                       \
        if (RT_FAILURE(_rc))                   \
        {                                      \
            WARN_NOBP(("RT_FAILURE rc(%#x)", _rc)); \
        }                                      \
    } while (0)


#define VBOX_WARN_IOCTLCB_RETRC(_ioctl, _cbreturned, _cbexpected, _rc)                   \
    do {                                                                                 \
        if ((_cbreturned)!=(_cbexpected))                                                \
        {                                                                                \
            WARN((_ioctl " returned %d, expected %d bytes!", _cbreturned, _cbexpected)); \
            return (_rc);                                                                \
        }                                                                                \
    } while (0)

#define abs(_v) ( ((_v)>0) ? (_v) : (-(_v)) )

typedef struct _CLIPRECTS {
    ULONG  c;
    RECTL  arcl[64];
} CLIPRECTS;

typedef struct _VRDPCLIPRECTS
{
    RECTL rclDstOrig; /* Original bounding rectangle. */
    RECTL rclDst;     /* Bounding rectangle of all rects. */
    CLIPRECTS rects;  /* Rectangles to update. */
} VRDPCLIPRECTS;

/* Mouse pointer related functions */
int VBoxDispInitPointerCaps(PVBOXDISPDEV pDev, DEVINFO *pDevInfo);
int VBoxDispInitPointerAttrs(PVBOXDISPDEV pDev);

/* Palette related functions */
int VBoxDispInitPalette(PVBOXDISPDEV pDev, DEVINFO *pDevInfo);
void VBoxDispDestroyPalette(PVBOXDISPDEV pDev);
int VBoxDispSetPalette8BPP(PVBOXDISPDEV pDev);

/* VBVA related */
int VBoxDispVBVAInit(PVBOXDISPDEV pDev);
void VBoxDispVBVAHostCommandComplete(PVBOXDISPDEV pDev, VBVAHOSTCMD RT_UNTRUSTED_VOLATILE_HOST *pCmd);

void vrdpReportDirtyRect(PVBOXDISPDEV pDev, RECTL *prcl);
void vbvaReportDirtyRect(PVBOXDISPDEV pDev, RECTL *prcl);

#ifdef VBOX_VBVA_ADJUST_RECT
void vrdpAdjustRect (SURFOBJ *pso, RECTL *prcl);
BOOL vbvaFindChangedRect(SURFOBJ *psoDest, SURFOBJ *psoSrc, RECTL *prclDest, POINTL *pptlSrc);
#endif /* VBOX_VBVA_ADJUST_RECT */

#define VRDP_TEXT_MAX_GLYPH_SIZE 0x100
#define VRDP_TEXT_MAX_GLYPHS     0xfe
BOOL vrdpReportText(PVBOXDISPDEV pDev, VRDPCLIPRECTS *pClipRects, STROBJ *pstro, FONTOBJ *pfo,
                    RECTL *prclOpaque, ULONG ulForeRGB, ULONG ulBackRGB);

BOOL vrdpReportOrderGeneric(PVBOXDISPDEV pDev, const VRDPCLIPRECTS *pClipRects,
                             const void *pvOrder, unsigned cbOrder, unsigned code);

BOOL VBoxDispIsScreenSurface(SURFOBJ *pso);
void VBoxDispDumpPSO(SURFOBJ *pso, char *s);

BOOL vrdpDrvRealizeBrush(BRUSHOBJ *pbo, SURFOBJ *psoTarget, SURFOBJ *psoPattern, SURFOBJ *psoMask,
                         XLATEOBJ *pxlo, ULONG iHatch);
void vrdpReset(PVBOXDISPDEV pDev);

DECLINLINE(int) format2BytesPerPixel(const SURFOBJ *pso)
{
    switch (pso->iBitmapFormat)
    {
        case BMF_16BPP: return 2;
        case BMF_24BPP: return 3;
        case BMF_32BPP: return 4;
    }

    return 0;
}

#endif /*VBOXDISPINTERNAL_H*/
