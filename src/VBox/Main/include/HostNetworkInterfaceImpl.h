/* $Id$ */

/** @file
 *
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

#ifndef MAIN_INCLUDED_HostNetworkInterfaceImpl_h
#define MAIN_INCLUDED_HostNetworkInterfaceImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "HostNetworkInterfaceWrap.h"

#ifdef VBOX_WITH_HOSTNETIF_API
struct NETIFINFO;
#endif

class PerformanceCollector;

class ATL_NO_VTABLE HostNetworkInterface :
    public HostNetworkInterfaceWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(HostNetworkInterface)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Utf8Str aInterfaceName, Utf8Str aShortName, Guid aGuid, HostNetworkInterfaceType_T ifType);
#ifdef VBOX_WITH_HOSTNETIF_API
    HRESULT init(Utf8Str aInterfaceName, HostNetworkInterfaceType_T ifType, struct NETIFINFO *pIfs);
    HRESULT updateConfig();
#endif

    HRESULT i_setVirtualBox(VirtualBox *pVirtualBox);
#ifdef RT_OS_WINDOWS
    HRESULT i_updatePersistentConfig();
#endif /* RT_OS_WINDOWS */

#ifdef VBOX_WITH_RESOURCE_USAGE_API
    void i_registerMetrics(PerformanceCollector *aCollector, ComPtr<IUnknown> objptr);
    void i_unregisterMetrics(PerformanceCollector *aCollector, ComPtr<IUnknown> objptr);
#endif

private:

    // Wrapped IHostNetworkInterface properties
    HRESULT getName(com::Utf8Str &aName);
    HRESULT getShortName(com::Utf8Str &aShortName);
    HRESULT getId(com::Guid &aGuiId);
    HRESULT getDHCPEnabled(BOOL *aDHCPEnabled);
    HRESULT getIPAddress(com::Utf8Str &aIPAddress);
    HRESULT getNetworkMask(com::Utf8Str &aNetworkMask);
    HRESULT getIPV6Supported(BOOL *aIPV6Supported);
    HRESULT getIPV6Address(com::Utf8Str &aIPV6Address);
    HRESULT getIPV6NetworkMaskPrefixLength(ULONG *aIPV6NetworkMaskPrefixLength);
    HRESULT getHardwareAddress(com::Utf8Str &aHardwareAddress);
    HRESULT getMediumType(HostNetworkInterfaceMediumType_T *aType);
    HRESULT getStatus(HostNetworkInterfaceStatus_T *aStatus);
    HRESULT getInterfaceType(HostNetworkInterfaceType_T *aType);
    HRESULT getNetworkName(com::Utf8Str &aNetworkName);
    HRESULT getWireless(BOOL *aWireless);

    // Wrapped IHostNetworkInterface methods
    HRESULT enableStaticIPConfig(const com::Utf8Str &aIPAddress,
                                 const com::Utf8Str &aNetworkMask);
    HRESULT enableStaticIPConfigV6(const com::Utf8Str &aIPV6Address,
                                   ULONG aIPV6NetworkMaskPrefixLength);
    HRESULT enableDynamicIPConfig();
    HRESULT dHCPRediscover();

    Utf8Str i_composeNetworkName(const Utf8Str szShortName);

#if defined(RT_OS_WINDOWS)
    HRESULT eraseAdapterConfigParameter(const char *szParamName);
    HRESULT saveAdapterConfigParameter(const char *szParamName, const Utf8Str& strValue);
    HRESULT saveAdapterConfigIPv4Dhcp();
    HRESULT saveAdapterConfigIPv4(ULONG addr, ULONG mask);
    HRESULT saveAdapterConfigIPv6(const Utf8Str& addr, ULONG prefix);
    HRESULT saveAdapterConfig();
    bool    isInConfigFile();
#endif /* defined(RT_OS_WINDOWS) */

    const Utf8Str mInterfaceName;
    const Guid mGuid;
    const Utf8Str mNetworkName;
    const Utf8Str mShortName;
    HostNetworkInterfaceType_T mIfType;

    VirtualBox * const  mVirtualBox;

    struct Data
    {
        Data() : IPAddress(0), networkMask(0), dhcpEnabled(FALSE),
            mediumType(HostNetworkInterfaceMediumType_Unknown),
            status(HostNetworkInterfaceStatus_Down), wireless(FALSE){}

        ULONG IPAddress;
        ULONG networkMask;
        Utf8Str IPV6Address;
        ULONG IPV6NetworkMaskPrefixLength;
        ULONG realIPAddress;
        ULONG realNetworkMask;
        Utf8Str realIPV6Address;
        ULONG realIPV6PrefixLength;
        BOOL dhcpEnabled;
        Utf8Str hardwareAddress;
        HostNetworkInterfaceMediumType_T mediumType;
        HostNetworkInterfaceStatus_T status;
        ULONG speedMbits;
        BOOL wireless;
    } m;

};

typedef std::list<ComObjPtr<HostNetworkInterface> > HostNetworkInterfaceList;

#endif /* !MAIN_INCLUDED_HostNetworkInterfaceImpl_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
