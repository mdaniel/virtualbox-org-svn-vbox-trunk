/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define NS_WEAK_OBSERVERS

#include "nsString.h"
#include "nsAutoLock.h"
#include "nsCOMPtr.h"
#include "nsIWeakReference.h"
#include "nsEnumeratorUtils.h"
#include "nsObserverList.h"

nsObserverList::nsObserverList()
{
    MOZ_COUNT_CTOR(nsObserverList);
    int vrc = RTSemFastMutexCreate(&m_hLock);
    AssertRC(vrc); RT_NOREF(vrc);
}

nsObserverList::~nsObserverList(void)
{
    MOZ_COUNT_DTOR(nsObserverList);
    int vrc = RTSemFastMutexDestroy(m_hLock);
    AssertRC(vrc); RT_NOREF(vrc);
}

nsresult
nsObserverList::AddObserver(nsIObserver* anObserver, PRBool ownsWeak)
{
    nsresult rv;
    PRBool inserted;
    
    NS_ENSURE_ARG(anObserver);

    nsAutoLock lock(m_hLock);

    if (!mObserverList) {
        rv = NS_NewISupportsArray(getter_AddRefs(mObserverList));
        if (NS_FAILED(rv)) return rv;
    }

#ifdef NS_WEAK_OBSERVERS
    nsCOMPtr<nsISupports> observerRef;
    if (ownsWeak) {
        nsCOMPtr<nsISupportsWeakReference> weakRefFactory = do_QueryInterface(anObserver);
        NS_ASSERTION(weakRefFactory, "AddObserver: trying weak object that doesnt support nsIWeakReference");
        if ( weakRefFactory )
            observerRef = getter_AddRefs(NS_STATIC_CAST(nsISupports*, NS_GetWeakReference(weakRefFactory)));
    } else {
#if DEBUG_dougt_xxx
        // if you are hitting this assertion, contact dougt@netscape.com.  There may be a ownership problem caused by his checkin to freeze nsIObserver
        nsCOMPtr<nsISupportsWeakReference> weakRefFactory = do_QueryInterface(anObserver);
        NS_ASSERTION(!weakRefFactory, "Your object supports weak references, but is being added with a strong reference");
#endif
        observerRef = anObserver;
    }
    if (!observerRef)
        return NS_ERROR_FAILURE;

    inserted = mObserverList->AppendElement(observerRef); 
#else
    if (*anObserver)
        inserted = mObserverList->AppendElement(*anObserver);
#endif
    return inserted ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsObserverList::RemoveObserver(nsIObserver* anObserver)
{
    PRBool removed = PR_FALSE;
    
    NS_ENSURE_ARG(anObserver);

    nsAutoLock lock(m_hLock);

    if (!mObserverList)
       return NS_ERROR_FAILURE;
    
#ifdef NS_WEAK_OBSERVERS
    nsCOMPtr<nsISupportsWeakReference> weakRefFactory = do_QueryInterface(anObserver);
    nsCOMPtr<nsISupports> observerRef;
    if (weakRefFactory) {
        observerRef = getter_AddRefs(NS_STATIC_CAST(nsISupports*, NS_GetWeakReference(weakRefFactory)));
        if (observerRef)
            removed = mObserverList->RemoveElement(observerRef);
        if (!removed)
            observerRef = anObserver;
    } else
        observerRef = anObserver;

    if (!removed && observerRef)
        removed = mObserverList->RemoveElement(observerRef);  
#else
    if (*anObserver)
        removed = mObserverList->RemoveElement(*anObserver);
#endif
    return removed ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsObserverList::GetObserverList(nsISimpleEnumerator** anEnumerator)
{
    nsAutoLock lock(m_hLock);

    ObserverListEnumerator * enumerator= new ObserverListEnumerator(mObserverList);
    *anEnumerator = enumerator;
    if (!enumerator)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(enumerator);
    return NS_OK;
}


ObserverListEnumerator::ObserverListEnumerator(nsISupportsArray* aValueArray)
    : mValueArray(aValueArray), mIndex(0)
{
    if (mValueArray) {
        NS_ADDREF(mValueArray);
        PRUint32 total;
        mValueArray->Count(&total);
        mIndex = PRInt32(total);
    }
}

ObserverListEnumerator::~ObserverListEnumerator(void)
{
    NS_IF_RELEASE(mValueArray);
}

NS_IMPL_ISUPPORTS1(ObserverListEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
ObserverListEnumerator::HasMoreElements(PRBool* aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (!mValueArray) {
        *aResult = PR_FALSE;
        return NS_OK;
    }

    *aResult = (mIndex > 0);
    return NS_OK;
}

NS_IMETHODIMP
ObserverListEnumerator::GetNext(nsISupports** aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (!mValueArray) {
        *aResult = nsnull;
        return NS_OK;
    }

    if (mIndex <= 0 )
        return NS_ERROR_UNEXPECTED;

    mValueArray->GetElementAt(--mIndex, aResult);
    if (*aResult) {
        nsCOMPtr<nsIWeakReference> weakRefFactory = do_QueryInterface(*aResult);
        if ( weakRefFactory ) {
            nsCOMPtr<nsISupports> weakref = do_QueryReferent(weakRefFactory);
            NS_RELEASE(*aResult);
            NS_IF_ADDREF(*aResult = weakref);

            return NS_OK;
        }
    }

    return NS_OK;
}
