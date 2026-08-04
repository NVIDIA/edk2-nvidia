/** @file
  Loads BMP artwork out of the bmp.blob GPT partition.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
#include <Protocol/PartitionInfo.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BmpBlobLib.h>
#include <Library/BmpSupportLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

//
// Upper bound on the blob we are willing to buffer in DXE memory. The BMP
// partition is 64 MiB today; the slack absorbs a future resize without
// letting a corrupt header make us allocate arbitrarily.
//
#define BMP_BLOB_MAX_READ_BYTES  (128U * 1024U * 1024U)

//
// Enough to cover the unsigned V2 blob header used to size the follow-up read.
//
#define BMP_BLOB_HEADER_PROBE_BYTES  (64U)
#define BMP_BLOB_MIN_BLOB_BYTES      (36U)

STATIC
UINT32
BmpBlobPartitionReadU32 (
  IN CONST UINT8  *Address
  )
{
  UINT32  Value;

  CopyMem (&Value, Address, sizeof (Value));
  return Value;
}

/**
  Find the GPT partition named PartitionName and hand back its IO protocols.
**/
STATIC
EFI_STATUS
BmpBlobLocatePartition (
  IN  CONST CHAR16           *PartitionName,
  OUT EFI_BLOCK_IO_PROTOCOL  **BlockIoOut,
  OUT EFI_DISK_IO_PROTOCOL   **DiskIoOut
  )
{
  EFI_STATUS                   Status;
  UINTN                        NumHandles;
  EFI_HANDLE                   *HandleBuffer;
  UINTN                        Index;
  EFI_PARTITION_INFO_PROTOCOL  *PartitionInfo;
  EFI_BLOCK_IO_PROTOCOL        *BlockIo;
  EFI_DISK_IO_PROTOCOL         *DiskIo;

  *BlockIoOut = NULL;
  *DiskIoOut  = NULL;

  NumHandles   = 0;
  HandleBuffer = NULL;
  Status       = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gEfiPartitionInfoProtocolGuid,
                        NULL,
                        &NumHandles,
                        &HandleBuffer
                        );
  if (EFI_ERROR (Status) || (HandleBuffer == NULL)) {
    return EFI_NOT_FOUND;
  }

  Status = EFI_NOT_FOUND;
  for (Index = 0; Index < NumHandles; Index++) {
    PartitionInfo = NULL;
    if (EFI_ERROR (
          gBS->HandleProtocol (
                 HandleBuffer[Index],
                 &gEfiPartitionInfoProtocolGuid,
                 (VOID **)&PartitionInfo
                 )
          ) ||
        (PartitionInfo == NULL) ||
        (PartitionInfo->Type != PARTITION_TYPE_GPT))
    {
      continue;
    }

    if (StrCmp (PartitionInfo->Info.Gpt.PartitionName, PartitionName) != 0) {
      continue;
    }

    BlockIo = NULL;
    DiskIo  = NULL;
    if (EFI_ERROR (
          gBS->HandleProtocol (
                 HandleBuffer[Index],
                 &gEfiBlockIoProtocolGuid,
                 (VOID **)&BlockIo
                 )
          ) ||
        EFI_ERROR (
          gBS->HandleProtocol (
                 HandleBuffer[Index],
                 &gEfiDiskIoProtocolGuid,
                 (VOID **)&DiskIo
                 )
          ))
    {
      continue;
    }

    *BlockIoOut = BlockIo;
    *DiskIoOut  = DiskIo;
    Status      = EFI_SUCCESS;
    break;
  }

  FreePool (HandleBuffer);
  return Status;
}

