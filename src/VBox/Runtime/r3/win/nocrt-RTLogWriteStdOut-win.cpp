/* $Id$ */
/** @file
 * IPRT - Log To StdOut, Windows no-CRT.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/log.h>
#include "internal/iprt.h"

#include <iprt/win/windows.h>


RTDECL(void) RTLogWriteStdOut(const char *pch, size_t cb)
{
    /** @todo should flush the stdout stream first... */
    HANDLE hStdErr = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdErr != NULL && hStdErr != INVALID_HANDLE_VALUE)
        WriteFile(hStdErr, pch, (DWORD)cb, NULL, NULL); /** @todo do we need to translate \\n to \\r\\n? */
}
RT_EXPORT_SYMBOL(RTLogWriteStdOut);
