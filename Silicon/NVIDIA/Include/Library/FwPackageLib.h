/** @file

  FwPackageLib - Firmware update package support library

  Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FW_PACKAGE_LIB_H__
#define __FW_PACKAGE_LIB_H__

#include <Uefi/UefiBaseType.h>

/**
  The NVIDIA FW update package starts with the FW_PACKAGE_HEADER followed by
  an array of FW_PACKAGE_IMAGE_INFO entries, one for each image present,
  followed by data for each image.

  +----------------------------+
  |     FW_PACKAGE_HEADER      |
  +----------------------------+ <--- FW_PACKAGE_HEADER.HeaderSize
  |  FW_PACKAGE_IMAGE_INFO[0]  |  \
  +----------------------------+   \  FW_PACKAGE_IMAGE_INFO array of N elements
  |           ...              |    ) N = FW_PACKAGE_HEADER.ImageCount
  +----------------------------+   /
  | FW_PACKAGE_IMAGE_INFO[N-1] |  /
  +----------------------------+ <--- FW_PACKAGE_IMAGE_INFO[0].Offset
  |     Data for image 0       | <--- Data size is FW_PACKAGE_IMAGE_INFO[0].Bytes
  +----------------------------+
  |           ...              |
  +----------------------------+ <--- FW_PACKAGE_IMAGE_INFO[N-1].Offset
  |     Data for image N-1     | <--- Data size is FW_PACKAGE_IMAGE_INFO[N-1].Bytes
  +----------------------------+ <--- FW_PACKAGE_HEADER.PackageSize
**/
#define FW_PACKAGE_MAGIC       "NVIDIA__BLOB__V3"
#define FW_PACKAGE_MAGIC_SIZE  16

#define FW_PACKAGE_TYPE_FW  0

#define FW_PACKAGE_UPDATE_MODE_ALWAYS          0
#define FW_PACKAGE_UPDATE_MODE_NON_PRODUCTION  1
#define FW_PACKAGE_UPDATE_MODE_PRODUCTION      2

typedef struct {
  CHAR8     Magic[FW_PACKAGE_MAGIC_SIZE];       // not NULL-terminated
  UINT32    BupVersion;
  UINT32    PackageSize;
  UINT32    HeaderSize;
  UINT32    ImageCount;
  UINT32    Type;
  UINT32    UncompressedSize;
  UINT8     RatchetInfo[8];                     // FW_PACKAGE_TYPE_FW only
} FW_PACKAGE_HEADER;

#define FW_PACKAGE_NAME_LENGTH         40
#define FW_PACKAGE_TNSPEC_LENGTH       128
#define FW_PACKAGE_IMAGE_INFO_VERSION  0

typedef struct {
  CHAR8     Name[FW_PACKAGE_NAME_LENGTH];
  UINT32    Offset;                             // from beginning of package header
  UINT32    Bytes;
  UINT32    Version;
  UINT32    UpdateMode;
  CHAR8     TnSpec[FW_PACKAGE_TNSPEC_LENGTH];
} FW_PACKAGE_IMAGE_INFO;

/**
  Copy and convert ASCII image name from FW_PACKAGE_IMAGE_INFO into Unicode buffer.

  @param[out]   Name                    Pointer to Unicode output buffer
  @param[in]    ImageInfo               Pointer to FW_PACKAGE_IMAGE_INFO containing
                                        the image name to convert
  @param[in]    NameBufferBytes         Size in bytes of the output buffer provided

  @retval       UINTN                   The number of Unicode characters copied to the
                                        output buffer not including the null terminator

**/
UINTN
EFIAPI
FwPackageCopyImageName (
  OUT CHAR16                       *Name,
  IN  CONST FW_PACKAGE_IMAGE_INFO  *ImageInfo,
  IN  UINTN                        NameBufferBytes
  );

