/* $Id$ */
/** @file
 * BS3Kit - Bs3TrapSetJmpAndRestoreWithExtCtxAndRm
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


#undef Bs3TrapSetJmpAndRestoreWithExtCtxAndRm
BS3_CMN_DEF(void, Bs3TrapSetJmpAndRestoreWithExtCtxAndRm,(PCBS3REGCTX pCtxRestore, PCBS3EXTCTX pExtCtxRestore,
                                                          PBS3TRAPFRAME pTrapFrame, PBS3EXTCTX pExtCtxTrap))
{
    if (   pCtxRestore->bMode != BS3_MODE_RM
#if ARCH_BITS == 16
        || g_bBs3CurrentMode == BS3_MODE_RM
#endif
       )
    {
        BS3_ASSERT((pCtxRestore->bMode & BS3_MODE_SYS_MASK) == (g_bBs3CurrentMode & BS3_MODE_SYS_MASK));
        Bs3TrapSetJmpAndRestoreWithExtCtx(pCtxRestore, pExtCtxRestore, pTrapFrame, pExtCtxTrap);
    }
    else
    {
        /* ASSUMES compile emits no SSE instructions between the calls here
           (only a potential issue in 64-bit mode). */
        Bs3ExtCtxRestoreEx(pExtCtxRestore);
        Bs3TrapSetJmpAndRestoreInRm(pCtxRestore, pTrapFrame);
        Bs3ExtCtxSaveEx(pExtCtxTrap);
    }
}

