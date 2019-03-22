/* $Id$ */
/** @file
 * IPRT - ASN.1, OCTET STRING Type, Decoding.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/asn1.h>

#include <iprt/err.h>
#include <iprt/string.h>

#include <iprt/formats/asn1.h>


RTDECL(int) RTAsn1OctetString_DecodeAsn1(PRTASN1CURSOR pCursor, uint32_t fFlags, PRTASN1OCTETSTRING pThis,
                                         const char *pszErrorTag)
{
    pThis->pEncapsulated = NULL;
    RTAsn1CursorInitAllocation(pCursor, &pThis->EncapsulatedAllocation);

    int rc = RTAsn1CursorReadHdr(pCursor, &pThis->Asn1Core, pszErrorTag);
    if (RT_SUCCESS(rc))
    {
        rc = RTAsn1CursorMatchTagClassFlagsString(pCursor, &pThis->Asn1Core, ASN1_TAG_OCTET_STRING,
                                                  ASN1_TAGCLASS_UNIVERSAL | ASN1_TAGFLAG_PRIMITIVE,
                                                  fFlags, pszErrorTag, "OCTET STRING");
        if (RT_SUCCESS(rc))
        {
            if (   !(pThis->Asn1Core.fClass & ASN1_TAGFLAG_CONSTRUCTED)
                || (fFlags & RTASN1CURSOR_GET_F_IMPLICIT) ) /* PKCS #7 ContentInfo tweak. */
            {
                RTAsn1CursorSkip(pCursor, pThis->Asn1Core.cb);
                pThis->Asn1Core.pOps    = &g_RTAsn1OctetString_Vtable;
                pThis->Asn1Core.fFlags |= RTASN1CORE_F_PRIMITE_TAG_STRUCT;
                return VINF_SUCCESS;
            }
            rc = RTAsn1CursorSetInfo(pCursor, VERR_ASN1_CONSTRUCTED_STRING_NOT_IMPL,
                                     "%s: Constructed OCTET STRING not implemented.", pszErrorTag);
        }
        else
            rc = RTAsn1CursorSetInfo(pCursor, rc, "%s: Not OCTET STRING: fClass=%#x / uTag=%#x",
                                     pszErrorTag, pThis->Asn1Core.fClass, pThis->Asn1Core.uTag);
    }
    RT_ZERO(*pThis);
    return rc;
}


/*
 * Generate code for the associated collection types.
 */
#define RTASN1TMPL_TEMPLATE_FILE "../common/asn1/asn1-ut-octetstring-template.h"
#include <iprt/asn1-generator-internal-header.h>
#include <iprt/asn1-generator-asn1-decoder.h>

