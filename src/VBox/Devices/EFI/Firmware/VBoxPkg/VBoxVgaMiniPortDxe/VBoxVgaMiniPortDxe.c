/* $Id$ */
/** @file
 * VBoxVgaMiniPortDxe.c - VgaMiniPort Protocol Implementation.
 */


/*
 * Copyright (C) 2009-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/PciIo.h>
#include <Protocol/VgaMiniPort.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <IndustryStandard/Pci22.h>

#include "VBoxPkg.h"
#include "VBoxVgaFonts.h"
#include "iprt/asm.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Instance data for a VGA device this driver handles.
 */
typedef struct VBOXVGAMINIPORT
{
    /** The VGA Mini Port Protocol. */
    EFI_VGA_MINI_PORT_PROTOCOL  VgaMiniPort;
    /** Magic value, VBOX_VGA_MINI_PORT_MAGIC. */
    UINT32                      u32Magic;
    /** The controller handle of the device. */
    EFI_HANDLE                  hController;
    /** The PciIo protocol for the device. */
    EFI_PCI_IO_PROTOCOL        *pPciIo;
} VBOXVGAMINIPORT;
/** Pointer to a VBOXVGAMINIPORT structure. */
typedef VBOXVGAMINIPORT *PVBOXVGAMINIPORT;

/** VBOXVGAMINIPORT::u32Magic value (Isaac Asimov). */
#define VBOX_VGA_MINI_PORT_MAGIC        0x19200102
/** VBOXVGAMINIPORT::u32Magic dead value. */
#define VBOX_VGA_MINI_PORT_MAGIC_DEAD   0x19920406


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static EFI_STATUS EFIAPI
VBoxVgaMiniPortDB_Supported(IN EFI_DRIVER_BINDING_PROTOCOL *This, IN EFI_HANDLE ControllerHandle,
                            IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL);
static EFI_STATUS EFIAPI
VBoxVgaMiniPortDB_Start(IN EFI_DRIVER_BINDING_PROTOCOL *This, IN EFI_HANDLE ControllerHandle,
                        IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL);
static EFI_STATUS EFIAPI
VBoxVgaMiniPortDB_Stop(IN EFI_DRIVER_BINDING_PROTOCOL *This, IN EFI_HANDLE ControllerHandle,
                       IN UINTN NumberOfChildren, IN EFI_HANDLE *ChildHandleBuffer OPTIONAL);


static EFI_STATUS EFIAPI
VBoxVgaMiniPortVMP_SetMode(IN EFI_VGA_MINI_PORT_PROTOCOL *This, IN UINTN ModeNumber);


static EFI_STATUS EFIAPI
VBoxVgaMiniPortCN_GetDriverName(IN EFI_COMPONENT_NAME_PROTOCOL *This,
                                IN CHAR8 *Language, OUT CHAR16 **DriverName);
static EFI_STATUS EFIAPI
VBoxVgaMiniPortCN_GetControllerName(IN EFI_COMPONENT_NAME_PROTOCOL *This,
                                    IN EFI_HANDLE ControllerHandle,
                                    IN EFI_HANDLE ChildHandle OPTIONAL,
                                    IN CHAR8 *Language,  OUT CHAR16 **ControllerName);


static EFI_STATUS EFIAPI
VBoxVgaMiniPortCN2_GetDriverName(IN EFI_COMPONENT_NAME2_PROTOCOL *This,
                                 IN CHAR8 *Language, OUT CHAR16 **DriverName);
static EFI_STATUS EFIAPI
VBoxVgaMiniPortCN2_GetControllerName(IN EFI_COMPONENT_NAME2_PROTOCOL *This,
                                     IN EFI_HANDLE ControllerHandle,
                                     IN EFI_HANDLE ChildHandle OPTIONAL,
                                     IN CHAR8 *Language,  OUT CHAR16 **ControllerName);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** EFI Driver Binding Protocol. */
