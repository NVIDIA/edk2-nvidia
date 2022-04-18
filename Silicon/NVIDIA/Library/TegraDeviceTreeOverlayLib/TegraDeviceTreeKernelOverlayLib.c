/** @file
  Tegra Device Tree Overlay Library

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Uefi.h>
#include <PiDxe.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <libfdt.h>
#include <Library/UefiBootServicesTableLib.h>
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
  EFI_STATUS                  Status;
  TEGRA_BOARD_INFO            *TegraBoardInfo;
  TEGRA_EEPROM_BOARD_INFO     *Eeprom;
  EFI_HANDLE                  *HandleBuffer;
  UINTN                       ProtocolCount;
  UINTN                       i=0;
  VOID                        *Hob;

  Eeprom = NULL;
  HandleBuffer = NULL;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
    (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO))) {
    TegraBoardInfo = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->BoardInfo;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: No board info hob found\r\n", __FUNCTION__));
    ASSERT (FALSE);
    return EFI_DEVICE_ERROR;
  }

  BoardInfo->FuseBaseAddr = TegraBoardInfo->FuseBaseAddr;
  BoardInfo->FuseList = TegraBoardInfo->FuseList;
  BoardInfo->FuseCount = TegraBoardInfo->FuseCount;
  BoardInfo->IdCount = 0;

  Status = gBS->LocateHandleBuffer ( ByProtocol,
                                     &gNVIDIAEepromProtocolGuid,
                                     NULL,
                                     &ProtocolCount,
                                     &HandleBuffer
                                   );
  if (EFI_ERROR(Status) || (ProtocolCount == 0)) {
    DEBUG ((DEBUG_WARN, "Failed to get ID Eeprom protocol\r\n"));
    return Status;
  }

  BoardInfo->IdCount = ProtocolCount;
  BoardInfo->ProductIds = (TEGRA_EEPROM_PART_NUMBER *)AllocateZeroPool(BoardInfo->IdCount * sizeof(TEGRA_EEPROM_PART_NUMBER));

  for (i = 0; i < ProtocolCount; i++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[i],
                    &gNVIDIAEepromProtocolGuid,
                    (VOID **) &Eeprom);
    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_WARN, "Failed to get Eeprom protocol\r\n"));
      return EFI_NOT_FOUND;
    }
    CopyMem ((VOID *)(&BoardInfo->ProductIds[i]), (VOID *) &Eeprom->ProductId, PRODUCT_ID_LEN);
  }

  if (BoardInfo->IdCount == 0) {
    DEBUG((DEBUG_WARN, "%a: Failed to get board id from EEPROM\n.", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  DEBUG((DEBUG_INFO, "Eeprom product Ids: \n"));
  for (i=0; i < BoardInfo->IdCount; i++) {
    DEBUG((DEBUG_INFO, "%d. %a \n", i+1, BoardInfo->ProductIds[i]));
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
