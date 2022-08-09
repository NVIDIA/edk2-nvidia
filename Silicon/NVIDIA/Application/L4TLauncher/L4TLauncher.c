/** @file
  The main process for L4TLauncher application.

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/SecureBootVariableLib.h>

#include <NVIDIAConfiguration.h>
#include <libfdt.h>
#include <Library/PlatformResourceLib.h>
#include "L4TLauncher.h"

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
  VOID                *EfiDBSig = NULL;
  EFI_STATUS          Status    = EFI_SUCCESS;
  UINTN               EfiDBSigSize;
  UINTN               EfiDBSigSizeTmp;
  EFI_SIGNATURE_LIST  *CertList;
  EFI_SIGNATURE_LIST  **CertDB  = NULL;
  UINTN               ListCount = 0;
  UINTN               CertIndex;

  EfiDBSigSize = 0;
  Status       = gRT->GetVariable (
                        VariableName,
                        &gEfiImageSecurityDatabaseGuid,
                        NULL,
                        &EfiDBSigSize,
                        NULL
                        );
  if (Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to Locate %s(%r)\n",
      __FUNCTION__,
      EFI_IMAGE_SECURITY_DATABASE,
      Status
      ));
    goto Error;
  }

  EfiDBSig = (UINT8 *)AllocateZeroPool (EfiDBSigSize);
  if (EfiDBSig == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to allocate Memory for DBCert %r\n",
      __FUNCTION__,
      Status
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto Error;
  }

  Status = gRT->GetVariable (VariableName, &gEfiImageSecurityDatabaseGuid, NULL, &EfiDBSigSize, (VOID *)EfiDBSig);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:%s Data Not Found  %r\n",
      __FUNCTION__,
      EFI_IMAGE_SECURITY_DATABASE,
      Status
      ));
    goto Error;
  }

  CertList        = (EFI_SIGNATURE_LIST *)EfiDBSig;
  EfiDBSigSizeTmp = EfiDBSigSize;

  // Walk the list to determine how many signatures are present.
  while ((EfiDBSigSizeTmp > 0) && (EfiDBSigSizeTmp >= CertList->SignatureListSize)) {
    ListCount++;
    EfiDBSigSizeTmp -= CertList->SignatureListSize;
    CertList         = (EFI_SIGNATURE_LIST *)((UINT8 *)CertList + CertList->SignatureListSize);
  }

  CertDB = (EFI_SIGNATURE_LIST **)AllocateZeroPool (sizeof (EFI_SIGNATURE_LIST *) * (ListCount + 1));
  if (CertDB == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Error;
  }

  CertList = (EFI_SIGNATURE_LIST *)EfiDBSig;
  for (CertIndex = 0; CertIndex < ListCount; CertIndex++) {
    CertDB[CertIndex] = CertList;
    CertList          = (EFI_SIGNATURE_LIST *)((UINT8 *)CertList + CertList->SignatureListSize);
  }

  // Keep the last entry NULL (what the PKCS lib code expects)
  CertDB[CertIndex] = NULL;
  return CertDB;
Error:
  if (EfiDBSig) {
    FreePool (EfiDBSig);
  }

  return NULL;
}

/*
 *
  OpenAndReadFileToBuffer

  Utility function to open a File with FileName and read the contents of the
  file into DstBuffer. This function will open the file, allocate a data buffer
  based on the file size and returns the Buffer ,File Handle and File Size.
  If there is an error in this util function, then it will clean up all the
  resources it allocated else upon success the caller has to free up the
  resources allocated (File Handle/Data Buffer)

  @param[in]  FsHandle         The handle of partition where this file lives on.
  @param[in]  FileName         Name of File to be processed
  @param[out] DstBuffer        Destination Buffer with File Data.
  @param[out] FileHandle       File Handle of the opened file.
  @param[out] FileSize         Size of the File opened.

  @retval EFI_SUCCESS          The operation completed successfully.
          EFI_OUT_OF_RESOURCES Failed buffer allocation.
          EFI_XXX              Error status from other APIs called.
 *
 */
STATIC
EFI_STATUS
OpenAndReadFileToBuffer (
  IN CONST EFI_HANDLE  FsHandle,
  IN CONST CHAR16      *FileName,
  OUT VOID             **DstBuffer,
  OUT EFI_FILE_HANDLE  *FileHandle,
  OUT UINT64           *FileSize
  )
{
  EFI_DEVICE_PATH  *FullDevicePath;
  EFI_DEVICE_PATH  *TmpFullDevicePath;
  EFI_STATUS       Status;

  FullDevicePath = FileDevicePath (FsHandle, FileName);
  if (FullDevicePath == NULL) {
    ErrorPrint (L"%a: Failed to create file device path\r\n", __FUNCTION__);
    Status = EFI_OUT_OF_RESOURCES;
    goto Error;
  }

  TmpFullDevicePath = FullDevicePath;
  Status            = EfiOpenFileByDevicePath (&TmpFullDevicePath, FileHandle, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    ErrorPrint (
      L"%a: Failed to open %s: %r\r\n",
      __FUNCTION__,
      FileName,
      Status
      );
    goto Error;
  }

  FileHandleGetSize (*FileHandle, FileSize);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to get file size: %r\r\n", __FUNCTION__, Status);
    goto Error;
  }

  *DstBuffer = AllocatePool (*FileSize);
  if (*DstBuffer == NULL) {
    ErrorPrint (
      L"%a: Failed to Allocate buffer for %s\r\n",
      __FUNCTION__,
      FileName
      );
    FileHandleClose (*FileHandle);
    Status = EFI_OUT_OF_RESOURCES;
    goto Error;
  }

  Status = FileHandleRead (*FileHandle, FileSize, *DstBuffer);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to read file\r\n", __FUNCTION__);
    goto Error;
  }

  FreePool (FullDevicePath);
  return Status;
