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

#ifndef _EFI_NVM_EXPRESS_H_
#define _EFI_NVM_EXPRESS_H_

#include <Uefi.h>

#include <IndustryStandard/Pci.h>

#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/PciIo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskInfo.h>
#include <Protocol/DriverSupportedEfiVersion.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>

typedef struct _NVME_CONTROLLER_PRIVATE_DATA NVME_CONTROLLER_PRIVATE_DATA;
typedef struct _NVME_DEVICE_PRIVATE_DATA     NVME_DEVICE_PRIVATE_DATA;

#include "NvmExpressPassthru.h"
#include "NvmExpressBlockIo.h"
#include "NvmExpressDiskInfo.h"
#include "NvmExpressHci.h"

extern EFI_DRIVER_BINDING_PROTOCOL                gNvmExpressDriverBinding;
extern EFI_COMPONENT_NAME_PROTOCOL                gNvmExpressComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL               gNvmExpressComponentName2;
extern EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL  gNvmExpressDriverSupportedEfiVersion;

#define PCI_CLASS_MASS_STORAGE_NVM                0x08  // mass storage sub-class non-volatile memory.
#define PCI_IF_NVMHCI                             0x02  // mass storage programming interface NVMHCI.

#define NVME_ASQ_SIZE                             1     // Number of admin submission queue entries, which is 0-based
#define NVME_ACQ_SIZE                             1     // Number of admin completion queue entries, which is 0-based

#define NVME_CSQ_SIZE                             1     // Number of I/O submission queue entries, which is 0-based
#define NVME_CCQ_SIZE                             1     // Number of I/O completion queue entries, which is 0-based

#define NVME_MAX_IO_QUEUES                        2     // Number of I/O queues supported by the driver

#define NVME_CONTROLLER_ID                        0

//
// Time out value for Nvme transaction execution
//
#define NVME_GENERIC_TIMEOUT                      EFI_TIMER_PERIOD_SECONDS (5)

//
// Unique signature for private data structure.
//
#define NVME_CONTROLLER_PRIVATE_DATA_SIGNATURE    SIGNATURE_32 ('N','V','M','E')

//
// Nvme private data structure.
//
struct _NVME_CONTROLLER_PRIVATE_DATA {
  UINT32                          Signature;

  EFI_HANDLE                      ControllerHandle;
  EFI_HANDLE                      ImageHandle;
  EFI_HANDLE                      DriverBindingHandle;

  EFI_PCI_IO_PROTOCOL             *PciIo;
  UINT64                          PciAttributes;

  EFI_DEVICE_PATH_PROTOCOL        *ParentDevicePath;

  NVM_EXPRESS_PASS_THRU_MODE      PassThruMode;
  NVM_EXPRESS_PASS_THRU_PROTOCOL  Passthru;

  //
  // pointer to identify controller data
  //
  NVME_ADMIN_CONTROLLER_DATA      *ControllerData;

  //
  // 6 x 4kB aligned buffers will be carved out of this buffer.
  // 1st 4kB boundary is the start of the admin submission queue.
  // 2nd 4kB boundary is the start of submission queue #1.
  // 3rd 4kB boundary is the start of the admin completion queue.
  // 4th 4kB boundary is the start of completion queue #1.
  // 5th 4kB boundary is the start of the first PRP list page.
  // 6th 4kB boundary is the start of the second PRP list page.
  //
  UINT8                           *Buffer;
  UINT8                           *BufferPciAddr;

  //
  // Pointers to 4kB aligned submission & completion queues.
  //
  NVME_SQ                         *SqBuffer[NVME_MAX_IO_QUEUES];
  NVME_CQ                         *CqBuffer[NVME_MAX_IO_QUEUES];
  NVME_SQ                         *SqBufferPciAddr[NVME_MAX_IO_QUEUES];
  NVME_CQ                         *CqBufferPciAddr[NVME_MAX_IO_QUEUES];

  //
  // Submission and completion queue indices.
  //
  NVME_SQTDBL                     SqTdbl[NVME_MAX_IO_QUEUES];
  NVME_CQHDBL                     CqHdbl[NVME_MAX_IO_QUEUES];

