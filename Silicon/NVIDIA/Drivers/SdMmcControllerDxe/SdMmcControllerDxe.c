/** @file

  SD MMC Controller Driver

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Protocol/SdMmcOverride.h>

#include "SdMmcControllerPrivate.h"

/**

  Override function for SDHCI capability bits

  @param[in]      ControllerHandle      The EFI_HANDLE of the controller.
  @param[in]      Slot                  The 0 based slot index.
  @param[in,out]  SdMmcHcSlotCapability The SDHCI capability structure.

  @retval EFI_SUCCESS           The override function completed successfully.
  @retval EFI_NOT_FOUND         The specified controller or slot does not exist.
  @retval EFI_INVALID_PARAMETER SdMmcHcSlotCapability is NULL

**/
EFI_STATUS
SdMmcCapability (
  IN      EFI_HANDLE                      ControllerHandle,
  IN      UINT8                           Slot,
  IN  OUT VOID                            *SdMmcHcSlotCapability
  )
{
  SD_MMC_HC_SLOT_CAP  *Capability = (SD_MMC_HC_SLOT_CAP *)SdMmcHcSlotCapability;
  EFI_STATUS          Status;
  UINT64              Rate;


  if (SdMmcHcSlotCapability == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = DeviceDiscoveryGetClockFreq (ControllerHandle, "sdmmc", &Rate);
  if (!EFI_ERROR (Status)) {
    if (Rate > SD_MMC_MAX_CLOCK) {
      DEBUG ((EFI_D_ERROR, "%a: Clock rate %llu out of range for SDHCI\r\n",__FUNCTION__,Rate));
      return EFI_DEVICE_ERROR;
    }
    Capability->BaseClkFreq = Rate / 1000000;
  }

  Capability->Ddr50 = FALSE;
  Capability->SlotType = 0x1; //Embedded slot

  return EFI_SUCCESS;
}

/**

  Override function for SDHCI controller operations

  @param[in]      ControllerHandle      The EFI_HANDLE of the controller.
  @param[in]      Slot                  The 0 based slot index.
  @param[in]      PhaseType             The type of operation and whether the
                     Ok, the access denied is due to the memory already being                    hook is invoked right before (pre) or
                                        right after (post)

  @retval EFI_SUCCESS           The override function completed successfully.
  @retval EFI_NOT_FOUND         The specified controller or slot does not exist.
  @retval EFI_INVALID_PARAMETER PhaseType is invalid

**/
EFI_STATUS
SdMmcNotify (
  IN      EFI_HANDLE                      ControllerHandle,
  IN      UINT8                           Slot,
  IN      EDKII_SD_MMC_PHASE_TYPE         PhaseType
  )
{
  EFI_PHYSICAL_ADDRESS SlotBaseAddress  = 0;
  UINTN                SlotSize;
  EFI_STATUS           Status;

  Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, Slot, &SlotBaseAddress, &SlotSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SdMmcNotify: Unable to locate address range for slot %d\n", Slot));
    return EFI_UNSUPPORTED;
  }

  if (PhaseType == EdkiiSdMmcInitHostPost) {
    // Enable SDMMC Clock again.
    MmioOr32(SlotBaseAddress + SD_MMC_HC_CLOCK_CTRL, SD_MMC_CLK_CTRL_SD_CLK_EN);
  }

  return EFI_SUCCESS;
}


EDKII_SD_MMC_OVERRIDE gSdMmcOverride = {
  EDKII_SD_MMC_OVERRIDE_PROTOCOL_VERSION,
  SdMmcCapability,
  SdMmcNotify
};

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,tegra186-sdhci", &gEdkiiNonDiscoverableSdhciDeviceGuid },
    { "nvidia,tegra194-sdhci", &gEdkiiNonDiscoverableSdhciDeviceGuid },
    { NULL, NULL }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
    .DriverName = L"NVIDIA SdMmc controller driver",
    .UseDriverBinding = TRUE,
    .AutoEnableClocks = TRUE,
    .AutoSetParents = TRUE,
    .AutoDeassertReset = TRUE,
    .SkipEdkiiNondiscoverableInstall = FALSE
};

/**
  Callback that will be invoked at various phases of the driver initialization

  This function allows for modification of system behavior at various points in
  the driver binding process.

  @param[in] Phase                    Current phase of the driver initialization
  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.
  @param[in] DeviceTreeNode           Pointer to the device tree node protocol is available.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_SUCCESS              Driver does not handle this phase
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryNotify (
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES         Phase,
  IN  EFI_HANDLE                             DriverHandle,
  IN  EFI_HANDLE                             ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode OPTIONAL
  )
{
  switch (Phase) {
  case DeviceDiscoveryDriverStart:
    return gBS->InstallMultipleProtocolInterfaces (
                  &DriverHandle,
                  &gEdkiiSdMmcOverrideProtocolGuid,
                  &gSdMmcOverride,
                  NULL
                  );

  case DeviceDiscoveryDriverBindingStart:
    return DeviceDiscoverySetClockFreq (ControllerHandle, "sdmmc", SD_MMC_MAX_CLOCK);

  default:
    return EFI_SUCCESS;
  }
}
