/* $Id$ */
/** @file
 * ICloudUserProfileManager  COM class implementations.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#include <iprt/path.h>
#include <iprt/cpp/utils.h>
#include <VBox/com/array.h>
#include <map>

#include "CloudUserProfileManagerImpl.h"
#include "CloudUserProfilesImpl.h"
#include "VirtualBoxImpl.h"
#include "ExtPackManagerImpl.h"
#include "Global.h"
#include "ProgressImpl.h"
#include "MachineImpl.h"
#include "AutoCaller.h"
#include "Logging.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
//
// CloudUserProfileManager constructor / destructor
//
// ////////////////////////////////////////////////////////////////////////////////
CloudUserProfileManager::CloudUserProfileManager()
    : mParent(NULL)
{
}

CloudUserProfileManager::~CloudUserProfileManager()
{
}


HRESULT CloudUserProfileManager::FinalConstruct()
{
    return BaseFinalConstruct();
}

void CloudUserProfileManager::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

HRESULT CloudUserProfileManager::init(VirtualBox *aParent)
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

#ifdef VBOX_WITH_CLOUD_PROVIDERS_IN_EXTPACK
    /*
     * Engage the extension pack manager and get all the implementations of this class.
     */
    ExtPackManager *pExtPackMgr = aParent->i_getExtPackManager();
    std::vector<ComPtr<IUnknown> > Objects;
    com::Guid idObj(COM_IIDOF(ICloudUserProfileManager));
    pExtPackMgr->i_queryObjects(idObj.toString(), Objects);
    for (unsigned i = 0; i < Objects.size(); i++)
    {
        ComPtr<ICloudUserProfileManager> ptrTmp;
        HRESULT hrc = Objects[i].queryInterfaceTo(ptrTmp.asOutParam());
        if (SUCCEEDED(hrc))
            mUserProfileManagers.push_back(ptrTmp);
    }
#else

    mSupportedProviders.clear();
    mSupportedProviders.push_back(CloudProviderId_OCI);
#endif

    autoInitSpan.setSucceeded();
    return S_OK;
}

void CloudUserProfileManager::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst(mParent) = NULL;
}

HRESULT CloudUserProfileManager::getSupportedProviders(std::vector<CloudProviderId_T> &aSupportedProviders)
{
#ifdef VBOX_WITH_CLOUD_PROVIDERS_IN_EXTPACK
    /*
     * Collect all the provider names from all the extension pack objects.
     */
    HRESULT hrc = S_OK;
    for (unsigned i = 0; i < mUserProfileManagers.size(); i++)
    {
        SafeArray<CloudProviderId_T> FromCurrent;
        HRESULT hrc2 = mUserProfileManagers[i]->COMGETTER(SupportedProviders)(ComSafeArrayAsOutParam(FromCurrent));
        if (SUCCEEDED(hrc2))
            for (size_t i = 0; i < FromCurrent.size(); i++)
                aSupportedProviders.push_back(FromCurrent[i]);
        else if (SUCCEEDED(hrc))
            hrc = hrc2;
    }
    if (aSupportedProviders.size() > 0)
        hrc = S_OK;
    return hrc;
#else
    aSupportedProviders = mSupportedProviders;
    return S_OK;
#endif
}

HRESULT CloudUserProfileManager::getAllProfiles(std::vector<ComPtr<ICloudUserProfiles> > &aProfilesList)
{
#ifdef VBOX_WITH_CLOUD_PROVIDERS_IN_EXTPACK
    /*
     * Collect all the cloud providers from all the extension pack objects.
     */
    HRESULT hrc = S_OK;
    for (unsigned i = 0; i < mUserProfileManagers.size(); i++)
    {
        SafeIfaceArray<ICloudUserProfiles> FromCurrent;
        HRESULT hrc2 = mUserProfileManagers[i]->GetAllProfiles(ComSafeArrayAsOutParam(FromCurrent));
        if (SUCCEEDED(hrc2))
            for (size_t i = 0; i < FromCurrent.size(); i++)
                aProfilesList.push_back(FromCurrent[i]);
        else if (SUCCEEDED(hrc))
            hrc = hrc2;
    }
    if (aProfilesList.size() > 0)
        hrc = S_OK;
    return hrc;

#else
    HRESULT hrc = S_OK;
    std::vector<ComPtr<ICloudUserProfiles> > lProfilesList;
    for (size_t i=0;i<mSupportedProviders.size();++i)
    {
        ComPtr<ICloudUserProfiles> lProfiles;
        hrc = getProfilesByProvider(mSupportedProviders.at(i), lProfiles);
        if (FAILED(hrc))
            break;

        lProfilesList.push_back(lProfiles);
    }

    if (SUCCEEDED(hrc))
        aProfilesList = lProfilesList;

    return hrc;
#endif
}

HRESULT CloudUserProfileManager::getProfilesByProvider(CloudProviderId_T aProviderType,
                                                       ComPtr<ICloudUserProfiles> &aProfiles)
{
#ifdef VBOX_WITH_CLOUD_PROVIDERS_IN_EXTPACK
    /*
     * Return the first provider we get.
     */
    HRESULT hrc = VBOX_E_OBJECT_NOT_FOUND;
    for (unsigned i = 0; i < mUserProfileManagers.size(); i++)
    {
        hrc = mUserProfileManagers[i]->GetProfilesByProvider(aProviderType, aProfiles.asOutParam());
        if (SUCCEEDED(hrc))
            break;
    }
    return hrc;

#else

    ComObjPtr<CloudUserProfiles> ptrCloudUserProfiles;
    HRESULT hrc = ptrCloudUserProfiles.createObject();
    switch(aProviderType)
    {
        case CloudProviderId_OCI:
        default:
            ComObjPtr<OCIUserProfiles> ptrOCIUserProfiles;
            hrc = ptrOCIUserProfiles.createObject();
            if (SUCCEEDED(hrc))
            {
                AutoReadLock wlock(this COMMA_LOCKVAL_SRC_POS);

                hrc = ptrOCIUserProfiles->init(mParent);
                if (SUCCEEDED(hrc))
                {
                    char szOciConfigPath[RTPATH_MAX];
                    int vrc = RTPathUserHome(szOciConfigPath, sizeof(szOciConfigPath));
                    if (RT_SUCCESS(vrc))
                        vrc = RTPathAppend(szOciConfigPath, sizeof(szOciConfigPath), ".oci" RTPATH_SLASH_STR "config");
                    if (RT_SUCCESS(vrc))
                    {
                        LogRel(("config = %s\n", szOciConfigPath));
                        if (RTFileExists(szOciConfigPath))
                        {
                            hrc = ptrOCIUserProfiles->readProfiles(szOciConfigPath);
                            if (SUCCEEDED(hrc))
                                LogRel(("Reading profiles from %s has been done\n", szOciConfigPath));
                            else
                                LogRel(("Reading profiles from %s hasn't been done\n", szOciConfigPath));

                            ptrCloudUserProfiles = ptrOCIUserProfiles;
                            hrc = ptrCloudUserProfiles.queryInterfaceTo(aProfiles.asOutParam());
                        }
                        else
                            hrc = setErrorBoth(E_FAIL, VERR_FILE_NOT_FOUND, tr("Could not locate the config file '%s'"),
                                               szOciConfigPath);
                    }
                    else
                        hrc = setErrorVrc(vrc);
                }
            }
            break;
    }

    return hrc;
#endif
}

