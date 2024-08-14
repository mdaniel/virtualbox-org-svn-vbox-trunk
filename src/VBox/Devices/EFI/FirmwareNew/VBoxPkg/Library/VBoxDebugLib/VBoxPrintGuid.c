/* $Id$ */
/** @file
 * VBoxPrintGuid.c - Implementation of the VBoxPrintGuid() debug logging routine.
 */

/*
 * Copyright (C) 2009-2023 Oracle and/or its affiliates.
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
#include "VBoxDebugLib.h"
#include "DevEFI.h"


/**
 * Prints a EFI GUID.
 *
 * @returns Number of bytes printed.
 *
 * @param   pGuid           The GUID to print
 */
size_t VBoxPrintGuid(CONST EFI_GUID *pGuid)
{
    VBoxPrintHex(pGuid->Data1, sizeof(pGuid->Data1));
    VBoxPrintChar('-');
    VBoxPrintHex(pGuid->Data2, sizeof(pGuid->Data2));
    VBoxPrintChar('-');
    VBoxPrintHex(pGuid->Data3, sizeof(pGuid->Data3));
    VBoxPrintChar('-');
    VBoxPrintHex(pGuid->Data4[0], sizeof(pGuid->Data4[0]));
    VBoxPrintHex(pGuid->Data4[1], sizeof(pGuid->Data4[1]));
    VBoxPrintChar('-');
    VBoxPrintHex(pGuid->Data4[2], sizeof(pGuid->Data4[2]));
    VBoxPrintHex(pGuid->Data4[3], sizeof(pGuid->Data4[3]));
    VBoxPrintHex(pGuid->Data4[4], sizeof(pGuid->Data4[4]));
    VBoxPrintHex(pGuid->Data4[5], sizeof(pGuid->Data4[5]));
    VBoxPrintHex(pGuid->Data4[6], sizeof(pGuid->Data4[6]));
    VBoxPrintHex(pGuid->Data4[7], sizeof(pGuid->Data4[7]));
    return 37;
}

