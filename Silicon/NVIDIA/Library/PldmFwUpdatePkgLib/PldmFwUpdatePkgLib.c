/** @file

  PLDM FW update package library

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PldmFwUpdateLib.h>
#include <Library/PldmFwUpdatePkgLib.h>

STATIC CONST PLDM_UUID  mPldmGuidV1_0 = PLDM_FW_PKG_UUID_V1_0;
STATIC CONST PLDM_UUID  mPldmGuidV1_1 = PLDM_FW_PKG_UUID_V1_1;

CONST PLDM_FW_PKG_FW_DEVICE_ID_AREA *
EFIAPI
PldmFwPkgGetFwDeviceIdArea (
  IN CONST PLDM_FW_PKG_HDR  *Hdr
  )
{
  return (CONST PLDM_FW_PKG_FW_DEVICE_ID_AREA *)
         ((CONST UINT8 *)Hdr +
          OFFSET_OF (PLDM_FW_PKG_HDR, VersionString) +
          Hdr->VersionStringLength);
}

CONST PLDM_FW_PKG_DOWNSTREAM_DEVICE_ID_AREA *
EFIAPI
PldmFwPkgGetDownstreamDeviceIdArea (
  IN CONST PLDM_FW_PKG_HDR  *Hdr
  )
{
  CONST PLDM_FW_PKG_FW_DEVICE_ID_AREA  *FwDeviceIdArea;
  UINTN                                AreaSize;

  if (Hdr->FormatRevision < PLDM_FW_PKG_FORMAT_REVISION_2) {
    return NULL;
  }

  FwDeviceIdArea = PldmFwPkgGetFwDeviceIdArea (Hdr);
  AreaSize       = PldmFwPkgGetDeviceIdAreaSize (FwDeviceIdArea);

  return (CONST PLDM_FW_PKG_DOWNSTREAM_DEVICE_ID_AREA *)
         ((CONST UINT8 *)FwDeviceIdArea + AreaSize);
}

UINTN
EFIAPI
PldmFwPkgGetDeviceIdAreaSize (
  IN CONST PLDM_FW_PKG_FW_DEVICE_ID_AREA  *DeviceIdArea
  )
{
  CONST PLDM_FW_PKG_DEVICE_ID_RECORD  *Record;
  UINTN                               AreaSize;
  UINTN                               Index;

  AreaSize = OFFSET_OF (PLDM_FW_PKG_FW_DEVICE_ID_AREA, Records);
  Record   = DeviceIdArea->Records;
  for (Index = 0; Index < DeviceIdArea->RecordCount; Index++) {
    AreaSize += Record->Length;
    Record    = PldmFwPkgGetNextDeviceIdRecord (Record);
  }

  DEBUG ((DEBUG_INFO, "%a: AreaSize=%u\n", __FUNCTION__, AreaSize));

  return AreaSize;
}

CONST VOID *
EFIAPI
PldmFwPkgGetDeviceIdRecordImageSetVersionString (
  IN CONST PLDM_FW_PKG_HDR               *Hdr,
  IN CONST PLDM_FW_PKG_DEVICE_ID_RECORD  *Record
  )
{
  CONST UINT8  *Ptr;

  Ptr  = Record->ApplicableComponents;
  Ptr += (Hdr->ComponentBitmapBitLength / 8);

  return Ptr;
}

CONST PLDM_FW_DESCRIPTOR *
EFIAPI
PldmFwPkgGetFwDeviceIdRecordDescriptors (
  IN CONST PLDM_FW_PKG_HDR               *Hdr,
  IN CONST PLDM_FW_PKG_DEVICE_ID_RECORD  *Record
  )
{
  CONST UINT8  *Ptr;

  Ptr  = (CONST UINT8 *)PldmFwPkgGetDeviceIdRecordImageSetVersionString (Hdr, Record);
  Ptr += Record->ImageSetVersionStringLength;

  return (CONST PLDM_FW_DESCRIPTOR *)Ptr;
}

CONST PLDM_FW_PKG_DEVICE_ID_RECORD *
EFIAPI
PldmFwPkgGetNextDeviceIdRecord (
  CONST PLDM_FW_PKG_DEVICE_ID_RECORD  *Record
  )
{
  return (CONST PLDM_FW_PKG_DEVICE_ID_RECORD *)
         ((CONST UINT8 *)Record + Record->Length);
}

CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO_AREA *
EFIAPI
PldmFwPkgGetComponentImageInfoArea (
  IN CONST PLDM_FW_PKG_HDR  *Hdr
  )
{
  CONST PLDM_FW_PKG_FW_DEVICE_ID_AREA  *DeviceIdArea;
  UINTN                                AreaSize;

  DeviceIdArea = PldmFwPkgGetDownstreamDeviceIdArea (Hdr);
  if (DeviceIdArea == NULL) {
    DeviceIdArea = PldmFwPkgGetFwDeviceIdArea (Hdr);
  }

  AreaSize = PldmFwPkgGetDeviceIdAreaSize (DeviceIdArea);

  return (CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO_AREA *)
         ((CONST UINT8 *)DeviceIdArea + AreaSize);
}

UINTN
EFIAPI
PldmFwPkgGetComponentImageInfoAreaSize (
  IN CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO_AREA  *ImageInfoArea
  )
{
  CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO  *ImageInfo;
  UINTN                                   AreaSize;
  UINTN                                   Index;

  AreaSize  = OFFSET_OF (PLDM_FW_PKG_COMPONENT_IMAGE_INFO_AREA, ImageInfo);
  ImageInfo = ImageInfoArea->ImageInfo;
  for (Index = 0; Index < ImageInfoArea->ImageCount; Index++) {
    AreaSize += PldmFwPkgGetComponentImageInfoSize (ImageInfo);
    ImageInfo = PldmFwPkgGetNextComponentImage (ImageInfo);
  }

  DEBUG ((DEBUG_INFO, "%a: AreaSize=%u\n", __FUNCTION__, AreaSize));

  return AreaSize;
}

UINTN
EFIAPI
PldmFwPkgGetComponentImageInfoSize (
  IN CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO  *ImageInfo
  )
{
  return OFFSET_OF (PLDM_FW_PKG_COMPONENT_IMAGE_INFO, VersionString) + ImageInfo->VersionStringLength;
}

CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO *
EFIAPI
PldmFwPkgGetNextComponentImage (
  IN CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO  *ImageInfo
  )
{
  return (CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO *)
         ((CONST UINT8 *)ImageInfo + PldmFwPkgGetComponentImageInfoSize (ImageInfo));
}

BOOLEAN
EFIAPI
PldmFwPkgMatchesFD (
  IN CONST PLDM_FW_PKG_HDR                *Hdr,
  IN UINTN                                DescriptorCount,
  IN CONST PLDM_FW_DESCRIPTOR             *FwDescriptors,
  OUT CONST PLDM_FW_PKG_DEVICE_ID_RECORD  **FwDeviceRecord
  )
{
  CONST PLDM_FW_PKG_FW_DEVICE_ID_AREA  *FwDeviceIdArea;
  CONST PLDM_FW_PKG_DEVICE_ID_RECORD   *Record;
  CONST PLDM_FW_DESCRIPTOR             *Descriptor;
  UINTN                                Index;
  UINTN                                Index1;
  BOOLEAN                              Mismatch;

  FwDeviceIdArea = PldmFwPkgGetFwDeviceIdArea (Hdr);

  DEBUG ((DEBUG_INFO, "%a: DevIdAreaOffset=0x%llx\n", __FUNCTION__, (UINT64)FwDeviceIdArea - (UINT64)Hdr));

  Record = FwDeviceIdArea->Records;
  for (Index = 0; Index < FwDeviceIdArea->RecordCount; Index++) {
    Mismatch   = FALSE;
    Descriptor = PldmFwPkgGetFwDeviceIdRecordDescriptors (Hdr, Record);
    for (Index1 = 0; Index1 < Record->DescriptorCount; Index1++) {
      PldmFwPrintFwDesc (Descriptor);
      if (!PldmFwDescriptorIsInList (
             Descriptor,
             FwDescriptors,
             DescriptorCount
             ))
      {
        Mismatch = TRUE;
        break;
      }

      Descriptor = PldmFwDescNext (Descriptor);
    }

    if (!Mismatch) {
      *FwDeviceRecord = Record;
      return TRUE;
    }

    Record = PldmFwPkgGetNextDeviceIdRecord (Record);
  }

  return FALSE;
}

BOOLEAN
EFIAPI
PldmFwPkgComponentIsApplicable (
  UINTN                                  ComponentIndex,
  IN CONST PLDM_FW_PKG_HDR               *Hdr,
  IN CONST PLDM_FW_PKG_DEVICE_ID_RECORD  *Record
  )
{
  UINTN  Byte;
  UINTN  Bit;

  Byte = ComponentIndex / 8;
  ASSERT (Byte < Hdr->ComponentBitmapBitLength / 8);

  Bit = ComponentIndex % 8;

  return ((Record->ApplicableComponents[Byte] & (1 << Bit)) != 0);
}

EFI_STATUS
EFIAPI
PldmFwPkgHdrValidate (
  IN CONST PLDM_FW_PKG_HDR  *Hdr,
  IN UINTN                  Length
  )
{
  CONST PLDM_FW_PKG_FW_DEVICE_ID_AREA          *FwDeviceIdArea;
  CONST PLDM_FW_PKG_FW_DEVICE_ID_AREA          *DownstreamDeviceIdArea;
  CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO_AREA  *ImageInfoArea;
  CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO       *ImageInfo;
  CONST PLDM_UUID                              *PldmGuid;
  UINTN                                        PkgLength;
  UINT32                                       HdrCrc;
  UINT32                                       Crc;
  UINTN                                        CrcOffset;
  UINTN                                        Index;

  PkgLength = 0;

  if ((Length < sizeof (*Hdr)) || (Length < Hdr->Size)) {
    DEBUG ((DEBUG_ERROR, "%a: bad length=%u", __FUNCTION__, Length));
    return EFI_BAD_BUFFER_SIZE;
  }

  switch (Hdr->FormatRevision) {
    case PLDM_FW_PKG_FORMAT_REVISION_1:
      PldmGuid = &mPldmGuidV1_0;
      break;
    case PLDM_FW_PKG_FORMAT_REVISION_2:
      PldmGuid = &mPldmGuidV1_1;
      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a: unsupported version=%u", __FUNCTION__, Hdr->FormatRevision));
      return EFI_UNSUPPORTED;
  }

  if (CompareMem (&Hdr->Identifier, PldmGuid, sizeof (Hdr->Identifier)) != 0) {
    DEBUG ((DEBUG_ERROR, "%a: invalid package id\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  CrcOffset = Hdr->Size - sizeof (UINT32);
  HdrCrc    = ReadUnaligned32 ((UINT32 *)((UINT8 *)Hdr + CrcOffset));
  Crc       = CalculateCrc32 ((UINT8 *)Hdr, CrcOffset);

  if (HdrCrc != Crc) {
    DEBUG ((DEBUG_ERROR, "%a: Crc offset=%u mismatch 0x%x/0x%x:", __FUNCTION__, CrcOffset, HdrCrc, Crc));
    return EFI_CRC_ERROR;
  }

  if ((Hdr->ComponentBitmapBitLength % 8) != 0) {
    DEBUG ((DEBUG_ERROR, "%a: invalid ComponentBitmapBitLength=%u\n", __FUNCTION__, Hdr->ComponentBitmapBitLength));
    return EFI_UNSUPPORTED;
  }

  PkgLength = OFFSET_OF (PLDM_FW_PKG_HDR, VersionString) + Hdr->VersionStringLength;

  FwDeviceIdArea         = PldmFwPkgGetFwDeviceIdArea (Hdr);
  PkgLength             += PldmFwPkgGetDeviceIdAreaSize (FwDeviceIdArea);
  DownstreamDeviceIdArea = PldmFwPkgGetDownstreamDeviceIdArea (Hdr);
  if (DownstreamDeviceIdArea != NULL) {
    PkgLength += PldmFwPkgGetDeviceIdAreaSize (DownstreamDeviceIdArea);
  }

  ImageInfoArea = PldmFwPkgGetComponentImageInfoArea (Hdr);
  PkgLength    += PldmFwPkgGetComponentImageInfoAreaSize (ImageInfoArea);
  PkgLength    += sizeof (UINT32); // HeaderCrc

  if (PkgLength != Hdr->Size) {
    DEBUG ((DEBUG_ERROR, "%a: invalid hdr length %u/%u\n", __FUNCTION__, PkgLength, Hdr->Size));
    return EFI_UNSUPPORTED;
  }

  ImageInfo = ImageInfoArea->ImageInfo;
  for (Index = 0; Index < ImageInfoArea->ImageCount; Index++) {
    PkgLength += ImageInfo->Size;
    ImageInfo  = PldmFwPkgGetNextComponentImage (ImageInfo);
  }

  if (PkgLength != Length) {
    DEBUG ((DEBUG_ERROR, "%a: invalid pkg length %u/%u\n", __FUNCTION__, PkgLength, Length));
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}
