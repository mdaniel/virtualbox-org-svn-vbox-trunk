/** @file
  NvmExpressDxe driver is used to manage non-volatile memory subsystem which follows
  NVM Express specification.

  Copyright (c) 2013, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "NvmExpress.h"

//
// NVM Express Driver Binding Protocol Instance
//
EFI_DRIVER_BINDING_PROTOCOL gNvmExpressDriverBinding = {
  NvmExpressDriverBindingSupported,
  NvmExpressDriverBindingStart,
  NvmExpressDriverBindingStop,
  0x10,
  NULL,
  NULL
};

//
// NVM Express EFI Driver Supported EFI Version Protocol Instance
//
EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL gNvmExpressDriverSupportedEfiVersion = {
  sizeof (EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL), // Size of Protocol structure.
  0                                                   // Version number to be filled at start up.
};

/**
  Check if the specified Nvm Express device namespace is active, and create child handles
  for them with BlockIo and DiskInfo protocol instances.

  @param[in] Private         The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param[in] NamespaceId     The NVM Express namespace ID  for which a device path node is to be
                             allocated and built. Caller must set the NamespaceId to zero if the
                             device path node will contain a valid UUID.
  @param[in] NamespaceUuid   The NVM Express namespace UUID for which a device path node is to be
                             allocated and built. UUID will only be valid of the Namespace ID is zero.

  @retval EFI_SUCCESS        All the namespaces in the device are successfully enumerated.
  @return Others             Some error occurs when enumerating the namespaces.

**/
EFI_STATUS
EnumerateNvmeDevNamespace (
  IN NVME_CONTROLLER_PRIVATE_DATA       *Private,
  UINT32                                NamespaceId,
  UINT64                                NamespaceUuid
  )
{
  NVME_ADMIN_NAMESPACE_DATA             *NamespaceData;
  EFI_DEVICE_PATH_PROTOCOL              *NewDevicePathNode;
  EFI_DEVICE_PATH_PROTOCOL              *DevicePath;
  EFI_HANDLE                            DeviceHandle;
  EFI_DEVICE_PATH_PROTOCOL              *ParentDevicePath;
  EFI_DEVICE_PATH_PROTOCOL              *RemainingDevicePath;
  NVME_DEVICE_PRIVATE_DATA              *Device;
  EFI_STATUS                            Status;
  UINT32                                Lbads;
  UINT32                                Flbas;
  UINT32                                LbaFmtIdx;

  NewDevicePathNode = NULL;
  DevicePath        = NULL;
  Device            = NULL;

  //
  // Allocate a buffer for Identify Namespace data
  //
  NamespaceData = AllocateZeroPool(sizeof (NVME_ADMIN_NAMESPACE_DATA));
  if(NamespaceData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ParentDevicePath = Private->ParentDevicePath;
  //
  // Identify Namespace
  //
  Status = NvmeIdentifyNamespace (
             Private,
             NamespaceId,
             (VOID *)NamespaceData
             );
  if (EFI_ERROR(Status)) {
    goto Exit;
  }
  //
  // Validate Namespace
  //
  if (NamespaceData->Ncap == 0) {
    Status = EFI_DEVICE_ERROR;
  } else {
    //
    // allocate device private data for each discovered namespace
    //
    Device = AllocateZeroPool(sizeof(NVME_DEVICE_PRIVATE_DATA));
    if (Device == NULL) {
      goto Exit;
    }

    //
    // Initialize SSD namespace instance data
    //
    Device->Signature           = NVME_DEVICE_PRIVATE_DATA_SIGNATURE;
    Device->NamespaceId         = NamespaceId;
    Device->NamespaceUuid       = NamespaceData->Eui64;

    Device->ControllerHandle    = Private->ControllerHandle;
    Device->DriverBindingHandle = Private->DriverBindingHandle;
    Device->Controller          = Private;

    //
    // Build BlockIo media structure
    //
    Device->Media.MediaId        = 0;
    Device->Media.RemovableMedia = FALSE;
    Device->Media.MediaPresent   = TRUE;
    Device->Media.LogicalPartition = FALSE;
    Device->Media.ReadOnly       = FALSE;
    Device->Media.WriteCaching   = FALSE;

    Flbas     = NamespaceData->Flbas;
    LbaFmtIdx = Flbas & 0xF;
    Lbads     = NamespaceData->LbaFormat[LbaFmtIdx].Lbads;
    Device->Media.BlockSize = (UINT32)1 << Lbads;

    Device->Media.LastBlock                     = NamespaceData->Nsze - 1;
    Device->Media.LogicalBlocksPerPhysicalBlock = 1;
    Device->Media.LowestAlignedLba              = 1;

    //
    // Create BlockIo Protocol instance
    //
    Device->BlockIo.Revision     = EFI_BLOCK_IO_PROTOCOL_REVISION2;
    Device->BlockIo.Media        = &Device->Media;
    Device->BlockIo.Reset        = NvmeBlockIoReset;
    Device->BlockIo.ReadBlocks   = NvmeBlockIoReadBlocks;
    Device->BlockIo.WriteBlocks  = NvmeBlockIoWriteBlocks;
    Device->BlockIo.FlushBlocks  = NvmeBlockIoFlushBlocks;

    //
    // Create DiskInfo Protocol instance
    //
    InitializeDiskInfo (Device);

    //
    // Create a Nvm Express Namespace Device Path Node
    //
    Status = Private->Passthru.BuildDevicePath (
                                 &Private->Passthru,
                                 Device->NamespaceId,
                                 Device->NamespaceUuid,
                                 &NewDevicePathNode
                                 );

    if (EFI_ERROR(Status)) {
      goto Exit;
    }

    //
    // Append the SSD node to the controller's device path
    //
    DevicePath = AppendDevicePathNode (ParentDevicePath, NewDevicePathNode);
    if (DevicePath == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    DeviceHandle = NULL;
    RemainingDevicePath = DevicePath;
    Status = gBS->LocateDevicePath (&gEfiDevicePathProtocolGuid, &RemainingDevicePath, &DeviceHandle);
    if (!EFI_ERROR (Status) && (DeviceHandle != NULL) && IsDevicePathEnd(RemainingDevicePath)) {
      Status = EFI_ALREADY_STARTED;
      FreePool (DevicePath);
      goto Exit;
    }

    Device->DevicePath = DevicePath;

    //
    // Make sure the handle is NULL so we create a new handle
    //
    Device->DeviceHandle = NULL;

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Device->DeviceHandle,
                    &gEfiDevicePathProtocolGuid,
                    Device->DevicePath,
                    &gEfiBlockIoProtocolGuid,
                    &Device->BlockIo,
                    &gEfiDiskInfoProtocolGuid,
                    &Device->DiskInfo,
                    NULL
                    );

    if(EFI_ERROR(Status)) {
      goto Exit;
    }
    gBS->OpenProtocol (
           Private->ControllerHandle,
           &gEfiPciIoProtocolGuid,
           (VOID **) &Private->PciIo,
           Private->DriverBindingHandle,
           Device->DeviceHandle,
           EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
           );

    //
    // Dump NvmExpress Identify Namespace Data
    //
    DEBUG ((EFI_D_INFO, " == NVME IDENTIFY NAMESPACE [%d] DATA ==\n", NamespaceId));
    DEBUG ((EFI_D_INFO, "    NSZE        : 0x%x\n", NamespaceData->Nsze));
    DEBUG ((EFI_D_INFO, "    NCAP        : 0x%x\n", NamespaceData->Ncap));
    DEBUG ((EFI_D_INFO, "    NUSE        : 0x%x\n", NamespaceData->Nuse));
    DEBUG ((EFI_D_INFO, "    LBAF0.LBADS : 0x%x\n", (NamespaceData->LbaFormat[0].Lbads)));

    //
    // Build controller name for Component Name (2) protocol.
    //
    UnicodeSPrintAsciiFormat (Device->ModelName, sizeof (Device->ModelName), "%a-%a-%x", Private->ControllerData->Sn, Private->ControllerData->Mn, NamespaceData->Eui64);

    AddUnicodeString2 (
      "eng",
      gNvmExpressComponentName.SupportedLanguages,
      &Device->ControllerNameTable,
      Device->ModelName,
      TRUE
      );

    AddUnicodeString2 (
      "en",
      gNvmExpressComponentName2.SupportedLanguages,
      &Device->ControllerNameTable,
      Device->ModelName,
      FALSE
      );
  }

Exit:
  if(NamespaceData != NULL) {
    FreePool (NamespaceData);
  }

  if(EFI_ERROR(Status) && (Device != NULL) && (Device->DevicePath != NULL)) {
    FreePool (Device->DevicePath);
  }
  if(EFI_ERROR(Status) && (Device != NULL)) {
    FreePool (Device);
  }
  return Status;
}

/**
  Discover all Nvm Express device namespaces, and create child handles for them with BlockIo
  and DiskInfo protocol instances.

  @param[in] Private         The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.

  @retval EFI_SUCCESS        All the namespaces in the device are successfully enumerated.
  @return Others             Some error occurs when enumerating the namespaces.

**/
EFI_STATUS
DiscoverAllNamespaces (
  IN NVME_CONTROLLER_PRIVATE_DATA       *Private
  )
{
  EFI_STATUS                            Status;
  UINT32                                NamespaceId;
  UINT64                                NamespaceUuid;
  NVM_EXPRESS_PASS_THRU_PROTOCOL        *Passthru;

  NamespaceId   = 0xFFFFFFFF;
  NamespaceUuid = 0;
  Passthru      = &Private->Passthru;

  while (TRUE) {
    Status = Passthru->GetNextNamespace (
                         Passthru,
                         (UINT32 *)&NamespaceId,
                         (UINT64 *)&NamespaceUuid
                         );

    if (EFI_ERROR (Status)) {
      break;
    }

    Status = EnumerateNvmeDevNamespace (
               Private,
               NamespaceId,
               NamespaceUuid
               );

    if (EFI_ERROR(Status)) {
      continue;
    }
  }

  return EFI_SUCCESS;
}

/**
  Unregisters a Nvm Express device namespace.

  This function removes the protocols installed on the controller handle and
  frees the resources allocated for the namespace.

  @param  This                  The pointer to EFI_DRIVER_BINDING_PROTOCOL instance.
  @param  Controller            The controller handle of the namespace.
  @param  Handle                The child handle.

  @retval EFI_SUCCESS           The namespace is successfully unregistered.
  @return Others                Some error occurs when unregistering the namespace.

**/
EFI_STATUS
UnregisterNvmeNamespace (
  IN  EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN  EFI_HANDLE                     Controller,
  IN  EFI_HANDLE                     Handle
  )
{
  EFI_STATUS                               Status;
  EFI_PCI_IO_PROTOCOL                      *PciIo;
  EFI_BLOCK_IO_PROTOCOL                    *BlockIo;
  NVME_DEVICE_PRIVATE_DATA                 *Device;

  BlockIo = NULL;

  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **) &BlockIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Device = NVME_DEVICE_PRIVATE_DATA_FROM_BLOCK_IO (BlockIo);

  //
  // Close the child handle
  //
  gBS->CloseProtocol (
         Controller,
         &gEfiPciIoProtocolGuid,
         This->DriverBindingHandle,
         Handle
         );

  //
  // The Nvm Express driver installs the BlockIo and DiskInfo in the DriverBindingStart().
  // Here should uninstall both of them.
  //
  Status = gBS->UninstallMultipleProtocolInterfaces (
                  Handle,
                  &gEfiDevicePathProtocolGuid,
                  Device->DevicePath,
                  &gEfiBlockIoProtocolGuid,
                  &Device->BlockIo,
                  &gEfiDiskInfoProtocolGuid,
                  &Device->DiskInfo,
                  NULL
                  );

  if (EFI_ERROR (Status)) {
    gBS->OpenProtocol (
           Controller,
           &gEfiPciIoProtocolGuid,
           (VOID **) &PciIo,
           This->DriverBindingHandle,
           Handle,
           EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
           );
    return Status;
  }

  if(Device->DevicePath != NULL) {
    FreePool (Device->DevicePath);
  }

  if (Device->ControllerNameTable != NULL) {
    FreeUnicodeStringTable (Device->ControllerNameTable);
  }

  return EFI_SUCCESS;
}

