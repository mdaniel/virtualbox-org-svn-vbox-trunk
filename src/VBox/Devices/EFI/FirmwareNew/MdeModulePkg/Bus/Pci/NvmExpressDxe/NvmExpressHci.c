/** @file
  NvmExpressDxe driver is used to manage non-volatile memory subsystem which follows
  NVM Express specification.

  Copyright (c) 2013 - 2014, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "NvmExpress.h"

/**
  Read Nvm Express controller capability register.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param  Cap              The buffer used to store capability register content.

  @return EFI_SUCCESS      Successfully read the controller capability register content.
  @return EFI_DEVICE_ERROR Fail to read the controller capability register.

**/
EFI_STATUS
ReadNvmeControllerCapabilities (
  IN NVME_CONTROLLER_PRIVATE_DATA     *Private,
  IN NVME_CAP                         *Cap
  )
{
  EFI_PCI_IO_PROTOCOL   *PciIo;
  EFI_STATUS            Status;
  UINT64                Data;

  PciIo  = Private->PciIo;
  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        NVME_BAR,
                        NVME_CAP_OFFSET,
                        2,
                        &Data
                        );

  if (EFI_ERROR(Status)) {
    return Status;
  }

  WriteUnaligned64 ((UINT64*)Cap, Data);
  return EFI_SUCCESS;
}

/**
  Read Nvm Express controller configuration register.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param  Cc               The buffer used to store configuration register content.

  @return EFI_SUCCESS      Successfully read the controller configuration register content.
  @return EFI_DEVICE_ERROR Fail to read the controller configuration register.

**/
EFI_STATUS
ReadNvmeControllerConfiguration (
  IN NVME_CONTROLLER_PRIVATE_DATA     *Private,
  IN NVME_CC                          *Cc
  )
{
  EFI_PCI_IO_PROTOCOL   *PciIo;
  EFI_STATUS            Status;
  UINT32                Data;

  PciIo  = Private->PciIo;
  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        NVME_BAR,
                        NVME_CC_OFFSET,
                        1,
                        &Data
                        );

  if (EFI_ERROR(Status)) {
    return Status;
  }

  WriteUnaligned32 ((UINT32*)Cc, Data);
  return EFI_SUCCESS;
}

/**
  Write Nvm Express controller configuration register.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param  Cc               The buffer used to store the content to be written into configuration register.

  @return EFI_SUCCESS      Successfully write data into the controller configuration register.
  @return EFI_DEVICE_ERROR Fail to write data into the controller configuration register.

**/
EFI_STATUS
WriteNvmeControllerConfiguration (
  IN NVME_CONTROLLER_PRIVATE_DATA     *Private,
  IN NVME_CC                          *Cc
  )
{
  EFI_PCI_IO_PROTOCOL   *PciIo;
  EFI_STATUS            Status;
  UINT32                Data;

  PciIo  = Private->PciIo;
  Data   = ReadUnaligned32 ((UINT32*)Cc);
  Status = PciIo->Mem.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        NVME_BAR,
                        NVME_CC_OFFSET,
                        1,
                        &Data
                        );

  if (EFI_ERROR(Status)) {
    return Status;
  }

  DEBUG ((EFI_D_INFO, "Cc.En: %d\n", Cc->En));
  DEBUG ((EFI_D_INFO, "Cc.Css: %d\n", Cc->Css));
  DEBUG ((EFI_D_INFO, "Cc.Mps: %d\n", Cc->Mps));
  DEBUG ((EFI_D_INFO, "Cc.Ams: %d\n", Cc->Ams));
  DEBUG ((EFI_D_INFO, "Cc.Shn: %d\n", Cc->Shn));
  DEBUG ((EFI_D_INFO, "Cc.Iosqes: %d\n", Cc->Iosqes));
  DEBUG ((EFI_D_INFO, "Cc.Iocqes: %d\n", Cc->Iocqes));

  return EFI_SUCCESS;
}

