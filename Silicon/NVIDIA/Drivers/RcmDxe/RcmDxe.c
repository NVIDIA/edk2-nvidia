/** @file
*  RCM Boot Dxe
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "RcmDxe.h"

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>

/**
  Install rcm driver.

  @param  ImageHandle     The image handle.
  @param  SystemTable     The system table.

  @retval EFI_SUCEESS     Install Boot manager menu success.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
RcmDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  TEGRABL_BLOBHEADER            *RcmBlobHeader;
  T194_TEGRABL_BLOBHEADER       *T194RcmBlobHeader;
  UINTN                         RcmBlobBase;
  UINTN                         RcmBlobSize;
  UINTN                         OsCarveoutBase;
  UINTN                         OsCarveoutSize;
  UINT8                         BlobMagic[4] = { 'b', 'l', 'o', 'b' };
  UINTN                         Count;
  VOID                          *Hob;
  UINTN                         ChipId;
  UINTN                         KernelImageId;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  } else {
    DEBUG ((DEBUG_ERROR, "Failed to get PlatformResourceInfo\n"));
    return EFI_NOT_FOUND;
  }

  if (PlatformResourceInfo->BootType != TegrablBootRcm) {
    return EFI_NOT_FOUND;
  }

  RcmBlobBase = PlatformResourceInfo->RcmBlobInfo.Base;
  RcmBlobSize = PlatformResourceInfo->RcmBlobInfo.Size;

  if ((RcmBlobBase == 0) || (RcmBlobSize == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: RCM blob not found\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  ChipId = TegraGetChipID ();
  if (ChipId == T194_CHIP_ID) {
    T194RcmBlobHeader = (T194_TEGRABL_BLOBHEADER *)RcmBlobBase;
    if (CompareMem (T194RcmBlobHeader->BlobMagic, BlobMagic, sizeof (BlobMagic)) != 0) {
      DEBUG ((DEBUG_ERROR, "%a: T194 RCM blob corrupt\n", __FUNCTION__));
      return EFI_NOT_FOUND;
    }

    KernelImageId = T194_IMAGE_TYPE_KERNEL;

    for (Count = 0; Count < T194RcmBlobHeader->BlobEntries; Count++) {
      if (T194RcmBlobHeader->BlobInfo[Count].ImgType == KernelImageId) {
        DEBUG ((DEBUG_ERROR, "%a: ID: %d 0x%lx 0x%lx\n", __FUNCTION__, T194RcmBlobHeader->BlobInfo[Count].ImgType, T194RcmBlobHeader->BlobInfo[Count].Offset, T194RcmBlobHeader->BlobInfo[Count].Size));
        break;
      }
    }

    if (Count == T194RcmBlobHeader->BlobEntries) {
      DEBUG ((DEBUG_ERROR, "%a: OS image not found in RCM blob\n", __FUNCTION__));
      return EFI_NOT_FOUND;
    }

    PcdSet64S (PcdRcmKernelBase, (UINT64)T194RcmBlobHeader + T194RcmBlobHeader->BlobInfo[Count].Offset);
    PcdSet64S (PcdRcmKernelSize, T194RcmBlobHeader->BlobInfo[Count].Size);
  } else {
    RcmBlobHeader = (TEGRABL_BLOBHEADER *)RcmBlobBase;
    if (CompareMem (RcmBlobHeader->BlobMagic, BlobMagic, sizeof (BlobMagic)) != 0) {
      DEBUG ((DEBUG_ERROR, "%a: RCM blob corrupt\n", __FUNCTION__));
      return EFI_NOT_FOUND;
    }

    KernelImageId = IMAGE_TYPE_KERNEL;

    for (Count = 0; Count < RcmBlobHeader->BlobEntries; Count++) {
      if (RcmBlobHeader->BlobInfo[Count].ImgType == KernelImageId) {
        DEBUG ((DEBUG_ERROR, "%a: ID: %d 0x%lx 0x%lx\n", __FUNCTION__, RcmBlobHeader->BlobInfo[Count].ImgType, RcmBlobHeader->BlobInfo[Count].Offset, RcmBlobHeader->BlobInfo[Count].Size));
        break;
      }
    }

    if (Count == RcmBlobHeader->BlobEntries) {
      DEBUG ((DEBUG_ERROR, "%a: OS image not found in RCM blob\n", __FUNCTION__));
      return EFI_NOT_FOUND;
    }

    PcdSet64S (PcdRcmKernelBase, (UINT64)RcmBlobHeader + RcmBlobHeader->BlobInfo[Count].Offset);
    PcdSet64S (PcdRcmKernelSize, RcmBlobHeader->BlobInfo[Count].Size);
  }

  OsCarveoutBase = PlatformResourceInfo->RamdiskOSInfo.Base;
  OsCarveoutSize = PlatformResourceInfo->RamdiskOSInfo.Size;

  if ((OsCarveoutBase != 0) && (OsCarveoutSize != 0) && (OsCarveoutSize >= PcdGet64 (PcdRcmKernelSize))) {
    CopyMem ((VOID *)OsCarveoutBase, (VOID *)PcdGet64 (PcdRcmKernelBase), PcdGet64 (PcdRcmKernelSize));
  }

  return EFI_SUCCESS;
}
