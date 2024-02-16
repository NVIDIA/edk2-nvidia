/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2017, Linaro, Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiRuntimeServicesTableLib.h>

/**
  The entry point for DtPlatformDxe driver.

  @param[in] ImageHandle     The image handle of the driver.
  @param[in] SystemTable     The system table.

  @retval EFI_ALREADY_STARTED     The driver already exists in system.
  @retval EFI_OUT_OF_RESOURCES    Fail to execute entry point due to lack of
                                  resources.
  @retval EFI_SUCCESS             All the related protocols are installed on
                                  the driver.

**/
EFI_STATUS
EFIAPI
DtPlatformDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  VOID        *Dtb;
  UINTN       DtbSize;

  Dtb    = NULL;
  Status = DtPlatformLoadDtb (&Dtb, &DtbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: no DTB blob could be loaded - %r\n",
      __func__,
      Status
      ));
    ASSERT (FALSE);
    return Status;
  }

  Status = gBS->InstallConfigurationTable (&gFdtTableGuid, Dtb);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to install FDT configuration table -%r\n",
      __func__,
      Status
      ));
    if (Dtb != NULL) {
      FreePool (Dtb);
    }

    ASSERT (FALSE);
  }

  return Status;
}