Error:
  DEBUG ((DEBUG_ERROR, "%a: Cleanup\n", __FUNCTION__));
  if (*DstBuffer != NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Cleanup 1\n", __FUNCTION__));
    FreePool (*DstBuffer);
  }

  if (*FileHandle != NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Cleanup 2\n", __FUNCTION__));
    FileHandleClose (*FileHandle);
  }

  return Status;
}

/*
 *
  VerifyDetachedCertificateFile

  Verify a file that has a detached signature.
  For a given file name, read the file and its signature file contents in to
  data buffers, locate the signatures in DB and DBX (optional) and pass these
  to the PKCS Verify protocol to verify the file.
  The function returns the FileHandle of the file it opens and optionally the
  data buffer/size with the contents of the file.

  @param[in]   FileName        Name of File to be processed
  @param[in]   FsHandle        The handle of partition where this file lives on.
  @param[out]  FileHandle      The File Handle of the file for the caller to use.
  @param[out]  DataBuf         Optional parameter: The data buffer to put the
                               file contents in.
  @param[out]  DataSize        Optional parameter: The size of the file's data
                               contents.

  @retval EFI_SUCCESS          The operation completed successfully.
          EFI_OUT_OF_RESOURCES Failed buffer allocation
          EFI_XXX              Error status from other APIs called.
 *
 */
STATIC
EFI_STATUS
VerifyDetachedCertificateFile (
  IN CONST CHAR16      *FileName,
  IN CONST EFI_HANDLE  FsHandle,
  OUT EFI_FILE_HANDLE  *FileHandle,
  OUT VOID             **DataBuf OPTIONAL,
  OUT UINTN            *DataSize OPTIONAL
  )
{
  UINT8                      *SecureBootEnabled = NULL;
  EFI_FILE_HANDLE            FileSigHandle;
  CHAR16                     *NewFileName;
  VOID                       *FileData    = NULL;
  VOID                       *FileSigData = NULL;
  UINT64                     FileSize;
  UINT64                     FileSigSize;
  EFI_STATUS                 Status = EFI_SUCCESS;
  EFI_PKCS7_VERIFY_PROTOCOL  *PkcsVerifyProtocol;
  UINTN                      NewFileNameSize;

  FileSigHandle = NULL;

  GetVariable2 (
    EFI_SECURE_BOOT_ENABLE_NAME,
    &gEfiSecureBootEnableDisableGuid,
    (VOID **)&SecureBootEnabled,
    NULL
    );
  if (SecureBootEnabled && (*SecureBootEnabled == SECURE_BOOT_ENABLE)) {
    Status = OpenAndReadFileToBuffer (
               FsHandle,
               FileName,
               &FileData,
               FileHandle,
               &FileSize
               );
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"Error Reading %s \n", FileSize);
      goto Exit;
    }

    // The detached signature file should be <filename>.sig
    NewFileNameSize = StrSize (FileName) + StrSize (DETACHED_SIG_FILE_EXTENSION)
                      + sizeof (CHAR16);
    NewFileName = AllocateZeroPool (NewFileNameSize);
    if (NewFileName == NULL) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Cannot Allocate Buffer for NewFileName\n",
        __FUNCTION__
        ));
      Status = EFI_OUT_OF_RESOURCES;
      goto Error;
    }

    UnicodeSPrint (NewFileName, NewFileNameSize, L"%s%s", FileName, DETACHED_SIG_FILE_EXTENSION);
    Status = OpenAndReadFileToBuffer (
               FsHandle,
               NewFileName,
               &FileSigData,
               &FileSigHandle,
               &FileSigSize
               );
    if (EFI_ERROR (Status)) {
      ErrorPrint (
        L"%a: Failed to open/read Sig file %s\n",
        __FUNCTION__,
        NewFileName
        );
      goto Error;
    }

    Status = gBS->LocateProtocol (&gEfiPkcs7VerifyProtocolGuid, NULL, (VOID **)&PkcsVerifyProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a:Failed to locate PKCS Proto %r\n", __FUNCTION__, Status));
      goto Error;
    }

    // Do these steps once, to locate and setup the DB/DBX certs.
    if (AllowedDB == NULL) {
      AllowedDB = SetupCertList (EFI_IMAGE_SECURITY_DATABASE);
      if (AllowedDB == NULL) {
        DEBUG ((
          DEBUG_ERROR,
          "%a:Failed to setup Allowed DB %r\n",
          __FUNCTION__,
          Status
          ));
        goto Error;
      }
    }

    if (RevokedDB == NULL) {
      RevokedDB = SetupCertList (EFI_IMAGE_SECURITY_DATABASE1);
      if (RevokedDB == NULL) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Revoked DB not found(Not Fatal)\n",
          __FUNCTION__
          ));
      }
    }

    Status = PkcsVerifyProtocol->VerifyBuffer (
                                   PkcsVerifyProtocol,
                                   FileSigData,
                                   FileSigSize,
                                   FileData,
                                   FileSize,
                                   AllowedDB,
                                   RevokedDB,
                                   NULL,
                                   NULL,
                                   NULL
                                   );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a:PKCS7 Failed verification %r\n", __FUNCTION__, Status));
    } else {
      DEBUG ((DEBUG_INFO, "%a:PKCS7 Verification Success %r\n", __FUNCTION__, Status));
    }

