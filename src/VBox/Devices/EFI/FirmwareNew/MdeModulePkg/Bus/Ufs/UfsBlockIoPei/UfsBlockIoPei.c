/** @file

  Copyright (c) 2014, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "UfsBlockIoPei.h"

//
// Template for UFS HC Peim Private Data.
//
UFS_PEIM_HC_PRIVATE_DATA   gUfsHcPeimTemplate = {
  UFS_PEIM_HC_SIG,                // Signature
  NULL,                           // Controller
  NULL,                           // Pool
  {                               // BlkIoPpi
    UfsBlockIoPeimGetDeviceNo,
    UfsBlockIoPeimGetMediaInfo,
    UfsBlockIoPeimReadBlocks
  },
  {                               // BlkIoPpiList
    EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST,
    &gEfiPeiVirtualBlockIoPpiGuid,
    NULL
  },
  {                               // Media
    {
      UfsDevice,
      TRUE,
      0,
      0x1000
    },
    {
      UfsDevice,
      TRUE,
      0,
      0x1000
    },
    {
      UfsDevice,
      TRUE,
      0,
      0x1000
    },
    {
      UfsDevice,
      TRUE,
      0,
      0x1000
    },
    {
      UfsDevice,
      TRUE,
      0,
      0x1000
    },
    {
      UfsDevice,
      TRUE,
      0,
      0x1000
    },
    {
      UfsDevice,
      TRUE,
      0,
      0x1000
    },
    {
      UfsDevice,
      TRUE,
      0,
      0x1000
    }
  },
  0,                              // UfsHcBase
  0,                              // Capabilities
  0,                              // TaskTag
  0,                              // UtpTrlBase
  0,                              // Nutrs
  0,                              // UtpTmrlBase
  0,                              // Nutmrs
  {                               // Luns
    {
      UFS_LUN_0,                      // Ufs Common Lun 0
      UFS_LUN_1,                      // Ufs Common Lun 1
      UFS_LUN_2,                      // Ufs Common Lun 2
      UFS_LUN_3,                      // Ufs Common Lun 3
      UFS_LUN_4,                      // Ufs Common Lun 4
      UFS_LUN_5,                      // Ufs Common Lun 5
      UFS_LUN_6,                      // Ufs Common Lun 6
      UFS_LUN_7,                      // Ufs Common Lun 7
    },
    0x0000,                           // By default exposing all Luns.
    0x0
  }
};

/**
  Execute Request Sense SCSI command on a specific UFS device.

  @param[in]  Private              A pointer to UFS_PEIM_HC_PRIVATE_DATA data structure.
  @param[in]  Lun                  The lun on which the SCSI cmd executed.
  @param[out] DataBuffer           A pointer to output sense data.
  @param[out] DataBufferLength     The length of output sense data.

  @retval EFI_SUCCESS              The command executed successfully.
  @retval EFI_DEVICE_ERROR         A device error occurred while attempting to send SCSI Request Packet.
  @retval EFI_TIMEOUT              A timeout occurred while waiting for the SCSI Request Packet to execute.

**/
EFI_STATUS
UfsPeimRequestSense (
  IN     UFS_PEIM_HC_PRIVATE_DATA        *Private,
  IN     UINTN                           Lun,
     OUT VOID                            *DataBuffer,
     OUT UINT32                          *DataBufferLength
  )
{
  UFS_SCSI_REQUEST_PACKET                Packet;
  UINT8                                  Cdb[UFS_SCSI_OP_LENGTH_SIX];
  EFI_STATUS                             Status;

  ZeroMem (&Packet, sizeof (UFS_SCSI_REQUEST_PACKET));
  ZeroMem (Cdb, sizeof (Cdb));

  Cdb[0]  = EFI_SCSI_OP_REQUEST_SENSE;

  Packet.Timeout          = UFS_TIMEOUT;
  Packet.Cdb              = Cdb;
  Packet.CdbLength        = sizeof (Cdb);
  Packet.DataDirection    = UfsDataIn;
  Packet.InDataBuffer     = DataBuffer;
  Packet.InTransferLength = *DataBufferLength;
  Packet.SenseData        = NULL;
  Packet.SenseDataLength  = 0;

  Status = UfsExecScsiCmds (Private,(UINT8)Lun, &Packet);

  if (!EFI_ERROR (Status)) {
    *DataBufferLength = Packet.InTransferLength;
  }

  return Status;
}

