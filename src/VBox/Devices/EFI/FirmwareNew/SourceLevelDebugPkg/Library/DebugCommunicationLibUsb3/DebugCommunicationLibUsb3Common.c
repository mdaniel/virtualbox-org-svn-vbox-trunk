/** @file
  Debug Port Library implementation based on usb3 debug port.

  Copyright (c) 2014 - 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "DebugCommunicationLibUsb3Internal.h"

//
// The global variable which can be used after memory is ready.
//
USB3_DEBUG_PORT_HANDLE     mDebugCommunicationLibUsb3DebugPortHandle;

UINT16   mString0Desc[] = {
  //  String Descriptor Type + Length
  ( USB_DESC_TYPE_STRING << 8 ) + STRING0_DESC_LEN,
  0x0409
};

UINT16   mManufacturerStrDesc[] = {
  //  String Descriptor Type + Length
  ( USB_DESC_TYPE_STRING << 8 ) + MANU_DESC_LEN,
  'I', 'n', 't', 'e', 'l'
};

UINT16   mProductStrDesc[] = {
  //  String Descriptor Type + Length
  ( USB_DESC_TYPE_STRING << 8 ) +  PRODUCT_DESC_LEN,
  'U', 'S', 'B', ' ', '3', '.', '0', ' ', 'D', 'e', 'b', 'u', 'g', ' ', 'C', 'a', 'b', 'l', 'e'
};

UINT16   mSerialNumberStrDesc[] = {
  //  String Descriptor Type + Length
  ( USB_DESC_TYPE_STRING << 8 ) +  SERIAL_DESC_LEN,
  '1'
};

/**
  Sets bits as per the enabled bit positions in the mask.

  @param[in, out] Register    UINTN register
  @param[in]      BitMask     32-bit mask
**/
VOID
XhcSetR32Bit(
  IN OUT  UINTN  Register,
  IN      UINT32 BitMask
  )
{
  UINT32    RegisterValue;

  RegisterValue = MmioRead32 (Register);
  RegisterValue |= (UINT32)(BitMask);
  MmioWrite32 (Register, RegisterValue);
}

/**
  Clears bits as per the enabled bit positions in the mask.

  @param[in, out] Register    UINTN register
  @param[in]      BitMask     32-bit mask
**/
VOID
XhcClearR32Bit(
  IN OUT  UINTN  Register,
  IN      UINT32 BitMask
  )
{
  UINT32    RegisterValue;

  RegisterValue = MmioRead32 (Register);
  RegisterValue &= ~BitMask;
  MmioWrite32 (Register, RegisterValue);
}

/**
  Write the data to the XHCI debug register.

  @param  Handle       Debug port handle.
  @param  Offset       The offset of the runtime register.
  @param  Data         The data to write.

**/
VOID
XhcWriteDebugReg (
  IN USB3_DEBUG_PORT_HANDLE  *Handle,
  IN UINT32                   Offset,
  IN UINT32                   Data
  )
{
  EFI_PHYSICAL_ADDRESS  DebugCapabilityBase;

  DebugCapabilityBase = Handle->DebugCapabilityBase;
  MmioWrite32 ((UINTN)(DebugCapabilityBase + Offset), Data);

  return;
}

/**
  Read XHCI debug register.

  @param  Handle       Debug port handle.
  @param  Offset       The offset of the runtime register.

  @return The register content read

**/
UINT32
XhcReadDebugReg (
  IN  USB3_DEBUG_PORT_HANDLE *Handle,
  IN  UINT32                   Offset
  )
{
  UINT32                  Data;
  EFI_PHYSICAL_ADDRESS    DebugCapabilityBase;

  DebugCapabilityBase = Handle->DebugCapabilityBase;
  Data = MmioRead32 ((UINTN)(DebugCapabilityBase + Offset));

  return Data;
}

/**
  Set one bit of the runtime register while keeping other bits.

  @param  Handle       Debug port handle.
  @param  Offset       The offset of the runtime register.
  @param  Bit          The bit mask of the register to set.

**/
VOID
XhcSetDebugRegBit (
  IN USB3_DEBUG_PORT_HANDLE *Handle,
  IN UINT32                   Offset,
  IN UINT32                   Bit
  )
{
  UINT32                  Data;

  Data  = XhcReadDebugReg (Handle, Offset);
  Data |= Bit;
  XhcWriteDebugReg (Handle, Offset, Data);
}

/**
  Program and eanble XHCI MMIO base address.

  @return XHCI MMIO base address.

**/
EFI_PHYSICAL_ADDRESS
ProgramXhciBaseAddress (
  VOID
  )
{
  UINT16                      PciCmd;
  UINT32                      Low;
  UINT32                      High;
  EFI_PHYSICAL_ADDRESS        XhciMmioBase;

  Low = PciRead32 (PcdGet32(PcdUsbXhciPciAddress) + PCI_BASE_ADDRESSREG_OFFSET);
  High = PciRead32 (PcdGet32(PcdUsbXhciPciAddress) + PCI_BASE_ADDRESSREG_OFFSET + 4);
  XhciMmioBase = (EFI_PHYSICAL_ADDRESS) (LShiftU64 ((UINT64) High, 32) | Low);
  XhciMmioBase &= XHCI_BASE_ADDRESS_64_BIT_MASK;

  if ((XhciMmioBase == 0) || (XhciMmioBase == XHCI_BASE_ADDRESS_64_BIT_MASK)) {
    XhciMmioBase = PcdGet64(PcdUsbXhciMemorySpaceBase);
    PciWrite32(PcdGet32(PcdUsbXhciPciAddress) + PCI_BASE_ADDRESSREG_OFFSET, XhciMmioBase & 0xFFFFFFFF);
    PciWrite32(PcdGet32(PcdUsbXhciPciAddress) + PCI_BASE_ADDRESSREG_OFFSET + 4, (RShiftU64 (XhciMmioBase, 32) & 0xFFFFFFFF));
  }

  PciCmd = PciRead16 (PcdGet32(PcdUsbXhciPciAddress) + PCI_COMMAND_OFFSET);
  if (((PciCmd & EFI_PCI_COMMAND_MEMORY_SPACE) == 0) || ((PciCmd & EFI_PCI_COMMAND_BUS_MASTER) == 0)) {
    PciCmd |= EFI_PCI_COMMAND_MEMORY_SPACE | EFI_PCI_COMMAND_BUS_MASTER;
    PciWrite16(PcdGet32(PcdUsbXhciPciAddress) + PCI_COMMAND_OFFSET, PciCmd);
  }

  return XhciMmioBase;
}

