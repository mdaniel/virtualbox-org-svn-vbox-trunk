/*  $NetBSD: limits.h,v 1.2 2006/05/14 21:55:38 elad Exp $  */

/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  @(#)limits.h  7.2 (Berkeley) 6/28/90
 */

#ifndef _MACHINE_LIMITS_H_
#define _MACHINE_LIMITS_H_

#define __CHAR_BIT  8   /* number of bits in a char */
//#define MB_LEN_MAX  32    /* no multibyte characters */

#define __SCHAR_MIN (-128) /* max value for a signed char */
#define __SCHAR_MAX 127    /* min value for a signed char */

#define __UCHAR_MAX 255   /* max value for an unsigned char */
//#define CHAR_MAX  0x7f    /* max value for a char */
//#define CHAR_MIN  (-0x7f-1) /* min value for a char */

#define __USHRT_MAX 0xffffU   /* max value for an unsigned short */
#define __SHRT_MAX  0x7fff    /* max value for a short */
#define __SHRT_MIN  (-0x7fff-1) /* min value for a short */

#define __UINT_MAX  0xffffffffU /* max value for an unsigned int */
#define __INT_MAX   0x7fffffff  /* max value for an int */
#define __INT_MIN         (-0x7fffffff-1) /* min value for an int */

//#define __ULONG_MAX 0xffffffffffffffffUL  /* max value for an unsigned long */
//#define __LONG_MAX  0x7fffffffffffffffL /* max value for a long */
//#define __LONG_MIN  (-0x7fffffffffffffffL-1)  /* min value for a long */
#define __ULONG_MAX __UINT_MAX  /* max value for an unsigned long */
#define __LONG_MAX  __INT_MAX /* max value for a long */
#define __LONG_MIN  __INT_MIN  /* min value for a long */


#define SSIZE_MAX LONG_MAX  /* max value for a ssize_t */

#define __ULLONG_MAX  0xffffffffffffffffULL /* max unsigned long long */
#define __LLONG_MAX 0x7fffffffffffffffLL  /* max signed long long */
#define __LLONG_MIN (-0x7fffffffffffffffLL-1) /* min signed long long */

#define SIZE_T_MAX  __ULLONG_MAX /* max value for a size_t */

/* GCC requires that quad constants be written as expressions. */
#define UQUAD_MAX ((u_quad_t)0-1) /* max value for a uquad_t */
          /* max value for a quad_t */
#define QUAD_MAX  ((quad_t)(UQUAD_MAX >> 1))
#define QUAD_MIN  (-QUAD_MAX-1) /* min value for a quad_t */


#define LONG_BIT  32
#define WORD_BIT  32

/* Intel extensions to <limits.h> for UEFI */
#define __SHORT_BIT     16
#define __WCHAR_BIT     16
#define __INT_BIT       32
#define __LONG_BIT      32    /* Compiler dependent */
#define __LONG_LONG_BIT 64
#define __POINTER_BIT   64

#endif /* _MACHINE_LIMITS_H_ */
