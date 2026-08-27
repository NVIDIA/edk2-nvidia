/** @file
*  Golden Register Library
*
*  SPDX-FileCopyrightText: Copyright (c) 2021-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi/UefiBaseType.h>

#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/GoldenRegisterLib.h>

/**
  Compute the 64KB-aligned GR blob binary size with overflow checking.

  @param[in]  GrBlobHeader  Pointer to a GR blob header.
  @param[out] AlignedSize   On success, receives the aligned blob size.

  @retval TRUE   Size computed successfully.
  @retval FALSE  Invalid header fields or arithmetic overflow.
**/
STATIC
BOOLEAN
GrBlobComputeAlignedSize (
  IN  CONST GR_BLOB_HEADER  *GrBlobHeader,
  OUT UINT32                *AlignedSize
  )
{
  UINT64  Size;
  UINT32  Count;
  UINT32  BinSize;

  if ((GrBlobHeader == NULL) || (AlignedSize == NULL)) {
    return FALSE;
  }

  if ((GrBlobHeader->NumBins == 0) || (GrBlobHeader->NumBins > GR_MAX_BIN)) {
    return FALSE;
  }

  Size = sizeof (GR_BLOB_HEADER);
  for (Count = 0; Count < GrBlobHeader->NumBins; Count++) {
    BinSize = GrBlobHeader->BlobDesc[Count].Size;
    if (BinSize > (MAX_UINT64 - Size)) {
      DEBUG ((DEBUG_ERROR, "%a: BlobDesc[%u].Size causes overflow\n", __FUNCTION__, Count));
      return FALSE;
    }

    Size += BinSize;
  }

  //
  // ALIGN_VALUE (Size, SIZE_64KB) adds up to (SIZE_64KB - 1) before masking.
  // Reject sizes that cannot be aligned without exceeding UINT32.
  //
  if (Size > (MAX_UINT32 - (SIZE_64KB - 1))) {
    DEBUG ((DEBUG_ERROR, "%a: GR blob size 0x%lx too large to align\n", __FUNCTION__, Size));
    return FALSE;
  }

  Size         = ALIGN_VALUE (Size, SIZE_64KB);
  *AlignedSize = (UINT32)Size;
  return TRUE;
}

/**
  Get GR blob size

  @param[in] GrBlobBase            Base address of GR blob

  @retval                          Blob size in bytes, or 0 on error/overflow
**/
UINT32
EFIAPI
GrBlobBinarySize (
  IN  UINT64  GrBlobBase
  )
{
  GR_BLOB_HEADER  *GrBlobHeader;
  UINT32          AlignedSize;

  if (GrBlobBase == 0) {
    return 0;
  }

  GrBlobHeader = (GR_BLOB_HEADER *)GrBlobBase;
  if (!GrBlobComputeAlignedSize (GrBlobHeader, &AlignedSize)) {
    return 0;
  }

  return AlignedSize;
}

/**
  Locate UEFI GR binary in GR blob

  @param[in]  GrBlobBase           Base address of GR blob
  @param[out] Offset               Return offset of UEFI GR binary
  @param[out] Size                 Return size of UEFI GR binary.

  @retval EFI_SUCCESS              UEFI GR binary located
  @retval EFI_BAD_BUFFER_SIZE      Matched descriptor Offset/Size exceed the
                                   aligned blob, or blob size cannot be computed
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
  UINT32          AlignedSize;
  UINT32          BinOffset;
  UINT32          BinSize;

  if ((GrBlobBase == 0) || (Offset == NULL) || (Size == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  GrBlobHeader = (GR_BLOB_HEADER *)GrBlobBase;

  if ((GrBlobHeader->NumBins == 0) || (GrBlobHeader->NumBins > GR_MAX_BIN)) {
    return EFI_NOT_FOUND;
  }

  for (Count = 0; Count < GrBlobHeader->NumBins; Count++) {
    if (CompareMem (GrBlobHeader->BlobDesc[Count].Name, GR_STAGE_NAME, sizeof (GR_STAGE_NAME)) == 0) {
      break;
    }
  }

  if (Count == GrBlobHeader->NumBins) {
    return EFI_NOT_FOUND;
  }

  if (!GrBlobComputeAlignedSize (GrBlobHeader, &AlignedSize)) {
    return EFI_BAD_BUFFER_SIZE;
  }

  BinOffset = GrBlobHeader->BlobDesc[Count].Offset;
  BinSize   = GrBlobHeader->BlobDesc[Count].Size;
  if ((BinOffset >= AlignedSize) || (BinSize > (AlignedSize - BinOffset))) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: BlobDesc[%u] Offset 0x%x Size 0x%x exceeds aligned blob size 0x%x\n",
      __FUNCTION__,
      Count,
      BinOffset,
      BinSize,
      AlignedSize
      ));
    return EFI_BAD_BUFFER_SIZE;
  }

  *Offset = BinOffset;
  *Size   = BinSize;
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
  UINT32          AlignedSize;

  if (GrBlobBase == 0) {
    return EFI_INVALID_PARAMETER;
  }

  GrBlobHeader = (GR_BLOB_HEADER *)GrBlobBase;

  if (CompareMem (GrBlobHeader->Signature, GR_BLOB_SIGNATURE, GR_BLOB_SIG_LEN) != 0) {
    return EFI_NOT_FOUND;
  }

  if (!GrBlobComputeAlignedSize (GrBlobHeader, &AlignedSize)) {
    return EFI_BAD_BUFFER_SIZE;
  }

  return EFI_SUCCESS;
}
