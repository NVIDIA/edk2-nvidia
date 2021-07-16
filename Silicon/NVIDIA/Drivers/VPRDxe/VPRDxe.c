/** @file
*  Resource Configuration Dxe
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
*  Portions provided under the following terms:
*  Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#include "VPRDxe.h"

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PcdLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Protocol/KernelCmdLineUpdate.h>
#include <libfdt.h>


STATIC
CHAR16 VPRExistingCommandLineArgument[] = L"vpr=";
STATIC
CHAR16 VPRNewCommandLineArgument[VPR_CMDLINE_MAX_LEN];
STATIC
NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL VPRCmdLine;

STATIC
VOID
EFIAPI
OnEndOfDxe (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS Status;
  VOID       *AcpiBase;
  VOID       *FdtBase;
  INTN       NodeOffset;
  CONST VOID *RegProperty;
  UINT64     VPRBase;
  UINT64     VPRSize;
  UINT32     VPRCtrl;
  EFI_HANDLE Handle;

  RegProperty = NULL;
  Handle = NULL;

  gBS->CloseEvent (Event);

  VPRBase = ((UINT64) MmioRead32 (PcdGet64 (PcdTegraMCBBaseAddress) + MC_VIDEO_PROTECT_BOM_ADR_HI_0) << 32) |
              MmioRead32 (PcdGet64 (PcdTegraMCBBaseAddress) + MC_VIDEO_PROTECT_BOM_0);

  VPRSize = MmioRead32 (PcdGet64 (PcdTegraMCBBaseAddress) + MC_VIDEO_PROTECT_SIZE_MB_0);
  VPRSize <<= 20;

  VPRCtrl = MmioRead32 (PcdGet64 (PcdTegraMCBBaseAddress) + MC_VIDEO_PROTECT_REG_CTRL_0);

  if (VPRBase == 0 && VPRSize == 0 &&
      (VPRCtrl & VIDEO_PROTECT_ALLOW_TZ_WRITE_ACCESS_BMSK)) {
    UnicodeSPrint (VPRNewCommandLineArgument, VPR_CMDLINE_MAX_LEN, L"vpr_resize");

    VPRCmdLine.ExistingCommandLineArgument = VPRExistingCommandLineArgument;
    VPRCmdLine.NewCommandLineArgument = VPRNewCommandLineArgument;

    Status = gBS->InstallMultipleProtocolInterfaces (&Handle, &gNVIDIAKernelCmdLineUpdateGuid, &VPRCmdLine, NULL);
    if (EFI_ERROR(Status)) {
      return;
    }

    DEBUG ((DEBUG_INFO, "%a: VPR Resize Requested\n", __FUNCTION__));

    return;
  }

  if (VPRSize != 0 &&
      !(VPRCtrl & VIDEO_PROTECT_ALLOW_TZ_WRITE_ACCESS_BMSK)) {
    UnicodeSPrintAsciiFormat (VPRNewCommandLineArgument, VPR_CMDLINE_MAX_LEN, "vpr=0x%lx@0x%lx", VPRSize, VPRBase);

    VPRCmdLine.ExistingCommandLineArgument = VPRExistingCommandLineArgument;
    VPRCmdLine.NewCommandLineArgument = VPRNewCommandLineArgument;

    Status = gBS->InstallMultipleProtocolInterfaces (&Handle, &gNVIDIAKernelCmdLineUpdateGuid, &VPRCmdLine, NULL);
    if (EFI_ERROR(Status)) {
      return;
    }

    DEBUG ((DEBUG_INFO, "%a: VPR Region Requested\n", __FUNCTION__));

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

    NodeOffset = fdt_node_offset_by_compatible (FdtBase, 0, "nvidia,vpr-carveout");
    if (NodeOffset < 0) {
      return;
    }

    fdt_del_node (FdtBase, NodeOffset);
    DEBUG ((DEBUG_INFO, "%a: VPR Node Deleted\n", __FUNCTION__));
    return;
  }
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
  IN EFI_HANDLE               ImageHandle,
  IN EFI_SYSTEM_TABLE         *SystemTable
)
{
  EFI_STATUS                      Status;
  EFI_EVENT                       EndOfDxeEvent;

  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                               TPL_CALLBACK,
                               OnEndOfDxe,
                               NULL,
                               &gEfiEndOfDxeEventGroupGuid,
                               &EndOfDxeEvent);

  return Status;
}
