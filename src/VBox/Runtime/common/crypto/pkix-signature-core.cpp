/* $Id$ */
/** @file
 * IPRT - Crypto - Public Key Signature Schema Algorithm, Core API.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/crypto/pkix.h>

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/crypto/digest.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Generic public key signature scheme instance.
 */
typedef struct RTCRPKIXSIGNATUREINT
{
    /** Magic value (RTCRPKIXSIGNATUREINT_MAGIC). */
    uint32_t                u32Magic;
    /** Reference counter. */
    uint32_t volatile       cRefs;
    /** Pointer to the message digest descriptor. */
    PCRTCRPKIXSIGNATUREDESC pDesc;
    /** The operation this instance is licensed for. */
    bool                    fSigning;
    /** State. */
    uint32_t                uState;
#if ARCH_BITS == 32
    uint32_t                uPadding;
#endif

    /** Opaque data specific to the message digest algorithm, size given by
     * RTCRPKIXSIGNATUREDESC::cbState. */
    uint8_t                 abState[1];
} RTCRPKIXSIGNATUREINT;
AssertCompileMemberAlignment(RTCRPKIXSIGNATUREINT, abState, 8);
/** Pointer to a public key algorithm instance. */
typedef RTCRPKIXSIGNATUREINT *PRTCRPKIXSIGNATUREINT;

/** Magic value for RTCRPKIXSIGNATUREINT::u32Magic (Baley Withfield Diffie). */
#define RTCRPKIXSIGNATUREINT_MAGIC         UINT32_C(0x19440605)

/** @name RTCRPKIXSIGNATUREINT::uState values.
 * @{ */
/** Ready to go. */
#define RTCRPKIXSIGNATURE_STATE_READY      UINT32_C(1)
/** Need reset. */
#define RTCRPKIXSIGNATURE_STATE_DONE       UINT32_C(2)
/** Busted state, can happen after re-init. */
#define RTCRPKIXSIGNATURE_STATE_BUSTED     UINT32_C(3)
/** @} */



RTDECL(int) RTCrPkixSignatureCreate(PRTCRPKIXSIGNATURE phSignature, PCRTCRPKIXSIGNATUREDESC pDesc, void *pvOpaque,
                                    bool fSigning, PCRTASN1BITSTRING pKey, PCRTASN1DYNTYPE pParams)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(phSignature, VERR_INVALID_POINTER);
    AssertPtrReturn(pDesc, VERR_INVALID_POINTER);
    AssertPtrReturn(pKey, VERR_INVALID_POINTER);
    AssertReturn(RTAsn1BitString_IsPresent(pKey), VERR_INVALID_PARAMETER);
    if (pParams)
    {
        AssertPtrReturn(pParams, VERR_INVALID_POINTER);
        if (   pParams->enmType == RTASN1TYPE_NULL
            || !RTASN1CORE_IS_PRESENT(&pParams->u.Core))
            pParams = NULL;
    }

    /*
     * Instantiate the algorithm for the given operation.
     */
    int rc = VINF_SUCCESS;
    PRTCRPKIXSIGNATUREINT pThis = (PRTCRPKIXSIGNATUREINT)RTMemAllocZ(RT_OFFSETOF(RTCRPKIXSIGNATUREINT, abState[pDesc->cbState]));
    if (pThis)
    {
        pThis->u32Magic     = RTCRPKIXSIGNATUREINT_MAGIC;
        pThis->cRefs        = 1;
        pThis->pDesc        = pDesc;
        pThis->fSigning     = fSigning;
        pThis->uState       = RTCRPKIXSIGNATURE_STATE_READY;
        if (pDesc->pfnInit)
            rc = pDesc->pfnInit(pDesc, pThis->abState, pvOpaque, fSigning, pKey, pParams);
        if (RT_SUCCESS(rc))
        {
            *phSignature = pThis;
            return VINF_SUCCESS;
        }
        pThis->u32Magic = 0;
        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;

}


RTDECL(uint32_t) RTCrPkixSignatureRetain(RTCRPKIXSIGNATURE hSignature)
{
    PRTCRPKIXSIGNATUREINT pThis = hSignature;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTCRPKIXSIGNATUREINT_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs < 64);
    return cRefs;
}


RTDECL(uint32_t) RTCrPkixSignatureRelease(RTCRPKIXSIGNATURE hSignature)
{
    PRTCRPKIXSIGNATUREINT pThis = hSignature;
    if (pThis == NIL_RTCRPKIXSIGNATURE)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTCRPKIXSIGNATUREINT_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    Assert(cRefs < 64);
    if (!cRefs)
    {
        pThis->u32Magic = ~RTCRPKIXSIGNATUREINT_MAGIC;
        if (pThis->pDesc->pfnDelete)
            pThis->pDesc->pfnDelete(pThis->pDesc, pThis->abState, pThis->fSigning);

        size_t cbToWipe = RT_OFFSETOF(RTCRPKIXSIGNATUREINT, abState[pThis->pDesc->cbState]);
        RTMemWipeThoroughly(pThis, cbToWipe, 6);

        RTMemFree(pThis);
    }
    return cRefs;
}


