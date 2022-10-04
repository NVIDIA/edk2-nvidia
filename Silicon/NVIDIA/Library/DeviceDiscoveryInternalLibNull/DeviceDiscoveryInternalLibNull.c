/** @file
*
*  Copyright (c) 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/DeviceDiscoveryInternalLib.h>

EFI_STATUS
EFIAPI
GetDeviceDiscoveryCompatibleInternal (
  OUT NVIDIA_COMPATIBILITY_INTERNAL  **CompatibleInternal
  )
{
  *CompatibleInternal = NULL;

  return EFI_SUCCESS;
}