/**
  Check if the timer is timeout.

  @param[in] UsbDebugPortHandle  Pointer to USB Debug port handle
  @param[in] Timer               The start timer from the begin.
  @param[in] TimeoutTicker       Ticker number need time out.

  @return TRUE  Timer time out occurs.
  @retval FALSE Timer does not time out.

**/
BOOLEAN
IsTimerTimeout (
  IN USB3_DEBUG_PORT_HANDLE  *UsbDebugPortHandle,
  IN UINT64                  Timer,
  IN UINT64                  TimeoutTicker
  )
{
  UINT64  CurrentTimer;
  UINT64  Delta;

  CurrentTimer = GetPerformanceCounter ();

  if (UsbDebugPortHandle->TimerCountDown) {
    //
    // The timer counter counts down.  Check for roll over condition.
    //
    if (CurrentTimer < Timer) {
      Delta = Timer - CurrentTimer;
    } else {
      //
      // Handle one roll-over.
      //
      Delta = UsbDebugPortHandle->TimerCycle - (CurrentTimer - Timer);
    }
  } else {
    //
    // The timer counter counts up.  Check for roll over condition.
    //
    if (CurrentTimer > Timer) {
      Delta = CurrentTimer - Timer;
    } else {
      //
      // Handle one roll-over.
      //
      Delta = UsbDebugPortHandle->TimerCycle - (Timer - CurrentTimer);
    }
  }

  return (BOOLEAN) (Delta >= TimeoutTicker);
}

/**
  Update XHC MMIO base address when MMIO base address is changed.

  @param  Handle          Debug port handle.
  @param  XhciMmioBase    XHCI MMIO base address.

**/
VOID
UpdateXhcResource (
  IN OUT USB3_DEBUG_PORT_HANDLE            *Handle,
  IN EFI_PHYSICAL_ADDRESS                   XhciMmioBase
  )
{
  if ((Handle == NULL) || (Handle->XhciMmioBase == XhciMmioBase)) {
    return;
  }

  //
  // Need fix Handle data according to new XHCI MMIO base address.
  //
  Handle->XhciMmioBase        = XhciMmioBase;
  Handle->DebugCapabilityBase = XhciMmioBase + Handle->DebugCapabilityOffset;
  Handle->XhciOpRegister      = XhciMmioBase + MmioRead8 ((UINTN)XhciMmioBase);
}

/**
  Calculate the usb debug port bar address.

  @param  Handle             Debug port handle.

  @retval RETURN_UNSUPPORTED The usb host controller does not supported usb debug port capability.
  @retval RETURN_SUCCESS     Get bar and offset successfully.

**/
RETURN_STATUS
EFIAPI
CalculateUsbDebugPortMmioBase (
  USB3_DEBUG_PORT_HANDLE          *Handle
 )
{
  UINT16                          VendorId;
  UINT16                          DeviceId;
  UINT8                           ProgInterface;
  UINT8                           SubClassCode;
  UINT8                           BaseCode;
  BOOLEAN                         Flag;
  UINT32                          Capability;
  EFI_PHYSICAL_ADDRESS            CapabilityPointer;
  UINT8                           CapLength;

  VendorId = PciRead16 (PcdGet32(PcdUsbXhciPciAddress) + PCI_VENDOR_ID_OFFSET);
  DeviceId = PciRead16 (PcdGet32(PcdUsbXhciPciAddress) + PCI_DEVICE_ID_OFFSET);

  if ((VendorId == 0xFFFF) || (DeviceId == 0xFFFF)) {
    goto Done;
  }

  ProgInterface = PciRead8 (PcdGet32(PcdUsbXhciPciAddress) + PCI_CLASSCODE_OFFSET);
  SubClassCode  = PciRead8 (PcdGet32(PcdUsbXhciPciAddress) + PCI_CLASSCODE_OFFSET + 1);
  BaseCode      = PciRead8 (PcdGet32(PcdUsbXhciPciAddress) + PCI_CLASSCODE_OFFSET + 2);

  if ((ProgInterface != PCI_IF_XHCI) || (SubClassCode != PCI_CLASS_SERIAL_USB) || (BaseCode != PCI_CLASS_SERIAL)) {
    goto Done;
  }

  CapLength = MmioRead8 ((UINTN) Handle->XhciMmioBase);

  //
  // Get capability pointer from HCCPARAMS at offset 0x10
  //
  CapabilityPointer = Handle->XhciMmioBase + (MmioRead32 ((UINTN)(Handle->XhciMmioBase + XHC_HCCPARAMS_OFFSET)) >> 16) * 4;

  //
  // Search XHCI debug capability
  //
  Flag = FALSE;
  Capability = MmioRead32 ((UINTN)CapabilityPointer);
  while (TRUE) {
    if ((Capability & XHC_CAPABILITY_ID_MASK) == PCI_CAPABILITY_ID_DEBUG_PORT) {
      Flag = TRUE;
      break;
    }
    if ((((Capability & XHC_NEXT_CAPABILITY_MASK) >> 8) & XHC_CAPABILITY_ID_MASK) == 0) {
      //
      // Reach the end of capability list, quit
      //
      break;
    }
    CapabilityPointer += ((Capability & XHC_NEXT_CAPABILITY_MASK) >> 8) * 4;
    Capability = MmioRead32 ((UINTN)CapabilityPointer);
  }

  if (!Flag) {
    goto Done;
  }

  //
  // USB3 debug capability is supported.
  //
  Handle->DebugCapabilityBase   = CapabilityPointer;
  Handle->DebugCapabilityOffset = CapabilityPointer - Handle->XhciMmioBase;
  Handle->XhciOpRegister        = Handle->XhciMmioBase + CapLength;
  Handle->Initialized = USB3DBG_DBG_CAB;
  return RETURN_SUCCESS;

Done:
  Handle->Initialized = USB3DBG_NO_DBG_CAB;
  return RETURN_UNSUPPORTED;
}

