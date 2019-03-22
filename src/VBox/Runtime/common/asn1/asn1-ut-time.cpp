/* $Id$ */
/** @file
 * IPRT - ASN.1, UTC TIME and GENERALIZED TIME Types.
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

#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/uni.h>

#include <iprt/formats/asn1.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The UTC TIME encoding of the IPRT epoch time. */
static const char g_szEpochUtc[] = "700101000000Z";
/** The GENERALIZED TIME encoding of the IPRT epoch time. */
static const char g_szEpochGeneralized[] = "19700101000000Z";


/*
 * ASN.1 TIME - Special Methods.
 */

RTDECL(int) RTAsn1Time_InitEx(PRTASN1TIME pThis, uint32_t uTag, PCRTASN1ALLOCATORVTABLE pAllocator)
{
    RT_NOREF_PV(pAllocator);
    AssertReturn(uTag == ASN1_TAG_UTC_TIME || uTag == ASN1_TAG_GENERALIZED_TIME, VERR_INVALID_PARAMETER);
    RTAsn1Core_InitEx(&pThis->Asn1Core,
                      uTag,
                      ASN1_TAGCLASS_UNIVERSAL | ASN1_TAGFLAG_PRIMITIVE,
                      &g_RTAsn1Time_Vtable,
                      RTASN1CORE_F_PRESENT | RTASN1CORE_F_PRIMITE_TAG_STRUCT);
    if (uTag == ASN1_TAG_UTC_TIME)
    {
        pThis->Asn1Core.cb = sizeof(g_szEpochUtc) - 1;
        pThis->Asn1Core.uData.pv = &g_szEpochUtc[0];
    }
    else
    {
        pThis->Asn1Core.cb = sizeof(g_szEpochGeneralized) - 1;
        pThis->Asn1Core.uData.pv = &g_szEpochGeneralized[0];
    }
    return VINF_SUCCESS;
}


RTDECL(int) RTAsn1Time_CompareWithTimeSpec(PCRTASN1TIME pLeft, PCRTTIMESPEC pTsRight)
{
    int iDiff = RTAsn1Time_IsPresent(pLeft) ? 0 : -1;
    if (!iDiff)
    {
        RTTIME RightTime;
        iDiff = RTTimeCompare(&pLeft->Time, RTTimeExplode(&RightTime, pTsRight));
    }

    return iDiff;
}


/*
 * ASN.1 TIME - Standard Methods.
 */

RT_DECL_DATA_CONST(RTASN1COREVTABLE const) g_RTAsn1Time_Vtable =
{
    "RTAsn1Time",
    sizeof(RTASN1TIME),
    UINT8_MAX,
    ASN1_TAGCLASS_UNIVERSAL | ASN1_TAGFLAG_PRIMITIVE,
    0,
    (PFNRTASN1COREVTDTOR)RTAsn1Time_Delete,
    NULL,
    (PFNRTASN1COREVTCLONE)RTAsn1Time_Clone,
    (PFNRTASN1COREVTCOMPARE)RTAsn1Time_Compare,
    (PFNRTASN1COREVTCHECKSANITY)RTAsn1Time_CheckSanity,
    NULL,
    NULL
};


RTDECL(int) RTAsn1Time_Init(PRTASN1TIME pThis, PCRTASN1ALLOCATORVTABLE pAllocator)
{
    /* Using UTC TIME since epoch would be encoded using UTC TIME following
       X.509 Validity / Whatever time tag guidelines. */
    return RTAsn1Time_InitEx(pThis, ASN1_TAG_UTC_TIME, pAllocator);
}


RTDECL(int) RTAsn1Time_Clone(PRTASN1TIME pThis, PCRTASN1TIME pSrc, PCRTASN1ALLOCATORVTABLE pAllocator)
{
    AssertPtr(pSrc); AssertPtr(pThis); AssertPtr(pAllocator);
    RT_ZERO(*pThis);
    if (RTAsn1Time_IsPresent(pSrc))
    {
        AssertReturn(pSrc->Asn1Core.pOps == &g_RTAsn1Time_Vtable, VERR_INTERNAL_ERROR_3);

        int rc = RTAsn1Core_CloneContent(&pThis->Asn1Core, &pSrc->Asn1Core, pAllocator);
        if (RT_SUCCESS(rc))
        {
            pThis->Time = pSrc->Time;
            return VINF_SUCCESS;
        }
        return rc;
    }
    return VINF_SUCCESS;
}


RTDECL(void) RTAsn1Time_Delete(PRTASN1TIME pThis)
{
    if (   pThis
        && RTAsn1Time_IsPresent(pThis))
    {
        Assert(pThis->Asn1Core.pOps == &g_RTAsn1Time_Vtable);

        RTAsn1ContentFree(&pThis->Asn1Core);
        RT_ZERO(*pThis);
    }
}


RTDECL(int) RTAsn1Time_Enum(PRTASN1TIME pThis, PFNRTASN1ENUMCALLBACK pfnCallback, uint32_t uDepth, void *pvUser)
{
    RT_NOREF_PV(pThis); RT_NOREF_PV(pfnCallback); RT_NOREF_PV(uDepth); RT_NOREF_PV(pvUser);
    Assert(pThis && (!RTAsn1Time_IsPresent(pThis) || pThis->Asn1Core.pOps == &g_RTAsn1Time_Vtable));

    /* No children to enumerate. */
    return VINF_SUCCESS;
}


