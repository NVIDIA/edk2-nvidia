/** @file

  SD MMC Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2018-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

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
#include <Protocol/Regulator.h>
#include <Protocol/PlatformToDriverConfiguration.h>
#include <libfdt.h>
#include <PlatformToDriverStructures.h>
#include <Library/TegraPlatformInfoLib.h>

#include "SdMmcControllerPrivate.h"

/**

  Override function for SDHCI capability bits

  @param[in]      ControllerHandle      The EFI_HANDLE of the controller.
  @param[in]      Slot                  The 0 based slot index.
  @param[in,out]  SdMmcHcSlotCapability The SDHCI capability structure.
  @param[in,out]  BaseClkFreq           The base clock frequency value that

  @retval EFI_SUCCESS           The override function completed successfully.
  @retval EFI_NOT_FOUND         The specified controller or slot does not exist.
  @retval EFI_INVALID_PARAMETER SdMmcHcSlotCapability is NULL

**/
EFI_STATUS
SdMmcCapability (
  IN      EFI_HANDLE  ControllerHandle,
  IN      UINT8       Slot,
  IN  OUT VOID        *SdMmcHcSlotCapability,
  IN  OUT UINT32      *BaseClkFreq
  )
{
  EFI_STATUS                        Status;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node;

  SD_MMC_HC_SLOT_CAP  *Capability = (SD_MMC_HC_SLOT_CAP *)SdMmcHcSlotCapability;

  if (SdMmcHcSlotCapability == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Node   = NULL;
  Status = gBS->HandleProtocol (ControllerHandle, &gNVIDIADeviceTreeNodeProtocolGuid, (VOID **)&Node);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (NULL != fdt_get_property (Node->DeviceTreeBase, Node->NodeOffset, "non-removable", NULL)) {
    Capability->SlotType = 0x1; // Embedded slot
  }

  if (PcdGetBool (PcdSdhciHighSpeedDisable)) {
    Capability->Hs400 = 0;
  }

  if (PcdGetBool (PcdSdhciAdma2Disable)) {
    Capability->Adma2 = 0;
  }

  return EFI_SUCCESS;
}

/**

  Override function for SDHCI controller operations

  @param[in]      ControllerHandle      The EFI_HANDLE of the controller.
  @param[in]      Slot                  The 0 based slot index.
  @param[in]      PhaseType             The type of operation and whether the
                                        hook is invoked right before (pre) or
                                        right after (post)
  @param[in,out]  PhaseData             The pointer to a phase-specific data.

  @retval EFI_SUCCESS           The override function completed successfully.
  @retval EFI_NOT_FOUND         The specified controller or slot does not exist.
  @retval EFI_INVALID_PARAMETER PhaseType is invalid
**/
EFI_STATUS
SdMmcNotify (
  IN      EFI_HANDLE               ControllerHandle,
  IN      UINT8                    Slot,
  IN      EDKII_SD_MMC_PHASE_TYPE  PhaseType,
  IN OUT  VOID                     *PhaseData
  )
{
  EFI_PHYSICAL_ADDRESS              SlotBaseAddress = 0;
  UINTN                             SlotSize;
  EFI_STATUS                        Status;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node;
  UINT8                             TapValue;
  UINT8                             TrimValue;

  Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, Slot, &SlotBaseAddress, &SlotSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SdMmcNotify: Unable to locate address range for slot %u\n", Slot));
    return EFI_UNSUPPORTED;
  }

  if (PhaseType == EdkiiSdMmcInitHostPost) {
    Node   = NULL;
    Status = gBS->HandleProtocol (ControllerHandle, &gNVIDIADeviceTreeNodeProtocolGuid, (VOID **)&Node);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: no DT protocol: %r\n", __FUNCTION__, Status));
      return Status;
    }

    if (NULL != fdt_get_property (Node->DeviceTreeBase, Node->NodeOffset, "non-removable", NULL)) {
      TapValue  = SDHCI_TAP_EMBEDDED;
      TrimValue = SDHCI_TRIM_EMBEDDED;
    } else {
      TapValue  = SDHCI_TAP_REMOVABLE;
      TrimValue = SDHCI_TRIM_REMOVABLE;
    }

    // Enable SDMMC Clock again.
    MmioOr32 (SlotBaseAddress + SD_MMC_HC_CLOCK_CTRL, SD_MMC_CLK_CTRL_SD_CLK_EN);
    // Set trim and tap
    MmioBitFieldWrite32 (SlotBaseAddress + SDHCI_TEGRA_VENDOR_CLOCK_CTRL, SDHCI_CLOCK_CTRL_TRIM_START, SDHCI_CLOCK_CTRL_TRIM_END, TrimValue);
    MmioBitFieldWrite32 (SlotBaseAddress + SDHCI_TEGRA_VENDOR_CLOCK_CTRL, SDHCI_CLOCK_CTRL_TAP_START, SDHCI_CLOCK_CTRL_TAP_END, TapValue);
  }

  return EFI_SUCCESS;
}

