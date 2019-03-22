/* $Id$ */
/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_MAIN_VRDESERVER
#include "VRDEServerImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#ifdef VBOX_WITH_EXTPACK
# include "ExtPackManagerImpl.h"
#endif

#include <iprt/cpp/utils.h>
#include <iprt/ctype.h>
#include <iprt/ldr.h>
#include <iprt/path.h>

#include <VBox/err.h>
#include <VBox/sup.h>
#include <VBox/com/array.h>

#include <VBox/RemoteDesktop/VRDE.h>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Global.h"
#include "LoggingNew.h"

// defines
/////////////////////////////////////////////////////////////////////////////
#define VRDP_DEFAULT_PORT_STR "3389"

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

VRDEServer::VRDEServer()
    : mParent(NULL)
{
}

VRDEServer::~VRDEServer()
{
}

HRESULT VRDEServer::FinalConstruct()
{
    return BaseFinalConstruct();
}

void VRDEServer::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the VRDP server object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT VRDEServer::init(Machine *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    /* mPeer is left null */

    mData.allocate();

    mData->fEnabled = false;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the object given another object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT VRDEServer::init(Machine *aParent, VRDEServer *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    unconst(mPeer) = aThat;

    AutoCaller thatCaller(aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
    mData.share(aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT VRDEServer::initCopy(Machine *aParent, VRDEServer *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    /* mPeer is left null */

    AutoCaller thatCaller(aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
    mData.attachCopy(aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void VRDEServer::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    mData.free();

    unconst(mPeer) = NULL;
    unconst(mParent) = NULL;
}

/**
 *  Loads settings from the given machine node.
 *  May be called once right after this object creation.
 *
 *  @param data Configuration settings.
 *
 *  @note Locks this object for writing.
 */
HRESULT VRDEServer::i_loadSettings(const settings::VRDESettings &data)
{
    using namespace settings;

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    mData.assignCopy(&data);

    return S_OK;
}

/**
 *  Saves settings to the given machine node.
 *
 *  @param data Configuration settings.
 *
 *  @note Locks this object for reading.
 */
HRESULT VRDEServer::i_saveSettings(settings::VRDESettings &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    data = *mData.data();

    return S_OK;
}

// IVRDEServer properties
/////////////////////////////////////////////////////////////////////////////

HRESULT VRDEServer::getEnabled(BOOL *aEnabled)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = mData->fEnabled;

    return S_OK;
}

HRESULT VRDEServer::setEnabled(BOOL aEnabled)
{
    /* the machine can also be in saved state for this property to change */
    AutoMutableOrSavedOrRunningStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    if (mData->fEnabled != RT_BOOL(aEnabled))
    {
        mData.backup();
        mData->fEnabled = RT_BOOL(aEnabled);

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
        mParent->i_setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        /* Avoid deadlock when i_onVRDEServerChange eventually calls SetExtraData. */
        adep.release();

        rc = mParent->i_onVRDEServerChange(/* aRestart */ TRUE);
        if (FAILED(rc))
        {
            /* Failed to enable/disable the server. Revert the internal state. */
            adep.add();
            if (SUCCEEDED(adep.rc()))
            {
                alock.acquire();
                mData->fEnabled = !RT_BOOL(aEnabled);
                alock.release();
                mlock.acquire();
                mParent->i_setModified(Machine::IsModified_VRDEServer);
            }
        }
    }

    return rc;
}

static int i_portParseNumber(uint16_t *pu16Port, const char *pszStart, const char *pszEnd)
{
    /* Gets a string of digits, converts to 16 bit port number.
     * Note: pszStart <= pszEnd is expected, the string contains
     *       only digits and pszEnd points to the char after last
     *       digit.
     */
    size_t cch = pszEnd - pszStart;
    if (cch > 0 && cch <= 5) /* Port is up to 5 decimal digits. */
    {
        unsigned uPort = 0;
        while (pszStart != pszEnd)
        {
            uPort = uPort * 10 + *pszStart - '0';
            pszStart++;
        }

        if (uPort != 0 && uPort < 0x10000)
        {
            if (pu16Port)
                *pu16Port = (uint16_t)uPort;
            return VINF_SUCCESS;
        }
    }

    return VERR_INVALID_PARAMETER;
}

static int i_vrdpServerVerifyPortsString(const com::Utf8Str &aPortRange)
{
    const char *pszPortRange = aPortRange.c_str();

    if (!pszPortRange || *pszPortRange == 0) /* Reject empty string. */
        return VERR_INVALID_PARAMETER;

    /* The string should be like "1000-1010,1020,2000-2003" */
    while (*pszPortRange)
    {
        const char *pszStart = pszPortRange;
        const char *pszDash = NULL;
        const char *pszEnd = pszStart;

        while (*pszEnd && *pszEnd != ',')
        {
            if (*pszEnd == '-')
            {
                if (pszDash != NULL)
                    return VERR_INVALID_PARAMETER; /* More than one '-'. */

                pszDash = pszEnd;
            }
            else if (!RT_C_IS_DIGIT(*pszEnd))
                return VERR_INVALID_PARAMETER;

            pszEnd++;
        }

        /* Update the next range pointer. */
        pszPortRange = pszEnd;
        if (*pszPortRange == ',')
        {
            pszPortRange++;
        }

        /* A probably valid range. Verify and parse it. */
        int rc;
        if (pszDash)
        {
            rc = i_portParseNumber(NULL, pszStart, pszDash);
            if (RT_SUCCESS(rc))
                rc = i_portParseNumber(NULL, pszDash + 1, pszEnd);
        }
        else
            rc = i_portParseNumber(NULL, pszStart, pszEnd);

        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}

HRESULT VRDEServer::setVRDEProperty(const com::Utf8Str &aKey, const com::Utf8Str &aValue)
{
    LogFlowThisFunc(("\n"));

    /* the machine can also be in saved state for this property to change */
    AutoMutableOrSavedOrRunningStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Special processing for some "standard" properties. */
    if (aKey == "TCP/Ports")
    {
        /* Verify the string. "0" means the default port. */
        Utf8Str strPorts = aValue == "0"?
                               VRDP_DEFAULT_PORT_STR:
                               aValue;
        int vrc = i_vrdpServerVerifyPortsString(strPorts);
        if (RT_FAILURE(vrc))
            return E_INVALIDARG;

        if (strPorts != mData->mapProperties["TCP/Ports"])
        {
            /* Port value is not verified here because it is up to VRDP transport to
             * use it. Specifying a wrong port number will cause a running server to
             * stop. There is no fool proof here.
             */
            mData.backup();
            mData->mapProperties["TCP/Ports"] = strPorts;

            /* leave the lock before informing callbacks */
            alock.release();

            AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
            mParent->i_setModified(Machine::IsModified_VRDEServer);
            mlock.release();

            /* Avoid deadlock when i_onVRDEServerChange eventually calls SetExtraData. */
            adep.release();

            mParent->i_onVRDEServerChange(/* aRestart */ TRUE);
        }
    }
    else
    {
        /* Generic properties processing.
         * Look up the old value first; if nothing's changed then do nothing.
         */
        Utf8Str strOldValue;

        settings::StringsMap::const_iterator it = mData->mapProperties.find(aKey);
        if (it != mData->mapProperties.end())
            strOldValue = it->second;

        if (strOldValue != aValue)
        {
            if (aValue.isEmpty())
                mData->mapProperties.erase(aKey);
            else
                mData->mapProperties[aKey] = aValue;

            /* leave the lock before informing callbacks */
            alock.release();

            AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);
            mParent->i_setModified(Machine::IsModified_VRDEServer);
            mlock.release();

            /* Avoid deadlock when i_onVRDEServerChange eventually calls SetExtraData. */
            adep.release();

            mParent->i_onVRDEServerChange(/* aRestart */ TRUE);
        }
    }

    return S_OK;
}

HRESULT VRDEServer::getVRDEProperty(const com::Utf8Str &aKey, com::Utf8Str &aValue)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    settings::StringsMap::const_iterator it = mData->mapProperties.find(aKey);
    if (it != mData->mapProperties.end())
        aValue = it->second; // source is a Utf8Str
    else if (aKey == "TCP/Ports")
        aValue = VRDP_DEFAULT_PORT_STR;

    return S_OK;
}

static int loadVRDELibrary(const char *pszLibraryName, RTLDRMOD *phmod, PFNVRDESUPPORTEDPROPERTIES *ppfn)
{
    int rc = VINF_SUCCESS;

    RTLDRMOD hmod = NIL_RTLDRMOD;

    RTERRINFOSTATIC ErrInfo;
    RTErrInfoInitStatic(&ErrInfo);
    if (RTPathHavePath(pszLibraryName))
        rc = SUPR3HardenedLdrLoadPlugIn(pszLibraryName, &hmod, &ErrInfo.Core);
    else
        rc = SUPR3HardenedLdrLoadAppPriv(pszLibraryName, &hmod, RTLDRLOAD_FLAGS_LOCAL, &ErrInfo.Core);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(hmod, "VRDESupportedProperties", (void **)ppfn);

        if (RT_FAILURE(rc) && rc != VERR_SYMBOL_NOT_FOUND)
            LogRel(("VRDE: Error resolving symbol '%s', rc %Rrc.\n", "VRDESupportedProperties", rc));
    }
    else
    {
        if (RTErrInfoIsSet(&ErrInfo.Core))
            LogRel(("VRDE: Error loading the library '%s': %s (%Rrc)\n", pszLibraryName, ErrInfo.Core.pszMsg, rc));
        else
            LogRel(("VRDE: Error loading the library '%s' rc = %Rrc.\n", pszLibraryName, rc));

        hmod = NIL_RTLDRMOD;
    }

    if (RT_SUCCESS(rc))
    {
        *phmod = hmod;
    }
    else
    {
        if (hmod != NIL_RTLDRMOD)
        {
            RTLdrClose(hmod);
            hmod = NIL_RTLDRMOD;
        }
    }

    return rc;
}

HRESULT VRDEServer::getVRDEProperties(std::vector<com::Utf8Str> &aProperties)
{
    size_t cProperties = 0;
    aProperties.resize(0);
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->fEnabled)
    {
        return S_OK;
    }
    alock.release();

    /*
     * Check that a VRDE extension pack name is set and resolve it into a
     * library path.
     */
    Bstr bstrExtPack;
    HRESULT hrc = COMGETTER(VRDEExtPack)(bstrExtPack.asOutParam());
    Log(("VRDEPROP: get extpack hrc 0x%08X, isEmpty %d\n", hrc, bstrExtPack.isEmpty()));
    if (FAILED(hrc))
        return hrc;
    if (bstrExtPack.isEmpty())
        return E_FAIL;

    Utf8Str strExtPack(bstrExtPack);
    Utf8Str strVrdeLibrary;
    int vrc = VINF_SUCCESS;
    if (strExtPack.equals(VBOXVRDP_KLUDGE_EXTPACK_NAME))
        strVrdeLibrary = "VBoxVRDP";
    else
    {
#ifdef VBOX_WITH_EXTPACK
        VirtualBox *pVirtualBox = mParent->i_getVirtualBox();
        ExtPackManager *pExtPackMgr = pVirtualBox->i_getExtPackManager();
        vrc = pExtPackMgr->i_getVrdeLibraryPathForExtPack(&strExtPack, &strVrdeLibrary);
#else
        vrc = VERR_FILE_NOT_FOUND;
#endif
    }
    Log(("VRDEPROP: library get rc %Rrc\n", vrc));

    if (RT_SUCCESS(vrc))
    {
        /*
         * Load the VRDE library and start the server, if it is enabled.
         */
        PFNVRDESUPPORTEDPROPERTIES pfn = NULL;
        RTLDRMOD hmod = NIL_RTLDRMOD;
        vrc = loadVRDELibrary(strVrdeLibrary.c_str(), &hmod, &pfn);
        Log(("VRDEPROP: load library [%s] rc %Rrc\n", strVrdeLibrary.c_str(), vrc));
        if (RT_SUCCESS(vrc))
        {
            const char * const *papszNames = pfn();

            if (papszNames)
            {
                size_t i;
                for (i = 0; papszNames[i] != NULL; ++i)
                {
                    cProperties++;
                }
            }
            Log(("VRDEPROP: %d properties\n", cProperties));

            if (cProperties > 0)
            {
                aProperties.resize(cProperties);
                for (size_t i = 0; i < cProperties && papszNames[i] != NULL; ++i)
                {
                     aProperties[i] = papszNames[i];
                }
            }

            /* Do not forget to unload the library. */
            RTLdrClose(hmod);
            hmod = NIL_RTLDRMOD;
        }
    }

    if (RT_FAILURE(vrc))
    {
        return E_FAIL;
    }

    return S_OK;
}


HRESULT VRDEServer::getAuthType(AuthType_T *aType)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aType = mData->authType;

    return S_OK;
}

HRESULT VRDEServer::setAuthType(AuthType_T aType)
{
    /* the machine can also be in saved state for this property to change */
    AutoMutableOrSavedOrRunningStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->authType != aType)
    {
        mData.backup();
        mData->authType = aType;

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
        mParent->i_setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        mParent->i_onVRDEServerChange(/* aRestart */ TRUE);
    }

    return S_OK;
}

