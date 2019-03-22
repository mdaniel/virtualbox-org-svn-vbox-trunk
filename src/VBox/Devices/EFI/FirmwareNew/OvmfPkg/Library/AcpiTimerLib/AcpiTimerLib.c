/** @file
  ACPI Timer implements one instance of Timer Library.

  Copyright (c) 2008 - 2012, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2011, Andrei Warkentin <andreiw@motorola.com>

  This program and the accompanying materials are
  licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Base.h>
#include <Library/TimerLib.h>
#include <Library/BaseLib.h>
#include <Library/IoLib.h>
#include <Library/PciLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <IndustryStandard/Pci22.h>
#include <IndustryStandard/Acpi.h>

//
// PCI Location of PIIX4 Power Management PCI Configuration Registers
//
#define PIIX4_POWER_MANAGEMENT_BUS       0x00
#define PIIX4_POWER_MANAGEMENT_DEVICE    0x01
#define PIIX4_POWER_MANAGEMENT_FUNCTION  0x03

//
// Macro to access PIIX4 Power Management PCI Configuration Registers
//
#define PIIX4_PCI_POWER_MANAGEMENT_REGISTER(Register) \
  PCI_LIB_ADDRESS (                                   \
    PIIX4_POWER_MANAGEMENT_BUS,                       \
    PIIX4_POWER_MANAGEMENT_DEVICE,                    \
    PIIX4_POWER_MANAGEMENT_FUNCTION,                  \
    Register                                          \
    )

//
// PCI Location of Q35 Power Management PCI Configuration Registers
//
#define Q35_POWER_MANAGEMENT_BUS       0x00
#define Q35_POWER_MANAGEMENT_DEVICE    0x1f
#define Q35_POWER_MANAGEMENT_FUNCTION  0x00

//
// Macro to access Q35 Power Management PCI Configuration Registers
//
#define Q35_PCI_POWER_MANAGEMENT_REGISTER(Register) \
  PCI_LIB_ADDRESS (                                 \
    Q35_POWER_MANAGEMENT_BUS,                       \
    Q35_POWER_MANAGEMENT_DEVICE,                    \
    Q35_POWER_MANAGEMENT_FUNCTION,                  \
    Register                                        \
    )

//
// PCI Location of Host Bridge PCI Configuration Registers
//
#define HOST_BRIDGE_BUS       0x00
#define HOST_BRIDGE_DEVICE    0x00
#define HOST_BRIDGE_FUNCTION  0x00

//
// Macro to access Host Bridge Configuration Registers
//
#define HOST_BRIDGE_REGISTER(Register) \
  PCI_LIB_ADDRESS (                    \
    HOST_BRIDGE_BUS,                   \
    HOST_BRIDGE_DEVICE,                \
    HOST_BRIDGE_FUNCTION,              \
    Register                           \
    )

//
// Host Bridge Device ID (DID) Register
//
#define HOST_BRIDGE_DID  HOST_BRIDGE_REGISTER (0x02)

//
// Host Bridge DID Register values
//
#define PCI_DEVICE_ID_INTEL_82441    0x1237  // DID value for PIIX4
#define PCI_DEVICE_ID_INTEL_Q35_MCH  0x29C0  // DID value for Q35

//
// Access Power Management PCI Config Regs based on Host Bridge type
//
#define PCI_POWER_MANAGEMENT_REGISTER(Register)                   \
  ((PciRead16 (HOST_BRIDGE_DID) == PCI_DEVICE_ID_INTEL_Q35_MCH) ? \
    Q35_PCI_POWER_MANAGEMENT_REGISTER (Register) :                \
    PIIX4_PCI_POWER_MANAGEMENT_REGISTER (Register))

//
// Power Management PCI Configuration Registers
//
#define PMBA                PCI_POWER_MANAGEMENT_REGISTER (0x40)
#define   PMBA_RTE          BIT0
#define PMREGMISC           PCI_POWER_MANAGEMENT_REGISTER (0x80)
#define   PMIOSE            BIT0

//
// The ACPI Time is a 24-bit counter
//
#define ACPI_TIMER_COUNT_SIZE  BIT24

//
// Offset in the Power Management Base Address to the ACPI Timer
//
#define ACPI_TIMER_OFFSET      0x8

#ifdef VBOX
UINT32 mPmba = 0x4000;

#define PCI_BAR_IO             0x1
#endif

/**
  The constructor function enables ACPI IO space.

  If ACPI I/O space not enabled, this function will enable it.
  It will always return RETURN_SUCCESS.

  @retval EFI_SUCCESS   The constructor always returns RETURN_SUCCESS.

**/
#ifndef VBOX
RETURN_STATUS
EFIAPI
AcpiTimerLibConstructor (
  VOID
  )
{
  //
  // Check to see if the Power Management Base Address is already enabled
  //
  if ((PciRead8 (PMREGMISC) & PMIOSE) == 0) {
    //
    // If the Power Management Base Address is not programmed,
    // then program the Power Management Base Address from a PCD.
    //
    PciAndThenOr32 (PMBA, (UINT32)(~0x0000FFC0), PcdGet16 (PcdAcpiPmBaseAddress));

    //
    // Enable PMBA I/O port decodes in PMREGMISC
    //
    PciOr8 (PMREGMISC, PMIOSE);
  }

  return RETURN_SUCCESS;
}
#else
RETURN_STATUS
EFIAPI
AcpiTimerLibConstructor (
  VOID
  )
{
  UINT8     u8Device = 7;
  UINT16    u16VendorID = 0;
  UINT16    u16DeviceID = 0;
  u16VendorID = PciRead16(PCI_LIB_ADDRESS(0, u8Device, 0, 0));
  u16DeviceID = PciRead16(PCI_LIB_ADDRESS(0, u8Device, 0, 2));
  if (   u16VendorID != 0x8086
      || u16DeviceID != 0x7113)
    return RETURN_ABORTED;

  if (PciRead8 (PCI_LIB_ADDRESS (0,u8Device,0,0x80)) & 1) {
    mPmba = PciRead32 (PCI_LIB_ADDRESS (0, u8Device, 0, 0x40));
    ASSERT (mPmba & PCI_BAR_IO);
    DEBUG((DEBUG_INFO, "%a:%d mPmba:%x\n", __FUNCTION__, __LINE__, mPmba));
    mPmba &= ~PCI_BAR_IO;
    DEBUG((DEBUG_INFO, "%a:%d mPmba:%x\n", __FUNCTION__, __LINE__, mPmba));
  } else {
    PciAndThenOr32 (PCI_LIB_ADDRESS (0,u8Device,0,0x40),
                    (UINT32) ~0xfc0, mPmba);
    PciOr8         (PCI_LIB_ADDRESS (0,u8Device,0,0x04), 0x01);
    DEBUG((DEBUG_INFO, "%a:%d mPmba:%x\n", __FUNCTION__, __LINE__, mPmba));
  }

  //
  // ACPI Timer enable is in Bus 0, Device ?, Function 3
  //
  PciOr8         (PCI_LIB_ADDRESS (0,u8Device,0,0x80), 0x01);
  return RETURN_SUCCESS;
}
#endif

