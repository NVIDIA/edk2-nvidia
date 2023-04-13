/** @file
  Uphy Configuration Dxe

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/HobLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MemoryAllocationLib.h>
#include <TH500/TH500Definitions.h>
#include <TH500/TH500MB1Configuration.h>
#include <Protocol/EmbeddedGpio.h>
#include <libfdt.h>

EFI_STATUS
EFIAPI
Th500UphyConfiguration (
  VOID
  )
{
  EFI_STATUS                    Status;
  VOID                          *Hob;
  TEGRABL_EARLY_BOOT_VARIABLES  *TH500Mb1Config;
  EMBEDDED_GPIO                 *Gpio;
  UINT32                        NumUphyConfig;
  UINT32                        *UphyConfigHandles;
  UINT32                        NumUphyConfigApply;
  UINT32                        *UphyConfigApplyHandles;
  UINT32                        Count;
  VOID                          *Dtb;
  INT32                         NodeOffset;
  CONST VOID                    *Property;
  CONST UINT32                  *Data;
  UINT32                        GpioControllerPhandle;
  UINT32                        GpioNum;
  UINT32                        SocketId;
  UINT32                        UphyId;
  EMBEDDED_GPIO_MODE            GpioMode;
  EMBEDDED_GPIO_PIN             GpioPin;
  BOOLEAN                       ConfigChanged;
  EMBEDDED_GPIO_PIN             *GpioApplyPin;

  UphyConfigHandles      = NULL;
  UphyConfigApplyHandles = NULL;
  GpioApplyPin           = NULL;

  Hob = GetFirstGuidHob (&gNVIDIATH500MB1DataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == (sizeof (TEGRABL_EARLY_BOOT_VARIABLES) * PcdGet32 (PcdTegraMaxSockets))))
  {
    TH500Mb1Config = (TEGRABL_EARLY_BOOT_VARIABLES *)GET_GUID_HOB_DATA (Hob);
  }

  Status = gBS->LocateProtocol (&gEmbeddedGpioProtocolGuid, NULL, (VOID **)&Gpio);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Failed to get embedded gpio protocol. Status = %r\n", Status));
    goto ErrorExit;
  }

  NumUphyConfig = 0;
  Status        = GetMatchingEnabledDeviceTreeNodes ("nvidia,th500-uphy-configuration", NULL, &NumUphyConfig);
  if (Status == EFI_NOT_FOUND) {
    Status = EFI_SUCCESS;
    goto ErrorExit;
  } else if (Status == EFI_BUFFER_TOO_SMALL) {
    UphyConfigHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumUphyConfig);
    if (UphyConfigHandles == NULL) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to allocate buffer for uphy configuration handles.\n"));
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,th500-uphy-configuration", UphyConfigHandles, &NumUphyConfig);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to get uphy configuration dtb node handles. Status = %r\n", Status));
      goto ErrorExit;
    }
  }

  NumUphyConfigApply = 0;
  Status             = GetMatchingEnabledDeviceTreeNodes ("nvidia,th500-uphy-configuration-apply", NULL, &NumUphyConfigApply);
  if (Status == EFI_NOT_FOUND) {
    Status = EFI_SUCCESS;
    goto ErrorExit;
  } else if (Status == EFI_BUFFER_TOO_SMALL) {
    UphyConfigApplyHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumUphyConfigApply);
    if (UphyConfigApplyHandles == NULL) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to allocate buffer for uphy configuration reset handles.\n"));
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    GpioApplyPin = (EMBEDDED_GPIO_PIN *)AllocatePool (sizeof (EMBEDDED_GPIO_PIN) * NumUphyConfigApply);
    if (GpioApplyPin == NULL) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to allocate buffer for gpio apply ping.\n"));
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,th500-uphy-configuration-apply", UphyConfigApplyHandles, &NumUphyConfigApply);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to get uphy configuration dtb node handles. Status = %r\n", Status));
      goto ErrorExit;
    }
  }

  ConfigChanged = FALSE;
  for (Count = 0; Count < NumUphyConfig; Count++) {
    Status = GetDeviceTreeNode (UphyConfigHandles[Count], &Dtb, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to get uphy configuration dtb node information.\n"));
      goto ErrorExit;
    }

    Property = NULL;
    Property = fdt_getprop (Dtb, NodeOffset, "gpio", NULL);
    if (Property == NULL) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to get gpio information from uphy configuration dtb node.\n"));
      Status = EFI_NOT_FOUND;
      goto ErrorExit;
    }

    Data                  = (CONST UINT32 *)Property;
    GpioControllerPhandle = SwapBytes32 (Data[0]);
    GpioNum               = SwapBytes32 (Data[1]);

    Property = NULL;
    Property = fdt_getprop (Dtb, NodeOffset, "nvidia,hw-instance-id", NULL);
    if (Property == NULL) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to get hw instance information from uphy configuration dtb node.\n"));
      Status = EFI_NOT_FOUND;
      goto ErrorExit;
    }

    Data     = (CONST UINT32 *)Property;
    UphyId   = SwapBytes32 (Data[0]) & 0xF;
    SocketId = (SwapBytes32 (Data[0]) >> 4) & 0xF;

    GpioPin = GPIO (GpioControllerPhandle, GpioNum);

    if (TH500Mb1Config->Data.Mb1Data.UphyConfig.UphyConfig[SocketId][UphyId] == UPHY_LANE_BIFURCATION_X16) {
      Status = Gpio->GetMode (Gpio, GpioPin, &GpioMode);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "ERROR: Gpio getmode failed: %r\r\n", Status));
        goto ErrorExit;
      }

      if (GpioMode != GPIO_MODE_INPUT) {
        Status = Gpio->Set (Gpio, GpioPin, GPIO_MODE_INPUT);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "ERROR: Gpio set failed: %r\r\n", Status));
          goto ErrorExit;
        }

        ConfigChanged = TRUE;
      }
    } else if (TH500Mb1Config->Data.Mb1Data.UphyConfig.UphyConfig[SocketId][UphyId] == UPHY_LANE_BIFURCATION_2X8) {
      Status = Gpio->GetMode (Gpio, GpioPin, &GpioMode);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "ERROR: Gpio getmode failed: %r\r\n", Status));
        goto ErrorExit;
      }

      if (GpioMode != GPIO_MODE_OUTPUT_1) {
        Status = Gpio->Set (Gpio, GpioPin, GPIO_MODE_OUTPUT_1);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "ERROR: Gpio set failed: %r\r\n", Status));
          goto ErrorExit;
        }

        ConfigChanged = TRUE;
      }
    }
  }

  if (ConfigChanged) {
    for (Count = 0; Count < NumUphyConfigApply; Count++) {
      Status = GetDeviceTreeNode (UphyConfigApplyHandles[Count], &Dtb, &NodeOffset);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "ERROR: Failed to get uphy configuration apply dtb node information.\n"));
        goto ErrorExit;
      }

      Property = NULL;
      Property = fdt_getprop (Dtb, NodeOffset, "gpio", NULL);
      if (Property == NULL) {
        DEBUG ((DEBUG_ERROR, "ERROR: Failed to get gpio information from uphy configuration dtb node.\n"));
        Status = EFI_NOT_FOUND;
        goto ErrorExit;
      }

      Data                  = (CONST UINT32 *)Property;
      GpioControllerPhandle = SwapBytes32 (Data[0]);
      GpioNum               = SwapBytes32 (Data[1]);

      GpioApplyPin[Count] = GPIO (GpioControllerPhandle, GpioNum);
    }

    for (Count = 0; Count < NumUphyConfigApply; Count++) {
      Status = Gpio->Set (Gpio, GpioApplyPin[Count], GPIO_MODE_INPUT);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "ERROR: Gpio set failed: %r\r\n", Status));
        goto ErrorExit;
      }
    }

    DEBUG ((DEBUG_ERROR, "UPHY Config: 3s delay after powering off all PEX slots power.\n"));
    gBS->Stall (UPHY_LANE_BIFURCATION_DELAY_OFF);

    for (Count = 0; Count < NumUphyConfigApply; Count++) {
      Status = Gpio->Set (Gpio, GpioApplyPin[Count], GPIO_MODE_OUTPUT_1);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "ERROR: Gpio set failed: %r\r\n", Status));
        goto ErrorExit;
      }
    }

    DEBUG ((DEBUG_ERROR, "UPHY Config: : 10s delay after powering on all PEX slots power.\n"));
    gBS->Stall (UPHY_LANE_BIFURCATION_DELAY_ON);
  }

ErrorExit:
  if (UphyConfigHandles != NULL) {
    FreePool (UphyConfigHandles);
  }

  if (UphyConfigApplyHandles != NULL) {
    FreePool (UphyConfigApplyHandles);
  }

  if (GpioApplyPin != NULL) {
    FreePool (GpioApplyPin);
  }

  return Status;
}

/**
  Entrypoint of Uphy Configuration Dxe.

  @param  ImageHandle
  @param  SystemTable

  @return EFI_SUCCESS
  @return EFI_LOAD_ERROR
  @return EFI_OUT_OF_RESOURCES

**/
EFI_STATUS
EFIAPI
UphyConfigurationDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINTN       ChipID;

  ChipID = TegraGetChipID ();

  if (ChipID == TH500_CHIP_ID) {
    Status = Th500UphyConfiguration ();
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR: Th500UphyConfiguration failed. Status = %r\n", Status));
      return Status;
    }
  }

  Status = gBS->InstallMultipleProtocolInterfaces (&ImageHandle, &gNVIDIAUphyConfigurationCompleteGuid, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Failed to install uphy configuration complete protocol. Status = %r\n", Status));
    return Status;
  }

  return Status;
}