/**
  Tests to see if this driver supports a given controller. If a child device is provided,
  it further tests to see if this driver supports creating a handle for the specified child device.

  This function checks to see if the driver specified by This supports the device specified by
  ControllerHandle. Drivers will typically use the device path attached to
  ControllerHandle and/or the services from the bus I/O abstraction attached to
  ControllerHandle to determine if the driver supports ControllerHandle. This function
  may be called many times during platform initialization. In order to reduce boot times, the tests
  performed by this function must be very small, and take as little time as possible to execute. This
  function must not change the state of any hardware devices, and this function must be aware that the
  device specified by ControllerHandle may already be managed by the same driver or a
  different driver. This function must match its calls to AllocatePages() with FreePages(),
  AllocatePool() with FreePool(), and OpenProtocol() with CloseProtocol().
  Since ControllerHandle may have been previously started by the same driver, if a protocol is
  already in the opened state, then it must not be closed with CloseProtocol(). This is required
  to guarantee the state of ControllerHandle is not modified by this function.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For bus drivers, if this parameter is not NULL, then
                                   the bus driver must determine if the bus controller specified
                                   by ControllerHandle and the child controller specified
                                   by RemainingDevicePath are both supported by this
                                   bus driver.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by the driver
                                   specified by This.
  @retval EFI_ACCESS_DENIED        The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by a different
                                   driver or an application that requires exclusive access.
                                   Currently not implemented.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the driver specified by This.
**/
EFI_STATUS
EFIAPI
NvmExpressDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                Status;
  EFI_DEV_PATH_PTR          DevicePathNode;
  EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath;
  EFI_PCI_IO_PROTOCOL       *PciIo;
  UINT8                     ClassCode[3];

  //
  // Check whether device path is valid
  //
  if (RemainingDevicePath != NULL) {
    //
    // Check if RemainingDevicePath is the End of Device Path Node,
    // if yes, go on checking other conditions
    //
    if (!IsDevicePathEnd (RemainingDevicePath)) {
      //
      // If RemainingDevicePath isn't the End of Device Path Node,
      // check its validation
      //
      DevicePathNode.DevPath = RemainingDevicePath;

      if ((DevicePathNode.DevPath->Type    != MESSAGING_DEVICE_PATH) ||
          (DevicePathNode.DevPath->SubType != MSG_NVME_NAMESPACE_DP) ||
           DevicePathNodeLength(DevicePathNode.DevPath) != sizeof(NVME_NAMESPACE_DEVICE_PATH)) {
        return EFI_UNSUPPORTED;
      }
    }
  }

  //
  // Open the EFI Device Path protocol needed to perform the supported test
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **) &ParentDevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (Status == EFI_ALREADY_STARTED) {
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Close protocol, don't use device path protocol in the Support() function
  //
  gBS->CloseProtocol (
         Controller,
         &gEfiDevicePathProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  //
  // Attempt to Open PCI I/O Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **) &PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (Status == EFI_ALREADY_STARTED) {
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Now further check the PCI header: Base class (offset 0x0B) and Sub Class (offset 0x0A).
  // This controller should be a Nvm Express controller.
  //
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        PCI_CLASSCODE_OFFSET,
                        sizeof (ClassCode),
                        ClassCode
                        );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  //
  // Examine Nvm Express controller PCI Configuration table fields
  //
  if ((ClassCode[0] != PCI_IF_NVMHCI) || (ClassCode[1] != PCI_CLASS_MASS_STORAGE_NVM) || (ClassCode[2] != PCI_CLASS_MASS_STORAGE)) {
    Status = EFI_UNSUPPORTED;
  }

Done:
  gBS->CloseProtocol (
         Controller,
         &gEfiPciIoProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  return Status;
}


/**
  Starts a device controller or a bus controller.

  The Start() function is designed to be invoked from the EFI boot service ConnectController().
  As a result, much of the error checking on the parameters to Start() has been moved into this
  common boot service. It is legal to call Start() from other locations,
  but the following calling restrictions must be followed or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE.
  2. If RemainingDevicePath is not NULL, then it must be a pointer to a naturally aligned
     EFI_DEVICE_PATH_PROTOCOL.
  3. Prior to calling Start(), the Supported() function for the driver specified by This must
     have been called with the same calling parameters, and Supported() must have returned EFI_SUCCESS.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to start. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For a bus driver, if this parameter is NULL, then handles
                                   for all the children of Controller are created by this driver.
                                   If this parameter is not NULL and the first Device Path Node is
                                   not the End of Device Path Node, then only the handle for the
                                   child device specified by the first Device Path Node of
                                   RemainingDevicePath is created by this driver.
                                   If the first Device Path Node of RemainingDevicePath is
                                   the End of Device Path Node, no child handle is created by this
                                   driver.

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a device error.Currently not implemented.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.
  @retval Others                   The driver failded to start the device.

**/
EFI_STATUS
EFIAPI
NvmExpressDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                        Status;
  EFI_PCI_IO_PROTOCOL               *PciIo;
  NVME_CONTROLLER_PRIVATE_DATA      *Private;
  EFI_DEVICE_PATH_PROTOCOL          *ParentDevicePath;
  UINT32                            NamespaceId;
  UINT64                            NamespaceUuid;
  EFI_PHYSICAL_ADDRESS              MappedAddr;
  UINTN                             Bytes;

  DEBUG ((EFI_D_INFO, "NvmExpressDriverBindingStart: start\n"));

  Private = NULL;
  ParentDevicePath = NULL;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **) &ParentDevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if ((EFI_ERROR (Status)) && (Status != EFI_ALREADY_STARTED)) {
    return Status;
  }

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **) &PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );

  if (EFI_ERROR (Status) && (Status != EFI_ALREADY_STARTED)) {
    return Status;
  }

  //
  // Check EFI_ALREADY_STARTED to reuse the original NVME_CONTROLLER_PRIVATE_DATA.
  //
  if (Status != EFI_ALREADY_STARTED) {
    Private = AllocateZeroPool (sizeof (NVME_CONTROLLER_PRIVATE_DATA));

    if (Private == NULL) {
      DEBUG ((EFI_D_ERROR, "NvmExpressDriverBindingStart: allocating pool for Nvme Private Data failed!\n"));
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit2;
    }

    //
    // 4 x 4kB aligned buffers will be carved out of this buffer.
    // 1st 4kB boundary is the start of the admin submission queue.
    // 2nd 4kB boundary is the start of the admin completion queue.
    // 3rd 4kB boundary is the start of I/O submission queue #1.
    // 4th 4kB boundary is the start of I/O completion queue #1.
    //
    // Allocate 4 pages of memory, then map it for bus master read and write.
    //
    Status = PciIo->AllocateBuffer (
                      PciIo,
                      AllocateAnyPages,
                      EfiBootServicesData,
                      4,
                      (VOID**)&Private->Buffer,
                      0
                      );
    if (EFI_ERROR (Status)) {
      goto Exit2;
    }

    Bytes = EFI_PAGES_TO_SIZE (4);
    Status = PciIo->Map (
                      PciIo,
                      EfiPciIoOperationBusMasterCommonBuffer,
                      Private->Buffer,
                      &Bytes,
                      &MappedAddr,
                      &Private->Mapping
                      );

    if (EFI_ERROR (Status) || (Bytes != EFI_PAGES_TO_SIZE (4))) {
      goto Exit2;
    }

    Private->BufferPciAddr = (UINT8 *)(UINTN)MappedAddr;
    ZeroMem (Private->Buffer, EFI_PAGES_TO_SIZE (4));

    Private->Signature = NVME_CONTROLLER_PRIVATE_DATA_SIGNATURE;
    Private->ControllerHandle          = Controller;
    Private->ImageHandle               = This->DriverBindingHandle;
    Private->DriverBindingHandle       = This->DriverBindingHandle;
    Private->PciIo                     = PciIo;
    Private->ParentDevicePath          = ParentDevicePath;
    Private->Passthru.Mode             = &Private->PassThruMode;
    Private->Passthru.PassThru         = NvmExpressPassThru;
    Private->Passthru.GetNextNamespace = NvmExpressGetNextNamespace;
    Private->Passthru.BuildDevicePath  = NvmExpressBuildDevicePath;
    Private->Passthru.GetNamespace     = NvmExpressGetNamespace;
    Private->PassThruMode.Attributes   = NVM_EXPRESS_PASS_THRU_ATTRIBUTES_PHYSICAL;

    Status = NvmeControllerInit (Private);

    if (EFI_ERROR(Status)) {
      goto Exit2;
    }

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Controller,
                    &gEfiCallerIdGuid,
                    Private,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      goto Exit2;
    }
  } else {
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiCallerIdGuid,
                    (VOID **) &Private,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      Private = NULL;
      goto Exit1;
    }
  }

  if (RemainingDevicePath == NULL) {
    //
    // Enumerate all NVME namespaces in the controller
    //
    Status = DiscoverAllNamespaces (
               Private
               );

  } else if (!IsDevicePathEnd (RemainingDevicePath)) {
    //
    // Enumerate the specified NVME namespace
    //
    Status = Private->Passthru.GetNamespace (
                                 &Private->Passthru,
                                 RemainingDevicePath,
                                 &NamespaceId,
                                 &NamespaceUuid
                                 );

    if (!EFI_ERROR (Status)) {
        Status = EnumerateNvmeDevNamespace (
                   Private,
                   NamespaceId,
                   NamespaceUuid
                   );
    }
  }

  DEBUG ((EFI_D_INFO, "NvmExpressDriverBindingStart: end successfully\n"));
  return EFI_SUCCESS;

