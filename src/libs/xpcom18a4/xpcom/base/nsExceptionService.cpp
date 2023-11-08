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
 * ActiveState Tool Corp..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Hammond <MarkH@ActiveState.com>
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

#include "nsISupports.h"
#include "nsExceptionService.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "prthread.h"
#include "prlock.h"

#include <iprt/asm.h>
#include <iprt/errcore.h>

#define CHECK_SERVICE_USE_OK() if (tlsIndex == NIL_RTTLS) return NS_ERROR_NOT_INITIALIZED
#define CHECK_MANAGER_USE_OK() if (!mService || nsExceptionService::tlsIndex == NIL_RTTLS) return NS_ERROR_NOT_INITIALIZED

// A key for our registered module providers hashtable
class nsProviderKey : public nsHashKey {
protected:
  PRUint32 mKey;
public:
  nsProviderKey(PRUint32 key) : mKey(key) {}
  PRUint32 HashCode(void) const {
    return mKey;
  }
  PRBool Equals(const nsHashKey *aKey) const {
    return mKey == ((const nsProviderKey *) aKey)->mKey;
  }
  nsHashKey *Clone() const {
    return new nsProviderKey(mKey);
  }
  PRUint32 GetValue() { return mKey; }
};

/** Exception Manager definition **/
class nsExceptionManager : public nsIExceptionManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXCEPTIONMANAGER

  nsExceptionManager(nsExceptionService *svc);
  /* additional members */
  nsCOMPtr<nsIException> mCurrentException;
  nsExceptionManager *mNextThread; // not ref-counted.
  nsExceptionService *mService; // not ref-counted
#ifdef NS_DEBUG
  static volatile uint32_t totalInstances;
#endif

#ifdef NS_DEBUG
  inline nsrefcnt ReleaseQuiet() {
    // shut up NS_ASSERT_OWNINGTHREAD (see explanation below)
    nsAutoOwningThread old = _mOwningThread;
    _mOwningThread = nsAutoOwningThread();
    nsrefcnt ref = Release();
    NS_ASSERTION(ref == 0, "the object is still referenced by other threads while it shouldn't");
    if (ref != 0)
      _mOwningThread = old;
    return ref;
  }
#else  
  inline nsrefcnt ReleaseQuiet(void) { return Release(); }
#endif

private:
  ~nsExceptionManager();
};


#ifdef NS_DEBUG
volatile uint32_t nsExceptionManager::totalInstances = 0;
#endif

// Note: the nsExceptionManager object is single threaded - the exception
// service itself ensures one per thread. However, there are two methods that
// may be called on foreign threads: DropAllThreads (called on the thread
// shutting down xpcom) and ThreadDestruct (called after xpcom destroyed the
// internal thread struct, so that PR_GetCurrentThread() will create a new one
// from scratch which will obviously not match the old one stored in the
// instance on creation). In both cases, there should be no other threads
// holding objects (i.e. it's thread-safe to call them), but
// NS_CheckThreadSafe() assertions will still happen and yell in the debug
// build. Since it is quite annoying, we use a special ReleaseQuiet() method
// in DoDropThread() to shut them up.
NS_IMPL_THREADSAFE_ISUPPORTS1(nsExceptionManager, nsIExceptionManager)

nsExceptionManager::nsExceptionManager(nsExceptionService *svc) :
  mNextThread(nsnull),
  mService(svc)
{
  /* member initializers and constructor code */
#ifdef NS_DEBUG
  ASMAtomicIncU32(&totalInstances);
#endif
}

nsExceptionManager::~nsExceptionManager()
{
  /* destructor code */
#ifdef NS_DEBUG
  ASMAtomicDecU32(&totalInstances);
#endif // NS_DEBUG
}

/* void setCurrentException (in nsIException error); */
NS_IMETHODIMP nsExceptionManager::SetCurrentException(nsIException *error)
{
    CHECK_MANAGER_USE_OK();
    mCurrentException = error;
    return NS_OK;
}

/* nsIException getCurrentException (); */
NS_IMETHODIMP nsExceptionManager::GetCurrentException(nsIException **_retval)
{
    CHECK_MANAGER_USE_OK();
    *_retval = mCurrentException;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}