/**
  Parse the on-media blob header so only the bytes the blob declares are read,
  rather than the whole (mostly empty) partition.
**/
STATIC
EFI_STATUS
BmpBlobGetReadSize (
  IN  EFI_DISK_IO_PROTOCOL  *DiskIo,
  IN  UINT32                MediaId,
  IN  UINT64                PartitionBytes,
  OUT UINTN                 *BlobReadBytes
  )
{
  EFI_STATUS  Status;
  UINT8       Header[BMP_BLOB_HEADER_PROBE_BYTES];
  UINT64      ReadBytes;

  if (PartitionBytes < sizeof (Header)) {
    return EFI_BAD_BUFFER_SIZE;
  }

  Status = DiskIo->ReadDisk (DiskIo, MediaId, 0, sizeof (Header), Header);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (CompareMem (Header, NVIDIA_BLOB_MAGIC, NVIDIA_BLOB_MAGIC_SIZE) == 0) {
    ReadBytes = (UINT64)BmpBlobPartitionReadU32 (&Header[OFFSET_OF (NVIDIA_BLOB_HEADER, Size)]);
  } else {
    //
    // Signed tegrasign wrappers are not authenticated by this library, so
    // reject them rather than treating an unverified wrapper as trusted.
    //
    return EFI_UNSUPPORTED;
  }

  if ((ReadBytes < BMP_BLOB_MIN_BLOB_BYTES) ||
      (ReadBytes > PartitionBytes) ||
      (ReadBytes > BMP_BLOB_MAX_READ_BYTES))
  {
    return EFI_BAD_BUFFER_SIZE;
  }

  *BlobReadBytes = (UINTN)ReadBytes;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
BmpBlobGetPartitionImage (
  IN  UINT32                         ImageType,
  IN  UINTN                          ScreenWidth,
  IN  UINTN                          ScreenHeight,
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL  **Bitmap,
  OUT UINTN                          *Width,
  OUT UINTN                          *Height
  )
{
  EFI_STATUS             Status;
  EFI_BLOCK_IO_PROTOCOL  *BlockIo;
  EFI_DISK_IO_PROTOCOL   *DiskIo;
  UINT64                 PartitionBytes;
  UINTN                  BlobReadBytes;
  VOID                   *BlobBuffer;
  BMP_BLOB_CONTEXT       BlobContext;
  CONST VOID             *BmpData;
  UINT32                 BmpSize;
  UINT32                 PanelResolution;
  UINTN                  BltBufferSize;

  if ((Bitmap == NULL) || (Width == NULL) || (Height == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Bitmap = NULL;
  *Width  = 0;
  *Height = 0;

  Status = BmpBlobLocatePartition (NVIDIA_BMP_BLOB_PARTITION_NAME, &BlockIo, &DiskIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: no %s partition: %r\n", __FUNCTION__, NVIDIA_BMP_BLOB_PARTITION_NAME, Status));
    return Status;
  }

  PartitionBytes = ((UINT64)BlockIo->Media->LastBlock + 1) * (UINT64)BlockIo->Media->BlockSize;
  if ((PartitionBytes == 0) || (PartitionBytes > MAX_UINTN)) {
    DEBUG ((DEBUG_ERROR, "%a: bad partition size 0x%Lx\n", __FUNCTION__, PartitionBytes));
    return EFI_BAD_BUFFER_SIZE;
  }

  Status = BmpBlobGetReadSize (
             DiskIo,
             BlockIo->Media->MediaId,
             PartitionBytes,
             &BlobReadBytes
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: bad blob header: %r\n", __FUNCTION__, Status));
    return Status;
  }

  BlobBuffer = AllocatePool (BlobReadBytes);
  if (BlobBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DiskIo->ReadDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     0,
                     BlobReadBytes,
                     BlobBuffer
                     );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: read of %u bytes failed: %r\n", __FUNCTION__, (UINT32)BlobReadBytes, Status));
    goto Exit;
  }

  Status = BmpBlobInitContext (BlobBuffer, BlobReadBytes, &BlobContext);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: BmpBlobInitContext failed: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  PanelResolution = (UINT32)MIN (ScreenWidth, ScreenHeight);
  Status          = BmpBlobGetBestBmp (
                      &BlobContext,
                      ImageType,
                      PanelResolution,
                      (BOOLEAN)(ScreenHeight > ScreenWidth),
                      0,
                      &BmpData,
                      &BmpSize,
                      NULL
                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: no blob entry for type %u: %r\n", __FUNCTION__, ImageType, Status));
    goto Exit;
  }

  Status = TranslateBmpToGopBlt (
             (VOID *)BmpData,
             BmpSize,
             Bitmap,
             &BltBufferSize,
             Height,
             Width
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: TranslateBmpToGopBlt failed: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: type %u decoded from blob, %ux%u\n",
    __FUNCTION__,
    ImageType,
    (UINT32)*Width,
    (UINT32)*Height
    ));

Exit:
  FreePool (BlobBuffer);
  return Status;
}