Exit1:
  gBS->UninstallMultipleProtocolInterfaces (
         Controller,
         &gEfiCallerIdGuid,
         Private,
         NULL
         );
Exit2:
  if ((Private != NULL) && (Private->Mapping != NULL)) {
    PciIo->Unmap (PciIo, Private->Mapping);
  }

  if ((Private != NULL) && (Private->Buffer != NULL)) {
    PciIo->FreeBuffer (PciIo, 4, Private->Buffer);
  }

  if (Private != NULL) {
    FreePool (Private);
  }

  gBS->CloseProtocol (
         Controller,
         &gEfiPciIoProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  gBS->CloseProtocol (
         Controller,
         &gEfiDevicePathProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  DEBUG ((EFI_D_INFO, "NvmExpressDriverBindingStart: end with %r\n", Status));

  return Status;
}


/**
  Stops a device controller or a bus controller.

  The Stop() function is designed to be invoked from the EFI boot service DisconnectController().
  As a result, much of the error checking on the parameters to Stop() has been moved
  into this common boot service. It is legal to call Stop() from other locations,
  but the following calling restrictions must be followed or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE that was used on a previous call to this
     same driver's Start() function.
  2. The first NumberOfChildren handles of ChildHandleBuffer must all be a valid
     EFI_HANDLE. In addition, all of these handles must have been created in this driver's
     Start() function, and the Start() function must have called OpenProtocol() on
     ControllerHandle with an Attribute of EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped. The handle must
                                support a bus specific I/O protocol for the driver
                                to use to stop the device.
  @param[in]  NumberOfChildren  The number of child device handles in ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed. May be NULL
                                if NumberOfChildren is 0.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.

**/
EFI_STATUS
EFIAPI
NvmExpressDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL     *This,
  IN  EFI_HANDLE                      Controller,
  IN  UINTN                           NumberOfChildren,
  IN  EFI_HANDLE                      *ChildHandleBuffer
  )
{
  EFI_STATUS                          Status;
  BOOLEAN                             AllChildrenStopped;
  UINTN                               Index;
  NVME_CONTROLLER_PRIVATE_DATA        *Private;

  if (NumberOfChildren == 0) {
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiCallerIdGuid,
                    (VOID **) &Private,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );

    if (!EFI_ERROR (Status)) {
      gBS->UninstallMultipleProtocolInterfaces (
            Controller,
            &gEfiCallerIdGuid,
            Private,
            NULL
            );

      if (Private->Mapping != NULL) {
        Private->PciIo->Unmap (Private->PciIo, Private->Mapping);
      }

      if (Private->Buffer != NULL) {
        Private->PciIo->FreeBuffer (Private->PciIo, 4, Private->Buffer);
      }

      FreePool (Private->ControllerData);
      FreePool (Private);
    }

    gBS->CloseProtocol (
          Controller,
          &gEfiPciIoProtocolGuid,
          This->DriverBindingHandle,
          Controller
          );
    gBS->CloseProtocol (
          Controller,
          &gEfiDevicePathProtocolGuid,
          This->DriverBindingHandle,
          Controller
          );
    return EFI_SUCCESS;
  }

  AllChildrenStopped = TRUE;

  for (Index = 0; Index < NumberOfChildren; Index++) {
    Status = UnregisterNvmeNamespace (This, Controller, ChildHandleBuffer[Index]);
    if (EFI_ERROR (Status)) {
      AllChildrenStopped = FALSE;
    }
  }

  if (!AllChildrenStopped) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  This is the unload handle for the NVM Express driver.

  Disconnect the driver specified by ImageHandle from the NVMe device in the handle database.
  Uninstall all the protocols installed in the driver.

  @param[in]  ImageHandle       The drivers' driver image.

  @retval EFI_SUCCESS           The image is unloaded.
  @retval Others                Failed to unload the image.

