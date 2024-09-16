/* $Id$ */
/** @file
 * BS3Kit - Bs3RegCtxSaveForMode
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


#undef Bs3RegCtxSaveForMode
BS3_CMN_DEF(void, Bs3RegCtxSaveForMode,(PBS3REGCTX pRegCtx, uint8_t bMode, uint16_t cbExtraStack))
{
    if (   bMode != BS3_MODE_RM
#if ARCH_BIT == 16
        || g_bBs3CurrentMode == BS3_MODE_RM
#endif
       )
    {
        BS3_ASSERT((bMode & BS3_MODE_SYS_MASK) == (g_bBs3CurrentMode & BS3_MODE_SYS_MASK));
        Bs3RegCtxSaveEx(pRegCtx, bMode, cbExtraStack);
    }
    else
    {
#if ARCH_BIT == 64
        BS3_ASSERT(0); /* No V86 mode in LM! */
#endif
        Bs3RegCtxSaveEx(pRegCtx, (bMode & ~BS3_MODE_CODE_MASK) | BS3_MODE_CODE_V86, cbExtraStack);
        Bs3RegCtxConvertV86ToRm(pRegCtx);
    }
}

