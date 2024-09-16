/* $Id$ */
/** @file
 * IPRT - RTFileQuerySectorSize, Linux implementation.
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
#include <iprt/errcore.h>

#include <errno.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <sys/ioctl.h>


RTDECL(int) RTFileQuerySectorSize(RTFILE hFile, uint32_t *pcbSector)
{
    AssertPtrReturn(pcbSector, VERR_INVALID_PARAMETER);

    int rc;
    int const fd = (int)RTFileToNative(hFile);
    struct stat DevStat = { 0 };
    if (!fstat(fd, &DevStat))
    {
        if (S_ISBLK(DevStat.st_mode))
        {
            int cbLogicalBlock = 0;
            if (!ioctl(fd, BLKSSZGET, &cbLogicalBlock))
            {
                AssertReturn(cbLogicalBlock > 0, VERR_INVALID_FUNCTION);
                *pcbSector = cbLogicalBlock;
                return VINF_SUCCESS;
            }

            rc = RTErrConvertFromErrno(errno);
            AssertMsgFailed(("ioctl failed: errno=%d / %Rrc\n", errno, rc));
        }
        else
        {
            AssertMsgFailed(("not a block device.\n"));
            rc = VERR_INVALID_FUNCTION;
        }
    }
    else
    {
        rc = RTErrConvertFromErrno(errno);
        AssertMsgFailed(("fstat failed: errno=%d / %Rrc\n", errno, rc));
    }
    return rc;
}

