/** @file

  Tegra Firmware Management Protocol support

  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#include <LastAttemptStatus.h>
#include <Guid/SystemResourceTable.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/DebugLib.h>
#include <Library/DisplayUpdateProgressLib.h>
#include <Library/FwImageLib.h>
#include <Library/FwPackageLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/EFuse.h>
#include <Protocol/FirmwareManagementProgress.h>
#include <Protocol/FwImageProtocol.h>
#include <Protocol/BrBctUpdateProtocol.h>
#include "TegraFmp.h"

#define FMP_CAPSULE_SINGLE_PARTITION_VARIABLE_NAME  L"FmpCapsuleSinglePartitionName"

#define FMP_DATA_BUFFER_SIZE            (4 * 1024)
#define FMP_WRITE_LOOP_SIZE             (64 * 1024)

// progress percentages (total=100)
#define FMP_PROGRESS_CHECK_IMAGE        5
#define FMP_PROGRESS_WRITE_IMAGES       (90 - FMP_PROGRESS_VERIFY_IMAGES)
#define FMP_PROGRESS_VERIFY_IMAGES      ((mPcdFmpWriteVerifyImage) ? 30 : 0)
#define FMP_PROGRESS_SETUP_REBOOT       5

// last attempt status error codes
enum {
  LAS_ERROR_BAD_IMAGE_POINTER = LAST_ATTEMPT_STATUS_DEVICE_LIBRARY_MIN_ERROR_CODE_VALUE,
  LAS_ERROR_INVALID_PACKAGE_HEADER,
  LAS_ERROR_UNSUPPORTED_PACKAGE_TYPE,
  LAS_ERROR_INVALID_PACKAGE_IMAGE_INFO_ARRAY,
  LAS_ERROR_IMAGE_TOO_BIG,
  LAS_ERROR_PACKAGE_SIZE_ERROR,
  LAS_ERROR_NOT_UPDATABLE,
  LAS_ERROR_IMAGE_NOT_IN_PACKAGE,
  LAS_ERROR_MB1_INVALIDATE_ERROR,
  LAS_ERROR_SINGLE_IMAGE_NOT_SUPPORTED,
  LAS_ERROR_IMAGE_INDEX_MISSING,
  LAS_ERROR_NO_PROTOCOL_FOR_IMAGE,
  LAS_ERROR_IMAGE_ATTRIBUTES_ERROR,
  LAS_ERROR_BCT_UPDATE_FAILED,
  LAS_ERROR_WRITE_IMAGES_FAILED,
  LAS_ERROR_MB1_WRITE_ERROR,
  LAS_ERROR_VERIFY_IMAGES_FAILED,
  LAS_ERROR_SET_SINGLE_IMAGE_FAILED,
  LAS_ERROR_SETUP_REBOOT_FAILED,
  LAS_ERROR_FMP_LIB_UNINITIALIZED,
  LAS_ERROR_TN_SPEC_MISMATCH,
};

// special images that are not processed in the main loop
STATIC CONST CHAR16 *SpecialImageNames[] = {
  L"BCT",
  L"mb1",
  NULL
};

// progress tracking variables
STATIC UINTN        mTotalBytesToFlash      = 0;
STATIC UINTN        mTotalBytesFlashed      = 0;
STATIC UINTN        mTotalBytesToVerify     = 0;
STATIC UINTN        mTotalBytesVerified     = 0;
STATIC UINTN        mCurrentCompletion      = 0;

// module variables
STATIC EFI_EVENT        mAddressChangeEvent         = NULL;
STATIC EFI_EVENT        mExitBootServicesEvent      = NULL;
STATIC BOOLEAN          mPcdFmpWriteVerifyImage     = FALSE;
STATIC BOOLEAN          mPcdFmpSingleImageUpdate    = FALSE;
STATIC VOID             *mFmpDataBuffer             = NULL;
STATIC UINTN            mFmpDataBufferSize          = 0;
STATIC BOOLEAN          mFmpLibInitialized          = FALSE;
STATIC BOOLEAN          mIsProductionFused          = FALSE;
STATIC UINT32           mActiveBootChain            = MAX_UINT32;
STATIC UINT32           mTegraVersion               = 0;
STATIC CHAR16           *mTegraVersionString        = NULL;

STATIC NVIDIA_BR_BCT_UPDATE_PROTOCOL    *mBrBctUpdateProtocol   = NULL;
STATIC EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS mProgress  = NULL;


/**
  Get system fuse settings.

  @retval EFI_SUCCESS                   Operation completed successfully
  @retval Others                        An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
GetFuseSettings (
  VOID
  )
{
  mIsProductionFused = FALSE;

  return EFI_SUCCESS;
}

/**
  Get version info.

  @retval EFI_SUCCESS                   Operation completed successfully
  @retval Others                        An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
GetVersionInfo (
  VOID
  )
{

  mTegraVersionString   = (CHAR16 *) PcdGetPtr (PcdFirmwareVersionString);
  mTegraVersion         = PcdGet32 (PcdFmpTegraVersion);

  return EFI_SUCCESS;
}

/**
  Increment image verify bytes complete and update FW update progress bar.

  @param[in]  Bytes                     Byte count to add to verify progress

  @retval None

**/
STATIC
VOID
EFIAPI
ImageVerifyProgress (
  IN  UINTN Bytes
  )
{
  UINTN VerifyCompletion;

  mTotalBytesVerified += Bytes;
  VerifyCompletion = (mTotalBytesVerified * FMP_PROGRESS_VERIFY_IMAGES) /
    mTotalBytesToVerify;

  mProgress (mCurrentCompletion + VerifyCompletion);
}

