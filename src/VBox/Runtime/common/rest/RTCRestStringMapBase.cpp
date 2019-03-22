/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestStringMapBase implementation.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#include <iprt/cpp/restbase.h>

#include <iprt/err.h>
#include <iprt/string.h>


/**
 * Default destructor.
 */
RTCRestStringMapBase::RTCRestStringMapBase()
    : m_Map(NULL)
    , m_cEntries(0)
{
    RTListInit(&m_ListHead);
}


#if 0 /* trigger link error for now. */
/** Copy constructor. */
RTCRestStringMapBase::RTCRestStringMapBase(RTCRestStringMapBase const &a_rThat);
#endif


/**
 * Destructor.
 */
RTCRestStringMapBase::~RTCRestStringMapBase()
{
    clear();
}



#if 0 /* trigger link error for now. */
/** Copy assignment operator. */
RTCRestStringMapBase &RTCRestStringMapBase::operator=(RTCRestStringMapBase const &a_rThat);
#endif


/*********************************************************************************************************************************
*   Overridden base object methods                                                                                               *
*********************************************************************************************************************************/

void RTCRestStringMapBase::resetToDefault()
{
    /* Default is an empty map. */
    clear();
}


RTCRestOutputBase &RTCRestStringMapBase::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    a_rDst.printf("{\n");
    unsigned const uOldIndent = a_rDst.incrementIndent();

    MapEntry const * const pLast = RTListGetLastCpp(&m_ListHead, MapEntry, ListEntry);
    MapEntry const * pCur;
    RTListForEachCpp(&m_ListHead, pCur, MapEntry, ListEntry)
    {
        a_rDst.printf("%RJs: ", pCur->strKey.c_str());
        pCur->pValue->serializeAsJson(a_rDst);

        if (pCur != pLast)
            a_rDst.printf(",\n");
        else
            a_rDst.printf("\n");
    }

    a_rDst.setIndent(uOldIndent);
    a_rDst.printf("}");
    return a_rDst;
}


int RTCRestStringMapBase::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    /*
     * Make sure the object starts out with an empty map.
     */
    if (m_cEntries > 0)
        clear();

    /*
     * Iterate the object values.
     */
    RTJSONIT hIterator;
    int rcRet = RTJsonIteratorBegin(a_rCursor.m_hValue, &hIterator);
    if (RT_SUCCESS(rcRet))
    {
        for (;;)
        {
            RTCRestJsonCursor SubCursor(a_rCursor);
            int rc = RTJsonIteratorQueryValue(hIterator, &SubCursor.m_hValue, &SubCursor.m_pszName);
            if (RT_SUCCESS(rc))
            {
                RTCRestObjectBase *pObj = createValue();
                if (pObj)
                {
                    rc = pObj->deserializeFromJson(SubCursor);
                    if (RT_SUCCESS(rc))
                    { /* likely */ }
                    else if (RT_SUCCESS(rcRet))
                        rcRet = rc;

                    rc = putWorker(SubCursor.m_pszName, pObj, true /*a_fReplace*/);
                    if (rc == VINF_SUCCESS)
                    { /* likely */ }
                    else if (RT_SUCCESS(rc))
                    {
                        a_rCursor.m_pPrimary->addError(a_rCursor, rc, "warning %Rrc inserting '%s' into map",
                                                       rc, SubCursor.m_pszName);
                        if (rcRet == VINF_SUCCESS)
                            rcRet = rc;
                    }
                    else
                    {
                        rcRet = a_rCursor.m_pPrimary->addError(a_rCursor, rc, "Failed to insert '%s' into map: %Rrc",
                                                               SubCursor.m_pszName, rc);
                        delete pObj;
                    }
                }
                else
                    rcRet = a_rCursor.m_pPrimary->addError(a_rCursor, rc, "Failed to create new value object");
            }
            else
                rcRet = a_rCursor.m_pPrimary->addError(a_rCursor, rc, "RTJsonIteratorQueryValue failed: %Rrc", rc);

            /*
             * Advance.
             */
            rc = RTJsonIteratorNext(hIterator);
            if (RT_SUCCESS(rc))
            { /* likely */ }
            else if (rc == VERR_JSON_ITERATOR_END)
                break;
            else
            {
                rcRet = a_rCursor.m_pPrimary->addError(a_rCursor, rc, "RTJsonIteratorNext failed: %Rrc", rc);
                break;
            }
        }

        RTJsonIteratorFree(hIterator);
    }
    else
        rcRet = a_rCursor.m_pPrimary->addError(a_rCursor, rcRet, "RTJsonIteratorBegin failed: %Rrc", rcRet);
    return rcRet;

}

// later?
//    virtual int RTCRestStringMapBase::toString(RTCString *a_pDst, uint32_t a_fFlags = 0) const ;
//    virtual int RTCRestStringMapBase::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
//                           uint32_t a_fFlags = kCollectionFormat_Unspecified) ;
//


