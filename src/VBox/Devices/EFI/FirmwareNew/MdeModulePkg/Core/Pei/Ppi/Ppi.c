/** @file
  EFI PEI Core PPI services

Copyright (c) 2006 - 2017, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "PeiMain.h"

/**

  Initialize PPI services.

  @param PrivateData     Pointer to the PEI Core data.
  @param OldCoreData     Pointer to old PEI Core data.
                         NULL if being run in non-permament memory mode.

**/
VOID
InitializePpiServices (
  IN PEI_CORE_INSTANCE *PrivateData,
  IN PEI_CORE_INSTANCE *OldCoreData
  )
{
  if (OldCoreData == NULL) {
    PrivateData->PpiData.NotifyListEnd = PcdGet32 (PcdPeiCoreMaxPpiSupported)-1;
    PrivateData->PpiData.DispatchListEnd = PcdGet32 (PcdPeiCoreMaxPpiSupported)-1;
    PrivateData->PpiData.LastDispatchedNotify = PcdGet32 (PcdPeiCoreMaxPpiSupported)-1;
  }
}

/**

  Migrate Pointer from the temporary memory to PEI installed memory.

  @param Pointer         Pointer to the Pointer needs to be converted.
  @param TempBottom      Base of old temporary memory
  @param TempTop         Top of old temporary memory
  @param Offset          Offset of new memory to old temporary memory.
  @param OffsetPositive  Positive flag of Offset value.

**/
VOID
ConvertPointer (
  IN OUT VOID              **Pointer,
  IN UINTN                 TempBottom,
  IN UINTN                 TempTop,
  IN UINTN                 Offset,
  IN BOOLEAN               OffsetPositive
  )
{
  if (((UINTN) *Pointer < TempTop) &&
    ((UINTN) *Pointer >= TempBottom)) {
    if (OffsetPositive) {
      *Pointer = (VOID *) ((UINTN) *Pointer + Offset);
    } else {
      *Pointer = (VOID *) ((UINTN) *Pointer - Offset);
    }
  }
}

/**

  Migrate Pointer in ranges of the temporary memory to PEI installed memory.

  @param SecCoreData     Points to a data structure containing SEC to PEI handoff data, such as the size
                         and location of temporary RAM, the stack location and the BFV location.
  @param PrivateData     Pointer to PeiCore's private data structure.
  @param Pointer         Pointer to the Pointer needs to be converted.

**/
VOID
ConvertPointerInRanges (
  IN CONST EFI_SEC_PEI_HAND_OFF  *SecCoreData,
  IN PEI_CORE_INSTANCE           *PrivateData,
  IN OUT VOID                    **Pointer
  )
{
  UINT8                 IndexHole;

  if (PrivateData->MemoryPages.Size != 0) {
    //
    // Convert PPI pointer in old memory pages
    // It needs to be done before Convert PPI pointer in old Heap
    //
    ConvertPointer (
      Pointer,
      (UINTN)PrivateData->MemoryPages.Base,
      (UINTN)PrivateData->MemoryPages.Base + PrivateData->MemoryPages.Size,
      PrivateData->MemoryPages.Offset,
      PrivateData->MemoryPages.OffsetPositive
      );
  }

  //
  // Convert PPI pointer in old Heap
  //
  ConvertPointer (
    Pointer,
    (UINTN)SecCoreData->PeiTemporaryRamBase,
    (UINTN)SecCoreData->PeiTemporaryRamBase + SecCoreData->PeiTemporaryRamSize,
    PrivateData->HeapOffset,
    PrivateData->HeapOffsetPositive
    );

  //
  // Convert PPI pointer in old Stack
  //
  ConvertPointer (
    Pointer,
    (UINTN)SecCoreData->StackBase,
    (UINTN)SecCoreData->StackBase + SecCoreData->StackSize,
    PrivateData->StackOffset,
    PrivateData->StackOffsetPositive
    );

  //
  // Convert PPI pointer in old TempRam Hole
  //
  for (IndexHole = 0; IndexHole < HOLE_MAX_NUMBER; IndexHole ++) {
    if (PrivateData->HoleData[IndexHole].Size == 0) {
      continue;
    }

    ConvertPointer (
      Pointer,
      (UINTN)PrivateData->HoleData[IndexHole].Base,
      (UINTN)PrivateData->HoleData[IndexHole].Base + PrivateData->HoleData[IndexHole].Size,
      PrivateData->HoleData[IndexHole].Offset,
      PrivateData->HoleData[IndexHole].OffsetPositive
      );
  }
}

