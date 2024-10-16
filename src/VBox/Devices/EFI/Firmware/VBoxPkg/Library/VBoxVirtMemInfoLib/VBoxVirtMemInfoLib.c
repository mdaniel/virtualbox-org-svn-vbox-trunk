/* $Id$ */
/** @file
 * VBoxVirtMemInfoLib.c - Providing the address map for setting up the MMU based on the platform settings.
 */

/*
 * Copyright (C) 2023-2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#include <Uefi.h>
#include <Pi/PiMultiPhase.h>
#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Library/VBoxArmPlatformLib.h>

// Number of Virtual Memory Map Descriptors
#define MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS  8

/**
  Default library constructur that obtains the memory size from a PCD.

  @return  Always returns RETURN_SUCCESS

**/
RETURN_STATUS
EFIAPI
VBoxVirtMemInfoLibConstructor (
  VOID
  )
{
  UINT64  Size;
  VOID    *Hob;

  Size = VBoxArmPlatformRamBaseSizeGet();
  Hob  = BuildGuidDataHob (&gArmVirtSystemMemorySizeGuid, &Size, sizeof Size);
  ASSERT (Hob != NULL);

  return RETURN_SUCCESS;
}

/**
  Return the Virtual Memory Map of your platform

  This Virtual Memory Map is used by MemoryInitPei Module to initialize the MMU
  on your platform.

  @param[out]   VirtualMemoryMap    Array of ARM_MEMORY_REGION_DESCRIPTOR
                                    describing a Physical-to-Virtual Memory
                                    mapping. This array must be ended by a
                                    zero-filled entry. The allocated memory
                                    will not be freed.

**/
VOID
ArmVirtGetMemoryMap (
  OUT ARM_MEMORY_REGION_DESCRIPTOR  **VirtualMemoryMap
  )
{
  ARM_MEMORY_REGION_DESCRIPTOR  *VirtualMemoryTable;
  UINT8 idxMemDesc = 0;

  ASSERT (VirtualMemoryMap != NULL);

  VirtualMemoryTable = AllocatePool (
                         sizeof (ARM_MEMORY_REGION_DESCRIPTOR) *
                         MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS
                         );

  if (VirtualMemoryTable == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Error: Failed AllocatePool()\n", __func__));
    return;
  }

  // System DRAM
  VirtualMemoryTable[idxMemDesc].PhysicalBase = VBoxArmPlatformRamBaseStartGetPhysAddr();
  VirtualMemoryTable[idxMemDesc].VirtualBase  = VirtualMemoryTable[idxMemDesc].PhysicalBase;
  VirtualMemoryTable[idxMemDesc].Length       = VBoxArmPlatformRamBaseSizeGet();
  VirtualMemoryTable[idxMemDesc].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;
  idxMemDesc++;

  // Memory mapped peripherals
  VirtualMemoryTable[idxMemDesc].PhysicalBase = VBoxArmPlatformMmioStartGetPhysAddr();
  VirtualMemoryTable[idxMemDesc].VirtualBase  = VirtualMemoryTable[idxMemDesc].PhysicalBase;
  VirtualMemoryTable[idxMemDesc].Length       = VBoxArmPlatformMmioSizeGet();
  VirtualMemoryTable[idxMemDesc].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;
  idxMemDesc++;

  // Map the FV region as normal executable memory
  VirtualMemoryTable[idxMemDesc].PhysicalBase = VBoxArmPlatformUefiRomStartGetPhysAddr();
  VirtualMemoryTable[idxMemDesc].VirtualBase  = VirtualMemoryTable[idxMemDesc].PhysicalBase;
  VirtualMemoryTable[idxMemDesc].Length       = FixedPcdGet32 (PcdFvSize);
  VirtualMemoryTable[idxMemDesc].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK_RO;
  idxMemDesc++;

  // Map the FDT region readonnly.
  VirtualMemoryTable[idxMemDesc].PhysicalBase = VBoxArmPlatformFdtGet();
  VirtualMemoryTable[idxMemDesc].VirtualBase  = VirtualMemoryTable[idxMemDesc].PhysicalBase;
  VirtualMemoryTable[idxMemDesc].Length       = VBoxArmPlatformFdtSizeGet();
  VirtualMemoryTable[idxMemDesc].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK_RO;
  idxMemDesc++;

  // Map the VBox descriptor region readonly.
  VirtualMemoryTable[idxMemDesc].PhysicalBase = VBoxArmPlatformDescGetPhysAddr();
  VirtualMemoryTable[idxMemDesc].VirtualBase  = VirtualMemoryTable[idxMemDesc].PhysicalBase;
  VirtualMemoryTable[idxMemDesc].Length       = VBoxArmPlatformDescSizeGet();
  VirtualMemoryTable[idxMemDesc].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK_RO;
  idxMemDesc++;

  // Map the ACPI region if it exists.
  if (VBoxArmPlatformAcpiSizeGet() != 0)
  {
    VirtualMemoryTable[idxMemDesc].PhysicalBase = VBoxArmPlatformAcpiStartGetPhysAddr();
    VirtualMemoryTable[idxMemDesc].VirtualBase  = VirtualMemoryTable[idxMemDesc].PhysicalBase;
    VirtualMemoryTable[idxMemDesc].Length       = VBoxArmPlatformAcpiSizeGet();
    VirtualMemoryTable[idxMemDesc].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK_RO;
    idxMemDesc++;
  }

  // Map the MMIO32 region if it exists.
  if (VBoxArmPlatformMmio32SizeGet() != 0)
  {
    VirtualMemoryTable[idxMemDesc].PhysicalBase = VBoxArmPlatformMmio32StartGetPhysAddr();
    VirtualMemoryTable[idxMemDesc].VirtualBase  = VirtualMemoryTable[idxMemDesc].PhysicalBase;
    VirtualMemoryTable[idxMemDesc].Length       = VBoxArmPlatformMmio32SizeGet();
    VirtualMemoryTable[idxMemDesc].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;
    idxMemDesc++;
  }

  // End of Table
  ZeroMem (&VirtualMemoryTable[idxMemDesc], sizeof (ARM_MEMORY_REGION_DESCRIPTOR));

  for (UINTN i = 0; i < MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS; i++)
  {
    DEBUG ((
      DEBUG_INFO,
      "%a: Memory Table Entry %u:\n"
      "\tPhysicalBase: 0x%lX\n"
      "\tVirtualBase: 0x%lX\n"
      "\tLength: 0x%lX\n",
      __func__, i,
      VirtualMemoryTable[i].PhysicalBase,
      VirtualMemoryTable[i].VirtualBase,
      VirtualMemoryTable[i].Length
      ));
  }

  *VirtualMemoryMap = VirtualMemoryTable;
}
