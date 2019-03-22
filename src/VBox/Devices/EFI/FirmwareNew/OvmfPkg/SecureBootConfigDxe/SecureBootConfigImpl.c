/** @file
  HII Config Access protocol implementation of SecureBoot configuration module.

Copyright (c) 2011 - 2012, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "SecureBootConfigImpl.h"

CHAR16              mSecureBootStorageName[] = L"SECUREBOOT_CONFIGURATION";

SECUREBOOT_CONFIG_PRIVATE_DATA         mSecureBootConfigPrivateDateTemplate = {
  SECUREBOOT_CONFIG_PRIVATE_DATA_SIGNATURE,
  {
    SecureBootExtractConfig,
    SecureBootRouteConfig,
    SecureBootCallback
  }
};

HII_VENDOR_DEVICE_PATH          mSecureBootHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8) (sizeof (VENDOR_DEVICE_PATH)),
        (UINT8) ((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    SECUREBOOT_CONFIG_FORM_SET_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8) (END_DEVICE_PATH_LENGTH),
      (UINT8) ((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};


//
// OID ASN.1 Value for Hash Algorithms
//
UINT8 mHashOidValue[] = {
  0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x02, 0x05,         // OBJ_md5
  0x2B, 0x0E, 0x03, 0x02, 0x1A,                           // OBJ_sha1
  0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04,   // OBJ_sha224
  0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01,   // OBJ_sha256
  0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02,   // OBJ_sha384
  0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03,   // OBJ_sha512
  };

HASH_TABLE mHash[] = {
  { L"SHA1",   20, &mHashOidValue[8],  5, Sha1GetContextSize,  Sha1Init,   Sha1Update,    Sha1Final  },
  { L"SHA224", 28, &mHashOidValue[13], 9, NULL,                NULL,       NULL,          NULL       },
  { L"SHA256", 32, &mHashOidValue[22], 9, Sha256GetContextSize,Sha256Init, Sha256Update,  Sha256Final},
  { L"SHA384", 48, &mHashOidValue[31], 9, NULL,                NULL,       NULL,          NULL       },
  { L"SHA512", 64, &mHashOidValue[40], 9, NULL,                NULL,       NULL,          NULL       }
};

//
// Variable Definitions
//
UINT32            mPeCoffHeaderOffset = 0;
WIN_CERTIFICATE   *mCertificate = NULL;
IMAGE_TYPE        mImageType;
UINT8             *mImageBase = NULL;
UINTN             mImageSize = 0;
UINT8             mImageDigest[MAX_DIGEST_SIZE];
UINTN             mImageDigestSize;
EFI_GUID          mCertType;
EFI_IMAGE_SECURITY_DATA_DIRECTORY    *mSecDataDir = NULL;
EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION  mNtHeader;

//
// Possible DER-encoded certificate file suffixes, end with NULL pointer.
//
CHAR16* mDerEncodedSuffix[] = {
  L".cer",
  L".der",
  L".crt",
  NULL
};
CHAR16* mSupportX509Suffix = L"*.cer/der/crt";

/**
  This code checks if the FileSuffix is one of the possible DER-encoded certificate suffix.

  @param[in] FileSuffix            The suffix of the input certificate file

  @retval    TRUE           It's a DER-encoded certificate.
  @retval    FALSE          It's NOT a DER-encoded certificate.

**/
BOOLEAN
IsDerEncodeCertificate (
  IN CONST CHAR16         *FileSuffix
)
{
  UINTN     Index;
  for (Index = 0; mDerEncodedSuffix[Index] != NULL; Index++) {
    if (StrCmp (FileSuffix, mDerEncodedSuffix[Index]) == 0) {
      return TRUE;
    }
  }
  return FALSE;
}

/**
  Set Secure Boot option into variable space.

  @param[in] VarValue              The option of Secure Boot.

  @retval    EFI_SUCCESS           The operation is finished successfully.
  @retval    Others                Other errors as indicated.

**/
EFI_STATUS
SaveSecureBootVariable (
  IN UINT8                         VarValue
  )
{
  EFI_STATUS                       Status;

  Status = gRT->SetVariable (
             EFI_SECURE_BOOT_ENABLE_NAME,
             &gEfiSecureBootEnableDisableGuid,
             EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
             sizeof (UINT8),
             &VarValue
             );
  return Status;
}

/**
  Create a time based data payload by concatenating the EFI_VARIABLE_AUTHENTICATION_2
  descriptor with the input data. NO authentication is required in this function.

  @param[in, out]   DataSize       On input, the size of Data buffer in bytes.
                                   On output, the size of data returned in Data
                                   buffer in bytes.
  @param[in, out]   Data           On input, Pointer to data buffer to be wrapped or
                                   pointer to NULL to wrap an empty payload.
                                   On output, Pointer to the new payload date buffer allocated from pool,
                                   it's caller's responsibility to free the memory when finish using it.

  @retval EFI_SUCCESS              Create time based payload successfully.
  @retval EFI_OUT_OF_RESOURCES     There are not enough memory resourses to create time based payload.
  @retval EFI_INVALID_PARAMETER    The parameter is invalid.
  @retval Others                   Unexpected error happens.

**/
EFI_STATUS
CreateTimeBasedPayload (
  IN OUT UINTN            *DataSize,
  IN OUT UINT8            **Data
  )
{
  EFI_STATUS                       Status;
  UINT8                            *NewData;
  UINT8                            *Payload;
  UINTN                            PayloadSize;
  EFI_VARIABLE_AUTHENTICATION_2    *DescriptorData;
  UINTN                            DescriptorSize;
  EFI_TIME                         Time;

  if (Data == NULL || DataSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // In Setup mode or Custom mode, the variable does not need to be signed but the
  // parameters to the SetVariable() call still need to be prepared as authenticated
  // variable. So we create EFI_VARIABLE_AUTHENTICATED_2 descriptor without certificate
  // data in it.
  //
  Payload     = *Data;
  PayloadSize = *DataSize;

  DescriptorSize    = OFFSET_OF (EFI_VARIABLE_AUTHENTICATION_2, AuthInfo) + OFFSET_OF (WIN_CERTIFICATE_UEFI_GUID, CertData);
  NewData = (UINT8*) AllocateZeroPool (DescriptorSize + PayloadSize);
  if (NewData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if ((Payload != NULL) && (PayloadSize != 0)) {
    CopyMem (NewData + DescriptorSize, Payload, PayloadSize);
  }

  DescriptorData = (EFI_VARIABLE_AUTHENTICATION_2 *) (NewData);

  ZeroMem (&Time, sizeof (EFI_TIME));
  Status = gRT->GetTime (&Time, NULL);
  if (EFI_ERROR (Status)) {
    FreePool(NewData);
    return Status;
  }
  Time.Pad1       = 0;
  Time.Nanosecond = 0;
  Time.TimeZone   = 0;
  Time.Daylight   = 0;
  Time.Pad2       = 0;
  CopyMem (&DescriptorData->TimeStamp, &Time, sizeof (EFI_TIME));

  DescriptorData->AuthInfo.Hdr.dwLength         = OFFSET_OF (WIN_CERTIFICATE_UEFI_GUID, CertData);
  DescriptorData->AuthInfo.Hdr.wRevision        = 0x0200;
  DescriptorData->AuthInfo.Hdr.wCertificateType = WIN_CERT_TYPE_EFI_GUID;
  CopyGuid (&DescriptorData->AuthInfo.CertType, &gEfiCertPkcs7Guid);

  if (Payload != NULL) {
    FreePool(Payload);
  }

  *DataSize = DescriptorSize + PayloadSize;
  *Data     = NewData;
  return EFI_SUCCESS;
}

/**
  Internal helper function to delete a Variable given its name and GUID, NO authentication
  required.

  @param[in]      VariableName            Name of the Variable.
  @param[in]      VendorGuid              GUID of the Variable.

  @retval EFI_SUCCESS              Variable deleted successfully.
  @retval Others                   The driver failed to start the device.

**/
EFI_STATUS
DeleteVariable (
  IN  CHAR16                    *VariableName,
  IN  EFI_GUID                  *VendorGuid
  )
{
  EFI_STATUS              Status;
  VOID*                   Variable;
  UINT8                   *Data;
  UINTN                   DataSize;
  UINT32                  Attr;

  GetVariable2 (VariableName, VendorGuid, &Variable, NULL);
  if (Variable == NULL) {
    return EFI_SUCCESS;
  }

  Data     = NULL;
  DataSize = 0;
  Attr     = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS
             | EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS;

  Status = CreateTimeBasedPayload (&DataSize, &Data);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Fail to create time-based data payload: %r", Status));
    return Status;
  }

  Status = gRT->SetVariable (
                  VariableName,
                  VendorGuid,
                  Attr,
                  DataSize,
                  Data
                  );
  if (Data != NULL) {
    FreePool (Data);
  }
  return Status;
}

/**
  Generate the PK signature list from the X509 Certificate storing file (.cer)

  @param[in]   X509File              FileHandle of X509 Certificate storing file.
  @param[out]  PkCert                Point to the data buffer to store the signature list.

  @return EFI_UNSUPPORTED            Unsupported Key Length.
  @return EFI_OUT_OF_RESOURCES       There are not enough memory resourses to form the signature list.

**/
EFI_STATUS
CreatePkX509SignatureList (
  IN    EFI_FILE_HANDLE             X509File,
  OUT   EFI_SIGNATURE_LIST          **PkCert
  )
{
  EFI_STATUS              Status;
  UINT8                   *X509Data;
  UINTN                   X509DataSize;
  EFI_SIGNATURE_DATA      *PkCertData;

  X509Data = NULL;
  PkCertData = NULL;
  X509DataSize = 0;

  Status = ReadFileContent (X509File, (VOID**) &X509Data, &X509DataSize, 0);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }
  ASSERT (X509Data != NULL);

  //
  // Allocate space for PK certificate list and initialize it.
  // Create PK database entry with SignatureHeaderSize equals 0.
  //
  *PkCert = (EFI_SIGNATURE_LIST*) AllocateZeroPool (
              sizeof(EFI_SIGNATURE_LIST) + sizeof(EFI_SIGNATURE_DATA) - 1
              + X509DataSize
              );
  if (*PkCert == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  (*PkCert)->SignatureListSize   = (UINT32) (sizeof(EFI_SIGNATURE_LIST)
                                    + sizeof(EFI_SIGNATURE_DATA) - 1
                                    + X509DataSize);
  (*PkCert)->SignatureSize       = (UINT32) (sizeof(EFI_SIGNATURE_DATA) - 1 + X509DataSize);
  (*PkCert)->SignatureHeaderSize = 0;
  CopyGuid (&(*PkCert)->SignatureType, &gEfiCertX509Guid);
  PkCertData                     = (EFI_SIGNATURE_DATA*) ((UINTN)(*PkCert)
                                                          + sizeof(EFI_SIGNATURE_LIST)
                                                          + (*PkCert)->SignatureHeaderSize);
  CopyGuid (&PkCertData->SignatureOwner, &gEfiGlobalVariableGuid);
  //
  // Fill the PK database with PKpub data from X509 certificate file.
  //
  CopyMem (&(PkCertData->SignatureData[0]), X509Data, X509DataSize);

ON_EXIT:

  if (X509Data != NULL) {
    FreePool (X509Data);
  }

  if (EFI_ERROR(Status) && *PkCert != NULL) {
    FreePool (*PkCert);
    *PkCert = NULL;
  }

  return Status;
}

/**
  Enroll new PK into the System without original PK's authentication.

  The SignatureOwner GUID will be the same with PK's vendorguid.

  @param[in] PrivateData     The module's private data.

  @retval   EFI_SUCCESS            New PK enrolled successfully.
  @retval   EFI_INVALID_PARAMETER  The parameter is invalid.
  @retval   EFI_OUT_OF_RESOURCES   Could not allocate needed resources.

**/
EFI_STATUS
EnrollPlatformKey (
   IN  SECUREBOOT_CONFIG_PRIVATE_DATA*   Private
  )
{
  EFI_STATUS                      Status;
  UINT32                          Attr;
  UINTN                           DataSize;
  EFI_SIGNATURE_LIST              *PkCert;
  UINT16*                         FilePostFix;
  UINTN                           NameLength;

  if (Private->FileContext->FileName == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PkCert = NULL;

  //
  // Parse the file's postfix. Only support DER encoded X.509 certificate files.
  //
  NameLength = StrLen (Private->FileContext->FileName);
  if (NameLength <= 4) {
    return EFI_INVALID_PARAMETER;
  }
  FilePostFix = Private->FileContext->FileName + NameLength - 4;
  if (!IsDerEncodeCertificate(FilePostFix)) {
    DEBUG ((EFI_D_ERROR, "Unsupported file type, only DER encoded certificate (%s) is supported.", mSupportX509Suffix));
    return EFI_INVALID_PARAMETER;
  }
  DEBUG ((EFI_D_INFO, "FileName= %s\n", Private->FileContext->FileName));
  DEBUG ((EFI_D_INFO, "FilePostFix = %s\n", FilePostFix));

  //
  // Prase the selected PK file and generature PK certificate list.
  //
  Status = CreatePkX509SignatureList (
            Private->FileContext->FHandle,
            &PkCert
            );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }
  ASSERT (PkCert != NULL);

  //
  // Set Platform Key variable.
  //
  Attr = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_RUNTIME_ACCESS
          | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS;
  DataSize = PkCert->SignatureListSize;
  Status = CreateTimeBasedPayload (&DataSize, (UINT8**) &PkCert);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Fail to create time-based data payload: %r", Status));
    goto ON_EXIT;
  }

  Status = gRT->SetVariable(
                  EFI_PLATFORM_KEY_NAME,
                  &gEfiGlobalVariableGuid,
                  Attr,
                  DataSize,
                  PkCert
                  );
  if (EFI_ERROR (Status)) {
    if (Status == EFI_OUT_OF_RESOURCES) {
      DEBUG ((EFI_D_ERROR, "Enroll PK failed with out of resource.\n"));
    }
    goto ON_EXIT;
  }

ON_EXIT:

  if (PkCert != NULL) {
    FreePool(PkCert);
  }

  if (Private->FileContext->FHandle != NULL) {
    CloseFile (Private->FileContext->FHandle);
    Private->FileContext->FHandle = NULL;
  }

  return Status;
}

