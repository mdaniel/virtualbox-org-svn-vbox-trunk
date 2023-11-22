/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
** File:                prlong.h
** Description: Portable access to 64 bit numerics
**
** Long-long (64-bit signed integer type) support. Some C compilers
** don't support 64 bit integers yet, so we use these macros to
** support both machines that do and don't.
**/
#ifndef prlong_h___
#define prlong_h___

#include "prtypes.h"

#include <iprt/types.h>

#ifdef VBOX_WITH_XPCOM_NAMESPACE_CLEANUP
#define LL_MaxInt VBoxNsllLL_MaxInt
#define LL_MaxUint VBoxNsllLL_MaxUint
#define LL_MinInt VBoxNsllLL_MinInt
#define LL_Zero VBoxNsllLL_Zero
#endif /* VBOX_WITH_XPCOM_NAMESPACE_CLEANUP */

PR_BEGIN_EXTERN_C

/***********************************************************************
** DEFINES:     LL_MaxInt
**              LL_MinInt
**              LL_Zero
**              LL_MaxUint
** DESCRIPTION:
**      Various interesting constants and static variable
**      initializer
***********************************************************************/
DECL_FORCE_INLINE(PRInt64) LL_MaxInt(void)
{
    return INT64_MAX;
}

DECL_FORCE_INLINE(PRInt64) LL_MinInt(void)
{
    return INT64_MIN;
}

DECL_FORCE_INLINE(PRInt64) LL_Zero(void)
{
    return 0;
}

DECL_FORCE_INLINE(PRUint64) LL_MaxUint(void)
{
    return UINT64_MAX;
}

#define LL_MAXINT   LL_MaxInt()
#define LL_MININT   LL_MinInt()
#define LL_ZERO     LL_Zero()
#define LL_MAXUINT  LL_MaxUint()

#if PR_BYTES_PER_LONG == 8
#define LL_INIT(hi, lo)  ((hi ## L << 32) + lo ## L)
#elif (defined(WIN32) || defined(WIN16)) && !defined(__GNUC__)
#define LL_INIT(hi, lo)  ((hi ## i64 << 32) + lo ## i64)
#else
#define LL_INIT(hi, lo)  ((hi ## LL << 32) + lo ## LL)
#endif

/***********************************************************************
** MACROS:      LL_*
** DESCRIPTION:
**      The following macros define portable access to the 64 bit
**      math facilities.
**
***********************************************************************/

/***********************************************************************
** MACROS:      LL_<relational operators>
**
**  LL_IS_ZERO        Test for zero
**  LL_EQ             Test for equality
**  LL_NE             Test for inequality
**  LL_GE_ZERO        Test for zero or positive
**  LL_CMP            Compare two values
***********************************************************************/
#define LL_IS_ZERO(a)       ((a) == 0)
#define LL_EQ(a, b)         ((a) == (b))
#define LL_NE(a, b)         ((a) != (b))
#define LL_GE_ZERO(a)       ((a) >= 0)
#define LL_CMP(a, op, b)    ((PRInt64)(a) op (PRInt64)(b))
#define LL_UCMP(a, op, b)   ((PRUint64)(a) op (PRUint64)(b))

/***********************************************************************
** MACROS:      LL_<logical operators>
**
**  LL_AND            Logical and
**  LL_OR             Logical or
**  LL_XOR            Logical exclusion
**  LL_OR2            A disgusting deviation
**  LL_NOT            Negation (one's complement)
***********************************************************************/
#define LL_AND(r, a, b)        ((r) = (a) & (b))
#define LL_OR(r, a, b)        ((r) = (a) | (b))
#define LL_XOR(r, a, b)        ((r) = (a) ^ (b))
#define LL_OR2(r, a)        ((r) = (r) | (a))
#define LL_NOT(r, a)        ((r) = ~(a))

/***********************************************************************
** MACROS:      LL_<mathematical operators>
**
**  LL_NEG            Negation (two's complement)
**  LL_ADD            Summation (two's complement)
**  LL_SUB            Difference (two's complement)
***********************************************************************/
#define LL_NEG(r, a)        ((r) = -(a))
#define LL_ADD(r, a, b)     ((r) = (a) + (b))
#define LL_SUB(r, a, b)     ((r) = (a) - (b))

/***********************************************************************
** MACROS:      LL_<mathematical operators>
**
**  LL_MUL            Product (two's complement)
**  LL_DIV            Quotient (two's complement)
**  LL_MOD            Modulus (two's complement)
***********************************************************************/
#define LL_MUL(r, a, b)        ((r) = (a) * (b))
#define LL_DIV(r, a, b)        ((r) = (a) / (b))
#define LL_MOD(r, a, b)        ((r) = (a) % (b))

/***********************************************************************
** MACROS:      LL_<shifting operators>
**
**  LL_SHL            Shift left [0..64] bits
**  LL_SHR            Shift right [0..64] bits with sign extension
**  LL_USHR           Unsigned shift right [0..64] bits
**  LL_ISHL           Signed shift left [0..64] bits
***********************************************************************/
#define LL_SHL(r, a, b)     ((r) = (PRInt64)(a) << (b))
#define LL_SHR(r, a, b)     ((r) = (PRInt64)(a) >> (b))
#define LL_USHR(r, a, b)    ((r) = (PRUint64)(a) >> (b))
#define LL_ISHL(r, a, b)    ((r) = (PRInt64)(a) << (b))

/***********************************************************************
** MACROS:      LL_<conversion operators>
**
**  LL_L2I            Convert to signed 32 bit
**  LL_L2UI           Convert to unsigned 32 bit
**  LL_L2F            Convert to floating point
**  LL_L2D            Convert to floating point
**  LL_I2L            Convert signed to 64 bit
**  LL_UI2L           Convert unsigned to 64 bit
**  LL_F2L            Convert float to 64 bit
**  LL_D2L            Convert float to 64 bit
***********************************************************************/
#define LL_L2I(i, l)        ((i) = (PRInt32)(l))
#define LL_L2UI(ui, l)        ((ui) = (PRUint32)(l))
#define LL_L2F(f, l)        ((f) = (PRFloat64)(l))
#define LL_L2D(d, l)        ((d) = (PRFloat64)(l))

#define LL_I2L(l, i)        ((l) = (PRInt64)(i))
#define LL_UI2L(l, ui)        ((l) = (PRInt64)(ui))
#define LL_F2L(l, f)        ((l) = (PRInt64)(f))
#define LL_D2L(l, d)        ((l) = (PRInt64)(d))

/***********************************************************************
** MACROS:      LL_UDIVMOD
** DESCRIPTION:
**  Produce both a quotient and a remainder given an unsigned 
** INPUTS:      PRUint64 a: The dividend of the operation
**              PRUint64 b: The quotient of the operation
** OUTPUTS:     PRUint64 *qp: pointer to quotient
**              PRUint64 *rp: pointer to remainder
***********************************************************************/
#define LL_UDIVMOD(qp, rp, a, b) \
    (*(qp) = ((PRUint64)(a) / (b)), \
     *(rp) = ((PRUint64)(a) % (b)))

PR_END_EXTERN_C

#endif /* prlong_h___ */