/**
  Check if it needs to re-initialize usb debug port hardware.

  During different phases switch, such as SEC to PEI or PEI to DXE or DXE to SMM, we should check
  whether the usb debug port hardware configuration is changed. Such case can be triggerred by
  Pci bus resource allocation and so on.

  @param  Handle           Debug port handle.

  @retval TRUE             The usb debug port hardware configuration is changed.
  @retval FALSE            The usb debug port hardware configuration is not changed.

**/
BOOLEAN
EFIAPI
NeedReinitializeHardware(
  IN USB3_DEBUG_PORT_HANDLE *Handle
  )
{
  BOOLEAN                 Result;
  volatile UINT32         Dcctrl;

  Result = FALSE;

  //
  // If DCE bit, it means USB3 debug is not enabled.
  //
  Dcctrl = XhcReadDebugReg (Handle, XHC_DC_DCCTRL);
  if ((Dcctrl & BIT0) == 0) {
    Result = TRUE;
  }

  return Result;
}

/**
  Create XHCI event ring.

  @param  Handle              Debug port handle.
  @param  EventRing           The created event ring.

**/
EFI_STATUS
CreateEventRing (
  IN  USB3_DEBUG_PORT_HANDLE     *Handle,
  OUT EVENT_RING                 *EventRing
  )
{
  VOID                        *Buf;
  EVENT_RING_SEG_TABLE_ENTRY  *ERSTBase;

  ASSERT (EventRing != NULL);

  //
  // Allocate Event Ring
  //
  Buf = AllocateAlignBuffer (sizeof (TRB_TEMPLATE) * EVENT_RING_TRB_NUMBER);
  ASSERT (Buf != NULL);
  ASSERT (((UINTN) Buf & 0x3F) == 0);
  ZeroMem (Buf, sizeof (TRB_TEMPLATE) * EVENT_RING_TRB_NUMBER);

  EventRing->EventRingSeg0    = (EFI_PHYSICAL_ADDRESS)(UINTN) Buf;
  EventRing->TrbNumber        = EVENT_RING_TRB_NUMBER;
  EventRing->EventRingDequeue = (EFI_PHYSICAL_ADDRESS)(UINTN) EventRing->EventRingSeg0;
  EventRing->EventRingEnqueue = (EFI_PHYSICAL_ADDRESS)(UINTN) EventRing->EventRingSeg0;

  //
  // Software maintains an Event Ring Consumer Cycle State (CCS) bit, initializing it to '1'
  // and toggling it every time the Event Ring Dequeue Pointer wraps back to the beginning of the Event Ring.
  //
  EventRing->EventRingCCS = 1;

  //
  // Allocate Event Ring Segment Table Entry 0 in Event Ring Segment Table
  //
  Buf = AllocateAlignBuffer (sizeof (EVENT_RING_SEG_TABLE_ENTRY) * ERST_NUMBER);
  ASSERT (Buf != NULL);
  ASSERT (((UINTN) Buf & 0x3F) == 0);
  ZeroMem (Buf, sizeof (EVENT_RING_SEG_TABLE_ENTRY) * ERST_NUMBER);

  ERSTBase              = (EVENT_RING_SEG_TABLE_ENTRY *) Buf;
  EventRing->ERSTBase   = (EFI_PHYSICAL_ADDRESS)(UINTN) ERSTBase;

  //
  // Fill Event Segment address
  //
  ERSTBase->PtrLo       = XHC_LOW_32BIT (EventRing->EventRingSeg0);
  ERSTBase->PtrHi       = XHC_HIGH_32BIT (EventRing->EventRingSeg0);
  ERSTBase->RingTrbSize = EVENT_RING_TRB_NUMBER;

  //
  // Program the Interrupter Event Ring Dequeue Pointer (DCERDP) register (7.6.4.1)
  //
  XhcWriteDebugReg (
    Handle,
    XHC_DC_DCERDP,
    XHC_LOW_32BIT((UINT64)(UINTN)EventRing->EventRingDequeue)
    );

  XhcWriteDebugReg (
    Handle,
    XHC_DC_DCERDP + 4,
    XHC_HIGH_32BIT((UINT64)(UINTN)EventRing->EventRingDequeue)
    );

  //
  // Program the Debug Capability Event Ring Segment Table Base Address (DCERSTBA) register(7.6.4.1)
  //
  XhcWriteDebugReg (
    Handle,
    XHC_DC_DCERSTBA,
    XHC_LOW_32BIT((UINT64)(UINTN)ERSTBase)
    );

  XhcWriteDebugReg (
    Handle,
    XHC_DC_DCERSTBA + 4,
    XHC_HIGH_32BIT((UINT64)(UINTN)ERSTBase)
    );

  //
  // Program the Debug Capability Event Ring Segment Table Size (DCERSTSZ) register(7.6.4.1)
  //
  XhcWriteDebugReg (
    Handle,
    XHC_DC_DCERSTSZ,
    ERST_NUMBER
    );
  return EFI_SUCCESS;
}

