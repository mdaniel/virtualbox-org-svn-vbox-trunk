/** @file
  Ihis library is BaseCrypto router. It will redirect hash request to each individual
  hash handler registerd, such as SHA1, SHA256.
  Platform can use PcdTpm2HashMask to mask some hash engines.

Copyright (c) 2013 - 2014, Intel Corporation. All rights reserved. <BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiPei.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/Tpm2CommandLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/HobLib.h>
#include <Library/HashLib.h>

#include "HashLibBaseCryptoRouterCommon.h"

#define HASH_LIB_PEI_ROUTER_GUID \
  { 0x84681c08, 0x6873, 0x46f3, { 0x8b, 0xb7, 0xab, 0x66, 0x18, 0x95, 0xa1, 0xb3 } }

EFI_GUID mHashLibPeiRouterGuid = HASH_LIB_PEI_ROUTER_GUID;

typedef struct {
  UINTN            HashInterfaceCount;
  HASH_INTERFACE   HashInterface[HASH_COUNT];
} HASH_INTERFACE_HOB;

/**
  This function get hash interface.

  @retval hash interface.
**/
HASH_INTERFACE_HOB *
InternalGetHashInterface (
  VOID
  )
{
  EFI_HOB_GUID_TYPE *Hob;

  Hob = GetFirstGuidHob (&mHashLibPeiRouterGuid);
  if (Hob == NULL) {
    return NULL;
  }
  return (HASH_INTERFACE_HOB *)(Hob + 1);
}

/**
  Start hash sequence.

  @param HashHandle Hash handle.

  @retval EFI_SUCCESS          Hash sequence start and HandleHandle returned.
  @retval EFI_OUT_OF_RESOURCES No enough resource to start hash.
**/
EFI_STATUS
EFIAPI
HashStart (
  OUT HASH_HANDLE    *HashHandle
  )
{
  HASH_INTERFACE_HOB *HashInterfaceHob;
  HASH_HANDLE        *HashCtx;
  UINTN              Index;

  HashInterfaceHob = InternalGetHashInterface ();
  if (HashInterfaceHob == NULL) {
    return EFI_UNSUPPORTED;
  }

  if (HashInterfaceHob->HashInterfaceCount == 0) {
    return EFI_UNSUPPORTED;
  }

  HashCtx = AllocatePool (sizeof(*HashCtx) * HashInterfaceHob->HashInterfaceCount);
  ASSERT (HashCtx != NULL);

  for (Index = 0; Index < HashInterfaceHob->HashInterfaceCount; Index++) {
    HashInterfaceHob->HashInterface[Index].HashInit (&HashCtx[Index]);
  }

  *HashHandle = (HASH_HANDLE)HashCtx;

  return EFI_SUCCESS;
}

/**
  Update hash sequence data.

  @param HashHandle    Hash handle.
  @param DataToHash    Data to be hashed.
  @param DataToHashLen Data size.

  @retval EFI_SUCCESS     Hash sequence updated.
**/
EFI_STATUS
EFIAPI
HashUpdate (
  IN HASH_HANDLE    HashHandle,
  IN VOID           *DataToHash,
  IN UINTN          DataToHashLen
  )
{
  HASH_INTERFACE_HOB *HashInterfaceHob;
  HASH_HANDLE        *HashCtx;
  UINTN              Index;

  HashInterfaceHob = InternalGetHashInterface ();
  if (HashInterfaceHob == NULL) {
    return EFI_UNSUPPORTED;
  }

  if (HashInterfaceHob->HashInterfaceCount == 0) {
    return EFI_UNSUPPORTED;
  }

  HashCtx = (HASH_HANDLE *)HashHandle;

  for (Index = 0; Index < HashInterfaceHob->HashInterfaceCount; Index++) {
    HashInterfaceHob->HashInterface[Index].HashUpdate (HashCtx[Index], DataToHash, DataToHashLen);
  }

  return EFI_SUCCESS;
}

