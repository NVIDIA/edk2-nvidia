/** @file
*  Resource Configuration Dxe
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "VPRDxe.h"

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PcdLib.h>
#include <Library/IoLib.h>
#include <libfdt.h>

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
  EFI_STATUS  Status;
  VOID        *AcpiBase;
  VOID        *FdtBase;
  INTN        NodeOffset;
  UINT64      VPRBase;
  UINT64      VPRSize;
  INT32       AddressCells;
  INT32       SizeCells;
  UINT8       *Data;

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

  VPRBase = ((UINT64)MmioRead32 (PcdGet64 (PcdTegraMCBBaseAddress) + MC_VIDEO_PROTECT_BOM_ADR_HI_0) << 32) |
            MmioRead32 (PcdGet64 (PcdTegraMCBBaseAddress) + MC_VIDEO_PROTECT_BOM_0);

  VPRSize   = MmioRead32 (PcdGet64 (PcdTegraMCBBaseAddress) + MC_VIDEO_PROTECT_SIZE_MB_0);
  VPRSize <<= 20;

  if ((VPRBase == 0) && (VPRSize == 0)) {
    fdt_del_node (FdtBase, NodeOffset);
    DEBUG ((DEBUG_INFO, "%a: VPR Node Deleted\n", __FUNCTION__));
  } else {
    AddressCells = fdt_address_cells (FdtBase, fdt_parent_offset (FdtBase, NodeOffset));
    SizeCells    = fdt_size_cells (FdtBase, fdt_parent_offset (FdtBase, NodeOffset));
    if ((AddressCells > 2) ||
        (AddressCells == 0) ||
        (SizeCells > 2) ||
        (SizeCells == 0))
    {
      DEBUG ((DEBUG_ERROR, "%a: Bad cell values, %d, %d\r\n", __FUNCTION__, AddressCells, SizeCells));
      return;
    }

    Data   = NULL;
    Status = gBS->AllocatePool (
                    EfiBootServicesData,
                    (AddressCells + SizeCells) * sizeof (UINT32),
                    (VOID **)&Data
                    );
    if (EFI_ERROR (Status)) {
      return;
    }

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

    gBS->FreePool (Data);
  }

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
  return gBS->CreateEventEx (
                EVT_NOTIFY_SIGNAL,
                TPL_CALLBACK,
                FdtInstalled,
                NULL,
                &gFdtTableGuid,
                &FdtInstallEvent
                );
}
