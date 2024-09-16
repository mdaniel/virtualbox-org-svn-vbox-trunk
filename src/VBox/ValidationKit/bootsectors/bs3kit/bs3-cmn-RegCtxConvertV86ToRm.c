/* $Id$ */
/** @file
 * BS3Kit - Bs3RegCtxConvertV86ToRm
 */

/*
 * Copyright (C) 2007-2024 Oracle and/or its affiliates.
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


#undef Bs3RegCtxConvertV86ToRm
BS3_CMN_DEF(void, Bs3RegCtxConvertV86ToRm,(PBS3REGCTX pRegCtx))
{
    BS3_ASSERT(BS3_MODE_IS_V86(pRegCtx->bMode));

    pRegCtx->cr0.u32    &= ~(X86_CR0_PE | X86_CR0_PG);
    pRegCtx->rflags.u32 &= ~X86_EFL_VM;
    pRegCtx->fbFlags    |= BS3REG_CTX_F_NO_TR_LDTR | BS3REG_CTX_F_NO_AMD64;
    pRegCtx->bCpl        = 0;
    pRegCtx->bMode       = BS3_MODE_RM;
}

