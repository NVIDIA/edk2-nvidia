/** @file
  The main process for L4TLauncher application.

  Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/HandleParsingLib.h>
#include <Library/PrintLib.h>
#include <Library/FileHandleLib.h>
#include <Library/DevicePathLib.h>
#include <Library/AndroidBootImgLib.h>

#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/AndroidBootImg.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
#include <Protocol/LoadFile2.h>

#include <Guid/LinuxEfiInitrdMedia.h>
#include <Protocol/Pkcs7Verify.h>

#include <Guid/LinuxEfiInitrdMedia.h>
#include <Guid/AuthenticatedVariableFormat.h>
#include <Guid/ImageAuthentication.h>

#include <UefiSecureBoot.h>
#include <Library/SecureBootVariableLib.h>

#include <NVIDIAConfiguration.h>
#include <libfdt.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraDeviceTreeOverlayLib.h>
#include "L4TLauncher.h"
#include "L4TRootfsValidation.h"

/**
  Causes the driver to load a specified file.

  @param  This       Protocol instance pointer.
  @param  FilePath   The device specific path of the file to load.
  @param  BootPolicy Should always be FALSE.
  @param  BufferSize On input the size of Buffer in bytes. On output with a return
                     code of EFI_SUCCESS, the amount of data transferred to
                     Buffer. On output with a return code of EFI_BUFFER_TOO_SMALL,
                     the size of Buffer required to retrieve the requested file.
                     On other errors this will not be changed.
  @param  Buffer     The memory buffer to transfer the file to. IF Buffer is NULL,
                     then no the size of the requested file is returned in
                     BufferSize.

  @retval EFI_SUCCESS           The file was loaded.
  @retval EFI_UNSUPPORTED       BootPolicy is TRUE.
  @retval EFI_INVALID_PARAMETER FilePath is not a valid device path, or
                                BufferSize is NULL.
  @retval EFI_NOT_FOUND         The file was not found
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize is too small to read the current
                                directory entry. BufferSize has been updated with
                                the size needed to complete the request.


**/
STATIC
EFI_STATUS
EFIAPI
L4TImgLoadFile2 (
  IN EFI_LOAD_FILE2_PROTOCOL   *This,
  IN EFI_DEVICE_PATH_PROTOCOL  *FilePath,
  IN BOOLEAN                   BootPolicy,
  IN OUT UINTN                 *BufferSize,
  IN VOID                      *Buffer OPTIONAL
  )