  UINT8                           Pt[2];
  UINT16                          Cid[2];

  //
  // Nvme controller capabilities
  //
  NVME_CAP                        Cap;

  VOID                            *Mapping;
};

#define NVME_CONTROLLER_PRIVATE_DATA_FROM_PASS_THRU(a) \
  CR (a, \
      NVME_CONTROLLER_PRIVATE_DATA, \
      Passthru, \
      NVME_CONTROLLER_PRIVATE_DATA_SIGNATURE \
      )

//
// Unique signature for private data structure.
//
#define NVME_DEVICE_PRIVATE_DATA_SIGNATURE     SIGNATURE_32 ('X','S','S','D')

//
// Nvme device private data structure
//
struct _NVME_DEVICE_PRIVATE_DATA {
  UINT32                            Signature;

  EFI_HANDLE                        DeviceHandle;
  EFI_HANDLE                        ControllerHandle;
  EFI_HANDLE                        DriverBindingHandle;

  EFI_DEVICE_PATH_PROTOCOL          *DevicePath;

  EFI_UNICODE_STRING_TABLE          *ControllerNameTable;

  UINT32                            NamespaceId;
  UINT64                            NamespaceUuid;

  EFI_BLOCK_IO_MEDIA                Media;
  EFI_BLOCK_IO_PROTOCOL             BlockIo;
  EFI_DISK_INFO_PROTOCOL            DiskInfo;

  EFI_LBA                           NumBlocks;

  CHAR16                            ModelName[80];
  NVME_ADMIN_NAMESPACE_DATA         NamespaceData;

  NVME_CONTROLLER_PRIVATE_DATA      *Controller;

};

//
// Statments to retrieve the private data from produced protocols.
//
#define NVME_DEVICE_PRIVATE_DATA_FROM_BLOCK_IO(a) \
  CR (a, \
      NVME_DEVICE_PRIVATE_DATA, \
      BlockIo, \
      NVME_DEVICE_PRIVATE_DATA_SIGNATURE \
      )

#define NVME_DEVICE_PRIVATE_DATA_FROM_DISK_INFO(a) \
  CR (a, \
      NVME_DEVICE_PRIVATE_DATA, \
      DiskInfo, \
      NVME_DEVICE_PRIVATE_DATA_SIGNATURE \
      )

/**
  Retrieves a Unicode string that is the user readable name of the driver.

  This function retrieves the user readable name of a driver in the form of a
  Unicode string. If the driver specified by This has a user readable name in
  the language specified by Language, then a pointer to the driver name is
  returned in DriverName, and EFI_SUCCESS is returned. If the driver specified
  by This does not support the language specified by Language,
  then EFI_UNSUPPORTED is returned.

  @param  This[in]              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.

  @param  Language[in]          A pointer to a Null-terminated ASCII string
                                array indicating the language. This is the
                                language of the driver name that the caller is
                                requesting, and it must match one of the
                                languages specified in SupportedLanguages. The
                                number of languages supported by a driver is up
                                to the driver writer. Language is specified
                                in RFC 4646 or ISO 639-2 language code format.

  @param  DriverName[out]       A pointer to the Unicode string to return.
                                This Unicode string is the name of the
                                driver specified by This in the language
                                specified by Language.

  @retval EFI_SUCCESS           The Unicode string for the Driver specified by
                                This and the language specified by Language was
                                returned in DriverName.

  @retval EFI_INVALID_PARAMETER Language is NULL.

  @retval EFI_INVALID_PARAMETER DriverName is NULL.

  @retval EFI_UNSUPPORTED       The driver specified by This does not support
                                the language specified by Language.

**/
EFI_STATUS
EFIAPI
NvmExpressComponentNameGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  );