/**
  Execute TEST UNITY READY SCSI command on a specific UFS device.

  @param[in]  Private              A pointer to UFS_PEIM_HC_PRIVATE_DATA data structure.
  @param[in]  Lun                  The lun on which the SCSI cmd executed.
  @param[out] SenseData            A pointer to output sense data.
  @param[out] SenseDataLength      The length of output sense data.

  @retval EFI_SUCCESS              The command executed successfully.
  @retval EFI_DEVICE_ERROR         A device error occurred while attempting to send SCSI Request Packet.
  @retval EFI_TIMEOUT              A timeout occurred while waiting for the SCSI Request Packet to execute.

**/
EFI_STATUS
UfsPeimTestUnitReady (
  IN     UFS_PEIM_HC_PRIVATE_DATA        *Private,
  IN     UINTN                           Lun,
     OUT VOID                            *SenseData,  OPTIONAL
     OUT UINT8                           *SenseDataLength
  )
{
  UFS_SCSI_REQUEST_PACKET                Packet;
  UINT8                                  Cdb[UFS_SCSI_OP_LENGTH_SIX];
  EFI_STATUS                             Status;

  ZeroMem (&Packet, sizeof (UFS_SCSI_REQUEST_PACKET));
  ZeroMem (Cdb, sizeof (Cdb));

  Cdb[0]  = EFI_SCSI_OP_TEST_UNIT_READY;

  Packet.Timeout         = UFS_TIMEOUT;
  Packet.Cdb             = Cdb;
  Packet.CdbLength       = sizeof (Cdb);
  Packet.DataDirection   = UfsNoData;
  Packet.SenseData       = SenseData;
  Packet.SenseDataLength = *SenseDataLength;

  Status = UfsExecScsiCmds (Private,(UINT8)Lun, &Packet);

  if (*SenseDataLength != 0) {
    *SenseDataLength = Packet.SenseDataLength;
  }

  return Status;
}

/**
  Execute INQUIRY SCSI command on a specific UFS device.

  @param[in]  Private              A pointer to UFS_PEIM_HC_PRIVATE_DATA data structure.
  @param[in]  Lun                  The lun on which the SCSI cmd executed.
  @param[out] Inquiry              A pointer to Inquiry data buffer.
  @param[out] InquiryLengths       The length of output Inquiry data.
  @param[out] SenseData            A pointer to output sense data.
  @param[out] SenseDataLength      The length of output sense data.

  @retval EFI_SUCCESS              The command executed successfully.
  @retval EFI_DEVICE_ERROR         A device error occurred while attempting to send SCSI Request Packet.
  @retval EFI_TIMEOUT              A timeout occurred while waiting for the SCSI Request Packet to execute.

**/
EFI_STATUS
UfsPeimInquiry (
  IN     UFS_PEIM_HC_PRIVATE_DATA     *Private,
  IN     UINTN                        Lun,
     OUT VOID                         *Inquiry,
     OUT UINT32                       *InquiryLength,
     OUT VOID                         *SenseData,  OPTIONAL
     OUT UINT8                        *SenseDataLength
  )
{
  UFS_SCSI_REQUEST_PACKET             Packet;
  UINT8                               Cdb[UFS_SCSI_OP_LENGTH_SIX];
  EFI_STATUS                          Status;

  ZeroMem (&Packet, sizeof (UFS_SCSI_REQUEST_PACKET));
  ZeroMem (Cdb, sizeof (Cdb));

  Cdb[0]  = EFI_SCSI_OP_INQUIRY;
  Cdb[4]  = sizeof (EFI_SCSI_INQUIRY_DATA);

  Packet.Timeout          = UFS_TIMEOUT;
  Packet.Cdb              = Cdb;
  Packet.CdbLength        = sizeof (Cdb);
  Packet.InDataBuffer     = Inquiry;
  Packet.InTransferLength = *InquiryLength;
  Packet.DataDirection    = UfsDataIn;
  Packet.SenseData        = SenseData;
  Packet.SenseDataLength  = *SenseDataLength;

  Status = UfsExecScsiCmds (Private, (UINT8)Lun, &Packet);

  if (*SenseDataLength != 0) {
    *SenseDataLength = Packet.SenseDataLength;
  }

  if (!EFI_ERROR (Status)) {
    *InquiryLength = Packet.InTransferLength;
  }

  return Status;
}

