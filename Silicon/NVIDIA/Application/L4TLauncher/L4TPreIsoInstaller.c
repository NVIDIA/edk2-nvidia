/** @file
  PreIsoInstaller logic for L4TLauncher.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/FileHandleLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HandleParsingLib.h>
#include <Library/FdtLib.h>

#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/BlockIo.h>
#include <Protocol/Hash2.h>
#include <Protocol/ServiceBinding.h>

#include <Guid/Fdt.h>
#include <Guid/SystemResourceTable.h>
#include <Protocol/Eeprom.h>

#include "L4TLauncher.h"
#include "L4TPreIsoInstaller.h"

#define LOG_LINE_MAX  512

STATIC EFI_FILE_HANDLE  mLogFileHandle = NULL;

VOID
EFIAPI
PreIsoLogInit (
  IN EFI_HANDLE  DeviceHandle
  )
{
  EFI_STATUS       Status;
  EFI_DEVICE_PATH  *LogDevicePath;
  EFI_DEVICE_PATH  *LogDevicePathToFree;
  EFI_TIME         Time;
  CHAR8            Header[LOG_LINE_MAX];

  if (mLogFileHandle != NULL) {
    return;
  }

  LogDevicePath = FileDevicePath (DeviceHandle, LOG_FILE_PATH);
  if (LogDevicePath == NULL) {
    return;
  }

  LogDevicePathToFree = LogDevicePath;
  Status              = EfiOpenFileByDevicePath (
                          &LogDevicePath,
                          &mLogFileHandle,
                          EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                          0
                          );
  FreePool (LogDevicePathToFree);
  if (EFI_ERROR (Status)) {
    mLogFileHandle = NULL;
    return;
  }

  {
    UINT64  FileSize;
    FileSize = 0;
    Status   = FileHandleGetSize (mLogFileHandle, &FileSize);
    if (EFI_ERROR (Status)) {
      FileSize = 0;
    }

    if (FileSize > LOG_FILE_MAX_SIZE) {
      FileHandleSetPosition (mLogFileHandle, 0);
      FileHandleSetSize (mLogFileHandle, 0);
      FileSize = 0;
    }

    FileHandleSetPosition (mLogFileHandle, FileSize);
  }

  ZeroMem (&Time, sizeof (Time));
  gRT->GetTime (&Time, NULL);

  AsciiSPrint (
    Header,
    sizeof (Header),
    "\r\n===== PreIsoInstaller Log %04d-%02d-%02d %02d:%02d:%02d =====\r\n",
    Time.Year,
    Time.Month,
    Time.Day,
    Time.Hour,
    Time.Minute,
    Time.Second
    );

  {
    UINTN  Len;
    Len    = AsciiStrLen (Header);
    Status = FileHandleWrite (mLogFileHandle, &Len, Header);
  }
}

VOID
EFIAPI
PreIsoLogPrint (
  IN CONST CHAR16  *Fmt,
  ...
  )
{
  VA_LIST  Args;
  CHAR16   Buffer[LOG_LINE_MAX];
  CHAR8    AsciiBuffer[LOG_LINE_MAX];
  UINTN    Len;

  VA_START (Args, Fmt);
  UnicodeVSPrint (Buffer, sizeof (Buffer), Fmt, Args);
  VA_END (Args);

  ErrorPrint (L"%s", Buffer);

  if (mLogFileHandle != NULL) {
    UnicodeStrToAsciiStrS (Buffer, AsciiBuffer, sizeof (AsciiBuffer));
    Len = AsciiStrLen (AsciiBuffer);
    FileHandleWrite (mLogFileHandle, &Len, AsciiBuffer);
  }
}

STATIC
VOID
EFIAPI
PreIsoLogWrite (
  IN CONST CHAR16  *Fmt,
  ...
  )
{
  VA_LIST  Args;
  CHAR16   Buffer[LOG_LINE_MAX];
  CHAR8    AsciiBuffer[LOG_LINE_MAX];
  UINTN    Len;

  if (mLogFileHandle == NULL) {
    return;
  }

  VA_START (Args, Fmt);
  UnicodeVSPrint (Buffer, sizeof (Buffer), Fmt, Args);
  VA_END (Args);

  UnicodeStrToAsciiStrS (Buffer, AsciiBuffer, sizeof (AsciiBuffer));
  Len = AsciiStrLen (AsciiBuffer);
  FileHandleWrite (mLogFileHandle, &Len, AsciiBuffer);
}

VOID
EFIAPI
PreIsoLogClose (
  VOID
  )
{
  if (mLogFileHandle != NULL) {
    FileHandleFlush (mLogFileHandle);
    FileHandleClose (mLogFileHandle);
    mLogFileHandle = NULL;
  }
}

/**
  Read the entire content of a file into a newly allocated buffer.

  @param[in]  DeviceHandle   Device handle where the file is located.
  @param[in]  FilePath       Path to the file to read.
  @param[out] Buffer         On success, pointer to allocated buffer with file content.
                             Caller must free with FreePool().
  @param[out] BufferSize     On success, number of bytes read.

  @retval EFI_SUCCESS        File read successfully.
  @retval EFI_NOT_FOUND      File not found or empty.
  @retval Other              Error occurred during file operations.

**/
STATIC
EFI_STATUS
EFIAPI
ReadFileContent (
  IN  EFI_HANDLE    DeviceHandle,
  IN  CONST CHAR16  *FilePath,
  OUT VOID          **Buffer,
  OUT UINTN         *BufferSize
  )
{
  EFI_STATUS       Status;
  EFI_FILE_HANDLE  FileHandle = NULL;
  EFI_DEVICE_PATH  *DevicePath;
  EFI_DEVICE_PATH  *DevicePathToFree;
  UINT64           FileSize;
  UINTN            ReadSize;

  *Buffer     = NULL;
  *BufferSize = 0;

  DevicePath = FileDevicePath (DeviceHandle, FilePath);
  if (DevicePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  DevicePathToFree = DevicePath;
  Status           = EfiOpenFileByDevicePath (&DevicePath, &FileHandle, EFI_FILE_MODE_READ, 0);
  FreePool (DevicePathToFree);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = FileHandleGetSize (FileHandle, &FileSize);
  if (EFI_ERROR (Status) || (FileSize == 0)) {
    FileHandleClose (FileHandle);
    return EFI_NOT_FOUND;
  }

  if (FileSize > MAX_READ_FILE_SIZE) {
    PreIsoLogPrint (L"%a: File %s too large: 0x%lx (max 0x%x)\r\n", __FUNCTION__, FilePath, FileSize, MAX_READ_FILE_SIZE);
    FileHandleClose (FileHandle);
    return EFI_VOLUME_FULL;
  }

  *Buffer = AllocatePool ((UINTN)FileSize);
  if (*Buffer == NULL) {
    FileHandleClose (FileHandle);
    return EFI_OUT_OF_RESOURCES;
  }

  ReadSize = (UINTN)FileSize;
  Status   = FileHandleRead (FileHandle, &ReadSize, *Buffer);
  FileHandleClose (FileHandle);

  if (EFI_ERROR (Status)) {
    FreePool (*Buffer);
    *Buffer = NULL;
    return Status;
  }

  if (ReadSize != (UINTN)FileSize) {
    FreePool (*Buffer);
    *Buffer = NULL;
    return EFI_DEVICE_ERROR;
  }

  *BufferSize = ReadSize;
  return EFI_SUCCESS;
}

/**
  Compute SHA256 hash of a data buffer and return it as a lowercase hex string.

  @param[in]  Data           Pointer to data buffer to hash.
  @param[in]  DataSize       Size of data buffer in bytes.
  @param[out] HexString      Buffer to receive the 64-character hex string (+ NUL).
  @param[in]  HexStringSize  Size of HexString buffer (must be >= 65).

  @retval EFI_SUCCESS          Hash computed and converted successfully.
  @retval EFI_BUFFER_TOO_SMALL HexStringSize is less than SHA256_HEX_STRING_SIZE + 1.
  @retval EFI_NOT_FOUND        EFI_HASH2_SERVICE_BINDING_PROTOCOL not available.
  @retval Other                Error from CreateChild, HandleProtocol, or Hash operation.

**/
STATIC
EFI_STATUS
EFIAPI
ComputeSha256Hex (
  IN  VOID   *Data,
  IN  UINTN  DataSize,
  OUT CHAR8  *HexString,
  IN  UINTN  HexStringSize
  )
{
  EFI_STATUS                    Status;
  EFI_STATUS                    DestroyStatus;
  EFI_SERVICE_BINDING_PROTOCOL  *Hash2Sb;
  EFI_HANDLE                    ChildHandle;
  EFI_HASH2_PROTOCOL            *Hash2;
  EFI_HASH2_OUTPUT              HashOutput;
  UINTN                         Index;
  STATIC CONST CHAR8            HexChars[] = "0123456789abcdef";

  if (HexStringSize < SHA256_HEX_STRING_SIZE + 1) {
    return EFI_BUFFER_TOO_SMALL;
  }

  // 1. Locate the service binding (what Hash2DxeCrypto actually installs)
  Status = gBS->LocateProtocol (
                  &gEfiHash2ServiceBindingProtocolGuid,
                  NULL,
                  (VOID **)&Hash2Sb
                  );
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: EFI_HASH2_SERVICE_BINDING_PROTOCOL not available: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  // 2. Create a child handle — this installs gEfiHash2ProtocolGuid on it
  ChildHandle = NULL;
  Status      = Hash2Sb->CreateChild (Hash2Sb, &ChildHandle);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Hash2 CreateChild failed: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  // 3. Retrieve Hash2 from the child handle
  Status = gBS->HandleProtocol (ChildHandle, &gEfiHash2ProtocolGuid, (VOID **)&Hash2);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: HandleProtocol(Hash2) failed: %r\r\n", __FUNCTION__, Status);
    DestroyStatus = Hash2Sb->DestroyChild (Hash2Sb, ChildHandle);
    if (EFI_ERROR (DestroyStatus)) {
      PreIsoLogPrint (L"%a: DestroyChild failed: %r\r\n", __FUNCTION__, DestroyStatus);
    }

    return Status;
  }

  Status = Hash2->Hash (
                    Hash2,
                    &gEfiHashAlgorithmSha256Guid,
                    (CONST UINT8 *)Data,
                    DataSize,
                    &HashOutput
                    );

  // 4. Destroy the child regardless of hash outcome
  DestroyStatus = Hash2Sb->DestroyChild (Hash2Sb, ChildHandle);
  if (EFI_ERROR (DestroyStatus)) {
    PreIsoLogPrint (L"%a: DestroyChild failed: %r\r\n", __FUNCTION__, DestroyStatus);
  }

  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: SHA256 hash failed: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  for (Index = 0; Index < SHA256_DIGEST_SIZE; Index++) {
    HexString[Index * 2]     = HexChars[(HashOutput.Sha256Hash[Index] >> 4) & 0x0F];
    HexString[Index * 2 + 1] = HexChars[HashOutput.Sha256Hash[Index] & 0x0F];
  }

  HexString[SHA256_HEX_STRING_SIZE] = '\0';

  return EFI_SUCCESS;
}

