/** @file
  Tegra Device Tree Overlay Library

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Uefi.h>
#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <libfdt.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraDeviceTreeOverlayLib.h>
#include <Protocol/Eeprom.h>
#include "TegraDeviceTreeOverlayLibCommon.h"

STATIC
EFI_STATUS
ReadBoardInfo (
  VOID                *Fdt,
  OVERLAY_BOARD_INFO  *BoardInfo
  )
{
  TEGRA_BOARD_INFO  *TegraBoardInfo;
  VOID              *Hob;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    TegraBoardInfo = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->BoardInfo;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: No board info hob found\r\n", __FUNCTION__));
    ASSERT (FALSE);
    return EFI_DEVICE_ERROR;
  }

  BoardInfo->FuseBaseAddr = TegraBoardInfo->FuseBaseAddr;
  BoardInfo->FuseList     = TegraBoardInfo->FuseList;
  BoardInfo->FuseCount    = TegraBoardInfo->FuseCount;
  BoardInfo->IdCount      = 2; /*CVM and CVB*/
  BoardInfo->ProductIds   = (EEPROM_PART_NUMBER *)AllocateZeroPool (BoardInfo->IdCount * sizeof (EEPROM_PART_NUMBER));
  BoardInfo->EepromDeviceTreePaths   = (CHAR8 **)AllocateZeroPool (BoardInfo->IdCount * sizeof (CHAR8*));
  CopyMem ((VOID *)&BoardInfo->ProductIds[0], (VOID *)TegraBoardInfo->CvmProductId, TEGRA_PRODUCT_ID_LEN);
  CopyMem ((VOID *)&BoardInfo->ProductIds[1], (VOID *)TegraBoardInfo->CvbProductId, TEGRA_PRODUCT_ID_LEN);

  DEBUG ((DEBUG_INFO, "Cvm Product Id: %a \n", (CHAR8 *)TegraBoardInfo->CvmProductId));
  DEBUG ((DEBUG_INFO, "Cvb Product Id: %a \n", (CHAR8 *)TegraBoardInfo->CvbProductId));

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ApplyTegraDeviceTreeOverlay (
  VOID   *FdtBase,
  VOID   *FdtOverlay,
  CHAR8  *ModuleStr
  )
{
  EFI_STATUS          Status;
  OVERLAY_BOARD_INFO  BoardInfo;

  Status = ReadBoardInfo (FdtBase, &BoardInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "Warning: Failed to read board config.\n"));
  }

  Status = ApplyTegraDeviceTreeOverlayCommon (FdtBase, FdtOverlay, ModuleStr, &BoardInfo);

  if (BoardInfo.IdCount > 0) {
    FreePool (BoardInfo.ProductIds);
  }

  return Status;
}
