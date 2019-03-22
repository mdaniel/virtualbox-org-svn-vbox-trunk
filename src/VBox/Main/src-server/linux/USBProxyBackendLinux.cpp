/* $Id$ */
/** @file
 * VirtualBox USB Proxy Service, Linux Specialization.
 */

/*
 * Copyright (C) 2005-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "USBProxyService.h"
#include "USBGetDevices.h"
#include "Logging.h"

#include <VBox/usb.h>
#include <VBox/usblib.h>
#include <VBox/err.h>

#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/pipe.h>
#include <iprt/stream.h>
#include <iprt/linux/sysfs.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/statfs.h>
#include <sys/poll.h>
#ifdef VBOX_WITH_LINUX_COMPILER_H
# include <linux/compiler.h>
#endif
#include <linux/usbdevice_fs.h>


/**
 * Initialize data members.
 */
USBProxyBackendLinux::USBProxyBackendLinux()
    : USBProxyBackend(), mhFile(NIL_RTFILE), mhWakeupPipeR(NIL_RTPIPE), mhWakeupPipeW(NIL_RTPIPE), mpWaiter(NULL)
{
    LogFlowThisFunc(("\n"));
}


/**
 * Stop all service threads and free the device chain.
 */
USBProxyBackendLinux::~USBProxyBackendLinux()
{
    LogFlowThisFunc(("\n"));
}

/**
 * Initializes the object (called right after construction).
 *
 * @returns VBox status code.
 */
int USBProxyBackendLinux::init(USBProxyService *pUsbProxyService, const com::Utf8Str &strId,
                              const com::Utf8Str &strAddress, bool fLoadingSettings)
{
    USBProxyBackend::init(pUsbProxyService, strId, strAddress, fLoadingSettings);

    unconst(m_strBackend) = Utf8Str("host");

    const char *pcszDevicesRoot;
    int rc = USBProxyLinuxChooseMethod(&mUsingUsbfsDevices, &pcszDevicesRoot);
    if (RT_SUCCESS(rc))
    {
        mDevicesRoot = pcszDevicesRoot;
        rc = mUsingUsbfsDevices ? initUsbfs() : initSysfs();
        /* For the day when we have VBoxSVC release logging... */
        LogRel((RT_SUCCESS(rc) ? "Successfully initialised host USB using %s\n"
                               : "Failed to initialise host USB using %s\n",
                mUsingUsbfsDevices ? "USBFS" : "sysfs"));
    }

    return rc;
}

void USBProxyBackendLinux::uninit()
{
    /*
     * Stop the service.
     */
    if (isActive())
        stop();

    /*
     * Free resources.
     */
    doUsbfsCleanupAsNeeded();
#ifdef VBOX_USB_WITH_SYSFS
    if (mpWaiter)
        delete mpWaiter;
#endif

    USBProxyBackend::uninit();
}


/**
 * Initialization routine for the usbfs based operation.
 *
 * @returns iprt status code.
 */
int USBProxyBackendLinux::initUsbfs(void)
{
    Assert(mUsingUsbfsDevices);

    /*
     * Open the devices file.
     */
    int rc;
    char *pszDevices = RTPathJoinA(mDevicesRoot.c_str(), "devices");
    if (pszDevices)
    {
        rc = RTFileOpen(&mhFile, pszDevices, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc))
        {
            rc = RTPipeCreate(&mhWakeupPipeR, &mhWakeupPipeW, 0 /*fFlags*/);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Start the poller thread.
                 */
                rc = start();
                if (RT_SUCCESS(rc))
                {
                    RTStrFree(pszDevices);
                    LogFlowThisFunc(("returns successfully\n"));
                    return VINF_SUCCESS;
                }

                RTPipeClose(mhWakeupPipeR);
                RTPipeClose(mhWakeupPipeW);
                mhWakeupPipeW = mhWakeupPipeR = NIL_RTPIPE;
            }
            else
                Log(("USBProxyBackendLinux::USBProxyBackendLinux: RTFilePipe failed with rc=%Rrc\n", rc));
            RTFileClose(mhFile);
        }

        RTStrFree(pszDevices);
    }
    else
    {
        rc = VERR_NO_MEMORY;
        Log(("USBProxyBackendLinux::USBProxyBackendLinux: out of memory!\n"));
    }

    LogFlowThisFunc(("returns failure!!! (rc=%Rrc)\n", rc));
    return rc;
}


/**
 * Initialization routine for the sysfs based operation.
 *
 * @returns iprt status code
 */
int USBProxyBackendLinux::initSysfs(void)
{
    Assert(!mUsingUsbfsDevices);

#ifdef VBOX_USB_WITH_SYSFS
    try
    {
        mpWaiter = new VBoxMainHotplugWaiter(mDevicesRoot.c_str());
    }
    catch(std::bad_alloc &e)
    {
        return VERR_NO_MEMORY;
    }
    int rc = mpWaiter->getStatus();
    if (RT_SUCCESS(rc) || rc == VERR_TIMEOUT || rc == VERR_TRY_AGAIN)
        rc = start();
    else if (rc == VERR_NOT_SUPPORTED)
        /* This can legitimately happen if hal or DBus are not running, but of
         * course we can't start in this case. */
        rc = VINF_SUCCESS;
    return rc;

#else  /* !VBOX_USB_WITH_SYSFS */
    return VERR_NOT_IMPLEMENTED;
#endif /* !VBOX_USB_WITH_SYSFS */
}


/**
 * If any Usbfs-related resources are currently allocated, then free them
 * and mark them as freed.
 */
