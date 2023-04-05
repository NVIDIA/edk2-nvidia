/** @file
*  RCM Boot Dxe
*
*  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
  UINTN                         RcmBlobBase;
  UINTN                         RcmBlobSize;
  UINTN                         OsCarveoutBase;
  UINTN                         OsCarveoutSize;
  UINT8                         BlobMagic[4] = { 'b', 'l', 'o', 'b' };
  UINTN                         Count;
  VOID                          *Hob;
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
    return EFI_SUCCESS;
  }

  RcmBlobBase = PlatformResourceInfo->RcmBlobInfo.Base;
  RcmBlobSize = PlatformResourceInfo->RcmBlobInfo.Size;

  if ((RcmBlobBase == 0) || (RcmBlobSize == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: RCM blob not found\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  RcmBlobHeader = (TEGRABL_BLOBHEADER *)RcmBlobBase;
  if (CompareMem (RcmBlobHeader->BlobMagic, BlobMagic, sizeof (BlobMagic)) != 0) {
    DEBUG ((DEBUG_ERROR, "%a: RCM blob corrupt\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  for (Count = 0; Count < RcmBlobHeader->BlobEntries; Count++) {
    if (RcmBlobHeader->BlobInfo[Count].ImgType == IMAGE_TYPE_KERNEL) {
      break;
    }
  }

  if (Count == RcmBlobHeader->BlobEntries) {
    DEBUG ((DEBUG_ERROR, "%a: OS image not found in RCM blob\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  PcdSet64S (PcdRcmKernelBase, (UINT64)RcmBlobHeader + RcmBlobHeader->BlobInfo[Count].Offset);
  PcdSet64S (PcdRcmKernelSize, RcmBlobHeader->BlobInfo[Count].Size);

  OsCarveoutBase = PlatformResourceInfo->RamdiskOSInfo.Base;
  OsCarveoutSize = PlatformResourceInfo->RamdiskOSInfo.Size;

  if ((OsCarveoutBase != 0) && (OsCarveoutSize != 0) && (OsCarveoutSize >= PcdGet64 (PcdRcmKernelSize))) {
    CopyMem ((VOID *)OsCarveoutBase, (VOID *)PcdGet64 (PcdRcmKernelBase), PcdGet64 (PcdRcmKernelSize));
  }

  return EFI_SUCCESS;
}