{
  // Verify if the valid parameters
  if ((This == NULL) ||
      (BufferSize == NULL) ||
      (FilePath == NULL) ||
      !IsDevicePathValid (FilePath, 0))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (BootPolicy) {
    return EFI_UNSUPPORTED;
  }

  // Check if the given buffer size is big enough
  // EFI_BUFFER_TOO_SMALL to allow caller to allocate a bigger buffer
  if (mRamdiskSize == 0) {
    return EFI_NOT_FOUND;
  }

  if ((Buffer == NULL) || (*BufferSize < mRamdiskSize)) {
    *BufferSize = mRamdiskSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  // Copy InitRd
  CopyMem (Buffer, mRamdiskData, mRamdiskSize);
  *BufferSize = mRamdiskSize;

  return EFI_SUCCESS;
}

///
/// Load File Protocol instance
///
STATIC EFI_LOAD_FILE2_PROTOCOL  mAndroidBootImgLoadFile2 = {
  L4TImgLoadFile2
};

/**
  Find the index of the GPT on disk.

  @param[in]  DeviceHandle     The handle of partition.

  @retval Index of the partition.

**/
STATIC
UINT32
EFIAPI
LocatePartitionIndex (
  IN EFI_HANDLE  DeviceHandle
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  HARDDRIVE_DEVICE_PATH     *HardDrivePath;

  if (DeviceHandle == 0) {
    return 0;
  }

  DevicePath = DevicePathFromHandle (DeviceHandle);
  if (DevicePath == NULL) {
    ErrorPrint (L"%a: Unable to find device path\r\n", __FUNCTION__);
    return 0;
  }

  while (!IsDevicePathEndType (DevicePath)) {
    if ((DevicePathType (DevicePath) == MEDIA_DEVICE_PATH) &&
        (DevicePathSubType (DevicePath) == MEDIA_HARDDRIVE_DP))
    {
      HardDrivePath = (HARDDRIVE_DEVICE_PATH *)DevicePath;
      return HardDrivePath->PartitionNumber;
    }

    DevicePath = NextDevicePathNode (DevicePath);
  }

  ErrorPrint (L"%a: Unable to locate harddrive device path node\r\n", __FUNCTION__);
  return 0;
}

/**
  Find the partition on the same disk as the loaded image

  Will fall back to the other bootchain if needed

  @param[in]  DeviceHandle     The handle of partition where this file lives on.
  @param[out] PartitionIndex   The partition index on the disk
  @param[out] PartitionHandle  The partition handle

  @retval EFI_SUCCESS    The operation completed successfully.
  @retval EFI_NOT_FOUND  The partition is not on the filesystem.

**/
STATIC
EFI_STATUS
EFIAPI
FindPartitionInfo (
  IN EFI_HANDLE    DeviceHandle,
  IN CONST CHAR16  *PartitionBasename,
  IN UINT32        BootChain,
  OUT UINT32       *PartitionIndex OPTIONAL,
  OUT EFI_HANDLE   *PartitionHandle OPTIONAL
  )
{
  EFI_STATUS                   Status;
  EFI_HANDLE                   *ParentHandles;
  UINTN                        ParentCount;
  UINTN                        ParentIndex;
  EFI_HANDLE                   *ChildHandles;
  UINTN                        ChildCount;
  UINTN                        ChildIndex;
  UINT32                       FoundIndex = 0;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo;
  EFI_HANDLE                   FoundHandle        = 0;
  EFI_HANDLE                   FoundHandleGeneric = 0;
  EFI_HANDLE                   FoundHandleAlt     = 0;
  CHAR16                       *SubString;
  UINTN                        PartitionBasenameLen;

  if (BootChain > 1) {
    return EFI_UNSUPPORTED;
  }

  if (PartitionBasename == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PartitionBasenameLen = StrnLenS (PartitionBasename, MAX_PARTITION_NAME_SIZE);

  Status = PARSE_HANDLE_DATABASE_PARENTS (DeviceHandle, &ParentCount, &ParentHandles);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to find parents - %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  for (ParentIndex = 0; ParentIndex < ParentCount; ParentIndex++) {
    Status = ParseHandleDatabaseForChildControllers (ParentHandles[ParentIndex], &ChildCount, &ChildHandles);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to find child controllers - %r\r\n", __FUNCTION__, Status);
      return Status;
    }

    for (ChildIndex = 0; ChildIndex < ChildCount; ChildIndex++) {
      Status = gBS->HandleProtocol (ChildHandles[ChildIndex], &gEfiPartitionInfoProtocolGuid, (VOID **)&PartitionInfo);
      if (EFI_ERROR (Status)) {
        continue;
      }

      // Only GPT partitions are supported
      if (PartitionInfo->Type != PARTITION_TYPE_GPT) {
        continue;
      }

      // Look for A/B Names
      if (StrCmp (PartitionInfo->Info.Gpt.PartitionName, PartitionBasename) == 0) {
        ASSERT (FoundHandleGeneric == 0);
        FoundHandleGeneric = ChildHandles[ChildIndex];
      } else if ((PartitionBasenameLen + 2) == StrLen (PartitionInfo->Info.Gpt.PartitionName)) {
        SubString = StrStr (PartitionInfo->Info.Gpt.PartitionName, PartitionBasename);
        if (SubString != NULL) {
          // See if it is a prefix
          if ((SubString == (PartitionInfo->Info.Gpt.PartitionName + 2)) &&
              (PartitionInfo->Info.Gpt.PartitionName[1] == L'_'))
          {
            if ((PartitionInfo->Info.Gpt.PartitionName[0] == (L'A' + BootChain)) ||
                (PartitionInfo->Info.Gpt.PartitionName[0] == (L'a' + BootChain)))
            {
              ASSERT (FoundHandle == 0);
              FoundHandle = ChildHandles[ChildIndex];
            }

            if ((PartitionInfo->Info.Gpt.PartitionName[0] == (L'B' - BootChain)) ||
                (PartitionInfo->Info.Gpt.PartitionName[0] == (L'b' - BootChain)))
            {
              ASSERT (FoundHandleAlt == 0);
              FoundHandleAlt = ChildHandles[ChildIndex];
            }

            // See if it is a postfix
          } else if ((SubString == PartitionInfo->Info.Gpt.PartitionName) &&
                     (PartitionInfo->Info.Gpt.PartitionName[PartitionBasenameLen] == L'_'))
          {
            if ((PartitionInfo->Info.Gpt.PartitionName[PartitionBasenameLen + 1] == (L'a' + BootChain)) ||
                (PartitionInfo->Info.Gpt.PartitionName[PartitionBasenameLen + 1] == (L'A' + BootChain)))
            {
              ASSERT (FoundHandle == 0);
              FoundHandle = ChildHandles[ChildIndex];
            } else if ((PartitionInfo->Info.Gpt.PartitionName[PartitionBasenameLen + 1] == (L'b' - BootChain)) ||
                       (PartitionInfo->Info.Gpt.PartitionName[PartitionBasenameLen + 1] == (L'B' - BootChain)))
            {
              ASSERT (FoundHandleAlt == 0);
              FoundHandleAlt = ChildHandles[ChildIndex];
            }
          }
        }
      }
    }

    FreePool (ChildHandles);
  }

  FreePool (ParentHandles);

  if ((FoundHandle == 0) && (FoundHandleGeneric == 0) && (FoundHandleAlt == 0)) {
    return EFI_NOT_FOUND;
  } else if (FoundHandle == 0) {
    if (FoundHandleGeneric != 0) {
      FoundHandle = FoundHandleGeneric;
    } else {
      FoundHandle = FoundHandleAlt;
      Print (L"Falling back to alternative boot path\r\n");
    }
  }

  FoundIndex = LocatePartitionIndex (FoundHandle);

  if (FoundIndex == 0) {
    ErrorPrint (L"%a: Failed to find both partitions index\r\n", __FUNCTION__);
    return EFI_DEVICE_ERROR;
  }

  if (PartitionIndex != NULL) {
    *PartitionIndex = FoundIndex;
  }

  if (PartitionHandle != NULL) {
    *PartitionHandle = FoundHandle;
  }

  return EFI_SUCCESS;
}

/**
  Update the grub boot configuration file

  @param[in]  DeviceHandle     The handle of partition where this file lives on.
  @param[in]  PartitionIndex   Partition number of the root file system
  @param[in]  BootImgPresent   BootImage is present on system
  @param[in]  RecoveryPresent  Recovery kernel partition is present on system

  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
UpdateBootCfgFile (
  IN EFI_HANDLE  DeviceHandle,
  IN UINT32      PartitionIndex,
  IN BOOLEAN     BootImgPresent,
  IN BOOLEAN     RecoveryPresent
  )
{
  EFI_STATUS                Status;
  CHAR8                     CorrectPartitionContent[MAX_BOOTCONFIG_CONTENT_SIZE];
  CHAR8                     ReadPartitionContent[MAX_BOOTCONFIG_CONTENT_SIZE];
  CHAR16                    CpuBootArgs[MAX_CBOOTARG_SIZE/sizeof (CHAR16)];
  UINTN                     CorrectSize;
  UINT64                    FileSize;
  EFI_FILE_HANDLE           FileHandle;
  EFI_DEVICE_PATH           *FullDevicePath;
  ANDROID_BOOTIMG_PROTOCOL  *AndroidBootProtocol;

  ZeroMem (CpuBootArgs, MAX_CBOOTARG_SIZE);
  Status = gBS->LocateProtocol (&gAndroidBootImgProtocolGuid, NULL, (VOID **)&AndroidBootProtocol);
  if (!EFI_ERROR (Status)) {
    if (AndroidBootProtocol->AppendArgs != NULL) {
      Status = AndroidBootProtocol->AppendArgs (CpuBootArgs, MAX_CBOOTARG_SIZE);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Failed to get platform addition arguments\r\n", __FUNCTION__);
        return Status;
      }
    }
  }

  CorrectSize = AsciiSPrint (CorrectPartitionContent, MAX_BOOTCONFIG_CONTENT_SIZE, GRUB_BOOTCONFIG_CONTENT_FORMAT, CpuBootArgs, PartitionIndex, BootImgPresent, RecoveryPresent);

  FullDevicePath = FileDevicePath (DeviceHandle, GRUB_BOOTCONFIG_FILE);
  if (FullDevicePath == NULL) {
    ErrorPrint (L"%a: Failed to create file device path\r\n", __FUNCTION__);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = EfiOpenFileByDevicePath (&FullDevicePath, &FileHandle, EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE, 0);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to open file: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  Status = FileHandleGetSize (FileHandle, &FileSize);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to get file size: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  if (FileSize == CorrectSize) {
    ASSERT (FileSize <= MAX_BOOTCONFIG_CONTENT_SIZE);
    Status = FileHandleRead (FileHandle, &FileSize, ReadPartitionContent);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to read current file content: %r\r\n", __FUNCTION__, Status);
      return Status;
    }

    if (CompareMem (CorrectPartitionContent, ReadPartitionContent, CorrectSize) == 0) {
      return EFI_SUCCESS;
    }
  }

  Status = FileHandleSetSize (FileHandle, 0);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to set file size to 0\r\n", __FUNCTION__);
    return Status;
  }

  Status = FileHandleWrite (FileHandle, &CorrectSize, CorrectPartitionContent);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to write file content\r\n", __FUNCTION__);
    return Status;
  }

  FileHandleClose (FileHandle);
  return EFI_SUCCESS;
}

/**
  Update the grub partition configuration files

  @param[in]  DeviceHandle     The handle of partition where this file lives on.
  @param[in]  BootChain        Numeric version of the chain

  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
UpdateBootConfig (
  IN EFI_HANDLE  DeviceHandle,
  IN UINT32      BootChain
  )
{
  UINT32      PartitionIndex;
  EFI_STATUS  Status;
  BOOLEAN     BootImgPresent  = FALSE;
  BOOLEAN     RecoveryPresent = FALSE;

  Status = FindPartitionInfo (DeviceHandle, ROOTFS_BASE_NAME, BootChain, &PartitionIndex, NULL);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to find rootfs partition info\r\n", __FUNCTION__);
    return Status;
  }

  Status = FindPartitionInfo (DeviceHandle, BOOTIMG_BASE_NAME, BootChain, NULL, NULL);
  if (Status == EFI_SUCCESS) {
    BootImgPresent = TRUE;
  } else if (Status == EFI_NOT_FOUND) {
    BootImgPresent = FALSE;
  } else if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to find bootimg partition info\r\n", __FUNCTION__);
    return Status;
  }

  Status = FindPartitionInfo (DeviceHandle, RECOVERY_BASE_NAME, BootChain, NULL, NULL);
  if (Status == EFI_SUCCESS) {
    RecoveryPresent = TRUE;
  } else if (Status == EFI_NOT_FOUND) {
    RecoveryPresent = FALSE;
  } else if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to find recovery partition info\r\n", __FUNCTION__);
    return Status;
  }

  Status = UpdateBootCfgFile (DeviceHandle, PartitionIndex, BootImgPresent, RecoveryPresent);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Unable to update boot configuration file\r\n");
    return Status;
  }

  return Status;
}

/**
  Remove comments and leading trailing whitespace

  @param[in]  InputString

  @returns   Cleaned string

**/
STATIC
CHAR16 *
EFIAPI
CleanExtLinuxLine (
  IN CHAR16  *InputString
  )
{
  CHAR16  *CurrentString;
  CHAR16  *EndSearch;
  CHAR16  *LastNonSpace;

  // Remove any comments
  CurrentString = StrStr (InputString, L"#");
  if (CurrentString != NULL) {
    *CurrentString = CHAR_NULL;
  }

  CurrentString = InputString;
  while ((*CurrentString == L' ') ||
         (*CurrentString == L'\t'))
  {
    CurrentString++;
  }

  LastNonSpace = CurrentString;
  EndSearch    = CurrentString;
  if (*LastNonSpace != CHAR_NULL) {
    while (*EndSearch != CHAR_NULL) {
      if ((*EndSearch != L' ') &&
          (*EndSearch != L'\t'))
      {
        LastNonSpace = EndSearch;
      }

      EndSearch++;
    }

    LastNonSpace[1] = CHAR_NULL;
  }

  return CurrentString;
}

/*
 *
  SetupCertList

  Function to read and setup the Ceriticate list according to what the PKCS
  Verification Lib expects.
  The PKCS Verification lib expects to walk a list of EFI_SIGNATURE_LIST entries
  and a NULL entry to mark the end of the list.
  To get this , first we get the stored list of certificates using variable
  services, then walk the list (each DB entry can vary in size) so before
  moving to the next EFI_SIGNATURE_LIST entry, we need to parse that header to
  determine the size of the entry.

  @param[in]  VariableName     The Variable Name under which the certificate DB
                               is stored.

  @retval SUCCESS : List of DB Entry pointers (with a terminating NULL entry)
          FAILURE:  NULL
 *
 */
STATIC
EFI_SIGNATURE_LIST **
SetupCertList (
  IN CHAR16  *VariableName
  )
{
  EFI_STATUS          Status;
  EFI_SIGNATURE_LIST  *CertDb = NULL;
  UINTN               CertDbSize;
  EFI_SIGNATURE_LIST  *CertList;
  UINTN               CertListIndex;
  UINTN               CertListCount;
  EFI_SIGNATURE_LIST  **CertLists = NULL;

  Status = GetVariable2 (
             VariableName,
             &gEfiImageSecurityDatabaseGuid,
             (VOID **)&CertDb,
             &CertDbSize
             );
  if (EFI_ERROR (Status)) {
    if (Status != EFI_NOT_FOUND) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to retrieve certificate database '%s': %r\r\n",
        __FUNCTION__,
        VariableName,
        Status
        ));
    }

    // In case of an error, assume an empty certificate database.
    CertDb     = NULL;
    CertDbSize = 0;
    Status     = EFI_SUCCESS;
  }

  // Walk the list to determine how many signature lists are present.
  CertListCount = 0;
  CertList      = CertDb;
  while (  ((UINT8 *)CertList < (UINT8 *)CertDb + CertDbSize)
        && ((UINT8 *)CertList + CertList->SignatureListSize <= (UINT8 *)CertDb + CertDbSize))
  {
    CertListCount++;
    CertList = (EFI_SIGNATURE_LIST *)((UINT8 *)CertList + CertList->SignatureListSize);
  }

  CertLists = (EFI_SIGNATURE_LIST **)AllocateZeroPool (sizeof (EFI_SIGNATURE_LIST *) * (CertListCount + 1));
  if (CertLists == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  CertList = CertDb;
  for (CertListIndex = 0; CertListIndex < CertListCount; CertListIndex++) {
    CertLists[CertListIndex] = CertList;
    CertList                 = (EFI_SIGNATURE_LIST *)((UINT8 *)CertList + CertList->SignatureListSize);
  }

  // Keep the last entry NULL (what the PKCS lib code expects)
  CertLists[CertListCount] = NULL;

Exit:
  if (EFI_ERROR (Status)) {
    if (CertLists != NULL) {
      FreePool (CertLists);
    }

    if (CertDb != NULL) {
      FreePool (CertDb);
    }
  }

  return CertLists;
}

