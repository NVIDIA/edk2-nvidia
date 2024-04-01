/** @file

  Tegra Firmware Management Protocol support

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <LastAttemptStatus.h>
#include <Guid/SystemResourceTable.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootChainInfoLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/DisplayUpdateProgressLib.h>
#include <Library/FwImageLib.h>
#include <Library/FwPackageLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/VerPartitionLib.h>
#include <Protocol/EFuse.h>
#include <Protocol/FirmwareManagementProgress.h>
#include <Protocol/FwImageProtocol.h>
#include <Protocol/FwPartitionProtocol.h>
#include <Protocol/BrBctUpdateProtocol.h>
#include <Protocol/BootChainProtocol.h>
#include "TegraFmp.h"

#define FMP_CAPSULE_SINGLE_PARTITION_CHAIN_VARIABLE  L"FmpCapsuleSinglePartitionChain"
#define FMP_PLATFORM_SPEC_VARIABLE_NAME              L"TegraPlatformSpec"
#define FMP_PLATFORM_COMPAT_SPEC_VARIABLE_NAME       L"TegraPlatformCompatSpec"
#define FMP_PLATFORM_SPEC_DEFAULT                    "-------"

#define FMP_DATA_BUFFER_SIZE  (4 * 1024)
#define FMP_WRITE_LOOP_SIZE   (32 * 1024)

// progress percentages (total=100)
#define FMP_PROGRESS_CHECK_IMAGE    5
#define FMP_PROGRESS_WRITE_IMAGES   (90 - FMP_PROGRESS_VERIFY_IMAGES)
#define FMP_PROGRESS_VERIFY_IMAGES  ((mPcdFmpWriteVerifyImage) ? 30 : 0)
#define FMP_PROGRESS_UPDATE_BCT     5

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
  LAS_ERROR_IMAGE_INDEX_MISSING,                        // deprecated
  LAS_ERROR_NO_PROTOCOL_FOR_IMAGE,
  LAS_ERROR_IMAGE_ATTRIBUTES_ERROR,
  LAS_ERROR_BCT_UPDATE_FAILED,
  LAS_ERROR_WRITE_IMAGES_FAILED,
  LAS_ERROR_MB1_WRITE_ERROR,
  LAS_ERROR_VERIFY_IMAGES_FAILED,
  LAS_ERROR_SET_SINGLE_IMAGE_FAILED,
  LAS_ERROR_FMP_LIB_UNINITIALIZED,
  LAS_ERROR_BOOT_CHAIN_UPDATE_CANCELED,
  LAS_ERROR_GPT_METADATA_UPDATE_FAILED,
  LAS_ERROR_GPT_VERIFY_FAILED,
  LAS_ERROR_GPT_INVALIDATE_FAILED,
  LAS_ERROR_GPT_WRITE_FAILED,
};

// special images that are not processed in the main loop
STATIC CONST CHAR16  *SpecialImageNames[] = {
  L"GPT",
  L"mb1",
  FW_PARTITION_UPDATE_INACTIVE_PARTITIONS,
  NULL
};

// partitions not verified in main loop
STATIC CONST CHAR16  *NoVerifyPartitionNames[] = {
  L"BCT",
  FW_PARTITION_UPDATE_INACTIVE_PARTITIONS,
  NULL
};

// progress tracking variables
STATIC UINTN  mTotalBytesToFlash  = 0;
STATIC UINTN  mTotalBytesFlashed  = 0;
STATIC UINTN  mTotalBytesToVerify = 0;
STATIC UINTN  mTotalBytesVerified = 0;
STATIC UINTN  mCurrentCompletion  = 0;

// module variables
STATIC EFI_EVENT     mAddressChangeEvent      = NULL;
STATIC BOOLEAN       mPcdFmpWriteVerifyImage  = FALSE;
STATIC BOOLEAN       mPcdFmpSingleImageUpdate = FALSE;
STATIC VOID          *mFmpDataBuffer          = NULL;
STATIC UINTN         mFmpDataBufferSize       = 0;
STATIC BOOLEAN       mFmpLibInitialized       = FALSE;
STATIC BOOLEAN       mFmpLibInitializeFailed  = FALSE;
STATIC CHAR8         *mPlatformCompatSpec     = NULL;
STATIC CHAR8         *mPlatformSpec           = NULL;
STATIC BOOLEAN       mIsProductionFused       = FALSE;
STATIC UINT32        mActiveBootChain         = MAX_UINT32;
STATIC UINT32        mTegraVersion            = 0;
STATIC CHAR16        *mTegraVersionString     = NULL;
STATIC EFI_STATUS    mTegraVersionStatus      = EFI_UNSUPPORTED;
STATIC CONST CHAR16  **mFwImagesRequired      = NULL;
STATIC UINTN         mFwImagesRequiredCount   = 0;

STATIC NVIDIA_BOOT_CHAIN_PROTOCOL                     *mBootChainProtocol   = NULL;
STATIC NVIDIA_BR_BCT_UPDATE_PROTOCOL                  *mBrBctUpdateProtocol = NULL;
STATIC EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS  mProgress             = NULL;
STATIC EFI_HANDLE                                     mImageHandle          = NULL;
FMP_DEVICE_LIB_REGISTER_FMP_INSTALLER                 mInstaller            = NULL;

CONST CHAR16 **
EFIAPI
FwImageGetRequiredList (
  IN  UINTN  ChipId,
  OUT UINTN  *ImageCount
  );

/**
  Get production fuse setting from 5th field in TnSpec.  The field must contain
  exactly -0- for non-production fused, otherwise it is treated as
  production fused.

  TnSpec fields:
  ${BOARDID}-${FAB}-${BOARDSKU}-${BOARDREV}-${fuselevel_s}-${hwchiprev}-${ext_target_board}-

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
  UINTN       Dash;
  CHAR8       *Ptr;
  EFI_STATUS  Status;
  UINTN       Size;

  mIsProductionFused = TRUE;

  Size   = 0;
  Status = gRT->GetVariable (
                  FMP_PLATFORM_SPEC_VARIABLE_NAME,
                  &gNVIDIAPublicVariableGuid,
                  NULL,
                  &Size,
                  NULL
                  );
  if (Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting %s size: %r\n",
      __FUNCTION__,
      FMP_PLATFORM_SPEC_VARIABLE_NAME,
      Status
      ));
    return EFI_SUCCESS;
  }

  // ensure null termination
  mPlatformSpec = (CHAR8 *)AllocateRuntimeZeroPool (Size + 1);
  if (mPlatformSpec == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Spec alloc failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gRT->GetVariable (
                  FMP_PLATFORM_SPEC_VARIABLE_NAME,
                  &gNVIDIAPublicVariableGuid,
                  NULL,
                  &Size,
                  mPlatformSpec
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting %s: %r\n",
      __FUNCTION__,
      FMP_PLATFORM_SPEC_VARIABLE_NAME,
      Status
      ));
    FreePool (mPlatformSpec);
    mPlatformSpec = NULL;
    return Status;
  }

  Ptr = mPlatformSpec;
  for (Dash = 0; Dash < 4; Dash++) {
    while (TRUE) {
      if (*Ptr == '\0') {
        break;
      }

      if (*Ptr == '-') {
        Ptr++;
        break;
      }

      Ptr++;
    }
  }

  if ((*Ptr == '0') && (*(Ptr + 1) == '-')) {
    mIsProductionFused = FALSE;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: fuse=%u, offset=%u\n",
    __FUNCTION__,
    mIsProductionFused,
    Ptr - mPlatformSpec
    ));

  return EFI_SUCCESS;
}

/**
  Get the system's TnSpec string.

  @retval EFI_SUCCESS                   Operation completed successfully
  @retval Others                        An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
GetTnSpec (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       Size;

  Size   = 0;
  Status = gRT->GetVariable (
                  FMP_PLATFORM_COMPAT_SPEC_VARIABLE_NAME,
                  &gNVIDIAPublicVariableGuid,
                  NULL,
                  &Size,
                  NULL
                  );
  if (Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting %s size: %r\n",
      __FUNCTION__,
      FMP_PLATFORM_COMPAT_SPEC_VARIABLE_NAME,
      Status
      ));
    goto UseDefault;
  }

  // ensure null termination
  mPlatformCompatSpec = (CHAR8 *)AllocateRuntimeZeroPool (Size + 1);
  if (mPlatformCompatSpec == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: CompatSpec alloc failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gRT->GetVariable (
                  FMP_PLATFORM_COMPAT_SPEC_VARIABLE_NAME,
                  &gNVIDIAPublicVariableGuid,
                  NULL,
                  &Size,
                  mPlatformCompatSpec
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting %s: %r\n",
      __FUNCTION__,
      FMP_PLATFORM_COMPAT_SPEC_VARIABLE_NAME,
      Status
      ));
    FreePool (mPlatformCompatSpec);
    mPlatformCompatSpec = NULL;
    return Status;
  }

  goto Done;

UseDefault:
  mPlatformCompatSpec = (CHAR8 *)AllocateRuntimeCopyPool (
                                   AsciiStrSize (FMP_PLATFORM_SPEC_DEFAULT),
                                   FMP_PLATFORM_SPEC_DEFAULT
                                   );
  if (mPlatformCompatSpec == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: CompatSpec alloc failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

Done:
  DEBUG ((
    DEBUG_INFO,
    "%a: %s=%a\n",
    __FUNCTION__,
    FMP_PLATFORM_COMPAT_SPEC_VARIABLE_NAME,
    mPlatformCompatSpec
    ));

  return EFI_SUCCESS;
}

/**
  Get version info.

  @retval EFI_SUCCESS                   Operation completed successfully
  @retval EFI_NOT_FOUND                 The VER partition wasn't found
  @retval Others                        An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
GetVersionInfo (
  VOID
  )
{
  EFI_STATUS                Status;
  CHAR8                     *VerStr;
  UINTN                     VerStrSize;
  NVIDIA_FW_IMAGE_PROTOCOL  *Image;
  FW_IMAGE_ATTRIBUTES       Attributes;
  UINTN                     BufferSize;

  VerStr = NULL;
  Image  = FwImageFindProtocol (VER_PARTITION_NAME);
  if (Image == NULL) {
    return EFI_NOT_FOUND;
  }

  Status = Image->GetAttributes (Image, &Attributes);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  BufferSize = MIN (Attributes.ReadBytes, mFmpDataBufferSize);
  Status     = Image->Read (
                        Image,
                        0,
                        BufferSize,
                        mFmpDataBuffer,
                        FW_IMAGE_RW_FLAG_NONE
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: VER read failed: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  ((CHAR8 *)mFmpDataBuffer)[BufferSize - 1] = '\0';

  Status = VerPartitionGetVersion (mFmpDataBuffer, BufferSize, &mTegraVersion, &VerStr);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to parse version info: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  VerStrSize          = AsciiStrSize (VerStr);
  mTegraVersionString = (CHAR16 *)
                        AllocateRuntimeZeroPool (VerStrSize * sizeof (CHAR16));
  if (mTegraVersionString == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Status = AsciiStrToUnicodeStrS (
             VerStr,
             mTegraVersionString,
             VerStrSize
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  mTegraVersionStatus = Status;

Done:
  if (VerStr != NULL) {
    FreePool (VerStr);
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Version=0x%x, Str=(%s), Status=%r\n",
    __FUNCTION__,
    mTegraVersion,
    mTegraVersionString,
    Status
    ));

  if (EFI_ERROR (Status)) {
    if (mTegraVersionString != NULL) {
      FreePool (mTegraVersionString);
      mTegraVersionString = NULL;
    }

    mTegraVersion       = 0;
    mTegraVersionStatus = EFI_UNSUPPORTED;
  }

  return mTegraVersionStatus;
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
  IN  UINTN  Bytes
  )
{
  UINTN  VerifyCompletion;

  mTotalBytesVerified += Bytes;
  VerifyCompletion     = (mTotalBytesVerified * FMP_PROGRESS_VERIFY_IMAGES) /
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
  IN  UINTN  Bytes
  )
{
  UINTN  WriteCompletion;

  if (mProgress != NULL) {
    mTotalBytesFlashed += Bytes;
    WriteCompletion     = (mTotalBytesFlashed * FMP_PROGRESS_WRITE_IMAGES) /
                          mTotalBytesToFlash;

    mProgress (mCurrentCompletion + WriteCompletion);
  }
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
  IN  UINTN  CompletionIncrement
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
  IN  UINTN  Completion
  )
{
  EFI_STATUS EFIAPI
  UpdateImageProgress (
    IN  UINTN  Completion
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
  CONST CHAR16  *Name,
  CONST CHAR16  **List
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
  IN  CONST CHAR16  *ImageName
  )
{
  return NameIsInList (ImageName, SpecialImageNames);
}

/**
  Get package image name for FwImage name.

  @param[in]  Name                  Name of the FwImage
  @param[in]  Header                Pointer to the FW package header

  @retval CONST CHAR16 *            Package image name to use

**/
STATIC
CONST CHAR16 *
EFIAPI
GetPackageImageName (
  IN  CONST CHAR16             *Name,
  IN  CONST FW_PACKAGE_HEADER  *Header
  )
{
  CONST CHAR16  *PkgImageName;
  EFI_STATUS    Status;
  UINTN         ImageIndex;

  PkgImageName = Name;

  // special case for T194 mb1_b when B is inactive boot chain being updated
  if ((StrCmp (Name, L"mb1") == 0) && (mActiveBootChain == BOOT_CHAIN_A)) {
    Status = FwPackageGetImageIndex (
               Header,
               L"mb1_b",
               mIsProductionFused,
               mPlatformCompatSpec,
               mPlatformSpec,
               &ImageIndex
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "No mb1_b in package: %r\n", Status));
    } else {
      DEBUG ((DEBUG_INFO, "Using package mb1_b image\n"));
      PkgImageName = L"mb1_b";
    }
  }

  if ((StrCmp (Name, L"GPT") == 0) ||
      (StrCmp (Name, FW_PARTITION_UPDATE_INACTIVE_PARTITIONS) == 0))
  {
    switch (OTHER_BOOT_CHAIN (mActiveBootChain)) {
      case BOOT_CHAIN_A:
        PkgImageName = L"secondary_gpt";
        break;
      case BOOT_CHAIN_B:
        PkgImageName = L"secondary_gpt_backup";
        break;
    }
  }

  return PkgImageName;
}

