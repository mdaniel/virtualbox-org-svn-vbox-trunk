/** @file
  Semaphore mechanism to indicate to the BSP that an AP has exited SMM
  after SMBASE relocation.

  Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "InternalSmmRelocationLib.h"

UINTN             mSmmRelocationOriginalAddress;
volatile BOOLEAN  *mRebasedFlag;

/**
  Hook return address of SMM Save State so that semaphore code
  can be executed immediately after AP exits SMM to indicate to
  the BSP that an AP has exited SMM after SMBASE relocation.

  @param[in] RebasedFlag  A pointer to a flag that is set to TRUE
                          immediately after AP exits SMM.

**/
VOID
SemaphoreHook (
  IN volatile BOOLEAN  *RebasedFlag
  )
{
  SMRAM_SAVE_STATE_MAP  *CpuState;

  mRebasedFlag = RebasedFlag;

  CpuState                      = (SMRAM_SAVE_STATE_MAP *)(UINTN)(SMM_DEFAULT_SMBASE + SMRAM_SAVE_STATE_MAP_OFFSET);
  mSmmRelocationOriginalAddress = (UINTN)HookReturnFromSmm (
                                           CpuState,
                                           (UINT64)(UINTN)&SmmRelocationSemaphoreComplete,
                                           (UINT64)(UINTN)&SmmRelocationSemaphoreComplete
                                           );
}
