/* $Id$ */
/** @file
 * Built-in drivers & devices (part 1) header.
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

#ifndef ___build_VBoxDD_h
#define ___build_VBoxDD_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/vmm/pdm.h>

RT_C_DECLS_BEGIN

/** The default BIOS logo data. */
extern const unsigned char  g_abVgaDefBiosLogo[];
extern const unsigned char  g_abVgaDefBiosLogoNY[];
/** The size of the default BIOS logo data. */
extern const unsigned       g_cbVgaDefBiosLogo;
extern const unsigned       g_cbVgaDefBiosLogoNY;


extern const PDMDEVREG g_DevicePCI;
extern const PDMDEVREG g_DevicePciIch9;
extern const PDMDEVREG g_DevicePcArch;
extern const PDMDEVREG g_DevicePcBios;
extern const PDMDEVREG g_DeviceIOAPIC;
extern const PDMDEVREG g_DevicePS2KeyboardMouse;
extern const PDMDEVREG g_DeviceI8254;
extern const PDMDEVREG g_DeviceI8259;
extern const PDMDEVREG g_DeviceHPET;
extern const PDMDEVREG g_DeviceSmc;
extern const PDMDEVREG g_DeviceFlash;
extern const PDMDEVREG g_DeviceMC146818;
extern const PDMDEVREG g_DevicePIIX3IDE;
extern const PDMDEVREG g_DeviceFloppyController;
extern const PDMDEVREG g_DeviceVga;
extern const PDMDEVREG g_DeviceVMMDev;
extern const PDMDEVREG g_DevicePCNet;
#ifdef VBOX_WITH_E1000
extern const PDMDEVREG g_DeviceE1000;
#endif
#ifdef VBOX_WITH_VIRTIO
extern const PDMDEVREG g_DeviceVirtioNet;
#endif
#ifdef VBOX_WITH_INIP
extern const PDMDEVREG g_DeviceINIP;
#endif
extern const PDMDEVREG g_DeviceICHAC97;
extern const PDMDEVREG g_DeviceSB16;
extern const PDMDEVREG g_DeviceHDA;
extern const PDMDEVREG g_DeviceOHCI;
extern const PDMDEVREG g_DeviceEHCI;
extern const PDMDEVREG g_DeviceXHCI;
extern const PDMDEVREG g_DeviceACPI;
extern const PDMDEVREG g_DeviceDMA;
extern const PDMDEVREG g_DeviceFloppyController;
extern const PDMDEVREG g_DeviceSerialPort;
extern const PDMDEVREG g_DeviceOxPcie958;
extern const PDMDEVREG g_DeviceParallelPort;
#ifdef VBOX_WITH_AHCI
extern const PDMDEVREG g_DeviceAHCI;
#endif
#ifdef VBOX_WITH_BUSLOGIC
extern const PDMDEVREG g_DeviceBusLogic;
#endif
extern const PDMDEVREG g_DevicePCIBridge;
extern const PDMDEVREG g_DevicePciIch9Bridge;
#ifdef VBOX_WITH_LSILOGIC
extern const PDMDEVREG g_DeviceLsiLogicSCSI;
extern const PDMDEVREG g_DeviceLsiLogicSAS;
#endif
#ifdef VBOX_WITH_NVME_IMPL
extern const PDMDEVREG g_DeviceNVMe;
#endif
#ifdef VBOX_WITH_EFI
extern const PDMDEVREG g_DeviceEFI;
#endif
#ifdef VBOX_WITH_PCI_PASSTHROUGH_IMPL
extern const PDMDEVREG g_DevicePciRaw;
#endif
extern const PDMDEVREG g_DeviceGIMDev;
#ifdef VBOX_WITH_NEW_LPC_DEVICE
extern const PDMDEVREG g_DeviceLPC;
#endif
#ifdef VBOX_WITH_VIRTUALKD
extern const PDMDEVREG g_DeviceVirtualKD;
#endif