/**
  Internal function to read the current tick counter of ACPI.

  Internal function to read the current tick counter of ACPI.

  @return The tick counter read.

**/
UINT32
InternalAcpiGetTimerTick (
  VOID
  )
{
  //
  //   Read PMBA to read and return the current ACPI timer value.
  //
#ifndef VBOX
  return IoRead32 ((PciRead32 (PMBA) & ~PMBA_RTE) + ACPI_TIMER_OFFSET);
#else
  return IoRead32 (mPmba + ACPI_TIMER_OFFSET);
#endif
}

/**
  Stalls the CPU for at least the given number of ticks.

  Stalls the CPU for at least the given number of ticks. It's invoked by
  MicroSecondDelay() and NanoSecondDelay().

  @param  Delay     A period of time to delay in ticks.

**/
VOID
InternalAcpiDelay (
  IN      UINT32                    Delay
  )
{
  UINT32                            Ticks;
  UINT32                            Times;

  Times    = Delay >> 22;
  Delay   &= BIT22 - 1;
  do {
    //
    // The target timer count is calculated here
    //
    Ticks    = InternalAcpiGetTimerTick () + Delay;
    Delay    = BIT22;
    //
    // Wait until time out
    // Delay >= 2^23 could not be handled by this function
    // Timer wrap-arounds are handled correctly by this function
    //
    while (((Ticks - InternalAcpiGetTimerTick ()) & BIT23) == 0) {
      CpuPause ();
    }
  } while (Times-- > 0);
}