/**
  Increment image write bytes complete and update FW update progress bar.

  @param[in]  Bytes                     Byte count to add to write progress

  @retval None

**/
STATIC
VOID
EFIAPI
ImageWriteProgress (
  IN  UINTN Bytes
  )
{
  UINTN WriteCompletion;

  mTotalBytesFlashed += Bytes;
  WriteCompletion = (mTotalBytesFlashed * FMP_PROGRESS_WRITE_IMAGES) /
    mTotalBytesToFlash;

  mProgress (mCurrentCompletion + WriteCompletion);
}

/**
  Increment SetImage progress percentage and update FW update progress bar.

  @param[in]  CompletionIncrement   Increment to add to current completion

  @retval None

**/
STATIC
VOID
EFIAPI
SetImageProgress (
  IN  UINTN     CompletionIncrement
  )
{
  mCurrentCompletion += CompletionIncrement;
  ASSERT (mCurrentCompletion <= 100);

  mProgress (mCurrentCompletion);
}

/**
  Update FW update progress bar to new completion percentage.

  @param[in]  Completion                Current percentage complete (0-100)

  @retval EFI_SUCCESS                   Progress updated successfully
  @retval Others                        An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
UpdateProgress (
  IN  UINTN     Completion
  )
{
  EFI_STATUS EFIAPI UpdateImageProgress (
    IN  UINTN   Completion
    );

  return UpdateImageProgress (Completion);
}

/**
  Check if given Name is in List.

  @param[in]  Name                      Name to look for
  @param[in]  List                      Null-terminated list to search

  @retval BOOLEAN                       TRUE if Name is in List

**/
STATIC
BOOLEAN
EFIAPI
NameIsInList (
  CONST CHAR16                  *Name,
  CONST CHAR16                  **List
  )
{
  while (*List != NULL) {
    if (StrCmp (Name, *List) == 0) {
        return TRUE;
    }

    List++;
  }

  return FALSE;
}

/**
  Check if given ImageName is a special image name.

  @param[in]  ImageName             ImageName to check

  @retval BOOLEAN                   TRUE if ImageName is a special image name

**/
STATIC
BOOLEAN
EFIAPI
IsSpecialImageName (
  CONST CHAR16                  *ImageName
  )
{
  return NameIsInList (ImageName, SpecialImageNames);
}

