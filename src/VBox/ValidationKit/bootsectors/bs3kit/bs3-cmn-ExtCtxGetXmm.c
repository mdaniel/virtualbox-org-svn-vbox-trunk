/* $Id$ */
/** @file
 * BS3Kit - Bs3ExtCtxGetXmm
 */

/*
 * Copyright (C) 2007-2023 Oracle and/or its affiliates.
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
#include "bs3kit-template-header.h"


#undef Bs3ExtCtxGetXmm
BS3_CMN_DEF(PRTUINT128U, Bs3ExtCtxGetXmm,(PCBS3EXTCTX pExtCtx, uint8_t iReg, PRTUINT128U pValue))
{
#ifdef _MSC_VER /* No-SSE hack */
    RTUINT128U volatile RT_FAR *pValueNoSseHack = (RTUINT128U volatile RT_FAR *)pValue;
# define pValue pValueNoSseHack
#endif
    AssertCompileMembersAtSameOffset(BS3EXTCTX, Ctx.x87.aXMM, BS3EXTCTX, Ctx.x.x87.aXMM);
    switch (pExtCtx->enmMethod)
    {
        case BS3EXTCTXMETHOD_FXSAVE:
        case BS3EXTCTXMETHOD_XSAVE:
            if (iReg < RT_ELEMENTS(pExtCtx->Ctx.x87.aXMM))
            {
                pValue->au64[0] = pExtCtx->Ctx.x87.aXMM[iReg].au64[0];
                pValue->au64[1] = pExtCtx->Ctx.x87.aXMM[iReg].au64[1];
                return pValue;
            }
            break;
    }

    pValue->au64[0] = 0;
    pValue->au64[1] = 0;
    return (PRTUINT128U)pValue;
}