HRESULT VRDEServer::getAuthTimeout(ULONG *aTimeout)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aTimeout = mData->ulAuthTimeout;

    return S_OK;
}


HRESULT VRDEServer::setAuthTimeout(ULONG aTimeout)
{
    /* the machine can also be in saved state for this property to change */
    AutoMutableOrSavedOrRunningStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (aTimeout != mData->ulAuthTimeout)
    {
        mData.backup();
        mData->ulAuthTimeout = aTimeout;

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
        mParent->i_setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        /* sunlover 20060131: This setter does not require the notification
         * really */
#if 0
        mParent->onVRDEServerChange();
#endif
    }

    return S_OK;
}

HRESULT VRDEServer::getAuthLibrary(com::Utf8Str &aLibrary)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    aLibrary = mData->strAuthLibrary;
    alock.release();

    if (aLibrary.isEmpty())
    {
        /* Get the global setting. */
        ComPtr<ISystemProperties> systemProperties;
        HRESULT hrc = mParent->i_getVirtualBox()->COMGETTER(SystemProperties)(systemProperties.asOutParam());
        if (SUCCEEDED(hrc))
        {
            Bstr strlib;
            hrc = systemProperties->COMGETTER(VRDEAuthLibrary)(strlib.asOutParam());
            if (SUCCEEDED(hrc))
                aLibrary = Utf8Str(strlib).c_str();
        }

        if (FAILED(hrc))
            return setError(hrc, "failed to query the library setting\n");
    }

    return S_OK;
}