/**
  Hash sequence complete and extend to PCR.

  @param HashHandle    Hash handle.
  @param PcrIndex      PCR to be extended.
  @param DataToHash    Data to be hashed.
  @param DataToHashLen Data size.
  @param DigestList    Digest list.

  @retval EFI_SUCCESS     Hash sequence complete and DigestList is returned.
**/
EFI_STATUS
EFIAPI
HashCompleteAndExtend (
  IN HASH_HANDLE         HashHandle,
  IN TPMI_DH_PCR         PcrIndex,
  IN VOID                *DataToHash,
  IN UINTN               DataToHashLen,
  OUT TPML_DIGEST_VALUES *DigestList
  )
{
  TPML_DIGEST_VALUES Digest;
  HASH_INTERFACE_HOB *HashInterfaceHob;
  HASH_HANDLE        *HashCtx;
  UINTN              Index;
  EFI_STATUS         Status;

  HashInterfaceHob = InternalGetHashInterface ();
  if (HashInterfaceHob == NULL) {
    return EFI_UNSUPPORTED;
  }

  if (HashInterfaceHob->HashInterfaceCount == 0) {
    return EFI_UNSUPPORTED;
  }

  HashCtx = (HASH_HANDLE *)HashHandle;
  ZeroMem (DigestList, sizeof(*DigestList));

  for (Index = 0; Index < HashInterfaceHob->HashInterfaceCount; Index++) {
    HashInterfaceHob->HashInterface[Index].HashUpdate (HashCtx[Index], DataToHash, DataToHashLen);
    HashInterfaceHob->HashInterface[Index].HashFinal (HashCtx[Index], &Digest);
    Tpm2SetHashToDigestList (DigestList, &Digest);
  }

  FreePool (HashCtx);

  Status = Tpm2PcrExtend (
             PcrIndex,
             DigestList
             );
  return Status;
}

/**
  Hash data and extend to PCR.

  @param PcrIndex      PCR to be extended.
  @param DataToHash    Data to be hashed.
  @param DataToHashLen Data size.
  @param DigestList    Digest list.

  @retval EFI_SUCCESS     Hash data and DigestList is returned.
**/
EFI_STATUS
EFIAPI
HashAndExtend (
  IN TPMI_DH_PCR                    PcrIndex,
  IN VOID                           *DataToHash,
  IN UINTN                          DataToHashLen,
  OUT TPML_DIGEST_VALUES            *DigestList
  )
{
  HASH_INTERFACE_HOB *HashInterfaceHob;
  HASH_HANDLE        HashHandle;
  EFI_STATUS         Status;

  HashInterfaceHob = InternalGetHashInterface ();
  if (HashInterfaceHob == NULL) {
    return EFI_UNSUPPORTED;
  }

  if (HashInterfaceHob->HashInterfaceCount == 0) {
    return EFI_UNSUPPORTED;
  }

  HashStart (&HashHandle);
  HashUpdate (HashHandle, DataToHash, DataToHashLen);
  Status = HashCompleteAndExtend (HashHandle, PcrIndex, NULL, 0, DigestList);

  return Status;
}

/**
  This service register Hash.

  @param HashInterface  Hash interface

  @retval EFI_SUCCESS          This hash interface is registered successfully.
  @retval EFI_UNSUPPORTED      System does not support register this interface.
  @retval EFI_ALREADY_STARTED  System already register this interface.
**/
EFI_STATUS
EFIAPI
RegisterHashInterfaceLib (
  IN HASH_INTERFACE   *HashInterface
  )
{
  UINTN              Index;
  HASH_INTERFACE_HOB *HashInterfaceHob;
  HASH_INTERFACE_HOB LocalHashInterfaceHob;
  UINT32             HashMask;

  //
  // Check allow
  //
  HashMask = Tpm2GetHashMaskFromAlgo (&HashInterface->HashGuid);
  if ((HashMask & PcdGet32 (PcdTpm2HashMask)) == 0) {
    return EFI_UNSUPPORTED;
  }

  HashInterfaceHob = InternalGetHashInterface ();
  if (HashInterfaceHob == NULL) {
    ZeroMem (&LocalHashInterfaceHob, sizeof(LocalHashInterfaceHob));
    HashInterfaceHob = BuildGuidDataHob (&mHashLibPeiRouterGuid, &LocalHashInterfaceHob, sizeof(LocalHashInterfaceHob));
    if (HashInterfaceHob == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  if (HashInterfaceHob->HashInterfaceCount >= HASH_COUNT) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Check duplication
  //
  for (Index = 0; Index < HashInterfaceHob->HashInterfaceCount; Index++) {
    if (CompareGuid (&HashInterfaceHob->HashInterface[Index].HashGuid, &HashInterface->HashGuid)) {
      //
      // In PEI phase, there will be shadow driver dispatched again.
      //
      DEBUG ((EFI_D_INFO, "RegisterHashInterfaceLib - Override\n"));
      CopyMem (&HashInterfaceHob->HashInterface[Index], HashInterface, sizeof(*HashInterface));
      return EFI_SUCCESS;
    }
  }

  CopyMem (&HashInterfaceHob->HashInterface[HashInterfaceHob->HashInterfaceCount], HashInterface, sizeof(*HashInterface));
  HashInterfaceHob->HashInterfaceCount ++;

  return EFI_SUCCESS;
}