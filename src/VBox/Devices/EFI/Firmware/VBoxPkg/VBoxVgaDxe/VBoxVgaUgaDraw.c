/* $Id$ */
/** @file
 * VBoxVgaUgaDraw.c
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

/*
  This code is based on:

  This file produces the graphics abstraction of UGA Draw. It is called by
  VBoxVga.c file which deals with the EFI 1.1 driver model.
  This file just does graphics.

  Copyright (c) 2006, Intel Corporation
  All rights reserved. This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

*/

#include "VBoxVga.h"

//
// UGA Draw Protocol Member Functions
//
EFI_STATUS
EFIAPI
VBoxVgaUgaDrawGetMode (
  IN  EFI_UGA_DRAW_PROTOCOL *This,
  OUT UINT32                *HorizontalResolution,
  OUT UINT32                *VerticalResolution,
  OUT UINT32                *ColorDepth,
  OUT UINT32                *RefreshRate
  )
{
  VBOX_VGA_PRIVATE_DATA  *Private;

  Private = VBOX_VGA_PRIVATE_DATA_FROM_UGA_DRAW_THIS (This);

  if (Private->HardwareNeedsStarting) {
    return EFI_NOT_STARTED;
  }

  if ((HorizontalResolution == NULL) ||
      (VerticalResolution == NULL)   ||
      (ColorDepth == NULL)           ||
      (RefreshRate == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *HorizontalResolution = Private->ModeData[Private->CurrentMode].HorizontalResolution;
  *VerticalResolution   = Private->ModeData[Private->CurrentMode].VerticalResolution;
  *ColorDepth           = Private->ModeData[Private->CurrentMode].ColorDepth;
  *RefreshRate          = Private->ModeData[Private->CurrentMode].RefreshRate;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
VBoxVgaUgaDrawSetMode (
  IN  EFI_UGA_DRAW_PROTOCOL *This,
  IN  UINT32                HorizontalResolution,
  IN  UINT32                VerticalResolution,
  IN  UINT32                ColorDepth,
  IN  UINT32                RefreshRate
  )
{
  VBOX_VGA_PRIVATE_DATA  *Private;
  UINTN                           Index;

  DEBUG((DEBUG_INFO, "%a:%d VIDEO: %dx%d %d bpp\n", __FILE__, __LINE__, HorizontalResolution, VerticalResolution, ColorDepth));
  Private = VBOX_VGA_PRIVATE_DATA_FROM_UGA_DRAW_THIS (This);

  for (Index = 0; Index < Private->MaxMode; Index++) {

    if (HorizontalResolution != Private->ModeData[Index].HorizontalResolution) {
      continue;
    }

    if (VerticalResolution != Private->ModeData[Index].VerticalResolution) {
      continue;
    }

    if (ColorDepth != Private->ModeData[Index].ColorDepth) {
      continue;
    }

#if 0
    if (RefreshRate != Private->ModeData[Index].RefreshRate) {
      continue;
    }
#endif

    InitializeGraphicsMode (Private, &VBoxVgaVideoModes[Private->ModeData[Index].ModeNumber]);

    Private->CurrentMode            = Index;

    Private->HardwareNeedsStarting  = FALSE;

    /* update current mode */
    Private->CurrentMode = Index;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
VBoxVgaUgaDrawBlt (
  IN  EFI_UGA_DRAW_PROTOCOL     *This,
  IN  EFI_UGA_PIXEL             *BltBuffer, OPTIONAL
  IN  EFI_UGA_BLT_OPERATION     BltOperation,
  IN  UINTN                     SourceX,
  IN  UINTN                     SourceY,
  IN  UINTN                     DestinationX,
  IN  UINTN                     DestinationY,
  IN  UINTN                     Width,
  IN  UINTN                     Height,
  IN  UINTN                     Delta
  )
{
  VBOX_VGA_PRIVATE_DATA     *Private;
  EFI_TPL                   OriginalTPL;
  UINTN                     DstY;
  UINTN                     SrcY;
  UINTN                     ScreenWidth;
  UINTN                     ScreenHeight;
  EFI_STATUS                Status;

  Private = VBOX_VGA_PRIVATE_DATA_FROM_UGA_DRAW_THIS (This);
  ScreenWidth = Private->ModeData[Private->CurrentMode].HorizontalResolution;
  ScreenHeight = Private->ModeData[Private->CurrentMode].VerticalResolution;

  if ((BltOperation < 0) || (BltOperation >= EfiUgaBltMax)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Width == 0 || Height == 0) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // If Delta is zero, then the entire BltBuffer is being used, so Delta
  // is the number of bytes in each row of BltBuffer.  Since BltBuffer is Width pixels size,
  // the number of bytes in each row can be computed.
  //
  if (Delta == 0) {
    Delta = Width * sizeof (EFI_UGA_PIXEL);
  }
  // code below assumes a Delta value in pixels, not bytes
  Delta /= sizeof (EFI_UGA_PIXEL);

  //
  // Make sure the SourceX, SourceY, DestinationX, DestinationY, Width, and Height parameters
  // are valid for the operation and the current screen geometry.
  //
  if (BltOperation == EfiUgaVideoToBltBuffer || BltOperation == EfiUgaVideoToVideo) {
    if (SourceY + Height > ScreenHeight) {
      return EFI_INVALID_PARAMETER;
    }

    if (SourceX + Width > ScreenWidth) {
      return EFI_INVALID_PARAMETER;
    }
  }
  if (BltOperation == EfiUgaBltBufferToVideo || BltOperation == EfiUgaVideoToVideo || BltOperation == EfiUgaVideoFill) {
    if (DestinationY + Height > ScreenHeight) {
      return EFI_INVALID_PARAMETER;
    }

    if (DestinationX + Width > ScreenWidth) {
      return EFI_INVALID_PARAMETER;
    }
  }

  //
  // We have to raise to TPL Notify, so we make an atomic write the frame buffer.
  // We would not want a timer based event (Cursor, ...) to come in while we are
  // doing this operation.
  //
  OriginalTPL = gBS->RaiseTPL (TPL_NOTIFY);

  switch (BltOperation) {
  case EfiUgaVideoToBltBuffer:
    //
    // Video to BltBuffer: Source is Video, destination is BltBuffer
    //
    for (SrcY = SourceY, DstY = DestinationY; DstY < (Height + DestinationY); SrcY++, DstY++) {
      /// @todo assumes that color depth is 32 (*4, EfiPciIoWidthUint32) and format matches EFI_UGA_PIXEL
      Status = Private->PciIo->Mem.Read (
                                    Private->PciIo,
                                    EfiPciIoWidthUint32,
                                    0,
                                    ((SrcY * ScreenWidth) + SourceX) * 4,
                                    Width,
                                    BltBuffer + (DstY * Delta) + DestinationX
                                    );
      ASSERT_EFI_ERROR((Status));
    }
    break;

  case EfiUgaBltBufferToVideo:
    //
    // BltBuffer to Video: Source is BltBuffer, destination is Video
    //
    for (SrcY = SourceY, DstY = DestinationY; SrcY < (Height + SourceY); SrcY++, DstY++) {
      /// @todo assumes that color depth is 32 (*4, EfiPciIoWidthUint32) and format matches EFI_UGA_PIXEL
      Status = Private->PciIo->Mem.Write (
                                    Private->PciIo,
                                    EfiPciIoWidthUint32,
                                    0,
                                    ((DstY * ScreenWidth) + DestinationX) * 4,
                                    Width,
                                    BltBuffer + (SrcY * Delta) + SourceX
                                    );
      ASSERT_EFI_ERROR((Status));
    }
    break;

  case EfiUgaVideoToVideo:
    //
    // Video to Video: Source is Video, destination is Video
    //
    if (DestinationY <= SourceY) {
      // forward copy
      for (SrcY = SourceY, DstY = DestinationY; SrcY < (Height + SourceY); SrcY++, DstY++) {
        /// @todo assumes that color depth is 32 (*4, EfiPciIoWidthUint32) and format matches EFI_UGA_PIXEL
        Status = Private->PciIo->CopyMem (
                                    Private->PciIo,
                                    EfiPciIoWidthUint32,
                                    0,
                                    ((DstY * ScreenWidth) + DestinationX) * 4,
                                    0,
                                    ((SrcY * ScreenWidth) + SourceX) * 4,
                                    Width
                                    );
        ASSERT_EFI_ERROR((Status));
      }
    } else {
      // reverse copy
      for (SrcY = SourceY + Height - 1, DstY = DestinationY + Height - 1; SrcY >= SourceY && SrcY <= SourceY + Height - 1; SrcY--, DstY--) {
        /// @todo assumes that color depth is 32 (*4, EfiPciIoWidthUint32) and format matches EFI_UGA_PIXEL
        Status = Private->PciIo->CopyMem (
                                    Private->PciIo,
                                    EfiPciIoWidthUint32,
                                    0,
                                    ((DstY * ScreenWidth) + DestinationX) * 4,
                                    0,
                                    ((SrcY * ScreenWidth) + SourceX) * 4,
                                    Width
                                    );
        ASSERT_EFI_ERROR((Status));
      }
    }
    break;

  case EfiUgaVideoFill:
    //
    // Video Fill: Source is BltBuffer, destination is Video
    //
    if (DestinationX == 0 && Width == ScreenWidth) {
      /// @todo assumes that color depth is 32 (*4, EfiPciIoWidthFillUint32) and format matches EFI_UGA_PIXEL
      Status = Private->PciIo->Mem.Write (
                                    Private->PciIo,
                                    EfiPciIoWidthFillUint32,
                                    0,
                                    DestinationY * ScreenWidth * 4,
                                    (Width * Height),
                                    BltBuffer
                                    );
      ASSERT_EFI_ERROR((Status));
    } else {
      for (SrcY = SourceY, DstY = DestinationY; SrcY < (Height + SourceY); SrcY++, DstY++) {
        /// @todo assumes that color depth is 32 (*4, EfiPciIoWidthFillUint32) and format matches EFI_UGA_PIXEL
        Private->PciIo->Mem.Write (
                              Private->PciIo,
                              EfiPciIoWidthFillUint32,
                              0,
                              ((DstY * ScreenWidth) + DestinationX) * 4,
                              Width,
                              BltBuffer
                              );
      }
    }
    break;

  default:
    ASSERT (FALSE);
  }

  gBS->RestoreTPL (OriginalTPL);

  return EFI_SUCCESS;
}

//
// Construction and Destruction functions
//
EFI_STATUS
VBoxVgaUgaDrawConstructor (
  VBOX_VGA_PRIVATE_DATA  *Private
  )
{
  EFI_UGA_DRAW_PROTOCOL *UgaDraw;
  UINT32                HorizontalResolution = 1024;
  UINT32                VerticalResolution = 768;

  //
  // Fill in Private->UgaDraw protocol
  //
  UgaDraw           = &Private->UgaDraw;

  UgaDraw->GetMode  = VBoxVgaUgaDrawGetMode;
  UgaDraw->SetMode  = VBoxVgaUgaDrawSetMode;
  UgaDraw->Blt      = VBoxVgaUgaDrawBlt;

  //
  // Initialize the private data
  //
  Private->CurrentMode            = 0;
  Private->HardwareNeedsStarting  = TRUE;

  //
  // Initialize the hardware
  //
  VBoxVgaGetVmVariable(EFI_INFO_INDEX_UGA_HORIZONTAL_RESOLUTION, (CHAR8 *)&HorizontalResolution,
                       sizeof(HorizontalResolution));
  VBoxVgaGetVmVariable(EFI_INFO_INDEX_UGA_VERTICAL_RESOLUTION, (CHAR8 *)&VerticalResolution,
                       sizeof(VerticalResolution));

  UgaDraw->SetMode (
            UgaDraw,
            HorizontalResolution,
            VerticalResolution,
            32,
            60
            );

  DrawLogo (
    Private,
    Private->ModeData[Private->CurrentMode].HorizontalResolution,
    Private->ModeData[Private->CurrentMode].VerticalResolution
    );

  PcdSet32(PcdVideoHorizontalResolution, HorizontalResolution);
  PcdSet32(PcdVideoVerticalResolution, VerticalResolution);

  return EFI_SUCCESS;
}

