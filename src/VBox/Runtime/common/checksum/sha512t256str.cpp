/* $Id$ */
/** @file
 * IPRT - SHA-512/256 string functions.
 */

/*
 * Copyright (C) 2009-2024 Oracle and/or its affiliates.
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
#include <iprt/sha.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/string.h>


RTDECL(int) RTSha512t256ToString(uint8_t const pabDigest[RTSHA512T256_HASH_SIZE], char *pszDigest, size_t cchDigest)
{
    return RTStrPrintHexBytes(pszDigest, cchDigest, &pabDigest[0], RTSHA512T256_HASH_SIZE, 0 /*fFlags*/);
}


RTDECL(int) RTSha512t256FromString(char const *pszDigest, uint8_t pabDigest[RTSHA512T256_HASH_SIZE])
{
    return RTStrConvertHexBytes(RTStrStripL(pszDigest), &pabDigest[0], RTSHA512T256_HASH_SIZE, 0 /*fFlags*/);
}

