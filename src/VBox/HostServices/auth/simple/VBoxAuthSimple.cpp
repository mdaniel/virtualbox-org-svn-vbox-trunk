/* $Id$ */
/** @file
 * VirtualBox External Authentication Library - Simple Authentication.
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
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <iprt/cdefs.h>
#include <iprt/uuid.h>
#include <iprt/sha.h>

#include <VBox/VBoxAuth.h>

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/VirtualBox.h>

using namespace com;

/* If defined, debug messages will be written to the specified file. */
//#define AUTH_DEBUG_FILE_NAME "/tmp/VBoxAuth.log"


static void dprintf(const char *pszFormat, ...)
{
#ifdef AUTH_DEBUG_FILE_NAME
    FILE *f = fopen(AUTH_DEBUG_FILE_NAME, "ab");
    if (f)
    {
        va_list va;
        va_start(va, pszFormat);
        vfprintf(f, pszFormat, va);
        va_end(va);
        fclose(f);
    }
#else
    RT_NOREF(pszFormat);
#endif
}

RT_C_DECLS_BEGIN
DECLEXPORT(FNAUTHENTRY3) AuthEntry;
RT_C_DECLS_END

DECLEXPORT(AuthResult) AUTHCALL AuthEntry(const char *pszCaller,
                                          PAUTHUUID pUuid,
                                          AuthGuestJudgement guestJudgement,
                                          const char *pszUser,
                                          const char *pszPassword,
                                          const char *pszDomain,
                                          int fLogon,
                                          unsigned clientId)
{
    RT_NOREF(pszCaller, guestJudgement, pszDomain, clientId);

    /* default is failed */
    AuthResult result = AuthResultAccessDenied;

    /* only interested in logon */
    if (!fLogon)
        /* return value ignored */
        return result;

    char uuid[RTUUID_STR_LENGTH] = {0};
    if (pUuid)
        RTUuidToStr((PCRTUUID)pUuid, (char*)uuid, RTUUID_STR_LENGTH);

    /* the user might contain a domain name, split it */
    const char *user = strchr(pszUser, '\\');
    if (user)
        user++;
    else
        user = (char*)pszUser;

    dprintf("VBoxAuth: uuid: %s, user: %s, pszPassword: %s\n", uuid, user, pszPassword);

    ComPtr<IVirtualBoxClient> virtualBoxClient;
    ComPtr<IVirtualBox> virtualBox;
    HRESULT rc;

    rc = virtualBoxClient.createInprocObject(CLSID_VirtualBoxClient);
    if (SUCCEEDED(rc))
    {
        rc = virtualBoxClient->COMGETTER(VirtualBox)(virtualBox.asOutParam());
        if (SUCCEEDED(rc))
        {
            Bstr key = BstrFmt("VBoxAuthSimple/users/%s", user);
            Bstr password;

            /* lookup in VM's extra data? */
            if (pUuid)
            {
                ComPtr<IMachine> machine;
                virtualBox->FindMachine(Bstr(uuid).raw(), machine.asOutParam());
                if (machine)
                    machine->GetExtraData(key.raw(), password.asOutParam());
            }
            else
                /* lookup global extra data */
                virtualBox->GetExtraData(key.raw(), password.asOutParam());

            if (!password.isEmpty())
            {
                /* calculate hash */
                uint8_t abDigest[RTSHA256_HASH_SIZE];
                RTSha256(pszPassword, strlen(pszPassword), abDigest);
                char pszDigest[RTSHA256_DIGEST_LEN + 1];
                RTSha256ToString(abDigest, pszDigest, sizeof(pszDigest));

                if (password == pszDigest)
                    result = AuthResultAccessGranted;
            }
        }
        else
            dprintf("VBoxAuth: failed to get VirtualBox object reference: %#x\n", rc);
    }
    else
        dprintf("VBoxAuth: failed to get VirtualBoxClient object reference: %#x\n", rc);

    return result;
}

