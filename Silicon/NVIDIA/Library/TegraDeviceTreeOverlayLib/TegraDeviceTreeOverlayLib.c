/** @file
  Tegra Device Tree Overlay Library

  Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
**/

#include <Base.h>
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <libfdt.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraDeviceTreeOverlayLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Protocol/Eeprom.h>
#include "TegraDeviceTreeOverlayLibCommon.h"


STATIC
EFI_STATUS
ReadBoardInfo (
  VOID *Fdt,
  OVERLAY_BOARD_INFO *BoardInfo
)
{
  TEGRA_BOARD_INFO TegraBoardInfo;

  ZeroMem (&TegraBoardInfo, sizeof (TegraBoardInfo));
  GetBoardInfo(&TegraBoardInfo);

  BoardInfo->FuseBaseAddr = TegraBoardInfo.FuseBaseAddr;
  BoardInfo->FuseList = TegraBoardInfo.FuseList;
  BoardInfo->FuseCount = TegraBoardInfo.FuseCount;
  BoardInfo->IdCount = 2; /*CVM and CVB*/
  BoardInfo->ProductIds = (TEGRA_EEPROM_PART_NUMBER *)AllocateZeroPool(BoardInfo->IdCount * sizeof(TEGRA_EEPROM_PART_NUMBER));
  CopyMem ((VOID *)&BoardInfo->ProductIds[0], (VOID *) TegraBoardInfo.CvmProductId, PRODUCT_ID_LEN);
  CopyMem ((VOID *)&BoardInfo->ProductIds[1], (VOID *) TegraBoardInfo.CvbProductId, PRODUCT_ID_LEN);

  DEBUG((DEBUG_INFO, "Cvm Product Id: %a \n", (CHAR8*)TegraBoardInfo.CvmProductId));
  DEBUG((DEBUG_INFO, "Cvb Product Id: %a \n", (CHAR8*)TegraBoardInfo.CvbProductId));

  if (TegraBoardInfo.CvmBoardId == NULL) {
    DEBUG((DEBUG_WARN, "%a: Failed to get board id from BCT\n.", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ApplyTegraDeviceTreeOverlay (
  VOID *FdtBase,
  VOID *FdtOverlay,
  CHAR8 *ModuleStr
  )
{
  EFI_STATUS  Status;
  OVERLAY_BOARD_INFO BoardInfo;


  Status = ReadBoardInfo(FdtBase, &BoardInfo);
  if (EFI_ERROR(Status)) {
    DEBUG((DEBUG_INFO, "Warning: Failed to read board config.\n"));
  }

  Status = ApplyTegraDeviceTreeOverlayCommon(FdtBase, FdtOverlay, ModuleStr, &BoardInfo);

  if (BoardInfo.IdCount > 0) {
    FreePool(BoardInfo.ProductIds);
  }
  return Status;
}
