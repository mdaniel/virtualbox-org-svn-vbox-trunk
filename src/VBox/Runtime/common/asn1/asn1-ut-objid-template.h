/* $Id$ */
/** @file
 * IPRT - ASN.1, OBJECT IDENTIFIER Type, Collection Type Template.
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

#define RTASN1TMPL_TYPE         RTASN1SEQOFOBJIDS
#define RTASN1TMPL_EXT_NAME     RTAsn1SeqOfObjIds
#define RTASN1TMPL_INT_NAME     rtAsn1SeqOfObjIds
RTASN1TMPL_SEQ_OF(RTASN1OBJID, RTAsn1ObjId);
#undef RTASN1TMPL_TYPE
#undef RTASN1TMPL_EXT_NAME
#undef RTASN1TMPL_INT_NAME


#define RTASN1TMPL_TYPE         RTASN1SETOFOBJIDS
#define RTASN1TMPL_EXT_NAME     RTAsn1SetOfObjIds
#define RTASN1TMPL_INT_NAME     rtAsn1SetOfObjIds
RTASN1TMPL_SET_OF(RTASN1OBJID, RTAsn1ObjId);
#undef RTASN1TMPL_TYPE
#undef RTASN1TMPL_EXT_NAME
#undef RTASN1TMPL_INT_NAME


#define RTASN1TMPL_TYPE         RTASN1SETOFOBJIDSEQS
#define RTASN1TMPL_EXT_NAME     RTAsn1SetOfObjIdSeqs
#define RTASN1TMPL_INT_NAME     rtAsn1SetOfObjIdSeqs
RTASN1TMPL_SET_OF(RTASN1SEQOFOBJIDS, RTAsn1SeqOfObjIds);
#undef RTASN1TMPL_TYPE
#undef RTASN1TMPL_EXT_NAME
#undef RTASN1TMPL_INT_NAME