Error:
    if (FileSigData) {
      FreePool (FileSigData);
    }

    if (FileSigHandle) {
      FileHandleClose (FileSigHandle);
    }

    if (NewFileName) {
      FreePool (NewFileName);
    }

    if (FileData && !DataBuf) {
      FreePool (FileData);
    } else {
      *DataBuf  = FileData;
      *DataSize = FileSize;
    }
  } else {
    DEBUG ((DEBUG_INFO, "%a: Secure Boot is not Enabled\n", __FUNCTION__));
  }

Exit:
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
  EFI_DEVICE_PATH  *FullDevicePath;
  EFI_FILE_HANDLE  FileHandle = NULL;
  CHAR16           *FileLine  = NULL;
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

  Status = VerifyDetachedCertificateFile (
             EXTLINUX_CONF_PATH,
             *RootFsHandle,
             &FileHandle,
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a:sds Failed to Authenticate %s (%r)\r\n", __FUNCTION__, EXTLINUX_CONF_PATH, Status);
    return Status;
  }

  if (FileHandle == NULL) {
    FullDevicePath = FileDevicePath (*RootFsHandle, EXTLINUX_CONF_PATH);
    if (FullDevicePath == NULL) {
      ErrorPrint (L"%a: Failed to create file device path\r\n", __FUNCTION__);
      return EFI_OUT_OF_RESOURCES;
    }

    Status = EfiOpenFileByDevicePath (&FullDevicePath, &FileHandle, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to open file: %r\r\n", __FUNCTION__, Status);
      return Status;
    }
  } else {
    FileHandleSetPosition (FileHandle, 0);
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
  EFI_DEVICE_PATH_PROTOCOL   *TempDevicePath       = NULL;
  EFI_DEVICE_PATH_PROTOCOL   *InitRdDevicePath     = NULL;
  EFI_FILE_HANDLE            InitRdFileHandle      = NULL;
  EFI_DEVICE_PATH_PROTOCOL   *FdtDevicePath        = NULL;
  EFI_FILE_HANDLE            FdtFileHandle         = NULL;
  UINTN                      FdtSize;
  VOID                       *AcpiBase         = NULL;
  VOID                       *OldFdtBase       = NULL;
  VOID                       *NewFdtBase       = NULL;
  VOID                       *ExpandedFdtBase  = NULL;
  BOOLEAN                    FdtUpdated        = FALSE;
  EFI_DEVICE_PATH_PROTOCOL   *KernelDevicePath = NULL;
  EFI_HANDLE                 KernelHandle      = NULL;
  EFI_LOADED_IMAGE_PROTOCOL  *ImageInfo;

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
    Status = VerifyDetachedCertificateFile (
               BootOption->InitrdPath,
               DeviceHandle,
               &InitRdFileHandle,
               &mRamdiskData,
               &mRamdiskSize
               );
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a:sds Failed to Authenticate %s (%r)\r\n", __FUNCTION__, BootOption->InitrdPath, Status);
      return Status;
    }

    if (InitRdFileHandle == NULL) {
      InitRdDevicePath = FileDevicePath (DeviceHandle, BootOption->InitrdPath);
      if (InitRdDevicePath == NULL) {
        ErrorPrint (L"%a: Failed to create file device path\r\n", __FUNCTION__);
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      TempDevicePath = InitRdDevicePath;
      Status         = EfiOpenFileByDevicePath (&TempDevicePath, &InitRdFileHandle, EFI_FILE_MODE_READ, 0);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Failed to open file: %s %r\r\n", __FUNCTION__, BootOption->InitrdPath, Status);
        goto Exit;
      }

      Status = FileHandleGetSize (InitRdFileHandle, &mRamdiskSize);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Failed to get file size: %r\r\n", __FUNCTION__, Status);
        goto Exit;
      }

      mRamdiskData = AllocatePool (mRamdiskSize);
      if (mRamdiskData == NULL) {
        ErrorPrint (L"%a: Failed to create ram disk buffer\r\n", __FUNCTION__);
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      Status = FileHandleRead (InitRdFileHandle, &mRamdiskSize, mRamdiskData);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Failed to read ram disk\r\n", __FUNCTION__);
        goto Exit;
      }
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

    Status = VerifyDetachedCertificateFile (
               BootOption->DtbPath,
               DeviceHandle,
               &FdtFileHandle,
               &NewFdtBase,
               &FdtSize
               );
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a:sds Failed to Authenticate %s (%r)\r\n", __FUNCTION__, EXTLINUX_CONF_PATH, Status);
      goto Exit;
    }

    if (FdtFileHandle == NULL) {
      FdtDevicePath = FileDevicePath (DeviceHandle, BootOption->DtbPath);
      if (FdtDevicePath == NULL) {
        ErrorPrint (L"%a: Failed to create file device path\r\n", __FUNCTION__);
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      TempDevicePath = FdtDevicePath;
      Status         = EfiOpenFileByDevicePath (&TempDevicePath, &FdtFileHandle, EFI_FILE_MODE_READ, 0);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Failed to open file: %s %r\r\n", __FUNCTION__, BootOption->DtbPath, Status);
        goto Exit;
      }

      Status = FileHandleGetSize (FdtFileHandle, &FdtSize);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Failed to get file size: %r\r\n", __FUNCTION__, Status);
        goto Exit;
      }

      NewFdtBase = AllocatePool (FdtSize);
      if (NewFdtBase == NULL) {
        ErrorPrint (L"%a: Failed to create FDT buffer\r\n", __FUNCTION__);
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      Status = FileHandleRead (FdtFileHandle, &FdtSize, NewFdtBase);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"%a: Failed to read fdt\r\n", __FUNCTION__);
        goto Exit;
      }
    }

    ExpandedFdtBase = AllocatePages (EFI_SIZE_TO_PAGES (2 * fdt_totalsize (NewFdtBase)));
    if (fdt_open_into (NewFdtBase, ExpandedFdtBase, 2 * fdt_totalsize (NewFdtBase)) != 0) {
      Status = EFI_NOT_FOUND;
      goto Exit;
    }

    Status = gBS->InstallConfigurationTable (&gFdtTableGuid, ExpandedFdtBase);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to install fdt\r\n", __FUNCTION__);
      goto Exit;
    }

    FdtUpdated = TRUE;
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
      DEBUG ((DEBUG_ERROR, "%s", ImageInfo->LoadOptions));
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

  if (InitRdFileHandle != NULL) {
    FileHandleClose (InitRdFileHandle);
    InitRdFileHandle = NULL;
  }

  if (FdtFileHandle != NULL) {
    FileHandleClose (FdtFileHandle);
    FdtFileHandle = NULL;
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
  if (FdtDevicePath != NULL) {
    FreePool (FdtDevicePath);
    FdtDevicePath = NULL;
  }

  if (mRamdiskData != NULL) {
    FreePool (mRamdiskData);
    mRamdiskData = NULL;
  }

  mRamdiskSize = 0;
  if (InitRdDevicePath != NULL) {
    FreePool (InitRdDevicePath);
    InitRdDevicePath = NULL;
  }

  if (NewArgs != NULL) {
    FreePool (NewArgs);
    NewArgs = NULL;
  }

  return Status;
}