/**
  Retrieves a Unicode string that is the user readable name of the controller
  that is being managed by a driver.

  This function retrieves the user readable name of the controller specified by
  ControllerHandle and ChildHandle in the form of a Unicode string. If the
  driver specified by This has a user readable name in the language specified by
  Language, then a pointer to the controller name is returned in ControllerName,
  and EFI_SUCCESS is returned.  If the driver specified by This is not currently
  managing the controller specified by ControllerHandle and ChildHandle,
  then EFI_UNSUPPORTED is returned.  If the driver specified by This does not
  support the language specified by Language, then EFI_UNSUPPORTED is returned.

  @param  This[in]              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.

  @param  ControllerHandle[in]  The handle of a controller that the driver
                                specified by This is managing.  This handle
                                specifies the controller whose name is to be
                                returned.

  @param  ChildHandle[in]       The handle of the child controller to retrieve
                                the name of.  This is an optional parameter that
                                may be NULL.  It will be NULL for device
                                drivers.  It will also be NULL for a bus drivers
                                that wish to retrieve the name of the bus
                                controller.  It will not be NULL for a bus
                                driver that wishes to retrieve the name of a
                                child controller.

  @param  Language[in]          A pointer to a Null-terminated ASCII string
                                array indicating the language.  This is the
                                language of the driver name that the caller is
                                requesting, and it must match one of the
                                languages specified in SupportedLanguages. The
                                number of languages supported by a driver is up
                                to the driver writer. Language is specified in
                                RFC 4646 or ISO 639-2 language code format.

  @param  ControllerName[out]   A pointer to the Unicode string to return.
                                This Unicode string is the name of the
                                controller specified by ControllerHandle and
                                ChildHandle in the language specified by
                                Language from the point of view of the driver
                                specified by This.

  @retval EFI_SUCCESS           The Unicode string for the user readable name in
                                the language specified by Language for the
                                driver specified by This was returned in
                                DriverName.

  @retval EFI_INVALID_PARAMETER ControllerHandle is NULL.

  @retval EFI_INVALID_PARAMETER ChildHandle is not NULL and it is not a valid
                                EFI_HANDLE.

  @retval EFI_INVALID_PARAMETER Language is NULL.

  @retval EFI_INVALID_PARAMETER ControllerName is NULL.

  @retval EFI_UNSUPPORTED       The driver specified by This is not currently
                                managing the controller specified by
                                ControllerHandle and ChildHandle.

  @retval EFI_UNSUPPORTED       The driver specified by This does not support
                                the language specified by Language.

**/
EFI_STATUS
EFIAPI
NvmExpressComponentNameGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL                     *This,
  IN  EFI_HANDLE                                      ControllerHandle,
  IN  EFI_HANDLE                                      ChildHandle        OPTIONAL,
  IN  CHAR8                                           *Language,
  OUT CHAR16                                          **ControllerName
  );

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
  );

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
  );

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
  );

/**
  Sends an NVM Express Command Packet to an NVM Express controller or namespace. This function supports
  both blocking I/O and nonblocking I/O. The blocking I/O functionality is required, and the nonblocking
  I/O functionality is optional.

  @param[in]     This                A pointer to the NVM_EXPRESS_PASS_THRU_PROTOCOL instance.
  @param[in]     NamespaceId         Is a 32 bit Namespace ID to which the Express HCI command packet will be sent.
                                     A value of 0 denotes the NVM Express controller, a value of all 0FFh in the namespace
                                     ID specifies that the command packet should be sent to all valid namespaces.
  @param[in]     NamespaceUuid       Is a 64 bit Namespace UUID to which the Express HCI command packet will be sent.
                                     A value of 0 denotes the NVM Express controller, a value of all 0FFh in the namespace
                                     UUID specifies that the command packet should be sent to all valid namespaces.
  @param[in,out] Packet              A pointer to the NVM Express HCI Command Packet to send to the NVMe namespace specified
                                     by NamespaceId.
  @param[in]     Event               If nonblocking I/O is not supported then Event is ignored, and blocking I/O is performed.
                                     If Event is NULL, then blocking I/O is performed. If Event is not NULL and non blocking I/O
                                     is supported, then nonblocking I/O is performed, and Event will be signaled when the NVM
                                     Express Command Packet completes.

  @retval EFI_SUCCESS                The NVM Express Command Packet was sent by the host. TransferLength bytes were transferred
                                     to, or from DataBuffer.
  @retval EFI_BAD_BUFFER_SIZE        The NVM Express Command Packet was not executed. The number of bytes that could be transferred
                                     is returned in TransferLength.
  @retval EFI_NOT_READY              The NVM Express Command Packet could not be sent because the controller is not ready. The caller
                                     may retry again later.
  @retval EFI_DEVICE_ERROR           A device error occurred while attempting to send the NVM Express Command Packet.
  @retval EFI_INVALID_PARAMETER      Namespace, or the contents of NVM_EXPRESS_PASS_THRU_COMMAND_PACKET are invalid. The NVM
                                     Express Command Packet was not sent, so no additional status information is available.
  @retval EFI_UNSUPPORTED            The command described by the NVM Express Command Packet is not supported by the host adapter.
                                     The NVM Express Command Packet was not sent, so no additional status information is available.
  @retval EFI_TIMEOUT                A timeout occurred while waiting for the NVM Express Command Packet to execute.

**/
EFI_STATUS
EFIAPI
NvmExpressPassThru (
  IN     NVM_EXPRESS_PASS_THRU_PROTOCOL              *This,
  IN     UINT32                                      NamespaceId,
  IN     UINT64                                      NamespaceUuid,
  IN OUT NVM_EXPRESS_PASS_THRU_COMMAND_PACKET        *Packet,
  IN     EFI_EVENT                                   Event OPTIONAL
  );

