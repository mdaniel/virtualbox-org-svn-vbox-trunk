/* $Id$ */
/** @file
 * Implementation of IHost.
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

#ifndef ____H_HOSTIMPL
#define ____H_HOSTIMPL
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "HostWrap.h"

class HostUSBDeviceFilter;
class USBProxyService;
class SessionMachine;
class Progress;
class PerformanceCollector;

namespace settings
{
    struct Host;
}

#include <list>

class ATL_NO_VTABLE Host :
    public HostWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(Host)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(VirtualBox *aParent);
    void uninit();

    // public methods only for internal purposes

    /**
     * Override of the default locking class to be used for validating lock
     * order with the standard member lock handle.
     */
    virtual VBoxLockingClass getLockingClass() const
    {
        return LOCKCLASS_HOSTOBJECT;
    }

    HRESULT i_loadSettings(const settings::Host &data);
    HRESULT i_saveSettings(settings::Host &data);

    HRESULT i_getDrives(DeviceType_T mediumType, bool fRefresh, MediaList *&pll, AutoWriteLock &treeLock);
    HRESULT i_findHostDriveById(DeviceType_T mediumType, const Guid &uuid, bool fRefresh, ComObjPtr<Medium> &pMedium);
    HRESULT i_findHostDriveByName(DeviceType_T mediumType, const Utf8Str &strLocationFull, bool fRefresh, ComObjPtr<Medium> &pMedium);

#ifdef VBOX_WITH_USB
    typedef std::list< ComObjPtr<HostUSBDeviceFilter> > USBDeviceFilterList;

    /** Must be called from under this object's lock. */
    USBProxyService* i_usbProxyService();

    HRESULT i_addChild(HostUSBDeviceFilter *pChild);
    HRESULT i_removeChild(HostUSBDeviceFilter *pChild);
    VirtualBox* i_parent();

    HRESULT i_onUSBDeviceFilterChange(HostUSBDeviceFilter *aFilter, BOOL aActiveChanged = FALSE);
    void i_getUSBFilters(USBDeviceFilterList *aGlobalFiltes);
    HRESULT i_checkUSBProxyService();
#endif /* !VBOX_WITH_USB */

    static void i_generateMACAddress(Utf8Str &mac);

