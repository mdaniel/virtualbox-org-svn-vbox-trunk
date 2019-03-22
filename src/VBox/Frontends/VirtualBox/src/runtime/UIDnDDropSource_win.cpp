/* $Id$ */
/** @file
 * VBox Qt GUI - UIDnDDropSource class implementation for Windows. This implements
 * the IDropSource interface.
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

#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/thread.h>

/* Qt includes: */
#include <QApplication>

/* Windows includes: */
#include <QApplication>
#include <iprt/win/windows.h>
#include <new> /* For bad_alloc. */

#include "UIDnDDropSource_win.h"
#include "UIDnDDataObject_win.h"

UIDnDDropSource::UIDnDDropSource(QWidget *pParent, UIDnDDataObject *pDataObject)
    : m_cRefCount(1)
    , m_pParent(pParent)
    , m_pDataObject(pDataObject)
    , m_dwCurEffect(DROPEFFECT_NONE)
    , m_uCurAction(Qt::IgnoreAction)
{
    LogFlowFunc(("pParent=0x%p\n", m_pParent));
}

UIDnDDropSource::~UIDnDDropSource(void)
{
    LogFlowFunc(("mRefCount=%RU32\n", m_cRefCount));
}

/*
 * IUnknown methods.
 */

STDMETHODIMP_(ULONG) UIDnDDropSource::AddRef(void)
{
    LogFlowFunc(("mRefCount=%RU32\n", m_cRefCount + 1));
    return (ULONG)InterlockedIncrement(&m_cRefCount);
}

STDMETHODIMP_(ULONG) UIDnDDropSource::Release(void)
{
    Assert(m_cRefCount > 0);
    LogFlowFunc(("mRefCount=%RU32\n", m_cRefCount - 1));
    LONG lCount = InterlockedDecrement(&m_cRefCount);
    if (lCount <= 0)
    {
        delete this;
        return 0;
    }

    return (ULONG)lCount;
}

STDMETHODIMP UIDnDDropSource::QueryInterface(REFIID iid, void **ppvObject)
{
    AssertPtrReturn(ppvObject, E_INVALIDARG);

    if (   iid == IID_IDropSource
        || iid == IID_IUnknown)
    {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    *ppvObject = 0;
    return E_NOINTERFACE;
}

/*
 * IDropSource methods.
 */

/**
 * The system informs us about whether we should continue the drag'n drop
 * operation or not, depending on the sent key states.
 *
 * @return  HRESULT
 */
STDMETHODIMP UIDnDDropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD dwKeyState)
{
    LogFlowFunc(("fEscapePressed=%RTbool, dwKeyState=0x%x, m_dwCurEffect=%RI32, m_uCurAction=%RU32\n",
                 RT_BOOL(fEscapePressed), dwKeyState, m_dwCurEffect, m_uCurAction));

    /* ESC pressed? Bail out. */
    if (fEscapePressed)
    {
        m_dwCurEffect = DROPEFFECT_NONE;
        m_uCurAction  = Qt::IgnoreAction;

        LogRel2(("DnD: User cancelled dropping data to the host\n"));
        return DRAGDROP_S_CANCEL;
    }

    bool fDropContent = false;

    /* Left mouse button released? Start "drop" action. */
    if ((dwKeyState & MK_LBUTTON) == 0)
        fDropContent = true;
    /** @todo Make this configurable? */

    if (fDropContent)
    {
        if (m_pDataObject)
            m_pDataObject->Signal();

        LogRel2(("DnD: User dropped data to the host\n"));
        return DRAGDROP_S_DROP;
    }

    QApplication::processEvents();

    /* No change, just continue. */
    return S_OK;
}

/**
 * The drop target gives our source feedback about whether
 * it can handle our data or not.
 *
 * @return  HRESULT
 */
STDMETHODIMP UIDnDDropSource::GiveFeedback(DWORD dwEffect)
{
    Qt::DropActions dropActions = Qt::IgnoreAction;

    LogFlowFunc(("dwEffect=0x%x\n", dwEffect));
    if (dwEffect)
    {
        if (dwEffect & DROPEFFECT_COPY)
            dropActions |= Qt::CopyAction;
        if (dwEffect & DROPEFFECT_MOVE)
            dropActions |= Qt::MoveAction;
        if (dwEffect & DROPEFFECT_LINK)
            dropActions |= Qt::LinkAction;

        m_dwCurEffect = dwEffect;
    }

    m_uCurAction  = dropActions;

    return DRAGDROP_S_USEDEFAULTCURSORS;
}