/**
  Remove the PK variable.

  @retval EFI_SUCCESS    Delete PK successfully.
  @retval Others         Could not allow to delete PK.

**/
EFI_STATUS
DeletePlatformKey (
  VOID
)
{
  EFI_STATUS Status;

  Status = DeleteVariable (
             EFI_PLATFORM_KEY_NAME,
             &gEfiGlobalVariableGuid
             );
  return Status;
}

/**
  Enroll a new KEK item from public key storing file (*.pbk).

  @param[in] PrivateData           The module's private data.

  @retval   EFI_SUCCESS            New KEK enrolled successfully.
  @retval   EFI_INVALID_PARAMETER  The parameter is invalid.
  @retval   EFI_UNSUPPORTED        Unsupported command.
  @retval   EFI_OUT_OF_RESOURCES   Could not allocate needed resources.

**/
EFI_STATUS
EnrollRsa2048ToKek (
  IN SECUREBOOT_CONFIG_PRIVATE_DATA *Private
  )
{
  EFI_STATUS                      Status;
  UINT32                          Attr;
  UINTN                           DataSize;
  EFI_SIGNATURE_LIST              *KekSigList;
  UINTN                           KeyBlobSize;
  UINT8                           *KeyBlob;
  CPL_KEY_INFO                    *KeyInfo;
  EFI_SIGNATURE_DATA              *KEKSigData;
  UINTN                           KekSigListSize;
  UINT8                           *KeyBuffer;
  UINTN                           KeyLenInBytes;

  Attr        = 0;
  DataSize    = 0;
  KeyBuffer   = NULL;
  KeyBlobSize = 0;
  KeyBlob     = NULL;
  KeyInfo     = NULL;
  KEKSigData  = NULL;
  KekSigList  = NULL;
  KekSigListSize = 0;

  //
  // Form the KeKpub certificate list into EFI_SIGNATURE_LIST type.
  // First, We have to parse out public key data from the pbk key file.
  //
  Status = ReadFileContent (
             Private->FileContext->FHandle,
             (VOID**) &KeyBlob,
             &KeyBlobSize,
             0
             );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }
  ASSERT (KeyBlob != NULL);
  KeyInfo = (CPL_KEY_INFO *) KeyBlob;
  if (KeyInfo->KeyLengthInBits / 8 != WIN_CERT_UEFI_RSA2048_SIZE) {
    DEBUG ((DEBUG_ERROR, "Unsupported key length, Only RSA2048 is supported.\n"));
    Status = EFI_UNSUPPORTED;
    goto ON_EXIT;
  }

  //
  // Convert the Public key to fix octet string format represented in RSA PKCS#1.
  //
  KeyLenInBytes = KeyInfo->KeyLengthInBits / 8;
  KeyBuffer = AllocateZeroPool (KeyLenInBytes);
  if (KeyBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }
  Int2OctStr (
    (UINTN*) (KeyBlob + sizeof (CPL_KEY_INFO)),
    KeyLenInBytes / sizeof (UINTN),
    KeyBuffer,
    KeyLenInBytes
    );
  CopyMem(KeyBlob + sizeof(CPL_KEY_INFO), KeyBuffer, KeyLenInBytes);

  //
  // Form an new EFI_SIGNATURE_LIST.
  //
  KekSigListSize = sizeof(EFI_SIGNATURE_LIST)
                     + sizeof(EFI_SIGNATURE_DATA) - 1
                     + WIN_CERT_UEFI_RSA2048_SIZE;

  KekSigList = (EFI_SIGNATURE_LIST*) AllocateZeroPool (KekSigListSize);
  if (KekSigList == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  KekSigList->SignatureListSize   = sizeof(EFI_SIGNATURE_LIST)
                                  + sizeof(EFI_SIGNATURE_DATA) - 1
                                  + WIN_CERT_UEFI_RSA2048_SIZE;
  KekSigList->SignatureHeaderSize = 0;
  KekSigList->SignatureSize = sizeof(EFI_SIGNATURE_DATA) - 1 + WIN_CERT_UEFI_RSA2048_SIZE;
  CopyGuid (&KekSigList->SignatureType, &gEfiCertRsa2048Guid);

  KEKSigData = (EFI_SIGNATURE_DATA*)((UINT8*)KekSigList + sizeof(EFI_SIGNATURE_LIST));
  CopyGuid (&KEKSigData->SignatureOwner, Private->SignatureGUID);
  CopyMem (
    KEKSigData->SignatureData,
    KeyBlob + sizeof(CPL_KEY_INFO),
    WIN_CERT_UEFI_RSA2048_SIZE
    );

  //
  // Check if KEK entry has been already existed.
  // If true, use EFI_VARIABLE_APPEND_WRITE attribute to append the
  // new KEK to original variable.
  //
  Attr = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_RUNTIME_ACCESS
         | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS;
  Status = CreateTimeBasedPayload (&KekSigListSize, (UINT8**) &KekSigList);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Fail to create time-based data payload: %r", Status));
    goto ON_EXIT;
  }

  Status = gRT->GetVariable(
                  EFI_KEY_EXCHANGE_KEY_NAME,
                  &gEfiGlobalVariableGuid,
                  NULL,
                  &DataSize,
                  NULL
                  );
  if (Status == EFI_BUFFER_TOO_SMALL) {
    Attr |= EFI_VARIABLE_APPEND_WRITE;
  } else if (Status != EFI_NOT_FOUND) {
    goto ON_EXIT;
  }

  //
  // Done. Now we have formed the correct KEKpub database item, just set it into variable storage,
  //
  Status = gRT->SetVariable(
                  EFI_KEY_EXCHANGE_KEY_NAME,
                  &gEfiGlobalVariableGuid,
                  Attr,
                  KekSigListSize,
                  KekSigList
                  );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

ON_EXIT:

  CloseFile (Private->FileContext->FHandle);
  Private->FileContext->FHandle = NULL;
  Private->FileContext->FileName = NULL;

  if (Private->SignatureGUID != NULL) {
    FreePool (Private->SignatureGUID);
    Private->SignatureGUID = NULL;
  }

  if (KeyBlob != NULL) {
    FreePool (KeyBlob);
  }
  if (KeyBuffer != NULL) {
    FreePool (KeyBuffer);
  }
  if (KekSigList != NULL) {
    FreePool (KekSigList);
  }

  return Status;
}

/**
  Enroll a new KEK item from X509 certificate file.

  @param[in] PrivateData           The module's private data.

  @retval   EFI_SUCCESS            New X509 is enrolled successfully.
  @retval   EFI_INVALID_PARAMETER  The parameter is invalid.
  @retval   EFI_UNSUPPORTED        Unsupported command.
  @retval   EFI_OUT_OF_RESOURCES   Could not allocate needed resources.

**/
EFI_STATUS
EnrollX509ToKek (
  IN SECUREBOOT_CONFIG_PRIVATE_DATA *Private
  )
{
  EFI_STATUS                        Status;
  UINTN                             X509DataSize;
  VOID                              *X509Data;
  EFI_SIGNATURE_DATA                *KEKSigData;
  EFI_SIGNATURE_LIST                *KekSigList;
  UINTN                             DataSize;
  UINTN                             KekSigListSize;
  UINT32                            Attr;

  X509Data       = NULL;
  X509DataSize   = 0;
  KekSigList     = NULL;
  KekSigListSize = 0;
  DataSize       = 0;
  KEKSigData     = NULL;

  Status = ReadFileContent (
             Private->FileContext->FHandle,
             &X509Data,
             &X509DataSize,
             0
             );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }
  ASSERT (X509Data != NULL);

  KekSigListSize = sizeof(EFI_SIGNATURE_LIST) + sizeof(EFI_SIGNATURE_DATA) - 1 + X509DataSize;
  KekSigList = (EFI_SIGNATURE_LIST*) AllocateZeroPool (KekSigListSize);
  if (KekSigList == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  //
  // Fill Certificate Database parameters.
  //
  KekSigList->SignatureListSize   = (UINT32) KekSigListSize;
  KekSigList->SignatureHeaderSize = 0;
  KekSigList->SignatureSize = (UINT32) (sizeof(EFI_SIGNATURE_DATA) - 1 + X509DataSize);
  CopyGuid (&KekSigList->SignatureType, &gEfiCertX509Guid);

  KEKSigData = (EFI_SIGNATURE_DATA*) ((UINT8*) KekSigList + sizeof (EFI_SIGNATURE_LIST));
  CopyGuid (&KEKSigData->SignatureOwner, Private->SignatureGUID);
  CopyMem (KEKSigData->SignatureData, X509Data, X509DataSize);

  //
  // Check if KEK been already existed.
  // If true, use EFI_VARIABLE_APPEND_WRITE attribute to append the
  // new kek to original variable
  //
  Attr = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_RUNTIME_ACCESS
          | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS;
  Status = CreateTimeBasedPayload (&KekSigListSize, (UINT8**) &KekSigList);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Fail to create time-based data payload: %r", Status));
    goto ON_EXIT;
  }

  Status = gRT->GetVariable(
                  EFI_KEY_EXCHANGE_KEY_NAME,
                  &gEfiGlobalVariableGuid,
                  NULL,
                  &DataSize,
                  NULL
                  );
  if (Status == EFI_BUFFER_TOO_SMALL) {
    Attr |= EFI_VARIABLE_APPEND_WRITE;
  } else if (Status != EFI_NOT_FOUND) {
    goto ON_EXIT;
  }

  Status = gRT->SetVariable(
                  EFI_KEY_EXCHANGE_KEY_NAME,
                  &gEfiGlobalVariableGuid,
                  Attr,
                  KekSigListSize,
                  KekSigList
                  );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

ON_EXIT:

  CloseFile (Private->FileContext->FHandle);
  Private->FileContext->FileName = NULL;
  Private->FileContext->FHandle = NULL;

  if (Private->SignatureGUID != NULL) {
    FreePool (Private->SignatureGUID);
    Private->SignatureGUID = NULL;
  }

  if (KekSigList != NULL) {
    FreePool (KekSigList);
  }

  return Status;
}

