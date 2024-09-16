/* $Id$ */
/** @file
 * BS3Kit - PIC Setup.
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
#include <iprt/asm-amd64-x86.h>
#include "bs3-cmn-pic.h"



/**
 * Configures the PIC, once only.
 *
 * Subsequent calls to this function will not do anything.
 *
 * The PIC will be programmed to use IDT/IVT vectors 0x70 thru 0x7f, auto
 * end-of-interrupt, and all IRQs masked.  The individual PIC users will have to
 * use #Bs3PicUpdateMask unmask their IRQ once they've got all the handlers
 * installed.
 */
#undef Bs3PicSetup
BS3_CMN_DEF(void, Bs3PicSetup,(bool fForcedReInit))
{
    /*
     * The first call configures the PIC to send interrupts to vectors 0x70 thru 0x7f,
     * masking all of them.  Things producing IRQs is responsible for configure their
     * handlers and then(!) use Bs3PicUpdateMask to unmask the IRQ.
     */
    if (!g_fBs3PicConfigured || fForcedReInit)
    {
        g_fBs3PicConfigured = true;

        /* Start init. */
        ASMOutU8(BS3_PIC_PORT_MASTER, BS3_PIC_CMD_INIT | BS3_PIC_CMD_INIT_F_4STEP);
        ASMOutU8(BS3_PIC_PORT_SLAVE,  BS3_PIC_CMD_INIT | BS3_PIC_CMD_INIT_F_4STEP);

        /* Set IRQ base. */
        ASMOutU8(BS3_PIC_PORT_MASTER + 1, 0x70);
        ASMOutU8(BS3_PIC_PORT_SLAVE  + 1, 0x78);

        /* Dunno. */
        ASMOutU8(BS3_PIC_PORT_MASTER + 1, 4);
        ASMOutU8(BS3_PIC_PORT_SLAVE  + 1, 2);

        /* Set IRQ base. */
        ASMOutU8(BS3_PIC_PORT_MASTER + 1, BS3_PIC_I4_F_AUTO_EOI);
        ASMOutU8(BS3_PIC_PORT_SLAVE  + 1, BS3_PIC_I4_F_AUTO_EOI);

        /* Mask everything. */
        ASMOutU8(BS3_PIC_PORT_MASTER + 1, UINT8_MAX);
        ASMOutU8(BS3_PIC_PORT_SLAVE  + 1, UINT8_MAX);
    }
}