/* nsIException getExceptionFromProvider( in nsresult rc, in nsIException defaultException); */
NS_IMETHODIMP nsExceptionManager::GetExceptionFromProvider(nsresult rc, nsIException * defaultException, nsIException **_retval)
{
    CHECK_MANAGER_USE_OK();
    // Just delegate back to the service with the provider map.
    return mService->GetExceptionFromProvider(rc, defaultException, _retval);
}

/* The Exception Service */

RTTLS nsExceptionService::tlsIndex = NIL_RTTLS;
RTSEMFASTMUTEX nsExceptionService::lock = NIL_RTSEMFASTMUTEX;
nsExceptionManager *nsExceptionService::firstThread = nsnull;

#ifdef NS_DEBUG
volatile uint32_t nsExceptionService::totalInstances = 0;
#endif

NS_IMPL_THREADSAFE_ISUPPORTS2(nsExceptionService, nsIExceptionService, nsIObserver)

nsExceptionService::nsExceptionService()
  : mProviders(4, PR_TRUE) /* small, thread-safe hashtable */
{
#ifdef NS_DEBUG
  if (ASMAtomicIncU32(&totalInstances)!=1) {
    NS_ERROR("The nsExceptionService is a singleton!");
  }
#endif
  /* member initializers and constructor code */
  if (tlsIndex == NIL_RTTLS) {
    int vrc = RTTlsAllocEx( &tlsIndex, ThreadDestruct );
    NS_WARN_IF_FALSE(RT_SUCCESS(vrc), "ScriptErrorService could not allocate TLS storage.");
  }
  int vrc = RTSemFastMutexCreate(&lock);
  NS_WARN_IF_FALSE(RT_SUCCESS(vrc), "Error allocating ExceptionService lock");

  // observe XPCOM shutdown.
  nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1");
  NS_WARN_IF_FALSE(observerService, "Could not get observer service!");
  if (observerService)
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
}

nsExceptionService::~nsExceptionService()
{
  Shutdown();
  if (lock != NIL_RTSEMFASTMUTEX) {
    RTSEMFASTMUTEX tmp = lock;
    lock = NULL;
    RTSemFastMutexDestroy(tmp);
  }
  /* destructor code */
#ifdef NS_DEBUG
  ASMAtomicDecU32(&totalInstances);
#endif
}

/*static*/
DECLCALLBACK(void) nsExceptionService::ThreadDestruct( void *data )
{
  if (lock == NIL_RTSEMFASTMUTEX) {
    NS_WARNING("nsExceptionService ignoring thread destruction after shutdown");
    return;
  }
  DropThread( (nsExceptionManager *)data );
}


void nsExceptionService::Shutdown()
{
  RTTLS tmp = tlsIndex;
  tlsIndex = NIL_RTTLS;
  RTTlsSet(tmp, NULL);
  mProviders.Reset();
  if (lock != NIL_RTSEMFASTMUTEX) {
    DropAllThreads();
  }
}

/* void setCurrentException (in nsIException error); */
NS_IMETHODIMP nsExceptionService::SetCurrentException(nsIException *error)
{
    CHECK_SERVICE_USE_OK();
    nsCOMPtr<nsIExceptionManager> sm;
    nsresult nr = GetCurrentExceptionManager(getter_AddRefs(sm));
    if (NS_FAILED(nr))
        return nr;
    return sm->SetCurrentException(error);
}

/* nsIException getCurrentException (); */
NS_IMETHODIMP nsExceptionService::GetCurrentException(nsIException **_retval)
{
    CHECK_SERVICE_USE_OK();
    nsCOMPtr<nsIExceptionManager> sm;
    nsresult nr = GetCurrentExceptionManager(getter_AddRefs(sm));
    if (NS_FAILED(nr))
        return nr;
    return sm->GetCurrentException(_retval);
}

/* nsIException getExceptionFromProvider( in nsresult rc, in nsIException defaultException); */
NS_IMETHODIMP nsExceptionService::GetExceptionFromProvider(nsresult rc, 
    nsIException * defaultException, nsIException **_retval)
{
    CHECK_SERVICE_USE_OK();
    return DoGetExceptionFromProvider(rc, defaultException, _retval);
}