/**
  Perform setup for reboot after FW update.  A variable is set which is
  examined after the reboot to determine if the FW update succeeded and
  the active FW set should be updated to the new FW during ExitBootServices().

  @retval EFI_SUCCESS               The operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FmpSetupReboot (
  VOID
  )
{
  return EFI_SUCCESS;
}

/**
  Write a buffer to a FwImage.

  @param[in]  FwImageProtocol       FwImage protocol structure pointer
  @param[in]  Bytes                 Number of bytes to write
  @param[in]  DataBuffer            Pointer to data to write
  @param[in]  Flags                 FwImage flags for the write.  See
                                    NVIDIA_FW_IMAGE_PROTOCOL.Write()

  @retval EFI_SUCCESS               The operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
WriteImageFromBuffer (
  IN  NVIDIA_FW_IMAGE_PROTOCOL      *FwImageProtocol,
  IN  UINTN                         Bytes,
  IN  CONST UINT8                   *DataBuffer,
  IN  UINTN                         Flags
  )
{
  EFI_STATUS                        Status;
  UINTN                             WriteOffset;
  UINTN                             BytesPerLoop;

  DEBUG ((DEBUG_VERBOSE, "Writing %s, bytes=%u\n",
          FwImageProtocol->ImageName, Bytes));

  BytesPerLoop  = FMP_WRITE_LOOP_SIZE;
  WriteOffset   = 0;
  while (Bytes > 0) {
    UINTN   WriteSize;

    WriteSize = (Bytes > BytesPerLoop) ? BytesPerLoop : Bytes;
    Status = FwImageProtocol->Write (FwImageProtocol,
                                     WriteOffset,
                                     WriteSize,
                                     DataBuffer + WriteOffset,
                                     Flags);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    WriteOffset += WriteSize;
    Bytes       -= WriteSize;
    ImageWriteProgress (WriteSize);
  }

  return Status;
}

/**
  Write FW package data to a FwImage.

  @param[in]  Header                Pointer to the FW package header
  @param[in]  Name                  Name of the FwImage to write
  @param[in]  Flags                 FwImage flags for the write.  See
                                    NVIDIA_FW_IMAGE_PROTOCOL.Write()

  @retval EFI_SUCCESS               The operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
WriteImage (
  IN  CONST FW_PACKAGE_HEADER       *Header,
  IN  CONST CHAR16                  *Name,
  IN  UINTN                         Flags
  )
{
  CONST FW_PACKAGE_IMAGE_INFO       *PkgImageInfo;
  EFI_STATUS                        Status;
  UINTN                             ImageIndex;
  CONST UINT8                       *DataBuffer;
  NVIDIA_FW_IMAGE_PROTOCOL          *FwImageProtocol;

  FwImageProtocol = FwImageFindProtocol (Name);
  if (FwImageProtocol == NULL) {
    DEBUG ((DEBUG_INFO, "%a: couldn't find image protocol for %s\n",
            __FUNCTION__, Name));
    return EFI_NOT_FOUND;
  }

  Status = FwPackageGetImageIndex (Header, Name, mIsProductionFused, &ImageIndex);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to find image=%s: %r\n", Name, Status));
    return Status;
  }

  PkgImageInfo = FwPackageImageInfoPtr (Header, ImageIndex);
  DataBuffer = (CONST UINT8 *) FwPackageImageDataPtr (Header, ImageIndex);

  Status = WriteImageFromBuffer (FwImageProtocol,
                                 PkgImageInfo->Bytes,
                                 DataBuffer,
                                 Flags);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to write image=%s: %r\n", Name, Status));
  }

  return Status;
}

/**
  Write FW package data to all FwImages except for special images.

  @param[in]  Header                Pointer to the FW package header

  @retval EFI_SUCCESS               The operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
WriteRegularImages (
  IN  CONST FW_PACKAGE_HEADER       *Header
  )
{
  EFI_STATUS                        Status;
  UINTN                             Index;
  UINTN                             PkgImageIndex;
  UINTN                             ImageCount;
  NVIDIA_FW_IMAGE_PROTOCOL          **FwImageProtocolArray;

  ImageCount            = FwImageGetCount ();
  FwImageProtocolArray  = FwImageGetProtocolArray ();

  // Write all images except special ones that are done later
  for (Index = 0; Index < ImageCount; Index++) {
    CONST CHAR16                *ImageName;
    NVIDIA_FW_IMAGE_PROTOCOL    *FwImageProtocol;

    FwImageProtocol = FwImageProtocolArray[Index];
    ImageName = FwImageProtocol->ImageName;
    if (IsSpecialImageName (ImageName)) {
      continue;
    }

    Status = FwPackageGetImageIndex (Header,
                                     ImageName,
                                     mIsProductionFused,
                                     &PkgImageIndex);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%s not found in package: %r\n", ImageName, Status));
      return Status;
    }

    Status = WriteImage (Header,
                         ImageName,
                         FW_IMAGE_RW_FLAG_NONE);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Verify that a FwImage matches its FW package data.  If PcdFmpWriteVerifyImage
  is FALSE, no verification is done and EFI_SUCCESS is returned.

  @param[in]  Header                Pointer to the FW package header
  @param[in]  Name                  Name of the FwImage to verify
  @param[in]  Flags                 FwImage flags for the read.  See
                                    NVIDIA_FW_IMAGE_PROTOCOL.Read()

  @retval EFI_SUCCESS               The operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
VerifyImage (
  IN  CONST FW_PACKAGE_HEADER       *Header,
  IN  CONST CHAR16                  *Name,
  IN  UINTN                         Flags
  )
{
  NVIDIA_FW_IMAGE_PROTOCOL          *FwImageProtocol;
  CONST UINT8                       *DataBuffer;
  UINTN                             Bytes;
  UINTN                             VerifyOffset;
  EFI_STATUS                        Status;
  CONST FW_PACKAGE_IMAGE_INFO       *PkgImageInfo;
  UINTN                             ImageIndex;
  FW_IMAGE_ATTRIBUTES               ImageAttributes;

  if (!mPcdFmpWriteVerifyImage) {
    return EFI_SUCCESS;
  }

  FwImageProtocol = FwImageFindProtocol (Name);
  if (FwImageProtocol == NULL) {
    DEBUG ((DEBUG_INFO, "%a: couldn't find image protocol for %s\n",
            __FUNCTION__, Name));
    return EFI_NOT_FOUND;
  }

  Status = FwImageProtocol->GetAttributes (FwImageProtocol, &ImageAttributes);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get image=%s attributes: %r\n",
            Name, Status));
    return Status;
  }

  Status = FwPackageGetImageIndex (Header, Name, mIsProductionFused, &ImageIndex);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to find image=%s: %r\n", Name, Status));
    return Status;
  }

  PkgImageInfo = FwPackageImageInfoPtr (Header, ImageIndex);
  DataBuffer = (CONST UINT8 *) FwPackageImageDataPtr (Header, ImageIndex);

  DEBUG ((DEBUG_VERBOSE, "Verifying %s: PkgOffset=%d, Bytes=%d\n",
          Name, PkgImageInfo->Offset, PkgImageInfo->Bytes));

  VerifyOffset = 0;
  Bytes = PkgImageInfo->Bytes;
  while (Bytes > 0) {
    UINTN   VerifySize;
    UINTN   VerifyBufferSize;

    VerifySize = (Bytes > mFmpDataBufferSize) ? mFmpDataBufferSize : Bytes;
    VerifyBufferSize = ALIGN_VALUE (VerifySize, ImageAttributes.BlockSize);
    ASSERT (VerifyBufferSize <= mFmpDataBufferSize);

    Status = FwImageProtocol->Read (FwImageProtocol,
                                    VerifyOffset,
                                    VerifyBufferSize,
                                    mFmpDataBuffer,
                                    Flags);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to read image=%s: %r\n", Name, Status));
      return Status;
    }

    if (CompareMem (mFmpDataBuffer, DataBuffer + VerifyOffset, VerifySize) != 0) {
      DEBUG ((DEBUG_ERROR, "Image=%s failed verify near offset=%u\n",
              Name, VerifyOffset));
      return EFI_VOLUME_CORRUPTED;
    }

    VerifyOffset    += VerifySize;
    Bytes           -= VerifySize;
    ImageVerifyProgress (VerifySize);
  }

  return Status;
}

/**
  Verify that all FwImages (except BCT) match their FW package data. If
  PcdFmpWriteVerifyImage is FALSE, no verification is done and EFI_SUCCESS
  is returned.

  @param[in]  Header                Pointer to the FW package header

  @retval EFI_SUCCESS               The operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
VerifyAllImages (
  IN  CONST FW_PACKAGE_HEADER       *Header
  )
{
  EFI_STATUS                        Status;
  UINTN                             Index;
  UINTN                             PkgImageIndex;
  UINTN                             ImageCount;
  NVIDIA_FW_IMAGE_PROTOCOL          **FwImageProtocolArray;

  if (!mPcdFmpWriteVerifyImage) {
    return EFI_SUCCESS;
  }

  ImageCount            = FwImageGetCount ();
  FwImageProtocolArray  = FwImageGetProtocolArray ();

  for (Index = 0; Index < ImageCount; Index++) {
    CONST CHAR16                *ImageName;
    NVIDIA_FW_IMAGE_PROTOCOL    *FwImageProtocol;

    FwImageProtocol = FwImageProtocolArray[Index];
    ImageName = FwImageProtocol->ImageName;
    if (StrCmp (ImageName, L"BCT") == 0) {
      continue;
    }

    Status = FwPackageGetImageIndex (Header,
                                     ImageName,
                                     mIsProductionFused,
                                     &PkgImageIndex);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%s not found in package: %r\n", ImageName, Status));
      return Status;
    }

    Status = VerifyImage (Header,
                          ImageName,
                          FW_IMAGE_RW_FLAG_READ_INACTIVE_IMAGE);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Invalidate the contents of an FwImage by writing the first
  FMP_DATA_BUFFER_SIZE bytes to 0xff.

  @param[in]  Name                  Name of the FwImage to verify
  @param[in]  Flags                 FwImage flags for the write.  See
                                    NVIDIA_FW_IMAGE_PROTOCOL.Write()

  @retval EFI_SUCCESS               The operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
InvalidateImage (
  IN  CONST CHAR16                  *Name,
  IN  UINTN                         Flags
  )
{
  NVIDIA_FW_IMAGE_PROTOCOL          *FwImageProtocol;
  FW_IMAGE_ATTRIBUTES               Attributes;
  EFI_STATUS                        Status;
  UINTN                             Bytes;

  FwImageProtocol = FwImageFindProtocol (Name);
  if (FwImageProtocol == NULL) {
    DEBUG ((DEBUG_INFO, "%a: couldn't find image protocol for %s\n",
            __FUNCTION__, Name));
    return EFI_NOT_FOUND;
  }

  Status = FwImageProtocol->GetAttributes (FwImageProtocol, &Attributes);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Bytes = MIN (Attributes.Bytes, mFmpDataBufferSize);
  SetMem (mFmpDataBuffer, Bytes, 0xff);

  mTotalBytesToFlash += Bytes;
  return WriteImageFromBuffer (FwImageProtocol,
                               Bytes,
                               (UINT8 *) mFmpDataBuffer,
                               Flags);
}

/**
  Update a single FwImage from a special single-image FW package/capsule.
  This is a development feature enabled by PcdFmpSingleImageUpdate and
  requires that the FMP_CAPSULE_SINGLE_PARTITION_VARIABLE_NAME variable
  be set to the partition name to be written.

  @param[in]  Header                Pointer to the single-image FW package header

  @retval EFI_SUCCESS               The operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FmpTegraSetSingleImage (
  IN  CONST FW_PACKAGE_HEADER       *Header
  )
{
  EFI_STATUS                    Status;
  CONST FW_PACKAGE_IMAGE_INFO   *PkgImageInfo;
  CHAR16                        PkgName[FW_IMAGE_NAME_LENGTH];
  CHAR16                        PartitionName[MAX_PARTITION_NAME_LEN];
  CHAR16                        BaseName[MAX_PARTITION_NAME_LEN];
  UINTN                         BootChain;
  UINTN                         WriteFlag;
  UINTN                         VariableSize;

  VariableSize = (MAX_PARTITION_NAME_LEN - 1) * sizeof (CHAR16);
  Status = gRT->GetVariable (FMP_CAPSULE_SINGLE_PARTITION_VARIABLE_NAME,
                             &gNVIDIATokenSpaceGuid,
                             NULL,
                             &VariableSize,
                             PartitionName);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting single partition name: %r\n",
            __FUNCTION__, Status));
    return Status;
  }
  PartitionName[VariableSize / sizeof (CHAR16)] = L'\0';

  Status = GetPartitionBaseNameAndBootChain (PartitionName, BaseName, &BootChain);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting base name and boot chain for %s: %r\n",
            __FUNCTION__, PartitionName, Status));
    return Status;
  }

  // Get capsule package image name and ensure match with variable
  PkgImageInfo = FwPackageImageInfoPtr (Header, 0);
  FwPackageCopyImageName (PkgName, PkgImageInfo, FW_IMAGE_NAME_LENGTH);
  if (StrCmp (BaseName, PkgName) != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Name mismatch package=%s, variable=%s\n",
            __FUNCTION__, PkgName, BaseName));
    return EFI_NOT_FOUND;
  }

  if (IsSpecialImageName (PkgName)) {
    DEBUG ((DEBUG_ERROR, "%a: %s single image not supported\n",
            __FUNCTION__, PkgName));
    return EFI_UNSUPPORTED;
  }

  // Determine A/B write flag
  switch (BootChain) {
    case BOOT_CHAIN_A:
      WriteFlag = FW_IMAGE_RW_FLAG_FORCE_PARTITION_A;
      break;
    case BOOT_CHAIN_B:
      WriteFlag = FW_IMAGE_RW_FLAG_FORCE_PARTITION_B;
      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a: Invalid Boot Chain=%u\n",
              __FUNCTION__, BootChain));
      return EFI_UNSUPPORTED;
  }

  DEBUG ((DEBUG_INFO, "%a: handling single image=%s\n",
          __FUNCTION__, PkgName));

  // write and verify single image
  Status = WriteImage (Header, PkgName, WriteFlag);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = VerifyImage (Header, PkgName, WriteFlag);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Update the inactive BCT slots with FW package data.

  @param[in]  Header                Pointer to the FW package header

  @retval EFI_SUCCESS               The operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
UpdateBct (
  IN  CONST FW_PACKAGE_HEADER   *Header
  )
{
  UINTN                         PkgImageIndex;
  UINTN                         Bytes;
  CONST VOID                    *ImageData;
  EFI_STATUS                    Status;

  Status = FwPackageGetImageIndex (Header,
                                   L"BCT",
                                   mIsProductionFused,
                                   &PkgImageIndex);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Bytes = FwPackageImageInfoPtr (Header, PkgImageIndex)->Bytes;
  ImageData = FwPackageImageDataPtr (Header, PkgImageIndex);

  Status = mBrBctUpdateProtocol->UpdateBct (mBrBctUpdateProtocol,
                                            Bytes,
                                            ImageData);
  ImageWriteProgress (Bytes);

  return Status;
}

/**
  Handle ExitBootServices() notification to update BCT if the reboot after
  a FW update successfully booted the new FW.

  @param[in]  Event         Event being handled
  @param[in]  Context       Event context

  @retval None

**/
STATIC
VOID
EFIAPI
FmpTegraExitBootServicesNotify (
  IN EFI_EVENT                            Event,
  IN VOID                                 *Context
  )
{
  DEBUG ((DEBUG_INFO, "%a: ExitBootServices\n", __FUNCTION__));
}

