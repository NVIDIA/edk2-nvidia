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
#include "TegraPlatformInitDxePrivate.h"

STATIC
EFI_STATUS
EFIAPI
UseEmulatedVariableStore (
  IN EFI_HANDLE        ImageHandle
  )
{
  EFI_STATUS Status;

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

  return Status;
}


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
  EFI_STATUS          Status;
  UINTN               ChipID;
  TEGRA_PLATFORM_TYPE PlatformType;
  BOOLEAN             SupportEmulatedVariables;
  UINTN               EmmcMagic;

  SupportEmulatedVariables = FALSE;

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
  } else {
    return EFI_UNSUPPORTED;
  }

  // Set PCI specific PCDs
  if (ChipID == T194_CHIP_ID) {
    PcdSet64S(PcdPciConfigurationSpaceBaseAddress, 0x30000000);
    PcdSet32S(PcdPciBusMin, 160);
    PcdSet32S(PcdPciBusMax, 161);
  } else if (ChipID == TH500_CHIP_ID) {
    PcdSet64S(PcdPciConfigurationSpaceBaseAddress, 0x2c800000);
    PcdSet32S(PcdPciBusMin, 0);
    PcdSet32S(PcdPciBusMax, 1);
  } else {
    // PCI is not supported on all targets. Do not return any error.
  }

  // Set Default OEM Table ID specific PCDs
  if (ChipID == T186_CHIP_ID) {
    PcdSet64S(PcdAcpiDefaultOemTableId, 0x2020202036383154);
  } else if (ChipID == T194_CHIP_ID) {
    PcdSet64S(PcdAcpiDefaultOemTableId, 0x2020202034393154);
  } else if (ChipID == T234_CHIP_ID) {
    PcdSet64S(PcdAcpiDefaultOemTableId, 0x2020202034333254);
  } else if (ChipID == TH500_CHIP_ID) {
    PcdSet64S(PcdAcpiDefaultOemTableId, 0x2020203030354854);
  } else {
    return EFI_UNSUPPORTED;
  }

  // Set Tegra PWM Fan Base
  if (ChipID == T194_CHIP_ID) {
    PcdSet64S(PcdTegraPwmFanBase, FixedPcdGet64 (PcdTegraPwmFanT194Base));
  } else {
    // PWM is not supported on all targets. Do not return any error.
  }

  PlatformType = TegraGetPlatform();
  if (PlatformType != TEGRA_PLATFORM_SILICON) {
    // Override boot timeout for pre-si platforms
    PcdSet16S(PcdPlatformBootTimeOut, PRE_SI_PLATFORM_BOOT_TIMEOUT);
    EmmcMagic = * ((UINTN *) (TegraGetSystemMemoryBaseAddress(ChipID) + SYSIMG_EMMC_MAGIC_OFFSET));
    if ((EmmcMagic != SYSIMG_EMMC_MAGIC) && (EmmcMagic == SYSIMG_DEFAULT_MAGIC)) {
      // Enable emulated variable NV mode in variable driver when ram loading images and emmc
      // is not enabled.
      SupportEmulatedVariables = TRUE;
    }
  }

  if (SupportEmulatedVariables) {
    Status = UseEmulatedVariableStore (ImageHandle);
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}