/**
  Enroll new KEK into the System without PK's authentication.
  The SignatureOwner GUID will be Private->SignatureGUID.

  @param[in] PrivateData     The module's private data.

  @retval   EFI_SUCCESS            New KEK enrolled successful.
  @retval   EFI_INVALID_PARAMETER  The parameter is invalid.
  @retval   others                 Fail to enroll KEK data.

**/
EFI_STATUS
EnrollKeyExchangeKey (
  IN SECUREBOOT_CONFIG_PRIVATE_DATA *Private
  )
{
  UINT16*     FilePostFix;
  UINTN       NameLength;

  if ((Private->FileContext->FileName == NULL) || (Private->SignatureGUID == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Parse the file's postfix. Supports DER-encoded X509 certificate,
  // and .pbk as RSA public key file.
  //
  NameLength = StrLen (Private->FileContext->FileName);
  if (NameLength <= 4) {
    return EFI_INVALID_PARAMETER;
  }
  FilePostFix = Private->FileContext->FileName + NameLength - 4;
  if (IsDerEncodeCertificate(FilePostFix)) {
    return EnrollX509ToKek (Private);
  } else if (CompareMem (FilePostFix, L".pbk",4) == 0) {
    return EnrollRsa2048ToKek (Private);
  } else {
    return EFI_INVALID_PARAMETER;
  }
}

/**
  Enroll a new X509 certificate into Signature Database (DB or DBX) without
  KEK's authentication.

  @param[in] PrivateData     The module's private data.
  @param[in] VariableName    Variable name of signature database, must be
                             EFI_IMAGE_SECURITY_DATABASE or EFI_IMAGE_SECURITY_DATABASE1.

  @retval   EFI_SUCCESS            New X509 is enrolled successfully.
  @retval   EFI_OUT_OF_RESOURCES   Could not allocate needed resources.

**/
EFI_STATUS
EnrollX509toSigDB (
  IN SECUREBOOT_CONFIG_PRIVATE_DATA *Private,
  IN CHAR16                         *VariableName
  )
{
  EFI_STATUS                        Status;
  UINTN                             X509DataSize;
  VOID                              *X509Data;
  EFI_SIGNATURE_LIST                *SigDBCert;
  EFI_SIGNATURE_DATA                *SigDBCertData;
  VOID                              *Data;
  UINTN                             DataSize;
  UINTN                             SigDBSize;
  UINT32                            Attr;

  X509DataSize  = 0;
  SigDBSize     = 0;
  DataSize      = 0;
  X509Data      = NULL;
  SigDBCert     = NULL;
  SigDBCertData = NULL;
  Data          = NULL;

  Status = ReadFileContent (
             Private->FileContext->FHandle,
             &X509Data,
             &X509DataSize,
             0
             );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }
  ASSERT (X509Data != NULL);

  SigDBSize = sizeof(EFI_SIGNATURE_LIST) + sizeof(EFI_SIGNATURE_DATA) - 1 + X509DataSize;

  Data = AllocateZeroPool (SigDBSize);
  if (Data == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  //
  // Fill Certificate Database parameters.
  //
  SigDBCert = (EFI_SIGNATURE_LIST*) Data;
  SigDBCert->SignatureListSize   = (UINT32) SigDBSize;
  SigDBCert->SignatureHeaderSize = 0;
  SigDBCert->SignatureSize = (UINT32) (sizeof(EFI_SIGNATURE_DATA) - 1 + X509DataSize);
  CopyGuid (&SigDBCert->SignatureType, &gEfiCertX509Guid);

  SigDBCertData = (EFI_SIGNATURE_DATA*) ((UINT8* ) SigDBCert + sizeof (EFI_SIGNATURE_LIST));
  CopyGuid (&SigDBCertData->SignatureOwner, Private->SignatureGUID);
  CopyMem ((UINT8* ) (SigDBCertData->SignatureData), X509Data, X509DataSize);

  //
  // Check if signature database entry has been already existed.
  // If true, use EFI_VARIABLE_APPEND_WRITE attribute to append the
  // new signature data to original variable
  //
  Attr = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_RUNTIME_ACCESS
          | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS;
  Status = CreateTimeBasedPayload (&SigDBSize, (UINT8**) &Data);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Fail to create time-based data payload: %r", Status));
    goto ON_EXIT;
  }

  Status = gRT->GetVariable(
                  VariableName,
                  &gEfiImageSecurityDatabaseGuid,
                  NULL,
                  &DataSize,
                  NULL
                  );
  if (Status == EFI_BUFFER_TOO_SMALL) {
    Attr |= EFI_VARIABLE_APPEND_WRITE;
  } else if (Status != EFI_NOT_FOUND) {
    goto ON_EXIT;
  }

  Status = gRT->SetVariable(
                  VariableName,
                  &gEfiImageSecurityDatabaseGuid,
                  Attr,
                  SigDBSize,
                  Data
                  );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

ON_EXIT:

  CloseFile (Private->FileContext->FHandle);
  Private->FileContext->FileName = NULL;
  Private->FileContext->FHandle = NULL;

  if (Private->SignatureGUID != NULL) {
    FreePool (Private->SignatureGUID);
    Private->SignatureGUID = NULL;
  }

  if (Data != NULL) {
    FreePool (Data);
  }

  if (X509Data != NULL) {
    FreePool (X509Data);
  }

  return Status;
}

/**
  Load PE/COFF image information into internal buffer and check its validity.

  @retval   EFI_SUCCESS         Successful
  @retval   EFI_UNSUPPORTED     Invalid PE/COFF file
  @retval   EFI_ABORTED         Serious error occurs, like file I/O error etc.

**/
EFI_STATUS
LoadPeImage (
  VOID
  )
{
  EFI_IMAGE_DOS_HEADER                  *DosHdr;
  EFI_IMAGE_NT_HEADERS32                *NtHeader32;
  EFI_IMAGE_NT_HEADERS64                *NtHeader64;

  NtHeader32 = NULL;
  NtHeader64 = NULL;
  //
  // Read the Dos header
  //
  DosHdr = (EFI_IMAGE_DOS_HEADER*)(mImageBase);
  if (DosHdr->e_magic == EFI_IMAGE_DOS_SIGNATURE)
  {
    //
    // DOS image header is present,
    // So read the PE header after the DOS image header
    //
    mPeCoffHeaderOffset = DosHdr->e_lfanew;
  }
  else
  {
    mPeCoffHeaderOffset = 0;
  }

  //
  // Read PE header and check the signature validity and machine compatibility
  //
  NtHeader32 = (EFI_IMAGE_NT_HEADERS32*) (mImageBase + mPeCoffHeaderOffset);
  if (NtHeader32->Signature != EFI_IMAGE_NT_SIGNATURE)
  {
    return EFI_UNSUPPORTED;
  }

  mNtHeader.Pe32 = NtHeader32;

  //
  // Check the architecture field of PE header and get the Certificate Data Directory data
  // Note the size of FileHeader field is constant for both IA32 and X64 arch
  //
  if ((NtHeader32->FileHeader.Machine == EFI_IMAGE_MACHINE_IA32)
      || (NtHeader32->FileHeader.Machine == EFI_IMAGE_MACHINE_EBC)) {
    //
    // IA-32 Architecture
    //
    mImageType = ImageType_IA32;
    mSecDataDir = (EFI_IMAGE_SECURITY_DATA_DIRECTORY*) &(NtHeader32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY]);
  }
  else if ((NtHeader32->FileHeader.Machine == EFI_IMAGE_MACHINE_IA64)
          || (NtHeader32->FileHeader.Machine == EFI_IMAGE_MACHINE_X64)) {
    //
    // 64-bits Architecture
    //
    mImageType = ImageType_X64;
    NtHeader64 = (EFI_IMAGE_NT_HEADERS64 *) (mImageBase + mPeCoffHeaderOffset);
    mSecDataDir = (EFI_IMAGE_SECURITY_DATA_DIRECTORY*) &(NtHeader64->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY]);
  } else {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Calculate hash of Pe/Coff image based on the authenticode image hashing in
  PE/COFF Specification 8.0 Appendix A

  @param[in]    HashAlg   Hash algorithm type.

  @retval TRUE            Successfully hash image.
  @retval FALSE           Fail in hash image.

**/
BOOLEAN
HashPeImage (
  IN  UINT32                HashAlg
  )
{
  BOOLEAN                   Status;
  UINT16                    Magic;
  EFI_IMAGE_SECTION_HEADER  *Section;
  VOID                      *HashCtx;
  UINTN                     CtxSize;
  UINT8                     *HashBase;
  UINTN                     HashSize;
  UINTN                     SumOfBytesHashed;
  EFI_IMAGE_SECTION_HEADER  *SectionHeader;
  UINTN                     Index;
  UINTN                     Pos;

  HashCtx       = NULL;
  SectionHeader = NULL;
  Status        = FALSE;

  if ((HashAlg != HASHALG_SHA1) && (HashAlg != HASHALG_SHA256)) {
    return FALSE;
  }

  //
  // Initialize context of hash.
  //
  ZeroMem (mImageDigest, MAX_DIGEST_SIZE);

  if (HashAlg == HASHALG_SHA1) {
    mImageDigestSize  = SHA1_DIGEST_SIZE;
    mCertType         = gEfiCertSha1Guid;
  } else if (HashAlg == HASHALG_SHA256) {
    mImageDigestSize  = SHA256_DIGEST_SIZE;
    mCertType         = gEfiCertSha256Guid;
  }

  CtxSize   = mHash[HashAlg].GetContextSize();

  HashCtx = AllocatePool (CtxSize);
  ASSERT (HashCtx != NULL);

  // 1.  Load the image header into memory.

  // 2.  Initialize a SHA hash context.
  Status = mHash[HashAlg].HashInit(HashCtx);
  if (!Status) {
    goto Done;
  }
  //
  // Measuring PE/COFF Image Header;
  // But CheckSum field and SECURITY data directory (certificate) are excluded
  //
  if (mNtHeader.Pe32->FileHeader.Machine == IMAGE_FILE_MACHINE_IA64 && mNtHeader.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    //
    // NOTE: Some versions of Linux ELILO for Itanium have an incorrect magic value
    //       in the PE/COFF Header. If the MachineType is Itanium(IA64) and the
    //       Magic value in the OptionalHeader is EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC
    //       then override the magic value to EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC
    //
    Magic = EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC;
  } else {
    //
    // Get the magic value from the PE/COFF Optional Header
    //
    Magic = mNtHeader.Pe32->OptionalHeader.Magic;
  }

  //
  // 3.  Calculate the distance from the base of the image header to the image checksum address.
  // 4.  Hash the image header from its base to beginning of the image checksum.
  //
  HashBase = mImageBase;
  if (Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    //
    // Use PE32 offset.
    //
    HashSize = (UINTN) ((UINT8 *) (&mNtHeader.Pe32->OptionalHeader.CheckSum) - HashBase);
  } else {
    //
    // Use PE32+ offset.
    //
    HashSize = (UINTN) ((UINT8 *) (&mNtHeader.Pe32Plus->OptionalHeader.CheckSum) - HashBase);
  }

  Status  = mHash[HashAlg].HashUpdate(HashCtx, HashBase, HashSize);
  if (!Status) {
    goto Done;
  }
  //
  // 5.  Skip over the image checksum (it occupies a single ULONG).
  // 6.  Get the address of the beginning of the Cert Directory.
  // 7.  Hash everything from the end of the checksum to the start of the Cert Directory.
  //
  if (Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    //
    // Use PE32 offset.
    //
    HashBase = (UINT8 *) &mNtHeader.Pe32->OptionalHeader.CheckSum + sizeof (UINT32);
    HashSize = (UINTN) ((UINT8 *) (&mNtHeader.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY]) - HashBase);
  } else {
    //
    // Use PE32+ offset.
    //
    HashBase = (UINT8 *) &mNtHeader.Pe32Plus->OptionalHeader.CheckSum + sizeof (UINT32);
    HashSize = (UINTN) ((UINT8 *) (&mNtHeader.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY]) - HashBase);
  }

  Status  = mHash[HashAlg].HashUpdate(HashCtx, HashBase, HashSize);
  if (!Status) {
    goto Done;
  }
  //
  // 8.  Skip over the Cert Directory. (It is sizeof(IMAGE_DATA_DIRECTORY) bytes.)
  // 9.  Hash everything from the end of the Cert Directory to the end of image header.
  //
  if (Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    //
    // Use PE32 offset
    //
    HashBase = (UINT8 *) &mNtHeader.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY + 1];
    HashSize = mNtHeader.Pe32->OptionalHeader.SizeOfHeaders - (UINTN) ((UINT8 *) (&mNtHeader.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY + 1]) - mImageBase);
  } else {
    //
    // Use PE32+ offset.
    //
    HashBase = (UINT8 *) &mNtHeader.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY + 1];
    HashSize = mNtHeader.Pe32Plus->OptionalHeader.SizeOfHeaders - (UINTN) ((UINT8 *) (&mNtHeader.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY + 1]) - mImageBase);
  }

  Status  = mHash[HashAlg].HashUpdate(HashCtx, HashBase, HashSize);
  if (!Status) {
    goto Done;
  }
  //
  // 10. Set the SUM_OF_BYTES_HASHED to the size of the header.
  //
  if (Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    //
    // Use PE32 offset.
    //
    SumOfBytesHashed = mNtHeader.Pe32->OptionalHeader.SizeOfHeaders;
  } else {
    //
    // Use PE32+ offset
    //
    SumOfBytesHashed = mNtHeader.Pe32Plus->OptionalHeader.SizeOfHeaders;
  }

  //
  // 11. Build a temporary table of pointers to all the IMAGE_SECTION_HEADER
  //     structures in the image. The 'NumberOfSections' field of the image
  //     header indicates how big the table should be. Do not include any
  //     IMAGE_SECTION_HEADERs in the table whose 'SizeOfRawData' field is zero.
  //
  SectionHeader = (EFI_IMAGE_SECTION_HEADER *) AllocateZeroPool (sizeof (EFI_IMAGE_SECTION_HEADER) * mNtHeader.Pe32->FileHeader.NumberOfSections);
  ASSERT (SectionHeader != NULL);
  //
  // 12.  Using the 'PointerToRawData' in the referenced section headers as
  //      a key, arrange the elements in the table in ascending order. In other
  //      words, sort the section headers according to the disk-file offset of
  //      the section.
  //
  Section = (EFI_IMAGE_SECTION_HEADER *) (
               mImageBase +
               mPeCoffHeaderOffset +
               sizeof (UINT32) +
               sizeof (EFI_IMAGE_FILE_HEADER) +
               mNtHeader.Pe32->FileHeader.SizeOfOptionalHeader
               );
  for (Index = 0; Index < mNtHeader.Pe32->FileHeader.NumberOfSections; Index++) {
    Pos = Index;
    while ((Pos > 0) && (Section->PointerToRawData < SectionHeader[Pos - 1].PointerToRawData)) {
      CopyMem (&SectionHeader[Pos], &SectionHeader[Pos - 1], sizeof (EFI_IMAGE_SECTION_HEADER));
      Pos--;
    }
    CopyMem (&SectionHeader[Pos], Section, sizeof (EFI_IMAGE_SECTION_HEADER));
    Section += 1;
  }

  //
  // 13.  Walk through the sorted table, bring the corresponding section
  //      into memory, and hash the entire section (using the 'SizeOfRawData'
  //      field in the section header to determine the amount of data to hash).
  // 14.  Add the section's 'SizeOfRawData' to SUM_OF_BYTES_HASHED .
  // 15.  Repeat steps 13 and 14 for all the sections in the sorted table.
  //
  for (Index = 0; Index < mNtHeader.Pe32->FileHeader.NumberOfSections; Index++) {
    Section = &SectionHeader[Index];
    if (Section->SizeOfRawData == 0) {
      continue;
    }
    HashBase  = mImageBase + Section->PointerToRawData;
    HashSize  = (UINTN) Section->SizeOfRawData;

    Status  = mHash[HashAlg].HashUpdate(HashCtx, HashBase, HashSize);
    if (!Status) {
      goto Done;
    }

    SumOfBytesHashed += HashSize;
  }

  //
  // 16.  If the file size is greater than SUM_OF_BYTES_HASHED, there is extra
  //      data in the file that needs to be added to the hash. This data begins
  //      at file offset SUM_OF_BYTES_HASHED and its length is:
  //             FileSize  -  (CertDirectory->Size)
  //
  if (mImageSize > SumOfBytesHashed) {
    HashBase = mImageBase + SumOfBytesHashed;
    if (Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
      //
      // Use PE32 offset.
      //
      HashSize = (UINTN)(
                 mImageSize -
                 mNtHeader.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].Size -
                 SumOfBytesHashed);
    } else {
      //
      // Use PE32+ offset.
      //
      HashSize = (UINTN)(
                 mImageSize -
                 mNtHeader.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].Size -
                 SumOfBytesHashed);
    }

    Status  = mHash[HashAlg].HashUpdate(HashCtx, HashBase, HashSize);
    if (!Status) {
      goto Done;
    }
  }

  Status  = mHash[HashAlg].HashFinal(HashCtx, mImageDigest);

