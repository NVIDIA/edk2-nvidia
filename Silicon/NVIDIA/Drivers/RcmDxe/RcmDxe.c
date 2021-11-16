/** @file
*  RCM Boot Dxe
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
*  Portions provided under the following terms:
*  Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#include "RcmDxe.h"

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
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
  IN EFI_HANDLE               ImageHandle,
  IN EFI_SYSTEM_TABLE         *SystemTable
)
{
  EFI_STATUS         Status;
  TEGRABL_BLOBHEADER *RcmBlobHeader;
  UINT32             RcmBlobSize;
  UINT8              BlobMagic[4] = { 'b' , 'l' , 'o' , 'b' };
  UINTN              Count;

  if (GetBootType () != TegrablBootRcm) {
    return EFI_NOT_FOUND;
  }

  Status = GetCarveoutInfo (TegraRcmCarveout, (UINTN *)RcmBlobHeader, &RcmBlobSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: RCM blob not found\n", __FUNCTION__));
    return Status;
  }

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

  PcdSet64S (PcdRcmKernelBase, (UINT64) RcmBlobHeader + RcmBlobHeader->BlobInfo[Count].Offset);
  PcdSet64S (PcdRcmKernelSize, RcmBlobHeader->BlobInfo[Count].Size);

  return EFI_SUCCESS;
}
