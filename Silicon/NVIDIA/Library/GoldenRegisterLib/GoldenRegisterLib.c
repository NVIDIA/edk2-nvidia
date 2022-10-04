/** @file
*  Golden Register Library
*
*  Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi/UefiBaseType.h>

#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/GoldenRegisterLib.h>

/**
  Get GR blob size

  @param[in] GrBlobBase            Base address of GR blob

  @retval                          Blob size in bytes
**/
UINT32
EFIAPI
GrBlobBinarySize (
  IN  UINT64  GrBlobBase
  )
{
  GR_BLOB_HEADER  *GrBlobHeader;
  UINT32          Count;
  UINT32          Size;

  if (GrBlobBase == 0) {
    return 0;
  }

  GrBlobHeader = (GR_BLOB_HEADER *)GrBlobBase;

  Size = sizeof (GR_BLOB_HEADER);
  for (Count = 0; Count < GrBlobHeader->NumBins; Count++) {
    Size += GrBlobHeader->BlobDesc[Count].Size;
  }

  // Make size aligned to 64KB.
  Size = ALIGN_VALUE (Size, SIZE_64KB);

  return Size;
}

/**
  Locate UEFI GR binary in GR blob

  @param[in]  GrBlobBase           Base address of GR blob
  @param[out] Offset               Return offset of UEFI GR binary
  @param[out] Size                 Return size of UEFI GR binary.

  @retval EFI_SUCCESS              UEFI GR binary located
  @retval others                   Error occurred
**/
EFI_STATUS
EFIAPI
LocateGrBlobBinary (
  IN  UINT64  GrBlobBase,
  OUT UINT32  *Offset,
  OUT UINT32  *Size
  )
{
  GR_BLOB_HEADER  *GrBlobHeader;
  UINT32          Count;

  if ((GrBlobBase == 0) || (Offset == NULL) || (Size == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  GrBlobHeader = (GR_BLOB_HEADER *)GrBlobBase;

  for (Count = 0; Count < GrBlobHeader->NumBins; Count++) {
    if (AsciiStrCmp ((CONST CHAR8 *)GrBlobHeader->BlobDesc[Count].Name, (CONST CHAR8 *)GR_STAGE_NAME) == 0) {
      break;
    }
  }

  if (Count == GrBlobHeader->NumBins) {
    return EFI_NOT_FOUND;
  }

  *Offset = GrBlobHeader->BlobDesc[Count].Offset;
  *Size   = GrBlobHeader->BlobDesc[Count].Size;
  return EFI_SUCCESS;
}

/**
  Validate GR Blob Header

  @param[in] GrBlobBase            Base address of GR blob

  @retval EFI_SUCCESS              Header Valid
  @retval others                   Error occurred
**/
EFI_STATUS
EFIAPI
ValidateGrBlobHeader (
  IN UINT64  GrBlobBase
  )
{
  GR_BLOB_HEADER  *GrBlobHeader;

  if (GrBlobBase == 0) {
    return EFI_INVALID_PARAMETER;
  }

  GrBlobHeader = (GR_BLOB_HEADER *)GrBlobBase;

  if (AsciiStrCmp ((CONST CHAR8 *)GrBlobHeader->Signature, (CONST CHAR8 *)GR_BLOB_SIGNATURE) != 0) {
    return EFI_NOT_FOUND;
  }

  if ((GrBlobHeader->NumBins == 0) || (GrBlobHeader->NumBins > GR_MAX_BIN)) {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}
