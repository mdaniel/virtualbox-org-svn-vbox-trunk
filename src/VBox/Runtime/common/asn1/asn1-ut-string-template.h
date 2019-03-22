/* $Id$ */
/** @file
 * IPRT - ASN.1, XXX STRING Types, Collection Type Template.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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

#define RTASN1TMPL_DECL         RTDECL

#define RTASN1TMPL_TYPE         RTASN1SEQOFSTRINGS
#define RTASN1TMPL_EXT_NAME     RTAsn1SeqOfStrings
#define RTASN1TMPL_INT_NAME     rtAsn1SeqOfStrings
RTASN1TMPL_SEQ_OF(RTASN1STRING, RTAsn1String);
#undef RTASN1TMPL_TYPE
#undef RTASN1TMPL_EXT_NAME
#undef RTASN1TMPL_INT_NAME


#define RTASN1TMPL_TYPE         RTASN1SETOFSTRINGS
#define RTASN1TMPL_EXT_NAME     RTAsn1SetOfStrings
#define RTASN1TMPL_INT_NAME     rtAsn1SetOfStrings
RTASN1TMPL_SET_OF(RTASN1STRING, RTAsn1String);
#undef RTASN1TMPL_TYPE
#undef RTASN1TMPL_EXT_NAME
#undef RTASN1TMPL_INT_NAME

