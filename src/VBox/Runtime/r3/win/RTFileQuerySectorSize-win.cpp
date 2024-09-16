/* $Id$ */
/** @file
 * IPRT - RTFileQuerySectorSize, Windows.
 */

/*
 * Copyright (C) 2017-2024 Oracle and/or its affiliates.
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
#include <iprt/file.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>

#include <iprt/win/windows.h>



RTDECL(int) RTFileQuerySectorSize(RTFILE hFile, uint32_t *pcbSector)
{
    AssertPtrReturn(pcbSector, VERR_INVALID_PARAMETER);

    DISK_GEOMETRY DriveGeo;
    RT_ZERO(DriveGeo);
    DWORD cbDriveGeo = 0;
    if (DeviceIoControl((HANDLE)RTFileToNative(hFile),
                        IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0,
                        &DriveGeo, sizeof(DriveGeo), &cbDriveGeo, NULL))
    {
        AssertReturn(DriveGeo.BytesPerSector > 0, VERR_INVALID_PARAMETER);
        *pcbSector = DriveGeo.BytesPerSector;
        return VINF_SUCCESS;
    }
    int rc = RTErrConvertFromWin32(GetLastError());
    AssertMsg(rc == VERR_IO_NOT_READY, ("%d / %Rrc\n", GetLastError(), rc));
    return rc;
}

