/** @file

  FwPackageLib - Firmware update package support library

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

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/FwPackageLib.h>
#include <Library/PrintLib.h>

/**
  Validate the FW_PACKAGE_IMAGE_INFO structures of image at requested index.
  Assumes the FW_PACKAGE_IMAGE_INFO array is in contiguous memory following
  the header.

  @param[in]    Header                  Pointer to package header structure
  @param[in]    ImageIndex              Index of image in the FW_PACKAGE_IMAGE_INFO array

  @retval       EFI_SUCCESS             Image info struct is valid
  @retval       EFI_INVALID_PARAMETER   Invalid Name or UpdateMode field
  @retval       EFI_BAD_BUFFER_SIZE     Image data Offset+Bytes exceeds package size
**/
STATIC
EFI_STATUS
EFIAPI
FwPackageValidateImageInfo (
  IN  CONST FW_PACKAGE_HEADER       *Header,
  IN  UINTN                         ImageIndex
  )
{
  CONST FW_PACKAGE_IMAGE_INFO       *ImageInfo;

  ImageInfo = FwPackageImageInfoPtr (Header, ImageIndex);

  // ensure null-terminated name field
  if (AsciiStrSize (ImageInfo->Name) > sizeof (ImageInfo->Name)) {
    DEBUG ((DEBUG_ERROR, "FW package image index %u name too long\n", ImageIndex));
    return EFI_INVALID_PARAMETER;
  }

  // validate update mode
  switch (ImageInfo->UpdateMode) {
    case FW_PACKAGE_UPDATE_MODE_ALWAYS:
    case FW_PACKAGE_UPDATE_MODE_NON_PRODUCTION:
    case FW_PACKAGE_UPDATE_MODE_PRODUCTION:
      break;

    default:
      DEBUG ((DEBUG_ERROR, "Unknown image UpdateMode=%u for %a\n", ImageInfo->UpdateMode,
            ImageInfo->Name));
      return EFI_INVALID_PARAMETER;
  }

  // ensure image data is within the package
  if ((ImageInfo->Offset + ImageInfo->Bytes) > Header->PackageSize) {
    DEBUG ((DEBUG_ERROR, "FW package image data for %a overflows PackageSize=%u\n",
            ImageInfo->Name, Header->PackageSize));
    return EFI_BAD_BUFFER_SIZE;
  }

  // warn on default version mismatch
  if (ImageInfo->Version != FW_PACKAGE_IMAGE_INFO_VERSION) {
    DEBUG ((DEBUG_WARN, "%a WARNING: image info for '%a' has version=%u, expected=%u\n",
            __FUNCTION__, ImageInfo->Name, ImageInfo->Version,
            FW_PACKAGE_IMAGE_INFO_VERSION));
  }

  return EFI_SUCCESS;
}

UINTN
EFIAPI
FwPackageCopyImageName (
  OUT CHAR16                            *Name,
  IN  CONST FW_PACKAGE_IMAGE_INFO       *ImageInfo,
  IN  UINTN                             NameBufferBytes
  )
{
  return UnicodeSPrintAsciiFormat (Name, NameBufferBytes, ImageInfo->Name);
}

