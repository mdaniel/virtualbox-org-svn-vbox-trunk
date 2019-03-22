/* $Id$ */
/** @file
 * Main - External authentication library interface.
 */

/*
 * Copyright (C) 2015-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "AuthLibrary.h"
#include "Logging.h"

#include <iprt/err.h>
#include <iprt/ldr.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/thread.h>

typedef struct AuthCtx
{
    AuthResult result;

    PAUTHENTRY3 pfnAuthEntry3;
    PAUTHENTRY2 pfnAuthEntry2;
    PAUTHENTRY  pfnAuthEntry;

    const char         *pszCaller;
    PAUTHUUID          pUuid;
    AuthGuestJudgement guestJudgement;
    const char         *pszUser;
    const char         *pszPassword;
    const char         *pszDomain;
    int                fLogon;
    unsigned           clientId;
} AuthCtx;

static DECLCALLBACK(int) authThread(RTTHREAD hThreadSelf, void *pvUser)
{
    RT_NOREF(hThreadSelf);
    AuthCtx *pCtx = (AuthCtx *)pvUser;

    if (pCtx->pfnAuthEntry3)
    {
        pCtx->result = pCtx->pfnAuthEntry3(pCtx->pszCaller, pCtx->pUuid, pCtx->guestJudgement,
                                           pCtx->pszUser, pCtx->pszPassword, pCtx->pszDomain,
                                           pCtx->fLogon, pCtx->clientId);
    }
    else if (pCtx->pfnAuthEntry2)
    {
        pCtx->result = pCtx->pfnAuthEntry2(pCtx->pUuid, pCtx->guestJudgement,
                                           pCtx->pszUser, pCtx->pszPassword, pCtx->pszDomain,
                                           pCtx->fLogon, pCtx->clientId);
    }
    else if (pCtx->pfnAuthEntry)
    {
        pCtx->result = pCtx->pfnAuthEntry(pCtx->pUuid, pCtx->guestJudgement,
                                          pCtx->pszUser, pCtx->pszPassword, pCtx->pszDomain);
    }
    return VINF_SUCCESS;
}

static AuthResult authCall(AuthCtx *pCtx)
{
    AuthResult result = AuthResultAccessDenied;

    /* Use a separate thread because external modules might need a lot of stack space. */
    RTTHREAD thread = NIL_RTTHREAD;
    int rc = RTThreadCreate(&thread, authThread, pCtx, 512*_1K,
                            RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "VRDEAuth");
    LogFlowFunc(("RTThreadCreate %Rrc\n", rc));

    if (RT_SUCCESS(rc))
    {
        rc = RTThreadWait(thread, RT_INDEFINITE_WAIT, NULL);
        LogFlowFunc(("RTThreadWait %Rrc\n", rc));
    }

    if (RT_SUCCESS(rc))
    {
        /* Only update the result if the thread finished without errors. */
        result = pCtx->result;
    }
    else
    {
        LogRel(("AUTH: Unable to execute the auth thread %Rrc\n", rc));
    }

    return result;
}

int AuthLibLoad(AUTHLIBRARYCONTEXT *pAuthLibCtx, const char *pszLibrary)
{
    RT_ZERO(*pAuthLibCtx);

    /* Load the external authentication library. */
    LogRel(("AUTH: Loading external authentication library '%s'\n", pszLibrary));

    int rc;
    if (RTPathHavePath(pszLibrary))
        rc = RTLdrLoad(pszLibrary, &pAuthLibCtx->hAuthLibrary);
    else
    {
        rc = RTLdrLoadAppPriv(pszLibrary, &pAuthLibCtx->hAuthLibrary);
        if (RT_FAILURE(rc))
        {
            /* Backward compatibility with old default 'VRDPAuth' name.
             * Try to load new default 'VBoxAuth' instead.
             */
            if (RTStrICmp(pszLibrary, "VRDPAuth") == 0)
            {
                LogRel(("AUTH: Loading external authentication library 'VBoxAuth'\n"));
                rc = RTLdrLoadAppPriv("VBoxAuth", &pAuthLibCtx->hAuthLibrary);
            }
        }
    }

    if (RT_FAILURE(rc))
    {
        LogRel(("AUTH: Failed to load external authentication library: %Rrc\n", rc));
        pAuthLibCtx->hAuthLibrary = NIL_RTLDRMOD;
    }

    if (RT_SUCCESS(rc))
    {
        typedef struct AuthEntryInfoStruct
        {
            const char *pszName;
            void **ppvAddress;
        } AuthEntryInfo;

        AuthEntryInfo entries[] =
        {
            { AUTHENTRY3_NAME, (void **)&pAuthLibCtx->pfnAuthEntry3 },
            { AUTHENTRY2_NAME, (void **)&pAuthLibCtx->pfnAuthEntry2 },
            { AUTHENTRY_NAME,  (void **)&pAuthLibCtx->pfnAuthEntry },
            { NULL, NULL }
        };

        /* Get the entry point. */
        AuthEntryInfo *pEntryInfo = &entries[0];
        while (pEntryInfo->pszName)
        {
            *pEntryInfo->ppvAddress = NULL;

            int rc2 = RTLdrGetSymbol(pAuthLibCtx->hAuthLibrary, pEntryInfo->pszName, pEntryInfo->ppvAddress);
            if (RT_SUCCESS(rc2))
            {
                /* Found an entry point. */
                LogRel(("AUTH: Using entry point '%s'\n", pEntryInfo->pszName));
                rc = VINF_SUCCESS;
                break;
            }

            if (rc2 != VERR_SYMBOL_NOT_FOUND)
                LogRel(("AUTH: Could not resolve import '%s': %Rrc\n", pEntryInfo->pszName, rc2));

            rc = rc2;

            pEntryInfo++;
        }
    }

    if (RT_FAILURE(rc))
        AuthLibUnload(pAuthLibCtx);

    return rc;
}

