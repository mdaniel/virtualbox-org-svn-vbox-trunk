/** @file
 * IPRT - Crypto - Time-Stamp Protocol (RFC-3161).
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

#ifndef ___iprt_crypto_tsp_h
#define ___iprt_crypto_tsp_h

#include <iprt/asn1.h>
#include <iprt/crypto/x509.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_cr_tap RTCrTap - Time-Stamp Protocol (RFC-3161)
 * @ingroup grp_rt_crypto
 * @{
 */


/**
 * RFC-3161 MessageImprint (IPRT representation).
 */
typedef struct RTCRTSPMESSAGEIMPRINT
{
    /** Sequence core. */
    RTASN1SEQUENCECORE                  SeqCore;
    /** The digest algorithm used to produce HashedMessage. */
    RTCRX509ALGORITHMIDENTIFIER         HashAlgorithm;
    /** The digest of the message being timestamped. */
    RTASN1OCTETSTRING                   HashedMessage;
} RTCRTSPMESSAGEIMPRINT;
/** Pointer to the IPRT representation of a RFC-3161 MessageImprint. */
typedef RTCRTSPMESSAGEIMPRINT *PRTCRTSPMESSAGEIMPRINT;
/** Pointer to the const IPRT representation of a RFC-3161 MessageImprint. */
typedef RTCRTSPMESSAGEIMPRINT const *PCRTCRTSPMESSAGEIMPRINT;
RTASN1TYPE_STANDARD_PROTOTYPES(RTCRTSPMESSAGEIMPRINT, RTDECL, RTCrTspMessageImprint, SeqCore.Asn1Core);


/**
 * RFC-3161 Accuracy (IPRT representation).
 */
typedef struct RTCRTSPACCURACY
{
    /** Sequence core. */
    RTASN1SEQUENCECORE                  SeqCore;
    /** The seconds accuracy.
     * This will be larger than 0.  If 1 inspect the Millis field.  */
    RTASN1INTEGER                       Seconds;
    /** The millisecond accuracy, optional, implicit tag 0.
     * Range 1..999.  If 1 inspect the Micros field.  */
    RTASN1INTEGER                       Millis;
    /** The microsecond accuracy, optional, implicit tag 1.
     * Range 1..999. */
    RTASN1INTEGER                       Micros;
} RTCRTSPACCURACY;
/** Pointer to the IPRT representation of a RFC-3161 Accuracy. */
typedef RTCRTSPACCURACY *PRTCRTSPACCURACY;
/** Pointer to the const IPRT representation of a RFC-3161 Accuracy. */
typedef RTCRTSPACCURACY const *PCRTCRTSPACCURACY;
RTASN1TYPE_STANDARD_PROTOTYPES(RTCRTSPACCURACY, RTDECL, RTCrTspAccuracy, SeqCore.Asn1Core);


/**
 * RFC-3161 TSTInfo (IPRT representation).
 */
typedef struct RTCRTSPTSTINFO
{
    /** Sequence core. */
    RTASN1SEQUENCECORE                  SeqCore;
    /** The structure version number, current only 1 is valid. */
    RTASN1INTEGER                       Version;
    /** Time authority policy. */
    RTASN1OBJID                         Policy;
    /** The message imprint. */
    RTCRTSPMESSAGEIMPRINT               MessageImprint;
    /** Timestamp request serial number. */
    RTASN1INTEGER                       SerialNumber;
    /** The timestamp. */
    RTASN1TIME                          GenTime;
    /** The timestamp accuracy, optional. */
    RTCRTSPACCURACY                     Accuracy;
    /** Ordering, whatever that means, defaults to FALSE. */
    RTASN1BOOLEAN                       Ordering;
    /** Nonce, optional. */
    RTASN1INTEGER                       Nonce;
    /** Timestamp authority name, explicit optional.
     * (Should match a name in the certificate of the signature.) */
    struct
    {
        /** Context tag 0. */
        RTASN1CONTEXTTAG0               CtxTag0;
        /** The TSA name. */
        RTCRX509GENERALNAME             Tsa;
    } T0;
    /** Extensions, optional, implicit tag 1.  */
    RTCRX509EXTENSION                   Extensions;
} RTCRTSPTSTINFO;
/** Pointer to the IPRT representation of a RFC-3161 TSTInfo. */
typedef RTCRTSPTSTINFO *PRTCRTSPTSTINFO;
/** Pointer to the const IPRT representation of a RFC-3161 TSTInfo. */
typedef RTCRTSPTSTINFO const *PCRTCRTSPTSTINFO;
RTASN1TYPE_STANDARD_PROTOTYPES(RTCRTSPTSTINFO, RTDECL, RTCrTspTstInfo, SeqCore.Asn1Core);

/** The object identifier for RTCRTSPTSTINFO.
 * Found in the ContentType field of PKCS \#7's ContentInfo structure and
 * the equivalent CMS field. */
#define RTCRTSPTSTINFO_OID              "1.2.840.113549.1.9.16.1.4"

/** @} */

RT_C_DECLS_END

#endif