/**
  Read Nvm Express controller status register.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param  Csts             The buffer used to store status register content.

  @return EFI_SUCCESS      Successfully read the controller status register content.
  @return EFI_DEVICE_ERROR Fail to read the controller status register.

**/
EFI_STATUS
ReadNvmeControllerStatus (
  IN NVME_CONTROLLER_PRIVATE_DATA     *Private,
  IN NVME_CSTS                        *Csts
  )
{
  EFI_PCI_IO_PROTOCOL   *PciIo;
  EFI_STATUS            Status;
  UINT32                Data;

  PciIo  = Private->PciIo;
  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        NVME_BAR,
                        NVME_CSTS_OFFSET,
                        1,
                        &Data
                        );

  if (EFI_ERROR(Status)) {
    return Status;
  }

  WriteUnaligned32 ((UINT32*)Csts, Data);
  return EFI_SUCCESS;
}

/**
  Read Nvm Express admin queue attributes register.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param  Aqa              The buffer used to store admin queue attributes register content.

  @return EFI_SUCCESS      Successfully read the admin queue attributes register content.
  @return EFI_DEVICE_ERROR Fail to read the admin queue attributes register.

**/
EFI_STATUS
ReadNvmeAdminQueueAttributes (
  IN NVME_CONTROLLER_PRIVATE_DATA     *Private,
  IN NVME_AQA                         *Aqa
  )
{
  EFI_PCI_IO_PROTOCOL   *PciIo;
  EFI_STATUS            Status;
  UINT32                Data;

  PciIo  = Private->PciIo;
  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        NVME_BAR,
                        NVME_AQA_OFFSET,
                        1,
                        &Data
                        );

  if (EFI_ERROR(Status)) {
    return Status;
  }

  WriteUnaligned32 ((UINT32*)Aqa, Data);
  return EFI_SUCCESS;
}

/**
  Write Nvm Express admin queue attributes register.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param  Aqa              The buffer used to store the content to be written into admin queue attributes register.

  @return EFI_SUCCESS      Successfully write data into the admin queue attributes register.
  @return EFI_DEVICE_ERROR Fail to write data into the admin queue attributes register.

**/
EFI_STATUS
WriteNvmeAdminQueueAttributes (
  IN NVME_CONTROLLER_PRIVATE_DATA     *Private,
  IN NVME_AQA                         *Aqa
  )
{
  EFI_PCI_IO_PROTOCOL   *PciIo;
  EFI_STATUS            Status;
  UINT32                Data;

  PciIo  = Private->PciIo;
  Data   = ReadUnaligned32 ((UINT32*)Aqa);
  Status = PciIo->Mem.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        NVME_BAR,
                        NVME_AQA_OFFSET,
                        1,
                        &Data
                        );

  if (EFI_ERROR(Status)) {
    return Status;
  }

  DEBUG ((EFI_D_INFO, "Aqa.Asqs: %d\n", Aqa->Asqs));
  DEBUG ((EFI_D_INFO, "Aqa.Acqs: %d\n", Aqa->Acqs));

  return EFI_SUCCESS;
}

/**
  Read Nvm Express admin submission queue base address register.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param  Asq              The buffer used to store admin submission queue base address register content.

  @return EFI_SUCCESS      Successfully read the admin submission queue base address register content.
  @return EFI_DEVICE_ERROR Fail to read the admin submission queue base address register.

**/
EFI_STATUS
ReadNvmeAdminSubmissionQueueBaseAddress (
  IN NVME_CONTROLLER_PRIVATE_DATA     *Private,
  IN NVME_ASQ                         *Asq
  )
{
  EFI_PCI_IO_PROTOCOL   *PciIo;
  EFI_STATUS            Status;
  UINT64                Data;

  PciIo  = Private->PciIo;
  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        NVME_BAR,
                        NVME_ASQ_OFFSET,
                        2,
                        &Data
                        );

  if (EFI_ERROR(Status)) {
    return Status;
  }

  WriteUnaligned64 ((UINT64*)Asq, Data);
  return EFI_SUCCESS;
}

