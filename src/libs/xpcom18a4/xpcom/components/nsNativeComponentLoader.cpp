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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
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
 * ***** END LICENSE BLOCK *****
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      Added PR_CALLBACK for Optlink use in OS2
 */

#include "prmem.h"
#include "nsNativeComponentLoader.h"
#include "nsComponentManager.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIModule.h"
#include "xcDll.h"
#include "nsHashtable.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"
#include "nsIObserverService.h"

#include <iprt/assert.h>
#include <VBox/log.h>

static PRBool PR_CALLBACK
DLLStore_Destroy(nsHashKey *aKey, void *aData, void* closure)
{
    nsDll* entry = NS_STATIC_CAST(nsDll*, aData);
    delete entry;
    return PR_TRUE;
}

nsNativeComponentLoader::nsNativeComponentLoader() :
    mCompMgr(nsnull),
    mLoadedDependentLibs(16, PR_TRUE),
    mDllStore(nsnull, nsnull, DLLStore_Destroy,
              nsnull, 256, PR_TRUE)
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsNativeComponentLoader,
                              nsIComponentLoader,
                              nsINativeComponentLoader)

NS_IMETHODIMP
nsNativeComponentLoader::GetFactory(const nsIID & aCID,
                                    const char *aLocation,
                                    const char *aType,
                                    nsIFactory **_retval)
{
    nsresult rv;

    if (!_retval)
        return NS_ERROR_NULL_POINTER;

    /* use a hashtable of WeakRefs to store the factory object? */

    /* Should this all live in xcDll? */
    nsDll *dll;
    rv = CreateDll(nsnull, aLocation, &dll);
    if (NS_FAILED(rv))
        return rv;

    if (!dll)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!dll->IsLoaded()) {
#ifdef LOG_ENABLED
        nsXPIDLCString displayPath;
        dll->GetDisplayPath(displayPath);

        Log(("nsNativeComponentLoader: loading \"%s\"\n", displayPath.get()));
#endif
        if (!dll->Load()) {

            Log(("nsNativeComponentLoader: load FAILED\n"));

            /** @todo r=aeichner Get error information from RTLdr. */
            char errorMsg[1024] = "<unknown; can't get error from NSPR>";
            DumpLoadError(dll, "GetFactory", errorMsg);
            return NS_ERROR_FAILURE;
        }
    }

    /* Get service manager for factory */
    nsCOMPtr<nsIServiceManager> serviceMgr;
    rv = NS_GetServiceManager(getter_AddRefs(serviceMgr));
    if (NS_FAILED(rv))
        return rv;      // XXX translate error code?

    rv = GetFactoryFromModule(dll, aCID, _retval);

    Log(("nsNativeComponentLoader: Factory creation %s for %s",
         (NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"),
         aLocation));

    // If the dll failed to get us a factory. But the dll registered that
    // it would be able to create a factory for this CID. mmh!
    // We cannot just delete the dll as the dll could be hosting
    // other CID for which factory creation can pass.
    // We will just let it be. The effect will be next time we try
    // creating the object, we will query the dll again. Since the
    // dll is loaded, this aint a big hit. So for optimized builds
    // this is ok to limp along.
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Factory creation failed");

    return rv;
}

NS_IMETHODIMP
nsNativeComponentLoader::Init(nsIComponentManager *aCompMgr, nsISupports *aReg)
{
    mCompMgr = aCompMgr;
    if (!mCompMgr)
        return NS_ERROR_INVALID_ARG;

    return NS_OK;
}

NS_IMETHODIMP
nsNativeComponentLoader::AutoRegisterComponents(PRInt32 aWhen,
                                                nsIFile *aDirectory)
{
    Log(("nsNativeComponentLoader: autoregistering begins.\n"));

    nsresult rv = RegisterComponentsInDir(aWhen, aDirectory);

    Log(("nsNativeComponentLoader: autoregistering %s\n",
         NS_FAILED(rv) ? "FAILED" : "succeeded"));
    return rv;
}

