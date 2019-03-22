/* $Id$ */
/** @file
 * VBox Miniport common utils header
 */

/*
 * Copyright (C) 2011-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOXMPUTILS_H
#define VBOXMPUTILS_H
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/*Sanity check*/
#if defined(VBOX_XPDM_MINIPORT) == defined(VBOX_WDDM_MINIPORT)
# error One of the VBOX_XPDM_MINIPORT or VBOX_WDDM_MINIPORT should be defined!
#endif

#include <iprt/cdefs.h>
#define LOG_GROUP LOG_GROUP_DRV_MINIPORT
#include <VBox/log.h>

#define VBOX_VIDEO_LOG_NAME "VBoxMP"
#ifdef VBOX_WDDM_MINIPORT
# ifndef VBOX_WDDM_MINIPORT_WITH_FLOW_LOGGING
#  define VBOX_VIDEO_LOGFLOW_LOGGER(_m) do {} while (0)
# endif
#endif
#include "common/VBoxVideoLog.h"

#include <iprt/err.h>
#include <iprt/assert.h>

#ifdef VBOX_XPDM_MINIPORT
# include <dderror.h>
# include <devioctl.h>
#else
# undef PAGE_SIZE
# undef PAGE_SHIFT
# include <iprt/nt/ntddk.h>
# include <iprt/nt/dispmprt.h>
# include <ntddvdeo.h>
# include <dderror.h>
#endif



/*Windows version identifier*/
typedef enum
{
    WINVERSION_UNKNOWN = 0,
    WINVERSION_NT4     = 1,
    WINVERSION_2K      = 2,
    WINVERSION_XP      = 3,
    WINVERSION_VISTA   = 4,
    WINVERSION_7       = 5,
    WINVERSION_8       = 6,
    WINVERSION_81      = 7,
    WINVERSION_10      = 8
} vboxWinVersion_t;

RT_C_DECLS_BEGIN
vboxWinVersion_t VBoxQueryWinVersion(uint32_t *pbuild);
uint32_t VBoxGetHeightReduction(void);
bool     VBoxLikesVideoMode(uint32_t display, uint32_t width, uint32_t height, uint32_t bpp);
bool     VBoxQueryDisplayRequest(uint32_t *xres, uint32_t *yres, uint32_t *bpp, uint32_t *pDisplayId);
bool     VBoxQueryHostWantsAbsolute(void);
bool     VBoxQueryPointerPos(uint16_t *pPosX, uint16_t *pPosY);
RT_C_DECLS_END


#define VBE_DISPI_TOTAL_VIDEO_MEMORY_BYTES (4*_1M)

#define VBOXMP_WARN_VPS_NOBP(_vps) \
    if ((_vps) != NO_ERROR) \
    { \
        WARN_NOBP(("vps(%#x)!=NO_ERROR", _vps)); \
    } else do { } while (0)

#define VBOXMP_WARN_VPS(_vps) \
    if ((_vps) != NO_ERROR) \
    { \
        WARN(("vps(%#x)!=NO_ERROR", _vps)); \
    } else do { } while (0)


#define VBOXMP_CHECK_VPS_BREAK(_vps) \
    if ((_vps) != NO_ERROR) \
    { \
        break; \
    } else do { } while (0)


#ifdef DEBUG_misha
 /* specifies whether the vboxVDbgBreakF should break in the debugger
  * windbg seems to have some issues when there is a lot ( >~50) of sw breakpoints defined
  * to simplify things we just insert breaks for the case of intensive debugging WDDM driver*/
extern int g_bVBoxVDbgBreakF;
extern int g_bVBoxVDbgBreakFv;
# define vboxVDbgBreakF() do { if (g_bVBoxVDbgBreakF) AssertBreakpoint(); } while (0)
# define vboxVDbgBreakFv() do { if (g_bVBoxVDbgBreakFv) AssertBreakpoint(); } while (0)
#else
# define vboxVDbgBreakF() do { } while (0)
# define vboxVDbgBreakFv() do { } while (0)
#endif

#endif