/**
  Check if the boot media contains an ISO9660 filesystem by reading the
  Primary Volume Descriptor (PVD) at sector 16 and verifying the "CD001"
  magic signature.

  Finds the parent whole-disk of DeviceHandle, then checks partition 1
  for a PVD.  If partition 1 is not exposed by firmware, falls back to
  reading the PVD directly from the parent disk at byte offset 32768.

  @param[in]  DeviceHandle   Device handle for a partition on the boot media.

  @retval TRUE   Boot media has an ISO9660 filesystem.
  @retval FALSE  Not ISO9660, or detection failed.

**/
BOOLEAN
EFIAPI
IsIso9660BootMedia (
  IN EFI_HANDLE  DeviceHandle
  )
{
  EFI_STATUS                Status;
  EFI_BLOCK_IO_PROTOCOL     *BlockIo;
  VOID                      *Buffer;
  UINT8                     *Bytes;
  EFI_LBA                   Lba;
  BOOLEAN                   IsIso;
  UINTN                     NumberOfHandles;
  EFI_HANDLE                *BlockIoHandles;
  UINTN                     DiskIndex;
  UINTN                     ChildCount;
  EFI_HANDLE                *ChildHandles;
  UINTN                     ChildIndex;
  BOOLEAN                   FoundParent;
  EFI_HANDLE                ParentDiskHandle;
  EFI_HANDLE                FirstPartHandle;
  EFI_DEVICE_PATH_PROTOCOL  *ChildPath;
  HARDDRIVE_DEVICE_PATH     *HdNode;

  if (DeviceHandle == NULL) {
    return FALSE;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiBlockIoProtocolGuid,
                  NULL,
                  &NumberOfHandles,
                  &BlockIoHandles
                  );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  FirstPartHandle  = NULL;
  ParentDiskHandle = NULL;
  FoundParent      = FALSE;

  for (DiskIndex = 0; DiskIndex < NumberOfHandles; DiskIndex++) {
    Status = gBS->HandleProtocol (
                    BlockIoHandles[DiskIndex],
                    &gEfiBlockIoProtocolGuid,
                    (VOID **)&BlockIo
                    );
    if (EFI_ERROR (Status) || BlockIo->Media->LogicalPartition) {
      continue;
    }

    ChildHandles = NULL;
    Status       = ParseHandleDatabaseForChildControllers (
                     BlockIoHandles[DiskIndex],
                     &ChildCount,
                     &ChildHandles
                     );
    if (EFI_ERROR (Status) || (ChildCount == 0)) {
      if (ChildHandles != NULL) {
        FreePool (ChildHandles);
      }

      continue;
    }

    FoundParent = FALSE;
    for (ChildIndex = 0; ChildIndex < ChildCount; ChildIndex++) {
      if (ChildHandles[ChildIndex] == DeviceHandle) {
        FoundParent = TRUE;
        break;
      }
    }

    if (!FoundParent) {
      FreePool (ChildHandles);
      continue;
    }

    for (ChildIndex = 0; ChildIndex < ChildCount; ChildIndex++) {
      ChildPath = DevicePathFromHandle (ChildHandles[ChildIndex]);
      if (ChildPath == NULL) {
        continue;
      }

      while (!IsDevicePathEnd (ChildPath)) {
        if ((DevicePathType (ChildPath) == MEDIA_DEVICE_PATH) &&
            (DevicePathSubType (ChildPath) == MEDIA_HARDDRIVE_DP))
        {
          HdNode = (HARDDRIVE_DEVICE_PATH *)ChildPath;
          if (HdNode->PartitionNumber == 1) {
            FirstPartHandle = ChildHandles[ChildIndex];
          }

          break;
        }

        ChildPath = NextDevicePathNode (ChildPath);
      }

      if (FirstPartHandle != NULL) {
        break;
      }
    }

    FreePool (ChildHandles);
    ParentDiskHandle = BlockIoHandles[DiskIndex];
    break;
  }

  FreePool (BlockIoHandles);

  if (!FoundParent) {
    return FALSE;
  }

  // Use partition 1 if found; otherwise fall back to the parent disk.
  if (FirstPartHandle != NULL) {
    Status = gBS->HandleProtocol (FirstPartHandle, &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
  } else {
    Status = gBS->HandleProtocol (ParentDiskHandle, &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
  }

  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  if ((BlockIo->Media->BlockSize == 0) ||
      ((ISO9660_PVD_OFFSET % BlockIo->Media->BlockSize) != 0))
  {
    return FALSE;
  }

  Lba = ISO9660_PVD_OFFSET / BlockIo->Media->BlockSize;

  Buffer = AllocatePool (BlockIo->Media->BlockSize);
  if (Buffer == NULL) {
    return FALSE;
  }

  Status = BlockIo->ReadBlocks (
                      BlockIo,
                      BlockIo->Media->MediaId,
                      Lba,
                      BlockIo->Media->BlockSize,
                      Buffer
                      );
  if (EFI_ERROR (Status)) {
    FreePool (Buffer);
    return FALSE;
  }

  Bytes = (UINT8 *)Buffer;
  IsIso = ((Bytes[0] == 0x01) &&
           (CompareMem (&Bytes[1], ISO9660_VOLDESC_MAGIC, ISO9660_MAGIC_LEN) == 0));

  FreePool (Buffer);

  if (IsIso) {
    PreIsoLogWrite (L"%a: ISO9660 boot media detected\r\n", __FUNCTION__);
  }

  return IsIso;
}

/**
  Check if EFI\ISO_ID file exists and its content matches the SHA256
  hash of the EFI\version file content concatenated with "L4T". This
  provides an alternative detection mechanism for ISO-based installation
  media.

  The ISO_ID file is expected to contain the sha256sum output (64 lowercase
  hex characters) of (version file content + "L4T").

  @param[in]  DeviceHandle   Device handle for ESP partition.

  @retval TRUE   ISO_ID file matches the version file SHA256.
  @retval FALSE  Files missing, hash protocol unavailable, or hash mismatch.

**/
BOOLEAN
EFIAPI
IsIsoIdFileValid (
  IN EFI_HANDLE  DeviceHandle
  )
{
  EFI_STATUS          Status;
  VOID                *VersionContent;
  UINTN               VersionSize;
  VOID                *IsoIdContent;
  UINTN               IsoIdSize;
  CHAR8               ComputedHash[SHA256_HEX_STRING_SIZE + 1];
  CHAR8               IsoIdHash[SHA256_HEX_STRING_SIZE + 1];
  CHAR8               *IsoIdStr;
  UINTN               Index;
  STATIC CONST CHAR8  HashSuffix[] = "L4T";
  UINTN               HashDataSize;
  UINT8               *HashData;

  Status = ReadFileContent (DeviceHandle, ISO_ID_FILE_PATH, &IsoIdContent, &IsoIdSize);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  Status = ReadFileContent (DeviceHandle, VERSION_FILE_PATH, &VersionContent, &VersionSize);
  if (EFI_ERROR (Status)) {
    FreePool (IsoIdContent);
    return FALSE;
  }

  {
    UINT8  *VerBytes;

    VerBytes = (UINT8 *)VersionContent;
    while ((VersionSize > 0) &&
           ((VerBytes[VersionSize - 1] == '\r') ||
            (VerBytes[VersionSize - 1] == '\n') ||
            (VerBytes[VersionSize - 1] == ' ')))
    {
      VersionSize--;
    }
  }

  HashDataSize = VersionSize + sizeof (HashSuffix) - 1;
  HashData     = AllocatePool (HashDataSize);
  if (HashData == NULL) {
    FreePool (VersionContent);
    FreePool (IsoIdContent);
    return FALSE;
  }

  CopyMem (HashData, VersionContent, VersionSize);
  CopyMem (HashData + VersionSize, HashSuffix, sizeof (HashSuffix) - 1);
  FreePool (VersionContent);

  Status = ComputeSha256Hex (HashData, HashDataSize, ComputedHash, sizeof (ComputedHash));
  FreePool (HashData);
  if (EFI_ERROR (Status)) {
    FreePool (IsoIdContent);
    return FALSE;
  }

  IsoIdStr = (CHAR8 *)IsoIdContent;
  ZeroMem (IsoIdHash, sizeof (IsoIdHash));
  for (Index = 0; (Index < IsoIdSize) && (Index < SHA256_HEX_STRING_SIZE); Index++) {
    if ((IsoIdStr[Index] == '\r') || (IsoIdStr[Index] == '\n') || (IsoIdStr[Index] == ' ')) {
      break;
    }

    if ((IsoIdStr[Index] >= 'A') && (IsoIdStr[Index] <= 'F')) {
      IsoIdHash[Index] = IsoIdStr[Index] - 'A' + 'a';
    } else {
      IsoIdHash[Index] = IsoIdStr[Index];
    }
  }

  FreePool (IsoIdContent);

  if (Index != SHA256_HEX_STRING_SIZE) {
    PreIsoLogPrint (L"%a: ISO_ID not a valid SHA256 hex string (length=%d)\r\n", __FUNCTION__, Index);
    return FALSE;
  }

  PreIsoLogWrite (L"%a: Computed SHA256: %a\r\n", __FUNCTION__, ComputedHash);
  PreIsoLogWrite (L"%a: ISO_ID SHA256:   %a\r\n", __FUNCTION__, IsoIdHash);

  if (CompareMem (ComputedHash, IsoIdHash, SHA256_HEX_STRING_SIZE) == 0) {
    return TRUE;
  }

  PreIsoLogPrint (L"%a: ISO_ID does not match version file SHA256\r\n", __FUNCTION__);
  return FALSE;
}

/**
  Set or clear the capsule status in OsIndications variable.

  @param[in]  SetCap  TRUE to set capsule delivery supported flag, FALSE to clear it.

  @retval EFI_SUCCESS    The operation completed successfully.
  @retval Other          Error occurred setting the variable.

**/
STATIC
EFI_STATUS
SetCapsuleStatusVariable (
  BOOLEAN  SetCap
  )
{
  EFI_STATUS  Status;
  UINT64      OsIndication;
  UINTN       DataSize;

  OsIndication = 0;
  DataSize     = sizeof (UINT64);
  Status       = gRT->GetVariable (
                        L"OsIndications",
                        &gEfiGlobalVariableGuid,
                        NULL,
                        &DataSize,
                        &OsIndication
                        );
  if (Status == EFI_NOT_FOUND) {
    OsIndication = 0;
  } else if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to read OsIndications: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  if (SetCap) {
    OsIndication |= ((UINT64)EFI_OS_INDICATIONS_FILE_CAPSULE_DELIVERY_SUPPORTED);
  } else {
    OsIndication &= ~((UINT64)EFI_OS_INDICATIONS_FILE_CAPSULE_DELIVERY_SUPPORTED);
  }

  Status = gRT->SetVariable (
                  L"OsIndications",
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  sizeof (UINT64),
                  &OsIndication
                  );

  return Status;
}

/**
  Create directory path, creating intermediate components as needed.

  Walks each path component separated by '\\' and creates it if it does
  not already exist.  Opening an existing directory with CREATE succeeds
  per the UEFI specification, so this is safe to call when the path
  already exists.

  @param[in]  DeviceHandle   Device handle where directory should be created.
  @param[in]  DirPath        Directory path to create (e.g. L"EFI\\UpdateCapsule\\").

  @retval EFI_SUCCESS    All components created / already exist.
  @retval Other          Error occurred during directory creation.

**/
STATIC
EFI_STATUS
EFIAPI
CreateDirectory (
  IN EFI_HANDLE    DeviceHandle,
  IN CONST CHAR16  *DirPath
  )
{
  EFI_STATUS       Status;
  EFI_FILE_HANDLE  RootHandle;
  EFI_FILE_HANDLE  CurrentHandle;
  EFI_FILE_HANDLE  NextHandle;
  EFI_DEVICE_PATH  *DevicePath;
  EFI_DEVICE_PATH  *DevicePathToFree;
  CHAR16           Component[MAX_CAPSULE_PATH_CHARS];
  UINTN            SrcIdx;
  UINTN            CompIdx;

  DevicePath = FileDevicePath (DeviceHandle, L"\\");
  if (DevicePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  DevicePathToFree = DevicePath;
  Status           = EfiOpenFileByDevicePath (&DevicePath, &RootHandle, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
  FreePool (DevicePathToFree);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CurrentHandle = RootHandle;
  SrcIdx        = 0;

  if (DirPath[SrcIdx] == L'\\') {
    SrcIdx++;
  }

  while (DirPath[SrcIdx] != L'\0') {
    CompIdx = 0;
    while ((DirPath[SrcIdx] != L'\\') && (DirPath[SrcIdx] != L'\0') &&
           (CompIdx < MAX_CAPSULE_PATH_CHARS - 1))
    {
      Component[CompIdx++] = DirPath[SrcIdx++];
    }

    while ((DirPath[SrcIdx] != L'\\') && (DirPath[SrcIdx] != L'\0')) {
      SrcIdx++;
    }

    Component[CompIdx] = L'\0';

    if (DirPath[SrcIdx] == L'\\') {
      SrcIdx++;
    }

    if (CompIdx == 0) {
      continue;
    }

    Status = CurrentHandle->Open (
                              CurrentHandle,
                              &NextHandle,
                              Component,
                              EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                              EFI_FILE_DIRECTORY
                              );
    if (EFI_ERROR (Status)) {
      PreIsoLogPrint (
        L"%a: Failed to create directory component '%s': %r\r\n",
        __FUNCTION__,
        Component,
        Status
        );
      break;
    }

    if (CurrentHandle != RootHandle) {
      FileHandleClose (CurrentHandle);
    }

    CurrentHandle = NextHandle;
  }

  if ((CurrentHandle != NULL) && (CurrentHandle != RootHandle)) {
    FileHandleClose (CurrentHandle);
  }

  FileHandleClose (RootHandle);

  return Status;
}

/**
  Copy a file from source path to destination path.

  @param[in]  DeviceHandle   Device handle where files are located.
  @param[in]  SourcePath     Source file path.
  @param[in]  DestPath       Destination file path.

  @retval EFI_SUCCESS    The operation completed successfully.
  @retval Other          Error occurred during file operations.

**/
STATIC
EFI_STATUS
EFIAPI
CopyFile (
  IN EFI_HANDLE    DeviceHandle,
  IN CONST CHAR16  *SourcePath,
  IN CONST CHAR16  *DestPath
  )
{
  EFI_STATUS       Status;
  EFI_FILE_HANDLE  SourceHandle = NULL;
  EFI_FILE_HANDLE  DestHandle   = NULL;
  EFI_DEVICE_PATH  *SourceDevicePath;
  EFI_DEVICE_PATH  *SourceDevicePathToFree;
  EFI_DEVICE_PATH  *DestDevicePath;
  EFI_DEVICE_PATH  *DestDevicePathToFree;
  UINT64           FileSize;
  UINTN            ReadSize;
  UINTN            WriteSize;
  UINTN            Remaining;
  VOID             *WritePtr;
  VOID             *Buffer = NULL;

  SourceDevicePath = FileDevicePath (DeviceHandle, SourcePath);
  if (SourceDevicePath == NULL) {
    PreIsoLogPrint (L"%a: Failed to create source file device path\r\n", __FUNCTION__);
    return EFI_OUT_OF_RESOURCES;
  }

  SourceDevicePathToFree = SourceDevicePath;
  Status                 = EfiOpenFileByDevicePath (&SourceDevicePath, &SourceHandle, EFI_FILE_MODE_READ, 0);
  FreePool (SourceDevicePathToFree);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to open source file: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  Status = FileHandleGetSize (SourceHandle, &FileSize);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to get file size: %r\r\n", __FUNCTION__, Status);
    goto Cleanup;
  }

  PreIsoLogWrite (L"%a: File size: 0x%lx\r\n", __FUNCTION__, FileSize);

  Status = CreateDirectory (DeviceHandle, CAPSULE_DEST_DIR);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to create destination directory: %r\r\n", __FUNCTION__, Status);
    goto Cleanup;
  }

  DestDevicePath = FileDevicePath (DeviceHandle, DestPath);
  if (DestDevicePath == NULL) {
    PreIsoLogPrint (L"%a: Failed to create destination file device path\r\n", __FUNCTION__);
    Status = EFI_OUT_OF_RESOURCES;
    goto Cleanup;
  }

  DestDevicePathToFree = DestDevicePath;
  Status               = EfiOpenFileByDevicePath (&DestDevicePath, &DestHandle, EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE, 0);
  FreePool (DestDevicePathToFree);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to open destination file: %r\r\n", __FUNCTION__, Status);
    goto Cleanup;
  }

  Status = FileHandleSetSize (DestHandle, 0);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to truncate destination file: %r\r\n", __FUNCTION__, Status);
    goto Cleanup;
  }

  Buffer = AllocatePool (FILE_COPY_BUFFER_SIZE);
  if (Buffer == NULL) {
    PreIsoLogPrint (L"%a: Failed to allocate copy buffer\r\n", __FUNCTION__);
    Status = EFI_OUT_OF_RESOURCES;
    goto Cleanup;
  }

  while (TRUE) {
    ReadSize = FILE_COPY_BUFFER_SIZE;
    Status   = FileHandleRead (SourceHandle, &ReadSize, Buffer);
    if (EFI_ERROR (Status)) {
      PreIsoLogPrint (L"%a: Failed to read from source file: %r\r\n", __FUNCTION__, Status);
      goto Cleanup;
    }

    if (ReadSize == 0) {
      break;
    }

    DEBUG ((DEBUG_VERBOSE, "%a: Copying chunk, size: 0x%lx\r\n", __FUNCTION__, ReadSize));

    Remaining = ReadSize;
    WritePtr  = Buffer;
    while (Remaining > 0) {
      WriteSize = Remaining;
      Status    = FileHandleWrite (DestHandle, &WriteSize, WritePtr);
      if (EFI_ERROR (Status)) {
        PreIsoLogPrint (L"%a: Failed to write to destination file: %r\r\n", __FUNCTION__, Status);
        goto Cleanup;
      }

      if (WriteSize == 0) {
        PreIsoLogPrint (L"%a: Write returned zero bytes, aborting\r\n", __FUNCTION__);
        Status = EFI_DEVICE_ERROR;
        goto Cleanup;
      }

      Remaining -= WriteSize;
      WritePtr   = (UINT8 *)WritePtr + WriteSize;
    }
  }

  PreIsoLogWrite (L"%a: File copied successfully\r\n", __FUNCTION__);
  Status = EFI_SUCCESS;

Cleanup:
  if (Buffer != NULL) {
    FreePool (Buffer);
  }

  if (SourceHandle != NULL) {
    FileHandleClose (SourceHandle);
  }

  if (DestHandle != NULL) {
    FileHandleClose (DestHandle);
  }

  return Status;
}

