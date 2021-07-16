/** @file
*  Golden Register Library
*
*  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
*  Portions provided under the following terms:
*  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#ifndef __GOLDENREGISTERLIB_H__
#define __GOLDENREGISTERLIB_H__

/** @brief Length of the signature used to validate the golden register blob */
#define GR_BLOB_SIG_LEN   8
/** @brief Signature of the golden register blob */
#define GR_BLOB_SIGNATURE "GOLDENR"
/** @brief Max number of binaries in the golden register blob */
#define GR_MAX_BIN        2
/** @brief Max length of the name of the associated bootloader stage */
#define GR_STAGE_NAME_LEN 8
/** @brief Name of the associated bootloader stage */
#define GR_STAGE_NAME     "UEFI"
/** @brief Max length of GR kernel command line argument */
#define GR_CMD_MAX_LEN    64

typedef struct {
  /** Base address of GR Blob */
  UINT64 GrBlobBase;
  /** Offset of data for the bootloader stage */
  UINT32 Offset;
  /** Size of data for the bootloader stage */
  UINT32 Size;
  /** Base of GR output location */
  UINTN  GrOutBase;
  /** Size of GR output location */
  UINTN  GrOutSize;
  /** Pointer to GR dump addresses */
  UINT32 *Address;
}GOLDEN_REGISTER_PRIVATE_DATA;

typedef struct {
  /** Name of the bootloader stage */
  UINT8  Name[GR_STAGE_NAME_LEN];
  /** Offset of the golden register list for the bootloader stage in the golden register blob */
  UINT32 Offset;
  /** Size of the golden register list for the bootloader stage in the golden register blob */
  UINT32 Size;
}GR_BLOB_BINARY_DESC;

typedef struct {
  /** Signature of the golden register blob */
  UINT8               Signature[GR_BLOB_SIG_LEN];
  /** Number of binaries in the golden register blob, upto GR_MAX_BIN */
  UINT32              NumBins;
  /** Binary descriptor associated with each binary of golden register dump */
  GR_BLOB_BINARY_DESC BlobDesc[GR_MAX_BIN];
}GR_BLOB_HEADER;

typedef struct {
  /** GR Data Address */
  UINT32 Address;
  /** GR Data Value */
  UINT32 Data;
}GR_DATA;

typedef struct {
  /** Offset of MB1 GR Data */
  UINT32 Mb1Offset;
  /** Size of MB1 GR Data */
  UINT32 Mb1Size;
  /** Offset of MB2 GR Data */
  UINT32 Mb2Offset;
  /** Size of MB2 GR Data */
  UINT32 Mb2Size;
  /** Offset of UEFI GR Data */
  UINT32 UefiOffset;
  /** Size of UEFI GR Data */
  UINT32 UefiSize;
}GR_DATA_HEADER;

/**
  Get GR blob size

  @param[in] GrBlobBase            Base address of GR blob

  @retval                          Blob size in bytes
**/
UINT32
EFIAPI
GrBlobBinarySize (
  IN  UINT64 GrBlobBase
);

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
  IN  UINT64 GrBlobBase,
  OUT UINT32 *Offset,
  OUT UINT32 *Size
);

/**
  Validate GR Blob Header

  @param[in] GrBlobBase            Base address of GR blob

  @retval EFI_SUCCESS              Header Valid
  @retval others                   Error occurred
**/
EFI_STATUS
EFIAPI
ValidateGrBlobHeader (
  IN UINT64 GrBlobBase
);

#endif // __GOLDENREGISTERLIB_H__