nsresult
nsNativeComponentLoader::RegisterComponentsInDir(PRInt32 when,
                                                 nsIFile *dir)
{
    nsresult rv = NS_ERROR_FAILURE;
    PRBool isDir = PR_FALSE;

    // Create a directory iterator
    nsCOMPtr<nsISimpleEnumerator> dirIterator;
    rv = dir->GetDirectoryEntries(getter_AddRefs(dirIterator));

    if (NS_FAILED(rv)) return rv;

    // whip through the directory to register every file
    nsCOMPtr<nsIFile> dirEntry;
    PRBool more = PR_FALSE;

    rv = dirIterator->HasMoreElements(&more);
    if (NS_FAILED(rv)) return rv;
    while (more == PR_TRUE)
    {
        rv = dirIterator->GetNext((nsISupports**)getter_AddRefs(dirEntry));
        if (NS_SUCCEEDED(rv))
        {
            rv = dirEntry->IsDirectory(&isDir);
            if (NS_SUCCEEDED(rv))
            {
                if (isDir == PR_TRUE)
                {
                    // This is a directory. Grovel for components into the directory.
#ifdef RT_OS_DARWIN // But not if it's a debug bundle.
                    nsCAutoString leafName;
                    rv = dirEntry->GetNativeLeafName(leafName);
                    if (   NS_FAILED(rv)
                        || leafName.Length() < sizeof(".dSYM")
                        || PL_strcasecmp(leafName.get() + (leafName.Length() - sizeof(".dSYM") + 1), ".dSYM"))
#endif
                        rv = RegisterComponentsInDir(when, dirEntry);
                }
                else
                {
                    PRBool registered;
                    // This is a file. Try to register it.
                    rv = AutoRegisterComponent(when, dirEntry, &registered);
                }
            }
        }
        rv = dirIterator->HasMoreElements(&more);
        if (NS_FAILED(rv)) return rv;
    }

    return rv;
}

DECLINLINE(void) nsLogDllMsg(nsDll *dll, const char *pszWhat)
{
#ifdef LOG_ENABLED
    nsXPIDLCString displayPath;
    dll->GetDisplayPath(displayPath);

    Log(("nsNativeComponentLoader: %s \"%s\".\n", pszWhat, displayPath.get()));
#endif
}