/**
  Get system firmware version from ESRT (EFI System Resource Table).

  @param[out] FwVersion   Pointer to store the firmware version.

  @retval EFI_SUCCESS    The operation completed successfully.
  @retval EFI_NOT_FOUND  ESRT not found or no system firmware entry.
  @retval Other          Error occurred reading ESRT.

**/
STATIC
EFI_STATUS
EFIAPI
GetSystemFwVersionFromEsrt (
  OUT UINT32  *FwVersion
  )
{
  EFI_STATUS                 Status;
  EFI_SYSTEM_RESOURCE_TABLE  *Esrt;
  EFI_SYSTEM_RESOURCE_ENTRY  *EsrtEntry;
  UINTN                      Index;

  if (FwVersion == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *FwVersion = 0;

  Status = EfiGetSystemConfigurationTable (&gEfiSystemResourceTableGuid, (VOID **)&Esrt);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to get ESRT: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  if (Esrt == NULL) {
    PreIsoLogPrint (L"%a: ESRT is NULL\r\n", __FUNCTION__);
    return EFI_NOT_FOUND;
  }

  PreIsoLogPrint (L"%a: ESRT FwResourceCount: %d\r\n", __FUNCTION__, Esrt->FwResourceCount);

  if (Esrt->FwResourceCount > 128) {
    PreIsoLogPrint (L"%a: ESRT FwResourceCount too large: %d\r\n", __FUNCTION__, Esrt->FwResourceCount);
    return EFI_NOT_FOUND;
  }

  EsrtEntry = (VOID *)(Esrt + 1);
  for (Index = 0; Index < Esrt->FwResourceCount; Index++) {
    DEBUG ((
      DEBUG_VERBOSE,
      "%a: ESRT[%d] FwClass: %g, FwVersion: 0x%x, FwType: 0x%x\r\n",
      __FUNCTION__,
      Index,
      &EsrtEntry->FwClass,
      EsrtEntry->FwVersion,
      EsrtEntry->FwType
      ));

    if (EsrtEntry->FwType == ESRT_FW_TYPE_SYSTEMFIRMWARE) {
      *FwVersion = EsrtEntry->FwVersion;
      PreIsoLogPrint (L"%a: System FwVersion found: 0x%x\r\n", __FUNCTION__, *FwVersion);
      return EFI_SUCCESS;
    }

    EsrtEntry++;
  }

  PreIsoLogPrint (L"%a: System firmware entry not found in ESRT\r\n", __FUNCTION__);
  return EFI_NOT_FOUND;
}

/**
  Get board specification information from EEPROM.

  @param[out] BoardInfo   Pointer to store pointer to board information.

  @retval EFI_SUCCESS    The operation completed successfully.
  @retval Other          Error occurred locating EEPROM protocol.

**/
STATIC
EFI_STATUS
EFIAPI
GetBoardInfo (
  OUT TEGRA_EEPROM_BOARD_INFO  **BoardInfo
  )
{
  EFI_STATUS               Status;
  TEGRA_EEPROM_BOARD_INFO  *CvmBoardInfo;
  EEPROM_PART_NUMBER       *EepromPartNumber;

  if (BoardInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->LocateProtocol (&gNVIDIACvmEepromProtocolGuid, NULL, (VOID **)&CvmBoardInfo);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to locate CvmEepromProtocol: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  *BoardInfo = CvmBoardInfo;

  EepromPartNumber = (EEPROM_PART_NUMBER *)&CvmBoardInfo->ProductId[0];
  PreIsoLogWrite (
    L"%a: Board ID=%c%c%c%c, SKU=%c%c%c%c, FAB=%c%c%c\r\n",
    __FUNCTION__,
    EepromPartNumber->TegraEepromPartNumber.Id[0],
    EepromPartNumber->TegraEepromPartNumber.Id[1],
    EepromPartNumber->TegraEepromPartNumber.Id[2],
    EepromPartNumber->TegraEepromPartNumber.Id[3],
    EepromPartNumber->TegraEepromPartNumber.Sku[0],
    EepromPartNumber->TegraEepromPartNumber.Sku[1],
    EepromPartNumber->TegraEepromPartNumber.Sku[2],
    EepromPartNumber->TegraEepromPartNumber.Sku[3],
    EepromPartNumber->TegraEepromPartNumber.Fab[0],
    EepromPartNumber->TegraEepromPartNumber.Fab[1],
    EepromPartNumber->TegraEepromPartNumber.Fab[2]
    );

  return EFI_SUCCESS;
}

/**
  Get current PreIsoInstaller version from version file.

  @param[in]  DeviceHandle  Device handle for ESP.
  @param[out] Version       Pointer to store the version value.

  @retval EFI_SUCCESS       The operation completed successfully.
  @retval EFI_NOT_FOUND     Version file not found.
  @retval EFI_UNSUPPORTED   Version data is invalid.

**/
STATIC
EFI_STATUS
EFIAPI
GetPreIsoInstallerVersion (
  IN  EFI_HANDLE  DeviceHandle,
  OUT UINT32      *Version
  )
{
  EFI_STATUS       Status;
  EFI_FILE_HANDLE  FileHandle = NULL;
  EFI_DEVICE_PATH  *FilePath;
  EFI_DEVICE_PATH  *FilePathToFree;
  UINT64           FileSize;
  CHAR8            VersionBuffer[32];
  CHAR16           VersionStrW[32];
  UINTN            ReadSize;
  UINT64           Version64;
  UINTN            Index;

  if (Version == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  FilePath = FileDevicePath (DeviceHandle, VERSION_FILE_PATH);
  if (FilePath == NULL) {
    PreIsoLogPrint (L"%a: Failed to create device path for version file\r\n", __FUNCTION__);
    return EFI_OUT_OF_RESOURCES;
  }

  FilePathToFree = FilePath;
  Status         = EfiOpenFileByDevicePath (&FilePath, &FileHandle, EFI_FILE_MODE_READ, 0);
  FreePool (FilePathToFree);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Version file not found: %r\r\n", __FUNCTION__, Status);
    return EFI_NOT_FOUND;
  }

  Status = FileHandleGetSize (FileHandle, &FileSize);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to get version file size: %r\r\n", __FUNCTION__, Status);
    FileHandleClose (FileHandle);
    return Status;
  }

  if ((FileSize == 0) || (FileSize >= sizeof (VersionBuffer))) {
    PreIsoLogPrint (L"%a: Version file size invalid: %lu\r\n", __FUNCTION__, FileSize);
    FileHandleClose (FileHandle);
    return EFI_UNSUPPORTED;
  }

  ReadSize = (UINTN)FileSize;
  ZeroMem (VersionBuffer, sizeof (VersionBuffer));
  Status = FileHandleRead (FileHandle, &ReadSize, VersionBuffer);
  FileHandleClose (FileHandle);

  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to read version file: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  ZeroMem (VersionStrW, sizeof (VersionStrW));
  for (Index = 0; (Index < ReadSize) && (Index < (sizeof (VersionStrW) / sizeof (CHAR16) - 1)); Index++) {
    if ((VersionBuffer[Index] == '\r') || (VersionBuffer[Index] == '\n') || (VersionBuffer[Index] == ' ')) {
      break;
    }

    VersionStrW[Index] = (CHAR16)VersionBuffer[Index];
  }

  Status = StrHexToUint64S (VersionStrW, NULL, &Version64);
  if (EFI_ERROR (Status) || (Version64 > MAX_UINT32)) {
    PreIsoLogPrint (L"%a: Version file content invalid: %s\r\n", __FUNCTION__, VersionStrW);
    return EFI_UNSUPPORTED;
  }

  *Version = (UINT32)Version64;

  return EFI_SUCCESS;
}

/**
  Get firmware version information from SystemFwVersions UEFI variable.

  @param[out] VersionSlotA   Pointer to store version for Slot A.
  @param[out] VersionSlotB   Pointer to store version for Slot B.

  @retval EFI_SUCCESS    The operation completed successfully.
  @retval Other          Error occurred reading both variable and ESRT.

**/
STATIC
EFI_STATUS
EFIAPI
GetSystemFwVersions (
  OUT UINT32  *VersionSlotA,
  OUT UINT32  *VersionSlotB
  )
{
  EFI_STATUS  Status;
  UINT32      VersionArray[2];
  UINTN       DataSize;
  UINT32      EsrtVersion;

  if ((VersionSlotA == NULL) || (VersionSlotB == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (VersionArray, sizeof (VersionArray));
  DataSize = sizeof (VersionArray);
  Status   = gRT->GetVariable (
                    L"SystemFwVersions",
                    &gNVIDIAPublicVariableGuid,
                    NULL,
                    &DataSize,
                    VersionArray
                    );
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Unable to read SystemFwVersions variable: %r, fallback to ESRT\r\n", __FUNCTION__, Status);

    Status = GetSystemFwVersionFromEsrt (&EsrtVersion);
    if (EFI_ERROR (Status)) {
      PreIsoLogPrint (L"%a: Unable to get version from ESRT: %r\r\n", __FUNCTION__, Status);
      return Status;
    }

    *VersionSlotA = EsrtVersion;
    *VersionSlotB = EsrtVersion;

    PreIsoLogPrint (L"%a: Using ESRT version for both slots: 0x%x\r\n", __FUNCTION__, EsrtVersion);

    return EFI_SUCCESS;
  }

  *VersionSlotA = VersionArray[0];
  *VersionSlotB = VersionArray[1];

  return EFI_SUCCESS;
}

/**
  Check whether the EEPROM part number is in Tegra format.

  Tegra-format part numbers start with the "699" prefix.  Customer-format
  EEPROMs use a different layout and the Tegra-specific fields (Id, Sku,
  Fab, Revision) are not valid.

  @param[in]  EepromPartNumber  Pointer to the EEPROM part number union.

  @retval TRUE   The part number is in Tegra format.
  @retval FALSE  The part number is in customer or unknown format.

**/
STATIC
BOOLEAN
EFIAPI
IsTegraBoardFormat (
  IN CONST EEPROM_PART_NUMBER  *EepromPartNumber
  )
{
  return CompareMem (
           EepromPartNumber->TegraEepromPartNumber.Leading,
           NVIDIA_EEPROM_BOARD_ID_PREFIX,
           3
           ) == 0;
}

/**
  Check whether the compatible spec identifies NanoE8GB.

  @param[in]  CompatSpec  TegraPlatformCompatSpec value.

  @retval TRUE   Board name identifies NanoE8GB.
  @retval FALSE  Board name does not identify NanoE8GB.

**/
STATIC
BOOLEAN
IsNanoe8gb (
  IN CONST CHAR8  *CompatSpec
  )
{
  return (CompatSpec != NULL) &&
         (AsciiStrStr (CompatSpec, BOARD_NAME_ORIN_NANOE8GB_DEVKIT) != NULL);
}

/**
  Check whether the compatible spec identifies a super board.

  @param[in]  CompatSpec  TegraPlatformCompatSpec value.

  @retval TRUE   Board name contains "super".
  @retval FALSE  Board name does not contain "super".

**/
STATIC
BOOLEAN
IsSuper (
  IN CONST CHAR8  *CompatSpec
  )
{
  return (CompatSpec != NULL) && (AsciiStrStr (CompatSpec, "super") != NULL);
}

/**
  Select AGX Orin capsule payload from board ID, SKU, and FAB.

  @param[in]  BoardSku  Board SKU parsed from EEPROM.
  @param[in]  BoardFab  Board FAB parsed from EEPROM.

  @retval Capsule file name for the detected AGX Orin variant.

**/
STATIC
CONST CHAR16 *
GetAgxOrinCapsuleFileNameFromBoardInfo (
  IN UINT32  BoardSku,
  IN UINT32  BoardFab
  )
{
  if ((BoardSku == 4) || (BoardSku == 5)) {
    return CAPSULE_3701_AGX;
  }

  if (BoardSku == 8) {
    return CAPSULE_3701_AGX_IND;
  }

  if (BoardSku == 0) {
    if (BoardFab == 300) {
      return CAPSULE_3701_AGX;
    }

    return CAPSULE_3701_000;
  }

  PreIsoLogPrint (L"%a: Unknown SKU %d for board 3701, using default\r\n", __FUNCTION__, BoardSku);
  return CAPSULE_DEFAULT_NAME;
}

/**
  Select AGX Orin capsule payload.

  Board ID/SKU/FAB are not sufficient to distinguish jetson-agx-orin-devkit
  from jetson-agx-orin-devkit-super, so the compatible spec board name is used.

  @param[in]  BoardSku  Board SKU parsed from EEPROM.
  @param[in]  BoardFab  Board FAB parsed from EEPROM.

  @retval Capsule file name for the detected AGX Orin variant.

**/
STATIC
CONST CHAR16 *
GetAgxOrinCapsuleFileName (
  IN UINT32  BoardSku,
  IN UINT32  BoardFab
  )
{
  EFI_STATUS    Status;
  CHAR8         CompatSpec[MAX_SPEC_STRING_LEN];
  UINTN         DataSize;
  CONST CHAR16  *DefaultCapsuleFileName;

  DefaultCapsuleFileName = GetAgxOrinCapsuleFileNameFromBoardInfo (BoardSku, BoardFab);

  DataSize = sizeof (CompatSpec) - 1;
  Status   = gRT->GetVariable (
                    TEGRA_PLATFORM_COMPAT_SPEC_VARIABLE_NAME,
                    &gNVIDIAPublicVariableGuid,
                    NULL,
                    &DataSize,
                    CompatSpec
                    );
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (
      L"%a: Unable to read TegraPlatformCompatSpec: %r, using %s\r\n",
      __FUNCTION__,
      Status,
      DefaultCapsuleFileName
      );
    return DefaultCapsuleFileName;
  }

  CompatSpec[DataSize] = '\0';
  PreIsoLogWrite (L"%a: TegraPlatformCompatSpec=%a\r\n", __FUNCTION__, CompatSpec);

  if (IsSuper (CompatSpec)) {
    return CAPSULE_3701_AGX_SUPER;
  }

  return DefaultCapsuleFileName;
}

/**
  Select Orin Nano capsule payload.

  Board ID/SKU/FAB are not sufficient to distinguish jetson-orin-nano-devkit
  from jetson-orin-nanoe8gb-devkit, so the compatible spec board name is used.

  @retval Capsule file name for the detected Orin Nano variant.

**/
STATIC
CONST CHAR16 *
GetOrinNanoCapsuleFileName (
  VOID
  )
{
  EFI_STATUS  Status;
  CHAR8       CompatSpec[MAX_SPEC_STRING_LEN];
  UINTN       DataSize;

  DataSize = sizeof (CompatSpec) - 1;
  Status   = gRT->GetVariable (
                    TEGRA_PLATFORM_COMPAT_SPEC_VARIABLE_NAME,
                    &gNVIDIAPublicVariableGuid,
                    NULL,
                    &DataSize,
                    CompatSpec
                    );
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (
      L"%a: Unable to read TegraPlatformCompatSpec: %r, using default 3767 capsule\r\n",
      __FUNCTION__,
      Status
      );
    return CAPSULE_3767;
  }

  CompatSpec[DataSize] = '\0';
  PreIsoLogWrite (L"%a: TegraPlatformCompatSpec=%a\r\n", __FUNCTION__, CompatSpec);

  if (IsNanoe8gb (CompatSpec)) {
    if (IsSuper (CompatSpec)) {
      return CAPSULE_3767_NANOE8GB_SUPER;
    }

    return CAPSULE_3767_NANOE8GB;
  }

  if (IsSuper (CompatSpec)) {
    return CAPSULE_3767_SUPER;
  }

  return CAPSULE_3767;
}

