/** @file
 * Host-Guest Communication Manager (HGCM) - Service library definitions.
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

#ifndef ___VBox_hgcm_h
#define ___VBox_hgcm_h

#include <iprt/assert.h>
#include <iprt/stdarg.h>
#include <iprt/string.h>
#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/err.h>
#ifdef IN_RING3
# include <VBox/vmm/stam.h>
# include <VBox/vmm/dbgf.h>
#endif
#ifdef VBOX_TEST_HGCM_PARMS
# include <iprt/test.h>
#endif

/** @todo proper comments. */

/**
 * Service interface version.
 *
 * Includes layout of both VBOXHGCMSVCFNTABLE and VBOXHGCMSVCHELPERS.
 *
 * A service can work with these structures if major version
 * is equal and minor version of service is <= version of the
 * structures.
 *
 * For example when a new helper is added at the end of helpers
 * structure, then the minor version will be increased. All older
 * services still can work because they have their old helpers
 * unchanged.
 *
 * Revision history.
 * 1.1->2.1 Because the pfnConnect now also has the pvClient parameter.
 * 2.1->2.2 Because pfnSaveState and pfnLoadState were added
 * 2.2->3.1 Because pfnHostCall is now synchronous, returns rc, and parameters were changed
 * 3.1->3.2 Because pfnRegisterExtension was added
 * 3.2->3.3 Because pfnDisconnectClient helper was added
 * 3.3->4.1 Because the pvService entry and parameter was added
 * 4.1->4.2 Because the VBOX_HGCM_SVC_PARM_CALLBACK parameter type was added
 * 4.2->5.1 Removed the VBOX_HGCM_SVC_PARM_CALLBACK parameter type, as
 *          this problem is already solved by service extension callbacks
 * 5.1->6.1 Because pfnCall got a new parameter. Also new helpers. (VBox 6.0)
 * 6.1->6.2 Because pfnCallComplete starts returning a status code (VBox 6.0).
 * 6.2->6.3 Because pfnGetRequestor was added (VBox 6.0).
 */
#define VBOX_HGCM_SVC_VERSION_MAJOR (0x0006)
#define VBOX_HGCM_SVC_VERSION_MINOR (0x0003)
#define VBOX_HGCM_SVC_VERSION ((VBOX_HGCM_SVC_VERSION_MAJOR << 16) + VBOX_HGCM_SVC_VERSION_MINOR)


/** Typed pointer to distinguish a call to service. */
struct VBOXHGCMCALLHANDLE_TYPEDEF;
typedef struct VBOXHGCMCALLHANDLE_TYPEDEF *VBOXHGCMCALLHANDLE;