static nsresult PR_CALLBACK
nsFreeLibrary(nsDll *dll, nsIServiceManager *serviceMgr, PRInt32 when)
{
    nsresult rv = NS_ERROR_FAILURE;

    if (!dll || dll->IsLoaded() == PR_FALSE)
    {
        return NS_ERROR_INVALID_ARG;
    }

    // Get if the dll was marked for unload in an earlier round
    PRBool dllMarkedForUnload = dll->IsMarkedForUnload();

    // Reset dll marking for unload just in case we return with
    // an error.
    dll->MarkForUnload(PR_FALSE);

    PRBool canUnload = PR_FALSE;

    // Get the module object
    nsCOMPtr<nsIModule> mobj;
    /* XXXshaver cheat and use the global component manager */
    rv = dll->GetModule(NS_STATIC_CAST(nsIComponentManager*, nsComponentManagerImpl::gComponentManager),
                        getter_AddRefs(mobj));
    if (NS_SUCCEEDED(rv))
    {
        rv = mobj->CanUnload(nsComponentManagerImpl::gComponentManager, &canUnload);
    }

    mobj = nsnull; // Release our reference to the module object
    // When shutting down, whether we can unload the dll or not,
    // we will shutdown the dll to release any memory it has got
    if (when == nsIComponentManagerObsolete::NS_Shutdown)
    {
        dll->Shutdown();
    }

    // Check error status on CanUnload() call
    if (NS_FAILED(rv))
    {
        nsLogDllMsg(dll, "nsIModule::CanUnload() returned error for");
        return rv;
    }

    if (canUnload)
    {
        if (dllMarkedForUnload)
        {
            nsLogDllMsg(dll, "+ Unloading");

#ifdef DEBUG_dougt
            // XXX dlls aren't counting their outstanding instances correctly
            // XXX hence, dont unload until this gets enforced.
            rv = dll->Unload();
#endif /* 0 */
        }
        else
            nsLogDllMsg(dll, "Ready for unload");
    }
    else
    {
        nsLogDllMsg(dll, "NOT ready for unload");
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}

struct freeLibrariesClosure
{
    nsIServiceManager *serviceMgr;
    PRInt32 when;
};

static PRBool PR_CALLBACK
nsFreeLibraryEnum(nsHashKey *aKey, void *aData, void* closure)
{
    nsDll *dll = (nsDll *) aData;
    struct freeLibrariesClosure *callData = (struct freeLibrariesClosure *) closure;
    nsFreeLibrary(dll,
                  (callData ? callData->serviceMgr : NULL),
                  (callData ? callData->when : nsIComponentManagerObsolete::NS_Timer));
    return PR_TRUE;
}

/*
 * SelfRegisterDll
 *
 * Given a dll abstraction, this will load, selfregister the dll and
 * unload the dll.
 *
 */
nsresult
nsNativeComponentLoader::SelfRegisterDll(nsDll *dll,
                                         const char *registryLocation,
                                         PRBool deferred)
{
    // Precondition: dll is not loaded already, unless we're deferred
    Assert(deferred || dll->IsLoaded() == PR_FALSE);

    nsresult res;
    nsCOMPtr<nsIServiceManager> serviceMgr;
    res = NS_GetServiceManager(getter_AddRefs(serviceMgr));
    if (NS_FAILED(res)) return res;

    if (dll->Load() == PR_FALSE)
    {
        /** @todo r=aeichner Get error text from RTLdr(). */
        // Cannot load. Probably not a dll.
        char errorMsg[1024] = "Cannot get error from nspr. Not enough memory.";
        DumpLoadError(dll, "SelfRegisterDll", errorMsg);
        return NS_ERROR_FAILURE;
    }

    nsLogDllMsg(dll, "Loaded");

    // Tell the module to self register
    nsCOMPtr<nsIFile> fs;
    nsCOMPtr<nsIModule> mobj;
    res = dll->GetModule(mCompMgr, getter_AddRefs(mobj));
    if (NS_SUCCEEDED(res))
    {
        /*************************************************************
         * WARNING: Why are use introducing 'res2' here and then     *
         * later assigning it to 'res' rather than just using 'res'? *
         * This is because this code turns up a code-generation bug  *
         * in VC6 on NT. Assigning to 'res' on the next line causes  *
         * the value of 'dll' to get nulled out! The two seem to be  *
         * getting aliased together during compilation.              *
         *************************************************************/
        nsresult res2 = dll->GetDllSpec(getter_AddRefs(fs));    // don't change 'res2' -- see warning, above
        if (NS_SUCCEEDED(res2)) {
            // in the case of re-registering a component, we want to remove
            // any optional data that this file may have had.
            AddDependentLibrary(fs, nsnull);

            res = mobj->RegisterSelf(mCompMgr, fs, registryLocation,
                                     nativeComponentType);
        }
        else
        {
            res = res2;         // don't take this out -- see warning, above
            nsLogDllMsg(dll, "dll->GetDllSpec() FAILED on");
        }
        mobj = NULL;    // Force a release of the Module object before unload()
    }

    // Update the timestamp and size of the dll in registry
    // Don't enter deferred modules in the registry, because it might only be
    // able to register on some later autoreg, after another component has been
    // installed.
    if (res != NS_ERROR_FACTORY_REGISTER_AGAIN) {
        PRInt64 modTime;
        if (!fs)
            return res;

        fs->GetLastModifiedTime(&modTime);
        nsCOMPtr<nsIComponentLoaderManager> manager = do_QueryInterface(mCompMgr);
        if (!manager)
            return NS_ERROR_FAILURE;

        nsCOMPtr<nsIFile> fs;
        res = dll->GetDllSpec(getter_AddRefs(fs));
        if (NS_FAILED(res)) return res;

        manager->SaveFileInfo(fs, registryLocation, modTime);
    }

    return res;
}

nsresult
nsNativeComponentLoader::DumpLoadError(nsDll *dll,
                                       const char *aCallerName,
                                       const char *aNsprErrorMsg)
{
    Assert(aCallerName != NULL);

    if (nsnull == dll || nsnull == aNsprErrorMsg)
        return NS_OK;

    nsCAutoString errorMsg(aNsprErrorMsg);

    nsXPIDLCString displayPath;
    dll->GetDisplayPath(displayPath);

#ifdef DEBUG
    fprintf(stderr,
            "nsNativeComponentLoader: %s(%s) Load FAILED with error: %s\n",
            aCallerName,
            displayPath.get(),
            errorMsg.get());
#endif

    Log(("nsNativeComponentLoader: %s(%s) Load FAILED with error: %s",
         aCallerName, displayPath.get(), errorMsg.get()));
    return NS_OK;
}

nsresult
nsNativeComponentLoader::SelfUnregisterDll(nsDll *dll)
{
    nsresult res;
    nsCOMPtr<nsIServiceManager> serviceMgr;
    res = NS_GetServiceManager(getter_AddRefs(serviceMgr));
    if (NS_FAILED(res)) return res;

    if (dll->Load() == PR_FALSE)
    {
        // Cannot load. Probably not a dll.
        return(NS_ERROR_FAILURE);
    }

    // Tell the module to self register
    nsCOMPtr<nsIModule> mobj;
    res = dll->GetModule(mCompMgr, getter_AddRefs(mobj));
    if (NS_SUCCEEDED(res))
    {
        nsLogDllMsg(dll, "using nsIModule to unregister self on");

        nsCOMPtr<nsIFile> fs;
        res = dll->GetDllSpec(getter_AddRefs(fs));
        if (NS_FAILED(res)) return res;
        // Get registry location for spec
        nsXPIDLCString registryName;

        // what I want to do here is QI for a Component Registration Manager.  Since this
        // has not been invented yet, QI to the obsolete manager.  Kids, don't do this at home.
        nsCOMPtr<nsIComponentManagerObsolete> obsoleteManager = do_QueryInterface(mCompMgr, &res);
        if (obsoleteManager)
            res = obsoleteManager->RegistryLocationForSpec(fs, getter_Copies(registryName));

        if (NS_FAILED(res)) return res;
        mobj->UnregisterSelf(mCompMgr, fs, registryName);
    }
    return res;
}

nsresult
nsNativeComponentLoader::AutoUnregisterComponent(PRInt32 when,
                                                 nsIFile *component,
                                                 PRBool *unregistered)
{

    nsresult rv = NS_ERROR_FAILURE;

    *unregistered = PR_FALSE;

    nsXPIDLCString persistentDescriptor;
    // what I want to do here is QI for a Component Registration Manager.  Since this
    // has not been invented yet, QI to the obsolete manager.  Kids, don't do this at home.
    nsCOMPtr<nsIComponentManagerObsolete> obsoleteManager = do_QueryInterface(mCompMgr, &rv);
    if (obsoleteManager)
        rv = obsoleteManager->RegistryLocationForSpec(component,
                                                      getter_Copies(persistentDescriptor));
    if (NS_FAILED(rv)) return rv;

    // Notify observers, if any, of autoregistration work
    nsCOMPtr<nsIObserverService> observerService =
             do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIServiceManager> mgr;
      rv = NS_GetServiceManager(getter_AddRefs(mgr));
      if (NS_SUCCEEDED(rv))
      {
        (void) observerService->NotifyObservers(mgr,
                                                NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID,
                                                NS_LITERAL_STRING("Unregistering native component").get());
      }
    }

    nsDll *dll = NULL;
    rv = CreateDll(component, persistentDescriptor, &dll);
    if (NS_FAILED(rv) || dll == NULL) return rv;

    rv = SelfUnregisterDll(dll);

    if (NS_SUCCEEDED(rv))
        nsLogDllMsg(dll, "AutoUnregistration succeeded for");
    else
        nsLogDllMsg(dll, "AutoUnregistration FAILED for");

    if (NS_FAILED(rv))
        return rv;

    // Remove any autoreg info about this dll
    nsCStringKey key(persistentDescriptor);
    mDllStore.RemoveAndDelete(&key);

    nsCOMPtr<nsIComponentLoaderManager> manager = do_QueryInterface(mCompMgr);
    NS_ASSERTION(manager, "Something is terribly wrong");

    manager->RemoveFileInfo(component, nsnull);

    *unregistered = PR_TRUE;
    return rv;
}