/**
  Write Nvm Express admin submission queue base address register.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param  Asq              The buffer used to store the content to be written into admin submission queue base address register.

  @return EFI_SUCCESS      Successfully write data into the admin submission queue base address register.
  @return EFI_DEVICE_ERROR Fail to write data into the admin submission queue base address register.

**/
EFI_STATUS
WriteNvmeAdminSubmissionQueueBaseAddress (
  IN NVME_CONTROLLER_PRIVATE_DATA     *Private,
  IN NVME_ASQ                         *Asq
  )
{
  EFI_PCI_IO_PROTOCOL   *PciIo;
  EFI_STATUS            Status;
  UINT64                Data;

  PciIo  = Private->PciIo;
  Data   = ReadUnaligned64 ((UINT64*)Asq);

  Status = PciIo->Mem.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        NVME_BAR,
                        NVME_ASQ_OFFSET,
                        2,
                        &Data
                        );

  if (EFI_ERROR(Status)) {
    return Status;
  }

  DEBUG ((EFI_D_INFO, "Asq.Asqb: %lx\n", Asq->Asqb));

  return EFI_SUCCESS;
}

/**
  Read Nvm Express admin completion queue base address register.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param  Acq              The buffer used to store admin completion queue base address register content.

  @return EFI_SUCCESS      Successfully read the admin completion queue base address register content.
  @return EFI_DEVICE_ERROR Fail to read the admin completion queue base address register.

**/
EFI_STATUS
ReadNvmeAdminCompletionQueueBaseAddress (
  IN NVME_CONTROLLER_PRIVATE_DATA     *Private,
  IN NVME_ACQ                         *Acq
  )
{
  EFI_PCI_IO_PROTOCOL   *PciIo;
  EFI_STATUS            Status;
  UINT64                Data;

  PciIo  = Private->PciIo;

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        NVME_BAR,
                        NVME_ACQ_OFFSET,
                        2,
                        &Data
                        );

  if (EFI_ERROR(Status)) {
    return Status;
  }

  WriteUnaligned64 ((UINT64*)Acq, Data);
  return EFI_SUCCESS;
}

/**
  Write Nvm Express admin completion queue base address register.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param  Acq              The buffer used to store the content to be written into admin completion queue base address register.

  @return EFI_SUCCESS      Successfully write data into the admin completion queue base address register.
  @return EFI_DEVICE_ERROR Fail to write data into the admin completion queue base address register.

**/
EFI_STATUS
WriteNvmeAdminCompletionQueueBaseAddress (
  IN NVME_CONTROLLER_PRIVATE_DATA     *Private,
  IN NVME_ACQ                         *Acq
  )
{
  EFI_PCI_IO_PROTOCOL   *PciIo;
  EFI_STATUS            Status;
  UINT64                Data;

  PciIo  = Private->PciIo;
  Data   = ReadUnaligned64 ((UINT64*)Acq);

  Status = PciIo->Mem.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        NVME_BAR,
                        NVME_ACQ_OFFSET,
                        2,
                        &Data
                        );

  if (EFI_ERROR(Status)) {
    return Status;
  }

  DEBUG ((EFI_D_INFO, "Acq.Acqb: %lxh\n", Acq->Acqb));

  return EFI_SUCCESS;
}

/**
  Disable the Nvm Express controller.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.

  @return EFI_SUCCESS      Successfully disable the controller.
  @return EFI_DEVICE_ERROR Fail to disable the controller.

**/
EFI_STATUS
NvmeDisableController (
  IN NVME_CONTROLLER_PRIVATE_DATA     *Private
  )
{
  NVME_CC                Cc;
  NVME_CSTS              Csts;
  EFI_STATUS             Status;
  UINT32                 Index;
  UINT8                  Timeout;

  //
  // Read Controller Configuration Register.
  //
  Status = ReadNvmeControllerConfiguration (Private, &Cc);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Cc.En = 0;

  //
  // Disable the controller.
  //
  Status = WriteNvmeControllerConfiguration (Private, &Cc);

  if (EFI_ERROR(Status)) {
    return Status;
  }

  //
  // Cap.To specifies max delay time in 500ms increments for Csts.Rdy to transition from 1 to 0 after
  // Cc.Enable transition from 1 to 0. Loop produces a 1 millisecond delay per itteration, up to 500 * Cap.To.
  //
  if (Private->Cap.To == 0) {
    Timeout = 1;
  } else {
    Timeout = Private->Cap.To;
  }

  for(Index = (Timeout * 500); Index != 0; --Index) {
    gBS->Stall(1000);

    //
    // Check if the controller is initialized
    //
    Status = ReadNvmeControllerStatus (Private, &Csts);

    if (EFI_ERROR(Status)) {
      return Status;
    }

    if (Csts.Rdy == 0) {
      break;
    }
  }

  if (Index == 0) {
    Status = EFI_DEVICE_ERROR;
  }

  DEBUG ((EFI_D_INFO, "NVMe controller is disabled with status [%r].\n", Status));
  return Status;
}

