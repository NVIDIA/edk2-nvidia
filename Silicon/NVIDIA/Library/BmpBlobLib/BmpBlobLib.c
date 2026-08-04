/** @file
  NVIDIA BMP blob parser library.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BmpBlobLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>

#define BMP_BLOB_MIN_HEADER_SIZE  (36U)

//
// Hard cap on NumEntries. A legitimate bmp.blob only needs a handful of
// (image-type, resolution) pairs; without a cap a crafted header can claim
// millions of 52-byte entries inside the 128 MiB read budget and stall boot
// in BmpBlobGetBestBmp()'s linear scan.
//
#define BMP_BLOB_MAX_ENTRIES  (64U)

STATIC
UINT32
BmpBlobReadU32 (
  IN CONST VOID  *Address
  )
{
  UINT32  Value;

  CopyMem (&Value, Address, sizeof (Value));
  return Value;
}

STATIC
CONST NVIDIA_BLOB_HEADER *
BmpBlobGetHeader (
  IN CONST BMP_BLOB_CONTEXT  *Context
  )
{
  return (CONST NVIDIA_BLOB_HEADER *)Context->Blob;
}

STATIC
UINT32
BmpBlobGetHeaderFieldU32 (
  IN CONST BMP_BLOB_CONTEXT  *Context,
  IN UINTN                   FieldOffset
  )
{
  return BmpBlobReadU32 (Context->Blob + FieldOffset);
}

STATIC
EFI_STATUS
BmpBlobGetEntrySize (
  IN  NVIDIA_BLOB_TYPE  Type,
  OUT UINT32            *EntrySize
  )
{
  if (EntrySize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  switch (Type) {
    case NVIDIA_BLOB_TYPE_UPDATE:
    case NVIDIA_BLOB_TYPE_FIRMWARE:
      *EntrySize = sizeof (NVIDIA_BLOB_UPDATE_ENTRY);
      return EFI_SUCCESS;
    case NVIDIA_BLOB_TYPE_BMP:
      *EntrySize = sizeof (NVIDIA_BLOB_BMP_ENTRY);
      return EFI_SUCCESS;
    default:
      return EFI_UNSUPPORTED;
  }
}

STATIC
EFI_STATUS
BmpBlobValidateRange (
  IN UINT32  Offset,
  IN UINT32  Size,
  IN UINT32  Limit
  )
{
  if (Offset > Limit) {
    return EFI_VOLUME_CORRUPTED;
  }

  if (Size > (Limit - Offset)) {
    return EFI_VOLUME_CORRUPTED;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
BmpBlobValidateContext (
  IN CONST BMP_BLOB_CONTEXT  *Context
  )
{
  CONST NVIDIA_BLOB_HEADER  *Header;
  UINT32                    DeclaredBlobSize;
  UINT32                    EntriesOffset;
  UINT32                    NumEntries;
  UINT32                    EntrySize;
  EFI_STATUS                Status;
  NVIDIA_BLOB_TYPE          Type;
  UINT64                    EntryBytes;

  if ((Context == NULL) || (Context->Blob == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Context->BlobSize < BMP_BLOB_MIN_HEADER_SIZE) {
    return EFI_VOLUME_CORRUPTED;
  }

  Header = BmpBlobGetHeader (Context);
  if (CompareMem (Header->Magic, NVIDIA_BLOB_MAGIC, NVIDIA_BLOB_MAGIC_SIZE) != 0) {
    return EFI_VOLUME_CORRUPTED;
  }

  DeclaredBlobSize = BmpBlobGetHeaderFieldU32 (Context, OFFSET_OF (NVIDIA_BLOB_HEADER, Size));
  EntriesOffset    = BmpBlobGetHeaderFieldU32 (Context, OFFSET_OF (NVIDIA_BLOB_HEADER, EntriesOffset));
  NumEntries       = BmpBlobGetHeaderFieldU32 (Context, OFFSET_OF (NVIDIA_BLOB_HEADER, NumEntries));
  Type             = (NVIDIA_BLOB_TYPE)BmpBlobGetHeaderFieldU32 (Context, OFFSET_OF (NVIDIA_BLOB_HEADER, Type));

  if ((DeclaredBlobSize > Context->BlobSize) || (DeclaredBlobSize < BMP_BLOB_MIN_HEADER_SIZE)) {
    return EFI_VOLUME_CORRUPTED;
  }

  if ((EntriesOffset < BMP_BLOB_MIN_HEADER_SIZE) || (EntriesOffset > DeclaredBlobSize)) {
    return EFI_VOLUME_CORRUPTED;
  }

  Status = BmpBlobGetEntrySize (Type, &EntrySize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (NumEntries > BMP_BLOB_MAX_ENTRIES) {
    return EFI_VOLUME_CORRUPTED;
  }

  EntryBytes = (UINT64)EntriesOffset + ((UINT64)NumEntries * (UINT64)EntrySize);
  if (EntryBytes > DeclaredBlobSize) {
    return EFI_VOLUME_CORRUPTED;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
BmpBlobGetEntryPointer (
  IN  CONST BMP_BLOB_CONTEXT  *Context,
  IN  UINT32                  Index,
  OUT CONST UINT8             **EntryPointer,
  OUT UINT32                  *EntrySize
  )
{
  EFI_STATUS        Status;
  UINT32            NumEntries;
  UINT32            EntriesOffset;
  UINT64            EntryOffset;
  NVIDIA_BLOB_TYPE  Type;

  if ((Context == NULL) || (EntryPointer == NULL) || (EntrySize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = BmpBlobGetNumEntries (Context, &NumEntries);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Index >= NumEntries) {
    return EFI_NOT_FOUND;
  }

  Status = BmpBlobGetType (Context, &Type);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = BmpBlobGetEntrySize (Type, EntrySize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  EntriesOffset = BmpBlobGetHeaderFieldU32 (Context, OFFSET_OF (NVIDIA_BLOB_HEADER, EntriesOffset));
  EntryOffset   = (UINT64)EntriesOffset + ((UINT64)Index * (UINT64)(*EntrySize));
  if ((EntryOffset + *EntrySize) > Context->BlobSize) {
    return EFI_VOLUME_CORRUPTED;
  }

  *EntryPointer = Context->Blob + EntryOffset;
  return EFI_SUCCESS;
}

STATIC
UINT32
BmpBlobGetOptimalBmpResolution (
  IN UINT32   PanelResolution,
  IN BOOLEAN  IsPanelPortrait,
  IN UINT32   RotationAngle
  )
{
  BOOLEAN  IsBmpPortrait;

  if ((RotationAngle == 0U) || (RotationAngle == 180U)) {
    IsBmpPortrait = IsPanelPortrait;
  } else {
    IsBmpPortrait = !IsPanelPortrait;
  }

  if (IsBmpPortrait) {
    return NVIDIA_BMP_RESOLUTION_1200P_PORTRAIT;
  }

  if (PanelResolution >= 2160U) {
    return NVIDIA_BMP_RESOLUTION_4K;
  } else if (PanelResolution >= 1080U) {
    return NVIDIA_BMP_RESOLUTION_1080P;
  } else if (PanelResolution >= 810U) {
    return NVIDIA_BMP_RESOLUTION_810P;
  } else if (PanelResolution >= 720U) {
    return NVIDIA_BMP_RESOLUTION_720P;
  }

  return NVIDIA_BMP_RESOLUTION_480P;
}

EFI_STATUS
EFIAPI
BmpBlobInitContext (
  IN  CONST VOID        *BlobBuffer,
  IN  UINTN             BlobBufferSize,
  OUT BMP_BLOB_CONTEXT  *Context
  )
{
  CONST UINT8               *Buffer;
  CONST NVIDIA_BLOB_HEADER  *Header;
  EFI_STATUS                Status;
  UINT32                    DeclaredBlobSize;

  if ((BlobBuffer == NULL) || (Context == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((BlobBufferSize < BMP_BLOB_MIN_HEADER_SIZE) || (BlobBufferSize > MAX_UINT32)) {
    return EFI_BAD_BUFFER_SIZE;
  }

  ZeroMem (Context, sizeof (*Context));
  Buffer = (CONST UINT8 *)BlobBuffer;

  //
  // Only unsigned NVIDIA__BLOB__V2 is supported. A tegrasign wrapper may exist
  // on disk historically, but this library does not authenticate it; accepting
  // that layout without verification would falsely imply the blob is trusted.
  //
  Context->Blob     = Buffer;
  Context->BlobSize = (UINT32)BlobBufferSize;

  Status = BmpBlobValidateContext (Context);
  if (EFI_ERROR (Status)) {
    ZeroMem (Context, sizeof (*Context));
    return Status;
  }

  Header           = BmpBlobGetHeader (Context);
  DeclaredBlobSize = BmpBlobReadU32 (&Header->Size);
  if (DeclaredBlobSize > Context->BlobSize) {
    ZeroMem (Context, sizeof (*Context));
    return EFI_VOLUME_CORRUPTED;
  }

  Context->BlobSize = DeclaredBlobSize;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
BmpBlobGetType (
  IN  CONST BMP_BLOB_CONTEXT  *Context,
  OUT NVIDIA_BLOB_TYPE        *Type
  )
{
  EFI_STATUS  Status;

  if (Type == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = BmpBlobValidateContext (Context);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *Type = (NVIDIA_BLOB_TYPE)BmpBlobGetHeaderFieldU32 (Context, OFFSET_OF (NVIDIA_BLOB_HEADER, Type));
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
BmpBlobGetNumEntries (
  IN  CONST BMP_BLOB_CONTEXT  *Context,
  OUT UINT32                  *NumEntries
  )
{
  EFI_STATUS  Status;

  if (NumEntries == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = BmpBlobValidateContext (Context);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *NumEntries = BmpBlobGetHeaderFieldU32 (Context, OFFSET_OF (NVIDIA_BLOB_HEADER, NumEntries));
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
BmpBlobGetBmpEntry (
  IN  CONST BMP_BLOB_CONTEXT  *Context,
  IN  UINT32                  Index,
  OUT NVIDIA_BLOB_BMP_ENTRY   *Entry
  )
{
  EFI_STATUS        Status;
  NVIDIA_BLOB_TYPE  Type;
  CONST UINT8       *EntryPointer;
  UINT32            EntrySize;

  if (Entry == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = BmpBlobGetType (Context, &Type);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Type != NVIDIA_BLOB_TYPE_BMP) {
    return EFI_UNSUPPORTED;
  }

  Status = BmpBlobGetEntryPointer (Context, Index, &EntryPointer, &EntrySize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (EntrySize != sizeof (NVIDIA_BLOB_BMP_ENTRY)) {
    return EFI_VOLUME_CORRUPTED;
  }

  CopyMem (Entry, EntryPointer, sizeof (*Entry));
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
BmpBlobGetUpdateEntry (
  IN  CONST BMP_BLOB_CONTEXT    *Context,
  IN  UINT32                    Index,
  OUT NVIDIA_BLOB_UPDATE_ENTRY  *Entry
  )
{
  EFI_STATUS        Status;
  NVIDIA_BLOB_TYPE  Type;
  CONST UINT8       *EntryPointer;
  UINT32            EntrySize;

  if (Entry == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = BmpBlobGetType (Context, &Type);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((Type != NVIDIA_BLOB_TYPE_UPDATE) && (Type != NVIDIA_BLOB_TYPE_FIRMWARE)) {
    return EFI_UNSUPPORTED;
  }

  Status = BmpBlobGetEntryPointer (Context, Index, &EntryPointer, &EntrySize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (EntrySize != sizeof (NVIDIA_BLOB_UPDATE_ENTRY)) {
    return EFI_VOLUME_CORRUPTED;
  }

  CopyMem (Entry, EntryPointer, sizeof (*Entry));
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
BmpBlobGetEntryData (
  IN  CONST BMP_BLOB_CONTEXT  *Context,
  IN  UINT32                  Index,
  OUT CONST VOID              **Data,
  OUT UINT32                  *DataSize
  )
{
  EFI_STATUS                Status;
  NVIDIA_BLOB_TYPE          Type;
  NVIDIA_BLOB_BMP_ENTRY     BmpEntry;
  NVIDIA_BLOB_UPDATE_ENTRY  UpdateEntry;
  UINT32                    Offset;
  UINT32                    Size;

  if ((Data == NULL) || (DataSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = BmpBlobGetType (Context, &Type);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  switch (Type) {
    case NVIDIA_BLOB_TYPE_BMP:
      Status = BmpBlobGetBmpEntry (Context, Index, &BmpEntry);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Offset = BmpEntry.BmpOffset;
      Size   = BmpEntry.BmpSize;
      break;
    case NVIDIA_BLOB_TYPE_UPDATE:
    case NVIDIA_BLOB_TYPE_FIRMWARE:
      Status = BmpBlobGetUpdateEntry (Context, Index, &UpdateEntry);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Offset = UpdateEntry.ImageOffset;
      Size   = UpdateEntry.ImageSize;
      break;
    default:
      return EFI_UNSUPPORTED;
  }

  Status = BmpBlobValidateRange (Offset, Size, Context->BlobSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *Data     = Context->Blob + Offset;
  *DataSize = Size;
  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS             Status;
  UINT32                 NumEntries;
  UINT32                 Index;
  UINT32                 BestIndex;
  UINT32                 BestResolution;
  UINT32                 OptimalResolution;
  NVIDIA_BLOB_BMP_ENTRY  Entry;
  NVIDIA_BLOB_TYPE       Type;

  if ((BmpData == NULL) || (BmpSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = BmpBlobGetType (Context, &Type);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Type != NVIDIA_BLOB_TYPE_BMP) {
    return EFI_UNSUPPORTED;
  }

  Status = BmpBlobGetNumEntries (Context, &NumEntries);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  OptimalResolution = BmpBlobGetOptimalBmpResolution (
                        PanelResolution,
                        IsPanelPortrait,
                        RotationAngle
                        );

  BestIndex      = MAX_UINT32;
  BestResolution = NVIDIA_BMP_RESOLUTION_480P;
  for (Index = 0; Index < NumEntries; Index++) {
    Status = BmpBlobGetBmpEntry (Context, Index, &Entry);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if ((Entry.BmpType == ImageType) &&
        (Entry.BmpResolution >= BestResolution) &&
        (Entry.BmpResolution <= OptimalResolution))
    {
      BestResolution = Entry.BmpResolution;
      BestIndex      = Index;
    }
  }

  if (BestIndex == MAX_UINT32) {
    return EFI_NOT_FOUND;
  }

  Status = BmpBlobGetEntryData (Context, BestIndex, BmpData, BmpSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (BmpResolution != NULL) {
    *BmpResolution = BestResolution;
  }

  return EFI_SUCCESS;
}