/**
  Get package image index for FwImage name.

  @param[in]  Name                  Name of the FwImage
  @param[in]  Header                Pointer to the FW package header
  @param[out] ImageIndex            Pointer to return image index

  @retval EFI_SUCCESS               The operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
GetPackageImageIndex (
  IN  CONST FW_PACKAGE_HEADER  *Header,
  IN  CONST CHAR16             *Name,
  OUT UINTN                    *ImageIndex
  )
{
  EFI_STATUS    Status;
  CONST CHAR16  *PkgImageName;

  PkgImageName = GetPackageImageName (Name, Header);
  Status       = FwPackageGetImageIndex (
                   Header,
                   PkgImageName,
                   mIsProductionFused,
                   mPlatformCompatSpec,
                   mPlatformSpec,
                   ImageIndex
                   );

  return Status;
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
  IN  NVIDIA_FW_IMAGE_PROTOCOL  *FwImageProtocol,
  IN  UINTN                     Bytes,
  IN  CONST UINT8               *DataBuffer,
  IN  UINTN                     Flags
  )
{
  EFI_STATUS  Status;
  UINTN       WriteOffset;
  UINTN       BytesPerLoop;

  Status = EFI_SUCCESS;

  DEBUG ((
    DEBUG_VERBOSE,
    "Writing %s, bytes=%u\n",
    FwImageProtocol->ImageName,
    Bytes
    ));

  BytesPerLoop = FMP_WRITE_LOOP_SIZE;
  WriteOffset  = 0;
  while (Bytes > 0) {
    UINTN  WriteSize;

    WriteSize = (Bytes > BytesPerLoop) ? BytesPerLoop : Bytes;
    Status    = FwImageProtocol->Write (
                                   FwImageProtocol,
                                   WriteOffset,
                                   WriteSize,
                                   DataBuffer + WriteOffset,
                                   Flags
                                   );
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
  IN  CONST FW_PACKAGE_HEADER  *Header,
  IN  CONST CHAR16             *Name,
  IN  UINTN                    Flags
  )
{
  CONST FW_PACKAGE_IMAGE_INFO  *PkgImageInfo;
  EFI_STATUS                   Status;
  UINTN                        ImageIndex;
  CONST UINT8                  *DataBuffer;
  NVIDIA_FW_IMAGE_PROTOCOL     *FwImageProtocol;

  FwImageProtocol = FwImageFindProtocol (Name);
  if (FwImageProtocol == NULL) {
    DEBUG ((
      DEBUG_INFO,
      "%a: couldn't find image protocol for %s\n",
      __FUNCTION__,
      Name
      ));
    return EFI_NOT_FOUND;
  }

  Status = GetPackageImageIndex (
             Header,
             Name,
             &ImageIndex
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%s not found in package: %r\n", Name, Status));
    return Status;
  }

  PkgImageInfo = FwPackageImageInfoPtr (Header, ImageIndex);
  DataBuffer   = (CONST UINT8 *)FwPackageImageDataPtr (Header, ImageIndex);

  Status = WriteImageFromBuffer (
             FwImageProtocol,
             PkgImageInfo->Bytes,
             DataBuffer,
             Flags
             );
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
  IN  CONST FW_PACKAGE_HEADER  *Header
  )
{
  EFI_STATUS                Status;
  UINTN                     Index;
  UINTN                     PkgImageIndex;
  UINTN                     ImageCount;
  NVIDIA_FW_IMAGE_PROTOCOL  **FwImageProtocolArray;

  ImageCount           = FwImageGetCount ();
  FwImageProtocolArray = FwImageGetProtocolArray ();

  // Write all images except special ones that are done later
  for (Index = 0; Index < ImageCount; Index++) {
    CONST CHAR16              *ImageName;
    NVIDIA_FW_IMAGE_PROTOCOL  *FwImageProtocol;

    FwImageProtocol = FwImageProtocolArray[Index];
    ImageName       = FwImageProtocol->ImageName;
    if (IsSpecialImageName (ImageName)) {
      continue;
    }

    Status = GetPackageImageIndex (
               Header,
               ImageName,
               &PkgImageIndex
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: No image for partition %s: %r\n", __FUNCTION__, ImageName, Status));

      if (NameIsInList (ImageName, mFwImagesRequired)) {
        DEBUG ((DEBUG_ERROR, "%a: Missing required image for partition %s: %r\n", __FUNCTION__, ImageName, Status));
        return Status;
      }

      continue;
    }

    Status = WriteImage (
               Header,
               ImageName,
               FW_IMAGE_RW_FLAG_NONE
               );
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
  @param[in]  CompareDebugLevel     Debug level for compare error message.

  @retval EFI_SUCCESS               The operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
VerifyImage (
  IN  CONST FW_PACKAGE_HEADER  *Header,
  IN  CONST CHAR16             *Name,
  IN  UINTN                    Flags,
  IN  UINTN                    CompareDebugLevel
  )
{
  NVIDIA_FW_IMAGE_PROTOCOL     *FwImageProtocol;
  CONST UINT8                  *DataBuffer;
  UINTN                        Bytes;
  UINTN                        VerifyOffset;
  EFI_STATUS                   Status;
  CONST FW_PACKAGE_IMAGE_INFO  *PkgImageInfo;
  UINTN                        ImageIndex;
  FW_IMAGE_ATTRIBUTES          ImageAttributes;

  if (!mPcdFmpWriteVerifyImage) {
    return EFI_SUCCESS;
  }

  FwImageProtocol = FwImageFindProtocol (Name);
  if (FwImageProtocol == NULL) {
    DEBUG ((
      DEBUG_INFO,
      "%a: couldn't find image protocol for %s\n",
      __FUNCTION__,
      Name
      ));
    return EFI_NOT_FOUND;
  }

  Status = FwImageProtocol->GetAttributes (FwImageProtocol, &ImageAttributes);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Failed to get image=%s attributes: %r\n",
      Name,
      Status
      ));
    return Status;
  }

  Status = GetPackageImageIndex (
             Header,
             Name,
             &ImageIndex
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%s not found in package: %r\n", Name, Status));
    return Status;
  }

  PkgImageInfo = FwPackageImageInfoPtr (Header, ImageIndex);
  DataBuffer   = (CONST UINT8 *)FwPackageImageDataPtr (Header, ImageIndex);

  DEBUG ((
    DEBUG_VERBOSE,
    "Verifying %s: PkgOffset=%d, Bytes=%d\n",
    Name,
    PkgImageInfo->Offset,
    PkgImageInfo->Bytes
    ));

  VerifyOffset = 0;
  Bytes        = PkgImageInfo->Bytes;
  while (Bytes > 0) {
    UINTN  VerifySize;
    UINTN  VerifyBufferSize;

    VerifySize       = (Bytes > mFmpDataBufferSize) ? mFmpDataBufferSize : Bytes;
    VerifyBufferSize = ALIGN_VALUE (VerifySize, ImageAttributes.BlockSize);
    ASSERT (VerifyBufferSize <= mFmpDataBufferSize);

    Status = FwImageProtocol->Read (
                                FwImageProtocol,
                                VerifyOffset,
                                VerifyBufferSize,
                                mFmpDataBuffer,
                                Flags
                                );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to read image=%s: %r\n", Name, Status));
      return Status;
    }

    if (CompareMem (mFmpDataBuffer, DataBuffer + VerifyOffset, VerifySize) != 0) {
      DEBUG ((
        CompareDebugLevel,
        "Image=%s failed verify near offset=%u\n",
        Name,
        VerifyOffset
        ));
      return EFI_VOLUME_CORRUPTED;
    }

    VerifyOffset += VerifySize;
    Bytes        -= VerifySize;
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
  IN  CONST FW_PACKAGE_HEADER  *Header
  )
{
  EFI_STATUS                Status;
  UINTN                     Index;
  UINTN                     PkgImageIndex;
  UINTN                     ImageCount;
  NVIDIA_FW_IMAGE_PROTOCOL  **FwImageProtocolArray;

  if (!mPcdFmpWriteVerifyImage) {
    return EFI_SUCCESS;
  }

  ImageCount           = FwImageGetCount ();
  FwImageProtocolArray = FwImageGetProtocolArray ();

  for (Index = 0; Index < ImageCount; Index++) {
    CONST CHAR16              *ImageName;
    NVIDIA_FW_IMAGE_PROTOCOL  *FwImageProtocol;

    FwImageProtocol = FwImageProtocolArray[Index];
    ImageName       = FwImageProtocol->ImageName;
    if (NameIsInList (ImageName, NoVerifyPartitionNames)) {
      continue;
    }

    Status = GetPackageImageIndex (
               Header,
               ImageName,
               &PkgImageIndex
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: No image for partition %s: %r\n", __FUNCTION__, ImageName, Status));

      if (NameIsInList (ImageName, mFwImagesRequired)) {
        DEBUG ((DEBUG_ERROR, "%a: Missing required image for partition %s: %r\n", __FUNCTION__, ImageName, Status));
        return Status;
      }

      continue;
    }

    Status = VerifyImage (
               Header,
               ImageName,
               FW_IMAGE_RW_FLAG_READ_INACTIVE_IMAGE,
               DEBUG_ERROR
               );
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
  IN  CONST CHAR16  *Name,
  IN  UINTN         Flags
  )
{
  NVIDIA_FW_IMAGE_PROTOCOL  *FwImageProtocol;
  FW_IMAGE_ATTRIBUTES       Attributes;
  EFI_STATUS                Status;
  UINTN                     Bytes;

  FwImageProtocol = FwImageFindProtocol (Name);
  if (FwImageProtocol == NULL) {
    DEBUG ((
      DEBUG_INFO,
      "%a: couldn't find image protocol for %s\n",
      __FUNCTION__,
      Name
      ));
    return EFI_NOT_FOUND;
  }

  Status = FwImageProtocol->GetAttributes (FwImageProtocol, &Attributes);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Bytes = MIN (Attributes.WriteBytes, mFmpDataBufferSize);
  SetMem (mFmpDataBuffer, Bytes, 0xff);

  mTotalBytesToFlash += Bytes;
  return WriteImageFromBuffer (
           FwImageProtocol,
           Bytes,
           (UINT8 *)mFmpDataBuffer,
           Flags
           );
}

/**
  Update a single FwImage from a special single-image FW package/capsule.
  This is a development feature enabled by PcdFmpSingleImageUpdate and
  requires that the FMP_CAPSULE_SINGLE_PARTITION_CHAIN_VARIABLE variable
  be set to the partition chain to be written (0=A, 1=B).

  @param[in]  Header                Pointer to the single-image FW package header

  @retval EFI_SUCCESS               The operation completed successfully
  @retval Others                    An error occurred

**/
STATIC
EFI_STATUS
EFIAPI
FmpTegraSetSingleImage (
  IN  CONST FW_PACKAGE_HEADER  *Header
  )
{
  EFI_STATUS                   Status;
  CONST FW_PACKAGE_IMAGE_INFO  *PkgImageInfo;
  CHAR16                       PkgName[FW_IMAGE_NAME_LENGTH];
  UINT8                        BootChain;
  UINTN                        WriteFlag;
  UINTN                        VariableSize;

  // Get capsule package image name
  PkgImageInfo = FwPackageImageInfoPtr (Header, 0);
  FwPackageCopyImageName (PkgName, PkgImageInfo, FW_IMAGE_NAME_LENGTH);

  // Get boot chain from variable
  VariableSize = sizeof (BootChain);
  Status       = gRT->GetVariable (
                        FMP_CAPSULE_SINGLE_PARTITION_CHAIN_VARIABLE,
                        &gNVIDIAPublicVariableGuid,
                        NULL,
                        &VariableSize,
                        &BootChain
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting single partition chain: %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
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
      DEBUG ((
        DEBUG_ERROR,
        "%a: Invalid Boot Chain=%u\n",
        __FUNCTION__,
        BootChain
        ));
      return EFI_UNSUPPORTED;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: handling single image=%s\n",
    __FUNCTION__,
    PkgName
    ));

  // write and verify single image
  Status = WriteImage (Header, PkgName, WriteFlag);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SetImageProgress (FMP_PROGRESS_WRITE_IMAGES);

  Status = VerifyImage (Header, PkgName, WriteFlag, DEBUG_ERROR);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SetImageProgress (FMP_PROGRESS_VERIFY_IMAGES);

  // delete the single partition chain variable
  Status = gRT->SetVariable (
                  FMP_CAPSULE_SINGLE_PARTITION_CHAIN_VARIABLE,
                  &gNVIDIAPublicVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS |
                  EFI_VARIABLE_RUNTIME_ACCESS |
                  EFI_VARIABLE_NON_VOLATILE,
                  0,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error deleting single partition chain: %r\n",
      __FUNCTION__,
      Status
      ));
  }

  return EFI_SUCCESS;
}

