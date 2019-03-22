/** @file
 * MS COM / XPCOM Abstraction Layer - Assertion macros for COM/XPCOM.
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

#ifndef ___VBox_com_assert_h
#define ___VBox_com_assert_h

#include <iprt/assert.h>

/** @defgroup grp_com_assert    Assertion Macros for COM/XPCOM
 * @ingroup grp_com
 * @{
 */


/**
 *  Asserts that the COM result code is succeeded in strict builds.
 *  In non-strict builds the result code will be NOREF'ed to kill compiler warnings.
 *
 *  @param hrc      The COM result code
 */
#define AssertComRC(hrc) \
    do { AssertMsg(SUCCEEDED(hrc), ("COM RC = %Rhrc (0x%08X)\n", hrc, hrc)); NOREF(hrc); } while (0)

/**
 *  Same as AssertComRC, except the caller already knows we failed.
 *
 *  @param hrc      The COM result code
 */
#define AssertComRCFailed(hrc) \
    do { AssertMsgFailed(("COM RC = %Rhrc (0x%08X)\n", hrc, hrc)); NOREF(hrc); } while (0)

/**
 *  A special version of AssertComRC that returns the given expression
 *  if the result code is failed.
 *
 *  @param hrc      The COM result code
 *  @param RetExpr  The expression to return
 */
#define AssertComRCReturn(hrc, RetExpr) \
    AssertMsgReturn(SUCCEEDED(hrc), ("COM RC = %Rhrc (0x%08X)\n", hrc, hrc), RetExpr)

/**
 *  A special version of AssertComRC that returns the given result code
 *  if it is failed.
 *
 *  @param hrc      The COM result code
 */
#define AssertComRCReturnRC(hrc) \
    AssertMsgReturn(SUCCEEDED(hrc), ("COM RC = %Rhrc (0x%08X)\n", hrc, hrc), hrc)

/**
 *  A special version of AssertComRC that returns if the result code is failed.
 *
 *  @param hrc      The COM result code
 */
#define AssertComRCReturnVoid(hrc) \
    AssertMsgReturnVoid(SUCCEEDED(hrc), ("COM RC = %Rhrc (0x%08X)\n", hrc, hrc))

/**
 *  A special version of AssertComRC that evaluates the given expression and
 *  breaks if the result code is failed.
 *
 *  @param hrc          The COM result code
 *  @param PreBreakExpr The expression to evaluate on failure.
 */
#define AssertComRCBreak(hrc, PreBreakExpr) \
    if (!SUCCEEDED(hrc)) { AssertComRCFailed(hrc); PreBreakExpr; break; } else do {} while (0)

/**
 *  A special version of AssertComRC that evaluates the given expression and
 *  throws it if the result code is failed.
 *
 *  @param hrc          The COM result code
 *  @param ThrowMeExpr  The expression which result to be thrown on failure.
 */
#define AssertComRCThrow(hrc, ThrowMeExpr) \
    do { if (SUCCEEDED(hrc)) { /*likely*/} else { AssertComRCFailed(hrc); throw (ThrowMeExpr); } } while (0)

/**
 *  A special version of AssertComRC that just breaks if the result code is
 *  failed.
 *
 *  @param hrc      The COM result code
 */
#define AssertComRCBreakRC(hrc) \
    if (!SUCCEEDED(hrc)) { AssertComRCFailed(hrc); break; } else do {} while (0)

/**
 *  A special version of AssertComRC that just throws @a hrc if the result code
 *  is failed.
 *
 *  @param hrc      The COM result code
 */
#define AssertComRCThrowRC(hrc) \
    do { if (SUCCEEDED(hrc)) { /*likely*/ } else { AssertComRCFailed(hrc); throw hrc; } } while (0)

/** @} */

#endif // !___VBox_com_assert_h