private:

    // wrapped IHost properties
    HRESULT getDVDDrives(std::vector<ComPtr<IMedium> > &aDVDDrives);
    HRESULT getFloppyDrives(std::vector<ComPtr<IMedium> > &aFloppyDrives);
    HRESULT getUSBDevices(std::vector<ComPtr<IHostUSBDevice> > &aUSBDevices);
    HRESULT getUSBDeviceFilters(std::vector<ComPtr<IHostUSBDeviceFilter> > &aUSBDeviceFilters);
    HRESULT getNetworkInterfaces(std::vector<ComPtr<IHostNetworkInterface> > &aNetworkInterfaces);
    HRESULT getNameServers(std::vector<com::Utf8Str> &aNameServers);
    HRESULT getDomainName(com::Utf8Str &aDomainName);
    HRESULT getSearchStrings(std::vector<com::Utf8Str> &aSearchStrings);
    HRESULT getProcessorCount(ULONG *aProcessorCount);
    HRESULT getProcessorOnlineCount(ULONG *aProcessorOnlineCount);
    HRESULT getProcessorCoreCount(ULONG *aProcessorCoreCount);
    HRESULT getProcessorOnlineCoreCount(ULONG *aProcessorOnlineCoreCount);
    HRESULT getMemorySize(ULONG *aMemorySize);
    HRESULT getMemoryAvailable(ULONG *aMemoryAvailable);
    HRESULT getOperatingSystem(com::Utf8Str &aOperatingSystem);
    HRESULT getOSVersion(com::Utf8Str &aOSVersion);
    HRESULT getUTCTime(LONG64 *aUTCTime);
    HRESULT getAcceleration3DAvailable(BOOL *aAcceleration3DAvailable);
    HRESULT getVideoInputDevices(std::vector<ComPtr<IHostVideoInputDevice> > &aVideoInputDevices);

    // wrapped IHost methods
    HRESULT getProcessorSpeed(ULONG aCpuId,
                              ULONG *aSpeed);
    HRESULT getProcessorFeature(ProcessorFeature_T aFeature,
                                BOOL *aSupported);
    HRESULT getProcessorDescription(ULONG aCpuId,
                                    com::Utf8Str &aDescription);
    HRESULT getProcessorCPUIDLeaf(ULONG aCpuId,
                                  ULONG aLeaf,
                                  ULONG aSubLeaf,
                                  ULONG *aValEax,
                                  ULONG *aValEbx,
                                  ULONG *aValEcx,
                                  ULONG *aValEdx);
    HRESULT createHostOnlyNetworkInterface(ComPtr<IHostNetworkInterface> &aHostInterface,
                                           ComPtr<IProgress> &aProgress);
    HRESULT removeHostOnlyNetworkInterface(const com::Guid &aId,
                                           ComPtr<IProgress> &aProgress);
    HRESULT createUSBDeviceFilter(const com::Utf8Str &aName,
                                  ComPtr<IHostUSBDeviceFilter> &aFilter);
    HRESULT insertUSBDeviceFilter(ULONG aPosition,
                                  const ComPtr<IHostUSBDeviceFilter> &aFilter);
    HRESULT removeUSBDeviceFilter(ULONG aPosition);
    HRESULT findHostDVDDrive(const com::Utf8Str &aName,
                             ComPtr<IMedium> &aDrive);
    HRESULT findHostFloppyDrive(const com::Utf8Str &aName,
                                ComPtr<IMedium> &aDrive);
    HRESULT findHostNetworkInterfaceByName(const com::Utf8Str &aName,
                                           ComPtr<IHostNetworkInterface> &aNetworkInterface);
    HRESULT findHostNetworkInterfaceById(const com::Guid &aId,
                                         ComPtr<IHostNetworkInterface> &aNetworkInterface);
    HRESULT findHostNetworkInterfacesOfType(HostNetworkInterfaceType_T aType,
                                            std::vector<ComPtr<IHostNetworkInterface> > &aNetworkInterfaces);
    HRESULT findUSBDeviceById(const com::Guid &aId,
                              ComPtr<IHostUSBDevice> &aDevice);
    HRESULT findUSBDeviceByAddress(const com::Utf8Str &aName,
                                   ComPtr<IHostUSBDevice> &aDevice);
    HRESULT generateMACAddress(com::Utf8Str &aAddress);

    HRESULT addUSBDeviceSource(const com::Utf8Str &aBackend, const com::Utf8Str &aId, const com::Utf8Str &aAddress,
                               const std::vector<com::Utf8Str> &aPropertyNames, const std::vector<com::Utf8Str> &aPropertyValues);

    HRESULT removeUSBDeviceSource(const com::Utf8Str &aId);

    // Internal Methods.

    HRESULT i_buildDVDDrivesList(MediaList &list);
    HRESULT i_buildFloppyDrivesList(MediaList &list);
    HRESULT i_findHostDriveByNameOrId(DeviceType_T mediumType, const Utf8Str &strNameOrId, ComObjPtr<Medium> &pMedium);

#if defined(RT_OS_SOLARIS) && defined(VBOX_USE_LIBHAL)
    bool i_getDVDInfoFromHal(std::list< ComObjPtr<Medium> > &list);
    bool i_getFloppyInfoFromHal(std::list< ComObjPtr<Medium> > &list);
#endif

#if defined(RT_OS_SOLARIS)
    void i_getDVDInfoFromDevTree(std::list< ComObjPtr<Medium> > &list);
    void i_parseMountTable(char *mountTable, std::list< ComObjPtr<Medium> > &list);
    bool i_validateDevice(const char *deviceNode, bool isCDROM);
#endif

    HRESULT i_updateNetIfList();

#ifndef RT_OS_WINDOWS
    HRESULT i_parseResolvConf();
#else
    HRESULT i_fetchNameResolvingInformation();
#endif

#ifdef VBOX_WITH_RESOURCE_USAGE_API
    void i_registerMetrics(PerformanceCollector *aCollector);
    void i_registerDiskMetrics(PerformanceCollector *aCollector);
    void i_unregisterMetrics(PerformanceCollector *aCollector);
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

    struct Data;        // opaque data structure, defined in HostImpl.cpp
    Data *m;
};

#endif // !____H_HOSTIMPL