void AuthLibUnload(AUTHLIBRARYCONTEXT *pAuthLibCtx)
{
    if (pAuthLibCtx->hAuthLibrary != NIL_RTLDRMOD)
        RTLdrClose(pAuthLibCtx->hAuthLibrary);

    RT_ZERO(*pAuthLibCtx);
    pAuthLibCtx->hAuthLibrary = NIL_RTLDRMOD;
}

AuthResult AuthLibAuthenticate(const AUTHLIBRARYCONTEXT *pAuthLibCtx,
                               PCRTUUID pUuid, AuthGuestJudgement guestJudgement,
                               const char *pszUser, const char *pszPassword, const char *pszDomain,
                               uint32_t u32ClientId)
{
    AuthResult result = AuthResultAccessDenied;

    AUTHUUID rawuuid;
    memcpy(rawuuid, pUuid, sizeof(rawuuid));

    LogFlowFunc(("pAuthLibCtx = %p, uuid = %RTuuid, guestJudgement = %d, pszUser = %s, pszPassword = %s, pszDomain = %s, u32ClientId = %d\n",
                 pAuthLibCtx, rawuuid, guestJudgement, pszUser, pszPassword, pszDomain, u32ClientId));

    if (   pAuthLibCtx->hAuthLibrary
        && (pAuthLibCtx->pfnAuthEntry || pAuthLibCtx->pfnAuthEntry2 || pAuthLibCtx->pfnAuthEntry3))
    {
        AuthCtx ctx;
        ctx.result         = AuthResultAccessDenied; /* Denied by default. */
        ctx.pfnAuthEntry3  = pAuthLibCtx->pfnAuthEntry3;
        ctx.pfnAuthEntry2  = pAuthLibCtx->pfnAuthEntry2;
        ctx.pfnAuthEntry   = pAuthLibCtx->pfnAuthEntry;
        ctx.pszCaller      = "vrde";
        ctx.pUuid          = &rawuuid;
        ctx.guestJudgement = guestJudgement;
        ctx.pszUser        = pszUser;
        ctx.pszPassword    = pszPassword;
        ctx.pszDomain      = pszDomain;
        ctx.fLogon         = true;
        ctx.clientId       = u32ClientId;

        result = authCall(&ctx);
    }
    else
    {
        LogRelMax(8, ("AUTH: Invalid authentication module context\n"));
        AssertFailed();
    }

    LogFlowFunc(("result = %d\n", result));

    return result;
}

void AuthLibDisconnect(const AUTHLIBRARYCONTEXT *pAuthLibCtx, PCRTUUID pUuid, uint32_t u32ClientId)
{
    AUTHUUID rawuuid;
    memcpy(rawuuid, pUuid, sizeof(rawuuid));

    LogFlowFunc(("pAuthLibCtx = %p, , uuid = %RTuuid, u32ClientId = %d\n",
                 pAuthLibCtx, rawuuid, u32ClientId));

    if (   pAuthLibCtx->hAuthLibrary
        && (pAuthLibCtx->pfnAuthEntry || pAuthLibCtx->pfnAuthEntry2 || pAuthLibCtx->pfnAuthEntry3))
    {
        AuthCtx ctx;
        ctx.result         = AuthResultAccessDenied; /* Not used. */
        ctx.pfnAuthEntry3  = pAuthLibCtx->pfnAuthEntry3;
        ctx.pfnAuthEntry2  = pAuthLibCtx->pfnAuthEntry2;
        ctx.pfnAuthEntry   = NULL;                   /* Does not use disconnect notification. */
        ctx.pszCaller      = "vrde";
        ctx.pUuid          = &rawuuid;
        ctx.guestJudgement = AuthGuestNotAsked;
        ctx.pszUser        = NULL;
        ctx.pszPassword    = NULL;
        ctx.pszDomain      = NULL;
        ctx.fLogon         = false;
        ctx.clientId       = u32ClientId;

        authCall(&ctx);
    }
}
