/** @file
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include <Library/DeviceDiscoveryInternalLib.h>

EFI_STATUS
EFIAPI
GetDeviceDiscoveryCompatibleInternal (
  OUT NVIDIA_COMPATIBILITY_INTERNAL   **CompatibleInternal
)
{
  *CompatibleInternal = NULL;

  return EFI_SUCCESS;
}