/**
  Verify a detached signature.

  @param[in]  SignData      Detached signature data
  @param[in]  SignDataSize  Size of the detached signature data
  @param[in]  InData        Data signed by the detached signature
  @param[in]  InDataSize    Length of the signed data

  @retval EFI_SUCCESS     Signature successfully verified
  @retval !(EFI_SUCCESS)  Could not verify signature
*/
STATIC
EFI_STATUS
VerifyDetachedSignature (
  IN       VOID   *CONST  SignData,
  IN CONST UINTN          SignDataSize,
  IN       VOID   *CONST  InData,
  IN CONST UINTN          InDataSize
  )
{
  STATIC EFI_SIGNATURE_LIST  **AllowedDb = NULL;
  STATIC EFI_SIGNATURE_LIST  **RevokedDb = NULL;

  EFI_STATUS                 Status;
  EFI_SIGNATURE_LIST         **TimeStampDb = NULL;
  EFI_PKCS7_VERIFY_PROTOCOL  *Pkcs7VerifyProtocol;

  // Do these steps once, to locate and setup the DB/DBX certs.
  if (AllowedDb == NULL) {
    AllowedDb = SetupCertList (EFI_IMAGE_SECURITY_DATABASE);
  }

  if (RevokedDb == NULL) {
    RevokedDb = SetupCertList (EFI_IMAGE_SECURITY_DATABASE1);
  }

  Status = gBS->LocateProtocol (
                  &gEfiPkcs7VerifyProtocolGuid,
                  NULL,
                  (VOID **)&Pkcs7VerifyProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to locate PKCS7 verification protocol: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  return Pkcs7VerifyProtocol->VerifyBuffer (
                                Pkcs7VerifyProtocol,
                                SignData,
                                SignDataSize,
                                InData,
                                InDataSize,
                                AllowedDb,
                                RevokedDb,
                                TimeStampDb,
                                NULL, /* Content */
                                NULL  /* ContentSize */
                                );
}

/**
  Utility function to open a file named FileName, return its
  FileHandle and read the contents of the file into buffer Data.

  This function will open the file, allocate a data buffer based on
  the file size and returns file handle, the data buffer and the file
  size.

  Upon successful return, the caller is responsible for freeing all
  allocated resources (file handle and/or data buffer).

  @param[in]   PartitionHandle  Handle of the partition where this file lives on.
  @param[in]   FileName         Name of file to be processed
  @param[out]  FileHandle       File handle of the opened file.
  @param[out]  FileData         Buffer with file data.
  @param[out]  FileDataSize     Size of the opened file.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES  Failed buffer allocation.
  @retval !(EFI_SUCCESS)        Error status from other APIs called.
*/
STATIC
EFI_STATUS
OpenAndReadUntrustedFileToBuffer (
  IN  CONST EFI_HANDLE          PartitionHandle,
  IN  CONST CHAR16      *CONST  FileName,
  OUT EFI_FILE_HANDLE   *CONST  FileHandle    OPTIONAL,
  OUT VOID             **CONST  FileData      OPTIONAL,
  OUT UINT64            *CONST  FileDataSize  OPTIONAL
  )
{
  EFI_STATUS       Status      = EFI_SUCCESS;
  EFI_DEVICE_PATH  *DevicePath = NULL;
  EFI_DEVICE_PATH  *NextDevicePath;
  EFI_FILE_HANDLE  Handle = NULL;
  VOID             *Data  = NULL;
  UINT64           DataSize;

  if ((FileHandle != NULL) || (FileData != NULL) || (FileDataSize != NULL)) {
    DevicePath = FileDevicePath (PartitionHandle, FileName);
    if (DevicePath == NULL) {
      ErrorPrint (L"%a: Failed to create file device path\r\n", __FUNCTION__);
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    NextDevicePath = DevicePath;
    Status         = EfiOpenFileByDevicePath (
                       &NextDevicePath,
                       &Handle,
                       EFI_FILE_MODE_READ,
                       0
                       );
    if (EFI_ERROR (Status)) {
      ErrorPrint (
        L"%a: Failed to open %s: %r\r\n",
        __FUNCTION__,
        FileName,
        Status
        );
      goto Exit;
    }
  }

  if ((FileData != NULL) || (FileDataSize != NULL)) {
    Status = FileHandleGetSize (Handle, &DataSize);
    if (EFI_ERROR (Status)) {
      ErrorPrint (
        L"%a: Failed to get size of file %s: %r\r\n",
        __FUNCTION__,
        FileName,
        Status
        );
      goto Exit;
    }
  }

  if (FileData != NULL) {
    Data = AllocatePool (DataSize);
    if (Data == NULL) {
      ErrorPrint (
        L"%a: Failed to allocate buffer for %s\r\n",
        __FUNCTION__,
        FileName
        );
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    Status = FileHandleRead (Handle, &DataSize, Data);
    if (EFI_ERROR (Status)) {
      ErrorPrint (
        L"%a: Failed to read %s\r\n",
        __FUNCTION__,
        FileName
        );
      goto Exit;
    }

    if (FileHandle != NULL) {
      // If both handle and data were requested, rewind the handle
      // back to the beginning of the file.
      Status = FileHandleSetPosition (Handle, 0);
      if (EFI_ERROR (Status)) {
        ErrorPrint (
          L"%a: Failed to rewind %s\r\n",
          __FUNCTION__,
          FileName
          );
        goto Exit;
      }
    }
  }

  if (FileHandle != NULL) {
    *FileHandle = Handle;
    Handle      = NULL;
  }

  if (FileData != NULL) {
    *FileData = Data;
    Data      = NULL;
  }

  if (FileDataSize != NULL) {
    *FileDataSize = DataSize;
  }

Exit:
  if (Data != NULL) {
    FreePool (Data);
  }

  if (Handle != NULL) {
    FileHandleClose (Handle);
  }

  if (DevicePath != NULL) {
    FreePool (DevicePath);
  }

  return Status;
}

/**
  Do exactly what OpenAndReadUntrustedFileToBuffer does, except when
  UEFI Secure Boot is enabled, also check detached signature of the
  file. A valid signature establishes trust in the file's contents.

  @param[in]   PartitionHandle  Handle of the partition where this file lives on.
  @param[in]   FileName         Name of file to be processed
  @param[out]  FileHandle       File handle of the opened file.
  @param[out]  FileData         Buffer with file data.
  @param[out]  FileDataSize     Size of the opened file.

  @retval EFI_SUCCESS     The operation completed successfully.
  @retval !(EFI_SUCCESS)  Error status from other APIs called.
*/
STATIC
EFI_STATUS
OpenAndReadFileToBuffer (
  IN  CONST EFI_HANDLE          PartitionHandle,
  IN  CONST CHAR16      *CONST  FileName,
  OUT EFI_FILE_HANDLE   *CONST  FileHandle    OPTIONAL,
  OUT VOID             **CONST  FileData      OPTIONAL,
  OUT UINT64            *CONST  FileDataSize  OPTIONAL
  )
{
  EFI_STATUS       Status;
  EFI_FILE_HANDLE  Handle = NULL;
  VOID             *Data  = NULL;
  UINT64           DataSize;
  CHAR16           *SigFileName = NULL;
  UINTN            SigFileNameSize;
  VOID             *SigData = NULL;
  UINT64           SigSize;

  if (!IsSecureBootEnabled ()) {
    DEBUG ((DEBUG_INFO, "%a: Secure Boot is disabled\r\n", __FUNCTION__));

    return OpenAndReadUntrustedFileToBuffer (
             PartitionHandle,
             FileName,
             FileHandle,
             FileData,
             FileDataSize
             );
  }

  Status = OpenAndReadUntrustedFileToBuffer (
             PartitionHandle,
             FileName,
             FileHandle != NULL ? &Handle : NULL,
             &Data,
             &DataSize
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  // The detached signature file should be <filename>.sig
  SigFileNameSize = StrSize (FileName) + StrSize (DETACHED_SIG_FILE_EXTENSION) - sizeof (CHAR16);
  SigFileName     = AllocatePool (SigFileNameSize);
  if (SigFileName == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: cannot allocate buffer for signature file name (%u bytes)\r\n",
      __FUNCTION__,
      SigFileNameSize
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  UnicodeSPrint (SigFileName, SigFileNameSize, L"%s%s", FileName, DETACHED_SIG_FILE_EXTENSION);

  Status = OpenAndReadUntrustedFileToBuffer (
             PartitionHandle,
             SigFileName,
             NULL,
             &SigData,
             &SigSize
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = VerifyDetachedSignature (
             SigData,
             SigSize,
             Data,
             DataSize
             );
  if (EFI_ERROR (Status)) {
    ErrorPrint (
      L"%a: %s failed signature verification: %r\r\n",
      __FUNCTION__,
      FileName,
      Status
      );
    goto Exit;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: %s signature verification successful\r\n",
    __FUNCTION__,
    FileName
    ));

  if (FileHandle != NULL) {
    *FileHandle = Handle;
    Handle      = NULL;
  }

  if (FileData != NULL) {
    *FileData = Data;
    Data      = NULL;
  }

  if (FileDataSize != NULL) {
    *FileDataSize = DataSize;
  }

Exit:
  if (SigData != NULL) {
    FreePool (SigData);
  }

  if (SigFileName != NULL) {
    FreePool (SigFileName);
  }

  if (Data != NULL) {
    FreePool (Data);
  }

  if (Handle != NULL) {
    FileHandleClose (Handle);
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
CheckCommandString (
  IN CHAR16        *CommandLine,
  IN CONST CHAR16  *Key,
  OUT CHAR16       **Buffer
  )
{
  CHAR16  *Value;

  if (StrnCmp (CommandLine, Key, StrLen (Key)) == 0) {
    Value = CleanExtLinuxLine (CommandLine + StrLen (Key));
    if (Buffer != NULL) {
      *Buffer = AllocateCopyPool (StrSize (Value), Value);
      if (*Buffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }
    }

    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

/**
  Process the extlinux.conf file

  @param[in]  DeviceHandle     The handle of partition where this file lives on.
  @param[in]  BootChain        Numeric version of the chain
  @param[out] ExtLinuxConfig   Pointer to an extlinux config object
  @param[out] RootFsHandle     Pointer to the handle of the device tree

  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
ProcessExtLinuxConfig (
  IN EFI_HANDLE             DeviceHandle,
  IN UINT32                 BootChain,
  OUT EXTLINUX_BOOT_CONFIG  *BootConfig,
  OUT EFI_HANDLE            *RootFsHandle
  )
{
  EFI_STATUS       Status;
  EFI_FILE_HANDLE  FileHandle;
  CHAR16           *FileLine = NULL;
  CHAR16           *CleanLine;
  CHAR16           *DefaultLabel = NULL;
  CHAR16           *Timeout      = NULL;
  CHAR16           *CbootArg     = NULL;
  CHAR16           *PostCbootArg = NULL;
  BOOLEAN          Ascii;
  UINTN            Index;

  ZeroMem (BootConfig, sizeof (EXTLINUX_BOOT_CONFIG));

  if (RootFsHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = FindPartitionInfo (DeviceHandle, ROOTFS_BASE_NAME, BootChain, NULL, RootFsHandle);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to find partition info\r\n", __FUNCTION__);
    return Status;
  }

  Status = OpenAndReadFileToBuffer (
             *RootFsHandle,
             EXTLINUX_CONF_PATH,
             &FileHandle,
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a:sds Failed to Authenticate %s (%r)\r\n", __FUNCTION__, EXTLINUX_CONF_PATH, Status);
    return Status;
  }

  while (!FileHandleEof (FileHandle)) {
    if (FileLine != NULL) {
      FreePool (FileLine);
      FileLine = NULL;
    }

    FileLine = FileHandleReturnLine (FileHandle, &Ascii);
    if (FileLine == NULL) {
      break;
    }

    CleanLine = CleanExtLinuxLine (FileLine);
    if (*CleanLine != CHAR_NULL) {
      Status = CheckCommandString (CleanLine, EXTLINUX_KEY_TIMEOUT, &Timeout);
      if (!EFI_ERROR (Status)) {
        BootConfig->Timeout = StrDecimalToUintn (Timeout);
        FreePool (Timeout);
        Timeout = NULL;
        continue;
      }

      Status = CheckCommandString (CleanLine, EXTLINUX_KEY_DEFAULT, &DefaultLabel);
      if (!EFI_ERROR (Status)) {
        continue;
      }

      Status = CheckCommandString (CleanLine, EXTLINUX_KEY_MENU_TITLE, &BootConfig->MenuTitle);
      if (!EFI_ERROR (Status)) {
        continue;
      }

      if (BootConfig->NumberOfBootOptions < MAX_EXTLINUX_OPTIONS) {
        Status = CheckCommandString (CleanLine, EXTLINUX_KEY_LABEL, &BootConfig->BootOptions[BootConfig->NumberOfBootOptions].Label);
        if (!EFI_ERROR (Status)) {
          BootConfig->NumberOfBootOptions++;
          continue;
        }
      }

      if ((BootConfig->NumberOfBootOptions <= MAX_EXTLINUX_OPTIONS) &&
          (BootConfig->NumberOfBootOptions != 0))
      {
        Status = CheckCommandString (CleanLine, EXTLINUX_KEY_MENU_LABEL, &BootConfig->BootOptions[BootConfig->NumberOfBootOptions-1].MenuLabel);
        if (!EFI_ERROR (Status)) {
          continue;
        }

        Status = CheckCommandString (CleanLine, EXTLINUX_KEY_LINUX, &BootConfig->BootOptions[BootConfig->NumberOfBootOptions-1].LinuxPath);
        if (!EFI_ERROR (Status)) {
          continue;
        }

        Status = CheckCommandString (CleanLine, EXTLINUX_KEY_INITRD, &BootConfig->BootOptions[BootConfig->NumberOfBootOptions-1].InitrdPath);
        if (!EFI_ERROR (Status)) {
          continue;
        }

        Status = CheckCommandString (CleanLine, EXTLINUX_KEY_FDT, &BootConfig->BootOptions[BootConfig->NumberOfBootOptions-1].DtbPath);
        if (!EFI_ERROR (Status)) {
          continue;
        }

        Status = CheckCommandString (CleanLine, EXTLINUX_KEY_OVERLAYS, &BootConfig->BootOptions[BootConfig->NumberOfBootOptions-1].Overlays);
        if (!EFI_ERROR (Status)) {
          continue;
        }

        Status = CheckCommandString (CleanLine, EXTLINUX_KEY_APPEND, &BootConfig->BootOptions[BootConfig->NumberOfBootOptions-1].BootArgs);
        if (!EFI_ERROR (Status)) {
          CbootArg = StrStr (BootConfig->BootOptions[BootConfig->NumberOfBootOptions-1].BootArgs, EXTLINUX_CBOOT_ARG);
          if (CbootArg != NULL) {
            PostCbootArg = CbootArg + StrLen (EXTLINUX_CBOOT_ARG);
            while (*PostCbootArg == L' ') {
              PostCbootArg++;
            }

            CopyMem (CbootArg, PostCbootArg, StrSize (PostCbootArg));
          }

          continue;
        }
      }
    }
  }

  if (FileLine != NULL) {
    FreePool (FileLine);
    FileLine = NULL;
  }

  FileHandleClose (FileHandle);

  if (DefaultLabel != NULL) {
    for (Index = 0; Index < BootConfig->NumberOfBootOptions; Index++) {
      if (StrCmp (DefaultLabel, BootConfig->BootOptions[Index].Label) == 0) {
        BootConfig->DefaultBootEntry = Index;
        break;
      }
    }
  }

  for (Index = 0; Index < BootConfig->NumberOfBootOptions; Index++) {
    if (BootConfig->BootOptions[Index].DtbPath != NULL) {
      PathCleanUpDirectories (BootConfig->BootOptions[Index].DtbPath);
    }

    if (BootConfig->BootOptions[Index].InitrdPath != NULL) {
      PathCleanUpDirectories (BootConfig->BootOptions[Index].InitrdPath);
    }

    if (BootConfig->BootOptions[Index].LinuxPath != NULL) {
      PathCleanUpDirectories (BootConfig->BootOptions[Index].LinuxPath);
    }

    if (BootConfig->BootOptions[Index].Overlays != NULL) {
      PathCleanUpDirectories (BootConfig->BootOptions[Index].Overlays);
    }
  }

  if (BootConfig->NumberOfBootOptions == 0) {
    return EFI_NOT_FOUND;
  } else {
    return EFI_SUCCESS;
  }
}

/**
  Wait for user input boot option

  @param[out] ExtLinuxConfig   Pointer to an extlinux.conf file

  @retval Selected boot option.

**/
STATIC
UINT32
EFIAPI
ExtLinuxBootMenu (
  IN EXTLINUX_BOOT_CONFIG  *BootConfig
  )
{
  EFI_STATUS     Status;
  UINTN          Index;
  EFI_EVENT      EventArray[2];
  UINTN          EventIndex;
  EFI_INPUT_KEY  Key;

  // Display boot options
  if ((BootConfig->Timeout == 0) ||
      (BootConfig->NumberOfBootOptions == 1))
  {
    return BootConfig->DefaultBootEntry;
  }

  Status = gBS->CreateEvent (EVT_TIMER, TPL_CALLBACK, NULL, NULL, &EventArray[0]);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to create timer event booting default\r\n");
    return BootConfig->DefaultBootEntry;
  }

  if (BootConfig->MenuTitle != NULL) {
    Print (L"%s\r\n", BootConfig->MenuTitle);
  } else {
    Print (L"L4T boot options\r\n");
  }

  for (Index = 0; Index < BootConfig->NumberOfBootOptions; Index++) {
    Print (L"%d: %s\r\n", Index, BootConfig->BootOptions[Index].MenuLabel);
  }

  Status = gBS->SetTimer (EventArray[0], TimerRelative, EFI_TIMER_PERIOD_SECONDS (BootConfig->Timeout)/10);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to set timer, booting default\r\n");
    return BootConfig->DefaultBootEntry;
  }

  EventArray[1] = gST->ConIn->WaitForKey;
  Print (L"Press 0-%d to boot selection within %d.%d seconds.\r\n", BootConfig->NumberOfBootOptions - 1, BootConfig->Timeout/10, BootConfig->Timeout %10);
  Print (L"Press any other key to boot default (Option: %d)\r\n", BootConfig->DefaultBootEntry);

  gBS->WaitForEvent (2, EventArray, &EventIndex);
  gBS->CloseEvent (EventArray[0]);
  if (EventIndex == 1) {
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if (!EFI_ERROR (Status) &&
        (Key.ScanCode == SCAN_NULL))
    {
      if ((Key.UnicodeChar >= L'0') &&
          (Key.UnicodeChar <= L'0' + BootConfig->NumberOfBootOptions - 1))
      {
        return Key.UnicodeChar - L'0';
      }
    }
  }

  return BootConfig->DefaultBootEntry;
}

/**
  Boots an android style partition located with Partition base name and bootchain

  @param[in]  ImageHandle       Handle of this application
  @param[in]  DeviceHandle      The handle of partition where this file lives on.
  @param[in]  BootOption        Boot options to load

  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
ExtLinuxBoot (
  IN EFI_HANDLE            ImageHandle,
  IN EFI_HANDLE            DeviceHandle,
  IN EXTLINUX_BOOT_OPTION  *BootOption
  )
{
  EFI_STATUS                 Status;
  CHAR16                     *NewArgs = NULL;
  UINTN                      ArgSize;
  ANDROID_BOOTIMG_PROTOCOL   *AndroidBootProtocol;
  EFI_HANDLE                 RamDiskLoadFileHandle = NULL;
  UINTN                      FdtSize;
  VOID                       *AcpiBase         = NULL;
  VOID                       *OldFdtBase       = NULL;
  VOID                       *NewFdtBase       = NULL;
  VOID                       *ExpandedFdtBase  = NULL;
  BOOLEAN                    FdtUpdated        = FALSE;
  EFI_DEVICE_PATH_PROTOCOL   *KernelDevicePath = NULL;
  EFI_HANDLE                 KernelHandle      = NULL;
  EFI_LOADED_IMAGE_PROTOCOL  *ImageInfo;
  VOID                       *OverlayBuffer = NULL;
  UINTN                      OverlaySize;
  CHAR16                     *Overlays = NULL;
  CHAR16                     *OverlayPath;
  UINTN                      Index;
  CHAR8                      SWModule[] = "kernel";
  INTN                       FdtStatus;

  // Process Args
  ArgSize = StrSize (BootOption->BootArgs) + MAX_CBOOTARG_SIZE;
  NewArgs = AllocateCopyPool (ArgSize, BootOption->BootArgs);
  if (NewArgs == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = gBS->LocateProtocol (&gAndroidBootImgProtocolGuid, NULL, (VOID **)&AndroidBootProtocol);
  if (!EFI_ERROR (Status)) {
    if (AndroidBootProtocol->AppendArgs != NULL) {
      Status = AndroidBootProtocol->AppendArgs (NewArgs, ArgSize);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Failed to get platform addition arguments\r\n", __FUNCTION__);
        goto Exit;
      }
    }
  }

  // Expose LoadFile2 for initrd
  if (BootOption->InitrdPath != NULL) {
    Status = OpenAndReadFileToBuffer (
               DeviceHandle,
               BootOption->InitrdPath,
               NULL,
               &mRamdiskData,
               &mRamdiskSize
               );
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a:sds Failed to Authenticate %s (%r)\r\n", __FUNCTION__, BootOption->InitrdPath, Status);
      return Status;
    }

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &RamDiskLoadFileHandle,
                    &gEfiLoadFile2ProtocolGuid,
                    &mAndroidBootImgLoadFile2,
                    &gEfiDevicePathProtocolGuid,
                    &mRamdiskDevicePath,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      goto Exit;
    }
  }

  // Reload fdt if needed
  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (EFI_ERROR (Status) && (BootOption->DtbPath != NULL)) {
    Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &OldFdtBase);
    if (EFI_ERROR (Status)) {
      OldFdtBase = NULL;
    }

    Status = OpenAndReadFileToBuffer (
               DeviceHandle,
               BootOption->DtbPath,
               NULL,
               &NewFdtBase,
               &FdtSize
               );
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a:sds Failed to Authenticate %s (%r)\r\n", __FUNCTION__, EXTLINUX_CONF_PATH, Status);
      goto Exit;
    }

    ExpandedFdtBase = AllocatePages (EFI_SIZE_TO_PAGES (4 * fdt_totalsize (NewFdtBase)));
    if (fdt_open_into (NewFdtBase, ExpandedFdtBase, 4 * fdt_totalsize (NewFdtBase)) != 0) {
      Status = EFI_NOT_FOUND;
      goto Exit;
    }

    Status = gBS->InstallConfigurationTable (&gFdtTableGuid, ExpandedFdtBase);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to install fdt\r\n", __FUNCTION__);
      goto Exit;
    }

    FdtUpdated = TRUE;

    if (BootOption->Overlays != NULL) {
      DEBUG ((DEBUG_INFO, "%a: applying overlays %s\r\n", __FUNCTION__, BootOption->Overlays));
      Overlays = AllocateCopyPool (StrSize (BootOption->Overlays) * sizeof (CHAR16), BootOption->Overlays);
      if (Overlays == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      OverlayPath = Overlays;
      for (Index = 0; Index < StrSize (BootOption->Overlays) / sizeof (CHAR16); Index++) {
        switch (Overlays[Index]) {
          case L',':
          case L'\0':
            break;
          default:
            continue;
        }

        Overlays[Index] = L'\0';

        DEBUG ((DEBUG_INFO, "%a: OverlayPath '%s'\r\n", __FUNCTION__, OverlayPath));
        Status = OpenAndReadFileToBuffer (
                   DeviceHandle,
                   OverlayPath,
                   NULL,
                   &OverlayBuffer,
                   &OverlaySize
                   );
        if (EFI_ERROR (Status)) {
          ErrorPrint (L"%a: Failed to load overlay %s: %r\r\n", __FUNCTION__, OverlayPath, Status);
          goto Exit;
        }

        FdtStatus = fdt_check_header (OverlayBuffer);
        if (FdtStatus != 0) {
          ErrorPrint (L"%a: Overlay %s bad header: %lld\r\n", __FUNCTION__, OverlayPath, FdtStatus);
          goto Exit;
        }

        Status = ApplyTegraDeviceTreeOverlay (ExpandedFdtBase, OverlayBuffer, SWModule);
        if (EFI_ERROR (Status)) {
          goto Exit;
        }

        FreePool (OverlayBuffer);
        OverlayBuffer = NULL;
        OverlayPath   = &Overlays[Index + 1];
      }

      FreePool (Overlays);
      Overlays = NULL;
    }
  }

  // Load and start the kernel
  if (BootOption->LinuxPath != NULL) {
    KernelDevicePath = FileDevicePath (DeviceHandle, BootOption->LinuxPath);
    if (KernelDevicePath == NULL) {
      ErrorPrint (L"%a: Failed to create device path\r\n", __FUNCTION__);
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    Status = gBS->LoadImage (FALSE, ImageHandle, KernelDevicePath, NULL, 0, &KernelHandle);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Unable to load image: %s %r\r\n", __FUNCTION__, BootOption->LinuxPath, Status);
      goto Exit;
    }

    if (NewArgs != NULL) {
      // Set kernel arguments
      Status = gBS->HandleProtocol (
                      KernelHandle,
                      &gEfiLoadedImageProtocolGuid,
                      (VOID **)&ImageInfo
                      );
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      ImageInfo->LoadOptions     = NewArgs;
      ImageInfo->LoadOptionsSize = StrLen (NewArgs) * sizeof (CHAR16);
      DEBUG ((DEBUG_ERROR, "%s", (CHAR16 *)ImageInfo->LoadOptions));
    }

    // Before calling the image, enable the Watchdog Timer for  the 5 Minute period
    gBS->SetWatchdogTimer (5 * 60, 0x10000, 0, NULL);

    DEBUG ((DEBUG_ERROR, "%a: Cmdline: \n", __FUNCTION__));

    Status = gBS->StartImage (KernelHandle, NULL, NULL);

    // Clear the Watchdog Timer if the image returns
    gBS->SetWatchdogTimer (0, 0x10000, 0, NULL);

    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Unable to start image: %r\r\n", __FUNCTION__, Status);
    }
  }

Exit:
  // Unload fdt
  if (FdtUpdated) {
    gBS->InstallConfigurationTable (&gFdtTableGuid, OldFdtBase);
  }

  // Close handles
  if (RamDiskLoadFileHandle != NULL) {
    gBS->UninstallMultipleProtocolInterfaces (
           RamDiskLoadFileHandle,
           &gEfiLoadFile2ProtocolGuid,
           &mAndroidBootImgLoadFile2,
           &gEfiDevicePathProtocolGuid,
           &mRamdiskDevicePath,
           NULL
           );
  }

  // Free Memory
  if (KernelDevicePath != NULL) {
    FreePool (KernelDevicePath);
    KernelDevicePath = NULL;
  }

  if (ExpandedFdtBase != NULL) {
    FreePages (ExpandedFdtBase, EFI_SIZE_TO_PAGES (2 * fdt_totalsize (NewFdtBase)));
    ExpandedFdtBase = NULL;
  }

  if (NewFdtBase != NULL) {
    FreePool (NewFdtBase);
    NewFdtBase = NULL;
  }

  FdtSize = 0;

  if (mRamdiskData != NULL) {
    FreePool (mRamdiskData);
    mRamdiskData = NULL;
  }

  mRamdiskSize = 0;

  if (NewArgs != NULL) {
    FreePool (NewArgs);
    NewArgs = NULL;
  }

  if (Overlays != NULL) {
    FreePool (Overlays);
    Overlays = NULL;
  }

  if (OverlayBuffer != NULL) {
    FreePool (OverlayBuffer);
    OverlayBuffer = NULL;
  }

  return Status;
}

/**
  Process the boot mode selection from command line and variables

  @param[in]  LoadedImage     The LoadedImage protocol for this execution
  @param[out] BootParams      The current boot parameters

  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
ProcessBootParams (
  IN  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage,
  OUT L4T_BOOT_PARAMS            *BootParams
  )
{
  CONST CHAR16  *CurrentBootOption;
  EFI_STATUS    Status;
  UINT32        BootChain;
  UINTN         DataSize;
  UINT64        StringValue;

  if ((LoadedImage == NULL) || (BootParams == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  BootParams->BootChain = 0;

  DataSize = sizeof (BootParams->BootMode);
  Status   = gRT->GetVariable (L4T_BOOTMODE_VARIABLE_NAME, &gNVIDIAPublicVariableGuid, NULL, &DataSize, &BootParams->BootMode);
  if (EFI_ERROR (Status) || (BootParams->BootMode > NVIDIA_L4T_BOOTMODE_RECOVERY)) {
    BootParams->BootMode = NVIDIA_L4T_BOOTMODE_GRUB;
  }

  DataSize = sizeof (BootChain);
  Status   = gRT->GetVariable (BOOT_FW_VARIABLE_NAME, &gNVIDIAPublicVariableGuid, NULL, &DataSize, &BootChain);
  // If variable does not exist, is >4 bytes or has a value larger than 1, boot partition A
  if (!EFI_ERROR (Status) && (BootChain <= 1)) {
    BootParams->BootChain = BootChain;
  }

  // Read current OS boot type to allow for chaining
  DataSize = sizeof (BootChain);
  Status   = gRT->GetVariable (BOOT_OS_VARIABLE_NAME, &gNVIDIAPublicVariableGuid, NULL, &DataSize, &BootChain);
  // If variable does not exist, is >4 bytes or has a value larger than 1, boot partition A
  if (!EFI_ERROR (Status) && (BootChain <= 1)) {
    BootParams->BootChain = BootChain;
  }

  if (LoadedImage->LoadOptionsSize) {
    CurrentBootOption = StrStr (LoadedImage->LoadOptions, BOOTMODE_DIRECT_STRING);
    if (CurrentBootOption != NULL) {
      BootParams->BootMode = NVIDIA_L4T_BOOTMODE_DIRECT;
    }

    CurrentBootOption = StrStr (LoadedImage->LoadOptions, BOOTMODE_GRUB_STRING);
    if (CurrentBootOption != NULL) {
      BootParams->BootMode = NVIDIA_L4T_BOOTMODE_GRUB;
    }

    CurrentBootOption = StrStr (LoadedImage->LoadOptions, BOOTMODE_BOOTIMG_STRING);
    if (CurrentBootOption != NULL) {
      BootParams->BootMode = NVIDIA_L4T_BOOTMODE_BOOTIMG;
    }

    CurrentBootOption = StrStr (LoadedImage->LoadOptions, BOOTMODE_RECOVERY_STRING);
    if (CurrentBootOption != NULL) {
      BootParams->BootMode = NVIDIA_L4T_BOOTMODE_RECOVERY;
    }

    // See if boot option is passed in
    CurrentBootOption = StrStr (LoadedImage->LoadOptions, BOOTCHAIN_OVERRIDE_STRING);
    if (CurrentBootOption != NULL) {
      CurrentBootOption += StrLen (BOOTCHAIN_OVERRIDE_STRING);
      Status             = StrDecimalToUint64S (CurrentBootOption, NULL, &StringValue);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"Failed to read boot chain override: %r\r\n", Status);
      } else if (StringValue <= 1) {
        BootParams->BootChain = (UINT32)StringValue;
      } else {
        ErrorPrint (L"Boot chain override value out of range, ignoring\r\n");
      }
    }
  }

  // Find valid Rootfs Chain. If not, select recovery kernel
  Status = ValidateRootfsStatus (BootParams);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to validate rootfs status: %r\r\n", Status);
  }

  // Store the current boot chain in volatile variable to allow chain loading
  Status = gRT->SetVariable (BOOT_OS_VARIABLE_NAME, &gNVIDIAPublicVariableGuid, EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS, sizeof (BootParams->BootChain), &BootParams->BootChain);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to set OS variable: %r\r\n", Status);
  }

  return EFI_SUCCESS;
}

/**
  Reads an android style kernel partition located with Partition base
  name and bootchain.

  This function allocates memory for Image with AllocatePool; the
  caller is responsible for passing Image to FreePool after use.

  @param[in]  DeviceHandle      The handle of device where the partition lives on.
  @param[in]  PartitionBasename The base name of the partion where the image to boot is located.
  @param[in]  BootParams        Boot params for L4T.
  @param[out] Image             Pointer to the kernel image.
  @param[out] ImageSize         Size of the kernel image.

  @retval EFI_SUCCESS    The operation completed successfully.
  @retval !=EFI_SUCCESS  Errors occurred.

**/
STATIC
EFI_STATUS
ReadAndroidStyleKernelPartition (
  IN  CONST EFI_HANDLE               DeviceHandle,
  IN  CONST CHAR16           *CONST  PartitionBasename,
  IN  CONST L4T_BOOT_PARAMS  *CONST  BootParams,
  OUT       VOID            **CONST  Image,
  OUT       UINTN            *CONST  ImageSize
  )
{
  EFI_STATUS              Status;
  EFI_HANDLE              PartitionHandle;
  EFI_BLOCK_IO_PROTOCOL   *BlockIo;
  EFI_DISK_IO_PROTOCOL    *DiskIo;
  ANDROID_BOOTIMG_HEADER  ImageHeader;
  VOID                    *ImageBuffer = NULL;
  UINTN                   ImageBufferSize;
  UINTN                   SignatureOffset;
  UINT8                   Signature[SIZE_2KB];
  CONST UINTN             SignatureSize = sizeof (Signature);

  Status = FindPartitionInfo (
             DeviceHandle,
             PartitionBasename,
             BootParams->BootChain,
             NULL,
             &PartitionHandle
             );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to located partition\r\n", __FUNCTION__);
    goto Exit;
  }

  Status = gBS->HandleProtocol (
                  PartitionHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo
                  );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to locate block io protocol on partition\r\n", __FUNCTION__);
    goto Exit;
  }

  Status = gBS->HandleProtocol (
                  PartitionHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **)&DiskIo
                  );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to locate disk io protocol on partition\r\n", __FUNCTION__);
    goto Exit;
  }

  Status = DiskIo->ReadDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     0,
                     sizeof (ANDROID_BOOTIMG_HEADER),
                     &ImageHeader
                     );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to read disk\r\n");
    goto Exit;
  }

  Status = AndroidBootImgGetImgSize (&ImageHeader, &ImageBufferSize);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Android image header not seen\r\n");
    goto Exit;
  }

  ImageBuffer = AllocatePool (ImageBufferSize);
  if (ImageBuffer == NULL) {
    ErrorPrint (L"Failed to allocate buffer for Image\r\n");
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = DiskIo->ReadDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     0,
                     ImageBufferSize,
                     ImageBuffer
                     );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to read disk\r\n");
    goto Exit;
  }

  if (IsSecureBootEnabled ()) {
    SignatureOffset = ALIGN_VALUE (ImageBufferSize, SignatureSize);
    Status          = DiskIo->ReadDisk (
                                DiskIo,
                                BlockIo->Media->MediaId,
                                SignatureOffset,
                                SignatureSize,
                                Signature
                                );
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"Failed to read kernel image signature\r\n");
      goto Exit;
    }

    Status = VerifyDetachedSignature (
               Signature,
               SignatureSize,
               ImageBuffer,
               ImageBufferSize
               );
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"Failed to verify kernel image signature\r\n");
      goto Exit;
    }
  }

  *Image      = ImageBuffer;
  ImageBuffer = NULL;
  *ImageSize  = ImageBufferSize;