/**
  Enable the Nvm Express controller.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.

  @return EFI_SUCCESS      Successfully enable the controller.
  @return EFI_DEVICE_ERROR Fail to enable the controller.
  @return EFI_TIMEOUT      Fail to enable the controller in given time slot.

**/
EFI_STATUS
NvmeEnableController (
  IN NVME_CONTROLLER_PRIVATE_DATA     *Private
  )
{
  NVME_CC                Cc;
  NVME_CSTS              Csts;
  EFI_STATUS             Status;
  UINT32                 Index;
  UINT8                  Timeout;

  //
  // Enable the controller
  //
  ZeroMem (&Cc, sizeof (NVME_CC));
  Cc.En     = 1;
  Cc.Iosqes = 6;
  Cc.Iocqes = 4;
  Status    = WriteNvmeControllerConfiguration (Private, &Cc);

  if (EFI_ERROR(Status)) {
    return Status;
  }

  //
  // Cap.To specifies max delay time in 500ms increments for Csts.Rdy to set after
  // Cc.Enable. Loop produces a 1 millisecond delay per itteration, up to 500 * Cap.To.
  //
  if (Private->Cap.To == 0) {
    Timeout = 1;
  } else {
    Timeout = Private->Cap.To;
  }

  for(Index = (Timeout * 500); Index != 0; --Index) {
    gBS->Stall(1000);

    //
    // Check if the controller is initialized
    //
    Status = ReadNvmeControllerStatus (Private, &Csts);

    if (EFI_ERROR(Status)) {
      return Status;
    }

    if (Csts.Rdy) {
      break;
    }
  }

  if (Index == 0) {
    Status = EFI_TIMEOUT;
  }

  DEBUG ((EFI_D_INFO, "NVMe controller is enabled with status [%r].\n", Status));
  return Status;
}

/**
  Get identify controller data.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param  Buffer           The buffer used to store the identify controller data.

  @return EFI_SUCCESS      Successfully get the identify controller data.
  @return EFI_DEVICE_ERROR Fail to get the identify controller data.

**/
EFI_STATUS
NvmeIdentifyController (
  IN NVME_CONTROLLER_PRIVATE_DATA       *Private,
  IN VOID                               *Buffer
  )
{
  NVM_EXPRESS_PASS_THRU_COMMAND_PACKET     CommandPacket;
  NVM_EXPRESS_COMMAND                      Command;
  NVM_EXPRESS_RESPONSE                     Response;
  EFI_STATUS                               Status;

  ZeroMem (&CommandPacket, sizeof(NVM_EXPRESS_PASS_THRU_COMMAND_PACKET));
  ZeroMem (&Command, sizeof(NVM_EXPRESS_COMMAND));
  ZeroMem (&Response, sizeof(NVM_EXPRESS_RESPONSE));

  Command.Cdw0.Opcode = NVME_ADMIN_IDENTIFY_OPC;
  Command.Cdw0.Cid    = Private->Cid[0]++;
  //
  // According to Nvm Express 1.1 spec Figure 38, When not used, the field shall be cleared to 0h.
  // For the Identify command, the Namespace Identifier is only used for the Namespace data structure.
  //
  Command.Nsid        = 0;

  CommandPacket.NvmeCmd        = &Command;
  CommandPacket.NvmeResponse   = &Response;
  CommandPacket.TransferBuffer = Buffer;
  CommandPacket.TransferLength = sizeof (NVME_ADMIN_CONTROLLER_DATA);
  CommandPacket.CommandTimeout = NVME_GENERIC_TIMEOUT;
  CommandPacket.QueueId        = NVME_ADMIN_QUEUE;
  //
  // Set bit 0 (Cns bit) to 1 to identify a controller
  //
  Command.Cdw10                = 1;
  Command.Flags                = CDW10_VALID;

  Status = Private->Passthru.PassThru (
                               &Private->Passthru,
                               NVME_CONTROLLER_ID,
                               0,
                               &CommandPacket,
                               NULL
                               );

  return Status;
}

