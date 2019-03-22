/* $Id$ */
/** @file
 * BS3Kit - Bs3TrapSetDpl
 */

/*
 * Copyright (C) 2007-2018 Oracle Corporation
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
#include "bs3kit-template-header.h"


#undef Bs3TrapSetDpl
BS3_CMN_DEF(uint8_t, Bs3TrapSetDpl,(uint8_t iIdt, uint8_t bDpl))
{
    uint8_t bRet;
    BS3_ASSERT(bDpl <= 3);

    Bs3Idt16[iIdt].Gate.u2Dpl = bDpl;
    Bs3Idt32[iIdt].Gate.u2Dpl = bDpl;
    bRet = Bs3Idt64[iIdt].Gate.u2Dpl;
    Bs3Idt64[iIdt].Gate.u2Dpl = bDpl;

    return bRet;
}

