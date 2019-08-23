/** @file

  Tegra Platform Init Driver.

  Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TegraPlatformInfoLib.h>


/**
  Runtime Configuration Of Tegra Platform.
**/
EFI_STATUS
EFIAPI
TegraPlatformInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;
  DEBUG ((DEBUG_INFO, "%a: Tegra Chip ID:  0x%x\n", __FUNCTION__, TegraGetChipID()));
  switch (TegraGetPlatform()) {
    case TEGRA_PLATFORM_VDK:
      DEBUG ((DEBUG_INFO, "%a: Tegra Platform:  Simulation/VDK\n", __FUNCTION__));
      break;
    case TEGRA_PLATFORM_SYSTEM_FPGA:
      DEBUG ((DEBUG_INFO, "%a: Tegra Platform:  System FPGA\n", __FUNCTION__));
      PcdSetBool(PcdRamLoadedKernelSupport, TRUE);
      // Enable emulated variable NV mode in variable driver.
      PcdSetBool(PcdEmuVariableNvModeEnable, TRUE);
      Status = gBS->InstallMultipleProtocolInterfaces (
                 &ImageHandle,
                 &gNVIDIAEmuVariableNvModeEnableProtocolGuid,
                 NULL,
                 NULL
                 );
      if (EFI_ERROR(Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error installing EmuVariableNvModeEnableProtocol\n", __FUNCTION__));
      }
      break;
    default:
      break;
  };

  return EFI_SUCCESS;
}
