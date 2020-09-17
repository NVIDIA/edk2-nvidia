/** @file
*  RCM Boot Dxe
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "RcmDxe.h"

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PcdLib.h>
#include <Library/IoLib.h>
#include <libfdt.h>


/**
  Install rcm driver.

  @param  ImageHandle     The image handle.
  @param  SystemTable     The system table.

  @retval EFI_SUCEESS     Install Boot manager menu success.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
RcmDxeInitialize (
  IN EFI_HANDLE               ImageHandle,
  IN EFI_SYSTEM_TABLE         *SystemTable
)
{
  EFI_STATUS                      Status;

  return Status;
}