/**
  Get specified identify namespace data.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.
  @param  NamespaceId      The specified namespace identifier.
  @param  Buffer           The buffer used to store the identify namespace data.

  @return EFI_SUCCESS      Successfully get the identify namespace data.
  @return EFI_DEVICE_ERROR Fail to get the identify namespace data.

**/
EFI_STATUS
NvmeIdentifyNamespace (
  IN NVME_CONTROLLER_PRIVATE_DATA      *Private,
  IN UINT32                            NamespaceId,
  IN VOID                              *Buffer
  )
{
  NVM_EXPRESS_PASS_THRU_COMMAND_PACKET     CommandPacket;
  NVM_EXPRESS_COMMAND                      Command;
  NVM_EXPRESS_RESPONSE                     Response;
  EFI_STATUS                               Status;

  ZeroMem (&CommandPacket, sizeof(NVM_EXPRESS_PASS_THRU_COMMAND_PACKET));
  ZeroMem (&Command, sizeof(NVM_EXPRESS_COMMAND));
  ZeroMem (&Response, sizeof(NVM_EXPRESS_RESPONSE));

  CommandPacket.NvmeCmd      = &Command;
  CommandPacket.NvmeResponse = &Response;

  Command.Cdw0.Opcode = NVME_ADMIN_IDENTIFY_OPC;
  Command.Cdw0.Cid    = Private->Cid[0]++;
  Command.Nsid        = NamespaceId;
  CommandPacket.TransferBuffer = Buffer;
  CommandPacket.TransferLength = sizeof (NVME_ADMIN_NAMESPACE_DATA);
  CommandPacket.CommandTimeout = NVME_GENERIC_TIMEOUT;
  CommandPacket.QueueId        = NVME_ADMIN_QUEUE;
  //
  // Set bit 0 (Cns bit) to 1 to identify a namespace
  //
  CommandPacket.NvmeCmd->Cdw10 = 0;
  CommandPacket.NvmeCmd->Flags = CDW10_VALID;

  Status = Private->Passthru.PassThru (
                               &Private->Passthru,
                               NamespaceId,
                               0,
                               &CommandPacket,
                               NULL
                               );

  return Status;
}