Done:
  if (HashCtx != NULL) {
    FreePool (HashCtx);
  }
  if (SectionHeader != NULL) {
    FreePool (SectionHeader);
  }
  return Status;
}

/**
  Recognize the Hash algorithm in PE/COFF Authenticode and caculate hash of
  Pe/Coff image based on the authenticated image hashing in PE/COFF Specification
  8.0 Appendix A

  @retval EFI_UNSUPPORTED             Hash algorithm is not supported.
  @retval EFI_SUCCESS                 Hash successfully.

**/
EFI_STATUS
HashPeImageByType (
  VOID
  )
{
  UINT8                     Index;
  WIN_CERTIFICATE_EFI_PKCS  *PkcsCertData;

  PkcsCertData = (WIN_CERTIFICATE_EFI_PKCS *) (mImageBase + mSecDataDir->Offset);

  for (Index = 0; Index < HASHALG_MAX; Index++) {
    //
    // Check the Hash algorithm in PE/COFF Authenticode.
    //    According to PKCS#7 Definition:
    //        SignedData ::= SEQUENCE {
    //            version Version,
    //            digestAlgorithms DigestAlgorithmIdentifiers,
    //            contentInfo ContentInfo,
    //            .... }
    //    The DigestAlgorithmIdentifiers can be used to determine the hash algorithm in PE/COFF hashing
    //    This field has the fixed offset (+32) in final Authenticode ASN.1 data.
    //    Fixed offset (+32) is calculated based on two bytes of length encoding.
     //
    if ((*(PkcsCertData->CertData + 1) & TWO_BYTE_ENCODE) != TWO_BYTE_ENCODE) {
      //
      // Only support two bytes of Long Form of Length Encoding.
      //
      continue;
    }

    //
    if (CompareMem (PkcsCertData->CertData + 32, mHash[Index].OidValue, mHash[Index].OidLength) == 0) {
      break;
    }
  }

  if (Index == HASHALG_MAX) {
    return EFI_UNSUPPORTED;
  }

  //
  // HASH PE Image based on Hash algorithm in PE/COFF Authenticode.
  //
  if (!HashPeImage(Index)) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Enroll a new executable's signature into Signature Database.

  @param[in] PrivateData     The module's private data.
  @param[in] VariableName    Variable name of signature database, must be
                             EFI_IMAGE_SECURITY_DATABASE or EFI_IMAGE_SECURITY_DATABASE1.

  @retval   EFI_SUCCESS            New signature is enrolled successfully.
  @retval   EFI_INVALID_PARAMETER  The parameter is invalid.
  @retval   EFI_UNSUPPORTED        Unsupported command.
  @retval   EFI_OUT_OF_RESOURCES   Could not allocate needed resources.

**/
EFI_STATUS
EnrollImageSignatureToSigDB (
  IN SECUREBOOT_CONFIG_PRIVATE_DATA *Private,
  IN CHAR16                         *VariableName
  )
{
  EFI_STATUS                        Status;
  EFI_SIGNATURE_LIST                *SigDBCert;
  EFI_SIGNATURE_DATA                *SigDBCertData;
  VOID                              *Data;
  UINTN                             DataSize;
  UINTN                             SigDBSize;
  UINT32                            Attr;
  WIN_CERTIFICATE_UEFI_GUID         *GuidCertData;

  Data = NULL;
  GuidCertData = NULL;

  //
  // Form the SigDB certificate list.
  // Format the data item into EFI_SIGNATURE_LIST type.
  //
  // We need to parse executable's signature data from specified signed executable file.
  // In current implementation, we simply trust the pass-in signed executable file.
  // In reality, it's OS's responsibility to verify the signed executable file.
  //

  //
  // Read the whole file content
  //
  Status = ReadFileContent(
             Private->FileContext->FHandle,
             (VOID **) &mImageBase,
             &mImageSize,
             0
             );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }
  ASSERT (mImageBase != NULL);

  Status = LoadPeImage ();
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  if (mSecDataDir->SizeOfCert == 0) {
    if (!HashPeImage (HASHALG_SHA256)) {
      Status =  EFI_SECURITY_VIOLATION;
      goto ON_EXIT;
    }
  } else {

    //
    // Read the certificate data
    //
    mCertificate = (WIN_CERTIFICATE *)(mImageBase + mSecDataDir->Offset);

    if (mCertificate->wCertificateType == WIN_CERT_TYPE_EFI_GUID) {
      GuidCertData = (WIN_CERTIFICATE_UEFI_GUID*) mCertificate;
      if (CompareMem (&GuidCertData->CertType, &gEfiCertTypeRsa2048Sha256Guid, sizeof(EFI_GUID)) != 0) {
        Status = EFI_ABORTED;
        goto ON_EXIT;
      }

      if (!HashPeImage (HASHALG_SHA256)) {
        Status = EFI_ABORTED;
        goto ON_EXIT;;
      }

    } else if (mCertificate->wCertificateType == WIN_CERT_TYPE_PKCS_SIGNED_DATA) {

      Status = HashPeImageByType ();
      if (EFI_ERROR (Status)) {
        goto ON_EXIT;;
      }
    } else {
      Status = EFI_ABORTED;
      goto ON_EXIT;
    }
  }

  //
  // Create a new SigDB entry.
  //
  SigDBSize = sizeof(EFI_SIGNATURE_LIST)
              + sizeof(EFI_SIGNATURE_DATA) - 1
              + (UINT32) mImageDigestSize;

  Data = (UINT8*) AllocateZeroPool (SigDBSize);
  if (Data == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  //
  // Adjust the Certificate Database parameters.
  //
  SigDBCert = (EFI_SIGNATURE_LIST*) Data;
  SigDBCert->SignatureListSize   = (UINT32) SigDBSize;
  SigDBCert->SignatureHeaderSize = 0;
  SigDBCert->SignatureSize       = sizeof(EFI_SIGNATURE_DATA) - 1 + (UINT32) mImageDigestSize;
  CopyGuid (&SigDBCert->SignatureType, &mCertType);

  SigDBCertData = (EFI_SIGNATURE_DATA*)((UINT8*)SigDBCert + sizeof(EFI_SIGNATURE_LIST));
  CopyGuid (&SigDBCertData->SignatureOwner, Private->SignatureGUID);
  CopyMem (SigDBCertData->SignatureData, mImageDigest, mImageDigestSize);

  Attr = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_RUNTIME_ACCESS
          | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS;
  Status = CreateTimeBasedPayload (&SigDBSize, (UINT8**) &Data);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Fail to create time-based data payload: %r", Status));
    goto ON_EXIT;
  }

  //
  // Check if SigDB variable has been already existed.
  // If true, use EFI_VARIABLE_APPEND_WRITE attribute to append the
  // new signature data to original variable
  //
  DataSize = 0;
  Status = gRT->GetVariable(
                  VariableName,
                  &gEfiImageSecurityDatabaseGuid,
                  NULL,
                  &DataSize,
                  NULL
                  );
  if (Status == EFI_BUFFER_TOO_SMALL) {
    Attr |= EFI_VARIABLE_APPEND_WRITE;
  } else if (Status != EFI_NOT_FOUND) {
    goto ON_EXIT;
  }

  //
  // Enroll the variable.
  //
  Status = gRT->SetVariable(
                  VariableName,
                  &gEfiImageSecurityDatabaseGuid,
                  Attr,
                  SigDBSize,
                  Data
                  );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

ON_EXIT:

  CloseFile (Private->FileContext->FHandle);
  Private->FileContext->FHandle = NULL;
  Private->FileContext->FileName = NULL;

  if (Private->SignatureGUID != NULL) {
    FreePool (Private->SignatureGUID);
    Private->SignatureGUID = NULL;
  }

  if (Data != NULL) {
    FreePool (Data);
  }

  if (mImageBase != NULL) {
    FreePool (mImageBase);
    mImageBase = NULL;
  }

  return Status;
}