STATIC
UINTN
EFIAPI
FmpTegraGetTotalBytesToFlash (
  IN  CONST FW_PACKAGE_HEADER  *Header
  )
{
  UINTN                        Index;
  UINTN                        TotalBytesToFlash;
  CONST FW_PACKAGE_IMAGE_INFO  *ImageInfo;

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
  OUT UINT32  *Version,
  OUT CHAR16  **VersionString
  )
{
  if (EFI_ERROR (mTegraVersionStatus)) {
    return mTegraVersionStatus;
  }

  if (Version != NULL) {
    *Version = mTegraVersion;
  }

  if (VersionString != NULL) {
    UINTN  VersionStringSize;

    // version string must be in allocated pool memory that caller frees
    VersionStringSize = StrSize (mTegraVersionString) * sizeof (CHAR16);
    *VersionString    = (CHAR16 *)AllocateRuntimeCopyPool (
                                    VersionStringSize,
                                    mTegraVersionString
                                    );
    if (*VersionString == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

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
  CONST FW_PACKAGE_HEADER      *Header;
  EFI_STATUS                   Status;
  UINTN                        Index;
  UINTN                        ImageCount;
  CHAR16                       SingleImageNameBuffer[FW_IMAGE_NAME_LENGTH];
  CONST CHAR16                 *SingleImageName;
  NVIDIA_FW_IMAGE_PROTOCOL     **FwImageProtocolArray;
  CONST FW_PACKAGE_IMAGE_INFO  *PkgImageInfo;
  BOOLEAN                      Canceled;
  CONST CHAR16                 *RequiredImageName;
  NVIDIA_FW_IMAGE_PROTOCOL     *FwImageProtocol;
  UINTN                        GptImageIndex;

  DEBUG ((
    DEBUG_INFO,
    "%a: Image=0x%p ImageSize=%u\n",
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

  if (!mFmpLibInitialized) {
    *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_FMP_LIB_UNINITIALIZED;
    return EFI_NOT_READY;
  }

  Status = mBootChainProtocol->CheckAndCancelUpdate (mBootChainProtocol, &Canceled);
  if (EFI_ERROR (Status) || Canceled) {
    *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_BOOT_CHAIN_UPDATE_CANCELED;
    return EFI_ABORTED;
  }

  Header = (CONST FW_PACKAGE_HEADER *)Image;

  Status = FwPackageValidateHeader (Header);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Update package header failed validation: %r\n",
      Status
      ));
    *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_INVALID_PACKAGE_HEADER;
    return EFI_ABORTED;
  }

  if (Header->Type != FW_PACKAGE_TYPE_FW) {
    DEBUG ((DEBUG_ERROR, "Package type=%u not supported!\n", Header->Type));
    *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_UNSUPPORTED_PACKAGE_TYPE;
    return EFI_ABORTED;
  }

  Status = FwPackageValidateImageInfoArray (Header);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Update package image info array invalid: %r\n",
      Status
      ));
    *LastAttemptStatus = LAS_ERROR_INVALID_PACKAGE_IMAGE_INFO_ARRAY;
    return Status;
  }

  // If supported, update inactive FwPartition meta-data by writing capsule
  // GPT data to pseudo-image if GPT image data is present in package.
  if ((FwImageFindProtocol (FW_PARTITION_UPDATE_INACTIVE_PARTITIONS) != NULL) &&
      !EFI_ERROR (GetPackageImageIndex (Header, L"GPT", &GptImageIndex)))
  {
    Status = WriteImage (
               Header,
               FW_PARTITION_UPDATE_INACTIVE_PARTITIONS,
               FW_IMAGE_RW_FLAG_NONE
               );
    if (EFI_ERROR (Status)) {
      *LastAttemptStatus = LAS_ERROR_GPT_METADATA_UPDATE_FAILED;
      return EFI_ABORTED;
    }
  }

  ImageCount = FwImageGetCount ();

  // Handle special case of a development package with exactly one image
  if (Header->ImageCount == 1) {
    if (!mPcdFmpSingleImageUpdate) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: PcdFmpSingleImageUpdateEnabled not set\n",
        __FUNCTION__
        ));
      *LastAttemptStatus = LAS_ERROR_SINGLE_IMAGE_NOT_SUPPORTED;
      return EFI_UNSUPPORTED;
    }

    PkgImageInfo = FwPackageImageInfoPtr (Header, 0);
    FwPackageCopyImageName (
      SingleImageNameBuffer,
      PkgImageInfo,
      sizeof (SingleImageNameBuffer)
      );
    SingleImageName = SingleImageNameBuffer;
    ImageCount      = 1;
    DEBUG ((
      DEBUG_INFO,
      "%a: handling single image=%s\n",
      __FUNCTION__,
      SingleImageName
      ));
  }

  FwImageProtocolArray = FwImageGetProtocolArray ();
  for (Index = 0; Index < ImageCount; Index++) {
    CONST CHAR16         *ImageName;
    UINTN                PkgImageIndex;
    FW_IMAGE_ATTRIBUTES  ImageAttributes;

    FwImageProtocol = FwImageProtocolArray[Index];
    ImageName       = FwImageProtocol->ImageName;

    if ((ImageCount == 1) && (StrCmp (ImageName, SingleImageName) != 0)) {
      continue;
    }

    Status = GetPackageImageIndex (
               Header,
               ImageName,
               &PkgImageIndex
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: No image for partition %s: %r\n", __FUNCTION__, ImageName, Status));

      if (NameIsInList (ImageName, mFwImagesRequired)) {
        DEBUG ((DEBUG_ERROR, "%a: Missing required image for partition %s: %r\n", __FUNCTION__, ImageName, Status));
        *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
        *LastAttemptStatus = LAS_ERROR_IMAGE_NOT_IN_PACKAGE;
        return EFI_ABORTED;
      }

      continue;
    }

    PkgImageInfo = FwPackageImageInfoPtr (Header, PkgImageIndex);

    Status = FwImageProtocol->GetAttributes (FwImageProtocol, &ImageAttributes);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "Error getting attributes for image %s: %r\n",
        ImageName,
        Status
        ));
      *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
      *LastAttemptStatus = LAS_ERROR_IMAGE_ATTRIBUTES_ERROR;
      return EFI_ABORTED;
    }

    if (PkgImageInfo->Bytes > ImageAttributes.WriteBytes) {
      DEBUG ((
        DEBUG_ERROR,
        "Package image %s is bigger than partition: %u > %u\n",
        ImageName,
        PkgImageInfo->Bytes,
        ImageAttributes.WriteBytes
        ));
      *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
      *LastAttemptStatus = LAS_ERROR_IMAGE_TOO_BIG;
      return EFI_ABORTED;
    }

    if (((CONST UINT8 *)FwPackageImageDataPtr (Header, PkgImageIndex) +
         PkgImageInfo->Bytes) > ((CONST UINT8 *)Image + ImageSize))
    {
      DEBUG ((
        DEBUG_ERROR,
        "Package image %s goes beyond end of capsule!\n",
        ImageName
        ));
      *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
      *LastAttemptStatus = LAS_ERROR_PACKAGE_SIZE_ERROR;
      return EFI_ABORTED;
    }
  }

  // Check that every required image has a protocol
  for (Index = 0; Index < mFwImagesRequiredCount; Index++) {
    RequiredImageName = mFwImagesRequired[Index];
    FwImageProtocol   = FwImageFindProtocol (RequiredImageName);
    if (FwImageProtocol == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: no protocol for %s\n", __FUNCTION__, RequiredImageName));
      *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
      *LastAttemptStatus = LAS_ERROR_NO_PROTOCOL_FOR_IMAGE;
      return EFI_ABORTED;
    }
  }

  *LastAttemptStatus = LAST_ATTEMPT_STATUS_SUCCESS;
  *ImageUpdatable    = IMAGE_UPDATABLE_VALID;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FmpTegraSetImage (
  IN  CONST VOID *Image,
  IN  UINTN ImageSize,
  IN  CONST VOID *VendorCode, OPTIONAL
  IN  EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS  Progress, OPTIONAL
  IN  UINT32                                         CapsuleFwVersion,
  OUT CHAR16                                         **AbortReason,
  OUT UINT32                                         *LastAttemptStatus
  )
{
  CONST FW_PACKAGE_HEADER  *Header;
  EFI_STATUS               Status;
  BOOLEAN                  GptUpdate;
  UINTN                    GptImageIndex;

  DEBUG ((
    DEBUG_INFO,
    "%a: Image=0x%p, ImageSize=%u Version=0x%x\n",
    __FUNCTION__,
    Image,
    ImageSize,
    CapsuleFwVersion
    ));

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

  Header              = (CONST FW_PACKAGE_HEADER *)Image;
  mTotalBytesFlashed  = 0;
  mTotalBytesVerified = 0;
  mCurrentCompletion  = 0;

  // Ignore Progress function parameter since it is a null implementation
  // when UpdateCapsule() is the caller.  Use our UpdateProgress() instead.
  mProgress = UpdateProgress;

  SetImageProgress (FMP_PROGRESS_CHECK_IMAGE);

  mTotalBytesToFlash  = FmpTegraGetTotalBytesToFlash (Header);
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
  DEBUG ((
    DEBUG_INFO,
    "%a: Starting FW update sequence, images=%u, bytes=%u\n",
    __FUNCTION__,
    FwImageGetCount (),
    mTotalBytesToFlash
    ));

  // detect optional GPT update
  GptUpdate = FALSE;
  if ((FwImageFindProtocol (FW_PARTITION_UPDATE_INACTIVE_PARTITIONS) != NULL) &&
      !EFI_ERROR (GetPackageImageIndex (Header, L"GPT", &GptImageIndex)))
  {
    Status = VerifyImage (
               Header,
               L"GPT",
               FW_IMAGE_RW_FLAG_READ_INACTIVE_IMAGE,
               DEBUG_INFO
               );
    if ((Status != EFI_VOLUME_CORRUPTED) && (Status != EFI_SUCCESS)) {
      DEBUG ((DEBUG_ERROR, "%a: error verifying GPT: %r\n", __FUNCTION__, Status));
      *LastAttemptStatus = LAS_ERROR_GPT_VERIFY_FAILED;
      return EFI_ABORTED;
    }

    GptUpdate = (Status != EFI_SUCCESS);
  }

  if (GptUpdate) {
    Status = InvalidateImage (L"GPT", FW_IMAGE_RW_FLAG_NONE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Invalidate GPT failed: %r\n", Status));
      *LastAttemptStatus = LAS_ERROR_GPT_INVALIDATE_FAILED;
      return EFI_ABORTED;
    }
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

  if (GptUpdate) {
    Status = WriteImage (Header, L"GPT", FW_IMAGE_RW_FLAG_NONE);
    if (EFI_ERROR (Status)) {
      *LastAttemptStatus = LAS_ERROR_GPT_WRITE_FAILED;
      return EFI_ABORTED;
    }

    DEBUG ((DEBUG_INFO, "\n%a: Chain %u GPT updated\n", __FUNCTION__, OTHER_BOOT_CHAIN (mActiveBootChain)));
  }

  SetImageProgress (FMP_PROGRESS_WRITE_IMAGES);

  Status = VerifyAllImages (Header);
  if (EFI_ERROR (Status)) {
    *LastAttemptStatus = LAS_ERROR_VERIFY_IMAGES_FAILED;
    return EFI_ABORTED;
  }

  SetImageProgress (FMP_PROGRESS_VERIFY_IMAGES);

  Status = mBrBctUpdateProtocol->UpdateFwChain (
                                   mBrBctUpdateProtocol,
                                   OTHER_BOOT_CHAIN (mActiveBootChain)
                                   );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Update BCT failed: %r\n", Status));
    *LastAttemptStatus = LAS_ERROR_BCT_UPDATE_FAILED;
    return EFI_ABORTED;
  }

Done:
  SetImageProgress (FMP_PROGRESS_UPDATE_BCT);
  *LastAttemptStatus = LAST_ATTEMPT_STATUS_SUCCESS;

  DEBUG ((DEBUG_INFO, "%a: exit success\n", __FUNCTION__));
  return EFI_SUCCESS;
}

