/* $Id$ */
/** @file
 * VBox USBDeviceFilters COM Class declaration.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_USBDEVICEFILTERSIMPL
#define ____H_USBDEVICEFILTERSIMPL
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "USBDeviceFiltersWrap.h"

class HostUSBDevice;
class USBDeviceFilter;

namespace settings
{
    struct USB;
}

class ATL_NO_VTABLE USBDeviceFilters :
    public USBDeviceFiltersWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(USBDeviceFilters)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aParent);
    HRESULT init(Machine *aParent, USBDeviceFilters *aThat);
    HRESULT initCopy(Machine *aParent, USBDeviceFilters *aThat);
    void uninit();

    // public methods only for internal purposes
    HRESULT i_loadSettings(const settings::USB &data);
    HRESULT i_saveSettings(settings::USB &data);

    void i_rollback();
    void i_commit();
    void i_copyFrom(USBDeviceFilters *aThat);

#ifdef VBOX_WITH_USB
    HRESULT i_onDeviceFilterChange(USBDeviceFilter *aFilter,
                                   BOOL aActiveChanged = FALSE);
    bool i_hasMatchingFilter(const ComObjPtr<HostUSBDevice> &aDevice, ULONG *aMaskedIfs);
    bool i_hasMatchingFilter(IUSBDevice *aUSBDevice, ULONG *aMaskedIfs);
    HRESULT i_notifyProxy(bool aInsertFilters);
#endif /* VBOX_WITH_USB */

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)
    Machine* i_getMachine();

private:

    // Wrapped IUSBDeviceFilters attributes
    HRESULT getDeviceFilters(std::vector<ComPtr<IUSBDeviceFilter> > &aDeviceFilters);

    // wrapped IUSBDeviceFilters methods
    HRESULT createDeviceFilter(const com::Utf8Str &aName,
                               ComPtr<IUSBDeviceFilter> &aFilter);
    HRESULT insertDeviceFilter(ULONG aPosition,
                               const ComPtr<IUSBDeviceFilter> &aFilter);
    HRESULT removeDeviceFilter(ULONG aPosition,
                               ComPtr<IUSBDeviceFilter> &aFilter);
    struct Data;
    Data *m;
};

#endif //!____H_USBDEVICEFILTERSIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