/**
  Execute READ CAPACITY(10) SCSI command on a specific UFS device.

  @param[in]  Private              A pointer to UFS_PEIM_HC_PRIVATE_DATA data structure.
  @param[in]  Lun                  The lun on which the SCSI cmd executed.
  @param[out] DataBuffer           A pointer to READ_CAPACITY data buffer.
  @param[out] DataLength           The length of output READ_CAPACITY data.
  @param[out] SenseData            A pointer to output sense data.
  @param[out] SenseDataLength      The length of output sense data.

  @retval EFI_SUCCESS              The command executed successfully.
  @retval EFI_DEVICE_ERROR         A device error occurred while attempting to send SCSI Request Packet.
  @retval EFI_TIMEOUT              A timeout occurred while waiting for the SCSI Request Packet to execute.

**/
EFI_STATUS
UfsPeimReadCapacity (
  IN     UFS_PEIM_HC_PRIVATE_DATA     *Private,
  IN     UINTN                        Lun,
     OUT VOID                         *DataBuffer,
     OUT UINT32                       *DataLength,
     OUT VOID                         *SenseData,  OPTIONAL
     OUT UINT8                        *SenseDataLength
  )
{
  UFS_SCSI_REQUEST_PACKET             Packet;
  UINT8                               Cdb[UFS_SCSI_OP_LENGTH_TEN];
  EFI_STATUS                          Status;

  ZeroMem (&Packet, sizeof (UFS_SCSI_REQUEST_PACKET));
  ZeroMem (Cdb, sizeof (Cdb));

  Cdb[0] = EFI_SCSI_OP_READ_CAPACITY;

  Packet.Timeout          = UFS_TIMEOUT;
  Packet.Cdb              = Cdb;
  Packet.CdbLength        = sizeof (Cdb);
  Packet.InDataBuffer     = DataBuffer;
  Packet.InTransferLength = *DataLength;
  Packet.DataDirection    = UfsDataIn;
  Packet.SenseData        = SenseData;
  Packet.SenseDataLength  = *SenseDataLength;

  Status = UfsExecScsiCmds (Private, (UINT8)Lun, &Packet);

  if (*SenseDataLength != 0) {
    *SenseDataLength = Packet.SenseDataLength;
  }

  if (!EFI_ERROR (Status)) {
    *DataLength = Packet.InTransferLength;
  }

  return Status;
}

/**
  Execute READ CAPACITY(16) SCSI command on a specific UFS device.

  @param[in]  Private              A pointer to UFS_PEIM_HC_PRIVATE_DATA data structure.
  @param[in]  Lun                  The lun on which the SCSI cmd executed.
  @param[out] DataBuffer           A pointer to READ_CAPACITY data buffer.
  @param[out] DataLength           The length of output READ_CAPACITY data.
  @param[out] SenseData            A pointer to output sense data.
  @param[out] SenseDataLength      The length of output sense data.

  @retval EFI_SUCCESS              The command executed successfully.
  @retval EFI_DEVICE_ERROR         A device error occurred while attempting to send SCSI Request Packet.
  @retval EFI_TIMEOUT              A timeout occurred while waiting for the SCSI Request Packet to execute.

**/
EFI_STATUS
UfsPeimReadCapacity16 (
  IN     UFS_PEIM_HC_PRIVATE_DATA     *Private,
  IN     UINTN                        Lun,
     OUT VOID                         *DataBuffer,
     OUT UINT32                       *DataLength,
     OUT VOID                         *SenseData,  OPTIONAL
     OUT UINT8                        *SenseDataLength
  )
{
  UFS_SCSI_REQUEST_PACKET             Packet;
  UINT8                               Cdb[UFS_SCSI_OP_LENGTH_SIXTEEN];
  EFI_STATUS                          Status;

  ZeroMem (&Packet, sizeof (UFS_SCSI_REQUEST_PACKET));
  ZeroMem (Cdb, sizeof (Cdb));

  Cdb[0]  = EFI_SCSI_OP_READ_CAPACITY16;
  Cdb[1]  = 0x10;          // Service Action should be 0x10 for UFS device.
  Cdb[13] = 0x20;          // The maximum number of bytes for returned data.

  Packet.Timeout          = UFS_TIMEOUT;
  Packet.Cdb              = Cdb;
  Packet.CdbLength        = sizeof (Cdb);
  Packet.InDataBuffer     = DataBuffer;
  Packet.InTransferLength = *DataLength;
  Packet.DataDirection    = UfsDataIn;
  Packet.SenseData        = SenseData;
  Packet.SenseDataLength  = *SenseDataLength;

  Status = UfsExecScsiCmds (Private, (UINT8)Lun, &Packet);

  if (*SenseDataLength != 0) {
    *SenseDataLength = Packet.SenseDataLength;
  }

  if (!EFI_ERROR (Status)) {
    *DataLength = Packet.InTransferLength;
  }

  return Status;
}