HRESULT VRDEServer::setAuthLibrary(const com::Utf8Str &aLibrary)
{
    /* the machine can also be in saved state for this property to change */
    AutoMutableOrSavedOrRunningStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->strAuthLibrary != aLibrary)
    {
        mData.backup();
        mData->strAuthLibrary = aLibrary;

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);
        mParent->i_setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        mParent->i_onVRDEServerChange(/* aRestart */ TRUE);
    }

    return S_OK;
}


HRESULT VRDEServer::getAllowMultiConnection(BOOL *aAllowMultiConnection)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aAllowMultiConnection = mData->fAllowMultiConnection;

    return S_OK;
}


HRESULT VRDEServer::setAllowMultiConnection(BOOL aAllowMultiConnection)
{
    /* the machine can also be in saved state for this property to change */
    AutoMutableOrSavedOrRunningStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->fAllowMultiConnection != RT_BOOL(aAllowMultiConnection))
    {
        mData.backup();
        mData->fAllowMultiConnection = RT_BOOL(aAllowMultiConnection);

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
        mParent->i_setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        mParent->i_onVRDEServerChange(/* aRestart */ TRUE); /// @todo does it need a restart?
    }

    return S_OK;
}

HRESULT VRDEServer::getReuseSingleConnection(BOOL *aReuseSingleConnection)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aReuseSingleConnection = mData->fReuseSingleConnection;

    return S_OK;
}