/**
  Used to retrieve the list of namespaces defined on an NVM Express controller.

  The NVM_EXPRESS_PASS_THRU_PROTOCOL.GetNextNamespace() function retrieves a list of namespaces
  defined on an NVM Express controller. If on input a NamespaceID is specified by all 0xFF in the
  namespace buffer, then the first namespace defined on the NVM Express controller is returned in
  NamespaceID, and a status of EFI_SUCCESS is returned.

  If NamespaceId is a Namespace value that was returned on a previous call to GetNextNamespace(),
  then the next valid NamespaceId  for an NVM Express SSD namespace on the NVM Express controller
  is returned in NamespaceId, and EFI_SUCCESS is returned.

  If Namespace array is not a 0xFFFFFFFF and NamespaceId was not returned on a previous call to
  GetNextNamespace(), then EFI_INVALID_PARAMETER is returned.

  If NamespaceId is the NamespaceId of the last SSD namespace on the NVM Express controller, then
  EFI_NOT_FOUND is returned

  @param[in]     This           A pointer to the NVM_EXPRESS_PASS_THRU_PROTOCOL instance.
  @param[in,out] NamespaceId    On input, a pointer to a legal NamespaceId for an NVM Express
                                namespace present on the NVM Express controller. On output, a
                                pointer to the next NamespaceId of an NVM Express namespace on
                                an NVM Express controller. An input value of 0xFFFFFFFF retrieves
                                the first NamespaceId for an NVM Express namespace present on an
                                NVM Express controller.
  @param[out]    NamespaceUuid  On output, the UUID associated with the next namespace, if a UUID
                                is defined for that NamespaceId, otherwise, zero is returned in
                                this parameter. If the caller does not require a UUID, then a NULL
                                pointer may be passed.

  @retval EFI_SUCCESS           The NamespaceId of the next Namespace was returned.
  @retval EFI_NOT_FOUND         There are no more namespaces defined on this controller.
  @retval EFI_INVALID_PARAMETER Namespace array is not a 0xFFFFFFFF and NamespaceId was not returned
                                on a previous call to GetNextNamespace().

**/
EFI_STATUS
EFIAPI
NvmExpressGetNextNamespace (
  IN     NVM_EXPRESS_PASS_THRU_PROTOCOL              *This,
  IN OUT UINT32                                      *NamespaceId,
     OUT UINT64                                      *NamespaceUuid  OPTIONAL
  );

