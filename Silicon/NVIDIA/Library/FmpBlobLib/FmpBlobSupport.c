/** @file

  FMP Blob support functions

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <LastAttemptStatus.h>
#include <Guid/SystemResourceTable.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FmpDeviceLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/FwPartitionDeviceLib.h>
#include <Protocol/FwPartitionProtocol.h>
#include "FmpBlobSupport.h"

#define FMP_WRITE_LOOP_SIZE  (64 * 1024)
#define PROGRESS_START       0
#define PROGRESS_END         100
#define PROGRESS_WRITE       80
#define PROGRESS_VERIFY      PROGRESS_END - PROGRESS_WRITE

#define FMP_BLOB_HEADER_VERSION  0
#define FMP_BLOB_TYPE_SIMPLE     0  // copy blob to offset 0

typedef struct {
  UINT32    HeaderSize;
  UINT32    HeaderVersion;
  UINT32    BlobType;
  UINT8     Reserved[500]; // pad to 512 bytes
} FMP_BLOB_HEADER;
_Static_assert ((sizeof (FMP_BLOB_HEADER) == 512), "bad FMP_BLOB_HEADER size");

// last attempt status error codes
enum {
  LAS_ERROR_BAD_IMAGE_POINTER = LAST_ATTEMPT_STATUS_DEVICE_LIBRARY_MIN_ERROR_CODE_VALUE,
  LAS_ERROR_FMP_LIB_UNINITIALIZED,
  LAS_ERROR_NO_FW_PARTITION_PROTOCOLS,
  LAS_ERROR_NO_BLOB_PARTITION,
  LAS_ERROR_BAD_HEADER_SIZE,
  LAS_ERROR_BAD_HEADER_VERSION,
  LAS_ERROR_BAD_BLOB_TYPE,
  LAS_ERROR_BLOB_WRITE_FAILED,
  LAS_ERROR_BLOB_VERIFY_FAILED,
};

STATIC BOOLEAN     mInitialized     = FALSE;
STATIC EFI_STATUS  mVersionStatus   = EFI_UNSUPPORTED;
STATIC UINT32      mVersion         = 0;
STATIC CHAR16      *mVersionString  = NULL;
STATIC UINT32      mActiveBootChain = MAX_UINT32;
STATIC EFI_EVENT   mEndOfDxeEvent   = NULL;
STATIC EFI_HANDLE  mImageHandle     = NULL;

FMP_DEVICE_LIB_REGISTER_FMP_INSTALLER  mInstaller = NULL;

EFI_STATUS
EFIAPI
UpdateImageProgress (
  IN  UINTN  Completion
  );

/**
  Write a buffer to a FwPartition.

  @param[in]  FwPartitionProtocol   FwPartition protocol structure pointer
  @param[in]  Bytes                 Number of bytes to write
  @param[in]  DataBuffer            Pointer to data to write

  @retval EFI_SUCCESS               The operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
WriteImageFromBuffer (
  IN  NVIDIA_FW_PARTITION_PROTOCOL  *FwPartitionProtocol,
  IN  UINTN                         ImageSize,
  IN  CONST UINT8                   *DataBuffer
  )
{
  EFI_STATUS  Status;
  UINTN       WriteOffset;
  UINTN       WriteBytes;
  UINTN       BytesPerLoop;

  UpdateImageProgress (PROGRESS_START);

  Status       = EFI_SUCCESS;
  BytesPerLoop = FMP_WRITE_LOOP_SIZE;
  WriteOffset  = 0;
  WriteBytes   = ImageSize;
  while (WriteBytes > 0) {
    UINTN  WriteSize;

    WriteSize = (WriteBytes > BytesPerLoop) ? BytesPerLoop : WriteBytes;
    Status    = FwPartitionProtocol->Write (
                                       FwPartitionProtocol,
                                       WriteOffset,
                                       WriteSize,
                                       DataBuffer + WriteOffset
                                       );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Write offset %x %r\n", __FUNCTION__, WriteOffset, Status));
      return Status;
    }

    WriteOffset += WriteSize;
    WriteBytes  -= WriteSize;
    UpdateImageProgress ((WriteOffset * PROGRESS_WRITE) / ImageSize);
  }

  UpdateImageProgress (PROGRESS_WRITE);

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
VerifyImageFromBuffer (
  IN  NVIDIA_FW_PARTITION_PROTOCOL  *FwPartitionProtocol,
  IN  UINTN                         ImageSize,
  IN  CONST UINT8                   *DataBuffer
  )
{
  EFI_STATUS  Status;
  UINTN       ReadOffset;
  UINTN       ReadBytes;
  UINTN       BytesPerLoop;
  UINT8       *Buffer;

  Buffer = AllocateZeroPool (FMP_WRITE_LOOP_SIZE);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status       = EFI_SUCCESS;
  BytesPerLoop = FMP_WRITE_LOOP_SIZE;
  ReadOffset   = 0;
  ReadBytes    = ImageSize;
  while (ReadBytes > 0) {
    UINTN  ReadSize;

    ReadSize = (ReadBytes > BytesPerLoop) ? BytesPerLoop : ReadBytes;
    Status   = FwPartitionProtocol->Read (
                                      FwPartitionProtocol,
                                      ReadOffset,
                                      ReadSize,
                                      Buffer
                                      );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Read offset %x %r\n", __FUNCTION__, ReadOffset, Status));
      return Status;
    }

    if (CompareMem (Buffer, DataBuffer + ReadOffset, ReadSize) != 0) {
      DEBUG ((
        DEBUG_ERROR,
        "Verify Image failed at offset=%u\n",
        ReadOffset
        ));
    }

    ReadOffset += ReadSize;
    ReadBytes  -= ReadSize;
    UpdateImageProgress (PROGRESS_WRITE + ((ReadOffset * PROGRESS_VERIFY) / ImageSize));
  }

  UpdateImageProgress (PROGRESS_END);

  if (Buffer != NULL) {
    FreePool (Buffer);
  }

  return Status;
}

EFI_STATUS
EFIAPI
FmpBlobGetVersion (
  OUT UINT32 *Version, OPTIONAL
  OUT CHAR16  **VersionString   OPTIONAL
  )
{
  UINTN  VersionStringSize;

  if (!mInitialized) {
    return EFI_UNSUPPORTED;
  }

  if (EFI_ERROR (mVersionStatus)) {
    DEBUG ((DEBUG_ERROR, "%a: bad status: %r\n", __FUNCTION__, mVersionStatus));
    return mVersionStatus;
  }

  if (Version != NULL) {
    *Version = mVersion;
  }

  if (VersionString != NULL) {
    // version string must be in allocated pool memory that caller frees
    VersionStringSize = StrSize (mVersionString);
    *VersionString    = (CHAR16 *)AllocateRuntimeCopyPool (
                                    VersionStringSize,
                                    mVersionString
                                    );
    if (*VersionString == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  DEBUG ((DEBUG_INFO, "%a: version 0x%08x (%s)\n", __FUNCTION__, mVersion, mVersionString));

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FmpBlobCheckImage (
  IN  CONST VOID  *Image,
  IN  UINTN       ImageSize,
  OUT UINT32      *ImageUpdatable,
  OUT UINT32      *LastAttemptStatus
  )
{
  FMP_BLOB_HEADER  *BlobHeader;

  DEBUG ((
    DEBUG_ERROR,
    "%a: Image=0x%p ImageSize=%x\n",
    __FUNCTION__,
    Image,
    ImageSize
    ));

  if ((ImageUpdatable == NULL) || (LastAttemptStatus == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Image == NULL) {
    *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_BAD_IMAGE_POINTER;
    return EFI_INVALID_PARAMETER;
  }

  if (!mInitialized) {
    *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_FMP_LIB_UNINITIALIZED;
    return EFI_NOT_READY;
  }

  BlobHeader = (FMP_BLOB_HEADER *)Image;
  if (BlobHeader->HeaderSize >= ImageSize) {
    DEBUG ((DEBUG_ERROR, "%a: bad header size=%u\n", __FUNCTION__, BlobHeader->HeaderSize));
    *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_BAD_HEADER_SIZE;
    return EFI_ABORTED;
  }

  if (BlobHeader->HeaderVersion != FMP_BLOB_HEADER_VERSION) {
    DEBUG ((DEBUG_ERROR, "%a: unknown header version=%u\n", __FUNCTION__, BlobHeader->HeaderVersion));
    *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_BAD_HEADER_VERSION;
    return EFI_ABORTED;
  }

  if (BlobHeader->BlobType != FMP_BLOB_TYPE_SIMPLE) {
    DEBUG ((DEBUG_ERROR, "%a: unknown blob type=%u\n", __FUNCTION__, BlobHeader->BlobType));
    *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_BAD_BLOB_TYPE;
    return EFI_ABORTED;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FmpBlobSetImage (
  IN  CONST VOID *Image,
  IN  UINTN ImageSize,
  IN  CONST VOID *VendorCode, OPTIONAL
  IN  EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS  Progress, OPTIONAL
  IN  UINT32                                         CapsuleFwVersion,
  OUT CHAR16                                         **AbortReason,
  OUT UINT32                                         *LastAttemptStatus
  )
{
  EFI_STATUS                    Status;
  EFI_HANDLE                    *Handles;
  UINTN                         HandleCount;
  NVIDIA_FW_PARTITION_PROTOCOL  *FwPartitionProtocol;
  FW_PARTITION_PRIVATE_DATA     *Private;
  FW_PARTITION_INFO             *PartitionInfo;
  BOOLEAN                       IsProtocolFound;
  FMP_BLOB_HEADER               *BlobHeader;

  IsProtocolFound = FALSE;

  if (LastAttemptStatus == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Image == NULL) {
    *LastAttemptStatus = LAS_ERROR_BAD_IMAGE_POINTER;
    return EFI_INVALID_PARAMETER;
  }

  if (!mInitialized) {
    *LastAttemptStatus = LAS_ERROR_FMP_LIB_UNINITIALIZED;
    return EFI_NOT_READY;
  }

  BlobHeader = (FMP_BLOB_HEADER *)Image;

  DEBUG ((DEBUG_INFO, "%a: header size=0x%x version=%u BlobType=%u\n", __FUNCTION__, BlobHeader->HeaderSize, BlobHeader->HeaderVersion, BlobHeader->BlobType));

  Image      = (VOID *)((UINT64)Image + BlobHeader->HeaderSize);
  ImageSize -= BlobHeader->HeaderSize;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIAFwPartitionProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get FW Partition protocol\n", __FUNCTION__));
    *LastAttemptStatus = LAS_ERROR_NO_FW_PARTITION_PROTOCOLS;
    return EFI_ABORTED;
  }

  do {
    HandleCount--;
    Status = gBS->HandleProtocol (
                    Handles[HandleCount],
                    &gNVIDIAFwPartitionProtocolGuid,
                    (VOID **)&FwPartitionProtocol
                    );
    if (!EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: PartitionName = %s\n", __FUNCTION__, FwPartitionProtocol->PartitionName));
      if (StrCmp (FwPartitionProtocol->PartitionName, L"NorFlash-Blob") == 0) {
        IsProtocolFound = TRUE;
        break;
      }
    }
  } while (HandleCount > 0);

  if (!IsProtocolFound) {
    DEBUG ((DEBUG_ERROR, "%a: Cannot find FW Partition.\n", __FUNCTION__));
    *LastAttemptStatus = LAS_ERROR_NO_BLOB_PARTITION;
    return EFI_ABORTED;
  }

  Private = CR (
              FwPartitionProtocol,
              FW_PARTITION_PRIVATE_DATA,
              Protocol,
              FW_PARTITION_PRIVATE_DATA_SIGNATURE
              );
  PartitionInfo                    = &Private->PartitionInfo;
  PartitionInfo->IsActivePartition = FALSE;

  Status = WriteImageFromBuffer (FwPartitionProtocol, ImageSize, Image);
  if (EFI_ERROR (Status)) {
    *LastAttemptStatus = LAS_ERROR_BLOB_WRITE_FAILED;
    return EFI_ABORTED;
  }

  Status = VerifyImageFromBuffer (FwPartitionProtocol, ImageSize, Image);
  if (EFI_ERROR (Status)) {
    *LastAttemptStatus = LAS_ERROR_BLOB_VERIFY_FAILED;
    return EFI_ABORTED;
  }

  *LastAttemptStatus = LAST_ATTEMPT_STATUS_SUCCESS;
  DEBUG ((DEBUG_ERROR, "\n%a: exit success\n", __FUNCTION__));

  return EFI_SUCCESS;
}

/**
  Get system firmware version info.

  @retval EFI_SUCCESS               No errors found.
  @retval Others                    Error detected.

**/
STATIC
EFI_STATUS
EFIAPI
FmpBlobGetVersionInfo (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       VersionStrLen;
  UINT64      Version64;
  CHAR16      *VersionStr;

  VersionStrLen  = StrSize ((CHAR16 *)PcdGetPtr (PcdUefiVersionString));
  mVersionString = (CHAR16 *)AllocateRuntimePool (VersionStrLen);
  if (mVersionString == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: string alloc failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  StrCpyS (mVersionString, VersionStrLen / sizeof (CHAR16), (CHAR16 *)PcdGetPtr (PcdUefiVersionString));

  VersionStr = (CHAR16 *)PcdGetPtr (PcdUefiHexVersionNumber);
  if (VersionStr == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Version data doesn't exist\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  if (VersionStr[0] == 0x0) {
    Version64 = 0;
  } else {
    Status = StrHexToUint64S (VersionStr, NULL, &Version64);
    if (EFI_ERROR (Status) || (Version64 > MAX_UINT32)) {
      DEBUG ((DEBUG_ERROR, "%a: Version data invalid\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    }
  }

  mVersion       = Version64;
  mVersionStatus = EFI_SUCCESS;

  DEBUG ((
    DEBUG_ERROR,
    "%a: got version=0x%x (%s)\n",
    __FUNCTION__,
    mVersion,
    mVersionString
    ));

  return EFI_SUCCESS;
}

/**
  Handle EndOfDxe event - send BootComplete and install FMP protocol.

  @param[in]  Event         Event pointer.
  @param[in]  Context       Event notification context.

  @retval None

**/
STATIC
VOID
EFIAPI
FmpBlobEndOfDxeNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;

  Status = FmpBlobGetVersionInfo ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FMP blob get version information %r\n", __FUNCTION__, Status));
  }

  if (mInstaller == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: installer not registered!\n", __FUNCTION__));
    return;
  }

  mInitialized = TRUE;
  Status       = mInstaller (mImageHandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FMP installer failed: %r\n", __FUNCTION__, Status));
    mInitialized = FALSE;
  }

  return;
}

/**
  FmpBlobLib constructor.

  @param[in]  ImageHandle       Image handle
  @param[in]  SystemTable       Pointer to system table

  @retval EFI_SUCCESS           Initialization successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FmpBlobLibConstructor (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                          Status;
  CONST TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;
  VOID                                *Hob;

  mImageHandle = ImageHandle;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
    mActiveBootChain     = PlatformResourceInfo->ActiveBootChain;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Error getting active boot chain\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  FmpParamLibInit ();

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  FmpBlobEndOfDxeNotify,
                  NULL,
                  &gEfiEndOfDxeEventGroupGuid,
                  &mEndOfDxeEvent
                  );

Done:
  // must exit with good status, API disabled if errors occurred above
  if (EFI_ERROR (Status)) {
    if (mEndOfDxeEvent != NULL) {
      gBS->CloseEvent (mEndOfDxeEvent);
      mEndOfDxeEvent = NULL;
    }

    mImageHandle     = NULL;
    mActiveBootChain = MAX_UINT32;
  }

  return EFI_SUCCESS;
}
