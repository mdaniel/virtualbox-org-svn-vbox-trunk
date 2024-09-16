/* $Id$ */
/** @file
 * Runtime VBox - Logger, Weak Function Pointers and related wrappers.
 */

/*
 * Copyright (C) 2022-2024 Oracle and/or its affiliates.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/log.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#if defined(IN_RING3) && (defined(IN_RT_STATIC) || defined(IPRT_NO_CRT))
RTDATADECL(PFNRTLOGLOGGEREXV)               g_pfnRTLogLoggerExV;
RTDATADECL(PFNLOGGETDEFAULTINSTANCE)        g_pfnRTLogGetDefaultInstance;
RTDATADECL(PFNLOGGETDEFAULTINSTANCEEX)      g_pfnRTLogGetDefaultInstanceEx;
#else
RTDATADECL(PFNRTLOGLOGGEREXV)               g_pfnRTLogLoggerExV             = RTLogLoggerExV;
RTDATADECL(PFNLOGGETDEFAULTINSTANCE)        g_pfnRTLogGetDefaultInstance    = RTLogGetDefaultInstance;
RTDATADECL(PFNLOGGETDEFAULTINSTANCEEX)      g_pfnRTLogGetDefaultInstanceEx  = RTLogGetDefaultInstanceEx;
#endif


#if defined(IN_RING3) && (defined(IN_RT_STATIC) || defined(IPRT_NO_CRT))

RTDECL(void) RTLogLoggerExWeak(PRTLOGGER pLogger, unsigned fFlags, unsigned iGroup, const char *pszFormat, ...)
{
    if (g_pfnRTLogLoggerExV)
    {
        va_list va;
        va_start(va, pszFormat);
        g_pfnRTLogLoggerExV(pLogger, fFlags, iGroup, pszFormat, va);
        va_end(va);
    }
}


RTDECL(void) RTLogLoggerWeak(PRTLOGGER pLogger, void *pvCallerRet, const char *pszFormat, ...)
{
    RT_NOREF(pvCallerRet);
    if (g_pfnRTLogLoggerExV)
    {
        va_list va;
        va_start(va, pszFormat);
        g_pfnRTLogLoggerExV(pLogger, 0, ~0U, pszFormat, va);
        va_end(va);
    }
}

#endif