STATIC
UINTN
EFIAPI
FmpTegraGetTotalBytesToFlash (
  IN  CONST FW_PACKAGE_HEADER   *Header
  )
{
  UINTN                         Index;
  UINTN                         TotalBytesToFlash;
  CONST FW_PACKAGE_IMAGE_INFO   *ImageInfo;

  TotalBytesToFlash = 0;
  for (Index = 0; Index < Header->ImageCount; Index++) {
    ImageInfo = FwPackageImageInfoPtr (Header, Index);
    if (!FwPackageUpdateModeIsOk (ImageInfo, mIsProductionFused)) {
      continue;
    }

    TotalBytesToFlash += ImageInfo->Bytes;
  }

  return TotalBytesToFlash;
}

EFI_STATUS
EFIAPI
FmpTegraGetVersion (
  OUT UINT32        *Version,
  OUT CHAR16        **VersionString
  )
{
  if (Version != NULL) {
    *Version = mTegraVersion;
  }

  if (VersionString != NULL) {
    UINTN   VersionStringSize;

    // version string must be in allocated pool memory
    VersionStringSize = StrSize (mTegraVersionString) * sizeof (CHAR16);
    *VersionString = (CHAR16 *) AllocateRuntimeCopyPool (VersionStringSize,
                                                         mTegraVersionString);
    if (*VersionString == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  DEBUG ((DEBUG_INFO, "%a: Version=0x%x, Str=%s\n",
          __FUNCTION__, mTegraVersion, mTegraVersionString));

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FmpTegraCheckImage (
  IN  CONST VOID  *Image,
  IN  UINTN       ImageSize,
  OUT UINT32      *ImageUpdatable,
  OUT UINT32      *LastAttemptStatus
  )
{
  CONST FW_PACKAGE_HEADER       *Header;
  EFI_STATUS                    Status;
  UINTN                         Index;
  UINTN                         ImageCount;
  CHAR16                        SingleImageNameBuffer[FW_IMAGE_NAME_LENGTH];
  CONST CHAR16                  *SingleImageName;
  NVIDIA_FW_IMAGE_PROTOCOL      **FwImageProtocolArray;
  CONST FW_PACKAGE_IMAGE_INFO   *PkgImageInfo;

  DEBUG ((DEBUG_INFO, "%a: Image=0x%p ImageSize=%u\n",
          __FUNCTION__, Image, ImageSize));

  if ((ImageUpdatable == NULL) || (LastAttemptStatus == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  if (Image == NULL) {
    *ImageUpdatable = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_BAD_IMAGE_POINTER;
    return EFI_INVALID_PARAMETER;
  }
  if (!mFmpLibInitialized) {
    *ImageUpdatable = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_FMP_LIB_UNINITIALIZED;
    return EFI_NOT_READY;
  }

  Header = (CONST FW_PACKAGE_HEADER *) Image;

  Status = FwPackageValidateHeader (Header);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Update package header failed validation: %r\n",
            Status));
    *ImageUpdatable = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_INVALID_PACKAGE_HEADER;
    return EFI_ABORTED;
  }

  if (Header->Type != FW_PACKAGE_TYPE_FW) {
    DEBUG ((DEBUG_ERROR, "Package type=%u not supported!\n", Header->Type));
    *ImageUpdatable = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_UNSUPPORTED_PACKAGE_TYPE;
    return EFI_ABORTED;
  }

  Status = FwPackageValidateImageInfoArray (Header);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Update package image info array invalid: %r\n",
            Status));
    *LastAttemptStatus = LAS_ERROR_INVALID_PACKAGE_IMAGE_INFO_ARRAY;
    return Status;
  }

  ImageCount = FwImageGetCount ();

  // Handle special case of a development package with exactly one image
  if (Header->ImageCount == 1) {
    if (!mPcdFmpSingleImageUpdate) {
      DEBUG ((DEBUG_ERROR, "%a: PcdFmpSingleImageUpdateEnabled not set\n",
              __FUNCTION__));
      *LastAttemptStatus = LAS_ERROR_SINGLE_IMAGE_NOT_SUPPORTED;
      return EFI_UNSUPPORTED;
    }

    PkgImageInfo = FwPackageImageInfoPtr (Header, 0);
    FwPackageCopyImageName (SingleImageNameBuffer,
                            PkgImageInfo,
                            FW_IMAGE_NAME_LENGTH);
    SingleImageName = SingleImageNameBuffer;
    ImageCount = 1;
    DEBUG ((DEBUG_INFO, "%a: handling single image=%s\n",
            __FUNCTION__, SingleImageName));
  }

  FwImageProtocolArray = FwImageGetProtocolArray ();
  for (Index = 0; Index < ImageCount; Index++) {
    CONST CHAR16                    *ImageName;
    UINTN                           PkgImageIndex;
    NVIDIA_FW_IMAGE_PROTOCOL        *FwImageProtocol;
    FW_IMAGE_ATTRIBUTES             ImageAttributes;

    FwImageProtocol = FwImageProtocolArray[Index];
    ImageName = FwImageProtocol->ImageName;

    if ((ImageCount == 1) && (StrCmp (ImageName, SingleImageName) != 0)) {
      continue;
    }

    Status = FwPackageGetImageIndex (Header,
                                     ImageName,
                                     mIsProductionFused,
                                     &PkgImageIndex);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%s not found in package: %r\n", ImageName, Status));
      *ImageUpdatable = IMAGE_UPDATABLE_INVALID;
      *LastAttemptStatus = LAS_ERROR_IMAGE_NOT_IN_PACKAGE;
      return EFI_ABORTED;
    }
    PkgImageInfo = FwPackageImageInfoPtr (Header, PkgImageIndex);

    Status = FwImageProtocol->GetAttributes (FwImageProtocol, &ImageAttributes);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Error getting attributes for image %s: %r\n",
              ImageName, Status));
      *ImageUpdatable = IMAGE_UPDATABLE_INVALID;
      *LastAttemptStatus = LAS_ERROR_IMAGE_ATTRIBUTES_ERROR;
      return EFI_ABORTED;
    }

    if (PkgImageInfo->Bytes > ImageAttributes.Bytes) {
      DEBUG ((DEBUG_ERROR, "Package image %s is bigger than partition: %u > %u\n",
              ImageName, PkgImageInfo->Bytes, ImageAttributes.Bytes));
      *ImageUpdatable = IMAGE_UPDATABLE_INVALID;
      *LastAttemptStatus = LAS_ERROR_IMAGE_TOO_BIG;
      return EFI_ABORTED;
    }

    if (((CONST UINT8 *) FwPackageImageDataPtr (Header, PkgImageIndex) +
         PkgImageInfo->Bytes) > ((CONST UINT8 *) Image + ImageSize)) {
      DEBUG ((DEBUG_ERROR, "Package image %s goes beyond end of capsule!\n",
              ImageName));
      *ImageUpdatable = IMAGE_UPDATABLE_INVALID;
      *LastAttemptStatus = LAS_ERROR_PACKAGE_SIZE_ERROR;
      return EFI_ABORTED;
    }
  }

  // Check that every image in the package has a protocol
  for (Index = 0; Index < Header->ImageCount; Index++) {
    CHAR16                      ImageName[FW_IMAGE_NAME_LENGTH];
    NVIDIA_FW_IMAGE_PROTOCOL    *FwImageProtocol;

    PkgImageInfo = FwPackageImageInfoPtr (Header, Index);
    if (PkgImageInfo == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Image %u not found in package with %u images\n",
              __FUNCTION__, Index, Header->ImageCount));
      *ImageUpdatable = IMAGE_UPDATABLE_INVALID;
      *LastAttemptStatus = LAS_ERROR_IMAGE_INDEX_MISSING;
      return EFI_ABORTED;
    }

    FwPackageCopyImageName (ImageName, PkgImageInfo, FW_IMAGE_NAME_LENGTH);
    FwImageProtocol = FwImageFindProtocol (ImageName);
    if (FwImageProtocol == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Image %u, no protocol for %s\n",
              __FUNCTION__, Index, ImageName));
      *ImageUpdatable = IMAGE_UPDATABLE_INVALID;
      *LastAttemptStatus = LAS_ERROR_NO_PROTOCOL_FOR_IMAGE;
      return EFI_ABORTED;
    }
  }

  *LastAttemptStatus = LAST_ATTEMPT_STATUS_SUCCESS;
  *ImageUpdatable = IMAGE_UPDATABLE_VALID;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FmpTegraSetImage (
  IN  CONST VOID                                     *Image,
  IN  UINTN                                          ImageSize,
  IN  CONST VOID                                     *VendorCode,       OPTIONAL
  IN  EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS  Progress,          OPTIONAL
  IN  UINT32                                         CapsuleFwVersion,
  OUT CHAR16                                         **AbortReason,
  OUT UINT32                                         *LastAttemptStatus
  )
{
  CONST FW_PACKAGE_HEADER           *Header;
  EFI_STATUS                        Status;

  DEBUG ((DEBUG_INFO, "%a: Image=0x%p, ImageSize=%d Version=0x%x\n",
          __FUNCTION__, Image, ImageSize, CapsuleFwVersion));

  if (LastAttemptStatus == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (Image == NULL) {
    *LastAttemptStatus = LAS_ERROR_BAD_IMAGE_POINTER;
    return EFI_INVALID_PARAMETER;
  }
  if (!mFmpLibInitialized) {
    *LastAttemptStatus = LAS_ERROR_FMP_LIB_UNINITIALIZED;
    return EFI_NOT_READY;
  }

  Header                    = (CONST FW_PACKAGE_HEADER *) Image;
  mTotalBytesFlashed        = 0;
  mTotalBytesVerified       = 0;
  mCurrentCompletion        = 0;

  // Ignore Progress function parameter since it is a null implementation
  // when UpdateCapsule() is the caller.  Use our UpdateProgress() instead.
  mProgress                 = UpdateProgress;

  SetImageProgress (FMP_PROGRESS_CHECK_IMAGE);

  mTotalBytesToFlash = FmpTegraGetTotalBytesToFlash (Header);
  mTotalBytesToVerify = (mPcdFmpWriteVerifyImage) ? mTotalBytesToFlash : 0;

  // Handle special case of a development capsule with exactly one image
  if (Header->ImageCount == 1) {
    Status = FmpTegraSetSingleImage (Header);
    if (EFI_ERROR (Status)) {
      *LastAttemptStatus = LAS_ERROR_SET_SINGLE_IMAGE_FAILED;
      return EFI_ABORTED;
    }
    goto Done;
  }

  // Perform full FW update sequence
  DEBUG ((DEBUG_INFO, "%a: Starting FW update sequence, images=%u, bytes=%u\n",
          __FUNCTION__, FwImageGetCount (), mTotalBytesToFlash));

  Status = UpdateBct (Header);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Update BCT failed: %r\n", Status));
    *LastAttemptStatus = LAS_ERROR_BCT_UPDATE_FAILED;
    return EFI_ABORTED;
  }

  Status = InvalidateImage (L"mb1", FW_IMAGE_RW_FLAG_NONE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Invalidate mb1 failed: %r\n", Status));
    *LastAttemptStatus = LAS_ERROR_MB1_INVALIDATE_ERROR;
    return EFI_ABORTED;
  }

  Status = WriteRegularImages (Header);
  if (EFI_ERROR (Status)) {
    *LastAttemptStatus = LAS_ERROR_WRITE_IMAGES_FAILED;
    return EFI_ABORTED;
  }

  Status = WriteImage (Header, L"mb1", FW_IMAGE_RW_FLAG_NONE);
  if (EFI_ERROR (Status)) {
    *LastAttemptStatus = LAS_ERROR_MB1_WRITE_ERROR;
    return EFI_ABORTED;
  }

  SetImageProgress (FMP_PROGRESS_WRITE_IMAGES);

  Status = VerifyAllImages (Header);
  if (EFI_ERROR (Status)) {
    *LastAttemptStatus = LAS_ERROR_VERIFY_IMAGES_FAILED;
    return EFI_ABORTED;
  }

  SetImageProgress (FMP_PROGRESS_VERIFY_IMAGES);

  Status = FmpSetupReboot ();
  if (EFI_ERROR (Status)) {
    *LastAttemptStatus = LAS_ERROR_SETUP_REBOOT_FAILED;
    return EFI_ABORTED;
  }

Done:
  SetImageProgress (FMP_PROGRESS_SETUP_REBOOT);
  *LastAttemptStatus = LAST_ATTEMPT_STATUS_SUCCESS;
  DEBUG ((DEBUG_INFO, "%a: exit success\n", __FUNCTION__));
  return EFI_SUCCESS;
}