/**
  Create XHCI transfer ring.

  @param  Handle            Debug port handle.
  @param  TrbNum            The number of TRB in the ring.
  @param  TransferRing      The created transfer ring.

**/
VOID
CreateTransferRing (
  IN  USB3_DEBUG_PORT_HANDLE      *Handle,
  IN  UINT32                      TrbNum,
  OUT TRANSFER_RING               *TransferRing
  )
{
  VOID                  *Buf;
  LINK_TRB              *EndTrb;

  Buf = AllocateAlignBuffer (sizeof (TRB_TEMPLATE) * TrbNum);
  ASSERT (Buf != NULL);
  ASSERT (((UINTN) Buf & 0xF) == 0);
  ZeroMem (Buf, sizeof (TRB_TEMPLATE) * TrbNum);

  TransferRing->RingSeg0     = (EFI_PHYSICAL_ADDRESS)(UINTN) Buf;
  TransferRing->TrbNumber    = TrbNum;
  TransferRing->RingEnqueue  = TransferRing->RingSeg0;
  TransferRing->RingDequeue  = TransferRing->RingSeg0;
  TransferRing->RingPCS      = 1;
  //
  // 4.9.2 Transfer Ring Management
  // To form a ring (or circular queue) a Link TRB may be inserted at the end of a ring to
  // point to the first TRB in the ring.
  //
  EndTrb        = (LINK_TRB *) ((UINTN)Buf + sizeof (TRB_TEMPLATE) * (TrbNum - 1));
  EndTrb->Type  = TRB_TYPE_LINK;
  EndTrb->PtrLo = XHC_LOW_32BIT (Buf);
  EndTrb->PtrHi = XHC_HIGH_32BIT (Buf);
  //
  // Toggle Cycle (TC). When set to '1', the xHC shall toggle its interpretation of the Cycle bit.
  //
  EndTrb->TC    = 1;
  //
  // Set Cycle bit as other TRB PCS init value
  //
  EndTrb->CycleBit = 0;
}

/**
  Create debug capability context for XHC debug device.

  @param  Handle       Debug port handle.

  @retval EFI_SUCCESS  The bit successfully changed by host controller.
  @retval EFI_TIMEOUT  The time out occurred.

**/
EFI_STATUS
CreateDebugCapabilityContext (
  IN  USB3_DEBUG_PORT_HANDLE   *Handle
  )
{
  VOID                        *Buf;
  XHC_DC_CONTEXT              *DebugCapabilityContext;
  UINT8                       *String0Desc;
  UINT8                       *ManufacturerStrDesc;
  UINT8                       *ProductStrDesc;
  UINT8                       *SerialNumberStrDesc;

  //
  // Allocate debug device context
  //
  Buf = AllocateAlignBuffer (sizeof (XHC_DC_CONTEXT));
  ASSERT (Buf != NULL);
  ASSERT (((UINTN) Buf & 0xF) == 0);
  ZeroMem (Buf, sizeof (XHC_DC_CONTEXT));

  DebugCapabilityContext = (XHC_DC_CONTEXT *)(UINTN) Buf;
  Handle->DebugCapabilityContext = (EFI_PHYSICAL_ADDRESS)(UINTN) DebugCapabilityContext;

  //
  // Initialize DbcInfoContext.
  //
  DebugCapabilityContext->DbcInfoContext.String0Length         = STRING0_DESC_LEN;
  DebugCapabilityContext->DbcInfoContext.ManufacturerStrLength = MANU_DESC_LEN;
  DebugCapabilityContext->DbcInfoContext.ProductStrLength      = PRODUCT_DESC_LEN;
  DebugCapabilityContext->DbcInfoContext.SerialNumberStrLength = SERIAL_DESC_LEN;

  //
  // Initialize EpOutContext.
  //
  DebugCapabilityContext->EpOutContext.CErr             = 0x3;
  DebugCapabilityContext->EpOutContext.EPType           = ED_BULK_OUT;
  DebugCapabilityContext->EpOutContext.MaxPacketSize    = XHCI_DEBUG_DEVICE_MAX_PACKET_SIZE;
  DebugCapabilityContext->EpOutContext.AverageTRBLength = 0x1000;

  //
  // Initialize EpInContext.
  //
  DebugCapabilityContext->EpInContext.CErr             = 0x3;
  DebugCapabilityContext->EpInContext.EPType           = ED_BULK_IN;
  DebugCapabilityContext->EpInContext.MaxPacketSize    = XHCI_DEBUG_DEVICE_MAX_PACKET_SIZE;
  DebugCapabilityContext->EpInContext.AverageTRBLength = 0x1000;

  //
  // Update string descriptor address
  //
  String0Desc = (UINT8 *) AllocateAlignBuffer (STRING0_DESC_LEN + MANU_DESC_LEN + PRODUCT_DESC_LEN + SERIAL_DESC_LEN);
  ASSERT (String0Desc != NULL);
  ZeroMem (String0Desc, STRING0_DESC_LEN + MANU_DESC_LEN + PRODUCT_DESC_LEN + SERIAL_DESC_LEN);
  CopyMem (String0Desc, mString0Desc, STRING0_DESC_LEN);
  DebugCapabilityContext->DbcInfoContext.String0DescAddress = (UINT64)(UINTN)String0Desc;

  ManufacturerStrDesc = String0Desc + STRING0_DESC_LEN;
  CopyMem (ManufacturerStrDesc, mManufacturerStrDesc, MANU_DESC_LEN);
  DebugCapabilityContext->DbcInfoContext.ManufacturerStrDescAddress = (UINT64)(UINTN)ManufacturerStrDesc;

  ProductStrDesc = ManufacturerStrDesc + MANU_DESC_LEN;
  CopyMem (ProductStrDesc, mProductStrDesc, PRODUCT_DESC_LEN);
  DebugCapabilityContext->DbcInfoContext.ProductStrDescAddress = (UINT64)(UINTN)ProductStrDesc;

  SerialNumberStrDesc = ProductStrDesc + PRODUCT_DESC_LEN;
  CopyMem (SerialNumberStrDesc, mSerialNumberStrDesc, SERIAL_DESC_LEN);
  DebugCapabilityContext->DbcInfoContext.SerialNumberStrDescAddress = (UINT64)(UINTN)SerialNumberStrDesc;

  //
  // Allocate and initialize the Transfer Ring for the Input Endpoint Context.
  //
  ZeroMem (&Handle->TransferRingIn, sizeof (TRANSFER_RING));
  CreateTransferRing (Handle, TR_RING_TRB_NUMBER, &Handle->TransferRingIn);
  DebugCapabilityContext->EpInContext.PtrLo = XHC_LOW_32BIT (Handle->TransferRingIn.RingSeg0) | BIT0;
  DebugCapabilityContext->EpInContext.PtrHi = XHC_HIGH_32BIT (Handle->TransferRingIn.RingSeg0);

  //
  // Allocate and initialize the Transfer Ring for the Output Endpoint Context.
  //
  ZeroMem (&Handle->TransferRingOut, sizeof (TRANSFER_RING));
  CreateTransferRing (Handle, TR_RING_TRB_NUMBER, &Handle->TransferRingOut);
  DebugCapabilityContext->EpOutContext.PtrLo = XHC_LOW_32BIT (Handle->TransferRingOut.RingSeg0) | BIT0;
  DebugCapabilityContext->EpOutContext.PtrHi = XHC_HIGH_32BIT (Handle->TransferRingOut.RingSeg0);

  //
  // Program the Debug Capability Context Pointer (DCCP) register(7.6.8.7)
  //
  XhcWriteDebugReg (
    Handle,
    XHC_DC_DCCP,
    XHC_LOW_32BIT((UINT64)(UINTN)DebugCapabilityContext)
    );
  XhcWriteDebugReg (
    Handle,
    XHC_DC_DCCP + 4,
    XHC_HIGH_32BIT((UINT64)(UINTN)DebugCapabilityContext)
    );
  return EFI_SUCCESS;
}

