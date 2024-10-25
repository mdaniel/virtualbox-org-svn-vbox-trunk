/* $Id$ */
/** @file
 * VirtualBox Windows Guest Shared Folders - Ugly hack for missing CopySup.lib on ARM64.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/nt/nt.h> /* includes ntifs.h + wdm.h */


extern "C" BOOLEAN NTAPI
FsRtlCopyRead2(PFILE_OBJECT pFileObject, PLARGE_INTEGER pFileOffset, ULONG cbLength, BOOLEAN fWait,
               ULONG uLockKey, void *pvBuffer, PIO_STATUS_BLOCK pIoStatus, PDEVICE_OBJECT pDeviceObject, void *pvTopLevelContext)
{
/** @todo win.arm64: either implement FsRtlCopyRead2 ourselves or find a
 * library in the WDK the supplies the function.  Note that rdbss.sys does not
 * include this code any more, nor does the x86 or amd64 WDK since Windows 7.
 *
 * Alternative reimplement shared folders from scratch without rdbsslib.lib. */

    RT_NOREF(pFileObject, pFileOffset, cbLength, fWait, uLockKey, pvBuffer, pIoStatus, pDeviceObject, pvTopLevelContext);
    /* Return false and hope the caller will take the slow IRQL-based code path. */
    return FALSE;
}


extern "C" BOOLEAN NTAPI
FsRtlCopyWrite2(PFILE_OBJECT pFileObject, PLARGE_INTEGER pFileOffset, ULONG cbLength, BOOLEAN fWait,
                ULONG uLockKey, void *pvBuffer, PIO_STATUS_BLOCK pIoStatus, PDEVICE_OBJECT pDeviceObject, void *pvTopLevelContext)
{
/** @todo win.arm64: either implement FsRtlCopyWrite2 ourselves or find a
 * library in the WDK the supplies the function.  Note that rdbss.sys does not
 * include this code any more, nor does the x86 or amd64 WDK since Windows 7.
 *
 * Alternative reimplement shared folders from scratch without rdbsslib.lib. */

    RT_NOREF(pFileObject, pFileOffset, cbLength, fWait, uLockKey, pvBuffer, pIoStatus, pDeviceObject, pvTopLevelContext);
    /* Return false and hope the caller will take the slow IRQL-based code path. */
    return FALSE;
}

