/* $Id$ */
/** @file
 * VBoxDD - Built-in drivers & devices (part 1).
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV
#include <VBox/vmm/pdm.h>
#include <VBox/version.h>
#include <iprt/errcore.h>
#include <VBox/usb.h>

#include <VBox/log.h>
#include <iprt/assert.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
const void *g_apvVBoxDDDependencies[] =
{
    NULL,
};


/**
 * Register builtin devices.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
extern "C" DECLEXPORT(int) VBoxDevicesRegister(PPDMDEVREGCB pCallbacks, uint32_t u32Version)
{
    LogFlow(("VBoxDevicesRegister: u32Version=%#x\n", u32Version));
    AssertReleaseMsg(u32Version == VBOX_VERSION, ("u32Version=%#x VBOX_VERSION=%#x\n", u32Version, VBOX_VERSION));
    int rc;

    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePCI);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePciIch9);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePcArch);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePcBios);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceIOAPIC);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePS2KeyboardMouse);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePIIX3IDE);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceI8254);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceI8259);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceHPET);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceSmc);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceFlash);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_EFI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceEFI);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceMC146818);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceVga);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceVMMDev);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePCNet);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_E1000
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceE1000);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_VIRTIO
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceVirtioNet);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_INIP
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceINIP);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceICHAC97);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceSB16);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceHDA);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_VUSB
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceOHCI);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_EHCI_IMPL
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceEHCI);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_XHCI_IMPL
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceXHCI);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_ACPI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceACPI);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceDMA);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceFloppyController);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceSerialPort);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceOxPcie958);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceParallelPort);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_AHCI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceAHCI);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_BUSLOGIC
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceBusLogic);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePCIBridge);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePciIch9Bridge);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_LSILOGIC
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceLsiLogicSCSI);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceLsiLogicSAS);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_NVME_IMPL
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceNVMe);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_PCI_PASSTHROUGH_IMPL
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePciRaw);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceGIMDev);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_NEW_LPC_DEVICE
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceLPC);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_VIRTUALKD
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceVirtualKD);
    if (RT_FAILURE(rc))
        return rc;
#endif

    return VINF_SUCCESS;
}


/**
 * Register builtin drivers.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
extern "C" DECLEXPORT(int) VBoxDriversRegister(PCPDMDRVREGCB pCallbacks, uint32_t u32Version)
{
    LogFlow(("VBoxDriversRegister: u32Version=%#x\n", u32Version));
    AssertReleaseMsg(u32Version == VBOX_VERSION, ("u32Version=%#x VBOX_VERSION=%#x\n", u32Version, VBOX_VERSION));

    int rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvMouseQueue);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvKeyboardQueue);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvVD);
    if (RT_FAILURE(rc))
        return rc;
#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS) || defined(RT_OS_FREEBSD)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostDVD);
    if (RT_FAILURE(rc))
        return rc;
#endif
#if defined(RT_OS_LINUX) || defined(RT_OS_WINDOWS)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostFloppy);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvNAT);
    if (RT_FAILURE(rc))
        return rc;
#if defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostInterface);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_UDPTUNNEL
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvUDPTunnel);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_VDE
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvVDE);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvIntNet);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvDedicatedNic);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvNetSniffer);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_NETSHAPER
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvNetShaper);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvAUDIO);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_AUDIO_DEBUG
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostDebugAudio);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_AUDIO_VALIDATIONKIT
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostValidationKitAudio);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostNullAudio);
    if (RT_FAILURE(rc))
        return rc;
#if defined(RT_OS_WINDOWS)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostDSound);
    if (RT_FAILURE(rc))
        return rc;
#endif
#if defined(RT_OS_DARWIN)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostCoreAudio);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_AUDIO_ALSA
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostALSAAudio);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_AUDIO_OSS
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostOSSAudio);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_AUDIO_PULSE
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostPulseAudio);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvACPI);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvAcpiCpu);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_VUSB
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvVUSBRootHub);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_USB_VIDEO_IMPL
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostWebcam);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvNamedPipe);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvTCP);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvUDP);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvRawFile);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvChar);
    if (RT_FAILURE(rc))
        return rc;
#if defined(RT_OS_LINUX) || defined(VBOX_WITH_WIN_PARPORT_SUP)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostParallel);
    if (RT_FAILURE(rc))
        return rc;
#endif
#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS) || defined(RT_OS_FREEBSD)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostSerial);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_SCSI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvSCSI);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_DRV_DISK_INTEGRITY
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvDiskIntegrity);
    if (RT_FAILURE(rc))
        return rc;

    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvRamDisk);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_PCI_PASSTHROUGH_IMPL
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvPciRaw);
    if (RT_FAILURE(rc))
        return rc;
#endif

    return VINF_SUCCESS;
}


/**
 * Register builtin USB device.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
extern "C" DECLEXPORT(int) VBoxUsbRegister(PCPDMUSBREGCB pCallbacks, uint32_t u32Version)
{
    int rc = VINF_SUCCESS;
    RT_NOREF1(u32Version);

#ifdef VBOX_WITH_USB
    rc = pCallbacks->pfnRegister(pCallbacks, &g_UsbDevProxy);
    if (RT_FAILURE(rc))
        return rc;
# ifdef VBOX_WITH_SCSI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_UsbMsd);
    if (RT_FAILURE(rc))
        return rc;
# endif
#endif
#ifdef VBOX_WITH_VUSB
    rc = pCallbacks->pfnRegister(pCallbacks, &g_UsbHidKbd);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_UsbHidMou);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_USB_VIDEO_IMPL
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevWebcam);
    if (RT_FAILURE(rc))
        return rc;
#endif

    return rc;
}