/**
  Create io completion queue.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.

  @return EFI_SUCCESS      Successfully create io completion queue.
  @return EFI_DEVICE_ERROR Fail to create io completion queue.

**/
EFI_STATUS
NvmeCreateIoCompletionQueue (
  IN NVME_CONTROLLER_PRIVATE_DATA      *Private
  )
{
  NVM_EXPRESS_PASS_THRU_COMMAND_PACKET     CommandPacket;
  NVM_EXPRESS_COMMAND                      Command;
  NVM_EXPRESS_RESPONSE                     Response;
  EFI_STATUS                               Status;
  NVME_ADMIN_CRIOCQ                        CrIoCq;

  ZeroMem (&CommandPacket, sizeof(NVM_EXPRESS_PASS_THRU_COMMAND_PACKET));
  ZeroMem (&Command, sizeof(NVM_EXPRESS_COMMAND));
  ZeroMem (&Response, sizeof(NVM_EXPRESS_RESPONSE));
  ZeroMem (&CrIoCq, sizeof(NVME_ADMIN_CRIOCQ));

  CommandPacket.NvmeCmd      = &Command;
  CommandPacket.NvmeResponse = &Response;

  Command.Cdw0.Opcode = NVME_ADMIN_CRIOCQ_OPC;
  Command.Cdw0.Cid    = Private->Cid[0]++;
  CommandPacket.TransferBuffer = Private->CqBufferPciAddr[1];
  CommandPacket.TransferLength = EFI_PAGE_SIZE;
  CommandPacket.CommandTimeout = NVME_GENERIC_TIMEOUT;
  CommandPacket.QueueId        = NVME_ADMIN_QUEUE;

  CrIoCq.Qid   = NVME_IO_QUEUE;
  CrIoCq.Qsize = NVME_CCQ_SIZE;
  CrIoCq.Pc    = 1;
  CopyMem (&CommandPacket.NvmeCmd->Cdw10, &CrIoCq, sizeof (NVME_ADMIN_CRIOCQ));
  CommandPacket.NvmeCmd->Flags = CDW10_VALID | CDW11_VALID;

  Status = Private->Passthru.PassThru (
                               &Private->Passthru,
                               0,
                               0,
                               &CommandPacket,
                               NULL
                               );

  return Status;
}

/**
  Create io submission queue.

  @param  Private          The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.

  @return EFI_SUCCESS      Successfully create io submission queue.
  @return EFI_DEVICE_ERROR Fail to create io submission queue.

**/
EFI_STATUS
NvmeCreateIoSubmissionQueue (
  IN NVME_CONTROLLER_PRIVATE_DATA      *Private
  )
{
  NVM_EXPRESS_PASS_THRU_COMMAND_PACKET     CommandPacket;
  NVM_EXPRESS_COMMAND                      Command;
  NVM_EXPRESS_RESPONSE                     Response;
  EFI_STATUS                               Status;
  NVME_ADMIN_CRIOSQ                        CrIoSq;

  ZeroMem (&CommandPacket, sizeof(NVM_EXPRESS_PASS_THRU_COMMAND_PACKET));
  ZeroMem (&Command, sizeof(NVM_EXPRESS_COMMAND));
  ZeroMem (&Response, sizeof(NVM_EXPRESS_RESPONSE));
  ZeroMem (&CrIoSq, sizeof(NVME_ADMIN_CRIOSQ));

  CommandPacket.NvmeCmd      = &Command;
  CommandPacket.NvmeResponse = &Response;

  Command.Cdw0.Opcode = NVME_ADMIN_CRIOSQ_OPC;
  Command.Cdw0.Cid    = Private->Cid[0]++;
  CommandPacket.TransferBuffer = Private->SqBufferPciAddr[1];
  CommandPacket.TransferLength = EFI_PAGE_SIZE;
  CommandPacket.CommandTimeout = NVME_GENERIC_TIMEOUT;
  CommandPacket.QueueId        = NVME_ADMIN_QUEUE;

  CrIoSq.Qid   = NVME_IO_QUEUE;
  CrIoSq.Qsize = NVME_CSQ_SIZE;
  CrIoSq.Pc    = 1;
  CrIoSq.Cqid  = NVME_IO_QUEUE;
  CrIoSq.Qprio = 0;
  CopyMem (&CommandPacket.NvmeCmd->Cdw10, &CrIoSq, sizeof (NVME_ADMIN_CRIOSQ));
  CommandPacket.NvmeCmd->Flags = CDW10_VALID | CDW11_VALID;

  Status = Private->Passthru.PassThru (
                               &Private->Passthru,
                               0,
                               0,
                               &CommandPacket,
                               NULL
                               );

  return Status;
}