/**
  Stalls the CPU for at least the given number of microseconds.

  Stalls the CPU for the number of microseconds specified by MicroSeconds.

  @param  MicroSeconds  The minimum number of microseconds to delay.

  @return MicroSeconds

**/
UINTN
EFIAPI
MicroSecondDelay (
  IN      UINTN                     MicroSeconds
  )
{
  InternalAcpiDelay (
    (UINT32)DivU64x32 (
              MultU64x32 (
                MicroSeconds,
                ACPI_TIMER_FREQUENCY
                ),
              1000000u
              )
    );
  return MicroSeconds;
}

/**
  Stalls the CPU for at least the given number of nanoseconds.

  Stalls the CPU for the number of nanoseconds specified by NanoSeconds.

  @param  NanoSeconds The minimum number of nanoseconds to delay.

  @return NanoSeconds

**/
UINTN
EFIAPI
NanoSecondDelay (
  IN      UINTN                     NanoSeconds
  )
{
  InternalAcpiDelay (
    (UINT32)DivU64x32 (
              MultU64x32 (
                NanoSeconds,
                ACPI_TIMER_FREQUENCY
                ),
              1000000000u
              )
    );
  return NanoSeconds;
}

/**
  Retrieves the current value of a 64-bit free running performance counter.

  Retrieves the current value of a 64-bit free running performance counter. The
  counter can either count up by 1 or count down by 1. If the physical
  performance counter counts by a larger increment, then the counter values
  must be translated. The properties of the counter can be retrieved from
  GetPerformanceCounterProperties().

  @return The current value of the free running performance counter.

**/
UINT64
EFIAPI
GetPerformanceCounter (
  VOID
  )
{
  return (UINT64)InternalAcpiGetTimerTick ();
}

/**
  Retrieves the 64-bit frequency in Hz and the range of performance counter
  values.

  If StartValue is not NULL, then the value that the performance counter starts
  with immediately after is it rolls over is returned in StartValue. If
  EndValue is not NULL, then the value that the performance counter end with
  immediately before it rolls over is returned in EndValue. The 64-bit
  frequency of the performance counter in Hz is always returned. If StartValue
  is less than EndValue, then the performance counter counts up. If StartValue
  is greater than EndValue, then the performance counter counts down. For
  example, a 64-bit free running counter that counts up would have a StartValue
  of 0 and an EndValue of 0xFFFFFFFFFFFFFFFF. A 24-bit free running counter
  that counts down would have a StartValue of 0xFFFFFF and an EndValue of 0.

  @param  StartValue  The value the performance counter starts with when it
                      rolls over.
  @param  EndValue    The value that the performance counter ends with before
                      it rolls over.

  @return The frequency in Hz.

**/
UINT64
EFIAPI
GetPerformanceCounterProperties (
  OUT      UINT64                    *StartValue,  OPTIONAL
  OUT      UINT64                    *EndValue     OPTIONAL
  )
{
  if (StartValue != NULL) {
    *StartValue = 0;
  }

  if (EndValue != NULL) {
    *EndValue = ACPI_TIMER_COUNT_SIZE - 1;
  }

  return ACPI_TIMER_FREQUENCY;
}

/**
  Converts elapsed ticks of performance counter to time in nanoseconds.

  This function converts the elapsed ticks of running performance counter to
  time value in unit of nanoseconds.

  @param  Ticks     The number of elapsed ticks of running performance counter.

  @return The elapsed time in nanoseconds.

**/
UINT64
EFIAPI
GetTimeInNanoSecond (
  IN      UINT64                     Ticks
  )
{
  UINT64  NanoSeconds;
  UINT32  Remainder;

  //
  //          Ticks
  // Time = --------- x 1,000,000,000
  //        Frequency
  //
  NanoSeconds = MultU64x32 (DivU64x32Remainder (Ticks, ACPI_TIMER_FREQUENCY, &Remainder), 1000000000u);

  //
  // Frequency < 0x100000000, so Remainder < 0x100000000, then (Remainder * 1,000,000,000)
  // will not overflow 64-bit.
  //
  NanoSeconds += DivU64x32 (MultU64x32 ((UINT64) Remainder, 1000000000u), ACPI_TIMER_FREQUENCY);

  return NanoSeconds;
}