/**
  Execute READ (10) SCSI command on a specific UFS device.

  @param[in]  Private              A pointer to UFS_PEIM_HC_PRIVATE_DATA data structure.
  @param[in]  Lun                  The lun on which the SCSI cmd executed.
  @param[in]  StartLba             The start LBA.
  @param[in]  SectorNum            The sector number to be read.
  @param[out] DataBuffer           A pointer to data buffer.
  @param[out] DataLength           The length of output data.
  @param[out] SenseData            A pointer to output sense data.
  @param[out] SenseDataLength      The length of output sense data.

  @retval EFI_SUCCESS              The command executed successfully.
  @retval EFI_DEVICE_ERROR         A device error occurred while attempting to send SCSI Request Packet.
  @retval EFI_TIMEOUT              A timeout occurred while waiting for the SCSI Request Packet to execute.

**/
EFI_STATUS
UfsPeimRead10 (
  IN     UFS_PEIM_HC_PRIVATE_DATA     *Private,
  IN     UINTN                        Lun,
  IN     UINTN                        StartLba,
  IN     UINT32                       SectorNum,
     OUT VOID                         *DataBuffer,
     OUT UINT32                       *DataLength,
     OUT VOID                         *SenseData,  OPTIONAL
     OUT UINT8                        *SenseDataLength
  )
{
  UFS_SCSI_REQUEST_PACKET             Packet;
  UINT8                               Cdb[UFS_SCSI_OP_LENGTH_TEN];
  EFI_STATUS                          Status;

  ZeroMem (&Packet, sizeof (UFS_SCSI_REQUEST_PACKET));
  ZeroMem (Cdb, sizeof (Cdb));

  Cdb[0] = EFI_SCSI_OP_READ10;
  WriteUnaligned32 ((UINT32 *)&Cdb[2], SwapBytes32 ((UINT32) StartLba));
  WriteUnaligned16 ((UINT16 *)&Cdb[7], SwapBytes16 ((UINT16) SectorNum));

  Packet.Timeout          = UFS_TIMEOUT;
  Packet.Cdb              = Cdb;
  Packet.CdbLength        = sizeof (Cdb);
  Packet.InDataBuffer     = DataBuffer;
  Packet.InTransferLength = *DataLength;
  Packet.DataDirection    = UfsDataIn;
  Packet.SenseData        = SenseData;
  Packet.SenseDataLength  = *SenseDataLength;

  Status = UfsExecScsiCmds (Private, (UINT8)Lun, &Packet);

  if (*SenseDataLength != 0) {
    *SenseDataLength = Packet.SenseDataLength;
  }

  if (!EFI_ERROR (Status)) {
    *DataLength = Packet.InTransferLength;
  }

  return Status;
}

/**
  Execute READ (16) SCSI command on a specific UFS device.

  @param[in]  Private              A pointer to UFS_PEIM_HC_PRIVATE_DATA data structure.
  @param[in]  Lun                  The lun on which the SCSI cmd executed.
  @param[in]  StartLba             The start LBA.
  @param[in]  SectorNum            The sector number to be read.
  @param[out] DataBuffer           A pointer to data buffer.
  @param[out] DataLength           The length of output data.
  @param[out] SenseData            A pointer to output sense data.
  @param[out] SenseDataLength      The length of output sense data.

  @retval EFI_SUCCESS              The command executed successfully.
  @retval EFI_DEVICE_ERROR         A device error occurred while attempting to send SCSI Request Packet.
  @retval EFI_TIMEOUT              A timeout occurred while waiting for the SCSI Request Packet to execute.

**/
EFI_STATUS
UfsPeimRead16 (
  IN     UFS_PEIM_HC_PRIVATE_DATA     *Private,
  IN     UINTN                        Lun,
  IN     UINTN                        StartLba,
  IN     UINT32                       SectorNum,
     OUT VOID                         *DataBuffer,
     OUT UINT32                       *DataLength,
     OUT VOID                         *SenseData,  OPTIONAL
     OUT UINT8                        *SenseDataLength
  )
{
  UFS_SCSI_REQUEST_PACKET             Packet;
  UINT8                               Cdb[UFS_SCSI_OP_LENGTH_SIXTEEN];
  EFI_STATUS                          Status;

  ZeroMem (&Packet, sizeof (UFS_SCSI_REQUEST_PACKET));
  ZeroMem (Cdb, sizeof (Cdb));

  Cdb[0] = EFI_SCSI_OP_READ16;
  WriteUnaligned64 ((UINT64 *)&Cdb[2], SwapBytes64 (StartLba));
  WriteUnaligned32 ((UINT32 *)&Cdb[10], SwapBytes32 (SectorNum));

  Packet.Timeout          = UFS_TIMEOUT;
  Packet.Cdb              = Cdb;
  Packet.CdbLength        = sizeof (Cdb);
  Packet.InDataBuffer     = DataBuffer;
  Packet.InTransferLength = *DataLength;
  Packet.DataDirection    = UfsDataIn;
  Packet.SenseData        = SenseData;
  Packet.SenseDataLength  = *SenseDataLength;

  Status = UfsExecScsiCmds (Private, (UINT8)Lun, &Packet);

  if (*SenseDataLength != 0) {
    *SenseDataLength = Packet.SenseDataLength;
  }

  if (!EFI_ERROR (Status)) {
    *DataLength = Packet.InTransferLength;
  }

  return Status;
}

