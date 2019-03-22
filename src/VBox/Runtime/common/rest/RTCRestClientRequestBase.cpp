/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestClientRequestBase implementation.
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
#define LOG_GROUP RTLOGGROUP_REST
#include <iprt/cpp/restbase.h>

#include <iprt/assert.h>
#include <iprt/err.h>


/**
 * Default constructor.
 */
RTCRestClientRequestBase::RTCRestClientRequestBase()
    : m_fIsSet(0)
    , m_fErrorSet(0)
{
}


/**
 * Copy constructor.
 */
RTCRestClientRequestBase::RTCRestClientRequestBase(RTCRestClientRequestBase const &a_rThat)
    : m_fIsSet(a_rThat.m_fIsSet)
    , m_fErrorSet(a_rThat.m_fErrorSet)
{
}


/**
 * Destructor
 */
RTCRestClientRequestBase::~RTCRestClientRequestBase()
{
    /* nothing to do */
}


/**
 * Copy assignment operator.
 */
RTCRestClientRequestBase &RTCRestClientRequestBase::operator=(RTCRestClientRequestBase const &a_rThat)
{
    m_fIsSet    = a_rThat.m_fIsSet;
    m_fErrorSet = a_rThat.m_fErrorSet;
    return *this;
}


int RTCRestClientRequestBase::doPathParameters(RTCString *a_pStrPath, const char *a_pszPathTemplate, size_t a_cchPathTemplate,
                                               PATHPARAMDESC const *a_paPathParams, PATHPARAMSTATE *a_paPathParamStates,
                                               size_t a_cPathParams) const
{
    int rc = a_pStrPath->assignNoThrow(a_pszPathTemplate, a_cchPathTemplate);
    AssertRCReturn(rc, rc);

    /* Locate the sub-string to replace with values first: */
    for (size_t i = 0; i < a_cPathParams; i++)
    {
        char const *psz = strstr(a_pszPathTemplate, a_paPathParams[i].pszName);
        AssertReturn(psz, VERR_INTERNAL_ERROR_5);
        a_paPathParamStates[i].offName = psz - a_pszPathTemplate;
    }

    /* Replace with actual values: */
    RTCString strTmpVal;
    for (size_t i = 0; i < a_cPathParams; i++)
    {
        AssertReturn(   (a_paPathParams[i].fFlags & RTCRestObjectBase::kCollectionFormat_Mask)
                     != RTCRestObjectBase::kCollectionFormat_multi,
                     VERR_INTERNAL_ERROR_3);
        AssertMsg(m_fIsSet & RT_BIT_64(a_paPathParams[i].iBitNo),
                  ("Path parameter '%s' is not set!\n", a_paPathParams[i].pszName));

        rc = a_paPathParamStates[i].pObj->toString(&strTmpVal, a_paPathParams[i].fFlags);
        AssertRCReturn(rc, rc);

        /* Replace. */
        ssize_t cchAdjust = strTmpVal.length() - a_paPathParams[i].cchName;
        rc = a_pStrPath->replaceNoThrow(a_paPathParamStates[i].offName, a_paPathParams[i].cchName, strTmpVal);
        AssertRCReturn(rc, rc);

        /* Adjust subsequent fields. */
        if (cchAdjust != 0)
            for (size_t j = i + 1; j < a_cPathParams; j++)
                if (a_paPathParamStates[j].offName > a_paPathParamStates[i].offName)
                    a_paPathParamStates[j].offName += cchAdjust;
    }

    return VINF_SUCCESS;
}


