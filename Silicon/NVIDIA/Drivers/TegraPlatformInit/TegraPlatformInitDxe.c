/** @file

  Tegra Platform Init Driver.

  Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
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
  UINTN ChipID;

  ChipID = TegraGetChipID();
  DEBUG ((DEBUG_INFO, "%a: Tegra Chip ID:  0x%x\n", __FUNCTION__, ChipID));

  // Set chip specific dynamic Pcds.
  PcdSet64S(PcdSystemMemoryBase, TegraGetSystemMemoryBaseAddress(ChipID));
  PcdSet64S(PcdGicDistributorBase, TegraGetGicDistributorBaseAddress(ChipID));

  // Set GIC specific PCDs
  if (ChipID == T186_CHIP_ID || ChipID == T194_CHIP_ID) {
    // Used in GICv2
    PcdSet64S(PcdGicInterruptInterfaceBase, TegraGetGicInterruptInterfaceBaseAddress(ChipID));
  } else if (ChipID == T234_CHIP_ID || ChipID == TH500_CHIP_ID) {
    // Used in GICv3
    PcdSet64S(PcdGicRedistributorsBase, TegraGetGicRedistributorBaseAddress(ChipID));
  }

  // Set PWM specific PCDs
  if (ChipID == T194_CHIP_ID) {
    PcdSet64S(PcdTegraPwmFanBase, FixedPcdGet64 (PcdTegraPwmFanT194Base));
  }

  switch (TegraGetPlatform()) {
    case TEGRA_PLATFORM_VDK:
      DEBUG ((DEBUG_INFO, "%a: Tegra Platform:  Simulation/VDK\n", __FUNCTION__));
      break;
    case TEGRA_PLATFORM_SYSTEM_FPGA:
      DEBUG ((DEBUG_INFO, "%a: Tegra Platform:  System FPGA\n", __FUNCTION__));
      // Enable emulated variable NV mode in variable driver.
      PcdSetBoolS(PcdEmuVariableNvModeEnable, TRUE);
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
