/** @file
  Implementation of the EfiSetMem routine. This function is broken
  out into its own source file so that it can be excluded from a
  build for a particular platform easily if an optimized version
  is desired.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/




#include "MemLibInternals.h"

/**
  Set Buffer to Value for Size bytes.

  @param  Buffer   The memory to set.
  @param  Length   The number of bytes to set.
  @param  Value    The value of the set operation.

  @return Buffer

**/
VOID *
EFIAPI
InternalMemSetMem (
  OUT     VOID                      *Buffer,
  IN      UINTN                     Length,
  IN      UINT8                     Value
  )
{
  //
  // Declare the local variables that actually move the data elements as
  // volatile to prevent the optimizer from replacing this function with
  // the intrinsic memset()
  //
  volatile UINT8                    *Pointer;

  Pointer = (UINT8*)Buffer;
  while (Length-- > 0) {
    *(Pointer++) = Value;
  }
  return Buffer;
}