/** Service helpers pointers table. */
typedef struct VBOXHGCMSVCHELPERS
{
    /** The service has processed the Call request. */
    DECLR3CALLBACKMEMBER(int, pfnCallComplete, (VBOXHGCMCALLHANDLE callHandle, int32_t rc));

    void *pvInstance;

    /** The service disconnects the client. */
    DECLR3CALLBACKMEMBER(void, pfnDisconnectClient, (void *pvInstance, uint32_t u32ClientID));

    /**
     * Check if the @a callHandle is for a call restored and re-submitted from saved state.
     *
     * @returns true if restored, false if not.
     * @param   callHandle      The call we're checking up on.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsCallRestored, (VBOXHGCMCALLHANDLE callHandle));

    /** Access to STAMR3RegisterV. */
    DECLR3CALLBACKMEMBER(int, pfnStamRegisterV,(void *pvInstance, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                STAMUNIT enmUnit, const char *pszDesc, const char *pszName, va_list va)
                                                RT_IPRT_FORMAT_ATTR(7, 0));
    /** Access to STAMR3DeregisterV. */
    DECLR3CALLBACKMEMBER(int, pfnStamDeregisterV,(void *pvInstance, const char *pszPatFmt, va_list va) RT_IPRT_FORMAT_ATTR(2, 0));

    /** Access to DBGFR3InfoRegisterExternal. */
    DECLR3CALLBACKMEMBER(int, pfnInfoRegister,(void *pvInstance, const char *pszName, const char *pszDesc,
                                               PFNDBGFHANDLEREXT pfnHandler, void *pvUser));
    /** Access to DBGFR3InfoDeregisterExternal. */
    DECLR3CALLBACKMEMBER(int, pfnInfoDeregister,(void *pvInstance, const char *pszName));

    /**
     * Retrieves the VMMDevRequestHeader::fRequestor value.
     *
     * @returns The field value, 0 if invalid call.
     * @param   hCall       The call we're checking up on.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnGetRequestor, (VBOXHGCMCALLHANDLE hCall));

} VBOXHGCMSVCHELPERS;

typedef VBOXHGCMSVCHELPERS *PVBOXHGCMSVCHELPERS;

#if defined(IN_RING3) || defined(IN_SLICKEDIT)

/** Wrapper around STAMR3RegisterF. */
DECLINLINE(int) RT_IPRT_FORMAT_ATTR(7, 8)
HGCMSvcHlpStamRegister(PVBOXHGCMSVCHELPERS pHlp, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                       STAMUNIT enmUnit, const char *pszDesc, const char *pszName, ...)
{
    int rc;
    va_list va;
    va_start(va, pszName);
    rc = pHlp->pfnStamRegisterV(pHlp->pvInstance, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName, va);
    va_end(va);
    return rc;
}

/** Wrapper around STAMR3RegisterV. */
DECLINLINE(int) RT_IPRT_FORMAT_ATTR(7, 0)
HGCMSvcHlpStamRegisterV(PVBOXHGCMSVCHELPERS pHlp, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                        STAMUNIT enmUnit, const char *pszDesc, const char *pszName, va_list va)
{
    return pHlp->pfnStamRegisterV(pHlp->pvInstance, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName, va);
}

/** Wrapper around STAMR3DeregisterF. */
DECLINLINE(int) RT_IPRT_FORMAT_ATTR(2, 3) HGCMSvcHlpStamDeregister(PVBOXHGCMSVCHELPERS pHlp, const char *pszPatFmt, ...)
{
    int rc;
    va_list va;
    va_start(va, pszPatFmt);
    rc = pHlp->pfnStamDeregisterV(pHlp->pvInstance, pszPatFmt, va);
    va_end(va);
    return rc;
}

/** Wrapper around STAMR3DeregisterV. */
DECLINLINE(int) RT_IPRT_FORMAT_ATTR(2, 0) HGCMSvcHlpStamDeregisterV(PVBOXHGCMSVCHELPERS pHlp, const char *pszPatFmt, va_list va)
{
    return pHlp->pfnStamDeregisterV(pHlp->pvInstance, pszPatFmt, va);
}

/** Wrapper around DBGFR3InfoRegisterExternal. */
DECLINLINE(int) HGCMSvcHlpInfoRegister(PVBOXHGCMSVCHELPERS pHlp, const char *pszName, const char *pszDesc,
                                       PFNDBGFHANDLEREXT pfnHandler, void *pvUser)
{
    return pHlp->pfnInfoRegister(pHlp->pvInstance, pszName, pszDesc, pfnHandler, pvUser);
}

/** Wrapper around DBGFR3InfoDeregisterExternal. */
DECLINLINE(int) HGCMSvcHlpInfoDeregister(PVBOXHGCMSVCHELPERS pHlp, const char *pszName)
{
    return pHlp->pfnInfoDeregister(pHlp->pvInstance, pszName);
}

#endif /* IN_RING3 */


#define VBOX_HGCM_SVC_PARM_INVALID (0U)
#define VBOX_HGCM_SVC_PARM_32BIT (1U)
#define VBOX_HGCM_SVC_PARM_64BIT (2U)
#define VBOX_HGCM_SVC_PARM_PTR   (3U)

typedef struct VBOXHGCMSVCPARM
{
    /** VBOX_HGCM_SVC_PARM_* values. */
    uint32_t type;

    union
    {
        uint32_t uint32;
        uint64_t uint64;
        struct
        {
            uint32_t size;
            void *addr;
        } pointer;
    } u;
} VBOXHGCMSVCPARM;

/** Extract an uint32_t value from an HGCM parameter structure. */
DECLINLINE(int) HGCMSvcGetU32(struct VBOXHGCMSVCPARM *pParm, uint32_t *pu32)
{
    int rc = VINF_SUCCESS;
    AssertPtrReturn(pParm, VERR_INVALID_POINTER);
    AssertPtrReturn(pParm, VERR_INVALID_POINTER);
    AssertPtrReturn(pu32, VERR_INVALID_POINTER);
    if (pParm->type != VBOX_HGCM_SVC_PARM_32BIT)
        rc = VERR_INVALID_PARAMETER;
    if (RT_SUCCESS(rc))
        *pu32 = pParm->u.uint32;
    return rc;
}

/** Extract an uint64_t value from an HGCM parameter structure. */
DECLINLINE(int) HGCMSvcGetU64(struct VBOXHGCMSVCPARM *pParm, uint64_t *pu64)
{
    int rc = VINF_SUCCESS;
    AssertPtrReturn(pParm, VERR_INVALID_POINTER);
    AssertPtrReturn(pParm, VERR_INVALID_POINTER);
    AssertPtrReturn(pu64, VERR_INVALID_POINTER);
    if (pParm->type != VBOX_HGCM_SVC_PARM_64BIT)
        rc = VERR_INVALID_PARAMETER;
    if (RT_SUCCESS(rc))
        *pu64 = pParm->u.uint64;
    return rc;
}

/** Extract an pointer value from an HGCM parameter structure. */
DECLINLINE(int) HGCMSvcGetPv(struct VBOXHGCMSVCPARM *pParm, void **ppv,
                               uint32_t *pcb)
{
    AssertPtrReturn(pParm, VERR_INVALID_POINTER);
    AssertPtrReturn(ppv, VERR_INVALID_POINTER);
    AssertPtrReturn(pcb, VERR_INVALID_POINTER);
    if (pParm->type == VBOX_HGCM_SVC_PARM_PTR)
    {
        *ppv = pParm->u.pointer.addr;
        *pcb = pParm->u.pointer.size;
        return VINF_SUCCESS;
    }

    return VERR_INVALID_PARAMETER;
}

/** Extract a constant pointer value from an HGCM parameter structure. */
DECLINLINE(int) HGCMSvcGetPcv(struct VBOXHGCMSVCPARM *pParm, const void **ppv,
                                uint32_t *pcb)
{
    AssertPtrReturn(pParm, VERR_INVALID_POINTER);
    AssertPtrReturn(ppv, VERR_INVALID_POINTER);
    AssertPtrReturn(pcb, VERR_INVALID_POINTER);
    if (pParm->type == VBOX_HGCM_SVC_PARM_PTR)
    {
        *ppv = (const void *)pParm->u.pointer.addr;
        *pcb = pParm->u.pointer.size;
        return VINF_SUCCESS;
    }

    return VERR_INVALID_PARAMETER;
}

/** Extract a valid pointer to a non-empty buffer from an HGCM parameter
 * structure. */
DECLINLINE(int) HGCMSvcGetBuf(struct VBOXHGCMSVCPARM *pParm, void **ppv,
                                uint32_t *pcb)
{
    AssertPtrReturn(pParm, VERR_INVALID_POINTER);
    AssertPtrReturn(ppv, VERR_INVALID_POINTER);
    AssertPtrReturn(pcb, VERR_INVALID_POINTER);
    if (   pParm->type == VBOX_HGCM_SVC_PARM_PTR
        && VALID_PTR(pParm->u.pointer.addr)
        && pParm->u.pointer.size > 0)
    {
        *ppv = pParm->u.pointer.addr;
        *pcb = pParm->u.pointer.size;
        return VINF_SUCCESS;
    }

    return VERR_INVALID_PARAMETER;
}

/** Extract a valid pointer to a non-empty constant buffer from an HGCM
 * parameter structure. */
DECLINLINE(int) HGCMSvcGetCBuf(struct VBOXHGCMSVCPARM *pParm,
                                 const void **ppv, uint32_t *pcb)
{
    AssertPtrReturn(pParm, VERR_INVALID_POINTER);
    AssertPtrReturn(ppv, VERR_INVALID_POINTER);
    AssertPtrReturn(pcb, VERR_INVALID_POINTER);
    if (   pParm->type == VBOX_HGCM_SVC_PARM_PTR
        && VALID_PTR(pParm->u.pointer.addr)
        && pParm->u.pointer.size > 0)
    {
        *ppv = (const void *)pParm->u.pointer.addr;
        *pcb = pParm->u.pointer.size;
        return VINF_SUCCESS;
    }

    return VERR_INVALID_PARAMETER;
}

/** Extract a string value from an HGCM parameter structure. */
DECLINLINE(int) HGCMSvcGetStr(struct VBOXHGCMSVCPARM *pParm, char **ppch,
                                uint32_t *pcb)
{
    AssertPtrReturn(pParm, VERR_INVALID_POINTER);
    AssertPtrReturn(ppch, VERR_INVALID_POINTER);
    AssertPtrReturn(pcb, VERR_INVALID_POINTER);
    if (   pParm->type == VBOX_HGCM_SVC_PARM_PTR
        && VALID_PTR(pParm->u.pointer.addr)
        && pParm->u.pointer.size > 0)
    {
        int rc = RTStrValidateEncodingEx((char *)pParm->u.pointer.addr,
                                         pParm->u.pointer.size,
                                         RTSTR_VALIDATE_ENCODING_ZERO_TERMINATED);
        if (RT_FAILURE(rc))
            return rc;
        *ppch = (char *)pParm->u.pointer.addr;
        *pcb = pParm->u.pointer.size;
        return VINF_SUCCESS;
    }

    return VERR_INVALID_PARAMETER;
}

/** Extract a constant string value from an HGCM parameter structure. */
DECLINLINE(int) HGCMSvcGetCStr(struct VBOXHGCMSVCPARM *pParm,
                                 const char **ppch, uint32_t *pcb)
{
    AssertPtrReturn(pParm, VERR_INVALID_POINTER);
    AssertPtrReturn(ppch, VERR_INVALID_POINTER);
    AssertPtrReturn(pcb, VERR_INVALID_POINTER);
    if (   pParm->type == VBOX_HGCM_SVC_PARM_PTR
        && VALID_PTR(pParm->u.pointer.addr)
        && pParm->u.pointer.size > 0)
    {
        int rc = RTStrValidateEncodingEx((char *)pParm->u.pointer.addr,
                                         pParm->u.pointer.size,
                                         RTSTR_VALIDATE_ENCODING_ZERO_TERMINATED);
        if (RT_FAILURE(rc))
            return rc;
        *ppch = (char *)pParm->u.pointer.addr;
        *pcb = pParm->u.pointer.size;
        return VINF_SUCCESS;
    }

    return VERR_INVALID_PARAMETER;
}

/** Extract a constant string value from an HGCM parameter structure. */
DECLINLINE(int) HGCMSvcGetPsz(struct VBOXHGCMSVCPARM *pParm, const char **ppch,
                                uint32_t *pcb)
{
    AssertPtrReturn(pParm, VERR_INVALID_POINTER);
    AssertPtrReturn(ppch, VERR_INVALID_POINTER);
    AssertPtrReturn(pcb, VERR_INVALID_POINTER);
    if (   pParm->type == VBOX_HGCM_SVC_PARM_PTR
        && VALID_PTR(pParm->u.pointer.addr)
        && pParm->u.pointer.size > 0)
    {
        int rc = RTStrValidateEncodingEx((const char *)pParm->u.pointer.addr,
                                         pParm->u.pointer.size,
                                         RTSTR_VALIDATE_ENCODING_ZERO_TERMINATED);
        if (RT_FAILURE(rc))
            return rc;
        *ppch = (const char *)pParm->u.pointer.addr;
        *pcb = pParm->u.pointer.size;
        return VINF_SUCCESS;
    }

    return VERR_INVALID_PARAMETER;
}

/** Set a uint32_t value to an HGCM parameter structure */
DECLINLINE(void) HGCMSvcSetU32(struct VBOXHGCMSVCPARM *pParm, uint32_t u32)
{
    AssertPtr(pParm);
    pParm->type = VBOX_HGCM_SVC_PARM_32BIT;
    pParm->u.uint32 = u32;
}

/** Set a uint64_t value to an HGCM parameter structure */
DECLINLINE(void) HGCMSvcSetU64(struct VBOXHGCMSVCPARM *pParm, uint64_t u64)
{
    AssertPtr(pParm);
    pParm->type = VBOX_HGCM_SVC_PARM_64BIT;
    pParm->u.uint64 = u64;
}

/** Set a pointer value to an HGCM parameter structure */
DECLINLINE(void) HGCMSvcSetPv(struct VBOXHGCMSVCPARM *pParm, void *pv,
                                uint32_t cb)
{
    AssertPtr(pParm);
    pParm->type = VBOX_HGCM_SVC_PARM_PTR;
    pParm->u.pointer.addr = pv;
    pParm->u.pointer.size = cb;
}

/** Set a pointer value to an HGCM parameter structure */
DECLINLINE(void) HGCMSvcSetStr(struct VBOXHGCMSVCPARM *pParm, const char *psz)
{
    AssertPtr(pParm);
    pParm->type = VBOX_HGCM_SVC_PARM_PTR;
    pParm->u.pointer.addr = (void *)psz;
    pParm->u.pointer.size = (uint32_t)strlen(psz) + 1;
}

#ifdef __cplusplus
# ifdef ___iprt_cpp_ministring_h
/** Set a const string value to an HGCM parameter structure */
DECLINLINE(void) HGCMSvcSetRTCStr(struct VBOXHGCMSVCPARM *pParm,
                                    const RTCString &rString)
{
    AssertPtr(pParm);
    pParm->type = VBOX_HGCM_SVC_PARM_PTR;
    pParm->u.pointer.addr = (void *)rString.c_str();
    pParm->u.pointer.size = (uint32_t)rString.length() + 1;
}
# endif
#endif

typedef VBOXHGCMSVCPARM *PVBOXHGCMSVCPARM;

#ifdef VBOX_WITH_CRHGSMI
typedef void * HGCMCVSHANDLE;

typedef DECLCALLBACK(void) HGCMHOSTFASTCALLCB (int32_t result, uint32_t u32Function, PVBOXHGCMSVCPARM pParam, void *pvContext);
typedef HGCMHOSTFASTCALLCB *PHGCMHOSTFASTCALLCB;
#endif


/** Service specific extension callback.
 *  This callback is called by the service to perform service specific operation.
 *
 * @param pvExtension The extension pointer.
 * @param u32Function What the callback is supposed to do.
 * @param pvParm      The function parameters.
 * @param cbParm      The size of the function parameters.
 */
typedef DECLCALLBACK(int) FNHGCMSVCEXT(void *pvExtension, uint32_t u32Function, void *pvParm, uint32_t cbParms);
typedef FNHGCMSVCEXT *PFNHGCMSVCEXT;

/** The Service DLL entry points.
 *
 *  HGCM will call the DLL "VBoxHGCMSvcLoad"
 *  function and the DLL must fill in the VBOXHGCMSVCFNTABLE
 *  with function pointers.
 */

/* The structure is used in separately compiled binaries so an explicit packing is required. */
#pragma pack(1) /** @todo r=bird: The pragma pack(1) is not at all required!! */
typedef struct VBOXHGCMSVCFNTABLE
{
    /** @name Filled by HGCM
     * @{ */

    /** Size of the structure. */
    uint32_t                 cbSize;

    /** Version of the structure, including the helpers. */
    uint32_t                 u32Version;

    PVBOXHGCMSVCHELPERS      pHelpers;
    /** @} */

    /** @name Filled in by the service.
     * @{ */

    /** Size of client information the service want to have. */
    uint32_t                 cbClient;
#if ARCH_BITS == 64
    /** Ensure that the following pointers are properly aligned on 64-bit system. */
    uint32_t                 u32Alignment0;
#endif

    /** Uninitialize service */
    DECLR3CALLBACKMEMBER(int, pfnUnload, (void *pvService));

    /** Inform the service about a client connection. */
    DECLR3CALLBACKMEMBER(int, pfnConnect, (void *pvService, uint32_t u32ClientID, void *pvClient));

    /** Inform the service that the client wants to disconnect. */
    DECLR3CALLBACKMEMBER(int, pfnDisconnect, (void *pvService, uint32_t u32ClientID, void *pvClient));

    /** Service entry point.
     *  Return code is passed to pfnCallComplete callback.
     */
    DECLR3CALLBACKMEMBER(void, pfnCall, (void *pvService, VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID, void *pvClient,
                                         uint32_t function, uint32_t cParms, VBOXHGCMSVCPARM paParms[], uint64_t tsArrival));

    /** Host Service entry point meant for privileged features invisible to the guest.
     *  Return code is passed to pfnCallComplete callback.
     */
    DECLR3CALLBACKMEMBER(int, pfnHostCall, (void *pvService, uint32_t function, uint32_t cParms, VBOXHGCMSVCPARM paParms[]));

    /** Inform the service about a VM save operation. */
    DECLR3CALLBACKMEMBER(int, pfnSaveState, (void *pvService, uint32_t u32ClientID, void *pvClient, PSSMHANDLE pSSM));

    /** Inform the service about a VM load operation. */
    DECLR3CALLBACKMEMBER(int, pfnLoadState, (void *pvService, uint32_t u32ClientID, void *pvClient, PSSMHANDLE pSSM));

    /** Register a service extension callback. */
    DECLR3CALLBACKMEMBER(int, pfnRegisterExtension, (void *pvService, PFNHGCMSVCEXT pfnExtension, void *pvExtension));

    /** User/instance data pointer for the service. */
    void *pvService;

    /** @} */
} VBOXHGCMSVCFNTABLE;
#pragma pack()


/** Service initialization entry point. */
typedef DECLCALLBACK(int) VBOXHGCMSVCLOAD(VBOXHGCMSVCFNTABLE *ptable);
typedef VBOXHGCMSVCLOAD *PFNVBOXHGCMSVCLOAD;
#define VBOX_HGCM_SVCLOAD_NAME "VBoxHGCMSvcLoad"

#endif