int RTCRestClientRequestBase::doQueryParameters(RTCString *a_pStrQuery, QUERYPARAMDESC const *a_paQueryParams,
                                                RTCRestObjectBase const **a_papQueryParamObjs, size_t a_cQueryParams) const
{
    RTCString strTmpVal;
    char chSep = a_pStrQuery->isEmpty() ? '?' : '&';
    for (size_t i = 0; i < a_cQueryParams; i++)
    {
        if (   a_paQueryParams[i].fRequired
            || (m_fIsSet & RT_BIT_64(a_paQueryParams[i].iBitNo)) )
        {
            AssertMsg(m_fIsSet & RT_BIT_64(a_paQueryParams[i].iBitNo),
                      ("Required query parameter '%s' is not set!\n", a_paQueryParams[i].pszName));

            if (   (a_paQueryParams[i].fFlags & RTCRestObjectBase::kCollectionFormat_Mask)
                != RTCRestObjectBase::kCollectionFormat_multi)
            {
                int rc = a_papQueryParamObjs[i]->toString(&strTmpVal, a_paQueryParams[i].fFlags);
                AssertRCReturn(rc, rc);

                rc = a_pStrQuery->appendPrintfNoThrow("%c%RMpq=%RMpq", chSep, a_paQueryParams[i].pszName, strTmpVal.c_str());
                AssertRCReturn(rc, rc);

                chSep = '&';
            }
            else
            {
                /*
                 * Enumerate array and add 'name=element' for each element in it.
                 */
                AssertReturn(a_papQueryParamObjs[i]->typeClass() == RTCRestObjectBase::kTypeClass_Array,
                             VERR_REST_INTERAL_ERROR_2);
                RTCRestArrayBase const *pArray = (RTCRestArrayBase const *)a_papQueryParamObjs[i];
                for (size_t j = 0; j < pArray->size(); j++)
                {
                    RTCRestObjectBase const *pObj = pArray->atBase(j);
                    int rc = pObj->toString(&strTmpVal, a_paQueryParams[i].fFlags & ~RTCRestObjectBase::kCollectionFormat_Mask);
                    AssertRCReturn(rc, rc);

                    rc = a_pStrQuery->appendPrintfNoThrow("%c%RMpq=%RMpq", chSep, a_paQueryParams[i].pszName, strTmpVal.c_str());
                    AssertRCReturn(rc, rc);

                    chSep = '&';
                }
            }
        }
    }
    return VINF_SUCCESS;
}


int RTCRestClientRequestBase::doHeaderParameters(RTHTTP a_hHttp, HEADERPARAMDESC const *a_paHeaderParams,
                                                 RTCRestObjectBase const **a_papHeaderParamObjs, size_t a_cHeaderParams) const
{
    RTCString strTmpVal;
    for (size_t i = 0; i < a_cHeaderParams; i++)
    {
        AssertReturn(   (a_paHeaderParams[i].fFlags & RTCRestObjectBase::kCollectionFormat_Mask)
                     != RTCRestObjectBase::kCollectionFormat_multi,
                     VERR_INTERNAL_ERROR_3);

        if (   a_paHeaderParams[i].fRequired
            || (m_fIsSet & RT_BIT_64(a_paHeaderParams[i].iBitNo)) )
        {
            AssertMsg(m_fIsSet & RT_BIT_64(a_paHeaderParams[i].iBitNo),
                      ("Required header parameter '%s' is not set!\n", a_paHeaderParams[i].pszName));

            if (!a_paHeaderParams[i].fMapCollection)
            {
                int rc = a_papHeaderParamObjs[i]->toString(&strTmpVal, a_paHeaderParams[i].fFlags);
                AssertRCReturn(rc, rc);

                rc = RTHttpAppendHeader(a_hHttp, a_paHeaderParams[i].pszName, strTmpVal.c_str(), 0);
                AssertRCReturn(rc, rc);
            }
            else if (!a_papHeaderParamObjs[i]->isNull())
            {
                /*
                 * Enumerate the map and produce a series of head fields on the form:
                 *      (a_paHeaderParams[i].pszName + key): value.toString()
                 */
                AssertReturn(a_papHeaderParamObjs[i]->typeClass() == RTCRestObjectBase::kTypeClass_StringMap,
                             VERR_REST_INTERAL_ERROR_1);
                RTCRestStringMapBase const *pMap    = (RTCRestStringMapBase const *)a_papHeaderParamObjs[i];
                const size_t                cchName = strlen(a_paHeaderParams[i].pszName);
                RTCString                   strTmpName;
                for (RTCRestStringMapBase::ConstIterator it = pMap->begin(); it != pMap->end(); ++it)
                {
                    int rc = strTmpName.assignNoThrow(a_paHeaderParams[i].pszName, cchName);
                    AssertRCReturn(rc, rc);
                    rc = strTmpName.appendNoThrow(it.getKey());
                    AssertRCReturn(rc, rc);

                    rc = it.getValue()->toString(&strTmpVal, a_paHeaderParams[i].fFlags);
                    AssertRCReturn(rc, rc);

                    rc = RTHttpAppendHeader(a_hHttp, strTmpName.c_str(), strTmpVal.c_str(), 0);
                    AssertRCReturn(rc, rc);
                }
            }
            else
                Assert(!a_paHeaderParams[i].fRequired);
        }
    }
    return VINF_SUCCESS;
}