/**
  Parsing Sense Keys from sense data.

  @param  Media              The pointer of EFI_PEI_BLOCK_IO_MEDIA
  @param  SenseData          The pointer of EFI_SCSI_SENSE_DATA
  @param  NeedRetry          The pointer of action which indicates what is need to retry

  @retval EFI_DEVICE_ERROR   Indicates that error occurs
  @retval EFI_SUCCESS        Successfully to complete the parsing

**/
EFI_STATUS
UfsPeimParsingSenseKeys (
  IN     EFI_PEI_BLOCK_IO_MEDIA    *Media,
  IN     EFI_SCSI_SENSE_DATA       *SenseData,
     OUT BOOLEAN                   *NeedRetry
  )
{
  if ((SenseData->Sense_Key == EFI_SCSI_SK_NOT_READY) &&
      (SenseData->Addnl_Sense_Code == EFI_SCSI_ASC_NO_MEDIA)) {
    Media->MediaPresent = FALSE;
    *NeedRetry = FALSE;
    DEBUG ((EFI_D_VERBOSE, "UfsBlockIoPei: Is No Media\n"));
    return EFI_DEVICE_ERROR;
  }

  if ((SenseData->Sense_Key == EFI_SCSI_SK_UNIT_ATTENTION) &&
      (SenseData->Addnl_Sense_Code == EFI_SCSI_ASC_MEDIA_CHANGE)) {
    *NeedRetry = TRUE;
    DEBUG ((EFI_D_VERBOSE, "UfsBlockIoPei: Is Media Change\n"));
    return EFI_SUCCESS;
  }

  if ((SenseData->Sense_Key == EFI_SCSI_SK_UNIT_ATTENTION) &&
      (SenseData->Addnl_Sense_Code == EFI_SCSI_ASC_RESET)) {
    *NeedRetry = TRUE;
    DEBUG ((EFI_D_VERBOSE, "UfsBlockIoPei: Was Reset Before\n"));
    return EFI_SUCCESS;
  }

  if ((SenseData->Sense_Key == EFI_SCSI_SK_MEDIUM_ERROR) ||
      ((SenseData->Sense_Key == EFI_SCSI_SK_NOT_READY) &&
      (SenseData->Addnl_Sense_Code == EFI_SCSI_ASC_MEDIA_UPSIDE_DOWN))) {
    *NeedRetry = FALSE;
    DEBUG ((EFI_D_VERBOSE, "UfsBlockIoPei: Media Error\n"));
    return EFI_DEVICE_ERROR;
  }

  if (SenseData->Sense_Key == EFI_SCSI_SK_HARDWARE_ERROR) {
    *NeedRetry = FALSE;
    DEBUG ((EFI_D_VERBOSE, "UfsBlockIoPei: Hardware Error\n"));
    return EFI_DEVICE_ERROR;
  }

  if ((SenseData->Sense_Key == EFI_SCSI_SK_NOT_READY) &&
      (SenseData->Addnl_Sense_Code == EFI_SCSI_ASC_NOT_READY) &&
      (SenseData->Addnl_Sense_Code_Qualifier == EFI_SCSI_ASCQ_IN_PROGRESS)) {
    *NeedRetry = TRUE;
    DEBUG ((EFI_D_VERBOSE, "UfsBlockIoPei: Was Reset Before\n"));
    return EFI_SUCCESS;
  }

  *NeedRetry = FALSE;
  DEBUG ((EFI_D_VERBOSE, "UfsBlockIoPei: Sense Key = 0x%x ASC = 0x%x!\n", SenseData->Sense_Key, SenseData->Addnl_Sense_Code));
  return EFI_DEVICE_ERROR;
}


/**
  Gets the count of block I/O devices that one specific block driver detects.

  This function is used for getting the count of block I/O devices that one
  specific block driver detects.  To the PEI ATAPI driver, it returns the number
  of all the detected ATAPI devices it detects during the enumeration process.
  To the PEI legacy floppy driver, it returns the number of all the legacy
  devices it finds during its enumeration process. If no device is detected,
  then the function will return zero.

  @param[in]  PeiServices          General-purpose services that are available
                                   to every PEIM.
  @param[in]  This                 Indicates the EFI_PEI_RECOVERY_BLOCK_IO_PPI
                                   instance.
  @param[out] NumberBlockDevices   The number of block I/O devices discovered.

  @retval     EFI_SUCCESS          The operation performed successfully.

**/
EFI_STATUS
EFIAPI
UfsBlockIoPeimGetDeviceNo (
  IN  EFI_PEI_SERVICES               **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO_PPI  *This,
  OUT UINTN                          *NumberBlockDevices
  )
{
  //
  // For Ufs device, it has up to 8 normal Luns plus some well-known Luns.
  // At PEI phase, we will only expose normal Luns to user.
  // For those disabled Lun, when user try to access it, the operation would fail.
  //
  *NumberBlockDevices = UFS_PEIM_MAX_LUNS;
  return EFI_SUCCESS;
}