/**
  Used to translate a device path node to a Target ID and LUN.

  The NVM_EXPRESS_PASS_THRU_PROTOCOL.GetNamwspace() function determines the Namespace ID and Namespace UUID
  associated with the NVM Express SSD namespace described by DevicePath. If DevicePath is a device path node type
  that the NVM Express Pass Thru driver supports, then the NVM Express Pass Thru driver will attempt to translate
  the contents DevicePath into a Namespace ID and UUID. If this translation is successful, then that Namespace ID
  and UUID are returned in NamespaceID and NamespaceUUID, and EFI_SUCCESS is returned.

  @param[in]  This                A pointer to the NVM_EXPRESS_PASS_THRU_PROTOCOL instance.
  @param[in]  DevicePath          A pointer to the device path node that describes an NVM Express namespace on
                                  the NVM Express controller.
  @param[out] NamespaceId         The NVM Express namespace ID contained in the device path node.
  @param[out] NamespaceUuid       The NVM Express namespace contained in the device path node.

  @retval EFI_SUCCESS             DevicePath was successfully translated to NamespaceId and NamespaceUuid.
  @retval EFI_INVALID_PARAMETER   If DevicePath, NamespaceId, or NamespaceUuid are NULL, then EFI_INVALID_PARAMETER
                                  is returned.
  @retval EFI_UNSUPPORTED         If DevicePath is not a device path node type that the NVM Express Pass Thru driver
                                  supports, then EFI_UNSUPPORTED is returned.
  @retval EFI_NOT_FOUND           If DevicePath is a device path node type that the Nvm Express Pass Thru driver
                                  supports, but there is not a valid translation from DevicePath to a NamespaceID
                                  and NamespaceUuid, then EFI_NOT_FOUND is returned.
**/
EFI_STATUS
EFIAPI
NvmExpressGetNamespace (
  IN     NVM_EXPRESS_PASS_THRU_PROTOCOL              *This,
  IN     EFI_DEVICE_PATH_PROTOCOL                    *DevicePath,
     OUT UINT32                                      *NamespaceId,
     OUT UINT64                                      *NamespaceUuid
  );

/**
  Used to allocate and build a device path node for an NVM Express namespace on an NVM Express controller.

  The NVM_EXPRESS_PASS_THRU_PROTOCOL.BuildDevicePath() function allocates and builds a single device
  path node for the NVM Express namespace specified by NamespaceId.

  If the namespace device specified by NamespaceId is not valid , then EFI_NOT_FOUND is returned.

  If DevicePath is NULL, then EFI_INVALID_PARAMETER is returned.

  If there are not enough resources to allocate the device path node, then EFI_OUT_OF_RESOURCES is returned.

  Otherwise, DevicePath is allocated with the boot service AllocatePool(), the contents of DevicePath are
  initialized to describe the NVM Express namespace specified by NamespaceId, and EFI_SUCCESS is returned.

  @param[in]     This                A pointer to the NVM_EXPRESS_PASS_THRU_PROTOCOL instance.
  @param[in]     NamespaceId         The NVM Express namespace ID  for which a device path node is to be
                                     allocated and built. Caller must set the NamespaceId to zero if the
                                     device path node will contain a valid UUID.
  @param[in]     NamespaceUuid       The NVM Express namespace UUID for which a device path node is to be
                                     allocated and built. UUID will only be valid of the Namespace ID is zero.
  @param[in,out] DevicePath          A pointer to a single device path node that describes the NVM Express
                                     namespace specified by NamespaceId. This function is responsible for
                                     allocating the buffer DevicePath with the boot service AllocatePool().
                                     It is the caller's responsibility to free DevicePath when the caller
                                     is finished with DevicePath.
  @retval EFI_SUCCESS                The device path node that describes the NVM Express namespace specified
                                     by NamespaceId was allocated and returned in DevicePath.
  @retval EFI_NOT_FOUND              The NVM Express namespace specified by NamespaceId does not exist on the
                                     NVM Express controller.
  @retval EFI_INVALID_PARAMETER      DevicePath is NULL.
  @retval EFI_OUT_OF_RESOURCES       There are not enough resources to allocate the DevicePath node.

**/
EFI_STATUS
EFIAPI
NvmExpressBuildDevicePath (
  IN     NVM_EXPRESS_PASS_THRU_PROTOCOL              *This,
  IN     UINT32                                      NamespaceId,
  IN     UINT64                                      NamespaceUuid,
  IN OUT EFI_DEVICE_PATH_PROTOCOL                    **DevicePath
  );

#endif
