/* $Id$ */
/** @file
 * IPRT - Crypto - RSA, Code Generator Template.
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

#define RTASN1TMPL_DECL         RTDECL

/*
 * RSA public key.
 */
#define RTASN1TMPL_TYPE         RTCRRSAPUBLICKEY
#define RTASN1TMPL_EXT_NAME     RTCrRsaPublicKey
#define RTASN1TMPL_INT_NAME     rtCrRsaPublicKey
RTASN1TMPL_BEGIN_SEQCORE();
RTASN1TMPL_MEMBER(              Modulus,            RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_MEMBER(              PublicExponent,     RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_END_SEQCORE();
#undef RTASN1TMPL_TYPE
#undef RTASN1TMPL_EXT_NAME
#undef RTASN1TMPL_INT_NAME


/*
 * One RSA other prime info.
 */
#define RTASN1TMPL_TYPE         RTCRRSAOTHERPRIMEINFO
#define RTASN1TMPL_EXT_NAME     RTCrRsaOtherPrimeInfo
#define RTASN1TMPL_INT_NAME     rtCrRsaOtherPrimeInfo
RTASN1TMPL_BEGIN_SEQCORE();
RTASN1TMPL_MEMBER(              Prime,              RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_MEMBER(              Exponent,           RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_MEMBER(              Coefficient,        RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_END_SEQCORE();
#undef RTASN1TMPL_TYPE
#undef RTASN1TMPL_EXT_NAME
#undef RTASN1TMPL_INT_NAME


/*
 * Sequence of RSA other prime infos.
 */
#define RTASN1TMPL_TYPE         RTCRRSAOTHERPRIMEINFOS
#define RTASN1TMPL_EXT_NAME     RTCrRsaOtherPrimeInfos
#define RTASN1TMPL_INT_NAME     rtCrRsaOtherPrimeInfos
RTASN1TMPL_SEQ_OF(RTCRRSAOTHERPRIMEINFO, RTCrRsaOtherPrimeInfo);
#undef RTASN1TMPL_TYPE
#undef RTASN1TMPL_EXT_NAME
#undef RTASN1TMPL_INT_NAME


/*
 * RSA private key.
 */
#define RTASN1TMPL_TYPE         RTCRRSAPRIVATEKEY
#define RTASN1TMPL_EXT_NAME     RTCrRsaPrivateKey
#define RTASN1TMPL_INT_NAME     rtCrRsaPrivateKey
RTASN1TMPL_BEGIN_SEQCORE();
RTASN1TMPL_MEMBER(              Version,            RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_MEMBER(              Modulus,            RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_MEMBER(              PublicExponent,     RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_MEMBER(              PrivateExponent,    RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_MEMBER(              Prime1,             RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_MEMBER(              Prime2,             RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_MEMBER(              Exponent1,          RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_MEMBER(              Exponent2,          RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_MEMBER(              Coefficient,        RTASN1INTEGER,                  RTAsn1Integer);
RTASN1TMPL_MEMBER_OPT_ITAG_EX(  OtherPrimeInfos,    RTCRRSAOTHERPRIMEINFOS,         RTCrRsaOtherPrimeInfos, ASN1_TAG_SEQUENCE, RTASN1TMPL_ITAG_F_UC,  RT_NOTHING);
RTASN1TMPL_END_SEQCORE();
#undef RTASN1TMPL_TYPE
#undef RTASN1TMPL_EXT_NAME
#undef RTASN1TMPL_INT_NAME


/*
 * RSA Digest Info.
 */
#define RTASN1TMPL_TYPE         RTCRRSADIGESTINFO
#define RTASN1TMPL_EXT_NAME     RTCrRsaDigestInfo
#define RTASN1TMPL_INT_NAME     rtCrRsaDigestInfo
RTASN1TMPL_BEGIN_SEQCORE();
RTASN1TMPL_MEMBER(              DigestAlgorithm,    RTCRX509ALGORITHMIDENTIFIER,    RTCrX509AlgorithmIdentifier);
RTASN1TMPL_MEMBER(              Digest,             RTASN1OCTETSTRING,              RTAsn1OctetString);
RTASN1TMPL_END_SEQCORE();
#undef RTASN1TMPL_TYPE
#undef RTASN1TMPL_EXT_NAME
#undef RTASN1TMPL_INT_NAME