/**
  FmpDeviceLib constructor.

  @param[in]  ImageHandle       Image handle
  @param[in]  SystemTable       Pointer to system table

  @retval EFI_SUCCESS           Initialization successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FmpDeviceLibConstructor (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS        Status;

  mPcdFmpWriteVerifyImage   = PcdGetBool (PcdFmpWriteVerifyImage);
  mPcdFmpSingleImageUpdate  = PcdGetBool (PcdFmpSingleImageUpdate);

  mFmpDataBufferSize = FMP_DATA_BUFFER_SIZE;
  mFmpDataBuffer = AllocateRuntimeZeroPool (mFmpDataBufferSize);
  if (mFmpDataBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: data buffer alloc failed\n",  __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Status = GetActiveBootChain (&mActiveBootChain);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting active boot chain: %r\n",
            __FUNCTION__, Status));
    goto Done;
  }

  Status = gBS->LocateProtocol (&gNVIDIABrBctUpdateProtocolGuid,
                                NULL,
                                (VOID **)&mBrBctUpdateProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "BrBctUpdate Protocol Guid=%g not found: %r\n",
            &gNVIDIABrBctUpdateProtocolGuid, Status));
    goto Done;
  }

  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                               TPL_NOTIFY,
                               FmpTegraExitBootServicesNotify,
                               NULL,
                               &gEfiEventExitBootServicesGuid,
                               &mExitBootServicesEvent);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error creating exit boot services event: %r\n",
            __FUNCTION__, Status));
    goto Done;
  }

  Status = GetVersionInfo ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting version info: %r\n",
            __FUNCTION__, Status));
    goto Done;
  }

  Status = GetFuseSettings ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting fuse settings: %r\n",
            __FUNCTION__, Status));
    goto Done;
  }

  mFmpLibInitialized = TRUE;

Done:
  if (EFI_ERROR (Status)) {
    if (mFmpDataBuffer != NULL) {
      FreePool (mFmpDataBuffer);
      mFmpDataBuffer = NULL;
    }
    if (mExitBootServicesEvent != NULL) {
      gBS->CloseEvent (mExitBootServicesEvent);
      mExitBootServicesEvent = NULL;
    }
    if (mAddressChangeEvent != NULL) {
      gBS->CloseEvent (mAddressChangeEvent);
      mAddressChangeEvent = NULL;
    }
    mFmpDataBufferSize      = 0;
    mBrBctUpdateProtocol    = NULL;
    mActiveBootChain        = MAX_UINT32;
  }

  // mFmpLibInitialized flag inibits API if there was an error
  return EFI_SUCCESS;
}
