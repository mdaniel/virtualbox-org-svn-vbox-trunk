/* $Id$ */
/** @file
 * IPRT - No-CRT - copysignl().
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
#define IPRT_NO_CRT_FOR_3RD_PARTY
#include "internal/nocrt.h"
#include <iprt/nocrt/math.h>
#include <iprt/assertcompile.h>


#undef copysignl
long double RT_NOCRT(copysignl)(long double lrdMagnitude, long double lrdSign)
{
#ifdef RT_COMPILER_WITH_80BIT_LONG_DOUBLE
    RTFLOAT80U2 uRet, uSign;
    uSign.lrd    = lrdSign;
    uRet.lrd     = lrdMagnitude;
    uRet.s.fSign = uSign.s.fSign;
    return uRet.lrd;
#else
    AssertCompile(sizeof(lrdMagnitude) == sizeof(RTFLOAT64U));
    RTFLOAT64U uRet, uSign;
    uSign.rd     = lrdSign;
    uRet.rd      = lrdMagnitude;
    uRet.s.fSign = uSign.s.fSign;
    return uRet.rd;
#endif
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(copysignl);