/**
  Initialize the Nvm Express controller.

  @param[in] Private                 The pointer to the NVME_CONTROLLER_PRIVATE_DATA data structure.

  @retval EFI_SUCCESS                The NVM Express Controller is initialized successfully.
  @retval Others                     A device error occurred while initializing the controller.

**/
EFI_STATUS
NvmeControllerInit (
  IN NVME_CONTROLLER_PRIVATE_DATA    *Private
  )
{
  EFI_STATUS                      Status;
  EFI_PCI_IO_PROTOCOL             *PciIo;
  UINT64                          Supports;
  NVME_AQA                        Aqa;
  NVME_ASQ                        Asq;
  NVME_ACQ                        Acq;

  //
  // Save original PCI attributes and enable this controller.
  //
  PciIo  = Private->PciIo;
  Status = PciIo->Attributes (
                    PciIo,
                    EfiPciIoAttributeOperationGet,
                    0,
                    &Private->PciAttributes
                    );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PciIo->Attributes (
                    PciIo,
                    EfiPciIoAttributeOperationSupported,
                    0,
                    &Supports
                    );

  if (!EFI_ERROR (Status)) {
    Supports &= (UINT64)EFI_PCI_DEVICE_ENABLE;
    Status    = PciIo->Attributes (
                         PciIo,
                         EfiPciIoAttributeOperationEnable,
                         Supports,
                         NULL
                         );
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_INFO, "NvmeControllerInit: failed to enable controller\n"));
    return Status;
  }

  //
  // Read the Controller Capabilities register and verify that the NVM command set is supported
  //
  Status = ReadNvmeControllerCapabilities (Private, &Private->Cap);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Private->Cap.Css != 0x01) {
    DEBUG ((EFI_D_INFO, "NvmeControllerInit: the controller doesn't support NVMe command set\n"));
    return EFI_UNSUPPORTED;
  }

  //
  // Currently the driver only supports 4k page size.
  //
  ASSERT ((Private->Cap.Mpsmin + 12) <= EFI_PAGE_SHIFT);

  Private->Cid[0] = 0;
  Private->Cid[1] = 0;

  Status = NvmeDisableController (Private);

  if (EFI_ERROR(Status)) {
    return Status;
  }

  //
  // set number of entries admin submission & completion queues.
  //
  Aqa.Asqs  = NVME_ASQ_SIZE;
  Aqa.Rsvd1 = 0;
  Aqa.Acqs  = NVME_ACQ_SIZE;
  Aqa.Rsvd2 = 0;

  //
  // Address of admin submission queue.
  //
  Asq.Rsvd1 = 0;
  Asq.Asqb  = (UINT64)(UINTN)(Private->BufferPciAddr) >> 12;

  //
  // Address of admin completion queue.
  //
  Acq.Rsvd1 = 0;
  Acq.Acqb  = (UINT64)(UINTN)(Private->BufferPciAddr + EFI_PAGE_SIZE) >> 12;

  //
  // Address of I/O submission & completion queue.
  //
  Private->SqBuffer[0]        = (NVME_SQ *)(UINTN)(Private->Buffer);
  Private->SqBufferPciAddr[0] = (NVME_SQ *)(UINTN)(Private->BufferPciAddr);
  Private->CqBuffer[0]        = (NVME_CQ *)(UINTN)(Private->Buffer + 1 * EFI_PAGE_SIZE);
  Private->CqBufferPciAddr[0] = (NVME_CQ *)(UINTN)(Private->BufferPciAddr + 1 * EFI_PAGE_SIZE);
  Private->SqBuffer[1]        = (NVME_SQ *)(UINTN)(Private->Buffer + 2 * EFI_PAGE_SIZE);
  Private->SqBufferPciAddr[1] = (NVME_SQ *)(UINTN)(Private->BufferPciAddr + 2 * EFI_PAGE_SIZE);
  Private->CqBuffer[1]        = (NVME_CQ *)(UINTN)(Private->Buffer + 3 * EFI_PAGE_SIZE);
  Private->CqBufferPciAddr[1] = (NVME_CQ *)(UINTN)(Private->BufferPciAddr + 3 * EFI_PAGE_SIZE);

  DEBUG ((EFI_D_INFO, "Private->Buffer = [%016X]\n", (UINT64)(UINTN)Private->Buffer));
  DEBUG ((EFI_D_INFO, "Admin Submission Queue size (Aqa.Asqs) = [%08X]\n", Aqa.Asqs));
  DEBUG ((EFI_D_INFO, "Admin Completion Queue size (Aqa.Acqs) = [%08X]\n", Aqa.Acqs));
  DEBUG ((EFI_D_INFO, "Admin Submission Queue (SqBuffer[0]) = [%016X]\n", Private->SqBuffer[0]));
  DEBUG ((EFI_D_INFO, "Admin Completion Queue (CqBuffer[0]) = [%016X]\n", Private->CqBuffer[0]));
  DEBUG ((EFI_D_INFO, "I/O   Submission Queue (SqBuffer[1]) = [%016X]\n", Private->SqBuffer[1]));
  DEBUG ((EFI_D_INFO, "I/O   Completion Queue (CqBuffer[1]) = [%016X]\n", Private->CqBuffer[1]));

  //
  // Program admin queue attributes.
  //
  Status = WriteNvmeAdminQueueAttributes (Private, &Aqa);

  if (EFI_ERROR(Status)) {
    return Status;
  }

  //
  // Program admin submission queue address.
  //
  Status = WriteNvmeAdminSubmissionQueueBaseAddress (Private, &Asq);

  if (EFI_ERROR(Status)) {
    return Status;
  }

  //
  // Program admin completion queue address.
  //
  Status = WriteNvmeAdminCompletionQueueBaseAddress (Private, &Acq);

  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = NvmeEnableController (Private);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  //
  // Create one I/O completion queue.
  //
  Status = NvmeCreateIoCompletionQueue (Private);
  if (EFI_ERROR(Status)) {
   return Status;
  }

  //
  // Create one I/O Submission queue.
  //
  Status = NvmeCreateIoSubmissionQueue (Private);
  if (EFI_ERROR(Status)) {
   return Status;
  }

  //
  // Allocate buffer for Identify Controller data
  //
  Private->ControllerData = (NVME_ADMIN_CONTROLLER_DATA *)AllocateZeroPool (sizeof(NVME_ADMIN_CONTROLLER_DATA));

  if (Private->ControllerData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Get current Identify Controller Data
  //
  Status = NvmeIdentifyController (Private, Private->ControllerData);

  if (EFI_ERROR(Status)) {
    FreePool(Private->ControllerData);
    Private->ControllerData = NULL;
    return EFI_NOT_FOUND;
  }

  //
  // Dump NvmExpress Identify Controller Data
  //
  Private->ControllerData->Sn[19] = 0;
  Private->ControllerData->Mn[39] = 0;
  DEBUG ((EFI_D_INFO, " == NVME IDENTIFY CONTROLLER DATA ==\n"));
  DEBUG ((EFI_D_INFO, "    PCI VID   : 0x%x\n", Private->ControllerData->Vid));
  DEBUG ((EFI_D_INFO, "    PCI SSVID : 0x%x\n", Private->ControllerData->Ssvid));
  DEBUG ((EFI_D_INFO, "    SN        : %a\n",   (CHAR8 *)(Private->ControllerData->Sn)));
  DEBUG ((EFI_D_INFO, "    MN        : %a\n",   (CHAR8 *)(Private->ControllerData->Mn)));
  DEBUG ((EFI_D_INFO, "    FR        : 0x%x\n", *((UINT64*)Private->ControllerData->Fr)));
  DEBUG ((EFI_D_INFO, "    RAB       : 0x%x\n", Private->ControllerData->Rab));
  DEBUG ((EFI_D_INFO, "    IEEE      : 0x%x\n", *(UINT32*)Private->ControllerData->Ieee_oui));
  DEBUG ((EFI_D_INFO, "    AERL      : 0x%x\n", Private->ControllerData->Aerl));
  DEBUG ((EFI_D_INFO, "    SQES      : 0x%x\n", Private->ControllerData->Sqes));
  DEBUG ((EFI_D_INFO, "    CQES      : 0x%x\n", Private->ControllerData->Cqes));
  DEBUG ((EFI_D_INFO, "    NN        : 0x%x\n", Private->ControllerData->Nn));

  return Status;
}

