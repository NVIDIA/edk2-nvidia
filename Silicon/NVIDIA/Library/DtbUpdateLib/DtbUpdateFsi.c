/** @file

  DTB update for FSI

  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/HobLib.h>
#include <Library/PlatformResourceLib.h>

#include "DtbUpdateLibPrivate.h"

STATIC CONST CHAR8  *FsiCompatibility[] = {
  "nvidia,fsi-carveout",
  NULL
};

/**
  update FSI carveout

**/
STATIC
VOID
EFIAPI
DtbUpdateFsi (
  VOID
  )
{
  EFI_STATUS                        Status;
  INT32                             NodeOffset;
  UINTN                             FsiBase = 0;
  UINTN                             FsiSize = 0;
  VOID                              *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO      *PlatformResourceInfo;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  RegisterArray[1];

  NodeOffset = 0;
  Status     = DeviceTreeGetNextCompatibleNode (FsiCompatibility, &NodeOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: no %a node: %r\n", __FUNCTION__, FsiCompatibility[0], Status));
    return;
  }

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
    FsiBase              = PlatformResourceInfo->FsiNsInfo.Base;
    FsiSize              = PlatformResourceInfo->FsiNsInfo.Size;
  }

  if ((FsiSize == 0) || (FsiBase == 0)) {
    Status = DeviceTreeSetNodeProperty (NodeOffset, "status", "disabled", sizeof ("disabled"));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: disable error: %r\n", __FUNCTION__, Status));
    } else {
      DEBUG ((DEBUG_INFO, "%a: no fsi info, disabled node\n", __FUNCTION__));
    }

    return;
  }

  RegisterArray[0].BaseAddress = FsiBase;
  RegisterArray[0].Size        = FsiSize;
  RegisterArray[0].Name        = NULL;

  Status = DeviceTreeSetRegisters (NodeOffset, RegisterArray, 1);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: error setting reg: %r\n", __FUNCTION__, Status));
    return;
  }

  DEBUG ((DEBUG_INFO, "%a: updated reg 0x%llx 0x%llx\n", __FUNCTION__, RegisterArray[0].BaseAddress, RegisterArray[0].Size));
}

/**
  constructor to register update functions

**/
RETURN_STATUS
EFIAPI
DtbUpdateFsiInitialize (
  VOID
  )
{
  DTB_UPDATE_REGISTER_FUNCTION (DtbUpdateFsi, DTB_UPDATE_KERNEL_DTB);

  return RETURN_SUCCESS;
}