/**
 * Resets the signature provider instance prior to a new signing or verification
 * opartion.
 *
 * @returns IPRT status code.
 * @param   pThis               The instance to reset.
 */
static int rtCrPkxiSignatureReset(PRTCRPKIXSIGNATUREINT pThis)
{
    if (pThis->uState == RTCRPKIXSIGNATURE_STATE_DONE)
    {
        if (pThis->pDesc->pfnReset)
        {
            int rc = pThis->pDesc->pfnReset(pThis->pDesc, pThis->abState, pThis->fSigning);
            if (RT_FAILURE(rc))
            {
                pThis->uState = RTCRPKIXSIGNATURE_STATE_BUSTED;
                return rc;
            }
        }
        pThis->uState = RTCRPKIXSIGNATURE_STATE_READY;
    }
    return VINF_SUCCESS;
}


RTDECL(int) RTCrPkixSignatureVerify(RTCRPKIXSIGNATURE hSignature, RTCRDIGEST hDigest,
                                    void const *pvSignature, size_t cbSignature)
{
    PRTCRPKIXSIGNATUREINT pThis = hSignature;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRPKIXSIGNATUREINT_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(!pThis->fSigning, VERR_INVALID_FUNCTION);
    AssertReturn(pThis->uState == RTCRPKIXSIGNATURE_STATE_READY || pThis->uState == RTCRPKIXSIGNATURE_STATE_DONE, VERR_INVALID_STATE);

    uint32_t cRefs = RTCrDigestRetain(hDigest);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    int rc = rtCrPkxiSignatureReset(pThis);
    if (RT_SUCCESS(rc))
    {
        rc = pThis->pDesc->pfnVerify(pThis->pDesc, pThis->abState, hDigest, pvSignature, cbSignature);
        pThis->uState = RTCRPKIXSIGNATURE_STATE_DONE;
    }

    RTCrDigestRelease(hDigest);
    return rc;
}


RTDECL(int) RTCrPkixSignatureVerifyBitString(RTCRPKIXSIGNATURE hSignature, RTCRDIGEST hDigest, PCRTASN1BITSTRING pSignature)
{
    /*
     * Just unpack it and pass it on to the lower level API.
     */
    AssertPtrReturn(pSignature, VERR_INVALID_POINTER);
    AssertReturn(RTAsn1BitString_IsPresent(pSignature), VERR_INVALID_PARAMETER);
    uint32_t    cbData = RTASN1BITSTRING_GET_BYTE_SIZE(pSignature);
    void const *pvData = RTASN1BITSTRING_GET_BIT0_PTR(pSignature);
    AssertPtrReturn(pvData, VERR_INVALID_PARAMETER);

    return RTCrPkixSignatureVerify(hSignature, hDigest, pvData, cbData);
}


RTDECL(int) RTCrPkixSignatureVerifyOctetString(RTCRPKIXSIGNATURE hSignature, RTCRDIGEST hDigest, PCRTASN1OCTETSTRING pSignature)
{
    /*
     * Just unpack it and pass it on to the lower level API.
     */
    AssertPtrReturn(pSignature, VERR_INVALID_POINTER);
    AssertReturn(RTAsn1OctetString_IsPresent(pSignature), VERR_INVALID_PARAMETER);
    uint32_t    cbData = pSignature->Asn1Core.cb;
    void const *pvData = pSignature->Asn1Core.uData.pv;
    AssertPtrReturn(pvData, VERR_INVALID_PARAMETER);

    return RTCrPkixSignatureVerify(hSignature, hDigest, pvData, cbData);
}


RTDECL(int) RTCrPkixSignatureSign(RTCRPKIXSIGNATURE hSignature, RTCRDIGEST hDigest,
                                  void *pvSignature, size_t *pcbSignature)
{
    PRTCRPKIXSIGNATUREINT pThis = hSignature;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRPKIXSIGNATUREINT_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->fSigning, VERR_INVALID_FUNCTION);
    AssertReturn(pThis->uState == RTCRPKIXSIGNATURE_STATE_READY || pThis->uState == RTCRPKIXSIGNATURE_STATE_DONE, VERR_INVALID_STATE);

    uint32_t cRefs = RTCrDigestRetain(hDigest);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    int rc = rtCrPkxiSignatureReset(pThis);
    if (RT_SUCCESS(rc))
    {
        rc = pThis->pDesc->pfnSign(pThis->pDesc, pThis->abState, hDigest, pvSignature, pcbSignature);
        if (rc != VERR_BUFFER_OVERFLOW)
            pThis->uState = RTCRPKIXSIGNATURE_STATE_DONE;
    }

    RTCrDigestRelease(hDigest);
    return rc;
}