nsresult
nsNativeComponentLoader::AutoRegisterComponent(PRInt32 when,
                                               nsIFile *component,
                                               PRBool *registered)
{
    nsresult rv;
    if (!registered)
        return NS_ERROR_NULL_POINTER;

    *registered = PR_FALSE;

    /* VBox: Only one valid suffix exist, so dispense with the the list. */
#ifdef RT_OS_DARWIN
# ifdef VBOX_IN_32_ON_64_MAIN_API
    static const char s_szSuff[]        = "-x86.dylib";
# else
    static const char s_szSuff[]        = ".dylib";
    static const char s_szSuffInvalid[] = "-x86.dylib";
# endif
#elif defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
# ifdef VBOX_IN_32_ON_64_MAIN_API
    static const char s_szSuff[]        = "-x86.dll";
# else
    static const char s_szSuff[]        = ".dll";
    static const char s_szSuffInvalid[] = "-x86.dll";
# endif
#else
# ifdef VBOX_IN_32_ON_64_MAIN_API
    static const char s_szSuff[]        = "-x86.so";
# else
    static const char s_szSuff[]        = ".so";
    static const char s_szSuffInvalid[] = "-x86.so";
# endif
#endif

    nsCAutoString strLeafName;
    rv = component->GetNativeLeafName(strLeafName);
    if (NS_FAILED(rv))
        return rv;
    size_t cchLeafName = strLeafName.Length();
    if (   cchLeafName <= sizeof(s_szSuff)
        || PL_strcasecmp(strLeafName.get() + cchLeafName - sizeof(s_szSuff) + 1, s_szSuff))
    {
        Log(("Skipping '%s'...", strLeafName.get()));
        return NS_OK; /* skip */
    }
#ifndef VBOX_IN_32_ON_64_MAIN_API
    if (   cchLeafName >= sizeof(s_szSuffInvalid)
        && !PL_strcasecmp(strLeafName.get() + cchLeafName - sizeof(s_szSuffInvalid) + 1, s_szSuffInvalid))
    {
        Log(("Skipping '%s' (#2)...", strLeafName.get()));
        return NS_OK; /* skip */
    }
#endif
    Log(("... '%s'", strLeafName.get()));

    nsXPIDLCString persistentDescriptor;
    // what I want to do here is QI for a Component Registration Manager.  Since this
    // has not been invented yet, QI to the obsolete manager.  Kids, don't do this at home.
    nsCOMPtr<nsIComponentManagerObsolete> obsoleteManager = do_QueryInterface(mCompMgr, &rv);
    if (obsoleteManager)
        rv = obsoleteManager->RegistryLocationForSpec(component,
                                                      getter_Copies(persistentDescriptor));
    if (NS_FAILED(rv))
        return rv;

    nsCStringKey key(persistentDescriptor);

    // Get the registry representation of the dll, if any
    nsDll *dll;
    rv = CreateDll(component, persistentDescriptor, &dll);
    if (NS_FAILED(rv))
        return rv;

    if (dll != NULL)
    {
        // We already have seen this dll. Check if this dll changed
        if (!dll->HasChanged())
        {
            // Dll hasn't changed. Skip.
            nsLogDllMsg(dll, "Skipping because nsDll has not changed");
            *registered = PR_TRUE;
            return NS_OK;
        }

        // Aagh! the dll has changed since the last time we saw it.
        // re-register dll


        // Notify observers, if any, of autoregistration work
        nsCOMPtr<nsIObserverService> observerService =
                 do_GetService("@mozilla.org/observer-service;1", &rv);
        if (NS_SUCCEEDED(rv))
        {
          nsCOMPtr<nsIServiceManager> mgr;
          rv = NS_GetServiceManager(getter_AddRefs(mgr));
          if (NS_SUCCEEDED(rv))
          {
            // this string can't come from a string bundle, because we
            // don't have string bundles yet.
            NS_ConvertASCIItoUCS2 fileName("(no name)");

            // get the file name
            nsCOMPtr<nsIFile> dllSpec;
            if (NS_SUCCEEDED(dll->GetDllSpec(getter_AddRefs(dllSpec))) && dllSpec)
            {
              dllSpec->GetLeafName(fileName);
            }

            // this string can't come from a string bundle, because we
            // don't have string bundles yet.
            (void) observerService->
                NotifyObservers(mgr,
                                NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID,
                                PromiseFlatString(NS_LITERAL_STRING("Registering native component ") +
                                                  fileName).get());
          }
        }

        if (dll->IsLoaded())
        {
            // We loaded the old version of the dll and now we find that the
            // on-disk copy if newer. Try to unload the dll.
            nsCOMPtr<nsIServiceManager> serviceMgr;
            rv = NS_GetServiceManager(getter_AddRefs(serviceMgr));

            rv = nsFreeLibrary(dll, serviceMgr, when);
            if (NS_FAILED(rv))
            {
                // THIS IS THE WORST SITUATION TO BE IN.
                // Dll doesn't want to be unloaded. Cannot re-register
                // this dll.
                nsLogDllMsg(dll, "Skipping because Dll already loaded. "
                                 "Cannot unload either. Hence cannot re-register");
                return rv;
            }
            else {
                                // dll doesn't have a CanUnload proc. Guess it is
                                // ok to unload it.
                dll->Unload();
                nsLogDllMsg(dll, "+ Unloading (no canUnloadProc)");
            }

        } // dll isloaded

        // Sanity.
        if (dll->IsLoaded())
        {
            // We went through all the above to make sure the dll
            // is unloaded. And here we are with the dll still
            // loaded. Whoever taught dp programming...
            nsLogDllMsg(dll, "Skipping because Dll still loaded. Cannot re-register");
            return NS_ERROR_FAILURE;
        }
    } // dll != NULL
    else
    {
        // Create and add the dll to the mDllStore
        // It is ok to do this even if the creation of nsDll
        // didnt succeed. That way we wont do this again
        // when we encounter the same dll.
        dll = new nsDll(component, this);
        if (dll == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
        mDllStore.Put(&key, (void *) dll);
    } // dll == NULL

    // Either we are seeing the dll for the first time or the dll has
    // changed since we last saw it and it is unloaded successfully.
    //
    // Now we can try register the dll for sure.
    nsresult res = SelfRegisterDll(dll, persistentDescriptor, PR_FALSE);
    if (NS_FAILED(res))
    {
        if (res == NS_ERROR_FACTORY_REGISTER_AGAIN) {
            /* defer for later loading */
            mDeferredComponents.AppendElement(dll);
            *registered = PR_TRUE;
            return NS_OK;
        } else {
            nsLogDllMsg(dll, "Skipping because Autoregistration FAILED for");
            return NS_ERROR_FACTORY_NOT_REGISTERED;
        }
    }
    else
    {
        nsLogDllMsg(dll, "Autoregistration Passed for");
        // Marking dll along with modified time and size in the
        // registry happens at PlatformRegister(). No need to do it
        // here again.
        *registered = PR_TRUE;
    }
    return NS_OK;
}

nsresult
nsNativeComponentLoader::RegisterDeferredComponents(PRInt32 aWhen,
                                                    PRBool *aRegistered)
{
    Log(("nNCL: registering deferred (%d)\n", mDeferredComponents.Count()));

    *aRegistered = PR_FALSE;
    if (!mDeferredComponents.Count())
        return NS_OK;

    for (int i = mDeferredComponents.Count() - 1; i >= 0; i--) {
        nsDll *dll = NS_STATIC_CAST(nsDll *, mDeferredComponents[i]);
        nsresult rv = SelfRegisterDll(dll,
                                      nsnull,
                                      PR_TRUE);
        if (rv != NS_ERROR_FACTORY_REGISTER_AGAIN) {
            if (NS_SUCCEEDED(rv))
                *aRegistered = PR_TRUE;
            mDeferredComponents.RemoveElementAt(i);
        }
    }

    if (*aRegistered)
        Log(("nNCL: registered deferred, %d left\n", mDeferredComponents.Count()));
    else
        Log(("nNCL: didn't register any components, %d left\n", mDeferredComponents.Count()));

    /* are there any fatal errors? */
    return NS_OK;
}

nsresult
nsNativeComponentLoader::OnRegister(const nsIID &aCID, const char *aType,
                                    const char *aClassName,
                                    const char *aContractID,
                                    const char *aLocation,
                                    PRBool aReplace,
                                    PRBool aPersist)
{
    return NS_OK;
}

nsresult
nsNativeComponentLoader::UnloadAll(PRInt32 aWhen)
{
    Log(("nsNativeComponentLoader: Unloading...."));

    struct freeLibrariesClosure callData;
    callData.serviceMgr = NULL; // XXX need to get this as a parameter
    callData.when = aWhen;

    // Cycle through the dlls checking to see if they want to be unloaded
    mDllStore.Enumerate(nsFreeLibraryEnum, &callData);
    return NS_OK;
}

//
// CreateDll
// The only way to create a dll or get it from the dll cache. This will
// be called in multiple situations:
//
// 1. Autoregister will create one for each dll it is trying to register. This
//    call will be passing a spec in.
//    {spec, NULL, 0, 0}
//
// 2. GetFactory() This will call CreateDll() with a null spec but will give
//    the registry represented name of the dll. If modtime and size are zero,
//    we will go the registry to find the right modtime and size.
//    {NULL, rel:libpref.so, 0, 0}
//
// 3. Prepopulation of dllCache A dll object created off a registry entry.
//    Specifically dll name is stored    in rel: or abs: or lib: formats in the
//    registry along with its    lastModTime and fileSize.
//    {NULL, rel:libpref.so, 8985659, 20987}
nsresult
nsNativeComponentLoader::CreateDll(nsIFile *aSpec,
                                   const char *aLocation,
                                   nsDll **aDll)
{
    nsDll *dll;
    nsCOMPtr<nsIFile> dllSpec;
    nsCOMPtr<nsIFile> spec;
    nsresult rv;

    nsCStringKey key(aLocation);
    dll = (nsDll *)mDllStore.Get(&key);
    if (dll)
    {
        *aDll = dll;
        return NS_OK;
    }

    if (!aSpec)
    {
        // what I want to do here is QI for a Component Registration Manager.  Since this
        // has not been invented yet, QI to the obsolete manager.  Kids, don't do this at home.
        nsCOMPtr<nsIComponentManagerObsolete> obsoleteManager = do_QueryInterface(mCompMgr, &rv);
        if (obsoleteManager)
            rv = obsoleteManager->SpecForRegistryLocation(aLocation,
                                                          getter_AddRefs(spec));
        if (NS_FAILED(rv))
            return rv;
    }
    else
    {
        spec = aSpec;
    }

    if (!dll)
    {
        dll = new nsDll(spec, this);
        if (!dll)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    *aDll = dll;
    mDllStore.Put(&key, dll);
    return NS_OK;
}

nsresult
nsNativeComponentLoader::GetFactoryFromModule(nsDll *aDll, const nsCID &aCID,
                                              nsIFactory **aFactory)
{
    nsresult rv;

    nsCOMPtr<nsIModule> module;
    rv = aDll->GetModule(mCompMgr, getter_AddRefs(module));

    if (NS_FAILED(rv))
        return rv;

    return module->GetClassObject(mCompMgr, aCID, NS_GET_IID(nsIFactory),
                                  (void **)aFactory);
}


NS_IMETHODIMP
nsNativeComponentLoader::AddDependentLibrary(nsIFile* aFile, const char* libName)
{
    nsCOMPtr<nsIComponentLoaderManager> manager = do_QueryInterface(mCompMgr);
    if (!manager)
    {
        NS_WARNING("Something is terribly wrong");
        return NS_ERROR_FAILURE;
    }

    // the native component loader uses the optional data
    // to store a space delimited list of dependent library
    // names

    if (!libName)
    {
        manager->SetOptionalData(aFile, nsnull, nsnull);
        return NS_OK;
    }

    nsXPIDLCString data;
    manager->GetOptionalData(aFile, nsnull, getter_Copies(data));

    if (!data.IsEmpty())
        data.AppendLiteral(" ");

    data.Append(nsDependentCString(libName));

    manager->SetOptionalData(aFile, nsnull, data);
    return NS_OK;
}