/**
  Select capsule file name based on board ID, SKU, FAB, and compatible spec.

  @param[in]  BoardInfo       Pointer to board information from EEPROM.
  @param[out] CapsuleSource   Buffer to store source capsule path.
  @param[out] CapsuleDest     Buffer to store destination capsule path.
  @param[in]  BufferSize      Size of the buffers.

  @retval EFI_SUCCESS         Capsule paths set successfully.
  @retval EFI_INVALID_PARAMETER Invalid parameters.

**/
STATIC
EFI_STATUS
EFIAPI
SelectCapsuleFile (
  IN  CONST TEGRA_EEPROM_BOARD_INFO  *BoardInfo,
  OUT CHAR16                         *CapsuleSource,
  OUT CHAR16                         *CapsuleDest,
  IN  UINTN                          BufferSize
  )
{
  CONST EEPROM_PART_NUMBER  *EepromPartNumber;
  CHAR16                    BoardIdStr[5];
  CHAR16                    BoardSkuStr[5];
  CHAR16                    BoardFabStr[4];
  UINTN                     BoardIdN;
  UINTN                     BoardSkuN;
  UINTN                     BoardFabN;
  UINT32                    BoardId;
  UINT32                    BoardSku;
  UINT32                    BoardFab;
  CONST CHAR16              *CapsuleFileName;
  EFI_STATUS                Status;

  if ((BoardInfo == NULL) || (CapsuleSource == NULL) || (CapsuleDest == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize < MAX_CAPSULE_PATH_SIZE) {
    return EFI_BUFFER_TOO_SMALL;
  }

  EepromPartNumber = (EEPROM_PART_NUMBER *)&BoardInfo->ProductId[0];

  if (!IsTegraBoardFormat (EepromPartNumber)) {
    PreIsoLogPrint (
      L"%a: Non-Tegra EEPROM format (ProductId: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x), using default capsule\r\n",
      __FUNCTION__,
      BoardInfo->ProductId[0],
      BoardInfo->ProductId[1],
      BoardInfo->ProductId[2],
      BoardInfo->ProductId[3],
      BoardInfo->ProductId[4],
      BoardInfo->ProductId[5],
      BoardInfo->ProductId[6],
      BoardInfo->ProductId[7],
      BoardInfo->ProductId[8],
      BoardInfo->ProductId[9]
      );
    CapsuleFileName = CAPSULE_DEFAULT_NAME;
    goto Done;
  }

  BoardIdStr[0] = (CHAR16)EepromPartNumber->TegraEepromPartNumber.Id[0];
  BoardIdStr[1] = (CHAR16)EepromPartNumber->TegraEepromPartNumber.Id[1];
  BoardIdStr[2] = (CHAR16)EepromPartNumber->TegraEepromPartNumber.Id[2];
  BoardIdStr[3] = (CHAR16)EepromPartNumber->TegraEepromPartNumber.Id[3];
  BoardIdStr[4] = L'\0';

  BoardSkuStr[0] = (CHAR16)EepromPartNumber->TegraEepromPartNumber.Sku[0];
  BoardSkuStr[1] = (CHAR16)EepromPartNumber->TegraEepromPartNumber.Sku[1];
  BoardSkuStr[2] = (CHAR16)EepromPartNumber->TegraEepromPartNumber.Sku[2];
  BoardSkuStr[3] = (CHAR16)EepromPartNumber->TegraEepromPartNumber.Sku[3];
  BoardSkuStr[4] = L'\0';

  BoardFabStr[0] = (CHAR16)EepromPartNumber->TegraEepromPartNumber.Fab[0];
  BoardFabStr[1] = (CHAR16)EepromPartNumber->TegraEepromPartNumber.Fab[1];
  BoardFabStr[2] = (CHAR16)EepromPartNumber->TegraEepromPartNumber.Fab[2];
  BoardFabStr[3] = L'\0';

  Status = StrDecimalToUintnS (BoardIdStr, NULL, &BoardIdN);
  if (EFI_ERROR (Status) || (BoardIdN > MAX_UINT32)) {
    PreIsoLogPrint (L"%a: Failed to parse board ID: %s, using default capsule\r\n", __FUNCTION__, BoardIdStr);
    CapsuleFileName = CAPSULE_DEFAULT_NAME;
    goto Done;
  }

  BoardId = (UINT32)BoardIdN;

  Status = StrDecimalToUintnS (BoardSkuStr, NULL, &BoardSkuN);
  if (EFI_ERROR (Status) || (BoardSkuN > MAX_UINT32)) {
    PreIsoLogPrint (L"%a: Failed to parse board SKU: %s, using default capsule\r\n", __FUNCTION__, BoardSkuStr);
    CapsuleFileName = CAPSULE_DEFAULT_NAME;
    goto Done;
  }

  BoardSku = (UINT32)BoardSkuN;

  Status = StrDecimalToUintnS (BoardFabStr, NULL, &BoardFabN);
  if (EFI_ERROR (Status) || (BoardFabN > MAX_UINT32)) {
    BoardFab = 0;
  } else {
    BoardFab = (UINT32)BoardFabN;
  }

  PreIsoLogWrite (L"%a: Board ID=%d SKU=%d FAB=%d\r\n", __FUNCTION__, BoardId, BoardSku, BoardFab);

  switch (BoardId) {
    case BOARD_ID_AGX_ORIN:
      CapsuleFileName = GetAgxOrinCapsuleFileName (BoardSku, BoardFab);
      break;

    case BOARD_ID_ORIN_NANO:
      CapsuleFileName = GetOrinNanoCapsuleFileName ();
      break;

    case BOARD_ID_AGX_THOR:
      CapsuleFileName = CAPSULE_3834_AGX;
      break;

    default:
      PreIsoLogPrint (L"%a: Unknown board ID %d, using default capsule\r\n", __FUNCTION__, BoardId);
      CapsuleFileName = CAPSULE_DEFAULT_NAME;
      break;
  }

Done:
  PreIsoLogWrite (L"%a: Selected capsule: %s\r\n", __FUNCTION__, CapsuleFileName);

  UnicodeSPrint (CapsuleSource, BufferSize, L"%s%s", CAPSULE_SOURCE_DIR, CapsuleFileName);
  UnicodeSPrint (CapsuleDest, BufferSize, L"%s%s", CAPSULE_DEST_DIR, CapsuleFileName);

  return EFI_SUCCESS;
}

/**
  Check whether an ASCII byte buffer contains a string.

  @param[in]  Buffer     Buffer to search.
  @param[in]  BufferLen  Size of Buffer in bytes.
  @param[in]  String     NUL-terminated string to find.

  @retval TRUE   Buffer contains String.
  @retval FALSE  Buffer does not contain String.

**/
STATIC
BOOLEAN
AsciiBufferContainsString (
  IN CONST CHAR8  *Buffer,
  IN UINTN        BufferLen,
  IN CONST CHAR8  *String
  )
{
  UINTN  Index;
  UINTN  StringLen;

  if ((Buffer == NULL) || (String == NULL)) {
    return FALSE;
  }

  StringLen = AsciiStrLen (String);
  if ((StringLen == 0) || (BufferLen < StringLen)) {
    return FALSE;
  }

  for (Index = 0; Index <= BufferLen - StringLen; Index++) {
    if (CompareMem (&Buffer[Index], String, StringLen) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Read the root compatible property from the installed DTB.

  @param[out] Compatible     Pointer to receive the compatible property.
  @param[out] CompatibleLen  Pointer to receive the property size in bytes.

  @retval TRUE   Compatible property was read.
  @retval FALSE  Compatible property is unavailable.

**/
STATIC
BOOLEAN
EFIAPI
ReadPlatformCompatible (
  OUT CONST CHAR8  **Compatible,
  OUT UINTN        *CompatibleLen
  )
{
  EFI_STATUS  Status;
  VOID        *FdtBase;
  CONST VOID  *CompatibleProp;
  INT32       PropLen;

  if ((Compatible == NULL) || (CompatibleLen == NULL)) {
    return FALSE;
  }

  *Compatible    = NULL;
  *CompatibleLen = 0;

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &FdtBase);
  if (EFI_ERROR (Status) || (FdtBase == NULL)) {
    return FALSE;
  }

  if (FdtCheckHeader (FdtBase) != 0) {
    return FALSE;
  }

  PropLen        = 0;
  CompatibleProp = FdtGetProp (FdtBase, 0, "compatible", &PropLen);
  if ((CompatibleProp == NULL) || (PropLen <= 0)) {
    return FALSE;
  }

  *Compatible    = CompatibleProp;
  *CompatibleLen = (UINTN)PropLen;

  return TRUE;
}

/**
  Resolve the board name and compatible FAB string based on board ID, SKU, and FAB.

  @param[in]  BoardId     The numeric board ID.
  @param[in]  BoardSku    The numeric board SKU.
  @param[in]  FabStr      The 3-character FAB string from EEPROM.
  @param[out] BoardName   Pointer to receive the board name string.
  @param[out] CompatFab   Pointer to receive the compatible FAB string.

**/
STATIC
VOID
EFIAPI
ResolveCompatSpecParams (
  IN  UINT32       BoardId,
  IN  UINT32       BoardSku,
  IN  CONST CHAR8  *FabStr,
  OUT CONST CHAR8  **BoardName,
  OUT CONST CHAR8  **CompatFab
  )
{
  CONST CHAR8  *Compatible;
  UINTN        CompatibleLen;
  BOOLEAN      IsNanoE8GBConf;
  BOOLEAN      IsSuperConf;
  UINT32       NumericFab;

  NumericFab = 0;
  if ((FabStr[0] >= '0') && (FabStr[0] <= '9') &&
      (FabStr[1] >= '0') && (FabStr[1] <= '9') &&
      (FabStr[2] >= '0') && (FabStr[2] <= '9'))
  {
    NumericFab = (UINT32)((FabStr[0] - '0') * 100 +
                          (FabStr[1] - '0') * 10 +
                          (FabStr[2] - '0'));
  }

  IsNanoE8GBConf = FALSE;
  IsSuperConf    = FALSE;
  if (ReadPlatformCompatible (&Compatible, &CompatibleLen)) {
    IsNanoE8GBConf = AsciiBufferContainsString (Compatible, CompatibleLen, "nanoe8gb");
    IsSuperConf    = AsciiBufferContainsString (Compatible, CompatibleLen, "super");
  }

  switch (BoardId) {
    case BOARD_ID_AGX_ORIN:
      if ((BoardSku == 4) || (BoardSku == 5)) {
        *CompatFab = COMPAT_FAB_300;
        *BoardName = BOARD_NAME_AGX_ORIN_DEVKIT;
      } else if (BoardSku == 8) {
        *CompatFab = COMPAT_FAB_300;
        *BoardName = BOARD_NAME_AGX_ORIN_DEVKIT_INDUSTRIAL;
      } else if (BoardSku == 0) {
        *BoardName = BOARD_NAME_AGX_ORIN_DEVKIT;
        if ((FabStr[0] == 'T') || (FabStr[0] == 'E') || (NumericFab < 300)) {
          *CompatFab = COMPAT_FAB_000;
        } else {
          *CompatFab = COMPAT_FAB_300;
        }
      } else {
        *CompatFab = COMPAT_FAB_300;
        *BoardName = BOARD_NAME_AGX_ORIN_DEVKIT;
      }

      if (AsciiStrCmp (*BoardName, BOARD_NAME_AGX_ORIN_DEVKIT) == 0) {
        if (IsSuperConf) {
          *BoardName = BOARD_NAME_AGX_ORIN_DEVKIT_SUPER;
        }
      }

      break;

    case BOARD_ID_ORIN_NANO:
      *CompatFab = COMPAT_FAB_000;
      if (IsNanoE8GBConf) {
        if (IsSuperConf) {
          *BoardName = BOARD_NAME_ORIN_NANOE8GB_DEVKIT_SUPER;
        } else {
          *BoardName = BOARD_NAME_ORIN_NANOE8GB_DEVKIT;
        }
      } else if (IsSuperConf) {
        *BoardName = BOARD_NAME_ORIN_NANO_DEVKIT_SUPER;
      } else {
        *BoardName = BOARD_NAME_ORIN_NANO_DEVKIT;
      }

      break;

    case BOARD_ID_AGX_THOR:
      if (BoardSku == 0) {
        *CompatFab = COMPAT_FAB_000;
        *BoardName = BOARD_NAME_AGX_THOR_T4000;
      } else if (BoardSku == 8) {
        *BoardName = BOARD_NAME_AGX_THOR_DEVKIT;
        if (((FabStr[0] == 'E') && (FabStr[1] == 'B') && (FabStr[2] >= '9')) ||
            ((FabStr[0] == 'T') && (FabStr[1] == 'S') && (FabStr[2] >= '5')) ||
            ((FabStr[0] == 'R') && (FabStr[1] == 'C') && (FabStr[2] >= '2')) ||
            (NumericFab > 400))
        {
          *CompatFab = COMPAT_FAB_401;
        } else {
          *CompatFab = COMPAT_FAB_000;
        }
      } else {
        *CompatFab = COMPAT_FAB_000;
        *BoardName = BOARD_NAME_AGX_THOR_DEVKIT;
      }

      break;

    default:
      *CompatFab = COMPAT_FAB_000;
      *BoardName = "jetson-unknown";
      break;
  }
}

/**
  Generate TegraPlatformSpec string from board information.

  @param[in]  BoardInfo     Pointer to board information from EEPROM.
  @param[out] SpecString    Buffer to store the generated spec string.
  @param[in]  BufferSize    Size of the buffer.

  @retval EFI_SUCCESS           Spec string generated successfully.
  @retval EFI_INVALID_PARAMETER Invalid parameters.

**/
STATIC
EFI_STATUS
EFIAPI
GeneratePlatformSpecString (
  IN  CONST TEGRA_EEPROM_BOARD_INFO  *BoardInfo,
  OUT CHAR8                          *SpecString,
  IN  UINTN                          BufferSize
  )
{
  CONST EEPROM_PART_NUMBER  *EepromPartNumber;
  CHAR8                     BoardIdStr[5];
  CHAR8                     SkuStr[5];
  CHAR8                     FabStr[4];
  CHAR8                     RevisionChar;
  UINT32                    BoardId;
  UINT32                    BoardSku;
  CONST CHAR8               *BoardName;
  CONST CHAR8               *Unused;
  UINTN                     SpecLen;

  if ((BoardInfo == NULL) || (SpecString == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize < MAX_SPEC_STRING_LEN) {
    return EFI_BUFFER_TOO_SMALL;
  }

  EepromPartNumber = (EEPROM_PART_NUMBER *)&BoardInfo->ProductId[0];

  if (!IsTegraBoardFormat (EepromPartNumber)) {
    PreIsoLogPrint (L"%a: Non-Tegra EEPROM format, cannot generate spec string\r\n", __FUNCTION__);
    return EFI_UNSUPPORTED;
  }

  BoardIdStr[0] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Id[0];
  BoardIdStr[1] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Id[1];
  BoardIdStr[2] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Id[2];
  BoardIdStr[3] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Id[3];
  BoardIdStr[4] = '\0';

  SkuStr[0] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Sku[0];
  SkuStr[1] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Sku[1];
  SkuStr[2] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Sku[2];
  SkuStr[3] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Sku[3];
  SkuStr[4] = '\0';

  FabStr[0] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Fab[0];
  FabStr[1] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Fab[1];
  FabStr[2] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Fab[2];
  FabStr[3] = '\0';

  RevisionChar = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Revision;
  if ((RevisionChar < 'A') || (RevisionChar > 'Z')) {
    RevisionChar = 'A';
  }

  BoardId  = 0;
  BoardSku = 0;
  if ((BoardIdStr[0] >= '0') && (BoardIdStr[0] <= '9') &&
      (BoardIdStr[1] >= '0') && (BoardIdStr[1] <= '9') &&
      (BoardIdStr[2] >= '0') && (BoardIdStr[2] <= '9') &&
      (BoardIdStr[3] >= '0') && (BoardIdStr[3] <= '9'))
  {
    BoardId = (BoardIdStr[0] - '0') * 1000 +
              (BoardIdStr[1] - '0') * 100 +
              (BoardIdStr[2] - '0') * 10 +
              (BoardIdStr[3] - '0');
  }

  if ((SkuStr[0] >= '0') && (SkuStr[0] <= '9') &&
      (SkuStr[1] >= '0') && (SkuStr[1] <= '9') &&
      (SkuStr[2] >= '0') && (SkuStr[2] <= '9') &&
      (SkuStr[3] >= '0') && (SkuStr[3] <= '9'))
  {
    BoardSku = (SkuStr[0] - '0') * 1000 +
               (SkuStr[1] - '0') * 100 +
               (SkuStr[2] - '0') * 10 +
               (SkuStr[3] - '0');
  }

  ResolveCompatSpecParams (BoardId, BoardSku, FabStr, &BoardName, &Unused);

  SpecLen = AsciiSPrint (
              SpecString,
              BufferSize,
              "%a-%a-%a-%c.0-1-2-%a-",
              BoardIdStr,
              FabStr,
              SkuStr,
              RevisionChar,
              BoardName
              );

  if ((SpecLen == 0) || (SpecLen >= BufferSize - 1)) {
    PreIsoLogPrint (L"%a: Spec string truncated or empty\r\n", __FUNCTION__);
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Generate TegraPlatformCompatSpec string from board information.

  @param[in]  BoardInfo         Pointer to board information from EEPROM.
  @param[out] CompatSpecString  Buffer to store the generated compat spec string.
  @param[in]  BufferSize        Size of the buffer.

  @retval EFI_SUCCESS           Compat spec string generated successfully.
  @retval EFI_INVALID_PARAMETER Invalid parameters.

**/
STATIC
EFI_STATUS
EFIAPI
GeneratePlatformCompatSpecString (
  IN  CONST TEGRA_EEPROM_BOARD_INFO  *BoardInfo,
  OUT CHAR8                          *CompatSpecString,
  IN  UINTN                          BufferSize
  )
{
  CONST EEPROM_PART_NUMBER  *EepromPartNumber;
  CHAR8                     BoardIdStr[5];
  CHAR8                     SkuStr[5];
  CHAR8                     FabStr[4];
  UINT32                    BoardId;
  UINT32                    BoardSku;
  CONST CHAR8               *BoardName;
  CONST CHAR8               *CompatFab;
  UINTN                     SpecLen;

  if ((BoardInfo == NULL) || (CompatSpecString == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize < MAX_SPEC_STRING_LEN) {
    return EFI_BUFFER_TOO_SMALL;
  }

  EepromPartNumber = (EEPROM_PART_NUMBER *)&BoardInfo->ProductId[0];

  if (!IsTegraBoardFormat (EepromPartNumber)) {
    PreIsoLogPrint (L"%a: Non-Tegra EEPROM format, cannot generate compat spec string\r\n", __FUNCTION__);
    return EFI_UNSUPPORTED;
  }

  BoardIdStr[0] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Id[0];
  BoardIdStr[1] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Id[1];
  BoardIdStr[2] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Id[2];
  BoardIdStr[3] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Id[3];
  BoardIdStr[4] = '\0';

  SkuStr[0] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Sku[0];
  SkuStr[1] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Sku[1];
  SkuStr[2] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Sku[2];
  SkuStr[3] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Sku[3];
  SkuStr[4] = '\0';

  FabStr[0] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Fab[0];
  FabStr[1] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Fab[1];
  FabStr[2] = (CHAR8)EepromPartNumber->TegraEepromPartNumber.Fab[2];
  FabStr[3] = '\0';

  BoardId  = 0;
  BoardSku = 0;
  if ((BoardIdStr[0] >= '0') && (BoardIdStr[0] <= '9') &&
      (BoardIdStr[1] >= '0') && (BoardIdStr[1] <= '9') &&
      (BoardIdStr[2] >= '0') && (BoardIdStr[2] <= '9') &&
      (BoardIdStr[3] >= '0') && (BoardIdStr[3] <= '9'))
  {
    BoardId = (BoardIdStr[0] - '0') * 1000 +
              (BoardIdStr[1] - '0') * 100 +
              (BoardIdStr[2] - '0') * 10 +
              (BoardIdStr[3] - '0');
  }

  if ((SkuStr[0] >= '0') && (SkuStr[0] <= '9') &&
      (SkuStr[1] >= '0') && (SkuStr[1] <= '9') &&
      (SkuStr[2] >= '0') && (SkuStr[2] <= '9') &&
      (SkuStr[3] >= '0') && (SkuStr[3] <= '9'))
  {
    BoardSku = (SkuStr[0] - '0') * 1000 +
               (SkuStr[1] - '0') * 100 +
               (SkuStr[2] - '0') * 10 +
               (SkuStr[3] - '0');
  }

  ResolveCompatSpecParams (BoardId, BoardSku, FabStr, &BoardName, &CompatFab);

  SpecLen = AsciiSPrint (
              CompatSpecString,
              BufferSize,
              "%a-%a-%a--1--%a-",
              BoardIdStr,
              CompatFab,
              SkuStr,
              BoardName
              );

  if ((SpecLen == 0) || (SpecLen >= BufferSize - 1)) {
    PreIsoLogPrint (L"%a: Compat spec string truncated or empty\r\n", __FUNCTION__);
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Check and create TegraPlatformSpec and TegraPlatformCompatSpec UEFI variables.

  @param[in]  BoardInfo     Pointer to board information from EEPROM.

  @retval EFI_SUCCESS       Variables exist or were created successfully.
  @retval EFI_NOT_FOUND     Board info not available.
  @retval Other             Error occurred during variable operations.

**/
STATIC
EFI_STATUS
EFIAPI
EnsurePlatformSpecVariables (
  IN CONST TEGRA_EEPROM_BOARD_INFO  *BoardInfo
  )
{
  EFI_STATUS  Status;
  CHAR8       SpecString[MAX_SPEC_STRING_LEN];
  CHAR8       CompatSpecString[MAX_SPEC_STRING_LEN];
  CHAR8       ExistingSpec[MAX_SPEC_STRING_LEN];
  UINTN       DataSize;
  BOOLEAN     SpecExists;
  BOOLEAN     CompatSpecExists;

  if (BoardInfo == NULL) {
    PreIsoLogPrint (L"%a: BoardInfo is NULL\r\n", __FUNCTION__);
    return EFI_INVALID_PARAMETER;
  }

  DataSize   = sizeof (ExistingSpec) - 1;
  SpecExists = FALSE;
  Status     = gRT->GetVariable (
                      TEGRA_PLATFORM_SPEC_VARIABLE_NAME,
                      &gNVIDIAPublicVariableGuid,
                      NULL,
                      &DataSize,
                      ExistingSpec
                      );
  if (!EFI_ERROR (Status)) {
    ExistingSpec[DataSize] = '\0';
    SpecExists             = TRUE;
    PreIsoLogWrite (L"%a: TegraPlatformSpec exists: %a\r\n", __FUNCTION__, ExistingSpec);
  } else if (Status != EFI_NOT_FOUND) {
    PreIsoLogPrint (L"%a: Error reading TegraPlatformSpec: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  DataSize         = sizeof (ExistingSpec) - 1;
  CompatSpecExists = FALSE;
  Status           = gRT->GetVariable (
                            TEGRA_PLATFORM_COMPAT_SPEC_VARIABLE_NAME,
                            &gNVIDIAPublicVariableGuid,
                            NULL,
                            &DataSize,
                            ExistingSpec
                            );
  if (!EFI_ERROR (Status)) {
    ExistingSpec[DataSize] = '\0';
    CompatSpecExists       = TRUE;
    PreIsoLogWrite (L"%a: TegraPlatformCompatSpec exists: %a\r\n", __FUNCTION__, ExistingSpec);
  } else if (Status != EFI_NOT_FOUND) {
    PreIsoLogPrint (L"%a: Error reading TegraPlatformCompatSpec: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  if (SpecExists && CompatSpecExists) {
    PreIsoLogWrite (L"%a: Both spec variables already exist\r\n", __FUNCTION__);
    return EFI_SUCCESS;
  }

  if (!SpecExists) {
    Status = GeneratePlatformSpecString (BoardInfo, SpecString, sizeof (SpecString));
    if (EFI_ERROR (Status)) {
      PreIsoLogPrint (L"%a: Failed to generate TegraPlatformSpec: %r\r\n", __FUNCTION__, Status);
      return Status;
    }

    Status = gRT->SetVariable (
                    TEGRA_PLATFORM_SPEC_VARIABLE_NAME,
                    &gNVIDIAPublicVariableGuid,
                    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                    AsciiStrLen (SpecString) + 1,
                    SpecString
                    );
    if (EFI_ERROR (Status)) {
      PreIsoLogPrint (L"%a: Failed to set TegraPlatformSpec: %r\r\n", __FUNCTION__, Status);
      return Status;
    }

    PreIsoLogWrite (L"%a: Created TegraPlatformSpec: %a\r\n", __FUNCTION__, SpecString);
  }

  if (!CompatSpecExists) {
    Status = GeneratePlatformCompatSpecString (BoardInfo, CompatSpecString, sizeof (CompatSpecString));
    if (EFI_ERROR (Status)) {
      PreIsoLogPrint (L"%a: Failed to generate TegraPlatformCompatSpec: %r\r\n", __FUNCTION__, Status);
      return Status;
    }

    Status = gRT->SetVariable (
                    TEGRA_PLATFORM_COMPAT_SPEC_VARIABLE_NAME,
                    &gNVIDIAPublicVariableGuid,
                    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                    AsciiStrLen (CompatSpecString) + 1,
                    CompatSpecString
                    );
    if (EFI_ERROR (Status)) {
      PreIsoLogPrint (L"%a: Failed to set TegraPlatformCompatSpec: %r\r\n", __FUNCTION__, Status);
      return Status;
    }

    PreIsoLogWrite (L"%a: Created TegraPlatformCompatSpec: %a\r\n", __FUNCTION__, CompatSpecString);
  }

  return EFI_SUCCESS;
}

/**
  Print an error message indicating the current UEFI firmware is too old
  to support ISO installation.

  @param[in]  CurrentSlotVersion  Version of the current active slot.

**/
STATIC
VOID
PrintUefiVersionUnsupported (
  IN UINT32  CurrentSlotVersion
  )
{
  PreIsoLogPrint (L"\r\n");
  PreIsoLogPrint (L"  *** NVIDIA UEFI Firmware Version Too Old ***\r\n");
  PreIsoLogPrint (L"\r\n");
  PreIsoLogPrint (
    L"  Current version : %d.%02d.%02d\r\n",
    (CurrentSlotVersion >> 16) & 0xFF,
    (CurrentSlotVersion >> 8) & 0xFF,
    CurrentSlotVersion & 0xFF
    );
  PreIsoLogPrint (L"\r\n");
  PreIsoLogPrint (L"  Your UEFI version is too old and does not support the current ISO.\r\n");
  PreIsoLogPrint (L"  Please install latest JP5 BSP and try again.\r\n");
  PreIsoLogPrint (L"\r\n");
}

/**
  Check whether the current chain version meets the minimum requirement
  for capsule update with layout change

  If the current slot version is earlier than 35.5.0, the UEFI firmware
  does not support capsule update with layout change. In that case the
  error banner is displayed and FALSE is returned; the caller converts
  this into EFI_ABORTED to abort ISO installation.

  @param[in]  CurrentSlotVersion  Version of the current active slot.

  @retval TRUE    Firmware version meets the minimum requirement.
  @retval FALSE   Firmware version is too old; capsule update with layout change is not supported.

**/
STATIC
BOOLEAN
IsCapsuleUpdateSupportLayoutChange (
  IN UINT32  CurrentSlotVersion
  )
{
  // 35.5.0 (0x230500) is the minimum UEFI version that supports
  // capsule update with layout change.
  if (CurrentSlotVersion < 0x230500) {
    PreIsoLogWrite (
      L"%a: Current slot version (0x%x) < 35.5.0 (0x230500) -> UEFI too old\r\n",
      __FUNCTION__,
      CurrentSlotVersion
      );
    PrintUefiVersionUnsupported (CurrentSlotVersion);
    return FALSE;
  }

  return TRUE;
}

/**
  Determine if capsule update is needed based on version comparison.

  @param[in]  PreIsoInstallerVersion      Version from capsule.
  @param[in]  CurrentSlotVersion    Version of current active slot.
  @param[in]  NonCurrentSlotVersion Version of non-active slot.

  @retval TRUE   Capsule update is needed.
  @retval FALSE  Capsule update is not needed.

**/
STATIC
BOOLEAN
EFIAPI
ShouldPerformCapsuleUpdate (
  IN UINT32  PreIsoInstallerVersion,
  IN UINT32  CurrentSlotVersion,
  IN UINT32  NonCurrentSlotVersion
  )
{
  if (PreIsoInstallerVersion < NonCurrentSlotVersion) {
    PreIsoLogWrite (
      L"%a: ISO version (0x%x) < non-current slot version (0x%x) -> Skipping update\r\n",
      __FUNCTION__,
      PreIsoInstallerVersion,
      NonCurrentSlotVersion
      );
    return FALSE;
  }

  if (PreIsoInstallerVersion > CurrentSlotVersion) {
    PreIsoLogWrite (
      L"%a: ISO version (0x%x) > current slot version (0x%x) -> Update required\r\n",
      __FUNCTION__,
      PreIsoInstallerVersion,
      CurrentSlotVersion
      );
    return TRUE;
  }

  if (PreIsoInstallerVersion > NonCurrentSlotVersion) {
    PreIsoLogWrite (
      L"%a: ISO version (0x%x) > non-current slot version (0x%x) -> Update required\r\n",
      __FUNCTION__,
      PreIsoInstallerVersion,
      NonCurrentSlotVersion
      );
    return TRUE;
  }

  PreIsoLogWrite (L"%a: ISO version (0x%x) is not newer than any slot -> Skipping update\r\n", __FUNCTION__, PreIsoInstallerVersion);
  return FALSE;
}

/**
  Prompt the user to confirm the QSPI capsule update.

  Displays the capsule file name and firmware version information,
  then waits up to CAPSULE_CONFIRM_TIMEOUT_SEC seconds for the user
  to press 'Y' (proceed) or 'N' (skip).  If no key is pressed before
  the timeout, the update is skipped automatically.

  @param[in]  PreIsoInstallerVersion Capsule firmware version.
  @param[in]  CurrentSlotVersion     Current slot firmware version.

  @retval TRUE   User confirmed the update.
  @retval FALSE  User declined (or timeout expired).

**/
STATIC
BOOLEAN
ConfirmCapsuleUpdate (
  IN UINT32  PreIsoInstallerVersion,
  IN UINT32  CurrentSlotVersion
  )
{
  EFI_STATUS     Status;
  EFI_INPUT_KEY  Key;
  EFI_EVENT      TimerEvent;
  EFI_EVENT      WaitEvents[2];
  UINTN          EventIndex;
  UINTN          Remaining;

  PreIsoLogPrint (L"\r\n");
  PreIsoLogPrint (L"  *** NVIDIA QSPI Firmware Update Required ***\r\n");
  PreIsoLogPrint (L"\r\n");
  PreIsoLogPrint (L"  A newer firmware version has been detected.\r\n");
  PreIsoLogPrint (
    L"  New FW version  : %d.%02d.%02d\r\n",
    (PreIsoInstallerVersion >> 16) & 0xFF,
    (PreIsoInstallerVersion >> 8) & 0xFF,
    PreIsoInstallerVersion & 0xFF
    );
  PreIsoLogPrint (
    L"  Current version : %d.%02d.%02d\r\n",
    (CurrentSlotVersion >> 16) & 0xFF,
    (CurrentSlotVersion >> 8) & 0xFF,
    CurrentSlotVersion & 0xFF
    );
  PreIsoLogPrint (L"\r\n");
  PreIsoLogPrint (L"  WARNING: Skipping the firmware update may cause the\r\n");
  PreIsoLogPrint (L"  subsequent ISO installation to fail.\r\n");
  PreIsoLogPrint (L"\r\n");
  PreIsoLogPrint (L"  Do you want to update the firmware?\r\n");
  PreIsoLogPrint (L"  Press [Y] to proceed, [N] to skip.\r\n");
  PreIsoLogPrint (L"  Auto-skipping in %d seconds...\r\n", CAPSULE_CONFIRM_TIMEOUT_SEC);
  PreIsoLogPrint (L"\r\n");
  PreIsoLogPrint (L"  Both firmware slots (A and B) will be updated.\r\n");
  PreIsoLogPrint (L"  This requires 2 automatic reboots. Do not power off\r\n");
  PreIsoLogPrint (L"  the system until the update is complete.\r\n");
  PreIsoLogPrint (L"\r\n");

  // Drain any buffered keystrokes.
  while (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key) == EFI_SUCCESS) {
  }

  Status = gBS->CreateEvent (EVT_TIMER, TPL_CALLBACK, NULL, NULL, &TimerEvent);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Cannot create timer, skipping update\r\n", __FUNCTION__);
    return FALSE;
  }

  Status = gBS->SetTimer (TimerEvent, TimerRelative, 10000000ULL);
  if (EFI_ERROR (Status)) {
    gBS->CloseEvent (TimerEvent);
    PreIsoLogPrint (L"%a: Cannot set timer, skipping update\r\n", __FUNCTION__);
    return FALSE;
  }

  WaitEvents[0] = gST->ConIn->WaitForKey;
  WaitEvents[1] = TimerEvent;
  Remaining     = CAPSULE_CONFIRM_TIMEOUT_SEC;

  while (Remaining > 0) {
    Status = gBS->WaitForEvent (2, WaitEvents, &EventIndex);
    if (EFI_ERROR (Status)) {
      break;
    }

    if (EventIndex == 0) {
      Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
      if (!EFI_ERROR (Status)) {
        if ((Key.UnicodeChar == L'Y') || (Key.UnicodeChar == L'y')) {
          PreIsoLogPrint (L"  User confirmed. Proceeding with capsule update.\r\n");
          gBS->CloseEvent (TimerEvent);
          return TRUE;
        }

        if ((Key.UnicodeChar == L'N') || (Key.UnicodeChar == L'n')) {
          PreIsoLogPrint (L"  User declined. Skipping capsule update.\r\n");
          gBS->CloseEvent (TimerEvent);
          return FALSE;
        }
      }
    } else {
      Remaining--;
      PreIsoLogPrint (L"  %d seconds remaining...\r\n", Remaining);
      if (Remaining == 0) {
        break;
      }

      gBS->SetTimer (TimerEvent, TimerRelative, 10000000ULL);
    }
  }

  gBS->CloseEvent (TimerEvent);
  PreIsoLogPrint (L"  Timeout reached. Skipping capsule update.\r\n");
  return FALSE;
}

/**
  Perform capsule update process.

  @param[in]  DeviceHandle    Device handle for file operations.
  @param[in]  CapsuleSource   Source capsule file path.
  @param[in]  CapsuleDest     Destination capsule file path.

  @retval EFI_SUCCESS    Capsule update prepared and warm reset triggered.
  @retval Other          Error occurred during capsule update preparation.

**/
STATIC
EFI_STATUS
EFIAPI
PerformCapsuleUpdate (
  IN EFI_HANDLE    DeviceHandle,
  IN CONST CHAR16  *CapsuleSource,
  IN CONST CHAR16  *CapsuleDest
  )
{
  EFI_STATUS  Status;

  PreIsoLogWrite (L"%a: Preparing capsule update: %s -> %s\r\n", __FUNCTION__, CapsuleSource, CapsuleDest);

  Status = CopyFile (DeviceHandle, CapsuleSource, CapsuleDest);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to copy capsule file: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  Status = SetCapsuleStatusVariable (TRUE);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Unable to set OsIndications variable: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  PreIsoLogPrint (L"Capsule update prepared, resetting the system in 2 seconds.\r\n");
  gBS->Stall (2 * 1000000);
  gRT->ResetSystem (EfiResetWarm, EFI_SUCCESS, 0, NULL);

  PreIsoLogPrint (L"%a: ResetSystem unexpectedly returned\r\n", __FUNCTION__);
  return EFI_DEVICE_ERROR;
}

/**
  Load and start shim bootloader from ESP.

  This function loads EFI\BOOT\shimaa64.efi and starts it.
  If successful, this function does not return.

  @param[in]  ImageHandle    The image handle of the calling application.
  @param[in]  DeviceHandle   Device handle for ESP.

  @retval EFI_NOT_FOUND      Shim file not found.
  @retval Other              Error occurred loading or starting shim.

**/
EFI_STATUS
EFIAPI
LoadAndStartShim (
  IN EFI_HANDLE  ImageHandle,
  IN EFI_HANDLE  DeviceHandle
  )
{
  EFI_STATUS       Status;
  EFI_DEVICE_PATH  *ShimDevicePath;
  EFI_HANDLE       ShimHandle;

  ShimDevicePath = FileDevicePath (DeviceHandle, SHIM_PATH);
  if (ShimDevicePath == NULL) {
    PreIsoLogPrint (L"%a: Failed to create device path for shim\r\n", __FUNCTION__);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gBS->LoadImage (FALSE, ImageHandle, ShimDevicePath, NULL, 0, &ShimHandle);
  FreePool (ShimDevicePath);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to load shim: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  PreIsoLogWrite (L"%a: Shim loaded successfully, starting...\r\n", __FUNCTION__);

  gBS->SetWatchdogTimer (5 * 60, 0x10000, 0, NULL);

  Status = gBS->StartImage (ShimHandle, NULL, NULL);

  gBS->SetWatchdogTimer (0, 0x10000, 0, NULL);
  gBS->UnloadImage (ShimHandle);

  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Shim returned with error: %r\r\n", __FUNCTION__, Status);
  } else {
    PreIsoLogPrint (L"%a: Shim returned successfully\r\n", __FUNCTION__);
  }

  return Status;
}

/**
  Execute PreIsoInstaller logic.

  @param[in]  ImageHandle    The image handle of this application.
  @param[in]  DeviceHandle   Device handle for ESP.
  @param[in]  BootChain      Current boot chain.

  @retval EFI_SUCCESS        No update needed, continue with normal boot.
  @retval EFI_NOT_READY      Update not needed, continue with normal boot.
  @retval Other              Error occurred during PreIsoInstaller execution.

**/
EFI_STATUS
EFIAPI
RunPreIsoInstaller (
  IN EFI_HANDLE  ImageHandle,
  IN EFI_HANDLE  DeviceHandle,
  IN UINT32      BootChain
  )
{
  EFI_STATUS               Status;
  UINT32                   VersionSlotA;
  UINT32                   VersionSlotB;
  UINT32                   PreIsoInstallerVersion;
  UINT32                   CurrentSlotVersion;
  UINT32                   NonCurrentSlotVersion;
  TEGRA_EEPROM_BOARD_INFO  *CvmBoardInfo;
  BOOLEAN                  NeedsCapsuleUpdate;
  CHAR16                   CapsuleSourcePath[MAX_CAPSULE_PATH_CHARS];
  CHAR16                   CapsuleDestPath[MAX_CAPSULE_PATH_CHARS];
  UINT8                    StagedFlag;
  UINTN                    DataSize;

  PreIsoLogInit (DeviceHandle);
  PreIsoLogWrite (L"%a: === NVIDIA Capsule Update Check ===\r\n", __FUNCTION__);

  if (BootChain > 1) {
    PreIsoLogPrint (L"%a: Invalid BootChain value: %u\r\n", __FUNCTION__, BootChain);
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  Status = GetSystemFwVersions (&VersionSlotA, &VersionSlotB);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Unable to read firmware versions: %r\r\n", __FUNCTION__, Status);
    goto Done;
  }

  Status = GetPreIsoInstallerVersion (DeviceHandle, &PreIsoInstallerVersion);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Unable to read PreIsoInstaller version: %r\r\n", __FUNCTION__, Status);
    goto Done;
  }

  if (BootChain == 0) {
    CurrentSlotVersion    = VersionSlotA;
    NonCurrentSlotVersion = VersionSlotB;
  } else {
    CurrentSlotVersion    = VersionSlotB;
    NonCurrentSlotVersion = VersionSlotA;
  }

  PreIsoLogWrite (L"%a: Current Boot Chain: %d\r\n", __FUNCTION__, BootChain);
  PreIsoLogWrite (L"%a: Capsule Version: 0x%x, Slot 0: 0x%x, Slot 1: 0x%x\r\n", __FUNCTION__, PreIsoInstallerVersion, VersionSlotA, VersionSlotB);

  if (!IsCapsuleUpdateSupportLayoutChange (CurrentSlotVersion)) {
    Status = EFI_ABORTED;
    goto Done;
  }

  NeedsCapsuleUpdate = ShouldPerformCapsuleUpdate (
                         PreIsoInstallerVersion,
                         CurrentSlotVersion,
                         NonCurrentSlotVersion
                         );

  if (!NeedsCapsuleUpdate) {
    Status = gRT->SetVariable (
                    PREISO_CAPSULE_STAGED_VARIABLE_NAME,
                    &gNVIDIAPublicVariableGuid,
                    0,
                    0,
                    NULL
                    );
    if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
      PreIsoLogPrint (L"%a: Failed to clear staged flag: %r\r\n", __FUNCTION__, Status);
    }

    Status = EFI_NOT_READY;
    goto Done;
  }

  // Guard against boot loop: capsule updates normally trigger twice (once per
  // A/B slot). Allow up to 5 staging attempts; abort after that to break a
  // loop where capsule processing fails to bump the firmware version.
  StagedFlag = 0;
  DataSize   = sizeof (StagedFlag);
  Status     = gRT->GetVariable (
                      PREISO_CAPSULE_STAGED_VARIABLE_NAME,
                      &gNVIDIAPublicVariableGuid,
                      NULL,
                      &DataSize,
                      &StagedFlag
                      );
  if (EFI_ERROR (Status)) {
    StagedFlag = 0;
  }

  if (StagedFlag >= 5) {
    PreIsoLogPrint (
      L"%a: Capsule staged %d times but version not bumped, aborting to prevent boot loop\r\n",
      __FUNCTION__,
      StagedFlag
      );
    gRT->SetVariable (
           PREISO_CAPSULE_STAGED_VARIABLE_NAME,
           &gNVIDIAPublicVariableGuid,
           0,
           0,
           NULL
           );
    Status = SetCapsuleStatusVariable (FALSE);
    if (EFI_ERROR (Status)) {
      PreIsoLogPrint (L"%a: Failed to clear OsIndications on abort: %r\r\n", __FUNCTION__, Status);
    }

    Status = EFI_ABORTED;
    goto Done;
  }

  Status = GetBoardInfo (&CvmBoardInfo);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Unable to get board information: %r\r\n", __FUNCTION__, Status);
    goto Done;
  }

  Status = EnsurePlatformSpecVariables (CvmBoardInfo);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to ensure platform spec variables: %r\r\n", __FUNCTION__, Status);
    goto Done;
  }

  Status = SelectCapsuleFile (CvmBoardInfo, CapsuleSourcePath, CapsuleDestPath, sizeof (CapsuleSourcePath));
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Unable to select capsule file: %r\r\n", __FUNCTION__, Status);
    goto Done;
  }

  if (StagedFlag == 0) {
    if (!ConfirmCapsuleUpdate (PreIsoInstallerVersion, CurrentSlotVersion)) {
      PreIsoLogPrint (L"%a: Capsule update skipped by user\r\n", __FUNCTION__);
      Status = EFI_NOT_READY;
      goto Done;
    }
  }

  // Increment the staging counter after user confirms but before the actual
  // capsule staging.  This ensures skipping does not consume an attempt.
  StagedFlag++;
  Status = gRT->SetVariable (
                  PREISO_CAPSULE_STAGED_VARIABLE_NAME,
                  &gNVIDIAPublicVariableGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof (StagedFlag),
                  &StagedFlag
                  );
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Failed to set capsule staged variable: %r\r\n", __FUNCTION__, Status);
    goto Done;
  }

  Status = PerformCapsuleUpdate (DeviceHandle, CapsuleSourcePath, CapsuleDestPath);
  if (EFI_ERROR (Status)) {
    PreIsoLogPrint (L"%a: Capsule update failed: %r\r\n", __FUNCTION__, Status);
    PreIsoLogPrint (L"%a: Continuing with normal boot...\r\n", __FUNCTION__);
    Status = EFI_NOT_READY;
  }

Done:
  PreIsoLogClose ();
  return Status;
}

/**
  Detect ISO installation medium and run the full ISO boot path.

  Checks whether the boot media is an ISO installation medium.  If so,
  clears the rootfs status register, runs the PreIsoInstaller capsule
  update flow, and loads the shim bootloader.

  @param[in]  ImageHandle    The image handle of this application.
  @param[in]  DeviceHandle   Device handle for ESP.

  @retval EFI_SUCCESS        ISO medium handled (or not an ISO medium).
  @retval EFI_ABORTED        Capsule boot loop detected — caller must halt.
  @retval Other              Fatal error from shim load.

**/
EFI_STATUS
EFIAPI
HandleIsoBootMedia (
  IN  EFI_HANDLE  ImageHandle,
  IN  EFI_HANDLE  DeviceHandle
  )
{
  EFI_STATUS  Status;
  UINT32      BootChain;
  UINTN       DataSize;

  if (!IsIsoIdFileValid (DeviceHandle)) {
    return EFI_SUCCESS;
  }

  if (!IsIso9660BootMedia (DeviceHandle)) {
    return EFI_SUCCESS;
  }

  ErrorPrint (L"ISO installation medium detected, running PreIsoInstaller logic\r\n");

  BootChain = 0;
  DataSize  = sizeof (BootChain);
  Status    = gRT->GetVariable (
                     BOOT_FW_VARIABLE_NAME,
                     &gNVIDIAPublicVariableGuid,
                     NULL,
                     &DataSize,
                     &BootChain
                     );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to read boot firmware chain, using chain 0: %r\r\n", __FUNCTION__, Status);
    BootChain = 0;
  } else if (BootChain > 1) {
    ErrorPrint (L"%a: Invalid boot firmware chain %u, using chain 0\r\n", __FUNCTION__, BootChain);
    BootChain = 0;
  }

  Status = gL4TSupportProtocol->SetRootfsStatusReg (0x0);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to clear Rootfs status register: %r\r\n", __FUNCTION__, Status);
  }

  Status = RunPreIsoInstaller (ImageHandle, DeviceHandle, BootChain);

  if (Status == EFI_NOT_READY) {
    ErrorPrint (L"%a: PreIsoInstaller: No update needed\r\n", __FUNCTION__);
  } else if (Status == EFI_ABORTED) {
    return EFI_ABORTED;
  } else if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: PreIsoInstaller failed: %r\r\n", __FUNCTION__, Status);
  }

  ErrorPrint (L"%a: Loading shim bootloader\r\n", __FUNCTION__);
  Status = LoadAndStartShim (ImageHandle, DeviceHandle);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Shim failed to load or start: %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  return EFI_SUCCESS;
}