HRESULT VRDEServer::setReuseSingleConnection(BOOL aReuseSingleConnection)
{
    AutoMutableOrSavedOrRunningStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->fReuseSingleConnection != RT_BOOL(aReuseSingleConnection))
    {
        mData.backup();
        mData->fReuseSingleConnection = RT_BOOL(aReuseSingleConnection);

        /* leave the lock before informing callbacks */
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, needs no locking
        mParent->i_setModified(Machine::IsModified_VRDEServer);
        mlock.release();

        mParent->i_onVRDEServerChange(/* aRestart */ TRUE); /// @todo needs a restart?
    }

    return S_OK;
}

HRESULT VRDEServer::getVRDEExtPack(com::Utf8Str &aExtPack)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    Utf8Str strExtPack = mData->strVrdeExtPack;
    alock.release();
    HRESULT hrc = S_OK;

    if (strExtPack.isNotEmpty())
    {
        if (strExtPack.equals(VBOXVRDP_KLUDGE_EXTPACK_NAME))
            hrc = S_OK;
        else
        {
#ifdef VBOX_WITH_EXTPACK
            ExtPackManager *pExtPackMgr = mParent->i_getVirtualBox()->i_getExtPackManager();
            hrc = pExtPackMgr->i_checkVrdeExtPack(&strExtPack);
#else
            hrc = setError(E_FAIL, tr("Extension pack '%s' does not exist"), strExtPack.c_str());
#endif
        }
        if (SUCCEEDED(hrc))
            aExtPack = strExtPack;
    }
    else
    {
        /* Get the global setting. */
        ComPtr<ISystemProperties> systemProperties;
        hrc = mParent->i_getVirtualBox()->COMGETTER(SystemProperties)(systemProperties.asOutParam());
        if (SUCCEEDED(hrc))
        {
            Bstr bstr;
            hrc = systemProperties->COMGETTER(DefaultVRDEExtPack)(bstr.asOutParam());
            if (SUCCEEDED(hrc))
                aExtPack = bstr;
        }
    }
    return hrc;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////