/**
  Check if debug device is running.

  @param  Handle       Debug port handle.

**/
VOID
XhcDetectDebugCapabilityReady (
  IN USB3_DEBUG_PORT_HANDLE *Handle
  )
{
  UINT64                          TimeOut;
  volatile UINT32                 Dcctrl;

  TimeOut = 1;
  if (Handle->Initialized == USB3DBG_DBG_CAB) {
    //
    // As detection is slow in seconds, wait for longer timeout for the first time.
    // If first initialization is failed, we will try to enable debug device in the
    // Poll function invoked by timer.
    //
    TimeOut = DivU64x32 (PcdGet64 (PcdUsbXhciDebugDetectTimeout), XHC_POLL_DELAY) + 1;
  }

  do {
    //
    // Check if debug device is in configured state
    //
    Dcctrl = XhcReadDebugReg (Handle, XHC_DC_DCCTRL);
    if ((Dcctrl & BIT0) != 0) {
      //
      // Set the flag to indicate debug device is in configured state
      //
      Handle->Ready = TRUE;
      break;
    }
    MicroSecondDelay (XHC_POLL_DELAY);
    TimeOut--;
  } while (TimeOut != 0);
}

/**
  Initialize usb debug port hardware.

  @param  Handle           Debug port handle.

  @retval TRUE             The usb debug port hardware configuration is changed.
  @retval FALSE            The usb debug port hardware configuration is not changed.

**/
RETURN_STATUS
EFIAPI
InitializeUsbDebugHardware (
  IN USB3_DEBUG_PORT_HANDLE *Handle
  )
{
  RETURN_STATUS                   Status;
  UINT8                           *Buffer;
  UINTN                           Index;
  UINT8                           TotalUsb3Port;
  EFI_PHYSICAL_ADDRESS            XhciOpRegister;

  XhciOpRegister = Handle->XhciOpRegister;
  TotalUsb3Port = MmioRead32 (((UINTN) Handle->XhciMmioBase + XHC_HCSPARAMS1_OFFSET)) >> 24;

  if (Handle->Initialized == USB3DBG_NOT_ENABLED) {
    //
    // If XHCI supports debug capability, hardware resource has been allocated,
    // but it has not been enabled, try to enable again.
    //
    goto Enable;
  }

  //
  // Initialize for PEI phase when AllocatePages can work.
  // Allocate data buffer with max packet size for data read and data poll.
  // Allocate data buffer for data write.
  //
  Buffer = AllocateAlignBuffer (XHCI_DEBUG_DEVICE_MAX_PACKET_SIZE * 2 + USB3_DEBUG_PORT_WRITE_MAX_PACKET_SIZE);
  if (Buffer == NULL) {
    //
    // AllocatePages can not still work now, return fail and do not initialize now.
    //
    return RETURN_NOT_READY;
  }

  //
  // Reset port to get debug device discovered
  //
  for (Index = 0; Index < TotalUsb3Port; Index++) {
    XhcSetR32Bit ((UINTN)XhciOpRegister + XHC_PORTSC_OFFSET + Index * 0x10, BIT4);
    MicroSecondDelay (10 * 1000);
  }

  //
  // Construct the buffer for read, poll and write.
  //
  Handle->UrbIn.Data  = (EFI_PHYSICAL_ADDRESS)(UINTN) Buffer;
  Handle->Data        = (EFI_PHYSICAL_ADDRESS)(UINTN) Buffer + XHCI_DEBUG_DEVICE_MAX_PACKET_SIZE;
  Handle->UrbOut.Data = Handle->UrbIn.Data + XHCI_DEBUG_DEVICE_MAX_PACKET_SIZE * 2;

  //
  // Initialize event ring
  //
  ZeroMem (&Handle->EventRing, sizeof (EVENT_RING));
  Status = CreateEventRing (Handle, &Handle->EventRing);
  ASSERT_EFI_ERROR (Status);

  //
  // Init IN and OUT endpoint context
  //
  Status = CreateDebugCapabilityContext (Handle);
  ASSERT_EFI_ERROR (Status);

  //
  // Init DCDDI1 and DCDDI2
  //
  XhcWriteDebugReg (
   Handle,
   XHC_DC_DCDDI1,
   (UINT32)((XHCI_DEBUG_DEVICE_VENDOR_ID << 16) | XHCI_DEBUG_DEVICE_PROTOCOL)
   );

  XhcWriteDebugReg (
   Handle,
   XHC_DC_DCDDI2,
   (UINT32)((XHCI_DEBUG_DEVICE_REVISION << 16) | XHCI_DEBUG_DEVICE_PRODUCT_ID)
   );

Enable:
  if ((Handle->Initialized == USB3DBG_NOT_ENABLED) && (!Handle->ChangePortPower)) {
    //
    // If the first time detection is failed, turn port power off and on in order to
    // reset port status this time, then try to check if debug device is ready again.
    //
    for (Index = 0; Index < TotalUsb3Port; Index++) {
      XhcClearR32Bit ((UINTN)XhciOpRegister + XHC_PORTSC_OFFSET + Index * 0x10, BIT9);
      MicroSecondDelay (XHC_DEBUG_PORT_ON_OFF_DELAY);
      XhcSetR32Bit ((UINTN)XhciOpRegister + XHC_PORTSC_OFFSET + Index * 0x10, BIT9);
      MicroSecondDelay (XHC_DEBUG_PORT_ON_OFF_DELAY);
      Handle->ChangePortPower = TRUE;
    }
  }

  //
  // Set DCE bit and LSE bit to "1" in DCCTRL in first initialization
  //
  XhcSetDebugRegBit (Handle, XHC_DC_DCCTRL, BIT1|BIT31);

  XhcDetectDebugCapabilityReady (Handle);

  Status = RETURN_SUCCESS;
  if (!Handle->Ready) {
    Handle->Initialized = USB3DBG_NOT_ENABLED;
    Status = RETURN_NOT_READY;
  } else {
    Handle->Initialized = USB3DBG_ENABLED;
  }

  return Status;
}

