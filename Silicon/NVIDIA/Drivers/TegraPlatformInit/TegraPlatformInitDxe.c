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
#include <Library/MemoryAllocationLib.h>
#include <Guid/RtPropertiesTable.h>
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
  EFI_STATUS              Status;
  UINTN                   ChipID;
  TEGRA_PLATFORM_TYPE     PlatformType;
  BOOLEAN                 SupportEmulatedVariables;
  UINTN                   EmmcMagic;
  EFI_RT_PROPERTIES_TABLE *RtProperties;

  SupportEmulatedVariables = FALSE;

  ChipID = TegraGetChipID();
  DEBUG ((DEBUG_INFO, "%a: Tegra Chip ID:  0x%x\n", __FUNCTION__, ChipID));

  // Set chip specific dynamic Pcds.
  PcdSet64S(PcdSystemMemoryBase, TegraGetSystemMemoryBaseAddress(ChipID));
  PcdSet64S(PcdGicDistributorBase, TegraGetGicDistributorBaseAddress(ChipID));

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

  RtProperties = (EFI_RT_PROPERTIES_TABLE *)AllocatePool (sizeof (EFI_RT_PROPERTIES_TABLE));
  if (RtProperties == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate RT properties table\r\n",__FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }
  RtProperties->Version = EFI_RT_PROPERTIES_TABLE_VERSION;
  RtProperties->Length = sizeof (EFI_RT_PROPERTIES_TABLE);
  if (PcdGetBool (PcdRuntimeVariableServicesSupported)) {
    RtProperties->RuntimeServicesSupported = PcdGet32 (PcdVariableRtProperties);
  } else {
    RtProperties->RuntimeServicesSupported = PcdGet32 (PcdNoVariableRtProperties);
  }
  gBS->InstallConfigurationTable (&gEfiRtPropertiesTableGuid, RtProperties);

  return EFI_SUCCESS;
}