EDKII_SD_MMC_OVERRIDE  gSdMmcOverride = {
  EDKII_SD_MMC_OVERRIDE_PROTOCOL_VERSION,
  SdMmcCapability,
  SdMmcNotify
};

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,*-sdhci", &gEdkiiNonDiscoverableSdhciDeviceGuid },
  { NULL,             NULL                                  }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NVIDIA SdMmc controller driver",
  .AutoEnableClocks                = TRUE,
  .AutoResetModule                 = TRUE,
  .SkipEdkiiNondiscoverableInstall = FALSE,
  .DisableInRcm                    = TRUE
};

#define MAX_SD_CONTROLLERS  16
STATIC EFI_PHYSICAL_ADDRESS  mSdhciBaseAddress[MAX_SD_CONTROLLERS];
STATIC UINT32                mNumberOfSdhciControllers = 0;

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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                                     Status;
  CONST UINT32                                   *RegulatorPointer;
  NVIDIA_REGULATOR_PROTOCOL                      *RegulatorProtocol;
  NON_DISCOVERABLE_DEVICE                        *Device;
  EFI_PLATFORM_TO_DRIVER_CONFIGURATION_PROTOCOL  *PlatformToDriverInterface;
  SDMMC_PARAMETER_INFO                           *SdMmcParameterInfo;
  SDMMC_PARAMETER_INFO                           SdMmcInfo;
  EFI_GUID                                       *SdMmcParameterInfoGuid;
  UINTN                                          Instance;
  UINTN                                          SdMmcParameterSize;
  UINT64                                         Rate;
  EFI_PHYSICAL_ADDRESS                           BaseAddress;
  UINTN                                          RegionSize;
  CONST CHAR8                                    *ClockName;
  REGULATOR_INFO                                 RegulatorInfo;
  UINT32                                         ClockId;
  CONST UINT32                                   *ClockIds;
  INT32                                          ClocksLength;
  UINT32                                         CurrentController;

  RegulatorPointer          = NULL;
  RegulatorProtocol         = NULL;
  Device                    = NULL;
  PlatformToDriverInterface = NULL;
  SdMmcParameterInfo        = NULL;
  SdMmcParameterInfoGuid    = NULL;
  BaseAddress               = 0;
  ClockIds                  = NULL;

  switch (Phase) {
    case DeviceDiscoveryDriverStart:
      return gBS->InstallMultipleProtocolInterfaces (
                    &DriverHandle,
                    &gEdkiiSdMmcOverrideProtocolGuid,
                    &gSdMmcOverride,
                    NULL
                    );

    case DeviceDiscoveryDriverBindingSupported:
      if (TegraGetPlatform () == TEGRA_PLATFORM_VSP) {
        DEBUG ((DEBUG_ERROR, "%a: VSP detected, skipping SdMmc\n", __FUNCTION__));
        return EFI_UNSUPPORTED;
      }

      return EFI_SUCCESS;

    case DeviceDiscoveryDriverBindingStart:
      Status = gBS->LocateProtocol (&gEfiPlatformToDriverConfigurationProtocolGuid, NULL, (VOID **)&PlatformToDriverInterface);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a, Could not locate Platform to Driver Config protocol %r\r\n", __FUNCTION__, Status));
        return Status;
      }

      // Collect SDMMC DT properties
      Instance = 0;
      Status   = PlatformToDriverInterface->Query (
                                              PlatformToDriverInterface,
                                              ControllerHandle,
                                              NULL,
                                              &Instance,
                                              &SdMmcParameterInfoGuid,
                                              (VOID **)&SdMmcParameterInfo,
                                              &SdMmcParameterSize
                                              );
      if (EFI_ERROR (Status) ||
          (SdMmcParameterInfo == NULL) ||
          (SdMmcParameterInfoGuid == NULL) ||
          (SdMmcParameterSize == 0))
      {
        DEBUG ((DEBUG_ERROR, "%a, Failed to call Query %r\r\n", __FUNCTION__, Status));
        return Status;
      }

      // Sanity check to ensure that the GUID returned is indeed SDMMC
      if (!CompareGuid (SdMmcParameterInfoGuid, &gEdkiiNonDiscoverableSdhciDeviceGuid)) {
        DEBUG ((DEBUG_ERROR, "GUID found does not match SDMMC GUID \r\n"));

        Status = PlatformToDriverInterface->Response (
                                              PlatformToDriverInterface,
                                              ControllerHandle,
                                              NULL,
                                              &Instance,
                                              SdMmcParameterInfoGuid,
                                              (VOID *)SdMmcParameterInfo,
                                              SdMmcParameterSize,
                                              EfiPlatformConfigurationActionUnsupportedGuid
                                              );
        Status = EFI_UNSUPPORTED;
        return Status;
      }

      // Keeping a copy of SDMMC Parameter Info before response is called
      gBS->CopyMem (&SdMmcInfo, (VOID *)SdMmcParameterInfo, sizeof (SDMMC_PARAMETER_INFO));

      Status = PlatformToDriverInterface->Response (
                                            PlatformToDriverInterface,
                                            ControllerHandle,
                                            NULL,
                                            &Instance,
                                            SdMmcParameterInfoGuid,
                                            (VOID *)SdMmcParameterInfo,
                                            SdMmcParameterSize,
                                            EfiPlatformConfigurationActionNone
                                            );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a, Failed to call Response %r\r\n", __FUNCTION__, Status));
        return Status;
      }

      if (PcdGetBool (PcdSdhciCoherentDMADisable)) {
        Status = gBS->HandleProtocol (
                        ControllerHandle,
                        &gNVIDIANonDiscoverableDeviceProtocolGuid,
                        (VOID **)&Device
                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Unable to locate non discoverable device\n", __FUNCTION__));
          return Status;
        }

        Device->DmaType = NonDiscoverableDeviceDmaTypeNonCoherent;
      }

      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range\n", __FUNCTION__));
        return EFI_UNSUPPORTED;
      }

      if (mNumberOfSdhciControllers != MAX_SD_CONTROLLERS) {
        mSdhciBaseAddress[mNumberOfSdhciControllers] = BaseAddress;
        mNumberOfSdhciControllers++;
      }

      // DeviceTreeNode becomes a required argument at this point.
      if (DeviceTreeNode == NULL) {
        ASSERT (FALSE);
        return EFI_INVALID_PARAMETER;
      }

      ClockIds = (CONST UINT32 *)fdt_getprop (
                                   DeviceTreeNode->DeviceTreeBase,
                                   DeviceTreeNode->NodeOffset,
                                   "clocks",
                                   &ClocksLength
                                   );
      if ((ClockIds != NULL) &&
          (ClocksLength != 0))
      {
        ClockName = SDHCI_CLOCK_NAME;
        Status    = DeviceDiscoveryGetClockId (ControllerHandle, ClockName, &ClockId);
        if (EFI_ERROR (Status)) {
          ClockName = SDHCI_CLOCK_OLD_NAME;
        }

        Status = DeviceDiscoveryGetClockId (ControllerHandle, ClockName, &ClockId);
        if (!EFI_ERROR (Status)) {
          Status = DeviceDiscoverySetClockFreq (ControllerHandle, ClockName, SD_MMC_MAX_CLOCK);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "%a, Failed to set clock frequency %r\r\n", __FUNCTION__, Status));
            return Status;
          }

          // Update base clock in capabilities register
          Status = DeviceDiscoveryGetClockFreq (ControllerHandle, ClockName, &Rate);
          if (!EFI_ERROR (Status)) {
            if (Rate > SD_MMC_MAX_CLOCK) {
              DEBUG ((DEBUG_ERROR, "%a: Clock rate %llu out of range for SDHCI\r\n", __FUNCTION__, Rate));
              return EFI_DEVICE_ERROR;
            }

            Rate = Rate / 1000000;
            MmioBitFieldWrite32 (
              BaseAddress + SDHCI_TEGRA_VENDOR_CLOCK_CTRL,
              SDHCI_CLOCK_CTRL_BASE_CLOCK_OVERRIDE_START,
              SDHCI_CLOCK_CTRL_BASE_CLOCK_OVERRIDE_END,
              Rate
              );
          }
        }
      }

      if (PcdGetBool (PcdSdhciHighSpeedDisable)) {
        MmioBitFieldWrite32 (
          BaseAddress + SDHCI_TEGRA_VENDOR_MISC_CTRL,
          SDHCI_MISC_CTRL_ENABLE_SDR50,
          SDHCI_MISC_CTRL_ENABLE_SDR50,
          0
          );
        MmioBitFieldWrite32 (
          BaseAddress + SDHCI_TEGRA_VENDOR_MISC_CTRL,
          SDHCI_MISC_CTRL_ENABLE_DDR50,
          SDHCI_MISC_CTRL_ENABLE_DDR50,
          0
          );
        MmioBitFieldWrite32 (
          BaseAddress + SDHCI_TEGRA_VENDOR_MISC_CTRL,
          SDHCI_MISC_CTRL_ENABLE_SDR104,
          SDHCI_MISC_CTRL_ENABLE_SDR104,
          0
          );
      }

      if (NULL == DeviceTreeNode) {
        return EFI_SUCCESS;
      }

      Status = gBS->LocateProtocol (&gNVIDIARegulatorProtocolGuid, NULL, (VOID **)&RegulatorProtocol);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a, Failed to locate regulator protocol %r\r\n", __FUNCTION__, Status));
        return Status;
      }

      if (SdMmcInfo.VqmmcRegulatorIdPresent) {
        UINTN   Microvolts   = 1800000;
        UINT32  MmcRegulator = SdMmcInfo.VqmmcRegulatorId;
        if (!SdMmcParameterInfo->Only1v8) {
          Microvolts = 3300000;
        }

        Status = RegulatorProtocol->GetInfo (RegulatorProtocol, MmcRegulator, &RegulatorInfo);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a, Failed to get regulator info %x, %r\r\n", __FUNCTION__, MmcRegulator, Status));
          return Status;
        }

        // Check if regulator supports 3.3v if not set voltage to 1.8v to support 1.8v devices that do not have
        // only-1-8-v device tree property
        if (Microvolts > RegulatorInfo.MaxMicrovolts) {
          Microvolts = 1800000;
        }

        if (RegulatorInfo.IsAvailable) {
          if (Microvolts != RegulatorInfo.CurrentMicrovolts) {
            Status = RegulatorProtocol->SetVoltage (RegulatorProtocol, MmcRegulator, Microvolts);
            if (EFI_ERROR (Status)) {
              DEBUG ((DEBUG_ERROR, "%a, Failed to set regulator voltage %x, %u, %r\r\n", __FUNCTION__, MmcRegulator, Microvolts, Status));
              return Status;
            }
          }

          if (!RegulatorInfo.IsEnabled) {
            Status = RegulatorProtocol->Enable (RegulatorProtocol, MmcRegulator, TRUE);
            if (EFI_ERROR (Status)) {
              DEBUG ((DEBUG_ERROR, "%a, Failed to enable regulator %x, %r\r\n", __FUNCTION__, MmcRegulator, Status));
              return Status;
            }
          }
        }
      }

      if (SdMmcInfo.VmmcRegulatorIdPresent) {
        UINT32  MmcRegulator = SdMmcInfo.VmmcRegulatorId;
        Status = RegulatorProtocol->GetInfo (RegulatorProtocol, MmcRegulator, &RegulatorInfo);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a, Failed to get regulator info %x, %r\r\n", __FUNCTION__, MmcRegulator, Status));
          return Status;
        }

        if (RegulatorInfo.IsAvailable) {
          Status = RegulatorProtocol->Enable (RegulatorProtocol, MmcRegulator, TRUE);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "%a, Failed to enable regulator %x, %r\r\n", __FUNCTION__, MmcRegulator, Status));
            return Status;
          }
        }
      }

      return Status;

    case DeviceDiscoveryOnExit:
      if (PcdGetBool (PcdSdhciClearHc2RegOnExit)) {
        for (CurrentController = 0; CurrentController < mNumberOfSdhciControllers; CurrentController++) {
          MmioBitFieldWrite16 (
            mSdhciBaseAddress[CurrentController] + SD_MMC_HC_HOST_CTRL2,
            SD_MMC_HC_V4_EN,
            SD_MMC_HC_V4_EN,
            0
            );
          MmioBitFieldWrite16 (
            mSdhciBaseAddress[CurrentController] + SD_MMC_HC_HOST_CTRL2,
            SD_MMC_HC_64_ADDR_EN,
            SD_MMC_HC_64_ADDR_EN,
            0
            );
          MmioBitFieldWrite16 (
            mSdhciBaseAddress[CurrentController] + SD_MMC_HC_HOST_CTRL2,
            SD_MMC_HC_26_DATA_LEN_ADMA_EN,
            SD_MMC_HC_26_DATA_LEN_ADMA_EN,
            0
            );
        }
      }

      return EFI_SUCCESS;

    default:
      return EFI_SUCCESS;
  }
}