void USBProxyBackendLinux::doUsbfsCleanupAsNeeded()
{
    /*
     * Free resources.
     */
    if (mhFile != NIL_RTFILE)
        RTFileClose(mhFile);
    mhFile = NIL_RTFILE;

    if (mhWakeupPipeR != NIL_RTPIPE)
        RTPipeClose(mhWakeupPipeR);
    if (mhWakeupPipeW != NIL_RTPIPE)
        RTPipeClose(mhWakeupPipeW);
    mhWakeupPipeW = mhWakeupPipeR = NIL_RTPIPE;
}


int USBProxyBackendLinux::captureDevice(HostUSBDevice *aDevice)
{
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);

    AutoReadLock devLock(aDevice COMMA_LOCKVAL_SRC_POS);
    LogFlowThisFunc(("aDevice=%s\n", aDevice->i_getName().c_str()));

    /*
     * Don't think we need to do anything when the device is held... fake it.
     */
    Assert(aDevice->i_getUnistate() == kHostUSBDeviceState_Capturing);
    devLock.release();
    interruptWait();

    return VINF_SUCCESS;
}


int USBProxyBackendLinux::releaseDevice(HostUSBDevice *aDevice)
{
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);

    AutoReadLock devLock(aDevice COMMA_LOCKVAL_SRC_POS);
    LogFlowThisFunc(("aDevice=%s\n", aDevice->i_getName().c_str()));

    /*
     * We're not really holding it atm., just fake it.
     */
    Assert(aDevice->i_getUnistate() == kHostUSBDeviceState_ReleasingToHost);
    devLock.release();
    interruptWait();

    return VINF_SUCCESS;
}


/**
 * A device was added, we need to adjust mUdevPolls.
 */
void USBProxyBackendLinux::deviceAdded(ComObjPtr<HostUSBDevice> &aDevice, PUSBDEVICE pDev)
{
    AssertReturnVoid(aDevice);
    AssertReturnVoid(!aDevice->isWriteLockOnCurrentThread());
    AutoReadLock devLock(aDevice COMMA_LOCKVAL_SRC_POS);
    if (pDev->enmState == USBDEVICESTATE_USED_BY_HOST)
    {
        LogRel(("USBProxyBackendLinux: Device %04x:%04x (%s) isn't accessible. giving udev a few seconds to fix this...\n",
                pDev->idVendor, pDev->idProduct, pDev->pszAddress));
        mUdevPolls = 10; /* (10 * 500ms = 5s) */
    }

    devLock.release();
}


bool USBProxyBackendLinux::isFakeUpdateRequired()
{
    return true;
}

int USBProxyBackendLinux::wait(RTMSINTERVAL aMillies)
{
    int rc;
    if (mUsingUsbfsDevices)
        rc = waitUsbfs(aMillies);
    else
        rc = waitSysfs(aMillies);
    return rc;
}


/** String written to the wakeup pipe. */
#define WAKE_UP_STRING      "WakeUp!"
/** Length of the string written. */
#define WAKE_UP_STRING_LEN  ( sizeof(WAKE_UP_STRING) - 1 )

int USBProxyBackendLinux::waitUsbfs(RTMSINTERVAL aMillies)
{
    struct pollfd PollFds[2];

    /* Cap the wait interval if we're polling for udevd changing device permissions. */
    if (aMillies > 500 && mUdevPolls > 0)
    {
        mUdevPolls--;
        aMillies = 500;
    }

    RT_ZERO(PollFds);
    PollFds[0].fd        = (int)RTFileToNative(mhFile);
    PollFds[0].events    = POLLIN;
    PollFds[1].fd        = (int)RTPipeToNative(mhWakeupPipeR);
    PollFds[1].events    = POLLIN | POLLERR | POLLHUP;

    int rc = poll(&PollFds[0], 2, aMillies);
    if (rc == 0)
        return VERR_TIMEOUT;
    if (rc > 0)
    {
        /* drain the pipe */
        if (PollFds[1].revents & POLLIN)
        {
            char szBuf[WAKE_UP_STRING_LEN];
            rc = RTPipeReadBlocking(mhWakeupPipeR, szBuf, sizeof(szBuf), NULL);
            AssertRC(rc);
        }
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}


int USBProxyBackendLinux::waitSysfs(RTMSINTERVAL aMillies)
{
#ifdef VBOX_USB_WITH_SYSFS
    int rc = mpWaiter->Wait(aMillies);
    if (rc == VERR_TRY_AGAIN)
    {
        RTThreadYield();
        rc = VINF_SUCCESS;
    }
    return rc;
#else  /* !VBOX_USB_WITH_SYSFS */
    return USBProxyService::wait(aMillies);
#endif /* !VBOX_USB_WITH_SYSFS */
}


int USBProxyBackendLinux::interruptWait(void)
{
    AssertReturn(!isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
#ifdef VBOX_USB_WITH_SYSFS
    LogFlowFunc(("mUsingUsbfsDevices=%d\n", mUsingUsbfsDevices));
    if (!mUsingUsbfsDevices)
    {
        mpWaiter->Interrupt();
        LogFlowFunc(("Returning VINF_SUCCESS\n"));
        return VINF_SUCCESS;
    }
#endif /* VBOX_USB_WITH_SYSFS */
    int rc = RTPipeWriteBlocking(mhWakeupPipeW, WAKE_UP_STRING, WAKE_UP_STRING_LEN, NULL);
    if (RT_SUCCESS(rc))
        RTPipeFlush(mhWakeupPipeW);
    LogFlowFunc(("returning %Rrc\n", rc));
    return rc;
}


PUSBDEVICE USBProxyBackendLinux::getDevices(void)
{
    return USBProxyLinuxGetDevices(mDevicesRoot.c_str(), !mUsingUsbfsDevices);
}
