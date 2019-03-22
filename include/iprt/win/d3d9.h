/** @file
 * Safe way to include d3d9.h.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
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


#ifndef ___iprt_win_d3d9_h___
#define ___iprt_win_d3d9_h___

#ifdef _MSC_VER
# pragma warning(push)
/*# pragma warning(disable:4163)*/
# pragma warning(disable:4668) /* warning C4668: 'WHEA_DOWNLEVEL_TYPE_NAMES' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif' */
# pragma warning(disable:4255) /* warning C4255: 'ObGetFilterVersion' : no function prototype given: converting '()' to '(void)' */
# if _MSC_VER >= 1800 /*RT_MSC_VER_VC120*/
#  pragma warning(disable:4005) /* sdk/v7.1/include/sal_supp.h(57) : warning C4005: '__useHeader' : macro redefinition */
# endif
#endif

#include <d3d9.h>

#ifdef _MSC_VER
# pragma warning(pop)
#endif

#endif