/**
  Read data from debug device and save the data in buffer.

  Reads NumberOfBytes data bytes from a debug device into the buffer
  specified by Buffer. The number of bytes actually read is returned.
  If the return value is less than NumberOfBytes, then the rest operation failed.
  If NumberOfBytes is zero, then return 0.

  @param  Handle           Debug port handle.
  @param  Buffer           Pointer to the data buffer to store the data read from the debug device.
  @param  NumberOfBytes    Number of bytes which will be read.
  @param  Timeout          Timeout value for reading from debug device. It unit is Microsecond.

  @retval 0                Read data failed, no data is to be read.
  @retval >0               Actual number of bytes read from debug device.

**/
UINTN
EFIAPI
DebugPortReadBuffer (
  IN   DEBUG_PORT_HANDLE        Handle,
  IN   UINT8                    *Buffer,
  IN   UINTN                    NumberOfBytes,
  IN   UINTN                    Timeout
  )
{
  USB3_DEBUG_PORT_HANDLE    *UsbDebugPortHandle;
  RETURN_STATUS             Status;
  UINTN                     Received;
  UINTN                     Total;
  UINTN                     Remaining;
  UINT8                     Index;
  UINTN                     Length;
  UINT64                    Begin;
  UINT64                    TimeoutTicker;
  UINT64                    TimerRound;
  EFI_PHYSICAL_ADDRESS      XhciMmioBase;
  UINT8                     *Data;

  if (NumberOfBytes == 0 || Buffer == NULL) {
    return 0;
  }

  Received  = 0;
  Total     = 0;
  Remaining = 0;

  //
  // If Handle is NULL, it means memory is ready for use.
  // Use global variable to store handle value.
  //
  if (Handle == NULL) {
    UsbDebugPortHandle = &mDebugCommunicationLibUsb3DebugPortHandle;
  } else {
    UsbDebugPortHandle = (USB3_DEBUG_PORT_HANDLE *)Handle;
  }

  if (UsbDebugPortHandle->Initialized == USB3DBG_NO_DBG_CAB) {
    return 0;
  }

  XhciMmioBase = ProgramXhciBaseAddress ();
  UpdateXhcResource (UsbDebugPortHandle, XhciMmioBase);

  if (NeedReinitializeHardware(UsbDebugPortHandle)) {
    Status = InitializeUsbDebugHardware (UsbDebugPortHandle);
    if (RETURN_ERROR(Status)) {
      return 0;
    }
  }

  Data = (UINT8 *)(UINTN)UsbDebugPortHandle->Data;

  //
  // First read data from buffer, then read debug port hw to get received data.
  //
  if (UsbDebugPortHandle->DataCount > 0) {
    if (NumberOfBytes <= UsbDebugPortHandle->DataCount) {
      Total = NumberOfBytes;
    } else {
      Total = UsbDebugPortHandle->DataCount;
    }

    for (Index = 0; Index < Total; Index++) {
      Buffer[Index] = Data[Index];
    }

    for (Index = 0; Index < UsbDebugPortHandle->DataCount - Total; Index++) {
      if (Total + Index >= XHCI_DEBUG_DEVICE_MAX_PACKET_SIZE) {
        return 0;
      }
      Data[Index] = Data[Total + Index];
    }
    UsbDebugPortHandle->DataCount = (UINT8)(UsbDebugPortHandle->DataCount - (UINT8)Total);
  }

  //
  // If Timeout is equal to 0, then it means it should always wait until all data required are received.
  //
  Begin         = 0;
  TimeoutTicker = 0;
  TimerRound    = 0;
  if (Timeout != 0) {
    Begin = GetPerformanceCounter ();
    TimeoutTicker = DivU64x32 (
                      MultU64x64 (
                        UsbDebugPortHandle->TimerFrequency,
                        Timeout
                        ),
                      1000000u
                      );
    TimerRound = DivU64x64Remainder (
                   TimeoutTicker,
                   DivU64x32 (UsbDebugPortHandle->TimerCycle, 2),
                   &TimeoutTicker
                   );
  }

  //
  // Read remaining data by executing one or more usb debug transfer transactions at usb debug port hw.
  //
  while (Total < NumberOfBytes) {
    if (Timeout != 0) {
      if (TimerRound == 0) {
        if (IsTimerTimeout (UsbDebugPortHandle, Begin, TimeoutTicker)) {
          //
          // If time out occurs.
          //
          return 0;
        }
      } else {
        if (IsTimerTimeout (UsbDebugPortHandle, Begin, DivU64x32 (UsbDebugPortHandle->TimerCycle, 2))) {
          TimerRound --;
        }
      }
    }
    Remaining = NumberOfBytes - Total;
    if (Remaining >= XHCI_DEBUG_DEVICE_MAX_PACKET_SIZE) {
      Received = XHCI_DEBUG_DEVICE_MAX_PACKET_SIZE;
      Status = XhcDataTransfer (UsbDebugPortHandle, EfiUsbDataIn, Buffer + Total, &Received, DATA_TRANSFER_READ_TIMEOUT);
    } else {
      Received = XHCI_DEBUG_DEVICE_MAX_PACKET_SIZE;
      Status = XhcDataTransfer (UsbDebugPortHandle, EfiUsbDataIn, (VOID *)Data, &Received, DATA_TRANSFER_READ_TIMEOUT);
      UsbDebugPortHandle->DataCount = (UINT8) Received;

      if (Remaining <= Received) {
        //
        // The data received are more than required
        //
        Length = (UINT8)Remaining;
      } else {
        //
        // The data received are less than the remaining bytes
        //
        Length = (UINT8)Received;
      }

      //
      // Copy required data from the data buffer to user buffer.
      //
      for (Index = 0; Index < Length; Index++) {
        (Buffer + Total)[Index] = Data[Index];
        UsbDebugPortHandle->DataCount--;
      }

      //
      // reorder the data buffer to make available data arranged from the beginning of the data buffer.
      //
      for (Index = 0; Index < Received - Length; Index++) {
        if (Length + Index >= XHCI_DEBUG_DEVICE_MAX_PACKET_SIZE) {
          return 0;
        }
        Data[Index] = Data[Length + Index];
      }
      //
      // fixup the real required length of data.
      //
      Received = Length;
    }
    Total += Received;
  }
  return Total;
}

