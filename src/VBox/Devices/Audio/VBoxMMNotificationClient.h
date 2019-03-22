/* $Id$ */
/** @file
 * VBoxMMNotificationClient.h - Implementation of the IMMNotificationClient interface
 *                              to detect audio endpoint changes.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBOX_MMNOTIFICATIONCLIENT_H___
#define ___VBOX_MMNOTIFICATIONCLIENT_H___

#include <iprt/critsect.h>
#include <iprt/win/windows.h>

#include <Mmdeviceapi.h>

class VBoxMMNotificationClient : IMMNotificationClient
{
public:

    VBoxMMNotificationClient();
    virtual ~VBoxMMNotificationClient();

    HRESULT Initialize();
    void    Dispose();

    /** @name IUnknown interface
     * @{ */
    IFACEMETHODIMP_(ULONG) Release();
    /** @} */

private:

    bool                 m_fRegisteredClient;
    IMMDeviceEnumerator *m_pEnum;
    IMMDevice           *m_pEndpoint;

    long                 m_cRef;

    HRESULT AttachToDefaultEndpoint();
    void    DetachFromEndpoint();

    /** @name IMMNotificationClient interface
     * @{ */
    IFACEMETHODIMP OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState);
    IFACEMETHODIMP OnDeviceAdded(LPCWSTR pwstrDeviceId);
    IFACEMETHODIMP OnDeviceRemoved(LPCWSTR pwstrDeviceId);
    IFACEMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId);
    IFACEMETHODIMP OnPropertyValueChanged(LPCWSTR /*pwstrDeviceId*/, const PROPERTYKEY /*key*/) { return S_OK; }
    IFACEMETHODIMP OnDeviceQueryRemove() { return S_OK; }
    IFACEMETHODIMP OnDeviceQueryRemoveFailed() { return S_OK; }
    IFACEMETHODIMP OnDeviceRemovePending() { return S_OK; }
    /** @} */

    /** @name IUnknown interface
     * @{ */
    IFACEMETHODIMP QueryInterface(const IID& iid, void** ppUnk);
    IFACEMETHODIMP_(ULONG) AddRef();
    /** @} */
};
#endif /* ___VBOX_MMNOTIFICATIONCLIENT_H___ */