/**
  Initialize rootfs status register

  @param[in]  RootfsSlot          Rootfs slot
  @param[in]  RootfsInfo          Value of variable RootfsInfo
  @param[out] RegisterValueRf     Value of rootfs status register

  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
InitializeRootfsStatusReg (
  IN  UINT32  RootfsSlot,
  IN  UINT32  RootfsInfo,
  OUT UINT32  *RegisterValueRf
  )
{
  EFI_STATUS  Status;
  UINT32      RegisterValue;
  UINT32      RetryCount;
  UINT32      MaxRetryCount;
  UINT32      RootfsStatus;

  Status = GetRootfsStatusReg (&RegisterValue);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to get rootfs status register\r\n", __FUNCTION__);
    return Status;
  }

  if (SR_RF_MAGIC_GET (RegisterValue) == SR_RF_MAGIC) {
    // Rootfs Status Reg has been properly set in previous boot
    goto Exit;
  }

  // This is first boot. Initialize SR_RF
  RegisterValue = 0;

  // Set magic
  RegisterValue = SR_RF_MAGIC_SET (RegisterValue);

  RegisterValue = SR_RF_CURRENT_SLOT_SET (RootfsSlot, RegisterValue);

  // Set retry count according to the rootfs status
  MaxRetryCount = RF_INFO_MAX_RETRY_CNT_GET (RootfsInfo);
  RootfsStatus  = RF_INFO_STATUS_A_GET (RootfsInfo);
  if (RootfsStatus == ROOTFS_UNBOOTABLE) {
    RetryCount = 0;
  } else {
    RetryCount = MaxRetryCount;
  }

  RegisterValue = SR_RF_RETRY_COUNT_A_SET (RetryCount, RegisterValue);

  RootfsStatus = RF_INFO_STATUS_B_GET (RootfsInfo);
  if (RootfsStatus == ROOTFS_UNBOOTABLE) {
    RetryCount = 0;
  } else {
    RetryCount = MaxRetryCount;
  }

  RegisterValue = SR_RF_RETRY_COUNT_B_SET (RetryCount, RegisterValue);

  // Write Rootfs Status register
  Status = SetRootfsStatusReg (RegisterValue);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to set Rootfs status register: %r\r\n", __FUNCTION__, Status);
  }

Exit:
  *RegisterValueRf = RegisterValue;
  return Status;
}

/**
  Set rootfs status value to RootfsInfo

  @param[in]  RootfsSlot      Current rootfs slot
  @param[in]  RootfsStatus    Value of rootfs status
  @param[out] RootfsInfo      The value of RootfsInfo variable

  @retval EFI_SUCCESS            The operation completed successfully.
  @retval EFI_INVALID_PARAMETER  Input parameter invalid

**/
STATIC
EFI_STATUS
EFIAPI
SetStatusToRootfsInfo (
  IN  UINT32  RootfsSlot,
  IN  UINT32  RootfsStatus,
  OUT UINT32  *RootfsInfo
  )
{
  if ((RootfsSlot > ROOTFS_SLOT_B) || (RootfsInfo == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (RootfsSlot == ROOTFS_SLOT_A) {
    *RootfsInfo = RF_INFO_STATUS_A_SET (RootfsStatus, *RootfsInfo);
  } else {
    *RootfsInfo = RF_INFO_STATUS_B_SET (RootfsStatus, *RootfsInfo);
  }

  return EFI_SUCCESS;
}

/**
  Get rootfs retry count from RootfsInfo

  @param[in]  RootfsInfo          The value of RootfsInfo variable
  @param[in]  RootfsSlot          Current rootfs slot
  @param[out] RootfsRetryCount    Rertry count of current RootfsSlot

  @retval EFI_SUCCESS            The operation completed successfully.
  @retval EFI_INVALID_PARAMETER  Input parameter invalid

**/
STATIC
EFI_STATUS
EFIAPI
GetRetryCountFromRootfsInfo (
  IN  UINT32  RootfsInfo,
  IN  UINT32  RootfsSlot,
  OUT UINT32  *RootfsRetryCount
  )
{
  if ((RootfsSlot > ROOTFS_SLOT_B) || (RootfsRetryCount == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (RootfsSlot == ROOTFS_SLOT_A) {
    *RootfsRetryCount = RF_INFO_RETRY_CNT_A_GET (RootfsInfo);
  } else {
    *RootfsRetryCount = RF_INFO_RETRY_CNT_B_GET (RootfsInfo);
  }

  return EFI_SUCCESS;
}

/**
  Set rootfs retry count value to RootfsInfo

  @param[in]  RootfsSlot          Current rootfs slot
  @param[in]  RootfsRetryCount    Rootfs retry count of current RootfsSlot
  @param[out] RootfsInfo          The value of RootfsInfo variable

  @retval EFI_SUCCESS            The operation completed successfully.
  @retval EFI_INVALID_PARAMETER  Input parameter invalid

**/
STATIC
EFI_STATUS
EFIAPI
SetRetryCountToRootfsInfo (
  IN  UINT32  RootfsSlot,
  IN  UINT32  RootfsRetryCount,
  OUT UINT32  *RootfsInfo
  )
{
  if ((RootfsSlot > ROOTFS_SLOT_B) || (RootfsInfo == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (RootfsSlot == ROOTFS_SLOT_A) {
    *RootfsInfo = RF_INFO_RETRY_CNT_A_SET (RootfsRetryCount, *RootfsInfo);
  } else {
    *RootfsInfo = RF_INFO_RETRY_CNT_B_SET (RootfsRetryCount, *RootfsInfo);
  }

  return EFI_SUCCESS;
}

/**
  Sync the Rootfs status register and RootfsInfo variable according to
  the specified direction

  @param[in]     Direction          The sync up direction
  @param[in/out] RegisterValue      The pointer of Rootfs Status register
  @param[in/out] RootfsInfo         The pointer of RootfsInfo variable

  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
SyncSrRfAndRootfsInfo (
  IN  UINT32  Direction,
  OUT UINT32  *RegisterValue,
  OUT UINT32  *RootfsInfo
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT32      RetryCount;
  UINT32      RootfsSlot;

  switch (Direction) {
    case FROM_REG_TO_VAR:
      // Copy RootfsSlot from Scratch Register to variable
      RootfsSlot  = SR_RF_CURRENT_SLOT_GET (*RegisterValue);
      *RootfsInfo = RF_INFO_CURRENT_SLOT_SET (RootfsSlot, *RootfsInfo);

      // Copy RetryCountA and RetryCountB from Scratch Register to variable
      RetryCount  = SR_RF_RETRY_COUNT_A_GET (*RegisterValue);
      *RootfsInfo = RF_INFO_RETRY_CNT_A_SET (RetryCount, *RootfsInfo);
      RetryCount  = SR_RF_RETRY_COUNT_B_GET (*RegisterValue);
      *RootfsInfo = RF_INFO_RETRY_CNT_B_SET (RetryCount, *RootfsInfo);
      break;
    case FROM_VAR_TO_REG:
      // Copy RootfsSlot from variable to Scratch Register
      RootfsSlot     = RF_INFO_CURRENT_SLOT_GET (*RootfsInfo);
      *RegisterValue = SR_RF_CURRENT_SLOT_SET (RootfsSlot, *RegisterValue);

      // Copy RetryCountA and RetryCountB from Variable to Scratch Register
      RetryCount     = RF_INFO_RETRY_CNT_A_GET (*RootfsInfo);
      *RegisterValue = SR_RF_RETRY_COUNT_A_SET (RetryCount, *RegisterValue);
      RetryCount     = RF_INFO_RETRY_CNT_B_GET (*RootfsInfo);
      *RegisterValue = SR_RF_RETRY_COUNT_B_SET (RetryCount, *RegisterValue);
      break;
    default:
      break;
  }

  return Status;
}

/**
  Check if there is valid rootfs or not

  @param[in] RootfsInfo      The value of RootfsInfo variable

  @retval TRUE     There is valid rootfs
  @retval FALSE    There is no valid rootfs

**/
STATIC
BOOLEAN
EFIAPI
IsValidRootfs (
  IN UINT32  RootfsInfo
  )
{
  UINT32   AbRedundancyLevel;
  BOOLEAN  Status = TRUE;

  AbRedundancyLevel = RF_INFO_REDUNDANCY_GET (RootfsInfo);

  if ((AbRedundancyLevel == REDUNDANCY_BOOT_ONLY) &&
      (RF_INFO_STATUS_A_GET (RootfsInfo) == ROOTFS_UNBOOTABLE))
  {
    Status = FALSE;
  }

  if ((AbRedundancyLevel == REDUNDANCY_BOOT_ROOTFS) &&
      (RF_INFO_STATUS_A_GET (RootfsInfo) == ROOTFS_UNBOOTABLE) &&
      (RF_INFO_STATUS_B_GET (RootfsInfo) == ROOTFS_UNBOOTABLE))
  {
    Status = FALSE;
  }

  return Status;
}

/**
  Check RootfsInfo, update the variable if there is any changes

  @param[in] RootfsInfo      The value of RootfsInfo variable
  @param[in] RootfsInfo      The backup of RootfsInfo

  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
CheckAndUpdateRootfsInfo (
  IN UINT32  RootfsInfo,
  IN UINT32  RootfsInfoBackup
  )
{
  EFI_STATUS  Status;
  UINT32      RetryCount;
  UINT32      DataSize;

  // Sync up the Backup RetryCount
  RetryCount       = RF_INFO_RETRY_CNT_A_GET (RootfsInfo);
  RootfsInfoBackup = RF_INFO_RETRY_CNT_A_SET (RetryCount, RootfsInfoBackup);
  RetryCount       = RF_INFO_RETRY_CNT_B_GET (RootfsInfo);
  RootfsInfoBackup = RF_INFO_RETRY_CNT_B_SET (RetryCount, RootfsInfoBackup);

  // Not update RootfsInfo
  if (RootfsInfo == RootfsInfoBackup) {
    return EFI_SUCCESS;
  }

  // RootfsInfo has been changed, update it.
  DataSize = sizeof (RootfsInfo);
  Status   = gRT->SetVariable (
                    ROOTFS_INFO_VARIABLE_NAME,
                    &gNVIDIAPublicVariableGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS|EFI_VARIABLE_NON_VOLATILE,
                    DataSize,
                    &RootfsInfo
                    );

  return Status;
}

/**
  Check input rootfs slot is bootable or not:
  If the retry count of rootfs slot is not 0, rootfs slot is bootable, decrease
  the retry count in RootfsInfo by 1.
  If the retry count of rootfs slot is 0, rootfs slot is unbootable, set status
  of the rootfs slot in RootfsInfo to ROOTFS_UNBOOTABLE.

  @param[in] RootfsSlot      The rootfs slot number
  @param[in] RootfsInfo      The RootfsInfo pointer

  @retval TRUE     The input rootfs slot is bootable
  @retval FALSE    The input rootfs slot is unbootable

**/
STATIC
BOOLEAN
EFIAPI
IsRootfsSlotBootable (
  IN UINT32  RootfsSlot,
  IN UINT32  *RootfsInfo
  )
{
  EFI_STATUS  Status;
  UINT32      RetryCount;
  BOOLEAN     Bootable = FALSE;

  Status = GetRetryCountFromRootfsInfo (*RootfsInfo, RootfsSlot, &RetryCount);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to Get Rootfs retry count of slot %d from RootfsInfo: %r\r\n", __FUNCTION__, RootfsSlot, Status);
    goto Exit;
  }

  // The rootfs slot is bootable.
  if (RetryCount != 0) {
    RetryCount--;
    Status = SetRetryCountToRootfsInfo (RootfsSlot, RetryCount, RootfsInfo);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to set retry count of slot %d to RootfsInfo: %r\r\n", __FUNCTION__, RootfsSlot, Status);
      goto Exit;
    }

    Bootable = TRUE;
  } else {
    // The rootfs slot is unbootable
    Bootable = FALSE;
    Status   = SetStatusToRootfsInfo (RootfsSlot, ROOTFS_UNBOOTABLE, RootfsInfo);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to set Rootfs status of slot %d to RootfsInfo: %r\r\n", __FUNCTION__, RootfsSlot, Status);
      goto Exit;
    }
  }

Exit:
  return Bootable;
}

/**
  Validate rootfs A/B status and update BootMode and BootChain accordingly, basic flow:
  If there is no rootfs B,
     (1) boot to rootfs A if retry count of rootfs A is not 0;
     (2) boot to recovery if rtry count of rootfs A is 0.
  If there is rootfs B,
     (1) boot to current rootfs slot if the retry count of current slot is not 0;
     (2) switch to non-current rootfs slot if the retry count of current slot is 0
         and non-current rootfs is bootable
     (3) boot to recovery if both rootfs slots are invalid.

  @param[out] BootParams      The current rootfs boot parameters

  @retval EFI_SUCCESS    The operation completed successfully.

**/
STATIC
EFI_STATUS
EFIAPI
ValidateRootfsStatus (
  OUT L4T_BOOT_PARAMS  *BootParams
  )
{
  EFI_STATUS  Status;
  UINT32      RegisterValueRf;
  UINT32      RootfsInfo = 0;
  UINT32      RootfsInfoBackup;
  UINTN       DataSize;
  UINT32      CurrentSlot;
  UINT32      NonCurrentSlot;
  UINT32      AbRedundancyLevel;
  UINT32      BootChainOverride;

  // If boot mode has been set to RECOVERY (via runtime service or UEFI menu),
  // boot to recovery
  if (BootParams->BootMode == NVIDIA_L4T_BOOTMODE_RECOVERY) {
    return EFI_SUCCESS;
  }

  if (BootParams->BootChain > ROOTFS_SLOT_B) {
    ErrorPrint (L"%a: Invalid BootChain: %d\r\n", __FUNCTION__, BootParams->BootChain);
    return EFI_INVALID_PARAMETER;
  }

  // Read RootfsInfo and backup it
  DataSize = sizeof (RootfsInfo);
  Status   = gRT->GetVariable (
                    ROOTFS_INFO_VARIABLE_NAME,
                    &gNVIDIAPublicVariableGuid,
                    NULL,
                    &DataSize,
                    &RootfsInfo
                    );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to get RootfsInfo variable: %r\r\n", __FUNCTION__, Status);
    goto Exit;
  }

  RootfsInfoBackup = RootfsInfo;

  // Initilize SR_RF if magic field of SR_RF is invalid
  Status = InitializeRootfsStatusReg (BootParams->BootChain, RootfsInfo, &RegisterValueRf);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to initialize rootfs status register: %r\r\n", __FUNCTION__, Status);
    goto Exit;
  }

  // Update the RootfsInfo to the latest from:
  // BootChainOverride, RootfsStatusReg and BootParams->BootChain
  DataSize = sizeof (BootChainOverride);
  Status   = gRT->GetVariable (
                    BOOT_OS_OVERRIDE_VARIABLE_NAME,
                    &gNVIDIAPublicVariableGuid,
                    NULL,
                    &DataSize,
                    &BootChainOverride
                    );
  if (!EFI_ERROR (Status) && (BootChainOverride == NVIDIA_OS_OVERRIDE_DEFAULT)) {
    RootfsInfo = RF_INFO_SLOT_LINK_FW_SET (RF_INFO_SLOT_LINK_FW, RootfsInfo);
  } else {
    RootfsInfo = RF_INFO_SLOT_LINK_FW_SET (RF_INFO_SLOT_NOT_LINK_FW, RootfsInfo);
  }

  // Three fields are updated from SR_RF:
  // 1. CurrentSlot
  // 2. Retry Count A
  // 3. Retry Count B
  SyncSrRfAndRootfsInfo (FROM_REG_TO_VAR, &RegisterValueRf, &RootfsInfo);

  // When the BootChainOverride value is 0 or 1, the value is set to BootParams->BootChain
  // in ProcessBootParams(), before calling ValidateRootfsStatus()
  RootfsInfo = RF_INFO_CURRENT_SLOT_SET (BootParams->BootChain, RootfsInfo);

  // Set BootMode to RECOVERY if there is no more valid rootfs
  if (!IsValidRootfs (RootfsInfo)) {
    BootParams->BootMode = NVIDIA_L4T_BOOTMODE_RECOVERY;
    return EFI_SUCCESS;
  }

  // Check redundancy level and validate rootfs status
  AbRedundancyLevel = RF_INFO_REDUNDANCY_GET (RootfsInfo);
  CurrentSlot       = RF_INFO_CURRENT_SLOT_GET (RootfsInfo);

  switch (AbRedundancyLevel) {
    case REDUNDANCY_BOOT_ONLY:
      // There is no rootfs B. Ensure to set rootfs slot to A.
      if (CurrentSlot != ROOTFS_SLOT_A) {
        CurrentSlot = ROOTFS_SLOT_A;
        RootfsInfo  = RF_INFO_CURRENT_SLOT_SET (CurrentSlot, RootfsInfo);
      }

      // If current slot is bootable, go on boot; if current slot is unbootable, boot to recovery kernel.
      if (!IsRootfsSlotBootable (CurrentSlot, &RootfsInfo)) {
        BootParams->BootMode = NVIDIA_L4T_BOOTMODE_RECOVERY;
      }

      break;
    case REDUNDANCY_BOOT_ROOTFS:
      // Redundancy for both bootloader and rootfs.
      // Go on boot if current slot is bootable.
      if (!IsRootfsSlotBootable (CurrentSlot, &RootfsInfo)) {
        // Current slot is unbootable, check non-current slot
        NonCurrentSlot = !CurrentSlot;

        if (IsRootfsSlotBootable (NonCurrentSlot, &RootfsInfo)) {
          // Non-current slot is bootable, switch to it.
          // Change UEFI boot chain (BootParams->BootChain) will be done at the end of this function
          RootfsInfo = RF_INFO_CURRENT_SLOT_SET (NonCurrentSlot, RootfsInfo);

          // Rootfs slot is not linked with bootloader chain
          BootChainOverride = NonCurrentSlot;
          DataSize = sizeof (BootChainOverride);
          Status   = gRT->SetVariable (
                    BOOT_OS_OVERRIDE_VARIABLE_NAME,
                    &gNVIDIAPublicVariableGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS|EFI_VARIABLE_NON_VOLATILE,
                    DataSize,
                    &BootChainOverride
                    );
          if (EFI_ERROR (Status)) {
            ErrorPrint (L"Failed to set OS override variable: %r\r\n", Status);
          }
        } else {
          // Non-current slot is unbootable, boot to recovery kernel.
          BootParams->BootMode = NVIDIA_L4T_BOOTMODE_RECOVERY;
        }
      }

      break;
    default:
      ErrorPrint (L"%a: Unsupported A/B redundancy level: %d\r\n", __FUNCTION__, AbRedundancyLevel);
      break;
  }

Exit:
  if (Status == EFI_SUCCESS) {
    // Sync RootfsInfo to RootfsStatusReg and save to register
    Status = SyncSrRfAndRootfsInfo (FROM_VAR_TO_REG, &RegisterValueRf, &RootfsInfo);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to sync RootfsInfo to Rootfs status register: %r\r\n", __FUNCTION__, Status);
      return Status;
    }

    Status = SetRootfsStatusReg (RegisterValueRf);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to set Rootfs status register (0x%x): %r\r\n", __FUNCTION__, RegisterValueRf, Status);
      return Status;
    }

    // Update BootParams->BootChain
    BootParams->BootChain = RF_INFO_CURRENT_SLOT_GET (RootfsInfo);

    // Set RootfsInfo variable if it is changed (Check RootfsInfo except
    // the RetryCount field, it is decreased in every normal boot).
    Status = CheckAndUpdateRootfsInfo (RootfsInfo, RootfsInfoBackup);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to check and update RootfsInfo: %r\r\n", __FUNCTION__, Status);
    }
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

  // Read override OS boot type
  DataSize = sizeof (BootChain);
  Status   = gRT->GetVariable (BOOT_OS_OVERRIDE_VARIABLE_NAME, &gNVIDIAPublicVariableGuid, NULL, &DataSize, &BootChain);
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
  EFI_STATUS              Status;
  EFI_HANDLE              PartitionHandle;
  EFI_BLOCK_IO_PROTOCOL   *BlockIo = NULL;
  EFI_DISK_IO_PROTOCOL    *DiskIo  = NULL;
  ANDROID_BOOTIMG_HEADER  ImageHeader;
  UINTN                   ImageSize;
  VOID                    *Image;
  UINT32                  Offset = 0;
  UINT64                  Size;
  VOID                    *KernelDtb;
  VOID                    *Dtb;
  VOID                    *ExpandedDtb;
  VOID                    *CurrentDtb;
  VOID                    *AcpiBase;

  Status = FindPartitionInfo (DeviceHandle, BootImgPartitionBasename, BootParams->BootChain, NULL, &PartitionHandle);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to located partition\r\n", __FUNCTION__);
    return Status;
  }

  Status = gBS->HandleProtocol (PartitionHandle, &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to locate block io protocol on partition\r\n", __FUNCTION__);
    goto Exit;
  }

  Status = gBS->HandleProtocol (PartitionHandle, &gEfiDiskIoProtocolGuid, (VOID **)&DiskIo);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Unable to locate disk io protocol on partition\r\n", __FUNCTION__);
    goto Exit;
  }

  Status = DiskIo->ReadDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     Offset,
                     sizeof (ANDROID_BOOTIMG_HEADER),
                     &ImageHeader
                     );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to read disk\r\n");
    goto Exit;
  }

  Status = AndroidBootImgGetImgSize (&ImageHeader, &ImageSize);
  if (EFI_ERROR (Status)) {
    Offset = FixedPcdGet32 (PcdSignedImageHeaderSize);
    Status = DiskIo->ReadDisk (
                       DiskIo,
                       BlockIo->Media->MediaId,
                       Offset,
                       sizeof (ANDROID_BOOTIMG_HEADER),
                       &ImageHeader
                       );
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"Failed to read disk\r\n");
      goto Exit;
    }

    Status = AndroidBootImgGetImgSize (&ImageHeader, &ImageSize);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"Header not seen at either offset 0 or offset 0x%x\r\n", Offset);
      goto Exit;
    }
  }

  Image = AllocatePool (ImageSize);
  if (Image == NULL) {
    ErrorPrint (L"Failed to allocate buffer for Image\r\n");
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = DiskIo->ReadDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     Offset,
                     ImageSize,
                     Image
                     );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to read disk\r\n");
    goto Exit;
  }

  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (EFI_ERROR (Status)) {
    Status = FindPartitionInfo (DeviceHandle, BootImgDtbPartitionBasename, BootParams->BootChain, NULL, &PartitionHandle);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Unable to located partition\r\n", __FUNCTION__);
      return Status;
    }

    Status = gBS->HandleProtocol (PartitionHandle, &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Unable to locate block io protocol on partition\r\n", __FUNCTION__);
      goto Exit;
    }

    Status = gBS->HandleProtocol (PartitionHandle, &gEfiDiskIoProtocolGuid, (VOID **)&DiskIo);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Unable to locate disk io protocol on partition\r\n", __FUNCTION__);
      goto Exit;
    }

    Size = MultU64x32 (BlockIo->Media->LastBlock+1, BlockIo->Media->BlockSize);

    KernelDtb = AllocatePool (Size);
    if (KernelDtb == NULL) {
      ErrorPrint (L"Failed to allocate buffer for dtb\r\n");
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    Status = DiskIo->ReadDisk (
                        DiskIo,
                        BlockIo->Media->MediaId,
                        0,
                        Size,
                        KernelDtb
                        );
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"Failed to read disk\r\n");
      goto Exit;
    }

    Dtb = KernelDtb;
    if (fdt_check_header (Dtb) != 0) {
      Dtb += PcdGet32 (PcdSignedImageHeaderSize);
      if (fdt_check_header (Dtb) != 0) {
        ErrorPrint (L"DTB on partition was corrupted, attempt use to UEFI DTB\r\n");
        goto Exit;
      }
    }

    ExpandedDtb = AllocatePages (EFI_SIZE_TO_PAGES (2 * fdt_totalsize (Dtb)));
    if ((ExpandedDtb != NULL) &&
        (fdt_open_into (Dtb, ExpandedDtb, 2 * fdt_totalsize (Dtb)) == 0))
    {
      DEBUG ((DEBUG_ERROR, "%a: Installing Kernel DTB\r\n", __FUNCTION__));
      Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &CurrentDtb);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"No existing DTB\r\n");
        goto Exit;
      }

      Status = gBS->InstallConfigurationTable (&gFdtTableGuid, ExpandedDtb);
      if (EFI_ERROR (Status)) {
        ErrorPrint (L"DTB Installation Failed\r\n");
        gBS->FreePages ((EFI_PHYSICAL_ADDRESS)ExpandedDtb, EFI_SIZE_TO_PAGES (fdt_totalsize (ExpandedDtb)));
        ExpandedDtb = NULL;
        goto Exit;
      }
    }
  }

  DEBUG ((DEBUG_ERROR, "%a: Cmdline: \n", __FUNCTION__));
  DEBUG ((DEBUG_ERROR, "%a", ImageHeader.KernelArgs));

  Status = AndroidBootImgBoot (Image, ImageSize);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"Failed to boot image: %r\r\n", Status);
    gBS->FreePages ((EFI_PHYSICAL_ADDRESS)ExpandedDtb, EFI_SIZE_TO_PAGES (fdt_totalsize (ExpandedDtb)));
    ExpandedDtb = NULL;
  }

  Status = gBS->InstallConfigurationTable (&gFdtTableGuid, CurrentDtb);
Exit:
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
