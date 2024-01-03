/** @file
*  Resource Configuration Dxe
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/HobLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>

#define DTB_VPR_CARVEOUT_SOCKET_MAX  100

STATIC
EFI_EVENT  FdtInstallEvent;

STATIC
VOID
EFIAPI
FdtInstalled (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                    Status;
  VOID                          *AcpiBase;
  VOID                          *FdtBase;
  INT32                         NodeOffset;
  UINT64                        VPRBase;
  UINT64                        VPRSize;
  INT32                         AddressCells;
  INT32                         SizeCells;
  UINT8                         *Data;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;
  UINTN                         Socket;
  CONST CHAR8                   *ReservedMemPath = "/reserved-memory";
  CHAR8                         VprPathStr[]     = "vprXX-carveout";
  TEGRA_BASE_AND_SIZE_INFO      *VprInfo;
  INT32                         ParentOffset;
  CHAR8                         *VprPathFormat;

  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    return;
  }

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &FdtBase);
  if (EFI_ERROR (Status)) {
    return;
  }

  if (fdt_check_header (FdtBase) != 0) {
    return;
  }

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

  ParentOffset = fdt_path_offset (FdtBase, ReservedMemPath);
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_INFO, "%a: %a not found err=%d\n", __FUNCTION__, ReservedMemPath, ParentOffset));
    return;
  }

  AddressCells = fdt_address_cells (FdtBase, ParentOffset);
  SizeCells    = fdt_size_cells (FdtBase, ParentOffset);
  if ((SizeCells <= 0) || (AddressCells <= 0) || (SizeCells > 2) || (AddressCells > 2)) {
    DEBUG ((DEBUG_ERROR, "%a: %a error addr=%d, size=%d\n", __FUNCTION__, ReservedMemPath, AddressCells, SizeCells));
    return;
  }

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  (AddressCells + SizeCells) * sizeof (UINT32),
                  (VOID **)&Data
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: alloc failed\n", __FUNCTION__));
    return;
  }

  for (Socket = 0; Socket < DTB_VPR_CARVEOUT_SOCKET_MAX; Socket++) {
    if (Socket == 0) {
      VprPathFormat = "vpr-carveout";
    } else {
      VprPathFormat = "vpr%u-carveout";
    }

    AsciiSPrint (VprPathStr, sizeof (VprPathStr), VprPathFormat, Socket);
    NodeOffset = fdt_subnode_offset (FdtBase, ParentOffset, VprPathStr);
    if (NodeOffset < 0) {
      DEBUG ((DEBUG_INFO, "%a: %a node missing\n", __FUNCTION__, VprPathStr));
      break;
    }

    if (!(PlatformResourceInfo->SocketMask & (1UL << Socket)) || (VprInfo[Socket].Size == 0)) {
      fdt_del_node (FdtBase, NodeOffset);
      DEBUG ((DEBUG_INFO, "%a: VPR Node Deleted\n", __FUNCTION__));

      continue;
    }

    VPRBase = VprInfo[Socket].Base;
    VPRSize = VprInfo[Socket].Size;

    if (AddressCells == 2) {
      *(UINT64 *)Data = SwapBytes64 (VPRBase);
    } else {
      *(UINT32 *)Data = SwapBytes32 (VPRBase);
    }

    if (SizeCells == 2) {
      *(UINT64 *)&Data[AddressCells * sizeof (UINT32)] = SwapBytes64 (VPRSize);
    } else {
      *(UINT32 *)&Data[AddressCells * sizeof (UINT32)] = SwapBytes32 (VPRSize);
    }

    fdt_setprop (FdtBase, NodeOffset, "reg", Data, (AddressCells + SizeCells) * sizeof (UINT32));
    fdt_setprop (FdtBase, NodeOffset, "status", "okay", sizeof ("okay"));

    DEBUG ((DEBUG_INFO, "%a: updated %a reg 0x%llx 0x%llx\n", __FUNCTION__, VprPathStr, VPRBase, VPRSize));
  }

  gBS->FreePool (Data);

  return;
}

/**
  Install VPR driver.

  @param  ImageHandle     The image handle.
  @param  SystemTable     The system table.

  @retval EFI_SUCEESS     Install Boot manager menu success.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
VPRDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  FdtInstalled (NULL, NULL);

  return gBS->CreateEventEx (
                EVT_NOTIFY_SIGNAL,
                TPL_CALLBACK,
                FdtInstalled,
                NULL,
                &gFdtTableGuid,
                &FdtInstallEvent
                );
}