/**
  Write data from buffer to debug device.

  Writes NumberOfBytes data bytes from Buffer to the debug device.
  The number of bytes actually written to the debug device is returned.
  If the return value is less than NumberOfBytes, then the write operation failed.
  If NumberOfBytes is zero, then return 0.

  @param  Handle           Debug port handle.
  @param  Buffer           Pointer to the data buffer to be written.
  @param  NumberOfBytes    Number of bytes to written to the debug device.

  @retval 0                NumberOfBytes is 0.
  @retval >0               The number of bytes written to the debug device.
                           If this value is less than NumberOfBytes, then the read operation failed.

**/
UINTN
EFIAPI
DebugPortWriteBuffer (
  IN   DEBUG_PORT_HANDLE    Handle,
  IN   UINT8                *Buffer,
  IN   UINTN                NumberOfBytes
  )
{
  USB3_DEBUG_PORT_HANDLE    *UsbDebugPortHandle;
  RETURN_STATUS             Status;
  UINTN                     Sent;
  UINTN                     Total;
  EFI_PHYSICAL_ADDRESS      XhciMmioBase;
  UINTN                     Index;

  if (NumberOfBytes == 0 || Buffer == NULL) {
    return 0;
  }

  Sent  = 0;
  Total = 0;

  //
  // If Handle is NULL, it means memory is ready for use.
  // Use global variable to store handle value.
  //
  if (Handle == NULL) {
    UsbDebugPortHandle = &mDebugCommunicationLibUsb3DebugPortHandle;
  } else {
    UsbDebugPortHandle = (USB3_DEBUG_PORT_HANDLE *)Handle;
  }

  if (UsbDebugPortHandle->Initialized == USB3DBG_NO_DBG_CAB) {
    return 0;
  }

  //
  // MMIO base address is possible to clear, set it if it is cleared. (XhciMemorySpaceClose in PchUsbCommon.c)
  //
  XhciMmioBase = ProgramXhciBaseAddress ();

  UpdateXhcResource (UsbDebugPortHandle, XhciMmioBase);

  if (NeedReinitializeHardware(UsbDebugPortHandle)) {
    Status = InitializeUsbDebugHardware (UsbDebugPortHandle);
    if (RETURN_ERROR(Status)) {
      return 0;
    }
  }

  //
  // When host is trying to send data, write will be blocked.
  // Poll to see if there is any data sent by host at first.
  //
  DebugPortPollBuffer (Handle);

  Index = 0;
  while ((Total < NumberOfBytes)) {
    if (NumberOfBytes - Total > USB3_DEBUG_PORT_WRITE_MAX_PACKET_SIZE) {
      Sent = USB3_DEBUG_PORT_WRITE_MAX_PACKET_SIZE;
    } else {
      Sent = (UINT8)(NumberOfBytes - Total);
    }
    Status = XhcDataTransfer (UsbDebugPortHandle, EfiUsbDataOut, Buffer + Total, &Sent, DATA_TRANSFER_WRITE_TIMEOUT);
    Total += Sent;
  }

  return Total;
}

