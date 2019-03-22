/* $Id$ */
/** @file
 * Main - Secret key interface.
 */

/*
 * Copyright (C) 2015-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/memsafer.h>

#include "SecretKeyStore.h"

SecretKey::SecretKey(const uint8_t *pbKey, size_t cbKey, bool fKeyBufNonPageable)
{
    m_cRefs            = 0;
    m_fRemoveOnSuspend = false;
    m_cUsers           = 0;
    m_cbKey            = cbKey;

    int rc = RTMemSaferAllocZEx((void **)&this->m_pbKey, cbKey,
                                fKeyBufNonPageable ? RTMEMSAFER_F_REQUIRE_NOT_PAGABLE : 0);
    if (RT_SUCCESS(rc))
    {
        memcpy(this->m_pbKey, pbKey, cbKey);

        /* Scramble content to make retrieving the key more difficult. */
        rc = RTMemSaferScramble(this->m_pbKey, cbKey);
    }
    else
        throw rc;
}

SecretKey::~SecretKey()
{
    Assert(!m_cRefs);

    RTMemSaferFree(m_pbKey, m_cbKey);
    m_cRefs = 0;
    m_pbKey = NULL;
    m_cbKey = 0;
    m_fRemoveOnSuspend = false;
    m_cUsers = 0;
}

uint32_t SecretKey::retain()
{
    uint32_t cRefs = ASMAtomicIncU32(&m_cRefs);
    if (cRefs == 1)
    {
        int rc = RTMemSaferUnscramble(m_pbKey, m_cbKey);
        AssertRC(rc);
    }

    return cRefs;
}

uint32_t SecretKey::release()
{
    uint32_t cRefs = ASMAtomicDecU32(&m_cRefs);
    if (!cRefs)
    {
        int rc = RTMemSaferScramble(m_pbKey, m_cbKey);
        AssertRC(rc);
    }

    return cRefs;
}

uint32_t SecretKey::refCount()
{
    return m_cRefs;
}

int SecretKey::setUsers(uint32_t cUsers)
{
    m_cUsers = cUsers;
    return VINF_SUCCESS;
}

uint32_t SecretKey::getUsers()
{
    return m_cUsers;
}

int SecretKey::setRemoveOnSuspend(bool fRemoveOnSuspend)
{
    m_fRemoveOnSuspend = fRemoveOnSuspend;
    return VINF_SUCCESS;
}

bool SecretKey::getRemoveOnSuspend()
{
    return m_fRemoveOnSuspend;
}

const void *SecretKey::getKeyBuffer()
{
    AssertReturn(m_cRefs > 0, NULL);
    return m_pbKey;
}

size_t SecretKey::getKeySize()
{
    return m_cbKey;
}

SecretKeyStore::SecretKeyStore(bool fKeyBufNonPageable)
{
    m_fKeyBufNonPageable = fKeyBufNonPageable;
}

SecretKeyStore::~SecretKeyStore()
{
    int rc = deleteAllSecretKeys(false /* fSuspend */, true /* fForce */);
    AssertRC(rc);
}

int SecretKeyStore::addSecretKey(const com::Utf8Str &strKeyId, const uint8_t *pbKey, size_t cbKey)
{
    /* Check that the ID is not existing already. */
    SecretKeyMap::const_iterator it = m_mapSecretKeys.find(strKeyId);
    if (it != m_mapSecretKeys.end())
        return VERR_ALREADY_EXISTS;

    try
    {
        SecretKey *pKey = new SecretKey(pbKey, cbKey, m_fKeyBufNonPageable);

        m_mapSecretKeys.insert(std::make_pair(strKeyId, pKey));
    }
    catch (int rc)
    {
        return rc;
    }

    return VINF_SUCCESS;
}

int SecretKeyStore::deleteSecretKey(const com::Utf8Str &strKeyId)
{
    SecretKeyMap::iterator it = m_mapSecretKeys.find(strKeyId);
    if (it == m_mapSecretKeys.end())
        return VERR_NOT_FOUND;

    SecretKey *pKey = it->second;
    if (pKey->refCount() != 0)
        return VERR_RESOURCE_IN_USE;

    m_mapSecretKeys.erase(it);
    delete pKey;

    return VINF_SUCCESS;
}

int SecretKeyStore::retainSecretKey(const com::Utf8Str &strKeyId, SecretKey **ppKey)
{
    SecretKeyMap::const_iterator it = m_mapSecretKeys.find(strKeyId);
    if (it == m_mapSecretKeys.end())
        return VERR_NOT_FOUND;

    SecretKey *pKey = it->second;
    pKey->retain();

    *ppKey = pKey;

    return VINF_SUCCESS;
}

int SecretKeyStore::releaseSecretKey(const com::Utf8Str &strKeyId)
{
    SecretKeyMap::const_iterator it = m_mapSecretKeys.find(strKeyId);
    if (it == m_mapSecretKeys.end())
        return VERR_NOT_FOUND;

    SecretKey *pKey = it->second;
    pKey->release();
    return VINF_SUCCESS;
}

int SecretKeyStore::deleteAllSecretKeys(bool fSuspend, bool fForce)
{
    /* First check whether a key is still in use. */
    if (!fForce)
    {
        for (SecretKeyMap::iterator it = m_mapSecretKeys.begin();
             it != m_mapSecretKeys.end();
             ++it)
        {
            SecretKey *pKey = it->second;
            if (   pKey->refCount()
                && (   (   pKey->getRemoveOnSuspend()
                        && fSuspend)
                    || !fSuspend))
                return VERR_RESOURCE_IN_USE;
        }
    }

    SecretKeyMap::iterator it = m_mapSecretKeys.begin();
    while (it != m_mapSecretKeys.end())
    {
        SecretKey *pKey = it->second;
        if (   pKey->getRemoveOnSuspend()
            || !fSuspend)
        {
            AssertMsg(!pKey->refCount(), ("No one should access the stored key at this point anymore!\n"));
            delete pKey;
            SecretKeyMap::iterator itNext = it;
            itNext++;
            m_mapSecretKeys.erase(it);
            it = itNext;
        }
        else
            ++it;
    }

    return VINF_SUCCESS;
}

