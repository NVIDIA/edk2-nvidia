/** @file
*  Functional Safety Island Dxe
*
*  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PlatformResourceLib.h>
#include <libfdt.h>


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
  UINTN      FsiBase;
  UINTN      FsiSize;
  INT32      AddressCells;
  INT32      SizeCells;
  UINT8      *Data;

  gBS->CloseEvent (Event);

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

  NodeOffset = fdt_node_offset_by_compatible (FdtBase, 0, "nvidia,fsi-carveout");
  if (NodeOffset < 0) {
    return;
  }

  AddressCells  = fdt_address_cells (FdtBase, fdt_parent_offset(FdtBase, NodeOffset));
  SizeCells = fdt_size_cells (FdtBase, fdt_parent_offset(FdtBase, NodeOffset));
  if ((AddressCells > 2) ||
      (AddressCells == 0) ||
      (SizeCells > 2) ||
      (SizeCells == 0)) {
    return;
  }

  FsiBase = 0;
  FsiSize = 0;
  if (!GetFsiNsBaseAndSize (&FsiBase, &FsiSize)) {
    fdt_del_node (FdtBase, NodeOffset);
    return;
  }

  if (FsiSize == 0 || FsiBase == 0) {
    fdt_del_node (FdtBase, NodeOffset);
    return;
  }

  Data = NULL;
  Status = gBS->AllocatePool (EfiBootServicesData,
                              (AddressCells + SizeCells) * sizeof (UINT32),
                              (VOID **)&Data);
  if (EFI_ERROR (Status)) {
    return;
  }

  if (AddressCells == 2) {
    *(UINT64*)Data = SwapBytes64 (FsiBase);
  } else {
    *(UINT32*)Data = SwapBytes32 (FsiBase);
  }

  if (SizeCells == 2) {
    *(UINT64*)&Data[AddressCells * sizeof (UINT32)] = SwapBytes64 (FsiSize);
  } else {
    *(UINT32*)&Data[AddressCells * sizeof (UINT32)] = SwapBytes32 (FsiSize);
  }

  fdt_setprop (FdtBase, NodeOffset, "reg", Data, (AddressCells + SizeCells) * sizeof (UINT32));

  gBS->FreePool (Data);

  return;
}


/**
  Install FSI driver.

  @param  ImageHandle     The image handle.
  @param  SystemTable     The system table.

  @retval EFI_SUCEESS     Install Boot manager menu success.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
FSIDxeInitialize (
  IN EFI_HANDLE               ImageHandle,
  IN EFI_SYSTEM_TABLE         *SystemTable
)
{
  EFI_EVENT  EndOfDxeEvent;

  return gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                             TPL_CALLBACK,
                             OnEndOfDxe,
                             NULL,
                             &gEfiEndOfDxeEventGroupGuid,
                             &EndOfDxeEvent);
}