extern const PDMDRVREG g_DrvMouseQueue;
extern const PDMDRVREG g_DrvKeyboardQueue;
extern const PDMDRVREG g_DrvVBoxHDD;
extern const PDMDRVREG g_DrvVD;
extern const PDMDRVREG g_DrvHostDVD;
extern const PDMDRVREG g_DrvHostFloppy;
extern const PDMDRVREG g_DrvISCSI;
extern const PDMDRVREG g_DrvISCSITransportTcp;
#if defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)
extern const PDMDRVREG g_DrvHostInterface;
#endif
#ifdef VBOX_WITH_UDPTUNNEL
extern const PDMDRVREG g_DrvUDPTunnel;
#endif
#ifdef VBOX_WITH_VDE
extern const PDMDRVREG g_DrvVDE;
#endif
extern const PDMDRVREG g_DrvIntNet;
extern const PDMDRVREG g_DrvDedicatedNic;
extern const PDMDRVREG g_DrvNAT;
#ifdef VBOX_WITH_NETSHAPER
extern const PDMDRVREG g_DrvNetShaper;
#endif /* VBOX_WITH_NETSHAPER */
extern const PDMDRVREG g_DrvNetSniffer;
extern const PDMDRVREG g_DrvAUDIO;
#ifdef VBOX_WITH_AUDIO_DEBUG
extern const PDMDRVREG g_DrvHostDebugAudio;
#endif
#ifdef VBOX_WITH_AUDIO_VALIDATIONKIT
extern const PDMDRVREG g_DrvHostValidationKitAudio;
#endif
extern const PDMDRVREG g_DrvHostNullAudio;
#if defined(RT_OS_WINDOWS)
extern const PDMDRVREG g_DrvHostDSound;
#endif
#if defined(RT_OS_DARWIN)
extern const PDMDRVREG g_DrvHostCoreAudio;
#endif
#ifdef VBOX_WITH_AUDIO_OSS
extern const PDMDRVREG g_DrvHostOSSAudio;
#endif
#ifdef VBOX_WITH_AUDIO_ALSA
extern const PDMDRVREG g_DrvHostALSAAudio;
#endif
#ifdef VBOX_WITH_AUDIO_PULSE
extern const PDMDRVREG g_DrvHostPulseAudio;
#endif
extern const PDMDRVREG g_DrvACPI;
extern const PDMDRVREG g_DrvAcpiCpu;
extern const PDMDRVREG g_DrvVUSBRootHub;
#ifdef VBOX_WITH_USB_VIDEO_IMPL
extern const PDMDRVREG g_DrvHostWebcam;
#endif
extern const PDMDRVREG g_DrvChar;
extern const PDMDRVREG g_DrvNamedPipe;
extern const PDMDRVREG g_DrvTCP;
extern const PDMDRVREG g_DrvUDP;
extern const PDMDRVREG g_DrvRawFile;
extern const PDMDRVREG g_DrvHostParallel;
extern const PDMDRVREG g_DrvHostSerial;
#ifdef VBOX_WITH_DRV_DISK_INTEGRITY
extern const PDMDRVREG g_DrvDiskIntegrity;
extern const PDMDRVREG g_DrvRamDisk;
#endif
#ifdef VBOX_WITH_PCI_PASSTHROUGH_IMPL
extern const PDMDRVREG g_DrvPciRaw;
#endif

#ifdef VBOX_WITH_USB
extern const PDMUSBREG g_UsbDevProxy;
extern const PDMUSBREG g_UsbMsd;
#endif
#ifdef VBOX_WITH_VUSB
extern const PDMUSBREG g_UsbHid;
extern const PDMUSBREG g_UsbHidKbd;
extern const PDMUSBREG g_UsbHidMou;
#endif
#ifdef VBOX_WITH_USB_VIDEO_IMPL
extern const PDMUSBREG g_DevWebcam;
#endif

#ifdef VBOX_WITH_SCSI
extern const PDMDRVREG g_DrvSCSI;
#endif


/* VBoxAcpi.cpp */
int acpiPrepareDsdt(PPDMDEVINS pDevIns, void **ppvPtr, size_t *pcbDsdt);
int acpiCleanupDsdt(PPDMDEVINS pDevIns, void *pvPtr);
int acpiPrepareSsdt(PPDMDEVINS pDevIns, void **ppvPtr, size_t *pcbSsdt);
int acpiCleanupSsdt(PPDMDEVINS pDevIns, void *pvPtr);

RT_C_DECLS_END

#endif

