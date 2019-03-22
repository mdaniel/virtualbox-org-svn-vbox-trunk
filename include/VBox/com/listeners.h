/* $Id$ */
/** @file
 * MS COM / XPCOM Abstraction Layer - Listener helpers.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
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

#ifndef VBOX_INCLUDED_com_listeners_h
#define VBOX_INCLUDED_com_listeners_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/com/com.h>
#include <VBox/com/VirtualBox.h>


/** @defgroup grp_com_listeners     Listener Helpers
 * @ingroup grp_com
 * @{
 */


#ifdef VBOX_WITH_XPCOM
# define NS_IMPL_QUERY_HEAD_INLINE() \
NS_IMETHODIMP QueryInterface(REFNSIID aIID, void **aInstancePtr) \
{ \
    NS_ASSERTION(aInstancePtr, "QueryInterface requires a non-NULL destination!"); \
    nsISupports *foundInterface;

# define NS_INTERFACE_MAP_BEGIN_INLINE()      NS_IMPL_QUERY_HEAD_INLINE()

# define NS_IMPL_QUERY_INTERFACE1_INLINE(a_i1) \
    NS_INTERFACE_MAP_BEGIN_INLINE() \
        NS_INTERFACE_MAP_ENTRY(a_i1) \
        NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, a_i1) \
    NS_INTERFACE_MAP_END
#endif

template <class T, class TParam = void *>
class ListenerImpl :
     public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>,
     VBOX_SCRIPTABLE_IMPL(IEventListener)
{
    T* mListener;

#ifdef RT_OS_WINDOWS
    /* FTM stuff */
    ComPtr<IUnknown> m_pUnkMarshaler;
#else
    nsAutoRefCnt mRefCnt;
    NS_DECL_OWNINGTHREAD
#endif

public:
    ListenerImpl()
    {
    }

    virtual ~ListenerImpl()
    {
    }

    HRESULT init(T* aListener, TParam param)
    {
       mListener = aListener;
       return mListener->init(param);
    }

    HRESULT init(T* aListener)
    {
       mListener = aListener;
       return mListener->init();
    }

    void uninit()
    {
       if (mListener)
       {
          mListener->uninit();
          delete mListener;
          mListener = 0;
       }
    }

    HRESULT   FinalConstruct()
    {
#ifdef RT_OS_WINDOWS
       return CoCreateFreeThreadedMarshaler(this, &m_pUnkMarshaler.m_p);
#else
       return S_OK;
#endif
    }

    void   FinalRelease()
    {
      uninit();
#ifdef RT_OS_WINDOWS
      m_pUnkMarshaler.setNull();
#endif
    }

    T* getWrapped()
    {
        return mListener;
    }

    DECLARE_NOT_AGGREGATABLE(ListenerImpl)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

#ifdef RT_OS_WINDOWS
    BEGIN_COM_MAP(ListenerImpl)
        COM_INTERFACE_ENTRY(IEventListener)
        COM_INTERFACE_ENTRY2(IDispatch, IEventListener)
        COM_INTERFACE_ENTRY_AGGREGATE(IID_IMarshal, m_pUnkMarshaler.m_p)
    END_COM_MAP()
#else
    NS_IMETHOD_(nsrefcnt) AddRef(void)
    {
        NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");
        nsrefcnt count;
        count = PR_AtomicIncrement((PRInt32*)&mRefCnt);
        NS_LOG_ADDREF(this, count, "ListenerImpl", sizeof(*this));
        return count;
    }

    NS_IMETHOD_(nsrefcnt) Release(void)
    {
        nsrefcnt count;
        NS_PRECONDITION(0 != mRefCnt, "dup release");
        count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);
        NS_LOG_RELEASE(this, count, "ListenerImpl");
        if (0 == count) {
            mRefCnt = 1; /* stabilize */
            /* enable this to find non-threadsafe destructors: */
            /* NS_ASSERT_OWNINGTHREAD(_class); */
            NS_DELETEXPCOM(this);
            return 0;
        }
        return count;
    }

    NS_IMPL_QUERY_INTERFACE1_INLINE(IEventListener)
#endif


    STDMETHOD(HandleEvent)(IEvent * aEvent)
    {
        VBoxEventType_T aType = VBoxEventType_Invalid;
        aEvent->COMGETTER(Type)(&aType);
        return mListener->HandleEvent(aType, aEvent);
    }
};

#ifdef VBOX_WITH_XPCOM
# define VBOX_LISTENER_DECLARE(klazz) NS_DECL_CLASSINFO(klazz)
#else
# define VBOX_LISTENER_DECLARE(klazz)
#endif

/** @} */
#endif