/* readonly attribute nsIExceptionManager currentExceptionManager; */
NS_IMETHODIMP nsExceptionService::GetCurrentExceptionManager(nsIExceptionManager * *aCurrentScriptManager)
{
    CHECK_SERVICE_USE_OK();
    nsExceptionManager *mgr = (nsExceptionManager *)RTTlsGet(tlsIndex);
    if (mgr == nsnull) {
        // Stick the new exception object in with no reference count.
        mgr = new nsExceptionManager(this);
        if (mgr == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        RTTlsSet(tlsIndex, mgr);
        // The reference count is held in the thread-list
        AddThread(mgr);
    }
    *aCurrentScriptManager = mgr;
    NS_ADDREF(*aCurrentScriptManager);
    return NS_OK;
}

/* void registerErrorProvider (in nsIExceptionProvider provider, in PRUint32 moduleCode); */
NS_IMETHODIMP nsExceptionService::RegisterExceptionProvider(nsIExceptionProvider *provider, PRUint32 errorModule)
{
    CHECK_SERVICE_USE_OK();

    nsProviderKey key(errorModule);
    if (mProviders.Put(&key, provider)) {
        NS_WARNING("Registration of exception provider overwrote another provider with the same module code!");
    }
    return NS_OK;
}

/* void unregisterErrorProvider (in nsIExceptionProvider provider, in PRUint32 errorModule); */
NS_IMETHODIMP nsExceptionService::UnregisterExceptionProvider(nsIExceptionProvider *provider, PRUint32 errorModule)
{
    CHECK_SERVICE_USE_OK();
    nsProviderKey key(errorModule);
    if (!mProviders.Remove(&key)) {
        NS_WARNING("Attempt to unregister an unregistered exception provider!");
        return NS_ERROR_UNEXPECTED;
    }
    return NS_OK;
}

// nsIObserver
NS_IMETHODIMP nsExceptionService::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
     Shutdown();
     return NS_OK;
}

nsresult
nsExceptionService::DoGetExceptionFromProvider(nsresult errCode, 
                                               nsIException * defaultException,
                                               nsIException **_exc)
{
    // Check for an existing exception
    nsresult nr = GetCurrentException(_exc);
    if (NS_SUCCEEDED(nr) && *_exc) {
        (*_exc)->GetResult(&nr);
        // If it matches our result then use it
        if (nr == errCode)
            return NS_OK;
        NS_RELEASE(*_exc);
    }
    nsProviderKey key(NS_ERROR_GET_MODULE(errCode));
    nsCOMPtr<nsIExceptionProvider> provider =
        dont_AddRef((nsIExceptionProvider *)mProviders.Get(&key));

    // No provider so we'll return the default exception
    if (!provider) {
        *_exc = defaultException;
        NS_IF_ADDREF(*_exc);
        return NS_OK;
    }

    return provider->GetException(errCode, defaultException, _exc);
}

// thread management
/*static*/ void nsExceptionService::AddThread(nsExceptionManager *thread)
{
    RTSemFastMutexRequest(lock);
    thread->mNextThread = firstThread;
    firstThread = thread;
    NS_ADDREF(thread);
    RTSemFastMutexRelease(lock);
}

/*static*/ void nsExceptionService::DoDropThread(nsExceptionManager *thread)
{
    nsExceptionManager **emp = &firstThread;
    while (*emp != thread) {
        if (!*emp)
        {
            NS_WARNING("Could not find the thread to drop!");
            return;
        }
        emp = &(*emp)->mNextThread;
    }
    *emp = thread->mNextThread;
    thread->ReleaseQuiet();
    thread = nsnull;
}

/*static*/ void nsExceptionService::DropThread(nsExceptionManager *thread)
{
    RTSemFastMutexRequest(lock);
    DoDropThread(thread);
    RTSemFastMutexRelease(lock);
}

/*static*/ void nsExceptionService::DropAllThreads()
{
    RTSemFastMutexRequest(lock);
    while (firstThread)
        DoDropThread(firstThread);
    RTSemFastMutexRelease(lock);
}