Exit:
  if (ImageBuffer != NULL) {
    FreePool (ImageBuffer);
  }

  return Status;
}

/**
  Reads an android style kernel dtb partition located with Partition base
  name and bootchain.

  This function allocates memory for Dtb with AllocatePool; the
  caller is responsible for passing Dtb to FreePool after use.

  @param[in]  DeviceHandle      The handle of device where this partition lives on.
  @param[in]  PartitionBasename The base name of the partion where the image to boot is located.
  @param[in]  BootParams        Boot params for L4T.
  @param[out] Dtb               Pointer to the allocated dtb buffer.
  @param[out] DtbSize           Size of the dtb buffer.

  @retval EFI_SUCCESS    The operation completed successfully.
  @retval !=EFI_SUCCESS  Errors occurred.

**/
STATIC
EFI_STATUS
ReadAndroidStyleDtbPartition (
  IN  CONST EFI_HANDLE               DeviceHandle,
  IN  CONST CHAR16           *CONST  PartitionBasename,
  IN  CONST L4T_BOOT_PARAMS  *CONST  BootParams,
  OUT       VOID            **CONST  Dtb,
  OUT       UINTN            *CONST  DtbSize
  )
{
  EFI_STATUS             Status;
  EFI_HANDLE             PartitionHandle;
  EFI_BLOCK_IO_PROTOCOL  *BlockIo;
  EFI_DISK_IO_PROTOCOL   *DiskIo;
  VOID                   *DtbBuffer;
  UINT64                 DtbBufferSize;
  UINTN                  Size;
  UINTN                  SignatureOffset;
  CONST UINTN            SignatureSize = SIZE_2KB;

  DtbBuffer = NULL;

  Status = FindPartitionInfo (
             DeviceHandle,
             PartitionBasename,
             BootParams->BootChain,
             NULL,
             &PartitionHandle
             );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to located partition\r\n", __FUNCTION__);
    goto Exit;
  }

  Status = gBS->HandleProtocol (
                  PartitionHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo
                  );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to locate block io protocol on partition\r\n", __FUNCTION__);
    goto Exit;
  }

  Status = gBS->HandleProtocol (
                  PartitionHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **)&DiskIo
                  );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to locate disk io protocol on partition\r\n", __FUNCTION__);
    goto Exit;
  }

  DtbBufferSize = MultU64x32 (BlockIo->Media->LastBlock + 1, BlockIo->Media->BlockSize);

  DtbBuffer = AllocatePool (DtbBufferSize);
  if (DtbBuffer == NULL) {
    ErrorPrint (L"Failed to allocate buffer for dtb\r\n");
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = DiskIo->ReadDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     0,
                     DtbBufferSize,
                     DtbBuffer
                     );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to read disk\r\n");
    goto Exit;
  }

  if (fdt_check_header ((UINT8 *)DtbBuffer) != 0) {
    ErrorPrint (L"DTB on partition was corrupted, trying to use UEFI DTB\r\n");
    goto Exit;
  }

  Size = fdt_totalsize ((UINT8 *)DtbBuffer);

  if (IsSecureBootEnabled ()) {
    SignatureOffset = ALIGN_VALUE (Size, SignatureSize);
    if (SignatureOffset + SignatureSize > DtbBufferSize) {
      ErrorPrint (L"DTB signature missing\r\n");
      Status = EFI_SECURITY_VIOLATION;
      goto Exit;
    }

    Status = VerifyDetachedSignature (
               (UINT8 *)DtbBuffer + SignatureOffset,
               SignatureSize,
               (UINT8 *)DtbBuffer,
               Size
               );
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"DTB signature invalid\r\n");
      goto Exit;
    }
  }

  *Dtb      = DtbBuffer;
  DtbBuffer = NULL;
  *DtbSize  = Size;