/**
  Install FMP by calling installer function.

  @return None

**/
VOID
EFIAPI
FwImageInstallFmp (
  VOID
  )
{
  EFI_STATUS  Status;

  if (mInstaller == NULL) {
    DEBUG ((DEBUG_INFO, "%a: no installer\n", __FUNCTION__));
    return;
  }

  DEBUG ((DEBUG_INFO, "%a: installing FMP\n", __FUNCTION__));

  Status = mInstaller (mImageHandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FMP installer failed: %r\n", __FUNCTION__, Status));
  }

  mInstaller = NULL;
}

/**
  Function to handle new FwImage found or FmpDxe registers installer function.

  @return None

**/
VOID
EFIAPI
FmpDeviceFwImageCallback (
  VOID
  )
{
  EFI_STATUS  Status;

  if (mTegraVersionStatus == EFI_UNSUPPORTED) {
    Status = GetVersionInfo ();
    if (!EFI_ERROR (Status)) {
      mFmpLibInitialized = TRUE;
    } else if (Status == EFI_NOT_FOUND) {
      return;
    }
  }

  FwImageInstallFmp ();
  FwImageRegisterImageAddedCallback (NULL);
}

VOID
EFIAPI
FmpTegraRegisterInstaller (
  IN FMP_DEVICE_LIB_REGISTER_FMP_INSTALLER  Function
  )
{
  mInstaller = Function;

  if (mFmpLibInitializeFailed) {
    DEBUG ((DEBUG_ERROR, "%a: init failed\n", __FUNCTION__));
    FwImageInstallFmp ();
    return;
  }

  FmpDeviceFwImageCallback ();
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
  EFI_STATUS  Status;
  VOID        *Hob;

  mImageHandle = ImageHandle;

  FmpParamLibInit ();

  mPcdFmpWriteVerifyImage  = PcdGetBool (PcdFmpWriteVerifyImage);
  mPcdFmpSingleImageUpdate = PcdGetBool (PcdFmpSingleImageUpdate);

  mFmpDataBufferSize = FMP_DATA_BUFFER_SIZE;
  mFmpDataBuffer     = AllocateRuntimeZeroPool (mFmpDataBufferSize);
  if (mFmpDataBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: data buffer alloc failed\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    mActiveBootChain = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ActiveBootChain;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting active boot chain\n",
      __FUNCTION__
      ));
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  Status = gBS->LocateProtocol (
                  &gNVIDIABrBctUpdateProtocolGuid,
                  NULL,
                  (VOID **)&mBrBctUpdateProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "BrBctUpdate Protocol Guid=%g not found: %r\n",
      &gNVIDIABrBctUpdateProtocolGuid,
      Status
      ));
    goto Done;
  }

  Status = gBS->LocateProtocol (
                  &gNVIDIABootChainProtocolGuid,
                  NULL,
                  (VOID **)&mBootChainProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "BootChain Protocol Guid=%g not found: %r\n",
      &gNVIDIABootChainProtocolGuid,
      Status
      ));
    goto Done;
  }

  Status = GetFuseSettings ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting fuse settings: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  Status = GetTnSpec ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error getting TnSpec: %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  mFwImagesRequired = FwImageGetRequiredList (TegraGetChipID (), &mFwImagesRequiredCount);
  FwImageRegisterImageAddedCallback (FmpDeviceFwImageCallback);
  Status = EFI_SUCCESS;

Done:
  if (EFI_ERROR (Status)) {
    if (mFmpDataBuffer != NULL) {
      FreePool (mFmpDataBuffer);
      mFmpDataBuffer = NULL;
    }

    if (mAddressChangeEvent != NULL) {
      gBS->CloseEvent (mAddressChangeEvent);
      mAddressChangeEvent = NULL;
    }

    if (mPlatformCompatSpec != NULL) {
      FreePool (mPlatformCompatSpec);
      mPlatformCompatSpec = NULL;
    }

    if (mPlatformSpec != NULL) {
      FreePool (mPlatformSpec);
      mPlatformSpec = NULL;
    }

    mFmpDataBufferSize      = 0;
    mBrBctUpdateProtocol    = NULL;
    mBootChainProtocol      = NULL;
    mActiveBootChain        = MAX_UINT32;
    mFmpLibInitializeFailed = TRUE;
  }

  // mFmpLibInitialized flag inibits API if there was an error
  return EFI_SUCCESS;
}