/**

  Migrate Single PPI Pointer from the temporary memory to PEI installed memory.

  @param SecCoreData     Points to a data structure containing SEC to PEI handoff data, such as the size
                         and location of temporary RAM, the stack location and the BFV location.
  @param PrivateData     Pointer to PeiCore's private data structure.
  @param PpiPointer      Pointer to Ppi

**/
VOID
ConvertSinglePpiPointer (
  IN CONST EFI_SEC_PEI_HAND_OFF  *SecCoreData,
  IN PEI_CORE_INSTANCE           *PrivateData,
  IN PEI_PPI_LIST_POINTERS       *PpiPointer
  )
{
  //
  // 1. Convert the pointer to the PPI descriptor from the old TempRam
  //    to the relocated physical memory.
  // It (for the pointer to the PPI descriptor) needs to be done before 2 (for
  // the pointer to the GUID) and 3 (for the pointer to the PPI interface structure).
  //
  ConvertPointerInRanges (SecCoreData, PrivateData, &PpiPointer->Raw);
  //
  // 2. Convert the pointer to the GUID in the PPI or NOTIFY descriptor
  //    from the old TempRam to the relocated physical memory.
  //
  ConvertPointerInRanges (SecCoreData, PrivateData, (VOID **) &PpiPointer->Ppi->Guid);
  //
  // 3. Convert the pointer to the PPI interface structure in the PPI descriptor
  //    from the old TempRam to the relocated physical memory.
  //
  ConvertPointerInRanges (SecCoreData, PrivateData, (VOID **) &PpiPointer->Ppi->Ppi);
}

/**

  Migrate PPI Pointers from the temporary memory to PEI installed memory.

  @param SecCoreData     Points to a data structure containing SEC to PEI handoff data, such as the size
                         and location of temporary RAM, the stack location and the BFV location.
  @param PrivateData     Pointer to PeiCore's private data structure.

**/
VOID
ConvertPpiPointers (
  IN CONST EFI_SEC_PEI_HAND_OFF  *SecCoreData,
  IN PEI_CORE_INSTANCE           *PrivateData
  )
{
  UINT8                 Index;

  for (Index = 0; Index < PcdGet32 (PcdPeiCoreMaxPpiSupported); Index++) {
    if (Index < PrivateData->PpiData.PpiListEnd || Index > PrivateData->PpiData.NotifyListEnd) {
      ConvertSinglePpiPointer (
        SecCoreData,
        PrivateData,
        &PrivateData->PpiData.PpiListPtrs[Index]
        );
    }
  }
}

