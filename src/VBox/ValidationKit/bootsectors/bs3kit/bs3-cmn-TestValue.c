/* $Id$ */
/** @file
 * BS3Kit - Bs3TestValue
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
#include "bs3-cmn-test.h"

#include <iprt/asm-amd64-x86.h>


#undef Bs3TestValue
BS3_CMN_DEF(void, Bs3TestValue,(const char BS3_FAR *pszName, uint64_t u64Value, uint8_t bUnit))
{
    const char * const pszUnit = g_aszBs3TestUnitNames[bUnit];
    Bs3Printf("  %-48s: %'16llu %s\n", pszName, u64Value, pszUnit);

    /*
     * Report it to the host.
     */
    if (g_fbBs3VMMDevTesting)
    {
#if ARCH_BITS == 16
        ASMOutU16(VMMDEV_TESTING_IOPORT_CMD, (uint16_t)VMMDEV_TESTING_CMD_VALUE);
        ASMOutU16(VMMDEV_TESTING_IOPORT_DATA, (uint16_t)u64Value);
        ASMOutU16(VMMDEV_TESTING_IOPORT_DATA, (uint16_t)(u64Value >> 16));
        ASMOutU16(VMMDEV_TESTING_IOPORT_DATA, (uint16_t)(u64Value >> 32));
        ASMOutU16(VMMDEV_TESTING_IOPORT_DATA, (uint16_t)(u64Value >> 48));
        ASMOutU16(VMMDEV_TESTING_IOPORT_DATA, (uint16_t)bUnit);
        ASMOutU16(VMMDEV_TESTING_IOPORT_DATA, 0);
# if 1
        ASMOutStrU8(VMMDEV_TESTING_IOPORT_DATA, pszName, Bs3StrLen(pszName) + 1);
# else
        for (;;)
        {
            uint8_t const b = *pszName++;
            ASMOutU8(VMMDEV_TESTING_IOPORT_DATA, b);
            if (!b)
                break;
        }
# endif
#else
        ASMOutU32(VMMDEV_TESTING_IOPORT_CMD, VMMDEV_TESTING_CMD_VALUE);
        ASMOutU32(VMMDEV_TESTING_IOPORT_DATA, (uint32_t)u64Value);
        ASMOutU32(VMMDEV_TESTING_IOPORT_DATA, (uint32_t)(u64Value >> 32));
        ASMOutU32(VMMDEV_TESTING_IOPORT_DATA, (uint32_t)bUnit);
        ASMOutStrU8(VMMDEV_TESTING_IOPORT_DATA, pszName, Bs3StrLen(pszName) + 1);
#endif
    }
}