/**
  Get image index for given image name.
  Assumes entire package is in contiguous memory starting at the header pointer.

  @param[in]    Header                  Pointer to package header structure
  @param[in]    Name                    Name of image to find
  @param[in]    IsProductionFused       Flag indicating if production mode is
                                        fused, used to enforce UpdateMode
                                        settings for the image.
  @param[in]    CompatSpec              Pointer to platform Compat TnSpec OPTIONAL
  @param[in]    FullSpec                Pointer to platform Full TnSpec OPTIONAL
  @param[out]   ImageIndex              Index of image in the FW_PACKAGE_IMAGE_INFO array

  @retval       EFI_SUCCESS             Image name found, ImageIndex valid
  @retval       EFI_NOT_FOUND           Image name not found, ImageIndex not valid

**/
EFI_STATUS
EFIAPI
FwPackageGetImageIndex (
  IN  CONST FW_PACKAGE_HEADER *Header,
  IN  CONST CHAR16 *Name,
  IN  BOOLEAN IsProductionFused,
  IN  CONST CHAR8 *CompatSpec, OPTIONAL
  IN  CONST CHAR8 *FullSpec, OPTIONAL
  OUT UINTN       *ImageIndex
  );

/**
  Return pointer to image data for image at requested index.
  Assumes entire package is in contiguous memory starting at the header pointer.

  @param[in]    Header                  Pointer to package header structure
  @param[in]    ImageIndex              Index of image in the FW_PACKAGE_IMAGE_INFO array

  @retval       VOID *                  Pointer to the first byte of data for the image

**/
CONST
VOID *
EFIAPI
FwPackageImageDataPtr (
  IN  CONST FW_PACKAGE_HEADER  *Header,
  IN  UINTN                    ImageIndex
  );

/**
  Return size in bytes of the FW_PACKAGE_IMAGE_INFO array for the package.

  @param[in]    Header                  Pointer to package header structure

  @retval       UINTN                   Number of bytes of the FW_PACKAGE_IMAGE_INFO
                                        array that follows the header structure
**/
UINTN
EFIAPI
FwPackageImageInfoArraySize (
  IN  CONST FW_PACKAGE_HEADER  *Header
  );

/**
  Return pointer to FW_PACKAGE_IMAGE_INFO structure of image at requested index.
  Assumes the FW_PACKAGE_IMAGE_INFO array is in contiguous memory following
  the header.

  @param[in]    Header                  Pointer to package header structure
  @param[in]    ImageIndex              Index of image in the FW_PACKAGE_IMAGE_INFO array

  @retval       FW_PACKAGE_IMAGE_INFO * Pointer to the desired image info structure

**/
CONST
FW_PACKAGE_IMAGE_INFO *
EFIAPI
FwPackageImageInfoPtr (
  IN  CONST FW_PACKAGE_HEADER  *Header,
  IN  UINTN                    ImageIndex
  );

/**
  Check if image's UpdateMode field is compatible with production mode
  fuse setting.

  @param[in]    ImageInfo               Pointer to FW_PACKAGE_IMAGE_INFO for image.
  @param[in]    IsProductionFused       Flag inidicating production mode/pre-production mode.

  @retval       BOOLEAN                 Flag indicating if image's UpdateMode
                                        field is compatible with production
                                        mode setting.
**/
BOOLEAN
EFIAPI
FwPackageUpdateModeIsOk (
  IN  CONST FW_PACKAGE_IMAGE_INFO  *ImageInfo,
  IN  BOOLEAN                      IsProductionFused
  );

/**
  Validate the package header structure.

  @param[in]    Header                  Pointer to package header structure

  @retval       EFI_SUCCESS             Header is valid
  @retval       EFI_INVALID_PARAMETER   Invalid Type field
  @retval       EFI_BAD_BUFFER_SIZE     PackageSize not big enough for header and
                                        FW_PACKAGE_IMAGE_INFO array
  @retval       EFI_INCOMPATIBLE_VERSION Bad magic string

**/
EFI_STATUS
EFIAPI
FwPackageValidateHeader (
  IN  CONST FW_PACKAGE_HEADER  *Header
  );

/**
  Validate the array of FW_PACKAGE_IMAGE_INFO structures in package.
  Assumes the FW_PACKAGE_IMAGE_INFO array is in contiguous memory following
  the header.

  @param[in]    Header                  Pointer to package header structure

  @retval       EFI_SUCCESS             Image info array is valid
  @retval       EFI_INVALID_PARAMETER   Invalid Name or UpdateMode field
  @retval       EFI_BAD_BUFFER_SIZE     Image data Offset+Bytes exceeds package size
                                        or computed package size not equal to
                                        Header->PackageSize
**/
EFI_STATUS
EFIAPI
FwPackageValidateImageInfoArray (
  IN  CONST FW_PACKAGE_HEADER  *Header
  );

#endif