Exit:
  if (DtbBuffer != NULL) {
    FreePool (DtbBuffer);
  }

  return Status;
}

/**
  Boots an android style partition located with Partition base name and bootchain

  @param[in]  DeviceHandle      The handle of partition where this file lives on.
  @param[in]  PartitionBasename The base name of the partion where the image to boot is located.
  @param[in]  BootChain         Numeric version of the chain


  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
BootAndroidStylePartition (
  IN EFI_HANDLE       DeviceHandle,
  IN CONST CHAR16     *BootImgPartitionBasename,
  IN CONST CHAR16     *BootImgDtbPartitionBasename,
  IN L4T_BOOT_PARAMS  *BootParams
  )
{
  EFI_STATUS  Status;
  EFI_STATUS  Status1;
  VOID        *Image = NULL;
  UINTN       ImageSize;
  VOID        *AcpiBase;
  VOID        *Dtb = NULL;
  UINTN       DtbSize;
  VOID        *OldDtb;
  VOID        *NewDtb = NULL;
  UINTN       NewDtbPages;
  BOOLEAN     NewDtbInstalled = FALSE;

  Status = ReadAndroidStyleKernelPartition (
             DeviceHandle,
             BootImgPartitionBasename,
             BootParams,
             &Image,
             &ImageSize
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  do {
    Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
    if (!EFI_ERROR (Status)) {
      break;
    }

    Status = ReadAndroidStyleDtbPartition (
               DeviceHandle,
               BootImgDtbPartitionBasename,
               BootParams,
               &Dtb,
               &DtbSize
               );
    if (EFI_ERROR (Status)) {
      break;
    }

    NewDtbPages = EFI_SIZE_TO_PAGES (2 * DtbSize);
    NewDtb      = AllocatePages (NewDtbPages);
    if (NewDtb == NULL) {
      DEBUG ((DEBUG_WARN, "%a: failed to allocate pages for expanded kernel DTB\r\n", __FUNCTION__));
      break;
    }

    if (fdt_open_into ((UINT8 *)Dtb, NewDtb, EFI_PAGES_TO_SIZE (NewDtbPages)) != 0) {
      DEBUG ((DEBUG_WARN, "%a: failed to relocate kernel DTB\r\n", __FUNCTION__));
      break;
    }

    DEBUG ((DEBUG_ERROR, "%a: Installing Kernel DTB\r\n", __FUNCTION__));
    Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &OldDtb);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"No existing DTB\r\n");
      goto Exit;
    }

    Status = gBS->InstallConfigurationTable (&gFdtTableGuid, NewDtb);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"DTB Installation Failed\r\n");
      goto Exit;
    }

    NewDtbInstalled = TRUE;
  } while (FALSE);

  DEBUG ((DEBUG_ERROR, "%a: Cmdline: \n", __FUNCTION__));
  DEBUG ((DEBUG_ERROR, "%a", ((ANDROID_BOOTIMG_HEADER *)Image)->KernelArgs));

  Status = AndroidBootImgBoot (Image, ImageSize);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to boot image: %r\r\n", Status);
  }

Exit:
  if (NewDtbInstalled) {
    Status1 = gBS->InstallConfigurationTable (&gFdtTableGuid, OldDtb);
    if (EFI_ERROR (Status1)) {
      ErrorPrint (L"%a: Failed to re-install UEFI DTB: %r\r\n", __FUNCTION__, Status);
    }

    if (!EFI_ERROR (Status)) {
      Status = Status1;
    }
  }

  if (NewDtb != NULL) {
    FreePages (NewDtb, NewDtbPages);
  }

  if (Dtb != NULL) {
    FreePool (Dtb);
  }

  if (Image != NULL) {
    FreePool (Image);
  }

  return Status;
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers, including
  both device drivers and bus drivers.

  The entry point for StackCheck application that should casue an abort due to stack overwrite.

  @param[in] ImageHandle    The image handle of this application.
  @param[in] SystemTable    The pointer to the EFI System Table.

  @retval EFI_SUCCESS    The operation completed successfully.

**/
EFI_STATUS
EFIAPI
L4TLauncher (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_DEVICE_PATH            *FullDevicePath;
  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;
  EFI_STATUS                 Status;
  EFI_HANDLE                 LoadedImageHandle  = 0;
  EFI_HANDLE                 RootFsDeviceHandle = 0;
  L4T_BOOT_PARAMS            BootParams;
  EXTLINUX_BOOT_CONFIG       ExtLinuxConfig;
  UINTN                      ExtLinuxBootOption;
  UINTN                      Index;

  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to locate loaded image: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  Status = ProcessBootParams (LoadedImage, &BootParams);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to process boot parameters: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  if (BootParams.BootMode == NVIDIA_L4T_BOOTMODE_GRUB) {
    ErrorPrint (L"%a: Attempting GRUB Boot\r\n", __FUNCTION__);
    do {
      FullDevicePath = FileDevicePath (LoadedImage->DeviceHandle, GRUB_PATH);
      if (FullDevicePath == NULL) {
        ErrorPrint (L"%a: Failed to create full device path\r\n", __FUNCTION__);
        BootParams.BootMode = NVIDIA_L4T_BOOTMODE_DIRECT;
        break;
      }

      Status = gBS->LoadImage (FALSE, ImageHandle, FullDevicePath, NULL, 0, &LoadedImageHandle);
      if (EFI_ERROR (Status)) {
        if (Status != EFI_NOT_FOUND) {
          ErrorPrint (L"%a: Unable to load image: %r\r\n", __FUNCTION__, Status);
        }

        BootParams.BootMode = NVIDIA_L4T_BOOTMODE_DIRECT;
        break;
      }

      Status = UpdateBootConfig (LoadedImage->DeviceHandle, BootParams.BootChain);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Unable to update partition files\r\n", __FUNCTION__);
        BootParams.BootMode = NVIDIA_L4T_BOOTMODE_DIRECT;
        break;
      }

      // Before calling the image, enable the Watchdog Timer for  the 5 Minute period
      gBS->SetWatchdogTimer (5 * 60, 0x10000, 0, NULL);

      Status = gBS->StartImage (LoadedImageHandle, NULL, NULL);

      // Clear the Watchdog Timer if the image returns
      gBS->SetWatchdogTimer (0, 0x10000, 0, NULL);

      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Unable to start image: %r\r\n", __FUNCTION__, Status);
        break;
      }
    } while (FALSE);
  }

  if (BootParams.BootMode == NVIDIA_L4T_BOOTMODE_DIRECT) {
    ErrorPrint (L"%a: Attempting Direct Boot\r\n", __FUNCTION__);
    do {
      Status = ProcessExtLinuxConfig (LoadedImage->DeviceHandle, BootParams.BootChain, &ExtLinuxConfig, &RootFsDeviceHandle);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Unable to process extlinux config: %r\r\n", __FUNCTION__, Status);
        BootParams.BootMode = NVIDIA_L4T_BOOTMODE_BOOTIMG;
        break;
      }

      ExtLinuxBootOption = ExtLinuxBootMenu (&ExtLinuxConfig);

      Status = ExtLinuxBoot (ImageHandle, RootFsDeviceHandle, &ExtLinuxConfig.BootOptions[ExtLinuxBootOption]);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Unable to boot via extlinux: %r\r\n", __FUNCTION__, Status);
        BootParams.BootMode = NVIDIA_L4T_BOOTMODE_BOOTIMG;
        break;
      }
    } while (FALSE);

    for (Index = 0; Index < ExtLinuxConfig.NumberOfBootOptions; Index++) {
      if (ExtLinuxConfig.BootOptions[Index].BootArgs != NULL) {
        FreePool (ExtLinuxConfig.BootOptions[Index].BootArgs);
        ExtLinuxConfig.BootOptions[Index].BootArgs = NULL;
      }

      if (ExtLinuxConfig.BootOptions[Index].DtbPath != NULL) {
        FreePool (ExtLinuxConfig.BootOptions[Index].DtbPath);
        ExtLinuxConfig.BootOptions[Index].DtbPath = NULL;
      }

      if (ExtLinuxConfig.BootOptions[Index].InitrdPath != NULL) {
        FreePool (ExtLinuxConfig.BootOptions[Index].InitrdPath);
        ExtLinuxConfig.BootOptions[Index].InitrdPath = NULL;
      }

      if (ExtLinuxConfig.BootOptions[Index].Label != NULL) {
        FreePool (ExtLinuxConfig.BootOptions[Index].Label);
        ExtLinuxConfig.BootOptions[Index].Label = NULL;
      }

      if (ExtLinuxConfig.BootOptions[Index].LinuxPath != NULL) {
        FreePool (ExtLinuxConfig.BootOptions[Index].LinuxPath);
        ExtLinuxConfig.BootOptions[Index].LinuxPath = NULL;
      }

      if (ExtLinuxConfig.BootOptions[Index].MenuLabel != NULL) {
        FreePool (ExtLinuxConfig.BootOptions[Index].MenuLabel);
        ExtLinuxConfig.BootOptions[Index].MenuLabel = NULL;
      }
    }

    if (ExtLinuxConfig.MenuTitle != NULL) {
      FreePool (ExtLinuxConfig.MenuTitle);
      ExtLinuxConfig.MenuTitle = NULL;
    }
  }

  // Not in else to allow fallback
  if (BootParams.BootMode == NVIDIA_L4T_BOOTMODE_BOOTIMG) {
    ErrorPrint (L"%a: Attempting Kernel Boot\r\n", __FUNCTION__);
    Status = BootAndroidStylePartition (LoadedImage->DeviceHandle, BOOTIMG_BASE_NAME, BOOTIMG_DTB_BASE_NAME, &BootParams);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"Failed to boot %s:%d partition\r\n", BOOTIMG_BASE_NAME, BootParams.BootChain);
    }
  } else if (BootParams.BootMode == NVIDIA_L4T_BOOTMODE_RECOVERY) {
    ErrorPrint (L"%a: Attempting Recovery Boot\r\n", __FUNCTION__);
    Status = BootAndroidStylePartition (LoadedImage->DeviceHandle, RECOVERY_BASE_NAME, RECOVERY_DTB_BASE_NAME, &BootParams);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"Failed to boot %s:%d partition\r\n", RECOVERY_BASE_NAME, BootParams.BootChain);
    }
  }

  return Status;
}