RTDECL(int) RTAsn1Time_Compare(PCRTASN1TIME pLeft, PCRTASN1TIME pRight)
{
    Assert(pLeft  && (!RTAsn1Time_IsPresent(pLeft)  || pLeft->Asn1Core.pOps  == &g_RTAsn1Time_Vtable));
    Assert(pRight && (!RTAsn1Time_IsPresent(pRight) || pRight->Asn1Core.pOps == &g_RTAsn1Time_Vtable));

    int iDiff;
    if (RTAsn1Time_IsPresent(pLeft))
    {
        if (RTAsn1Time_IsPresent(pRight))
            iDiff = RTTimeCompare(&pLeft->Time, &pRight->Time);
        else
            iDiff = -1;
    }
    else
        iDiff = 0 - (int)RTAsn1Time_IsPresent(pRight);
    return iDiff;
}


RTDECL(int) RTAsn1Time_CheckSanity(PCRTASN1TIME pThis, uint32_t fFlags, PRTERRINFO pErrInfo, const char *pszErrorTag)
{
    RT_NOREF_PV(fFlags);
    if (RT_UNLIKELY(!RTAsn1Time_IsPresent(pThis)))
        return RTErrInfoSetF(pErrInfo, VERR_ASN1_NOT_PRESENT, "%s: Missing (TIME).", pszErrorTag);
    return VINF_SUCCESS;
}


/*
 * Generate code for the tag specific methods.
 * Note! This is very similar to what we're doing in asn1-ut-string.cpp.
 */
#define RTASN1TIME_IMPL(a_uTag, a_szTag, a_Api) \
    \
    RTDECL(int) RT_CONCAT(a_Api,_Init)(PRTASN1TIME pThis, PCRTASN1ALLOCATORVTABLE pAllocator) \
    { \
        return RTAsn1Time_InitEx(pThis, a_uTag, pAllocator); \
    } \
    \
    RTDECL(int) RT_CONCAT(a_Api,_Clone)(PRTASN1TIME pThis, PCRTASN1TIME pSrc, PCRTASN1ALLOCATORVTABLE pAllocator) \
    { \
        AssertReturn(RTASN1CORE_GET_TAG(&pSrc->Asn1Core) == a_uTag || !RTAsn1Time_IsPresent(pSrc), \
                     VERR_ASN1_TIME_TAG_MISMATCH); \
        return RTAsn1Time_Clone(pThis, pSrc, pAllocator); \
    } \
    \
    RTDECL(void) RT_CONCAT(a_Api,_Delete)(PRTASN1TIME pThis) \
    { \
        Assert(   !pThis \
               || !RTAsn1Time_IsPresent(pThis) \
               || (   pThis->Asn1Core.pOps == &g_RTAsn1Time_Vtable \
                   && RTASN1CORE_GET_TAG(&pThis->Asn1Core) == a_uTag) ); \
        RTAsn1Time_Delete(pThis); \
    } \
    \
    RTDECL(int) RT_CONCAT(a_Api,_Enum)(PRTASN1TIME pThis, PFNRTASN1ENUMCALLBACK pfnCallback, uint32_t uDepth, void *pvUser) \
    { \
        RT_NOREF_PV(pThis); RT_NOREF_PV(pfnCallback); RT_NOREF_PV(uDepth); RT_NOREF_PV(pvUser); \
        Assert(   pThis \
               && (   !RTAsn1Time_IsPresent(pThis) \
                   || (   pThis->Asn1Core.pOps == &g_RTAsn1Time_Vtable \
                       && RTASN1CORE_GET_TAG(&pThis->Asn1Core) == a_uTag) ) ); \
        /* No children to enumerate. */ \
        return VINF_SUCCESS; \
    } \
    \
    RTDECL(int) RT_CONCAT(a_Api,_Compare)(PCRTASN1TIME pLeft, PCRTASN1TIME pRight) \
    { \
        int iDiff = RTAsn1Time_Compare(pLeft, pRight); \
        if (!iDiff && RTAsn1Time_IsPresent(pLeft)) \
        { \
            if (RTASN1CORE_GET_TAG(&pLeft->Asn1Core) == RTASN1CORE_GET_TAG(&pRight->Asn1Core)) \
            { \
                if (RTASN1CORE_GET_TAG(&pLeft->Asn1Core) != a_uTag) \
                    iDiff = RTASN1CORE_GET_TAG(&pLeft->Asn1Core) < a_uTag ? -1 : 1; \
            } \
            else \
                iDiff = RTASN1CORE_GET_TAG(&pLeft->Asn1Core) < RTASN1CORE_GET_TAG(&pRight->Asn1Core) ? -1 : 1; \
        } \
        return iDiff; \
    } \
    \
    RTDECL(int) RT_CONCAT(a_Api,_CheckSanity)(PCRTASN1TIME pThis, uint32_t fFlags, \
                                              PRTERRINFO pErrInfo, const char *pszErrorTag) \
    { \
        if (RTASN1CORE_GET_TAG(&pThis->Asn1Core) != a_uTag && RTAsn1Time_IsPresent(pThis)) \
            return RTErrInfoSetF(pErrInfo, VERR_ASN1_TIME_TAG_MISMATCH, "%s: uTag=%#x, expected %#x (%s)", \
                                 pszErrorTag, RTASN1CORE_GET_TAG(&pThis->Asn1Core), a_uTag, a_szTag); \
        return RTAsn1Time_CheckSanity(pThis, fFlags, pErrInfo, pszErrorTag); \
    }

#include "asn1-ut-time-template2.h"


/*
 * Generate code for the associated collection types.
 */
#define RTASN1TMPL_TEMPLATE_FILE "../common/asn1/asn1-ut-time-template.h"
#include <iprt/asn1-generator-internal-header.h>
#include <iprt/asn1-generator-core.h>
#include <iprt/asn1-generator-init.h>
#include <iprt/asn1-generator-sanity.h>