/**

  This function installs an interface in the PEI PPI database by GUID.
  The purpose of the service is to publish an interface that other parties
  can use to call additional PEIMs.

  @param PeiServices                An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param PpiList                    Pointer to a list of PEI PPI Descriptors.
  @param Single                     TRUE if only single entry in the PpiList.
                                    FALSE if the PpiList is ended with an entry which has the
                                    EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST flag set in its Flags field.

  @retval EFI_SUCCESS              if all PPIs in PpiList are successfully installed.
  @retval EFI_INVALID_PARAMETER    if PpiList is NULL pointer
                                   if any PPI in PpiList is not valid
  @retval EFI_OUT_OF_RESOURCES     if there is no more memory resource to install PPI

**/
EFI_STATUS
InternalPeiInstallPpi (
  IN CONST EFI_PEI_SERVICES        **PeiServices,
  IN CONST EFI_PEI_PPI_DESCRIPTOR  *PpiList,
  IN BOOLEAN                       Single
  )
{
  PEI_CORE_INSTANCE *PrivateData;
  INTN              Index;
  INTN              LastCallbackInstall;


  if (PpiList == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PrivateData = PEI_CORE_INSTANCE_FROM_PS_THIS(PeiServices);

  Index = PrivateData->PpiData.PpiListEnd;
  LastCallbackInstall = Index;

  //
  // This is loop installs all PPI descriptors in the PpiList.  It is terminated
  // by the EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST being set in the last
  // EFI_PEI_PPI_DESCRIPTOR in the list.
  //

  for (;;) {
    //
    // Since PpiData is used for NotifyList and PpiList, max resource
    // is reached if the Install reaches the NotifyList
    // PcdPeiCoreMaxPpiSupported can be set to a larger value in DSC to satisfy more PPI requirement.
    //
    if (Index == PrivateData->PpiData.NotifyListEnd + 1) {
      return  EFI_OUT_OF_RESOURCES;
    }
    //
    // Check if it is a valid PPI.
    // If not, rollback list to exclude all in this list.
    // Try to indicate which item failed.
    //
    if ((PpiList->Flags & EFI_PEI_PPI_DESCRIPTOR_PPI) == 0) {
      PrivateData->PpiData.PpiListEnd = LastCallbackInstall;
      DEBUG((EFI_D_ERROR, "ERROR -> InstallPpi: %g %p\n", PpiList->Guid, PpiList->Ppi));
      return  EFI_INVALID_PARAMETER;
    }

    DEBUG((EFI_D_INFO, "Install PPI: %g\n", PpiList->Guid));
    PrivateData->PpiData.PpiListPtrs[Index].Ppi = (EFI_PEI_PPI_DESCRIPTOR*) PpiList;
    PrivateData->PpiData.PpiListEnd++;

    if (Single) {
      //
      // Only single entry in the PpiList.
      //
      break;
    } else if ((PpiList->Flags & EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST) ==
               EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST) {
      //
      // Continue until the end of the PPI List.
      //
      break;
    }
    PpiList++;
    Index++;
  }

  //
  // Dispatch any callback level notifies for newly installed PPIs.
  //
  DispatchNotify (
    PrivateData,
    EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK,
    LastCallbackInstall,
    PrivateData->PpiData.PpiListEnd,
    PrivateData->PpiData.DispatchListEnd,
    PrivateData->PpiData.NotifyListEnd
    );


  return EFI_SUCCESS;
}

/**

  This function installs an interface in the PEI PPI database by GUID.
  The purpose of the service is to publish an interface that other parties
  can use to call additional PEIMs.

  @param PeiServices                An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param PpiList                    Pointer to a list of PEI PPI Descriptors.

  @retval EFI_SUCCESS              if all PPIs in PpiList are successfully installed.
  @retval EFI_INVALID_PARAMETER    if PpiList is NULL pointer
                                   if any PPI in PpiList is not valid
  @retval EFI_OUT_OF_RESOURCES     if there is no more memory resource to install PPI

**/
EFI_STATUS
EFIAPI
PeiInstallPpi (
  IN CONST EFI_PEI_SERVICES        **PeiServices,
  IN CONST EFI_PEI_PPI_DESCRIPTOR  *PpiList
  )
{
  return InternalPeiInstallPpi (PeiServices, PpiList, FALSE);
}

/**

  This function reinstalls an interface in the PEI PPI database by GUID.
  The purpose of the service is to publish an interface that other parties can
  use to replace an interface of the same name in the protocol database with a
  different interface.

  @param PeiServices            An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param OldPpi                 Pointer to the old PEI PPI Descriptors.
  @param NewPpi                 Pointer to the new PEI PPI Descriptors.

  @retval EFI_SUCCESS           if the operation was successful
  @retval EFI_INVALID_PARAMETER if OldPpi or NewPpi is NULL
  @retval EFI_INVALID_PARAMETER if NewPpi is not valid
  @retval EFI_NOT_FOUND         if the PPI was not in the database

**/
EFI_STATUS
EFIAPI
PeiReInstallPpi (
  IN CONST EFI_PEI_SERVICES        **PeiServices,
  IN CONST EFI_PEI_PPI_DESCRIPTOR  *OldPpi,
  IN CONST EFI_PEI_PPI_DESCRIPTOR  *NewPpi
  )
{
  PEI_CORE_INSTANCE   *PrivateData;
  INTN                Index;


  if ((OldPpi == NULL) || (NewPpi == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((NewPpi->Flags & EFI_PEI_PPI_DESCRIPTOR_PPI) == 0) {
    return  EFI_INVALID_PARAMETER;
  }

  PrivateData = PEI_CORE_INSTANCE_FROM_PS_THIS(PeiServices);

  //
  // Find the old PPI instance in the database.  If we can not find it,
  // return the EFI_NOT_FOUND error.
  //
  for (Index = 0; Index < PrivateData->PpiData.PpiListEnd; Index++) {
    if (OldPpi == PrivateData->PpiData.PpiListPtrs[Index].Ppi) {
      break;
    }
  }
  if (Index == PrivateData->PpiData.PpiListEnd) {
    return EFI_NOT_FOUND;
  }

  //
  // Remove the old PPI from the database, add the new one.
  //
  DEBUG((EFI_D_INFO, "Reinstall PPI: %g\n", NewPpi->Guid));
  ASSERT (Index < (INTN)(PcdGet32 (PcdPeiCoreMaxPpiSupported)));
  PrivateData->PpiData.PpiListPtrs[Index].Ppi = (EFI_PEI_PPI_DESCRIPTOR *) NewPpi;

  //
  // Dispatch any callback level notifies for the newly installed PPI.
  //
  DispatchNotify (
    PrivateData,
    EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK,
    Index,
    Index+1,
    PrivateData->PpiData.DispatchListEnd,
    PrivateData->PpiData.NotifyListEnd
    );


  return EFI_SUCCESS;
}

/**

  Locate a given named PPI.


  @param PeiServices        An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param Guid               Pointer to GUID of the PPI.
  @param Instance           Instance Number to discover.
  @param PpiDescriptor      Pointer to reference the found descriptor. If not NULL,
                            returns a pointer to the descriptor (includes flags, etc)
  @param Ppi                Pointer to reference the found PPI

  @retval EFI_SUCCESS   if the PPI is in the database
  @retval EFI_NOT_FOUND if the PPI is not in the database

**/
EFI_STATUS
EFIAPI
PeiLocatePpi (
  IN CONST EFI_PEI_SERVICES        **PeiServices,
  IN CONST EFI_GUID                *Guid,
  IN UINTN                         Instance,
  IN OUT EFI_PEI_PPI_DESCRIPTOR    **PpiDescriptor,
  IN OUT VOID                      **Ppi
  )
{
  PEI_CORE_INSTANCE   *PrivateData;
  INTN                Index;
  EFI_GUID            *CheckGuid;
  EFI_PEI_PPI_DESCRIPTOR  *TempPtr;


  PrivateData = PEI_CORE_INSTANCE_FROM_PS_THIS(PeiServices);

  //
  // Search the data base for the matching instance of the GUIDed PPI.
  //
  for (Index = 0; Index < PrivateData->PpiData.PpiListEnd; Index++) {
    TempPtr = PrivateData->PpiData.PpiListPtrs[Index].Ppi;
    CheckGuid = TempPtr->Guid;

    //
    // Don't use CompareGuid function here for performance reasons.
    // Instead we compare the GUID as INT32 at a time and branch
    // on the first failed comparison.
    //
    if ((((INT32 *)Guid)[0] == ((INT32 *)CheckGuid)[0]) &&
        (((INT32 *)Guid)[1] == ((INT32 *)CheckGuid)[1]) &&
        (((INT32 *)Guid)[2] == ((INT32 *)CheckGuid)[2]) &&
        (((INT32 *)Guid)[3] == ((INT32 *)CheckGuid)[3])) {
      if (Instance == 0) {

        if (PpiDescriptor != NULL) {
          *PpiDescriptor = TempPtr;
        }

        if (Ppi != NULL) {
          *Ppi = TempPtr->Ppi;
        }


        return EFI_SUCCESS;
      }
      Instance--;
    }
  }

  return EFI_NOT_FOUND;
}

/**

  This function installs a notification service to be called back when a given
  interface is installed or reinstalled. The purpose of the service is to publish
  an interface that other parties can use to call additional PPIs that may materialize later.

  @param PeiServices        An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param NotifyList         Pointer to list of Descriptors to notify upon.
  @param Single             TRUE if only single entry in the NotifyList.
                            FALSE if the NotifyList is ended with an entry which has the
                            EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST flag set in its Flags field.

  @retval EFI_SUCCESS           if successful
  @retval EFI_OUT_OF_RESOURCES  if no space in the database
  @retval EFI_INVALID_PARAMETER if not a good descriptor

**/
EFI_STATUS
InternalPeiNotifyPpi (
  IN CONST EFI_PEI_SERVICES           **PeiServices,
  IN CONST EFI_PEI_NOTIFY_DESCRIPTOR  *NotifyList,
  IN BOOLEAN                          Single
  )
{
  PEI_CORE_INSTANCE                *PrivateData;
  INTN                             Index;
  INTN                             NotifyIndex;
  INTN                             LastCallbackNotify;
  EFI_PEI_NOTIFY_DESCRIPTOR        *NotifyPtr;
  UINTN                            NotifyDispatchCount;


  NotifyDispatchCount = 0;

  if (NotifyList == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PrivateData = PEI_CORE_INSTANCE_FROM_PS_THIS(PeiServices);

  Index = PrivateData->PpiData.NotifyListEnd;
  LastCallbackNotify = Index;

  //
  // This is loop installs all Notify descriptors in the NotifyList.  It is
  // terminated by the EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST being set in the last
  // EFI_PEI_NOTIFY_DESCRIPTOR in the list.
  //

  for (;;) {
    //
    // Since PpiData is used for NotifyList and InstallList, max resource
    // is reached if the Install reaches the PpiList
    // PcdPeiCoreMaxPpiSupported can be set to a larger value in DSC to satisfy more Notify PPIs requirement.
    //
    if (Index == PrivateData->PpiData.PpiListEnd - 1) {
      return  EFI_OUT_OF_RESOURCES;
    }

    //
    // If some of the PPI data is invalid restore original Notify PPI database value
    //
    if ((NotifyList->Flags & EFI_PEI_PPI_DESCRIPTOR_NOTIFY_TYPES) == 0) {
        PrivateData->PpiData.NotifyListEnd = LastCallbackNotify;
        DEBUG((EFI_D_ERROR, "ERROR -> InstallNotify: %g %p\n", NotifyList->Guid, NotifyList->Notify));
      return  EFI_INVALID_PARAMETER;
    }

    if ((NotifyList->Flags & EFI_PEI_PPI_DESCRIPTOR_NOTIFY_DISPATCH) != 0) {
      NotifyDispatchCount ++;
    }

    PrivateData->PpiData.PpiListPtrs[Index].Notify = (EFI_PEI_NOTIFY_DESCRIPTOR *) NotifyList;

    PrivateData->PpiData.NotifyListEnd--;
    DEBUG((EFI_D_INFO, "Register PPI Notify: %g\n", NotifyList->Guid));
    if (Single) {
      //
      // Only single entry in the NotifyList.
      //
      break;
    } else if ((NotifyList->Flags & EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST) ==
               EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST) {
      //
      // Continue until the end of the Notify List.
      //
      break;
    }
    //
    // Go the next descriptor. Remember the NotifyList moves down.
    //
    NotifyList++;
    Index--;
  }

  //
  // If there is Dispatch Notify PPI installed put them on the bottom
  //
  if (NotifyDispatchCount > 0) {
    for (NotifyIndex = LastCallbackNotify; NotifyIndex > PrivateData->PpiData.NotifyListEnd; NotifyIndex--) {
      if ((PrivateData->PpiData.PpiListPtrs[NotifyIndex].Notify->Flags & EFI_PEI_PPI_DESCRIPTOR_NOTIFY_DISPATCH) != 0) {
        NotifyPtr = PrivateData->PpiData.PpiListPtrs[NotifyIndex].Notify;

        for (Index = NotifyIndex; Index < PrivateData->PpiData.DispatchListEnd; Index++){
          PrivateData->PpiData.PpiListPtrs[Index].Notify = PrivateData->PpiData.PpiListPtrs[Index + 1].Notify;
        }
        PrivateData->PpiData.PpiListPtrs[Index].Notify = NotifyPtr;
        PrivateData->PpiData.DispatchListEnd--;
      }
    }

    LastCallbackNotify -= NotifyDispatchCount;
  }

  //
  // Dispatch any callback level notifies for all previously installed PPIs.
  //
  DispatchNotify (
    PrivateData,
    EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK,
    0,
    PrivateData->PpiData.PpiListEnd,
    LastCallbackNotify,
    PrivateData->PpiData.NotifyListEnd
    );

  return  EFI_SUCCESS;
}

/**

  This function installs a notification service to be called back when a given
  interface is installed or reinstalled. The purpose of the service is to publish
  an interface that other parties can use to call additional PPIs that may materialize later.

  @param PeiServices        An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param NotifyList         Pointer to list of Descriptors to notify upon.

  @retval EFI_SUCCESS           if successful
  @retval EFI_OUT_OF_RESOURCES  if no space in the database
  @retval EFI_INVALID_PARAMETER if not a good descriptor

**/
EFI_STATUS
EFIAPI
PeiNotifyPpi (
  IN CONST EFI_PEI_SERVICES           **PeiServices,
  IN CONST EFI_PEI_NOTIFY_DESCRIPTOR  *NotifyList
  )
{
  return InternalPeiNotifyPpi (PeiServices, NotifyList, FALSE);
}


/**

  Process the Notify List at dispatch level.

  @param PrivateData  PeiCore's private data structure.

**/
VOID
ProcessNotifyList (
  IN PEI_CORE_INSTANCE  *PrivateData
  )
{
  INTN                    TempValue;

  while (TRUE) {
    //
    // Check if the PEIM that was just dispatched resulted in any
    // Notifies getting installed.  If so, go process any dispatch
    // level Notifies that match the previouly installed PPIs.
    // Use "while" instead of "if" since DispatchNotify can modify
    // DispatchListEnd (with NotifyPpi) so we have to iterate until the same.
    //
    while (PrivateData->PpiData.LastDispatchedNotify != PrivateData->PpiData.DispatchListEnd) {
      TempValue = PrivateData->PpiData.DispatchListEnd;
      DispatchNotify (
        PrivateData,
        EFI_PEI_PPI_DESCRIPTOR_NOTIFY_DISPATCH,
        0,
        PrivateData->PpiData.LastDispatchedInstall,
        PrivateData->PpiData.LastDispatchedNotify,
        PrivateData->PpiData.DispatchListEnd
        );
      PrivateData->PpiData.LastDispatchedNotify = TempValue;
    }


    //
    // Check if the PEIM that was just dispatched resulted in any
    // PPIs getting installed.  If so, go process any dispatch
    // level Notifies that match the installed PPIs.
    // Use "while" instead of "if" since DispatchNotify can modify
    // PpiListEnd (with InstallPpi) so we have to iterate until the same.
    //
    while (PrivateData->PpiData.LastDispatchedInstall != PrivateData->PpiData.PpiListEnd) {
      TempValue = PrivateData->PpiData.PpiListEnd;
      DispatchNotify (
        PrivateData,
        EFI_PEI_PPI_DESCRIPTOR_NOTIFY_DISPATCH,
        PrivateData->PpiData.LastDispatchedInstall,
        PrivateData->PpiData.PpiListEnd,
        PcdGet32 (PcdPeiCoreMaxPpiSupported)-1,
        PrivateData->PpiData.DispatchListEnd
        );
      PrivateData->PpiData.LastDispatchedInstall = TempValue;
    }

    if (PrivateData->PpiData.LastDispatchedNotify == PrivateData->PpiData.DispatchListEnd) {
      break;
    }
  }
  return;
}

/**

  Dispatch notifications.

  @param PrivateData        PeiCore's private data structure
  @param NotifyType         Type of notify to fire.
  @param InstallStartIndex  Install Beginning index.
  @param InstallStopIndex   Install Ending index.
  @param NotifyStartIndex   Notify Beginning index.
  @param NotifyStopIndex    Notify Ending index.

**/
VOID
DispatchNotify (
  IN PEI_CORE_INSTANCE  *PrivateData,
  IN UINTN               NotifyType,
  IN INTN                InstallStartIndex,
  IN INTN                InstallStopIndex,
  IN INTN                NotifyStartIndex,
  IN INTN                NotifyStopIndex
  )
{
  INTN                   Index1;
  INTN                   Index2;
  EFI_GUID                *SearchGuid;
  EFI_GUID                *CheckGuid;
  EFI_PEI_NOTIFY_DESCRIPTOR   *NotifyDescriptor;

  //
  // Remember that Installs moves up and Notifies moves down.
  //
  for (Index1 = NotifyStartIndex; Index1 > NotifyStopIndex; Index1--) {
    NotifyDescriptor = PrivateData->PpiData.PpiListPtrs[Index1].Notify;

    CheckGuid = NotifyDescriptor->Guid;

    for (Index2 = InstallStartIndex; Index2 < InstallStopIndex; Index2++) {
      SearchGuid = PrivateData->PpiData.PpiListPtrs[Index2].Ppi->Guid;
      //
      // Don't use CompareGuid function here for performance reasons.
      // Instead we compare the GUID as INT32 at a time and branch
      // on the first failed comparison.
      //
      if ((((INT32 *)SearchGuid)[0] == ((INT32 *)CheckGuid)[0]) &&
          (((INT32 *)SearchGuid)[1] == ((INT32 *)CheckGuid)[1]) &&
          (((INT32 *)SearchGuid)[2] == ((INT32 *)CheckGuid)[2]) &&
          (((INT32 *)SearchGuid)[3] == ((INT32 *)CheckGuid)[3])) {
        DEBUG ((EFI_D_INFO, "Notify: PPI Guid: %g, Peim notify entry point: %p\n",
          SearchGuid,
          NotifyDescriptor->Notify
          ));
        NotifyDescriptor->Notify (
                            (EFI_PEI_SERVICES **) GetPeiServicesTablePointer (),
                            NotifyDescriptor,
                            (PrivateData->PpiData.PpiListPtrs[Index2].Ppi)->Ppi
                            );
      }
    }
  }
}

/**
  Process PpiList from SEC phase.

  @param PeiServices    An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param PpiList        Points to a list of one or more PPI descriptors to be installed initially by the PEI core.
                        These PPI's will be installed and/or immediately signaled if they are notification type.

**/
VOID
ProcessPpiListFromSec (
  IN CONST EFI_PEI_SERVICES         **PeiServices,
  IN CONST EFI_PEI_PPI_DESCRIPTOR   *PpiList
  )
{
  EFI_STATUS                Status;
  EFI_SEC_HOB_DATA_PPI      *SecHobDataPpi;
  EFI_HOB_GENERIC_HEADER    *SecHobList;

  for (;;) {
    if ((PpiList->Flags & EFI_PEI_PPI_DESCRIPTOR_NOTIFY_TYPES) != 0) {
      //
      // It is a notification PPI.
      //
      Status = InternalPeiNotifyPpi (PeiServices, (CONST EFI_PEI_NOTIFY_DESCRIPTOR *) PpiList, TRUE);
      ASSERT_EFI_ERROR (Status);
    } else {
      //
      // It is a normal PPI.
      //
      Status = InternalPeiInstallPpi (PeiServices, PpiList, TRUE);
      ASSERT_EFI_ERROR (Status);
    }

    if ((PpiList->Flags & EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST) == EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST) {
      //
      // Continue until the end of the PPI List.
      //
      break;
    }

    PpiList++;
  }

  //
  // If the EFI_SEC_HOB_DATA_PPI is in the list of PPIs passed to the PEI entry point,
  // the PEI Foundation will call the GetHobs() member function and install all HOBs
  // returned into the HOB list. It does this after installing all PPIs passed from SEC
  // into the PPI database and before dispatching any PEIMs.
  //
  Status = PeiLocatePpi (PeiServices, &gEfiSecHobDataPpiGuid, 0, NULL, (VOID **) &SecHobDataPpi);
  if (!EFI_ERROR (Status)) {
    Status = SecHobDataPpi->GetHobs (SecHobDataPpi, &SecHobList);
    if (!EFI_ERROR (Status)) {
      Status = PeiInstallSecHobData (PeiServices, SecHobList);
      ASSERT_EFI_ERROR (Status);
    }
  }
}