HRESULT VRDEServer::setVRDEExtPack(const com::Utf8Str &aExtPack)
{
    HRESULT hrc = S_OK;
    /* the machine can also be in saved state for this property to change */
    AutoMutableOrSavedOrRunningStateDependency adep(mParent);
    hrc = adep.rc();
    if (SUCCEEDED(hrc))
    {
        /*
         * If not empty, check the specific extension pack.
         */
        if (!aExtPack.isEmpty())
        {
            if (aExtPack.equals(VBOXVRDP_KLUDGE_EXTPACK_NAME))
                hrc = S_OK;
            else
            {
#ifdef VBOX_WITH_EXTPACK
                ExtPackManager *pExtPackMgr = mParent->i_getVirtualBox()->i_getExtPackManager();
                hrc = pExtPackMgr->i_checkVrdeExtPack(&aExtPack);
#else
                hrc = setError(E_FAIL, tr("Extension pack '%s' does not exist"), aExtPack.c_str());
#endif
            }
        }
        if (SUCCEEDED(hrc))
        {
            /*
             * Update the setting if there is an actual change, post an
             * change event to trigger a VRDE server restart.
             */
             AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
             if (aExtPack != mData->strVrdeExtPack)
             {
                 mData.backup();
                 mData->strVrdeExtPack = aExtPack;

                /* leave the lock before informing callbacks */
                alock.release();

                AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);
                mParent->i_setModified(Machine::IsModified_VRDEServer);
                mlock.release();

                mParent->i_onVRDEServerChange(/* aRestart */ TRUE);
            }
        }
    }

    return hrc;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 *  @note Locks this object for writing.
 */
void VRDEServer::i_rollback()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.rollback();
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void VRDEServer::i_commit()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller(mPeer);
    AssertComRCReturnVoid(peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(mPeer, this COMMA_LOCKVAL_SRC_POS);

    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            /* attach new data to the peer and reshare it */
            mPeer->mData.attach(mData);
        }
    }
}

/**
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void VRDEServer::i_copyFrom(VRDEServer *aThat)
{
    AssertReturnVoid(aThat != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller(aThat);
    AssertComRCReturnVoid(thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* this will back up current data */
    mData.assignCopy(aThat->mData);
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
