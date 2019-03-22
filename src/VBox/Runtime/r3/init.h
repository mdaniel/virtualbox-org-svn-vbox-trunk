/* $Id$ */
/** @file
 * IPRT - Ring-3 initialization.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


#ifndef ___r3_init_h
#define ___r3_init_h

#include <iprt/types.h>

DECLHIDDEN(int)  rtR3InitNativeFirst(uint32_t fFlags);
DECLHIDDEN(int)  rtR3InitNativeFinal(uint32_t fFlags);
DECLHIDDEN(void) rtR3InitNativeObtrusive(uint32_t fFlags);

#endif