/**
  Polls a debug device to see if there is any data waiting to be read.

  Polls a debug device to see if there is any data waiting to be read.
  If there is data waiting to be read from the debug device, then TRUE is returned.
  If there is no data waiting to be read from the debug device, then FALSE is returned.

  @param  Handle           Debug port handle.

  @retval TRUE             Data is waiting to be read from the debug device.
  @retval FALSE            There is no data waiting to be read from the serial device.

**/
BOOLEAN
EFIAPI
DebugPortPollBuffer (
  IN DEBUG_PORT_HANDLE      Handle
  )
{
  USB3_DEBUG_PORT_HANDLE     *UsbDebugPortHandle;
  UINTN                     Length;
  RETURN_STATUS             Status;
  EFI_PHYSICAL_ADDRESS      XhciMmioBase;

  //
  // If Handle is NULL, it means memory is ready for use.
  // Use global variable to store handle value.
  //
  if (Handle == NULL) {
    UsbDebugPortHandle = &mDebugCommunicationLibUsb3DebugPortHandle;
  } else {
    UsbDebugPortHandle = (USB3_DEBUG_PORT_HANDLE *)Handle;
  }

  if (UsbDebugPortHandle->Initialized == USB3DBG_NO_DBG_CAB) {
    return 0;
  }

  XhciMmioBase = ProgramXhciBaseAddress ();
  UpdateXhcResource (UsbDebugPortHandle, XhciMmioBase);

  if (NeedReinitializeHardware(UsbDebugPortHandle)) {
    Status = InitializeUsbDebugHardware(UsbDebugPortHandle);
    if (RETURN_ERROR(Status)) {
      return FALSE;
    }
  }

  //
  // If the data buffer is not empty, then return TRUE directly.
  // Otherwise initialize a usb read transaction and read data to internal data buffer.
  //
  if (UsbDebugPortHandle->DataCount != 0) {
    return TRUE;
  }

  //
  // Read data as much as we can
  //
  Length = XHCI_DEBUG_DEVICE_MAX_PACKET_SIZE;
  XhcDataTransfer (Handle, EfiUsbDataIn, (VOID *)(UINTN)UsbDebugPortHandle->Data, &Length, DATA_TRANSFER_POLL_TIMEOUT);

  if (Length > XHCI_DEBUG_DEVICE_MAX_PACKET_SIZE) {
    return FALSE;
  }

  if (Length == 0) {
    return FALSE;
  }

  //
  // Store data into internal buffer for use later
  //
  UsbDebugPortHandle->DataCount = (UINT8) Length;
  return TRUE;
}

/**
  Initialize the debug port.

  If Function is not NULL, Debug Communication Libary will call this function
  by passing in the Context to be the first parameter. If needed, Debug Communication
  Library will create one debug port handle to be the second argument passing in
  calling the Function, otherwise it will pass NULL to be the second argument of
  Function.

  If Function is NULL, and Context is not NULL, the Debug Communication Library could
    a) Return the same handle as passed in (as Context parameter).
    b) Ignore the input Context parameter and create new hanlde to be returned.

  If parameter Function is NULL and Context is NULL, Debug Communication Library could
  created a new handle if needed and return it, otherwise it will return NULL.

  @param[in] Context      Context needed by callback function; it was optional.
  @param[in] Function     Continue function called by Debug Communication library;
                          it was optional.

  @return  The debug port handle created by Debug Communication Library if Function
           is not NULL.

**/
DEBUG_PORT_HANDLE
EFIAPI
DebugPortInitialize (
  IN VOID                 *Context,
  IN DEBUG_PORT_CONTINUE  Function
  )
{
  RETURN_STATUS             Status;
  USB3_DEBUG_PORT_HANDLE    Handle;
  USB3_DEBUG_PORT_HANDLE    *UsbDebugPortHandle;
  UINT64                    TimerStartValue;
  UINT64                    TimerEndValue;

  //
  // Validate the PCD PcdDebugPortHandleBufferSize value
  //
  ASSERT (PcdGet16 (PcdDebugPortHandleBufferSize) == sizeof (USB3_DEBUG_PORT_HANDLE));

  if (Function == NULL && Context != NULL) {
    UsbDebugPortHandle = (USB3_DEBUG_PORT_HANDLE *)Context;
  } else {
    ZeroMem(&Handle, sizeof (USB3_DEBUG_PORT_HANDLE));
    UsbDebugPortHandle = &Handle;
  }

  UsbDebugPortHandle->TimerFrequency = GetPerformanceCounterProperties (
                                         &TimerStartValue,
                                         &TimerEndValue
                                         );

  if (TimerEndValue < TimerStartValue) {
    UsbDebugPortHandle->TimerCountDown = TRUE;
    UsbDebugPortHandle->TimerCycle     = TimerStartValue - TimerEndValue;
  } else {
    UsbDebugPortHandle->TimerCountDown = FALSE;
    UsbDebugPortHandle->TimerCycle     = TimerEndValue - TimerStartValue;
  }

  if (Function == NULL && Context != NULL) {
    return (DEBUG_PORT_HANDLE *) Context;
  }

  //
  // Read 64-bit MMIO base address
  //
  UsbDebugPortHandle->XhciMmioBase = ProgramXhciBaseAddress ();

  Status = CalculateUsbDebugPortMmioBase (UsbDebugPortHandle);
  if (RETURN_ERROR (Status)) {
    goto Exit;
  }

  if (NeedReinitializeHardware(&Handle)) {
    Status = InitializeUsbDebugHardware (&Handle);
    if (RETURN_ERROR(Status)) {
      goto Exit;
    }
  }

Exit:

  if (Function != NULL) {
    Function (Context, &Handle);
  } else {
    CopyMem(&mDebugCommunicationLibUsb3DebugPortHandle, &Handle, sizeof (USB3_DEBUG_PORT_HANDLE));
  }

  return (DEBUG_PORT_HANDLE)(UINTN)&mDebugCommunicationLibUsb3DebugPortHandle;
}
