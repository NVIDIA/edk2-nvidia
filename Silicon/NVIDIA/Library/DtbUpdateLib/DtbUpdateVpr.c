/** @file

  DTB update for VPR

  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>

#include "DtbUpdateLibPrivate.h"

/**
  update VPR carveout

**/
STATIC
VOID
EFIAPI
DtbUpdateVpr (
  VOID
  )
{
  EFI_STATUS                        Status;
  VOID                              *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO      *PlatformResourceInfo;
  INT32                             ParentOffset;
  INT32                             NodeOffset;
  UINTN                             Socket;
  CONST CHAR8                       *ReservedMemPath = "/reserved-memory";
  CHAR8                             VprPathStr[]     = "vprXX-carveout";
  CHAR8                             *VprPathFormat;
  TEGRA_BASE_AND_SIZE_INFO          *VprInfo;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  RegisterArray[1];

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob == NULL) ||
      (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    DEBUG ((DEBUG_ERROR, "%a: no platform info\n", __FUNCTION__));
    return;
  }

  PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  VprInfo              = PlatformResourceInfo->VprInfo;
  if (VprInfo == NULL) {
    DEBUG ((DEBUG_INFO, "%a: no VPR info\n", __FUNCTION__));
    return;
  }

  Status = DeviceTreeGetNodeByPath (ReservedMemPath, &ParentOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: %a not found: %r\n", __FUNCTION__, ReservedMemPath, Status));
    return;
  }

  for (Socket = 0; Socket < PcdGet32 (PcdTegraMaxSockets); Socket++) {
    if (Socket == 0) {
      VprPathFormat = "vpr-carveout";
    } else {
      VprPathFormat = "vpr%u-carveout";
    }

    AsciiSPrint (VprPathStr, sizeof (VprPathStr), VprPathFormat, Socket);

    Status = DeviceTreeGetNamedSubnode (VprPathStr, ParentOffset, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: no %a node: %r\n", __FUNCTION__, VprPathStr, Status));
      continue;
    }

    if (!(PlatformResourceInfo->SocketMask & (1UL << Socket)) || (VprInfo[Socket].Size == 0)) {
      Status = DeviceTreeSetNodeProperty (NodeOffset, "status", "disabled", sizeof ("disabled"));
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: disable error: %r\n", __FUNCTION__, Status));
      } else {
        DEBUG ((DEBUG_INFO, "%a: VPR CO %u disabled\n", __FUNCTION__, Socket));
      }

      continue;
    }

    RegisterArray[0].BaseAddress = VprInfo[Socket].Base;
    RegisterArray[0].Size        = VprInfo[Socket].Size;
    RegisterArray[0].Name        = NULL;

    Status = DeviceTreeSetRegisters (NodeOffset, RegisterArray, 1);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: error setting %a reg: %r\n", __FUNCTION__, VprPathStr, Status));
      continue;
    }

    Status = DeviceTreeSetNodeProperty (NodeOffset, "status", "okay", sizeof ("okay"));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: error enabling %a: %r\n", __FUNCTION__, VprPathStr, Status));
      continue;
    }

    DEBUG ((DEBUG_INFO, "%a: updated %a reg 0x%llx 0x%llx\n", __FUNCTION__, VprPathStr, RegisterArray[0].BaseAddress, RegisterArray[0].Size));
  }
}

/**
  constructor to register update functions

**/
RETURN_STATUS
EFIAPI
DtbUpdateVprInitialize (
  VOID
  )
{
  DTB_UPDATE_REGISTER_FUNCTION (DtbUpdateVpr, DTB_UPDATE_KERNEL_DTB);

  return RETURN_SUCCESS;
}