static EFI_DRIVER_BINDING_PROTOCOL          g_VBoxVgaMiniPortDB =
{
    VBoxVgaMiniPortDB_Supported,
    VBoxVgaMiniPortDB_Start,
    VBoxVgaMiniPortDB_Stop,
    /* .Version             = */    1,  /* One higher than Pci/VgaMiniPortDxe. */
    /* .ImageHandle         = */ NULL,
    /* .DriverBindingHandle = */ NULL
};

/** EFI Component Name Protocol. */
static const EFI_COMPONENT_NAME_PROTOCOL    g_VBoxVgaMiniPortCN =
{
    VBoxVgaMiniPortCN_GetDriverName,
    VBoxVgaMiniPortCN_GetControllerName,
    "eng"
};

/** EFI Component Name 2 Protocol. */
static const EFI_COMPONENT_NAME2_PROTOCOL   g_VBoxVgaMiniPortCN2 =
{
    VBoxVgaMiniPortCN2_GetDriverName,
    VBoxVgaMiniPortCN2_GetControllerName,
    "en"
};

/** Driver name translation table. */
static CONST EFI_UNICODE_STRING_TABLE       g_aVBoxMiniPortDriverLangAndNames[] =
{
    {   "eng;en",   L"PCI VGA Mini Port Driver" },
    {   NULL,       NULL }
};



/**
 * VBoxVgaMiniPort entry point.
 *
 * @returns EFI status code.
 *
 * @param   ImageHandle     The image handle.
 * @param   SystemTable     The system table pointer.
 */
EFI_STATUS EFIAPI
DxeInitializeVBoxVgaMiniPort(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS  rc;
    DEBUG((DEBUG_INFO, "DxeInitializeVBoxVgaMiniPort\n"));

    rc = EfiLibInstallDriverBindingComponentName2(ImageHandle, SystemTable,
                                                  &g_VBoxVgaMiniPortDB, ImageHandle,
                                                  &g_VBoxVgaMiniPortCN, &g_VBoxVgaMiniPortCN2);
    ASSERT_EFI_ERROR(rc);
    return rc;
}

/**
 * @copydoc EFI_DRIVER_BINDING_SUPPORTED
 */