/**
  Gets a block device's media information.

  This function will provide the caller with the specified block device's media
  information. If the media changes, calling this function will update the media
  information accordingly.

  @param[in]  PeiServices   General-purpose services that are available to every
                            PEIM
  @param[in]  This          Indicates the EFI_PEI_RECOVERY_BLOCK_IO_PPI instance.
  @param[in]  DeviceIndex   Specifies the block device to which the function wants
                            to talk. Because the driver that implements Block I/O
                            PPIs will manage multiple block devices, the PPIs that
                            want to talk to a single device must specify the
                            device index that was assigned during the enumeration
                            process. This index is a number from one to
                            NumberBlockDevices.
  @param[out] MediaInfo     The media information of the specified block media.
                            The caller is responsible for the ownership of this
                            data structure.

  @par Note:
      The MediaInfo structure describes an enumeration of possible block device
      types.  This enumeration exists because no device paths are actually passed
      across interfaces that describe the type or class of hardware that is publishing
      the block I/O interface. This enumeration will allow for policy decisions
      in the Recovery PEIM, such as "Try to recover from legacy floppy first,
      LS-120 second, CD-ROM third." If there are multiple partitions abstracted
      by a given device type, they should be reported in ascending order; this
      order also applies to nested partitions, such as legacy MBR, where the
      outermost partitions would have precedence in the reporting order. The
      same logic applies to systems such as IDE that have precedence relationships
      like "Master/Slave" or "Primary/Secondary". The master device should be
      reported first, the slave second.

  @retval EFI_SUCCESS        Media information about the specified block device
                             was obtained successfully.
  @retval EFI_DEVICE_ERROR   Cannot get the media information due to a hardware
                             error.

**/
EFI_STATUS
EFIAPI
UfsBlockIoPeimGetMediaInfo (
  IN  EFI_PEI_SERVICES               **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO_PPI  *This,
  IN  UINTN                          DeviceIndex,
  OUT EFI_PEI_BLOCK_IO_MEDIA         *MediaInfo
  )
{
  EFI_STATUS                         Status;
  UFS_PEIM_HC_PRIVATE_DATA           *Private;
  EFI_SCSI_SENSE_DATA                SenseData;
  UINT8                              SenseDataLength;
  EFI_SCSI_DISK_CAPACITY_DATA        Capacity;
  EFI_SCSI_DISK_CAPACITY_DATA16      Capacity16;
  UINTN                              DataLength;
  BOOLEAN                            NeedRetry;

  Private   = GET_UFS_PEIM_HC_PRIVATE_DATA_FROM_THIS (This);
  NeedRetry = TRUE;

  if (DeviceIndex >= UFS_PEIM_MAX_LUNS) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Private->Luns.BitMask & (BIT0 << DeviceIndex)) == 0) {
    return EFI_ACCESS_DENIED;
  }

  ZeroMem (&SenseData, sizeof (SenseData));
  ZeroMem (&Capacity, sizeof (Capacity));
  ZeroMem (&Capacity16, sizeof (Capacity16));
  SenseDataLength = sizeof (SenseData);
  //
  // First test unit ready
  //
  do {
    Status = UfsPeimTestUnitReady (
               Private,
               DeviceIndex,
               &SenseData,
               &SenseDataLength
               );
    if (!EFI_ERROR (Status)) {
      break;
    }

    if (SenseDataLength == 0) {
      continue;
    }

    Status = UfsPeimParsingSenseKeys (&(Private->Media[DeviceIndex]), &SenseData, &NeedRetry);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }

  } while (NeedRetry);

  DataLength      = sizeof (EFI_SCSI_DISK_CAPACITY_DATA);
  SenseDataLength = 0;
  Status = UfsPeimReadCapacity (Private, DeviceIndex, &Capacity, (UINT32 *)&DataLength, NULL, &SenseDataLength);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  if ((Capacity.LastLba3 == 0xff) && (Capacity.LastLba2 == 0xff) &&
      (Capacity.LastLba1 == 0xff) && (Capacity.LastLba0 == 0xff)) {
    DataLength      = sizeof (EFI_SCSI_DISK_CAPACITY_DATA16);
    SenseDataLength = 0;
    Status = UfsPeimReadCapacity16 (Private, DeviceIndex, &Capacity16, (UINT32 *)&DataLength, NULL, &SenseDataLength);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }
    MediaInfo->LastBlock  = (Capacity16.LastLba3 << 24) | (Capacity16.LastLba2 << 16) | (Capacity16.LastLba1 << 8) | Capacity16.LastLba0;
    MediaInfo->LastBlock |= ((UINT64)Capacity16.LastLba7 << 56) | ((UINT64)Capacity16.LastLba6 << 48) | ((UINT64)Capacity16.LastLba5 << 40) | ((UINT64)Capacity16.LastLba4 << 32);
    MediaInfo->BlockSize  = (Capacity16.BlockSize3 << 24) | (Capacity16.BlockSize2 << 16) | (Capacity16.BlockSize1 << 8) | Capacity16.BlockSize0;
  } else {
    MediaInfo->LastBlock  = (Capacity.LastLba3 << 24) | (Capacity.LastLba2 << 16) | (Capacity.LastLba1 << 8) | Capacity.LastLba0;
    MediaInfo->BlockSize  = (Capacity.BlockSize3 << 24) | (Capacity.BlockSize2 << 16) | (Capacity.BlockSize1 << 8) | Capacity.BlockSize0;
  }

  MediaInfo->DeviceType   = UfsDevice;
  MediaInfo->MediaPresent = TRUE;

  return EFI_SUCCESS;
}