EFI_STATUS
EFIAPI
FwPackageGetImageIndex (
  IN  CONST FW_PACKAGE_HEADER           *Header,
  IN  CONST CHAR16                      *Name,
  IN  BOOLEAN                           IsProductionFused,
  OUT UINTN                             *ImageIndex
  )
{
  UINTN                                 Index;
  CHAR8                                 AsciiName[FW_PACKAGE_NAME_LENGTH];

  AsciiSPrintUnicodeFormat (AsciiName, sizeof (AsciiName), Name);
  for (Index = 0; Index < Header->ImageCount; Index++) {
    CONST FW_PACKAGE_IMAGE_INFO         *ImageInfo;

    ImageInfo = FwPackageImageInfoPtr (Header, Index);
    if (AsciiStrnCmp (AsciiName, ImageInfo->Name, sizeof (AsciiName)) == 0) {
      if (!FwPackageUpdateModeIsOk (ImageInfo, IsProductionFused)) {
        continue;
      }

      *ImageIndex = Index;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

CONST
VOID *
EFIAPI
FwPackageImageDataPtr (
  IN  CONST FW_PACKAGE_HEADER           *Header,
  IN  UINTN                             ImageIndex
  )
{
  CONST FW_PACKAGE_IMAGE_INFO           *ImageInfo;

  ImageInfo = FwPackageImageInfoPtr (Header, ImageIndex);

  return (CONST UINT8 *) Header + ImageInfo->Offset;
}

UINTN
EFIAPI
FwPackageImageInfoArraySize (
  IN  CONST FW_PACKAGE_HEADER       *Header
  )
{
  return Header->ImageCount * sizeof (FW_PACKAGE_IMAGE_INFO);
}

CONST
FW_PACKAGE_IMAGE_INFO *
EFIAPI
FwPackageImageInfoPtr (
  IN  CONST FW_PACKAGE_HEADER           *Header,
  IN  UINTN                             ImageIndex
  )
{
  CONST FW_PACKAGE_IMAGE_INFO           *ImageInfo;

  ASSERT (ImageIndex < Header->ImageCount);

  ImageInfo = (CONST FW_PACKAGE_IMAGE_INFO *)
    (((CONST UINT8 *) Header) + Header->HeaderSize);
  ImageInfo += ImageIndex;

  return ImageInfo;
}

BOOLEAN
EFIAPI
FwPackageUpdateModeIsOk (
  IN  CONST FW_PACKAGE_IMAGE_INFO       *ImageInfo,
  IN  BOOLEAN                           IsProductionFused
  )
{
  if (ImageInfo->UpdateMode != FW_PACKAGE_UPDATE_MODE_ALWAYS) {
    if ((IsProductionFused && (ImageInfo->UpdateMode != FW_PACKAGE_UPDATE_MODE_PRODUCTION)) ||
        (!IsProductionFused && (ImageInfo->UpdateMode != FW_PACKAGE_UPDATE_MODE_NON_PRODUCTION))) {
      return FALSE;
    }
  }

  return TRUE;
}

EFI_STATUS
EFIAPI
FwPackageValidateHeader (
  IN  CONST FW_PACKAGE_HEADER           *Header
  )
{
  // validate magic string, note: not NULL-terminated
  if (AsciiStrnCmp (Header->Magic, FW_PACKAGE_MAGIC, FW_PACKAGE_MAGIC_SIZE) != 0)
  {
    DEBUG ((DEBUG_ERROR, "Bad update package header magic: %.*a\n",
            FW_PACKAGE_MAGIC_SIZE, Header->Magic));
    return EFI_INCOMPATIBLE_VERSION;
  }

  // validate package size is bigger than header and all image info structs without data
  if (Header->PackageSize < (Header->HeaderSize + FwPackageImageInfoArraySize (Header))) {
    DEBUG ((DEBUG_ERROR, "Header PackageSize=%u too small for package info\n",
            Header->PackageSize));
    return EFI_BAD_BUFFER_SIZE;
  }

  // validate package type
  switch (Header->Type) {
    case FW_PACKAGE_TYPE_FW:
      break;

    default:
      DEBUG ((DEBUG_ERROR, "Unknown update package header type=%u\n", Header->Type));
      return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FwPackageValidateImageInfoArray (
  IN  CONST FW_PACKAGE_HEADER           *Header
  )
{
  UINTN                                 Index;
  UINTN                                 PackageSize;
  EFI_STATUS                            Status;

  // check that each image info is valid and compute total package size from image infos
  PackageSize = Header->HeaderSize + FwPackageImageInfoArraySize (Header);
  for (Index = 0; Index < Header->ImageCount; Index++) {
    CONST FW_PACKAGE_IMAGE_INFO         *ImageInfo;

    Status = FwPackageValidateImageInfo (Header, Index);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    ImageInfo = FwPackageImageInfoPtr (Header, Index);
    PackageSize += ImageInfo->Bytes;
  }

  // validate package size
  if (PackageSize != Header->PackageSize) {
    DEBUG ((DEBUG_ERROR, "Bad FW package size: header=%u, computed=%u\n",
            Header->PackageSize, PackageSize));
    return EFI_BAD_BUFFER_SIZE;
  }

  return EFI_SUCCESS;
}