static EFI_STATUS EFIAPI
VBoxVgaMiniPortDB_Supported(IN EFI_DRIVER_BINDING_PROTOCOL *This, IN EFI_HANDLE ControllerHandle,
                            IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL)
{
    EFI_STATUS              rcRet = EFI_UNSUPPORTED;
    EFI_PCI_IO_PROTOCOL    *pPciIo;
    EFI_STATUS              rc;

    DEBUG((DEBUG_INFO, "%a: Controller=%p\n", __FUNCTION__, ControllerHandle));

    /*
     * To perform the test we need to check some PCI configuration registers,
     * just read all the standard ones to make life simpler.
     */
    rc = gBS->OpenProtocol(ControllerHandle, &gEfiPciIoProtocolGuid, (VOID **)&pPciIo,
                           This->DriverBindingHandle, ControllerHandle, EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (!EFI_ERROR(rc))
    {
        PCI_TYPE00  CfgRegs;

        rc = pPciIo->Pci.Read(pPciIo,
                              EfiPciIoWidthUint32,
                              0 /* Offset */,
                              sizeof(CfgRegs) / sizeof(UINT32) /* Count */ ,
                              &CfgRegs);
        if (!EFI_ERROR(rc))
        {
            /*
             * Perform the test.
             */
            if (IS_PCI_VGA(&CfgRegs))
            {
#if 0 /** @todo this doesn't quite work with our DevVGA since it doesn't flag I/O access. */
                if (    CfgRegs.Hdr.Command & (PCI_COMMAND_IOACCESS | PCI_COMMAND_MEMACCESS)
                    == (PCI_COMMAND_IOACCESS | PCI_COMMAND_MEMACCESS))
#else
                if (1)
#endif
                {
                    DEBUG((DEBUG_INFO, "%a: Found supported VGA device! (VendorId=%x DeviceId=%x)\n",
                           __FUNCTION__, CfgRegs.Hdr.VendorId, CfgRegs.Hdr.DeviceId));
                    rcRet = EFI_SUCCESS;
                }
                else
                    DEBUG((DEBUG_INFO, "%a: VGA device not enabled! (VendorId=%x DeviceId=%x)\n",
                           __FUNCTION__, CfgRegs.Hdr.VendorId, CfgRegs.Hdr.DeviceId));
            }
            else
                DEBUG((DEBUG_INFO, "%a: Not VGA (Class=%x,%x,%x VendorId=%x DeviceId=%x)\n",
                       __FUNCTION__, CfgRegs.Hdr.ClassCode[0], CfgRegs.Hdr.ClassCode[1],
                       CfgRegs.Hdr.ClassCode[2], CfgRegs.Hdr.VendorId, CfgRegs.Hdr.DeviceId));
        }
        else
            DEBUG((DEBUG_INFO, "%a: pPciIo->Pci.Read -> %r\n", __FUNCTION__, rc));

        gBS->CloseProtocol(ControllerHandle, &gEfiPciIoProtocolGuid, This->DriverBindingHandle, ControllerHandle);
    }
    else
        DEBUG((DEBUG_INFO, "%a: PciIoProtocol -> %r\n", __FUNCTION__, rc));
    return rcRet;
}

/**
 * @copydoc EFI_DRIVER_BINDING_START
 */
static EFI_STATUS EFIAPI
VBoxVgaMiniPortDB_Start(IN EFI_DRIVER_BINDING_PROTOCOL *This, IN EFI_HANDLE ControllerHandle,
                        IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL)
{
    EFI_STATUS              rc;
    EFI_PCI_IO_PROTOCOL    *pPciIo;

    DEBUG((DEBUG_INFO, "%a\n", __FUNCTION__));

    /*
     * We need the PCI I/O abstraction protocol.
     */
    rc = gBS->OpenProtocol(ControllerHandle, &gEfiPciIoProtocolGuid, (VOID **)&pPciIo,
                           This->DriverBindingHandle, ControllerHandle, EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (!EFI_ERROR(rc))
    {
        /*
         * Allocate and initialize the instance data.
         */
        PVBOXVGAMINIPORT pThisDev;
        rc = gBS->AllocatePool(EfiBootServicesData, sizeof(*pThisDev), (VOID **)&pThisDev);
        if (!EFI_ERROR(rc))
        {
            pThisDev->VgaMiniPort.SetMode                   = VBoxVgaMiniPortVMP_SetMode;
            pThisDev->VgaMiniPort.VgaMemoryOffset           = 0x000b8000;
            pThisDev->VgaMiniPort.CrtcAddressRegisterOffset = 0x03d4;
            pThisDev->VgaMiniPort.CrtcDataRegisterOffset    = 0x03d5;
            pThisDev->VgaMiniPort.VgaMemoryBar              = EFI_PCI_IO_PASS_THROUGH_BAR;
            pThisDev->VgaMiniPort.CrtcAddressRegisterBar    = EFI_PCI_IO_PASS_THROUGH_BAR;
            pThisDev->VgaMiniPort.CrtcDataRegisterBar       = EFI_PCI_IO_PASS_THROUGH_BAR;
            pThisDev->VgaMiniPort.MaxMode                   = 2;
            pThisDev->u32Magic                              = VBOX_VGA_MINI_PORT_MAGIC;
            pThisDev->hController                           = ControllerHandle;
            pThisDev->pPciIo                                = pPciIo;

            /*
             * Register the VGA Mini Port Protocol.
             */
            rc = gBS->InstallMultipleProtocolInterfaces(&ControllerHandle,
                                                        &gEfiVgaMiniPortProtocolGuid, &pThisDev->VgaMiniPort,
                                                        NULL, NULL);
            if (!EFI_ERROR(rc))
            {
                DEBUG((DEBUG_INFO, "%a: Successfully started, pThisDev=%p ControllerHandle=%p\n",
                       __FUNCTION__, pThisDev, ControllerHandle));
                return EFI_SUCCESS;
            }

            DEBUG((DEBUG_INFO, "%a: InstallMultipleProtocolInterfaces -> %r\n", __FUNCTION__, rc));
            gBS->FreePool(pThisDev);
        }
        else
            DEBUG((DEBUG_INFO, "%a: AllocatePool -> %r\n", __FUNCTION__, rc));

        gBS->CloseProtocol(ControllerHandle, &gEfiPciIoProtocolGuid, This->DriverBindingHandle, ControllerHandle);
    }
    else
        DEBUG((DEBUG_INFO, "%a: PciIoProtocol -> %r\n", __FUNCTION__, rc));
    return rc;
}

/**
 * @copydoc EFI_DRIVER_BINDING_STOP
 */
static EFI_STATUS EFIAPI
VBoxVgaMiniPortDB_Stop(IN EFI_DRIVER_BINDING_PROTOCOL *This, IN EFI_HANDLE ControllerHandle,
                       IN UINTN NumberOfChildren, IN EFI_HANDLE *ChildHandleBuffer OPTIONAL)
{
    EFI_STATUS                  rc;
    PVBOXVGAMINIPORT            pThisDev;

    DEBUG((DEBUG_INFO, "%a: ControllerHandle=%p NumberOfChildren=%u\n", __FUNCTION__, ControllerHandle, NumberOfChildren));

    /*
     * Get the miniport driver instance associated with the controller.
     */
    rc = gBS->OpenProtocol(ControllerHandle, &gEfiVgaMiniPortProtocolGuid,
                           (VOID **)&pThisDev, This->DriverBindingHandle,
                           ControllerHandle, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (!EFI_ERROR(rc))
    {
        ASSERT(pThisDev->u32Magic == VBOX_VGA_MINI_PORT_MAGIC);
        ASSERT(pThisDev->hController == ControllerHandle);
        if (    pThisDev->u32Magic == VBOX_VGA_MINI_PORT_MAGIC
            &&  pThisDev->hController == ControllerHandle)
        {
            /*
             * Uninstall the VgaMiniPort interface.
             */
            rc = gBS->UninstallProtocolInterface(ControllerHandle,
                                                 &gEfiVgaMiniPortProtocolGuid,
                                                 &pThisDev->VgaMiniPort);
            if (!EFI_ERROR(rc))
            {
                /*
                 * Invalidate and release sources associated with the device instance.
                 */
                pThisDev->u32Magic = VBOX_VGA_MINI_PORT_MAGIC_DEAD;
                gBS->FreePool(pThisDev);

            }
            else
                DEBUG((DEBUG_INFO, "%a: UninstallProtocolInterface -> %r\n", __FUNCTION__, rc));
        }
        else
        {
            DEBUG((DEBUG_INFO, "%a: magic=%x/%x hController=%x/%x\n", __FUNCTION__,
                   pThisDev->u32Magic, VBOX_VGA_MINI_PORT_MAGIC,
                   pThisDev->hController, ControllerHandle));
            rc = EFI_DEVICE_ERROR;
        }
        gBS->CloseProtocol(ControllerHandle, &gEfiPciIoProtocolGuid,
                           This->DriverBindingHandle, ControllerHandle);
    }
    else
        DEBUG((DEBUG_INFO, "%a: VgaMiniPortProtocol -> %r\n", __FUNCTION__, rc));
    return rc;
}





/**
 * @copydoc EFI_VGA_MINI_PORT_SET_MODE
 */
static EFI_STATUS EFIAPI
VBoxVgaMiniPortVMP_SetMode(IN EFI_VGA_MINI_PORT_PROTOCOL *This, IN UINTN ModeNumber)
{
    PVBOXVGAMINIPORT    pThisDev = (PVBOXVGAMINIPORT)This;
    UINT8               r[64];
    int                 i;

    /*
     * Check input.
     */
    if (pThisDev->u32Magic != VBOX_VGA_MINI_PORT_MAGIC)
    {
        DEBUG((DEBUG_INFO, "%a: u32Magic=%x/%x\n", __FUNCTION__, pThisDev->u32Magic, VBOX_VGA_MINI_PORT_MAGIC));
        return EFI_DEVICE_ERROR;
    }
    if (ModeNumber >= This->MaxMode)
    {
        DEBUG((DEBUG_INFO, "%a: ModeNumber=%d >= MaxMode=%d\n", __FUNCTION__, ModeNumber, This->MaxMode));
        return EFI_UNSUPPORTED;
    }
    DEBUG((DEBUG_INFO, "%a: ModeNumber=%d\n", __FUNCTION__, ModeNumber));

    /* some initialization */
    ASMOutU8(0x3c2, 0xc3);
    ASMOutU16(0x3c4, 0x0204);

    ASMInU8(0x3da);             // reset attr F/F
    ASMOutU8(0x3c0, 0);         // disable palette
    ASMOutU16(0x3d4, 0x0011);   // unprotect crtc regs 0 - 7

#define BOUTB(count, aport, dport)                                  \
    do {                                                            \
        for (i = 0 ; i < (count); ++i)                              \
            if ((dport) == (aport) + 1)                             \
                ASMOutU16((aport), ((UINT16)r[i] << 8) | (UINT8)i); \
            else {                                                  \
                ASMOutU8((aport), (UINT8)i);                        \
                ASMOutU8((dport), r[i]);                            \
            }                                                       \
    } while (0)

    /* Reset and set sequencer registers */
    r[0] = 0x01;
    r[1] = 0x00;
    r[2] = 0x03;
    r[3] = 0x00;
    r[4] = 0x02;
    BOUTB(5, 0x3c4, 0x3c5);

    /* set misc out register */
    ASMOutU8(0x3c2, 0x67);

    /* enable sequencer */
    ASMOutU16(0x3c4, 0x0300);

    /*  set all crtc registers */
    r[0] = 0x5f;  r[1] = 0x4f;  r[2] = 0x50;  r[3] = 0x82;
    r[4] = 0x55;  r[5] = 0x81;  r[6] = 0xbf;  r[7] = 0x1f;
    r[8] = 0x00;  r[9] = 0x4f;  r[10]= 0x0d;  r[11]= 0x0e;
    r[12]= 0x00;  r[13]= 0x00;  r[14]= 0x03;  r[15]= 0xc0;
    r[16]= 0x9c;  r[17]= 0x0e;  r[18]= 0x8f;  r[19]= 0x28;
    r[20]= 0x1f;  r[21]= 0x96;  r[22]= 0xb9;  r[23]= 0xa3;
    r[24]= 0xff;
    BOUTB(25, 0x3d4, 0x3d5);

    /*  set all graphics controller registers */
    r[0]= 0x00;  r[1]= 0x00;  r[2]= 0x00;  r[3]= 0x00;
    r[4]= 0x00;  r[5]= 0x10;  r[6]= 0x0e;  r[7]= 0x00;
    r[8]= 0xff;
    BOUTB(9, 0x3ce, 0x3cf);

    /*  set all attribute registers */
    ASMInU8(0x3da);             // reset attr F/F
    r[0] = 0x00;  r[1] = 0x01;  r[2] = 0x02;  r[3] = 0x03;
    r[4] = 0x04;  r[5] = 0x05;  r[6] = 0x14;  r[7] = 0x07;
    r[8] = 0x38;  r[9] = 0x39;  r[10]= 0x3a;  r[11]= 0x3b;
    r[12]= 0x3c;  r[13]= 0x3d;  r[14]= 0x3e;  r[15]= 0x3f;
    r[16]= 0x0c;  r[17]= 0x00;  r[18]= 0x0f;  r[19]= 0x08;
    r[20]= 0x00;
    BOUTB(21, 0x3c0, 0x3c0);
    ASMOutU8(0x3c0, 0x20);              // re-enable palette

    /* set all VBox extended registers */

    /* enable sequencer */
    ASMOutU16(0x3c4, 0x0100);

    ASMOutU16(0x1ce, 0x04); ASMOutU16(0x1cf, 0);    // ENABLE

    /* enable sequencer */
    ASMOutU16(0x3c4, 0x0300);

    /* Load default values into the first 16 entries of the DAC */
    {
        static const UINT8 s_a3bVgaDac[64*3] =
        {
            0x00, 0x00, 0x00,
            0x00, 0x00, 0x2A,
            0x00, 0x2A, 0x00,
            0x00, 0x2A, 0x2A,
            0x2A, 0x00, 0x00,
            0x2A, 0x00, 0x2A,
            0x2A, 0x2A, 0x00,
            0x2A, 0x2A, 0x2A,
            0x00, 0x00, 0x15,
            0x00, 0x00, 0x3F,
            0x00, 0x2A, 0x15,
            0x00, 0x2A, 0x3F,
            0x2A, 0x00, 0x15,
            0x2A, 0x00, 0x3F,
            0x2A, 0x2A, 0x15,
            0x2A, 0x2A, 0x3F,
            0x00, 0x15, 0x00,
            0x00, 0x15, 0x2A,
            0x00, 0x3F, 0x00,
            0x00, 0x3F, 0x2A,
            0x2A, 0x15, 0x00,
            0x2A, 0x15, 0x2A,
            0x2A, 0x3F, 0x00,
            0x2A, 0x3F, 0x2A,
            0x00, 0x15, 0x15,
            0x00, 0x15, 0x3F,
            0x00, 0x3F, 0x15,
            0x00, 0x3F, 0x3F,
            0x2A, 0x15, 0x15,
            0x2A, 0x15, 0x3F,
            0x2A, 0x3F, 0x15,
            0x2A, 0x3F, 0x3F,
            0x15, 0x00, 0x00,
            0x15, 0x00, 0x2A,
            0x15, 0x2A, 0x00,
            0x15, 0x2A, 0x2A,
            0x3F, 0x00, 0x00,
            0x3F, 0x00, 0x2A,
            0x3F, 0x2A, 0x00,
            0x3F, 0x2A, 0x2A,
            0x15, 0x00, 0x15,
            0x15, 0x00, 0x3F,
            0x15, 0x2A, 0x15,
            0x15, 0x2A, 0x3F,
            0x3F, 0x00, 0x15,
            0x3F, 0x00, 0x3F,
            0x3F, 0x2A, 0x15,
            0x3F, 0x2A, 0x3F,
            0x15, 0x15, 0x00,
            0x15, 0x15, 0x2A,
            0x15, 0x3F, 0x00,
            0x15, 0x3F, 0x2A,
            0x3F, 0x15, 0x00,
            0x3F, 0x15, 0x2A,
            0x3F, 0x3F, 0x00,
            0x3F, 0x3F, 0x2A,
            0x15, 0x15, 0x15,
            0x15, 0x15, 0x3F,
            0x15, 0x3F, 0x15,
            0x15, 0x3F, 0x3F,
            0x3F, 0x15, 0x15,
            0x3F, 0x15, 0x3F,
            0x3F, 0x3F, 0x15,
            0x3F, 0x3F, 0x3F
        };

        for (i = 0; i < 64; ++i)
        {
            ASMOutU8(0x3c8, (UINT8)i);
            ASMOutU8(0x3c9, s_a3bVgaDac[i*3 + 0]);
            ASMOutU8(0x3c9, s_a3bVgaDac[i*3 + 1]);
            ASMOutU8(0x3c9, s_a3bVgaDac[i*3 + 2]);
        }
    }

    /* Load the appropriate font into the first map */
    {
        UINT8 const    *pabFont;
        unsigned        offBase = 0;
        UINT16          height;

        switch (ModeNumber) {
        case 0: // 80x25 mode, uses 8x16 font
            pabFont = g_abVgaFont_8x16;
            height  = 16;
            break;
        case 1: // 80x50 mode, uses 8x8 font
            pabFont = g_abVgaFont_8x8;
            height  = 8;
            break;
        default:
            ASSERT(0);  // Valid mode numbers checked above
            return EFI_UNSUPPORTED;
        }
        // Enable font map access
        {
            /* Write sequencer registers */
            ASMOutU16(0x3c4, 0x0100);
            ASMOutU16(0x3c4, 0x0402);
            ASMOutU16(0x3c4, 0x0704);
            ASMOutU16(0x3c4, 0x0300);
            /* Write graphics controller registers */
            ASMOutU16(0x3ce, 0x0204);
            ASMOutU16(0x3ce, 0x0005);
            ASMOutU16(0x3ce, 0x0406);
        }

        for (i = 0; i < 256; ++i)
        {
            int offChr = i * height;
            int offDst = offBase + i * 32;
            CopyMem((UINT8 *)(UINTN)0xA0000 + offDst, pabFont + offChr, height);
        }

        // Set the CRTC Maximum Scan Line register
        ASMOutU16(0x3d4, 0x4009 | ((height - 1) << 8));

        // Disable font map access again
        {
            /* Write sequencer registers */
            ASMOutU16(0x3c4, 0x0100);
            ASMOutU16(0x3c4, 0x0302);
            ASMOutU16(0x3c4, 0x0304);
            ASMOutU16(0x3c4, 0x0300);
            /* Write graphics controller registers */
            ASMOutU16(0x3ce, 0x0004);
            ASMOutU16(0x3ce, 0x1005);
            ASMOutU16(0x3ce, 0x0e06);
        }
    }

    return EFI_SUCCESS;
}




/** @copydoc EFI_COMPONENT_NAME_GET_DRIVER_NAME */
static EFI_STATUS EFIAPI
VBoxVgaMiniPortCN_GetDriverName(IN EFI_COMPONENT_NAME_PROTOCOL *This,
                                IN CHAR8 *Language, OUT CHAR16 **DriverName)
{
    return LookupUnicodeString2(Language,
                                This->SupportedLanguages,
                                &g_aVBoxMiniPortDriverLangAndNames[0],
                                DriverName,
                                TRUE);
}

/** @copydoc EFI_COMPONENT_NAME_GET_CONTROLLER_NAME */
static EFI_STATUS EFIAPI
VBoxVgaMiniPortCN_GetControllerName(IN EFI_COMPONENT_NAME_PROTOCOL *This,
                                    IN EFI_HANDLE ControllerHandle,
                                    IN EFI_HANDLE ChildHandle OPTIONAL,
                                    IN CHAR8 *Language, OUT CHAR16 **ControllerName)
{
    /** @todo try query the protocol from the controller and forward the query. */
    return EFI_UNSUPPORTED;
}




/** @copydoc EFI_COMPONENT_NAME2_GET_DRIVER_NAME */
static EFI_STATUS EFIAPI
VBoxVgaMiniPortCN2_GetDriverName(IN EFI_COMPONENT_NAME2_PROTOCOL *This,
                                 IN CHAR8 *Language, OUT CHAR16 **DriverName)
{
    return LookupUnicodeString2(Language,
                                This->SupportedLanguages,
                                &g_aVBoxMiniPortDriverLangAndNames[0],
                                DriverName,
                                FALSE);
}

/** @copydoc EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME */
static EFI_STATUS EFIAPI
VBoxVgaMiniPortCN2_GetControllerName(IN EFI_COMPONENT_NAME2_PROTOCOL *This,
                                     IN EFI_HANDLE ControllerHandle,
                                     IN EFI_HANDLE ChildHandle OPTIONAL,
                                     IN CHAR8 *Language, OUT CHAR16 **ControllerName)
{
    /** @todo try query the protocol from the controller and forward the query. */
    return EFI_UNSUPPORTED;
}