/*********************************************************************************************************************************
*   Generic map methods                                                                                                          *
*********************************************************************************************************************************/

/**
 * @callback_method_impl{FNRTSTRSPACECALLBACK}
 */
/*static*/ DECLCALLBACK(int) RTCRestStringMapBase::stringSpaceDestructorCallback(PRTSTRSPACECORE pStr, void *pvUser)
{
    MapEntry *pNode = (MapEntry *)pStr;
    if (pNode->pValue)
    {
        delete pNode->pValue;
        pNode->pValue = NULL;
    }
    pNode->strKey.setNull();
    delete pNode;

    RT_NOREF(pvUser);
    return VINF_SUCCESS;
}


void RTCRestStringMapBase::clear()
{
    RTStrSpaceDestroy(&m_Map, stringSpaceDestructorCallback, NULL);
    RTListInit(&m_ListHead);
    m_cEntries = 0;
}


size_t RTCRestStringMapBase::size() const
{
    return m_cEntries;
}


bool RTCRestStringMapBase::constainsKey(const char *a_pszKey) const
{
    return RTStrSpaceGet((PRTSTRSPACE)&m_Map, a_pszKey) != NULL;
}


bool RTCRestStringMapBase::constainsKey(RTCString const &a_rStrKey) const
{
    return constainsKey(a_rStrKey.c_str());
}


bool RTCRestStringMapBase::remove(const char *a_pszKey)
{
    PRTSTRSPACECORE pRemoved = RTStrSpaceRemove(&m_Map, a_pszKey);
    if (pRemoved)
    {
        stringSpaceDestructorCallback(pRemoved, NULL);
        return true;
    }
    return false;
}


bool RTCRestStringMapBase::remove(RTCString const &a_rStrKey)
{
    return remove(a_rStrKey.c_str());
}


/*********************************************************************************************************************************
*   Protected methods                                                                                                            *
*********************************************************************************************************************************/

int RTCRestStringMapBase::copyMapWorker(RTCRestStringMapBase const &a_rThat, bool fThrow)
{
    Assert(this != &a_rThat);
    clear();

    MapEntry const *pCur;
    RTListForEachCpp(&a_rThat.m_ListHead, pCur, MapEntry, ListEntry)
    {
        int rc = putCopyWorker(pCur->strKey.c_str(), *pCur->pValue, true /*a_fReplace*/);
        if (RT_SUCCESS(rc))
        { /* likely */ }
        else if (fThrow)
            throw std::bad_alloc();
        else
            return rc;
    }

    return VINF_SUCCESS;
}


int RTCRestStringMapBase::putWorker(const char *a_pszKey, RTCRestObjectBase *a_pValue, bool a_fReplace)
{
    int rc;
    MapEntry *pEntry =  new (std::nothrow) MapEntry;
    if (pEntry)
    {
        rc = pEntry->strKey.assignNoThrow(a_pszKey);
        if (RT_SUCCESS(rc))
        {
            pEntry->Core.pszString = pEntry->strKey.c_str();
            pEntry->Core.cchString = pEntry->strKey.length();
            pEntry->pValue = a_pValue;
            if (RTStrSpaceInsert(&m_Map, &pEntry->Core))
            {
                m_cEntries++;
                return VINF_SUCCESS;
            }

            if (!a_fReplace)
                rc = VERR_ALREADY_EXISTS;
            else
            {
                /* Just replace the pValue in the existing entry. */
                MapEntry *pCollision = (MapEntry *)RTStrSpaceGet(&m_Map, a_pszKey);
                if (pCollision)
                {
                    if (pCollision->pValue)
                        delete pCollision->pValue;
                    pCollision->pValue = a_pValue;
                    pEntry->pValue = NULL; /* paranoia */
                    rc = VWRN_ALREADY_EXISTS;
                }
                else
                    rc = VERR_INTERNAL_ERROR;
            }
        }
        delete pEntry;
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


int RTCRestStringMapBase::putCopyWorker(const char *a_pszKey, RTCRestObjectBase const &a_rValue, bool a_fReplace)
{
    int rc;
    RTCRestObjectBase *pValueCopy = createValueCopy(&a_rValue);
    if (pValueCopy)
    {
        rc = putWorker(a_pszKey, pValueCopy, a_fReplace);
        if (RT_SUCCESS(rc))
        { /* likely */ }
        else
            delete pValueCopy;
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


RTCRestObjectBase *RTCRestStringMapBase::getWorker(const char *a_pszKey)
{
    MapEntry *pHit = (MapEntry *)RTStrSpaceGet(&m_Map, a_pszKey);
    if (pHit)
        return pHit->pValue;
    return NULL;
}


RTCRestObjectBase const *RTCRestStringMapBase::getWorker(const char *a_pszKey) const
{
    MapEntry const *pHit = (MapEntry const *)RTStrSpaceGet((PRTSTRSPACE)&m_Map, a_pszKey);
    if (pHit)
        return pHit->pValue;
    return NULL;
}

