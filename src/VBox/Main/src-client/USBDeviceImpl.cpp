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

#define LOG_GROUP LOG_GROUP_MAIN_USBDEVICE
#include "LoggingNew.h"

#include "USBDeviceImpl.h"

#include "AutoCaller.h"

#include <iprt/cpp/utils.h>


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (OUSBDevice)

HRESULT OUSBDevice::FinalConstruct()
{
    return BaseFinalConstruct();
}

void OUSBDevice::FinalRelease()
{
    uninit ();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the USB device object.
 *
 * @returns COM result indicator
 * @param   aUSBDevice    The USB device (interface) to clone.
 */
HRESULT OUSBDevice::init(IUSBDevice *aUSBDevice)
{
    LogFlowThisFunc(("aUSBDevice=%p\n", aUSBDevice));

    ComAssertRet(aUSBDevice, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT hrc = aUSBDevice->COMGETTER(VendorId)(&unconst(mData.vendorId));
    ComAssertComRCRet(hrc, hrc);
    ComAssertRet(mData.vendorId, E_INVALIDARG);

    hrc = aUSBDevice->COMGETTER(ProductId)(&unconst(mData.productId));
    ComAssertComRCRet(hrc, hrc);

    hrc = aUSBDevice->COMGETTER(Revision)(&unconst(mData.revision));
    ComAssertComRCRet(hrc, hrc);

    BSTR tmp;
    BSTR *bptr = &tmp;

    hrc = aUSBDevice->COMGETTER(Manufacturer)(bptr);
    ComAssertComRCRet(hrc, hrc);
    unconst(mData.manufacturer) = Utf8Str(tmp);

    hrc = aUSBDevice->COMGETTER(Product)(bptr);
    ComAssertComRCRet(hrc, hrc);
    unconst(mData.product) = Utf8Str(tmp);

    hrc = aUSBDevice->COMGETTER(SerialNumber)(bptr);
    ComAssertComRCRet(hrc, hrc);
    unconst(mData.serialNumber) = Utf8Str(tmp);

    hrc = aUSBDevice->COMGETTER(Address)(bptr);
    ComAssertComRCRet(hrc, hrc);
    unconst(mData.address) = Utf8Str(tmp);

    hrc = aUSBDevice->COMGETTER(Backend)(bptr);
    ComAssertComRCRet(hrc, hrc);
    unconst(mData.backend) = Utf8Str(tmp);

    hrc = aUSBDevice->COMGETTER(Port)(&unconst(mData.port));
    ComAssertComRCRet(hrc, hrc);

    hrc = aUSBDevice->COMGETTER(Version)(&unconst(mData.version));
    ComAssertComRCRet(hrc, hrc);

    hrc = aUSBDevice->COMGETTER(PortVersion)(&unconst(mData.portVersion));
    ComAssertComRCRet(hrc, hrc);

    hrc = aUSBDevice->COMGETTER(Speed)(&unconst(mData.speed));
    ComAssertComRCRet(hrc, hrc);

    hrc = aUSBDevice->COMGETTER(Remote)(&unconst(mData.remote));
    ComAssertComRCRet(hrc, hrc);

    Bstr uuid;
    hrc = aUSBDevice->COMGETTER(Id)(uuid.asOutParam());
    ComAssertComRCRet(hrc, hrc);
    unconst(mData.id) = Guid(uuid);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void OUSBDevice::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst(mData.id).clear();

    unconst(mData.vendorId) = 0;
    unconst(mData.productId) = 0;
    unconst(mData.revision) = 0;

    unconst(mData.manufacturer).setNull();
    unconst(mData.product).setNull();
    unconst(mData.serialNumber).setNull();

    unconst(mData.address).setNull();
    unconst(mData.backend).setNull();

    unconst(mData.port) = 0;
    unconst(mData.version) = 1;
    unconst(mData.portVersion) = 1;

    unconst(mData.remote) = FALSE;
}

// IUSBDevice properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns the GUID.
 *
 * @returns COM status code
 * @param   aId   Address of result variable.
 */
HRESULT OUSBDevice::getId(com::Guid &aId)
{
    /* this is const, no need to lock */
    aId = mData.id;

    return S_OK;
}


/**
 * Returns the vendor Id.
 *
 * @returns COM status code
 * @param   aVendorId   Where to store the vendor id.
 */
HRESULT OUSBDevice::getVendorId(USHORT *aVendorId)
{
    /* this is const, no need to lock */
    *aVendorId = mData.vendorId;

    return S_OK;
}


/**
 * Returns the product Id.
 *
 * @returns COM status code
 * @param   aProductId  Where to store the product id.
 */
HRESULT OUSBDevice::getProductId(USHORT *aProductId)
{
    /* this is const, no need to lock */
    *aProductId = mData.productId;

    return S_OK;
}


/**
 * Returns the revision BCD.
 *
 * @returns COM status code
 * @param   aRevision  Where to store the revision BCD.
 */
HRESULT OUSBDevice::getRevision(USHORT *aRevision)
{
    /* this is const, no need to lock */
    *aRevision = mData.revision;

    return S_OK;
}

/**
 * Returns the manufacturer string.
 *
 * @returns COM status code
 * @param   aManufacturer     Where to put the return string.
 */
HRESULT OUSBDevice::getManufacturer(com::Utf8Str &aManufacturer)
{
    /* this is const, no need to lock */
    aManufacturer = mData.manufacturer;

    return S_OK;
}


/**
 * Returns the product string.
 *
 * @returns COM status code
 * @param   aProduct          Where to put the return string.
 */
HRESULT OUSBDevice::getProduct(com::Utf8Str &aProduct)
{
    /* this is const, no need to lock */
    aProduct = mData.product;

    return S_OK;
}


/**
 * Returns the serial number string.
 *
 * @returns COM status code
 * @param   aSerialNumber     Where to put the return string.
 */
HRESULT OUSBDevice::getSerialNumber(com::Utf8Str &aSerialNumber)
{
    /* this is const, no need to lock */
    aSerialNumber = mData.serialNumber;

    return S_OK;
}


/**
 * Returns the host specific device address.
 *
 * @returns COM status code
 * @param   aAddress          Where to put the return string.
 */
HRESULT OUSBDevice::getAddress(com::Utf8Str &aAddress)
{
    /* this is const, no need to lock */
    aAddress = mData.address;

    return S_OK;
}

HRESULT OUSBDevice::getPort(USHORT *aPort)
{
    /* this is const, no need to lock */
    *aPort = mData.port;

    return S_OK;
}

HRESULT OUSBDevice::getVersion(USHORT *aVersion)
{
    /* this is const, no need to lock */
    *aVersion = mData.version;

    return S_OK;
}

HRESULT OUSBDevice::getPortVersion(USHORT *aPortVersion)
{
    /* this is const, no need to lock */
    *aPortVersion = mData.portVersion;

    return S_OK;
}

HRESULT OUSBDevice::getSpeed(USBConnectionSpeed_T *aSpeed)
{
    /* this is const, no need to lock */
    *aSpeed = mData.speed;

    return S_OK;
}

HRESULT OUSBDevice::getRemote(BOOL *aRemote)
{
    /* this is const, no need to lock */
    *aRemote = mData.remote;

    return S_OK;
}

/**
 * Returns the device specific backend.
 *
 * @returns COM status code
 * @param   aBackend          Where to put the return string.
 */
HRESULT OUSBDevice::getBackend(com::Utf8Str &aBackend)
{
    /* this is const, no need to lock */
    aBackend = mData.backend;

    return S_OK;
}

HRESULT OUSBDevice::getDeviceInfo(std::vector<com::Utf8Str> &aInfo)
{
    /* this is const, no need to lock */
    aInfo.resize(2);
    aInfo[0] = mData.manufacturer;
    aInfo[1] = mData.product;

    return S_OK;
}

// private methods
/////////////////////////////////////////////////////////////////////////////
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