/**
  Reads the requested number of blocks from the specified block device.

  The function reads the requested number of blocks from the device. All the
  blocks are read, or an error is returned. If there is no media in the device,
  the function returns EFI_NO_MEDIA.

  @param[in]  PeiServices   General-purpose services that are available to
                            every PEIM.
  @param[in]  This          Indicates the EFI_PEI_RECOVERY_BLOCK_IO_PPI instance.
  @param[in]  DeviceIndex   Specifies the block device to which the function wants
                            to talk. Because the driver that implements Block I/O
                            PPIs will manage multiple block devices, PPIs that
                            want to talk to a single device must specify the device
                            index that was assigned during the enumeration process.
                            This index is a number from one to NumberBlockDevices.
  @param[in]  StartLBA      The starting logical block address (LBA) to read from
                            on the device
  @param[in]  BufferSize    The size of the Buffer in bytes. This number must be
                            a multiple of the intrinsic block size of the device.
  @param[out] Buffer        A pointer to the destination buffer for the data.
                            The caller is responsible for the ownership of the
                            buffer.

  @retval EFI_SUCCESS             The data was read correctly from the device.
  @retval EFI_DEVICE_ERROR        The device reported an error while attempting
                                  to perform the read operation.
  @retval EFI_INVALID_PARAMETER   The read request contains LBAs that are not
                                  valid, or the buffer is not properly aligned.
  @retval EFI_NO_MEDIA            There is no media in the device.
  @retval EFI_BAD_BUFFER_SIZE     The BufferSize parameter is not a multiple of
                                  the intrinsic block size of the device.

**/
EFI_STATUS
EFIAPI
UfsBlockIoPeimReadBlocks (
  IN  EFI_PEI_SERVICES               **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO_PPI  *This,
  IN  UINTN                          DeviceIndex,
  IN  EFI_PEI_LBA                    StartLBA,
  IN  UINTN                          BufferSize,
  OUT VOID                           *Buffer
  )
{
  EFI_STATUS                         Status;
  UINTN                              BlockSize;
  UINTN                              NumberOfBlocks;
  UFS_PEIM_HC_PRIVATE_DATA           *Private;
  EFI_SCSI_SENSE_DATA                SenseData;
  UINT8                              SenseDataLength;
  BOOLEAN                            NeedRetry;

  Status    = EFI_SUCCESS;
  NeedRetry = TRUE;
  Private   = GET_UFS_PEIM_HC_PRIVATE_DATA_FROM_THIS (This);

  ZeroMem (&SenseData, sizeof (SenseData));
  SenseDataLength = sizeof (SenseData);

  //
  // Check parameters
  //
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize == 0) {
    return EFI_SUCCESS;
  }

  if (DeviceIndex >= UFS_PEIM_MAX_LUNS) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Private->Luns.BitMask & (BIT0 << DeviceIndex)) == 0) {
    return EFI_ACCESS_DENIED;
  }

  BlockSize = Private->Media[DeviceIndex].BlockSize;

  if (BufferSize % BlockSize != 0) {
    Status = EFI_BAD_BUFFER_SIZE;
  }

  if (StartLBA > Private->Media[DeviceIndex].LastBlock) {
    Status = EFI_INVALID_PARAMETER;
  }

  NumberOfBlocks = BufferSize / BlockSize;

  do {
    Status = UfsPeimTestUnitReady (
               Private,
               DeviceIndex,
               &SenseData,
               &SenseDataLength
               );
    if (!EFI_ERROR (Status)) {
      break;
    }

    if (SenseDataLength == 0) {
      continue;
    }

    Status = UfsPeimParsingSenseKeys (&(Private->Media[DeviceIndex]), &SenseData, &NeedRetry);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }

  } while (NeedRetry);

  SenseDataLength = 0;
  if (Private->Media[DeviceIndex].LastBlock != ~((UINTN)0)) {
    Status = UfsPeimRead10 (
               Private,
               DeviceIndex,
               (UINT32)StartLBA,
               (UINT32)NumberOfBlocks,
               Buffer,
               (UINT32 *)&BufferSize,
               NULL,
               &SenseDataLength
               );
  } else {
    Status = UfsPeimRead16 (
               Private,
               DeviceIndex,
               (UINT32)StartLBA,
               (UINT32)NumberOfBlocks,
               Buffer,
               (UINT32 *)&BufferSize,
               NULL,
               &SenseDataLength
               );
  }
  return Status;
}

