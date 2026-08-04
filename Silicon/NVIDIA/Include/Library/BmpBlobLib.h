/** @file
  NVIDIA BMP blob parser library public interface.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __BMPBLOBLIB_H__
#define __BMPBLOBLIB_H__

#include <Uefi/UefiBaseType.h>
#include <Protocol/GraphicsOutput.h>

#define NVIDIA_BLOB_MAGIC       "NVIDIA__BLOB__V2"
#define NVIDIA_BLOB_MAGIC_SIZE  16

//
// GPT partition holding bmp.blob. Not part of the A/B boot chain, so the
// name is used verbatim.
//
#define NVIDIA_BMP_BLOB_PARTITION_NAME  L"BMP"

typedef enum {
  NVIDIA_BLOB_TYPE_UPDATE   = 0,
  NVIDIA_BLOB_TYPE_BMP      = 1,
  NVIDIA_BLOB_TYPE_FIRMWARE = 2
} NVIDIA_BLOB_TYPE;

typedef enum {
  NVIDIA_BMP_IMAGE_NVIDIA          = 0,
  NVIDIA_BMP_IMAGE_LOWBATTERY      = 1,
  NVIDIA_BMP_IMAGE_CHARGING        = 2,
  NVIDIA_BMP_IMAGE_CHARGED         = 3,
  NVIDIA_BMP_IMAGE_FULLYCHARGED    = 4,
  NVIDIA_BMP_IMAGE_ANDROID_OTA     = 5,
  NVIDIA_BMP_IMAGE_VERITY_ORANGE   = 6,
  NVIDIA_BMP_IMAGE_VERITY_RED_STOP = 7,
  NVIDIA_BMP_IMAGE_VERITY_RED_EIO  = 8,
  NVIDIA_BMP_IMAGE_USER_DEFINED    = 9
} NVIDIA_BMP_IMAGE_TYPE;

typedef enum {
  NVIDIA_BMP_RESOLUTION_480P           = 0,
  NVIDIA_BMP_RESOLUTION_720P           = 1,
  NVIDIA_BMP_RESOLUTION_810P           = 2,
  NVIDIA_BMP_RESOLUTION_1080P          = 3,
  NVIDIA_BMP_RESOLUTION_4K             = 4,
  NVIDIA_BMP_RESOLUTION_1200P_PORTRAIT = 5
} NVIDIA_BMP_RESOLUTION_TYPE;

#pragma pack(push, 1)
typedef struct {
  CHAR8     Magic[NVIDIA_BLOB_MAGIC_SIZE];
  UINT32    Version;
  UINT32    Size;
  UINT32    EntriesOffset;
  UINT32    NumEntries;
  UINT32    Type;
  UINT32    UncompressedSize;
  UINT8     Accessory[8];
} NVIDIA_BLOB_HEADER;

typedef struct {
  CHAR8     PartitionName[40];
  UINT32    ImageOffset;
  UINT32    ImageSize;
  UINT32    Version;
  UINT32    OpMode;
  CHAR8     SpecInfo[64];
} NVIDIA_BLOB_UPDATE_ENTRY;

typedef struct {
  UINT32    BmpType;
  UINT32    BmpOffset;
  UINT32    BmpSize;
  UINT32    BmpResolution;
  UINT8     Reserved[36];
} NVIDIA_BLOB_BMP_ENTRY;
#pragma pack(pop)

typedef struct {
  CONST UINT8    *Blob;
  UINT32         BlobSize;
} BMP_BLOB_CONTEXT;

EFI_STATUS
EFIAPI
BmpBlobInitContext (
  IN  CONST VOID        *BlobBuffer,
  IN  UINTN             BlobBufferSize,
  OUT BMP_BLOB_CONTEXT  *Context
  );

EFI_STATUS
EFIAPI
BmpBlobGetType (
  IN  CONST BMP_BLOB_CONTEXT  *Context,
  OUT NVIDIA_BLOB_TYPE        *Type
  );

EFI_STATUS
EFIAPI
BmpBlobGetNumEntries (
  IN  CONST BMP_BLOB_CONTEXT  *Context,
  OUT UINT32                  *NumEntries
  );

EFI_STATUS
EFIAPI
BmpBlobGetBmpEntry (
  IN  CONST BMP_BLOB_CONTEXT  *Context,
  IN  UINT32                  Index,
  OUT NVIDIA_BLOB_BMP_ENTRY   *Entry
  );

EFI_STATUS
EFIAPI
BmpBlobGetUpdateEntry (
  IN  CONST BMP_BLOB_CONTEXT    *Context,
  IN  UINT32                    Index,
  OUT NVIDIA_BLOB_UPDATE_ENTRY  *Entry
  );

EFI_STATUS
EFIAPI
BmpBlobGetEntryData (
  IN  CONST BMP_BLOB_CONTEXT  *Context,
  IN  UINT32                  Index,
  OUT CONST VOID              **Data,
  OUT UINT32                  *DataSize
  );

EFI_STATUS
EFIAPI
BmpBlobGetBestBmp (
  IN  CONST BMP_BLOB_CONTEXT  *Context,
  IN  UINT32                  ImageType,
  IN  UINT32                  PanelResolution,
  IN  BOOLEAN                 IsPanelPortrait,
  IN  UINT32                  RotationAngle,
  OUT CONST VOID              **BmpData,
  OUT UINT32                  *BmpSize,
  OUT UINT32                  *BmpResolution
  );

/**
  Read the BMP partition, pick the best entry for the panel and decode it
  into a GOP blt buffer.

  @param[in]  ImageType     NVIDIA_BMP_IMAGE_TYPE to look up.
  @param[in]  ScreenWidth   Active panel width in pixels.
  @param[in]  ScreenHeight  Active panel height in pixels.
  @param[out] Bitmap        Decoded blt buffer, caller frees with FreePool().
  @param[out] Width         Decoded image width in pixels.
  @param[out] Height        Decoded image height in pixels.

  @retval EFI_SUCCESS    Image decoded.
  @retval EFI_NOT_FOUND  No BMP partition, or no entry of ImageType.
  @retval Other          Partition read, blob parse or BMP decode failure.
**/
EFI_STATUS
EFIAPI
BmpBlobGetPartitionImage (
  IN  UINT32                         ImageType,
  IN  UINTN                          ScreenWidth,
  IN  UINTN                          ScreenHeight,
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL  **Bitmap,
  OUT UINTN                          *Width,
  OUT UINTN                          *Height
  );

#endif
