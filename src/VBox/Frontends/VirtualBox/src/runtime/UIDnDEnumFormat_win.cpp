/* $Id$ */
/** @file
 * VBox Qt GUI -  UIDnDEnumFormat class implementation. This class implements the
 * IEnumFORMATETC ("Format et cetera") interface.
 */

/*
 * Copyright (C) 2014-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>

#include <iprt/win/windows.h>
#include <new> /* For bad_alloc. */

#include "UIDnDEnumFormat_win.h"
#include "UIDnDDataObject_win.h"


UIDnDEnumFormatEtc::UIDnDEnumFormatEtc(FORMATETC *pFormatEtc, ULONG cFormats)
    : m_lRefCount(1),
      m_nIndex(0)
{
    HRESULT hr;

    try
    {
        LogFlowFunc(("pFormatEtc=%p, cFormats=%RU32\n", pFormatEtc, cFormats));
        if (cFormats)
            m_pFormatEtc  = new FORMATETC[cFormats];

        for (ULONG i = 0; i < cFormats; i++)
        {
            LogFlowFunc(("Format %RU32: cfFormat=%RI16, sFormat=%s, tyMed=%RU32, dwAspect=%RU32\n",
                         i, pFormatEtc[i].cfFormat, UIDnDDataObject::ClipboardFormatToString(pFormatEtc[i].cfFormat),
                         pFormatEtc[i].tymed, pFormatEtc[i].dwAspect));
            UIDnDEnumFormatEtc::CopyFormat(&m_pFormatEtc[i], &pFormatEtc[i]);
        }

        m_nNumFormats = cFormats;
        hr = S_OK;
    }
    catch (std::bad_alloc &)
    {
        hr = E_OUTOFMEMORY;
    }

    LogFlowFunc(("hr=%Rhrc\n", hr));
}

UIDnDEnumFormatEtc::~UIDnDEnumFormatEtc(void)
{
    if (m_pFormatEtc)
    {
        for (ULONG i = 0; i < m_nNumFormats; i++)
        {
            if(m_pFormatEtc[i].ptd)
                CoTaskMemFree(m_pFormatEtc[i].ptd);
        }

        delete[] m_pFormatEtc;
        m_pFormatEtc = NULL;
    }

    LogFlowFunc(("m_lRefCount=%RI32\n", m_lRefCount));
}

/*
 * IUnknown methods.
 */

STDMETHODIMP_(ULONG) UIDnDEnumFormatEtc::AddRef(void)
{
    return InterlockedIncrement(&m_lRefCount);
}

STDMETHODIMP_(ULONG) UIDnDEnumFormatEtc::Release(void)
{
    LONG lCount = InterlockedDecrement(&m_lRefCount);
    if (lCount == 0)
    {
        delete this;
        return 0;
    }

    return lCount;
}

STDMETHODIMP UIDnDEnumFormatEtc::QueryInterface(REFIID iid, void **ppvObject)
{
    if (   iid == IID_IEnumFORMATETC
        || iid == IID_IUnknown)
    {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    *ppvObject = 0;
    return E_NOINTERFACE;
}

STDMETHODIMP UIDnDEnumFormatEtc::Next(ULONG cFormats, FORMATETC *pFormatEtc, ULONG *pcFetched)
{
    ULONG ulCopied  = 0;

    if(cFormats == 0 || pFormatEtc == 0)
        return E_INVALIDARG;

    while (   m_nIndex < m_nNumFormats
           && ulCopied < cFormats)
    {
        UIDnDEnumFormatEtc::CopyFormat(&pFormatEtc[ulCopied],
                                         &m_pFormatEtc[m_nIndex]);
        ulCopied++;
        m_nIndex++;
    }

    if (pcFetched)
        *pcFetched = ulCopied;

    return (ulCopied == cFormats) ? S_OK : S_FALSE;
}

STDMETHODIMP UIDnDEnumFormatEtc::Skip(ULONG cFormats)
{
    m_nIndex += cFormats;
    return (m_nIndex <= m_nNumFormats) ? S_OK : S_FALSE;
}

STDMETHODIMP UIDnDEnumFormatEtc::Reset(void)
{
    m_nIndex = 0;
    return S_OK;
}

STDMETHODIMP UIDnDEnumFormatEtc::Clone(IEnumFORMATETC **ppEnumFormatEtc)
{
    HRESULT hResult =
        CreateEnumFormatEtc(m_nNumFormats, m_pFormatEtc, ppEnumFormatEtc);

    if (hResult == S_OK)
        ((UIDnDEnumFormatEtc *) *ppEnumFormatEtc)->m_nIndex = m_nIndex;

    return hResult;
}

/* static */
void UIDnDEnumFormatEtc::CopyFormat(FORMATETC *pDest, FORMATETC *pSource)
{
    AssertPtrReturnVoid(pDest);
    AssertPtrReturnVoid(pSource);

    *pDest = *pSource;

    if (pSource->ptd)
    {
        pDest->ptd = (DVTARGETDEVICE*)CoTaskMemAlloc(sizeof(DVTARGETDEVICE));
        *(pDest->ptd) = *(pSource->ptd);
    }
}

/* static */
HRESULT UIDnDEnumFormatEtc::CreateEnumFormatEtc(UINT nNumFormats, FORMATETC *pFormatEtc, IEnumFORMATETC **ppEnumFormatEtc)
{
    AssertPtrReturn(pFormatEtc, E_INVALIDARG);
    AssertPtrReturn(ppEnumFormatEtc, E_INVALIDARG);

    HRESULT hr;
    try
    {
        *ppEnumFormatEtc = new UIDnDEnumFormatEtc(pFormatEtc, nNumFormats);
        hr = S_OK;
    }
    catch(std::bad_alloc &)
    {
        hr = E_OUTOFMEMORY;
    }

    return hr;
}