/**
  The user code starts with this function.

  @param  FileHandle             Handle of the file being invoked.
  @param  PeiServices            Describes the list of possible PEI Services.

  @retval EFI_SUCCESS            The driver is successfully initialized.
  @retval Others                 Can't initialize the driver.

**/
EFI_STATUS
EFIAPI
InitializeUfsBlockIoPeim (
  IN EFI_PEI_FILE_HANDLE        FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS                    Status;
  UFS_PEIM_HC_PRIVATE_DATA      *Private;
  EDKII_UFS_HOST_CONTROLLER_PPI *UfsHcPpi;
  UINT32                        Index;
  UFS_CONFIG_DESC               Config;
  UINTN                         MmioBase;
  UINT8                         Controller;

  //
  // Shadow this PEIM to run from memory
  //
  if (!EFI_ERROR (PeiServicesRegisterForShadow (FileHandle))) {
    return EFI_SUCCESS;
  }

  //
  // locate ufs host controller PPI
  //
  Status = PeiServicesLocatePpi (
             &gEdkiiPeiUfsHostControllerPpiGuid,
             0,
             NULL,
             (VOID **) &UfsHcPpi
             );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Controller = 0;
  MmioBase   = 0;
  while (TRUE) {
    Status = UfsHcPpi->GetUfsHcMmioBar (UfsHcPpi, Controller, &MmioBase);
    //
    // When status is error, meant no controller is found
    //
    if (EFI_ERROR (Status)) {
      break;
    }

    Private = AllocateCopyPool (sizeof (UFS_PEIM_HC_PRIVATE_DATA), &gUfsHcPeimTemplate);
    if (Private == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      break;
    }

    Private->BlkIoPpiList.Ppi = &Private->BlkIoPpi;
    Private->UfsHcBase        = MmioBase;

    //
    // Initialize the memory pool which will be used in all transactions.
    //
    Status = UfsPeimInitMemPool (Private);
    if (EFI_ERROR (Status)) {
      Status = EFI_OUT_OF_RESOURCES;
      break;
    }

    //
    // Initialize UFS Host Controller H/W.
    //
    Status = UfsControllerInit (Private);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "UfsDevicePei: Host Controller Initialization Error, Status = %r\n", Status));
      Controller++;
      continue;
    }

    //
    // UFS 2.0 spec Section 13.1.3.3:
    // At the end of the UFS Interconnect Layer initialization on both host and device side,
    // the host shall send a NOP OUT UPIU to verify that the device UTP Layer is ready.
    //
    Status = UfsExecNopCmds (Private);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Ufs Sending NOP IN command Error, Status = %r\n", Status));
      Controller++;
      continue;
    }

    //
    // The host enables the device initialization completion by setting fDeviceInit flag.
    //
    Status = UfsSetFlag (Private, UfsFlagDevInit);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Ufs Set fDeviceInit Flag Error, Status = %r\n", Status));
      Controller++;
      continue;
    }

    //
    // Get Ufs Device's Lun Info by reading Configuration Descriptor.
    //
    Status = UfsRwDeviceDesc (Private, TRUE, UfsConfigDesc, 0, 0, &Config, sizeof (UFS_CONFIG_DESC));
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Ufs Get Configuration Descriptor Error, Status = %r\n", Status));
      Controller++;
      continue;
    }

    for (Index = 0; Index < UFS_PEIM_MAX_LUNS; Index++) {
      if (Config.UnitDescConfParams[Index].LunEn != 0) {
        Private->Luns.BitMask |= (BIT0 << Index);
        DEBUG ((EFI_D_INFO, "Ufs %d Lun %d is enabled\n", Controller, Index));
      }
    }

    Status = PeiServicesInstallPpi (&Private->BlkIoPpiList);
    Controller++;
  }

  return EFI_SUCCESS;
}