**/
EFI_STATUS
EFIAPI
NvmExpressUnload (
  IN EFI_HANDLE             ImageHandle
  )
{
  EFI_STATUS                        Status;
  EFI_HANDLE                        *DeviceHandleBuffer;
  UINTN                             DeviceHandleCount;
  UINTN                             Index;
  EFI_COMPONENT_NAME_PROTOCOL       *ComponentName;
  EFI_COMPONENT_NAME2_PROTOCOL      *ComponentName2;

  //
  // Get the list of the device handles managed by this driver.
  // If there is an error getting the list, then means the driver
  // doesn't manage any device. At this way, we would only close
  // those protocols installed at image handle.
  //
  DeviceHandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiCallerIdGuid,
                  NULL,
                  &DeviceHandleCount,
                  &DeviceHandleBuffer
                  );

  if (!EFI_ERROR (Status)) {
    //
    // Disconnect the driver specified by ImageHandle from all
    // the devices in the handle database.
    //
    for (Index = 0; Index < DeviceHandleCount; Index++) {
      Status = gBS->DisconnectController (
                      DeviceHandleBuffer[Index],
                      ImageHandle,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        goto EXIT;
      }
    }
  }

  //
  // Uninstall all the protocols installed in the driver entry point
  //
  Status = gBS->UninstallMultipleProtocolInterfaces (
                  ImageHandle,
                  &gEfiDriverBindingProtocolGuid,
                  &gNvmExpressDriverBinding,
                  &gEfiDriverSupportedEfiVersionProtocolGuid,
                  &gNvmExpressDriverSupportedEfiVersion,
                  NULL
                  );

  if (EFI_ERROR (Status)) {
    goto EXIT;
  }

  //
  // Note we have to one by one uninstall the following protocols.
  // It's because some of them are optionally installed based on
  // the following PCD settings.
  //   gEfiMdePkgTokenSpaceGuid.PcdDriverDiagnosticsDisable
  //   gEfiMdePkgTokenSpaceGuid.PcdComponentNameDisable
  //   gEfiMdePkgTokenSpaceGuid.PcdDriverDiagnostics2Disable
  //   gEfiMdePkgTokenSpaceGuid.PcdComponentName2Disable
  //
  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiComponentNameProtocolGuid,
                  (VOID **) &ComponentName
                  );
  if (!EFI_ERROR (Status)) {
    gBS->UninstallProtocolInterface (
           ImageHandle,
           &gEfiComponentNameProtocolGuid,
           ComponentName
           );
  }

  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiComponentName2ProtocolGuid,
                  (VOID **) &ComponentName2
                  );
  if (!EFI_ERROR (Status)) {
    gBS->UninstallProtocolInterface (
           ImageHandle,
           &gEfiComponentName2ProtocolGuid,
           ComponentName2
           );
  }

  Status = EFI_SUCCESS;

EXIT:
  //
  // Free the buffer containing the list of handles from the handle database
  //
  if (DeviceHandleBuffer != NULL) {
    gBS->FreePool (DeviceHandleBuffer);
  }
  return Status;
}

/**
  The entry point for Nvm Express driver, used to install Nvm Express driver on the ImageHandle.

  @param  ImageHandle   The firmware allocated handle for this driver image.
  @param  SystemTable   Pointer to the EFI system table.

  @retval EFI_SUCCESS   Driver loaded.
  @retval other         Driver not loaded.

**/
EFI_STATUS
EFIAPI
NvmExpressDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS              Status;

  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gNvmExpressDriverBinding,
             ImageHandle,
             &gNvmExpressComponentName,
             &gNvmExpressComponentName2
             );
  ASSERT_EFI_ERROR (Status);

  //
  // Install EFI Driver Supported EFI Version Protocol required for
  // EFI drivers that are on PCI and other plug in cards.
  //
  gNvmExpressDriverSupportedEfiVersion.FirmwareVersion = 0x00020028;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiDriverSupportedEfiVersionProtocolGuid,
                  &gNvmExpressDriverSupportedEfiVersion,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);
  return Status;
}