/**
  Enroll signature into DB/DBX without KEK's authentication.
  The SignatureOwner GUID will be Private->SignatureGUID.

  @param[in] PrivateData     The module's private data.
  @param[in] VariableName    Variable name of signature database, must be
                             EFI_IMAGE_SECURITY_DATABASE or EFI_IMAGE_SECURITY_DATABASE1.

  @retval   EFI_SUCCESS            New signature enrolled successfully.
  @retval   EFI_INVALID_PARAMETER  The parameter is invalid.
  @retval   others                 Fail to enroll signature data.

**/
EFI_STATUS
EnrollSignatureDatabase (
  IN SECUREBOOT_CONFIG_PRIVATE_DATA     *Private,
  IN CHAR16                             *VariableName
  )
{
  UINT16*      FilePostFix;
  UINTN        NameLength;

  if ((Private->FileContext->FileName == NULL) || (Private->FileContext->FHandle == NULL) || (Private->SignatureGUID == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Parse the file's postfix.
  //
  NameLength = StrLen (Private->FileContext->FileName);
  if (NameLength <= 4) {
    return EFI_INVALID_PARAMETER;
  }
  FilePostFix = Private->FileContext->FileName + NameLength - 4;
  if (IsDerEncodeCertificate(FilePostFix)) {
    //
    // Supports DER-encoded X509 certificate.
    //
    return EnrollX509toSigDB (Private, VariableName);
  }

  return EnrollImageSignatureToSigDB (Private, VariableName);
}

/**
  List all signatures in specified signature database (e.g. KEK/DB/DBX)
  by GUID in the page for user to select and delete as needed.

  @param[in]    PrivateData         Module's private data.
  @param[in]    VariableName        The variable name of the vendor's signature database.
  @param[in]    VendorGuid          A unique identifier for the vendor.
  @param[in]    LabelNumber         Label number to insert opcodes.
  @param[in]    FormId              Form ID of current page.
  @param[in]    QuestionIdBase      Base question id of the signature list.

  @retval   EFI_SUCCESS             Success to update the signature list page
  @retval   EFI_OUT_OF_RESOURCES    Unable to allocate required resources.

**/
EFI_STATUS
UpdateDeletePage (
  IN SECUREBOOT_CONFIG_PRIVATE_DATA   *PrivateData,
  IN CHAR16                           *VariableName,
  IN EFI_GUID                         *VendorGuid,
  IN UINT16                           LabelNumber,
  IN EFI_FORM_ID                      FormId,
  IN EFI_QUESTION_ID                  QuestionIdBase
  )
{
  EFI_STATUS                  Status;
  UINT32                      Index;
  UINTN                       CertCount;
  UINTN                       GuidIndex;
  VOID                        *StartOpCodeHandle;
  VOID                        *EndOpCodeHandle;
  EFI_IFR_GUID_LABEL          *StartLabel;
  EFI_IFR_GUID_LABEL          *EndLabel;
  UINTN                       DataSize;
  UINT8                       *Data;
  EFI_SIGNATURE_LIST          *CertList;
  EFI_SIGNATURE_DATA          *Cert;
  UINT32                      ItemDataSize;
  CHAR16                      *GuidStr;
  EFI_STRING_ID               GuidID;
  EFI_STRING_ID               Help;

  Data     = NULL;
  CertList = NULL;
  Cert     = NULL;
  GuidStr  = NULL;
  StartOpCodeHandle = NULL;
  EndOpCodeHandle   = NULL;

  //
  // Initialize the container for dynamic opcodes.
  //
  StartOpCodeHandle = HiiAllocateOpCodeHandle ();
  if (StartOpCodeHandle == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  EndOpCodeHandle = HiiAllocateOpCodeHandle ();
  if (EndOpCodeHandle == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  //
  // Create Hii Extend Label OpCode.
  //
  StartLabel = (EFI_IFR_GUID_LABEL *) HiiCreateGuidOpCode (
                                        StartOpCodeHandle,
                                        &gEfiIfrTianoGuid,
                                        NULL,
                                        sizeof (EFI_IFR_GUID_LABEL)
                                        );
  StartLabel->ExtendOpCode  = EFI_IFR_EXTEND_OP_LABEL;
  StartLabel->Number        = LabelNumber;

  EndLabel = (EFI_IFR_GUID_LABEL *) HiiCreateGuidOpCode (
                                      EndOpCodeHandle,
                                      &gEfiIfrTianoGuid,
                                      NULL,
                                      sizeof (EFI_IFR_GUID_LABEL)
                                      );
  EndLabel->ExtendOpCode  = EFI_IFR_EXTEND_OP_LABEL;
  EndLabel->Number        = LABEL_END;

  //
  // Read Variable.
  //
  DataSize = 0;
  Status = gRT->GetVariable (VariableName, VendorGuid, NULL, &DataSize, Data);
  if (EFI_ERROR (Status) && Status != EFI_BUFFER_TOO_SMALL) {
    goto ON_EXIT;
  }

  Data = (UINT8 *) AllocateZeroPool (DataSize);
  if (Data == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  Status = gRT->GetVariable (VariableName, VendorGuid, NULL, &DataSize, Data);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  GuidStr = AllocateZeroPool (100);
  if (GuidStr == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  //
  // Enumerate all KEK pub data.
  //
  ItemDataSize = (UINT32) DataSize;
  CertList = (EFI_SIGNATURE_LIST *) Data;
  GuidIndex = 0;

  while ((ItemDataSize > 0) && (ItemDataSize >= CertList->SignatureListSize)) {

    if (CompareGuid (&CertList->SignatureType, &gEfiCertRsa2048Guid)) {
      Help = STRING_TOKEN (STR_CERT_TYPE_RSA2048_SHA256_GUID);
    } else if (CompareGuid (&CertList->SignatureType, &gEfiCertX509Guid)) {
      Help = STRING_TOKEN (STR_CERT_TYPE_PCKS7_GUID);
    } else if (CompareGuid (&CertList->SignatureType, &gEfiCertSha1Guid)) {
      Help = STRING_TOKEN (STR_CERT_TYPE_SHA1_GUID);
    } else if (CompareGuid (&CertList->SignatureType, &gEfiCertSha256Guid)) {
      Help = STRING_TOKEN (STR_CERT_TYPE_SHA256_GUID);
    } else {
      //
      // The signature type is not supported in current implementation.
      //
      continue;
    }

    CertCount  = (CertList->SignatureListSize - sizeof (EFI_SIGNATURE_LIST) - CertList->SignatureHeaderSize) / CertList->SignatureSize;
    for (Index = 0; Index < CertCount; Index++) {
      Cert = (EFI_SIGNATURE_DATA *) ((UINT8 *) CertList
                                              + sizeof (EFI_SIGNATURE_LIST)
                                              + CertList->SignatureHeaderSize
                                              + Index * CertList->SignatureSize);
      //
      // Display GUID and help
      //
      GuidToString (&Cert->SignatureOwner, GuidStr, 100);
      GuidID  = HiiSetString (PrivateData->HiiHandle, 0, GuidStr, NULL);
      HiiCreateCheckBoxOpCode (
        StartOpCodeHandle,
        (EFI_QUESTION_ID) (QuestionIdBase + GuidIndex++),
        0,
        0,
        GuidID,
        Help,
        EFI_IFR_FLAG_CALLBACK,
        0,
        NULL
        );
    }

    ItemDataSize -= CertList->SignatureListSize;
    CertList = (EFI_SIGNATURE_LIST *) ((UINT8 *) CertList + CertList->SignatureListSize);
  }

ON_EXIT:
  HiiUpdateForm (
    PrivateData->HiiHandle,
    &gSecureBootConfigFormSetGuid,
    FormId,
    StartOpCodeHandle,
    EndOpCodeHandle
    );

  if (StartOpCodeHandle != NULL) {
    HiiFreeOpCodeHandle (StartOpCodeHandle);
  }

  if (EndOpCodeHandle != NULL) {
    HiiFreeOpCodeHandle (EndOpCodeHandle);
  }

  if (Data != NULL) {
    FreePool (Data);
  }

  if (GuidStr != NULL) {
    FreePool (GuidStr);
  }

  return EFI_SUCCESS;
}

/**
  Delete a KEK entry from KEK database.

  @param[in]    PrivateData         Module's private data.
  @param[in]    QuestionId          Question id of the KEK item to delete.

  @retval   EFI_SUCCESS            Delete kek item successfully.
  @retval   EFI_OUT_OF_RESOURCES   Could not allocate needed resources.

**/
EFI_STATUS
DeleteKeyExchangeKey (
  IN SECUREBOOT_CONFIG_PRIVATE_DATA   *PrivateData,
  IN EFI_QUESTION_ID                  QuestionId
  )
{
  EFI_STATUS                  Status;
  UINTN                       DataSize;
  UINT8                       *Data;
  UINT8                       *OldData;
  UINT32                      Attr;
  UINT32                      Index;
  EFI_SIGNATURE_LIST          *CertList;
  EFI_SIGNATURE_LIST          *NewCertList;
  EFI_SIGNATURE_DATA          *Cert;
  UINTN                       CertCount;
  UINT32                      Offset;
  BOOLEAN                     IsKEKItemFound;
  UINT32                      KekDataSize;
  UINTN                       DeleteKekIndex;
  UINTN                       GuidIndex;

  Data            = NULL;
  OldData         = NULL;
  CertList        = NULL;
  Cert            = NULL;
  Attr            = 0;
  DeleteKekIndex  = QuestionId - OPTION_DEL_KEK_QUESTION_ID;

  //
  // Get original KEK variable.
  //
  DataSize = 0;
  Status = gRT->GetVariable (EFI_KEY_EXCHANGE_KEY_NAME, &gEfiGlobalVariableGuid, NULL, &DataSize, NULL);
  if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL) {
    goto ON_EXIT;
  }

  OldData = (UINT8*)AllocateZeroPool(DataSize);
  if (OldData == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  Status = gRT->GetVariable (EFI_KEY_EXCHANGE_KEY_NAME, &gEfiGlobalVariableGuid, &Attr, &DataSize, OldData);
  if (EFI_ERROR(Status)) {
    goto ON_EXIT;
  }

  //
  // Allocate space for new variable.
  //
  Data = (UINT8*) AllocateZeroPool (DataSize);
  if (Data == NULL) {
    Status  =  EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  //
  // Enumerate all KEK pub data and erasing the target item.
  //
  IsKEKItemFound = FALSE;
  KekDataSize = (UINT32) DataSize;
  CertList = (EFI_SIGNATURE_LIST *) OldData;
  Offset = 0;
  GuidIndex = 0;
  while ((KekDataSize > 0) && (KekDataSize >= CertList->SignatureListSize)) {
    if (CompareGuid (&CertList->SignatureType, &gEfiCertRsa2048Guid) ||
        CompareGuid (&CertList->SignatureType, &gEfiCertX509Guid)) {
      CopyMem (Data + Offset, CertList, (sizeof(EFI_SIGNATURE_LIST) + CertList->SignatureHeaderSize));
      NewCertList = (EFI_SIGNATURE_LIST *)(Data + Offset);
      Offset += (sizeof(EFI_SIGNATURE_LIST) + CertList->SignatureHeaderSize);
      Cert      = (EFI_SIGNATURE_DATA *) ((UINT8 *) CertList + sizeof (EFI_SIGNATURE_LIST) + CertList->SignatureHeaderSize);
      CertCount  = (CertList->SignatureListSize - sizeof (EFI_SIGNATURE_LIST) - CertList->SignatureHeaderSize) / CertList->SignatureSize;
      for (Index = 0; Index < CertCount; Index++) {
        if (GuidIndex == DeleteKekIndex ) {
          //
          // Find it! Skip it!
          //
          NewCertList->SignatureListSize -= CertList->SignatureSize;
          IsKEKItemFound = TRUE;
        } else {
          //
          // This item doesn't match. Copy it to the Data buffer.
          //
          CopyMem (Data + Offset, Cert, CertList->SignatureSize);
          Offset += CertList->SignatureSize;
        }
        GuidIndex++;
        Cert = (EFI_SIGNATURE_DATA *) ((UINT8*) Cert + CertList->SignatureSize);
      }
    } else {
      //
      // This List doesn't match. Copy it to the Data buffer.
      //
      CopyMem (Data + Offset, CertList, CertList->SignatureListSize);
      Offset += CertList->SignatureListSize;
    }

    KekDataSize -= CertList->SignatureListSize;
    CertList = (EFI_SIGNATURE_LIST*) ((UINT8*) CertList + CertList->SignatureListSize);
  }

  if (!IsKEKItemFound) {
    //
    // Doesn't find the Kek Item!
    //
    Status = EFI_NOT_FOUND;
    goto ON_EXIT;
  }

  //
  // Delete the Signature header if there is no signature in the list.
  //
  KekDataSize = Offset;
  CertList = (EFI_SIGNATURE_LIST*) Data;
  Offset = 0;
  ZeroMem (OldData, KekDataSize);
  while ((KekDataSize > 0) && (KekDataSize >= CertList->SignatureListSize)) {
    CertCount  = (CertList->SignatureListSize - sizeof (EFI_SIGNATURE_LIST) - CertList->SignatureHeaderSize) / CertList->SignatureSize;
    DEBUG ((DEBUG_ERROR, "       CertCount = %x\n", CertCount));
    if (CertCount != 0) {
      CopyMem (OldData + Offset, CertList, CertList->SignatureListSize);
      Offset += CertList->SignatureListSize;
    }
    KekDataSize -= CertList->SignatureListSize;
    CertList = (EFI_SIGNATURE_LIST *) ((UINT8 *) CertList + CertList->SignatureListSize);
  }

  DataSize = Offset;
  if ((Attr & EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) != 0) {
    Status = CreateTimeBasedPayload (&DataSize, &OldData);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Fail to create time-based data payload: %r", Status));
      goto ON_EXIT;
    }
  }

  Status = gRT->SetVariable(
                  EFI_KEY_EXCHANGE_KEY_NAME,
                  &gEfiGlobalVariableGuid,
                  Attr,
                  DataSize,
                  OldData
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to set variable, Status = %r\n", Status));
    goto ON_EXIT;
  }

ON_EXIT:
  if (Data != NULL) {
    FreePool(Data);
  }

  if (OldData != NULL) {
    FreePool(OldData);
  }

  return UpdateDeletePage (
           PrivateData,
           EFI_KEY_EXCHANGE_KEY_NAME,
           &gEfiGlobalVariableGuid,
           LABEL_KEK_DELETE,
           FORMID_DELETE_KEK_FORM,
           OPTION_DEL_KEK_QUESTION_ID
           );
}

/**
  Delete a signature entry from siganture database.

  @param[in]    PrivateData         Module's private data.
  @param[in]    VariableName        The variable name of the vendor's signature database.
  @param[in]    VendorGuid          A unique identifier for the vendor.
  @param[in]    LabelNumber         Label number to insert opcodes.
  @param[in]    FormId              Form ID of current page.
  @param[in]    QuestionIdBase      Base question id of the signature list.
  @param[in]    DeleteIndex         Signature index to delete.

  @retval   EFI_SUCCESS             Delete siganture successfully.
  @retval   EFI_NOT_FOUND           Can't find the signature item,
  @retval   EFI_OUT_OF_RESOURCES    Could not allocate needed resources.
**/
EFI_STATUS
DeleteSignature (
  IN SECUREBOOT_CONFIG_PRIVATE_DATA   *PrivateData,
  IN CHAR16                           *VariableName,
  IN EFI_GUID                         *VendorGuid,
  IN UINT16                           LabelNumber,
  IN EFI_FORM_ID                      FormId,
  IN EFI_QUESTION_ID                  QuestionIdBase,
  IN UINTN                            DeleteIndex
  )
{
  EFI_STATUS                  Status;
  UINTN                       DataSize;
  UINT8                       *Data;
  UINT8                       *OldData;
  UINT32                      Attr;
  UINT32                      Index;
  EFI_SIGNATURE_LIST          *CertList;
  EFI_SIGNATURE_LIST          *NewCertList;
  EFI_SIGNATURE_DATA          *Cert;
  UINTN                       CertCount;
  UINT32                      Offset;
  BOOLEAN                     IsItemFound;
  UINT32                      ItemDataSize;
  UINTN                       GuidIndex;

  Data            = NULL;
  OldData         = NULL;
  CertList        = NULL;
  Cert            = NULL;
  Attr            = 0;

  //
  // Get original signature list data.
  //
  DataSize = 0;
  Status = gRT->GetVariable (VariableName, VendorGuid, NULL, &DataSize, NULL);
  if (EFI_ERROR (Status) && Status != EFI_BUFFER_TOO_SMALL) {
    goto ON_EXIT;
  }

  OldData = (UINT8 *) AllocateZeroPool (DataSize);
  if (OldData == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  Status = gRT->GetVariable (VariableName, VendorGuid, &Attr, &DataSize, OldData);
  if (EFI_ERROR(Status)) {
    goto ON_EXIT;
  }

  //
  // Allocate space for new variable.
  //
  Data = (UINT8*) AllocateZeroPool (DataSize);
  if (Data == NULL) {
    Status  =  EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  //
  // Enumerate all signature data and erasing the target item.
  //
  IsItemFound = FALSE;
  ItemDataSize = (UINT32) DataSize;
  CertList = (EFI_SIGNATURE_LIST *) OldData;
  Offset = 0;
  GuidIndex = 0;
  while ((ItemDataSize > 0) && (ItemDataSize >= CertList->SignatureListSize)) {
    if (CompareGuid (&CertList->SignatureType, &gEfiCertRsa2048Guid) ||
        CompareGuid (&CertList->SignatureType, &gEfiCertX509Guid) ||
        CompareGuid (&CertList->SignatureType, &gEfiCertSha1Guid) ||
        CompareGuid (&CertList->SignatureType, &gEfiCertSha256Guid)
        ) {
      //
      // Copy EFI_SIGNATURE_LIST header then calculate the signature count in this list.
      //
      CopyMem (Data + Offset, CertList, (sizeof(EFI_SIGNATURE_LIST) + CertList->SignatureHeaderSize));
      NewCertList = (EFI_SIGNATURE_LIST*) (Data + Offset);
      Offset += (sizeof(EFI_SIGNATURE_LIST) + CertList->SignatureHeaderSize);
      Cert      = (EFI_SIGNATURE_DATA *) ((UINT8 *) CertList + sizeof (EFI_SIGNATURE_LIST) + CertList->SignatureHeaderSize);
      CertCount  = (CertList->SignatureListSize - sizeof (EFI_SIGNATURE_LIST) - CertList->SignatureHeaderSize) / CertList->SignatureSize;
      for (Index = 0; Index < CertCount; Index++) {
        if (GuidIndex == DeleteIndex) {
          //
          // Find it! Skip it!
          //
          NewCertList->SignatureListSize -= CertList->SignatureSize;
          IsItemFound = TRUE;
        } else {
          //
          // This item doesn't match. Copy it to the Data buffer.
          //
          CopyMem (Data + Offset, (UINT8*)(Cert), CertList->SignatureSize);
          Offset += CertList->SignatureSize;
        }
        GuidIndex++;
        Cert = (EFI_SIGNATURE_DATA *) ((UINT8 *) Cert + CertList->SignatureSize);
      }
    } else {
      //
      // This List doesn't match. Just copy it to the Data buffer.
      //
      CopyMem (Data + Offset, (UINT8*)(CertList), CertList->SignatureListSize);
      Offset += CertList->SignatureListSize;
    }

    ItemDataSize -= CertList->SignatureListSize;
    CertList = (EFI_SIGNATURE_LIST *) ((UINT8 *) CertList + CertList->SignatureListSize);
  }

  if (!IsItemFound) {
    //
    // Doesn't find the signature Item!
    //
    Status = EFI_NOT_FOUND;
    goto ON_EXIT;
  }

  //
  // Delete the EFI_SIGNATURE_LIST header if there is no signature in the list.
  //
  ItemDataSize = Offset;
  CertList = (EFI_SIGNATURE_LIST *) Data;
  Offset = 0;
  ZeroMem (OldData, ItemDataSize);
  while ((ItemDataSize > 0) && (ItemDataSize >= CertList->SignatureListSize)) {
    CertCount  = (CertList->SignatureListSize - sizeof (EFI_SIGNATURE_LIST) - CertList->SignatureHeaderSize) / CertList->SignatureSize;
    DEBUG ((DEBUG_ERROR, "       CertCount = %x\n", CertCount));
    if (CertCount != 0) {
      CopyMem (OldData + Offset, (UINT8*)(CertList), CertList->SignatureListSize);
      Offset += CertList->SignatureListSize;
    }
    ItemDataSize -= CertList->SignatureListSize;
    CertList = (EFI_SIGNATURE_LIST *) ((UINT8 *) CertList + CertList->SignatureListSize);
  }

  DataSize = Offset;
  if ((Attr & EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) != 0) {
    Status = CreateTimeBasedPayload (&DataSize, &OldData);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Fail to create time-based data payload: %r", Status));
      goto ON_EXIT;
    }
  }

  Status = gRT->SetVariable(
                  VariableName,
                  VendorGuid,
                  Attr,
                  DataSize,
                  OldData
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to set variable, Status = %r\n", Status));
    goto ON_EXIT;
  }

ON_EXIT:
  if (Data != NULL) {
    FreePool(Data);
  }

  if (OldData != NULL) {
    FreePool(OldData);
  }

  return UpdateDeletePage (
           PrivateData,
           VariableName,
           VendorGuid,
           LabelNumber,
           FormId,
           QuestionIdBase
           );
}

/**
  This function extracts configuration from variable.

  @param[in, out]  ConfigData   Point to SecureBoot configuration private data.

**/
VOID
SecureBootExtractConfigFromVariable (
  IN OUT SECUREBOOT_CONFIGURATION    *ConfigData
  )
{
  UINT8   *SecureBootEnable;
  UINT8   *SetupMode;
  UINT8   *SecureBoot;
  UINT8   *SecureBootMode;

  SecureBootEnable = NULL;
  SetupMode        = NULL;
  SecureBoot       = NULL;
  SecureBootMode   = NULL;

  //
  // If the SecureBootEnable Variable doesn't exist, hide the SecureBoot Enable/Disable
  // Checkbox.
  //
  GetVariable2 (EFI_SECURE_BOOT_ENABLE_NAME, &gEfiSecureBootEnableDisableGuid, (VOID**)&SecureBootEnable, NULL);
  if (SecureBootEnable == NULL) {
    ConfigData->HideSecureBoot = TRUE;
  } else {
    ConfigData->HideSecureBoot = FALSE;
  }

  //
  // If it is Physical Presence User, set the PhysicalPresent to true.
  //
  if (UserPhysicalPresent()) {
    ConfigData->PhysicalPresent = TRUE;
  } else {
    ConfigData->PhysicalPresent = FALSE;
  }

  //
  // If there is no PK then the Delete Pk button will be gray.
  //
  GetVariable2 (EFI_SETUP_MODE_NAME, &gEfiGlobalVariableGuid, (VOID**)&SetupMode, NULL);
  if (SetupMode == NULL || (*SetupMode) == SETUP_MODE) {
    ConfigData->HasPk = FALSE;
  } else  {
    ConfigData->HasPk = TRUE;
  }

  //
  // If the value of SecureBoot variable is 1, the platform is operating in secure boot mode.
  //
  GetVariable2 (EFI_SECURE_BOOT_MODE_NAME, &gEfiGlobalVariableGuid, (VOID**)&SecureBoot, NULL);
  if (SecureBoot != NULL && *SecureBoot == SECURE_BOOT_MODE_ENABLE) {
    ConfigData->SecureBootState = TRUE;
  } else {
    ConfigData->SecureBootState = FALSE;
  }

  //
  // Get the SecureBootMode from CustomMode variable.
  //
  GetVariable2 (EFI_CUSTOM_MODE_NAME, &gEfiCustomModeEnableGuid, (VOID**)&SecureBootMode, NULL);
  if (SecureBootMode == NULL) {
    ConfigData->SecureBootMode = STANDARD_SECURE_BOOT_MODE;
  } else {
    ConfigData->SecureBootMode = *(SecureBootMode);
  }

}

/**
  This function allows a caller to extract the current configuration for one
  or more named elements from the target driver.

  @param[in]   This              Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param[in]   Request           A null-terminated Unicode string in
                                 <ConfigRequest> format.
  @param[out]  Progress          On return, points to a character in the Request
                                 string. Points to the string's null terminator if
                                 request was successful. Points to the most recent
                                 '&' before the first failing name/value pair (or
                                 the beginning of the string if the failure is in
                                 the first name/value pair) if the request was not
                                 successful.
  @param[out]  Results           A null-terminated Unicode string in
                                 <ConfigAltResp> format which has all values filled
                                 in for the names in the Request string. String to
                                 be allocated by the called function.

  @retval EFI_SUCCESS            The Results is filled with the requested values.
  @retval EFI_OUT_OF_RESOURCES   Not enough memory to store the results.
  @retval EFI_INVALID_PARAMETER  Request is illegal syntax, or unknown name.
  @retval EFI_NOT_FOUND          Routing data doesn't match any storage in this
                                 driver.

**/
EFI_STATUS
EFIAPI
SecureBootExtractConfig (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL        *This,
  IN CONST EFI_STRING                            Request,
       OUT EFI_STRING                            *Progress,
       OUT EFI_STRING                            *Results
  )
{
  EFI_STATUS                        Status;
  UINTN                             BufferSize;
  UINTN                             Size;
  SECUREBOOT_CONFIGURATION          Configuration;
  EFI_STRING                        ConfigRequest;
  EFI_STRING                        ConfigRequestHdr;
  SECUREBOOT_CONFIG_PRIVATE_DATA    *PrivateData;
  BOOLEAN                           AllocatedRequest;

  if (Progress == NULL || Results == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  AllocatedRequest = FALSE;
  ConfigRequestHdr = NULL;
  ConfigRequest    = NULL;
  Size             = 0;

  ZeroMem (&Configuration, sizeof (Configuration));
  PrivateData      = SECUREBOOT_CONFIG_PRIVATE_FROM_THIS (This);
  *Progress        = Request;

  if ((Request != NULL) && !HiiIsConfigHdrMatch (Request, &gSecureBootConfigFormSetGuid, mSecureBootStorageName)) {
    return EFI_NOT_FOUND;
  }

  //
  // Get Configuration from Variable.
  //
  SecureBootExtractConfigFromVariable (&Configuration);

  BufferSize = sizeof (SECUREBOOT_CONFIGURATION);
  ConfigRequest = Request;
  if ((Request == NULL) || (StrStr (Request, L"OFFSET") == NULL)) {
    //
    // Request is set to NULL or OFFSET is NULL, construct full request string.
    //
    // Allocate and fill a buffer large enough to hold the <ConfigHdr> template
    // followed by "&OFFSET=0&WIDTH=WWWWWWWWWWWWWWWW" followed by a Null-terminator
    //
    ConfigRequestHdr = HiiConstructConfigHdr (&gSecureBootConfigFormSetGuid, mSecureBootStorageName, PrivateData->DriverHandle);
    Size = (StrLen (ConfigRequestHdr) + 32 + 1) * sizeof (CHAR16);
    ConfigRequest = AllocateZeroPool (Size);
    ASSERT (ConfigRequest != NULL);
    AllocatedRequest = TRUE;
    UnicodeSPrint (ConfigRequest, Size, L"%s&OFFSET=0&WIDTH=%016LX", ConfigRequestHdr, (UINT64)BufferSize);
    FreePool (ConfigRequestHdr);
    ConfigRequestHdr = NULL;
  }

  Status = gHiiConfigRouting->BlockToConfig (
                                gHiiConfigRouting,
                                ConfigRequest,
                                (UINT8 *) &Configuration,
                                BufferSize,
                                Results,
                                Progress
                                );

  //
  // Free the allocated config request string.
  //
  if (AllocatedRequest) {
    FreePool (ConfigRequest);
  }

  //
  // Set Progress string to the original request string.
  //
  if (Request == NULL) {
    *Progress = NULL;
  } else if (StrStr (Request, L"OFFSET") == NULL) {
    *Progress = Request + StrLen (Request);
  }

  return Status;
}

/**
  This function processes the results of changes in configuration.

  @param[in]  This               Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param[in]  Configuration      A null-terminated Unicode string in <ConfigResp>
                                 format.
  @param[out] Progress           A pointer to a string filled in with the offset of
                                 the most recent '&' before the first failing
                                 name/value pair (or the beginning of the string if
                                 the failure is in the first name/value pair) or
                                 the terminating NULL if all was successful.

  @retval EFI_SUCCESS            The Results is processed successfully.
  @retval EFI_INVALID_PARAMETER  Configuration is NULL.
  @retval EFI_NOT_FOUND          Routing data doesn't match any storage in this
                                 driver.

**/
EFI_STATUS
EFIAPI
SecureBootRouteConfig (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL      *This,
  IN CONST EFI_STRING                          Configuration,
       OUT EFI_STRING                          *Progress
  )
{
  if (Configuration == NULL || Progress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *Progress = Configuration;
  if (!HiiIsConfigHdrMatch (Configuration, &gSecureBootConfigFormSetGuid, mSecureBootStorageName)) {
    return EFI_NOT_FOUND;
  }

  *Progress = Configuration + StrLen (Configuration);
  return EFI_SUCCESS;
}

/**
  This function is called to provide results data to the driver.

  @param[in]  This               Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param[in]  Action             Specifies the type of action taken by the browser.
  @param[in]  QuestionId         A unique value which is sent to the original
                                 exporting driver so that it can identify the type
                                 of data to expect.
  @param[in]  Type               The type of value for the question.
  @param[in]  Value              A pointer to the data being sent to the original
                                 exporting driver.
  @param[out] ActionRequest      On return, points to the action requested by the
                                 callback function.

  @retval EFI_SUCCESS            The callback successfully handled the action.
  @retval EFI_OUT_OF_RESOURCES   Not enough storage is available to hold the
                                 variable and its data.
  @retval EFI_DEVICE_ERROR       The variable could not be saved.
  @retval EFI_UNSUPPORTED        The specified Action is not supported by the
                                 callback.

**/
EFI_STATUS
EFIAPI
SecureBootCallback (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL      *This,
  IN     EFI_BROWSER_ACTION                    Action,
  IN     EFI_QUESTION_ID                       QuestionId,
  IN     UINT8                                 Type,
  IN     EFI_IFR_TYPE_VALUE                    *Value,
     OUT EFI_BROWSER_ACTION_REQUEST            *ActionRequest
  )
{
  EFI_INPUT_KEY                   Key;
  EFI_STATUS                      Status;
  SECUREBOOT_CONFIG_PRIVATE_DATA  *Private;
  UINTN                           BufferSize;
  SECUREBOOT_CONFIGURATION        *IfrNvData;
  UINT16                          LabelId;
  UINT8                           *SecureBootEnable;
  CHAR16                          PromptString[100];

  SecureBootEnable = NULL;

  if ((This == NULL) || (Value == NULL) || (ActionRequest == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Action != EFI_BROWSER_ACTION_CHANGED) && (Action != EFI_BROWSER_ACTION_CHANGING)) {
    return EFI_UNSUPPORTED;
  }

  Private = SECUREBOOT_CONFIG_PRIVATE_FROM_THIS (This);

  //
  // Retrieve uncommitted data from Browser
  //
  BufferSize = sizeof (SECUREBOOT_CONFIGURATION);
  IfrNvData = AllocateZeroPool (BufferSize);
  if (IfrNvData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = EFI_SUCCESS;

  HiiGetBrowserData (NULL, NULL, BufferSize, (UINT8 *) IfrNvData);

  if (Action == EFI_BROWSER_ACTION_CHANGING) {

    switch (QuestionId) {
    case KEY_SECURE_BOOT_ENABLE:
      GetVariable2 (EFI_SECURE_BOOT_ENABLE_NAME, &gEfiSecureBootEnableDisableGuid, (VOID**)&SecureBootEnable, NULL);
      if (NULL != SecureBootEnable) {
        if (EFI_ERROR (SaveSecureBootVariable (Value->u8))) {
          CreatePopUp (
            EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
            &Key,
            L"Only Physical Presence User could disable secure boot!",
            NULL
            );
          Status = EFI_UNSUPPORTED;
        }
        *ActionRequest = EFI_BROWSER_ACTION_REQUEST_FORM_APPLY;
      }
      break;

    case KEY_SECURE_BOOT_OPTION:
      FreeMenu (&DirectoryMenu);
      FreeMenu (&FsOptionMenu);
      break;

    case KEY_SECURE_BOOT_KEK_OPTION:
    case KEY_SECURE_BOOT_DB_OPTION:
    case KEY_SECURE_BOOT_DBX_OPTION:
      //
      // Clear Signature GUID.
      //
      ZeroMem (IfrNvData->SignatureGuid, sizeof (IfrNvData->SignatureGuid));
      if (Private->SignatureGUID == NULL) {
        Private->SignatureGUID = (EFI_GUID *) AllocateZeroPool (sizeof (EFI_GUID));
        if (Private->SignatureGUID == NULL) {
          return EFI_OUT_OF_RESOURCES;
        }
      }

      if (QuestionId == KEY_SECURE_BOOT_DB_OPTION) {
        LabelId = SECUREBOOT_ENROLL_SIGNATURE_TO_DB;
      } else if (QuestionId == KEY_SECURE_BOOT_DBX_OPTION) {
        LabelId = SECUREBOOT_ENROLL_SIGNATURE_TO_DBX;
      } else {
        LabelId = FORMID_ENROLL_KEK_FORM;
      }

      //
      // Refresh selected file.
      //
      CleanUpPage (LabelId, Private);
      break;

    case SECUREBOOT_ADD_PK_FILE_FORM_ID:
    case FORMID_ENROLL_KEK_FORM:
    case SECUREBOOT_ENROLL_SIGNATURE_TO_DB:
    case SECUREBOOT_ENROLL_SIGNATURE_TO_DBX:
      if (QuestionId == SECUREBOOT_ADD_PK_FILE_FORM_ID) {
        Private->FeCurrentState = FileExplorerStateEnrollPkFile;
      } else if (QuestionId == FORMID_ENROLL_KEK_FORM) {
        Private->FeCurrentState = FileExplorerStateEnrollKekFile;
      } else if (QuestionId == SECUREBOOT_ENROLL_SIGNATURE_TO_DB) {
        Private->FeCurrentState = FileExplorerStateEnrollSignatureFileToDb;
      } else {
        Private->FeCurrentState = FileExplorerStateEnrollSignatureFileToDbx;
      }

      Private->FeDisplayContext = FileExplorerDisplayUnknown;
      CleanUpPage (FORM_FILE_EXPLORER_ID, Private);
      UpdateFileExplorer (Private, 0);
      break;

    case KEY_SECURE_BOOT_DELETE_PK:
        if (Value->u8) {
          Status = DeletePlatformKey ();
          *ActionRequest = EFI_BROWSER_ACTION_REQUEST_FORM_APPLY;
        }
      break;

    case KEY_DELETE_KEK:
      UpdateDeletePage (
        Private,
        EFI_KEY_EXCHANGE_KEY_NAME,
        &gEfiGlobalVariableGuid,
        LABEL_KEK_DELETE,
        FORMID_DELETE_KEK_FORM,
        OPTION_DEL_KEK_QUESTION_ID
        );
      break;

    case SECUREBOOT_DELETE_SIGNATURE_FROM_DB:
      UpdateDeletePage (
        Private,
        EFI_IMAGE_SECURITY_DATABASE,
        &gEfiImageSecurityDatabaseGuid,
        LABEL_DB_DELETE,
        SECUREBOOT_DELETE_SIGNATURE_FROM_DB,
        OPTION_DEL_DB_QUESTION_ID
        );
       break;

    case SECUREBOOT_DELETE_SIGNATURE_FROM_DBX:
      UpdateDeletePage (
        Private,
        EFI_IMAGE_SECURITY_DATABASE1,
        &gEfiImageSecurityDatabaseGuid,
        LABEL_DBX_DELETE,
        SECUREBOOT_DELETE_SIGNATURE_FROM_DBX,
        OPTION_DEL_DBX_QUESTION_ID
        );

      break;

    case KEY_VALUE_SAVE_AND_EXIT_KEK:
      Status = EnrollKeyExchangeKey (Private);
      break;

    case KEY_VALUE_SAVE_AND_EXIT_DB:
      Status = EnrollSignatureDatabase (Private, EFI_IMAGE_SECURITY_DATABASE);
      break;

    case KEY_VALUE_SAVE_AND_EXIT_DBX:
      Status = EnrollSignatureDatabase (Private, EFI_IMAGE_SECURITY_DATABASE1);
      break;

    default:
      if (QuestionId >= FILE_OPTION_OFFSET) {
        UpdateFileExplorer (Private, QuestionId);
      } else if ((QuestionId >= OPTION_DEL_KEK_QUESTION_ID) &&
                 (QuestionId < (OPTION_DEL_KEK_QUESTION_ID + OPTION_CONFIG_RANGE))) {
        DeleteKeyExchangeKey (Private, QuestionId);
      } else if ((QuestionId >= OPTION_DEL_DB_QUESTION_ID) &&
                 (QuestionId < (OPTION_DEL_DB_QUESTION_ID + OPTION_CONFIG_RANGE))) {
        DeleteSignature (
          Private,
          EFI_IMAGE_SECURITY_DATABASE,
          &gEfiImageSecurityDatabaseGuid,
          LABEL_DB_DELETE,
          SECUREBOOT_DELETE_SIGNATURE_FROM_DB,
          OPTION_DEL_DB_QUESTION_ID,
          QuestionId - OPTION_DEL_DB_QUESTION_ID
          );
      } else if ((QuestionId >= OPTION_DEL_DBX_QUESTION_ID) &&
                 (QuestionId < (OPTION_DEL_DBX_QUESTION_ID + OPTION_CONFIG_RANGE))) {
        DeleteSignature (
          Private,
          EFI_IMAGE_SECURITY_DATABASE1,
          &gEfiImageSecurityDatabaseGuid,
          LABEL_DBX_DELETE,
          SECUREBOOT_DELETE_SIGNATURE_FROM_DBX,
          OPTION_DEL_DBX_QUESTION_ID,
          QuestionId - OPTION_DEL_DBX_QUESTION_ID
          );
      }
      break;
    }
  } else if (Action == EFI_BROWSER_ACTION_CHANGED) {
    switch (QuestionId) {
    case KEY_SECURE_BOOT_ENABLE:
      *ActionRequest = EFI_BROWSER_ACTION_REQUEST_SUBMIT;
      break;
    case KEY_VALUE_SAVE_AND_EXIT_PK:
      Status = EnrollPlatformKey (Private);
      UnicodeSPrint (
        PromptString,
        sizeof (PromptString),
        L"Only DER encoded certificate file (%s) is supported.",
        mSupportX509Suffix
        );
      if (EFI_ERROR (Status)) {
        CreatePopUp (
          EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
          &Key,
          L"ERROR: Unsupported file type!",
          PromptString,
          NULL
          );
      } else {
        *ActionRequest = EFI_BROWSER_ACTION_REQUEST_SUBMIT;
      }
      break;

    case KEY_VALUE_NO_SAVE_AND_EXIT_PK:
    case KEY_VALUE_NO_SAVE_AND_EXIT_KEK:
    case KEY_VALUE_NO_SAVE_AND_EXIT_DB:
    case KEY_VALUE_NO_SAVE_AND_EXIT_DBX:
      if (Private->FileContext->FHandle != NULL) {
        CloseFile (Private->FileContext->FHandle);
        Private->FileContext->FHandle = NULL;
        Private->FileContext->FileName = NULL;
      }

      if (Private->SignatureGUID != NULL) {
        FreePool (Private->SignatureGUID);
        Private->SignatureGUID = NULL;
      }
      *ActionRequest = EFI_BROWSER_ACTION_REQUEST_EXIT;
      break;

    case KEY_SECURE_BOOT_MODE:
      GetVariable2 (EFI_CUSTOM_MODE_NAME, &gEfiCustomModeEnableGuid, (VOID**)&SecureBootEnable, NULL);
      if (NULL != SecureBootEnable) {
        Status = gRT->SetVariable (
                        EFI_CUSTOM_MODE_NAME,
                        &gEfiCustomModeEnableGuid,
                        EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                        sizeof (UINT8),
                        &Value->u8
                        );
        *ActionRequest = EFI_BROWSER_ACTION_REQUEST_FORM_APPLY;
        IfrNvData->SecureBootMode = Value->u8;
      }
      break;

    case KEY_SECURE_BOOT_KEK_GUID:
    case KEY_SECURE_BOOT_SIGNATURE_GUID_DB:
    case KEY_SECURE_BOOT_SIGNATURE_GUID_DBX:
      ASSERT (Private->SignatureGUID != NULL);
      Status = StringToGuid (
                 IfrNvData->SignatureGuid,
                 StrLen (IfrNvData->SignatureGuid),
                 Private->SignatureGUID
                 );
      if (EFI_ERROR (Status)) {
        break;
      }

      *ActionRequest = EFI_BROWSER_ACTION_REQUEST_FORM_APPLY;
      break;

    case KEY_SECURE_BOOT_DELETE_PK:
      if (Value->u8) {
        *ActionRequest = EFI_BROWSER_ACTION_REQUEST_SUBMIT;
      }
      break;
    }
  }

  if (!EFI_ERROR (Status)) {
    BufferSize = sizeof (SECUREBOOT_CONFIGURATION);
    HiiSetBrowserData (NULL, NULL, BufferSize, (UINT8*) IfrNvData, NULL);
  }
  FreePool (IfrNvData);

  return EFI_SUCCESS;
}

/**
  This function publish the SecureBoot configuration Form.

  @param[in, out]  PrivateData   Points to SecureBoot configuration private data.

  @retval EFI_SUCCESS            HII Form is installed successfully.
  @retval EFI_OUT_OF_RESOURCES   Not enough resource for HII Form installation.
  @retval Others                 Other errors as indicated.

**/
EFI_STATUS
InstallSecureBootConfigForm (
  IN OUT SECUREBOOT_CONFIG_PRIVATE_DATA  *PrivateData
  )
{
  EFI_STATUS                      Status;
  EFI_HII_HANDLE                  HiiHandle;
  EFI_HANDLE                      DriverHandle;
  EFI_HII_CONFIG_ACCESS_PROTOCOL  *ConfigAccess;

  DriverHandle = NULL;
  ConfigAccess = &PrivateData->ConfigAccess;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &DriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mSecureBootHiiVendorDevicePath,
                  &gEfiHiiConfigAccessProtocolGuid,
                  ConfigAccess,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PrivateData->DriverHandle = DriverHandle;

  //
  // Publish the HII package list
  //
  HiiHandle = HiiAddPackages (
                &gSecureBootConfigFormSetGuid,
                DriverHandle,
                SecureBootConfigDxeStrings,
                SecureBootConfigBin,
                NULL
                );
  if (HiiHandle == NULL) {
    gBS->UninstallMultipleProtocolInterfaces (
           DriverHandle,
           &gEfiDevicePathProtocolGuid,
           &mSecureBootHiiVendorDevicePath,
           &gEfiHiiConfigAccessProtocolGuid,
           ConfigAccess,
           NULL
           );
    return EFI_OUT_OF_RESOURCES;
  }

  PrivateData->HiiHandle = HiiHandle;

  PrivateData->FileContext = AllocateZeroPool (sizeof (SECUREBOOT_FILE_CONTEXT));
  PrivateData->MenuEntry   = AllocateZeroPool (sizeof (SECUREBOOT_MENU_ENTRY));

  if (PrivateData->FileContext == NULL || PrivateData->MenuEntry == NULL) {
    UninstallSecureBootConfigForm (PrivateData);
    return EFI_OUT_OF_RESOURCES;
  }

  PrivateData->FeCurrentState = FileExplorerStateInActive;
  PrivateData->FeDisplayContext = FileExplorerDisplayUnknown;

  InitializeListHead (&FsOptionMenu.Head);
  InitializeListHead (&DirectoryMenu.Head);

  //
  // Init OpCode Handle and Allocate space for creation of Buffer
  //
  mStartOpCodeHandle = HiiAllocateOpCodeHandle ();
  if (mStartOpCodeHandle == NULL) {
    UninstallSecureBootConfigForm (PrivateData);
    return EFI_OUT_OF_RESOURCES;
  }

  mEndOpCodeHandle = HiiAllocateOpCodeHandle ();
  if (mEndOpCodeHandle == NULL) {
    UninstallSecureBootConfigForm (PrivateData);
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Create Hii Extend Label OpCode as the start opcode
  //
  mStartLabel = (EFI_IFR_GUID_LABEL *) HiiCreateGuidOpCode (
                                         mStartOpCodeHandle,
                                         &gEfiIfrTianoGuid,
                                         NULL,
                                         sizeof (EFI_IFR_GUID_LABEL)
                                         );
  mStartLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;

  //
  // Create Hii Extend Label OpCode as the end opcode
  //
  mEndLabel = (EFI_IFR_GUID_LABEL *) HiiCreateGuidOpCode (
                                       mEndOpCodeHandle,
                                       &gEfiIfrTianoGuid,
                                       NULL,
                                       sizeof (EFI_IFR_GUID_LABEL)
                                       );
  mEndLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  mEndLabel->Number       = LABEL_END;

  return EFI_SUCCESS;
}

/**
  This function removes SecureBoot configuration Form.

  @param[in, out]  PrivateData   Points to SecureBoot configuration private data.

**/
VOID
UninstallSecureBootConfigForm (
  IN OUT SECUREBOOT_CONFIG_PRIVATE_DATA    *PrivateData
  )
{
  //
  // Uninstall HII package list
  //
  if (PrivateData->HiiHandle != NULL) {
    HiiRemovePackages (PrivateData->HiiHandle);
    PrivateData->HiiHandle = NULL;
  }

  //
  // Uninstall HII Config Access Protocol
  //
  if (PrivateData->DriverHandle != NULL) {
    gBS->UninstallMultipleProtocolInterfaces (
           PrivateData->DriverHandle,
           &gEfiDevicePathProtocolGuid,
           &mSecureBootHiiVendorDevicePath,
           &gEfiHiiConfigAccessProtocolGuid,
           &PrivateData->ConfigAccess,
           NULL
           );
    PrivateData->DriverHandle = NULL;
  }

  if (PrivateData->SignatureGUID != NULL) {
    FreePool (PrivateData->SignatureGUID);
  }

  if (PrivateData->MenuEntry != NULL) {
    FreePool (PrivateData->MenuEntry);
  }

  if (PrivateData->FileContext != NULL) {
    FreePool (PrivateData->FileContext);
  }

  FreePool (PrivateData);

  FreeMenu (&DirectoryMenu);
  FreeMenu (&FsOptionMenu);

  if (mStartOpCodeHandle != NULL) {
    HiiFreeOpCodeHandle (mStartOpCodeHandle);
  }

  if (mEndOpCodeHandle != NULL) {
    HiiFreeOpCodeHandle (mEndOpCodeHandle);
  }
}
