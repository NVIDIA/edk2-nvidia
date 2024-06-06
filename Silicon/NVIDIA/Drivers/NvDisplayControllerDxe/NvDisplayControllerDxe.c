/** @file

  NV Display Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2021-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DisplayDeviceTreeHelperLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/ClockNodeProtocol.h>
#include <Protocol/EmbeddedGpio.h>

#include <libfdt.h>
#include <NVIDIAConfiguration.h>

#include "NvDisplay.h"

#define DISPLAY_SOR_COUNT      8
#define DISPLAY_FE_SW_SYS_CAP  0x00030000
#define DISPLAY_FE_SW_SYS_CAP_SOR_EXISTS_GET(x, i)     (BOOLEAN)(BitFieldRead32 ((x), 8 + (i), 8 + (i)) != 0)
#define DISPLAY_FE_CMGR_CLK_SOR(i)                     (0x00002300 + (i) * SIZE_2KB)
#define DISPLAY_FE_CMGR_CLK_SOR_MODE_BYPASS_SET(x, v)  BitFieldWrite32 ((x), 16, 17, (v))
#define DISPLAY_FE_CMGR_CLK_SOR_MODE_BYPASS_DP_SAFE  2

#define DISPLAY_CONTROLLER_SIGNATURE  SIGNATURE_32('N','V','D','C')

typedef struct {
  UINT32                     Signature;
  EFI_HANDLE                 DriverHandle;
  EFI_HANDLE                 ControllerHandle;
  UINT8                      HandoffMode;
  NON_DISCOVERABLE_DEVICE    EdkiiNonDiscoverableDevice;
  BOOLEAN                    ResetsDeasserted;
  BOOLEAN                    ClocksEnabled;
  BOOLEAN                    OutputGpiosConfigured;
  BOOLEAN                    FdtUpdated;
  EFI_EVENT                  OnFdtInstalledEvent;
  EFI_EVENT                  OnReadyToBootEvent;
} NVIDIA_DISPLAY_CONTROLLER_CONTEXT;

#define DISPLAY_CONTROLLER_CONTEXT_FROM_EDKII_DEVICE(a)  CR(\
    a,                                                      \
    NVIDIA_DISPLAY_CONTROLLER_CONTEXT,                      \
    EdkiiNonDiscoverableDevice,                             \
    DISPLAY_CONTROLLER_SIGNATURE                            \
    )

/* Discover driver */

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra234-display", &gNVIDIANonDiscoverableT234DisplayDeviceGuid },
  { NULL,                      NULL                                         }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NV Display Controller Driver",
  .AutoDeassertPg                  = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE
};

/**
   Perform NvDisplay engine resets

   @param[in] DriverHandle             Handle to the driver
   @param[in] ControllerHandle         Handle to the controller
   @param[in] Assert                   Assert/Deassert the reset signal

   @retval EFI_SUCCESS                 display engines successfully reset
   @retval others                      display engine reset failure
 */
STATIC
EFI_STATUS
ResetRequiredDisplayEngines (
  IN       EFI_HANDLE  DriverHandle,
  IN       EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Assert
  )
{
  STATIC CONST CHAR8 *CONST  DisplayResets[] = {
    "nvdisplay_reset",
    "dpaux0_reset",
    NULL
  };

  return NvDisplayAssertResets (
           DriverHandle,
           ControllerHandle,
           DisplayResets,
           Assert
           );
}

/**
   Modeled after dispTegraSocEnableRequiredClks_v04_02 and
   dispTegraSocInitMaxFreqForDispHubClks_v04_02 in
   <gpu/drv/drivers/resman/src/physical/gpu/disp/arch/v04/disp_0402.c>.

   @param[in] DriverHandle              Handle to the driver
   @param[in] ControllerHandle          Handle to the controller
   @param[in] Enable                    Enable/disable the clocks

   @return EFI_SUCCESS      Clocks successfully enabled/disabled
   @return !=EFI_SUCCESS    An error occurred
 */
STATIC
EFI_STATUS
EnableRequiredDisplayClocks (
  IN       EFI_HANDLE  DriverHandle,
  IN       EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Enable
  )
{
  STATIC CONST CHAR8 *CONST  Clocks[] = {
    "nvdisplay_disp_clk",
    "dpaux0_clk",
    "nvdisplayhub_clk",
    "dsi_core_clk",
    "maud_clk",
    "aza_2xbit_clk",
    "aza_bit_clk",
    NULL
  };
  STATIC CONST CHAR8 *CONST  ClockParents[][2] = {
    { "nvdisplay_disp_clk", "disppll_clk"        },
    { "nvdisplayhub_clk",   "sppll0_clkoutb_clk" },
    { NULL,                 NULL                 }
  };

  return NvDisplayEnableClocks (
           DriverHandle,
           ControllerHandle,
           Clocks,
           ClockParents,
           Enable
           );
}

/**
   Retrieves GPIO pin number from a subnode of the specified node.

   @param [in]  DeviceTreeBase  Base of the Device Tree to read.
   @param [in]  NodeOffset      Offset of the specified node.
   @param [in]  SubnodeName     Name of the subnode to look for.
   @param [out] Pin             Where to store the pin number.

   @retval TRUE     Pin number successfully retrieved.
   @retval FALSE    An error occurred.
*/
STATIC
BOOLEAN
GetSubnodeGpioPin (
  IN  VOID          *CONST  DeviceTreeBase,
  IN  CONST INT32           NodeOffset,
  IN  CONST CHAR8   *CONST  SubnodeName,
  OUT UINT32        *CONST  Pin
  )
{
  INT32                 SubnodeOffset;
  CONST CHAR8   *CONST  GpiosPropName = "gpios";
  CONST VOID            *GpiosProp;
  INT32                 PropSize;

  SubnodeOffset = fdt_subnode_offset (
                    DeviceTreeBase,
                    NodeOffset,
                    SubnodeName
                    );
  if (SubnodeOffset < 0) {
    if (SubnodeOffset != -FDT_ERR_NOTFOUND) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: could not locate subnode '%a': %a\r\n",
        __FUNCTION__,
        SubnodeName,
        fdt_strerror (SubnodeOffset)
        ));
    }

    return FALSE;
  }

  GpiosProp = fdt_getprop (
                DeviceTreeBase,
                SubnodeOffset,
                GpiosPropName,
                &PropSize
                );
  if (GpiosProp == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: could not locate property '%a': %a\r\n",
      __FUNCTION__,
      GpiosPropName,
      fdt_strerror (PropSize)
      ));
    return FALSE;
  }

  if (PropSize < sizeof (*Pin)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: invalid size of property '%a': %d\r\n",
      __FUNCTION__,
      GpiosPropName,
      (INTN)PropSize
      ));
    return FALSE;
  }

  *Pin = SwapBytes32 (*(CONST UINT32 *)GpiosProp);
  return TRUE;
}

/**
 configure any GPIOs needed for HDMI/DP output
 */
STATIC
EFI_STATUS
ConfigureOutputGpios (
  IN       EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Enable,
  IN CONST BOOLEAN     UseDpOutput
  )
{
  EFI_STATUS                                Status;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL          *DeviceTreeNode;
  VOID                                      *DeviceTreeBase;
  EMBEDDED_GPIO                             *EmbeddedGpio;
  CONST CHAR8                       *CONST  GpioCompatible = "ti,tca9539";
  INT32                                     GpioOffset;
  UINT32                                    GpioPhandle;
  UINT32                                    EnVddHdmiPin;
  UINT32                                    Dp0AuxUart6SelPin;
  UINT32                                    HdmiDp0MuxSelPin;
  UINT32                                    Dp0AuxI2c8SelPin;
  EMBEDDED_GPIO_PIN                         GpioPin;
  EMBEDDED_GPIO_MODE                        GpioMode;

  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gNVIDIADeviceTreeNodeProtocolGuid,
                  (VOID **)&DeviceTreeNode
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: could not retrieve DT node protocol: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  DeviceTreeBase = DeviceTreeNode->DeviceTreeBase;

  Status = gBS->LocateProtocol (
                  &gNVIDIAI2cExpanderGpioProtocolGuid,
                  NULL,
                  (VOID **)&EmbeddedGpio
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: could not locate I2C expander GPIO protocol: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  GpioOffset = -1;
  while (1) {
    GpioOffset = fdt_node_offset_by_compatible (DeviceTreeBase, GpioOffset, GpioCompatible);
    if (GpioOffset == -FDT_ERR_NOTFOUND) {
      DEBUG ((
        DEBUG_INFO,
        "%a: could not find compatible GPIO node in DT: not on SLT board?\r\n",
        __FUNCTION__
        ));
      /* Return success to avoid breaking boot on non-SLT boards. */
      return EFI_SUCCESS;
    } else if (GpioOffset < 0) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to lookup node by compatible '%a': %a\r\n",
        __FUNCTION__,
        GpioCompatible,
        fdt_strerror (GpioOffset)
        ));
      return EFI_NOT_FOUND;
    }

    if (  GetSubnodeGpioPin (DeviceTreeBase, GpioOffset, "en_vdd_hdmi_cvm", &EnVddHdmiPin)
       && GetSubnodeGpioPin (DeviceTreeBase, GpioOffset, "dp0_aux_uart6_sel", &Dp0AuxUart6SelPin)
       && GetSubnodeGpioPin (DeviceTreeBase, GpioOffset, "hdmi_dp0_mux_sel", &HdmiDp0MuxSelPin)
       && GetSubnodeGpioPin (DeviceTreeBase, GpioOffset, "dp0_aux_i2c8_sel", &Dp0AuxI2c8SelPin))
    {
      break;
    }
  }

  GpioPhandle = fdt_get_phandle (DeviceTreeBase, GpioOffset);
  if ((0 == GpioPhandle) || (-1 == GpioPhandle)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to find phandle of node at offset %d\r\n",
      __FUNCTION__,
      (INTN)GpioOffset
      ));
    return EFI_NOT_FOUND;
  }

  GpioPin  = GPIO (GpioPhandle, EnVddHdmiPin);
  GpioMode = Enable ? GPIO_MODE_OUTPUT_1 : GPIO_MODE_OUTPUT_0;
  Status   = EmbeddedGpio->Set (EmbeddedGpio, GpioPin, GpioMode);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: could not set pin 0x%x to mode 0x%x: %r\r\n",
      __FUNCTION__,
      GpioPin,
      GpioMode,
      Status
      ));
    return Status;
  }

  if (Enable) {
    GpioPin  = GPIO (GpioPhandle, Dp0AuxUart6SelPin);
    GpioMode = GPIO_MODE_OUTPUT_0;
    Status   = EmbeddedGpio->Set (EmbeddedGpio, GpioPin, GpioMode);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: could not set pin 0x%x to mode 0x%x: %r\r\n",
        __FUNCTION__,
        GpioPin,
        GpioMode,
        Status
        ));
      return Status;
    }

    GpioPin  = GPIO (GpioPhandle, HdmiDp0MuxSelPin);
    GpioMode = UseDpOutput ? GPIO_MODE_OUTPUT_1 : GPIO_MODE_OUTPUT_0;
    Status   = EmbeddedGpio->Set (EmbeddedGpio, GpioPin, GpioMode);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: could not set pin 0x%x to mode 0x%x: %r\r\n",
        __FUNCTION__,
        GpioPin,
        GpioMode,
        Status
        ));
      return Status;
    }

    GpioPin  = GPIO (GpioPhandle, Dp0AuxI2c8SelPin);
    GpioMode = GPIO_MODE_OUTPUT_0;
    Status   = EmbeddedGpio->Set (EmbeddedGpio, GpioPin, GpioMode);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: could not set pin 0x%x to mode 0x%x: %r\r\n",
        __FUNCTION__,
        GpioPin,
        GpioMode,
        Status
        ));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
   Switch all SOR clocks to a safe source to prevent a lingering bad
   display HW state.

   @param[in] Context  Controller context to use.

   @retval EFI_SUCCESS    Operation successful.
   @retval !=EFI_SUCCESS  Error(s) occurred
*/
STATIC
EFI_STATUS
DisplayBypassSorClocks (
  IN NVIDIA_DISPLAY_CONTROLLER_CONTEXT *CONST  Context
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  DisplayBase;
  UINTN                 DisplaySize;
  UINTN                 SorIndex;
  UINT32                FeSwSysCap, FeCmgrClkSor;
  CONST UINTN           DisplayRegion = 0;

  Status = DeviceDiscoveryGetMmioRegion (
             Context->ControllerHandle,
             DisplayRegion,
             &DisplayBase,
             &DisplaySize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to retrieve display region: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  FeSwSysCap = MmioRead32 (DisplayBase + DISPLAY_FE_SW_SYS_CAP);
  for (SorIndex = 0; SorIndex < DISPLAY_SOR_COUNT; ++SorIndex) {
    if (DISPLAY_FE_SW_SYS_CAP_SOR_EXISTS_GET (FeSwSysCap, SorIndex)) {
      FeCmgrClkSor = MmioRead32 (DisplayBase + DISPLAY_FE_CMGR_CLK_SOR (SorIndex));
      FeCmgrClkSor = DISPLAY_FE_CMGR_CLK_SOR_MODE_BYPASS_SET (
                       FeCmgrClkSor,
                       DISPLAY_FE_CMGR_CLK_SOR_MODE_BYPASS_DP_SAFE
                       );
      MmioWrite32 (DisplayBase + DISPLAY_FE_CMGR_CLK_SOR (SorIndex), FeCmgrClkSor);
    }
  }

  return EFI_SUCCESS;
}

/**
  Performs teardown of the display hardware during ExitBootServices.

  @param[in] Context  Context of the display controller.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Error(s) occurred
*/
STATIC
EFI_STATUS
DisplayStopOnExitBootServices (
  IN NVIDIA_DISPLAY_CONTROLLER_CONTEXT *CONST  Context
  )
{
  EFI_STATUS     Status = EFI_SUCCESS;
  EFI_STATUS     Status1;
  EFI_HANDLE     DriverHandle;
  EFI_HANDLE     ControllerHandle;
  CONST BOOLEAN  UseDpOutput = FALSE;

  if (Context != NULL) {
    DriverHandle     = Context->DriverHandle;
    ControllerHandle = Context->ControllerHandle;

    if (Context->OnFdtInstalledEvent != NULL) {
      Status1 = gBS->CloseEvent (Context->OnFdtInstalledEvent);
      if (EFI_ERROR (Status1)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to close OnFdtInstalled event: %r\r\n",
          __FUNCTION__,
          Status1
          ));
      }

      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }
    }

    if (Context->OnReadyToBootEvent != NULL) {
      Status1 = gBS->CloseEvent (Context->OnReadyToBootEvent);
      if (EFI_ERROR (Status1)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to close OnReadyToBoot event: %r\r\n",
          __FUNCTION__,
          Status1
          ));
      }

      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }
    }

    if (Context->OutputGpiosConfigured) {
      Status1 = ConfigureOutputGpios (ControllerHandle, FALSE, UseDpOutput);
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      Context->OutputGpiosConfigured = FALSE;
    }

    if (Context->ClocksEnabled) {
      Status1 = EnableRequiredDisplayClocks (DriverHandle, ControllerHandle, FALSE);
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      Context->ClocksEnabled = FALSE;
    }

    if (Context->ResetsDeasserted) {
      Status1 = ResetRequiredDisplayEngines (DriverHandle, ControllerHandle, TRUE);
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      Context->ResetsDeasserted = FALSE;
    }
  }

  return Status;
}

/**
  Performs teardown of the display hardware.

  Cannot be called during ExitBootServices since it also frees the
  display context.

  @param[in] Context  Context of the display controller.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Error(s) occurred
*/
STATIC
EFI_STATUS
DisplayStop (
  IN NVIDIA_DISPLAY_CONTROLLER_CONTEXT *CONST  Context
  )
{
  EFI_STATUS  Status;

  Status = DisplayStopOnExitBootServices (Context);

  if (Context != NULL) {
    FreePool (Context);
  }

  return Status;
}

/**
   Event notification function for updating the Device Tree with mode
   and framebuffer info.

   @param[in] Event          Event used for the notification.
   @param[in] NotifyContext  Context for the notification.
*/
STATIC
VOID
EFIAPI
UpdateFdtTableNotifyFunction (
  IN EFI_EVENT    Event,
  IN VOID *CONST  NotifyContext
  )
{
  NVIDIA_DISPLAY_CONTROLLER_CONTEXT *CONST  Context =
    (NVIDIA_DISPLAY_CONTROLLER_CONTEXT *)NotifyContext;

  Context->FdtUpdated = NvDisplayUpdateFdtTableActiveChildGop (
                          Context->DriverHandle,
                          Context->ControllerHandle
                          );
}

/**
  Check if we should perform display hand-off or not.

  @param[in] Context  The display context to use.

  @return TRUE   Leave the display running on UEFI exit.
  @return FALSE  Reset the display on UEFI exit.
*/
STATIC
BOOLEAN
DisplayCheckPerformHandoff (
  IN CONST NVIDIA_DISPLAY_CONTROLLER_CONTEXT *CONST  Context
  )
{
  EFI_STATUS  Status;
  VOID        *Table;

  switch (Context->HandoffMode) {
    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_NEVER:
    default:
      return FALSE;

    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_ALWAYS:
      return TRUE;

    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_AUTO:
      Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &Table);
      if (!EFI_ERROR (Status)) {
        /* ACPI boot: reset the display unless it is active. */
        Status = NvDisplayLocateActiveChildGop (
                   Context->DriverHandle,
                   Context->ControllerHandle,
                   NULL
                   );
        return !EFI_ERROR (Status);
      }

      Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &Table);
      if (!EFI_ERROR (Status)) {
        /* DT boot: reset the display unless the last FDT update was
           successful. */
        return Context->FdtUpdated;
      }

      /* Default to display reset. */
      return FALSE;
  }
}

/**
  Performs the necessary initialization of the display hardware.

  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
*/
STATIC
EFI_STATUS
DisplayStart (
  OUT NVIDIA_DISPLAY_CONTROLLER_CONTEXT **CONST  Context,
  IN  EFI_HANDLE                                 DriverHandle,
  IN  EFI_HANDLE                                 ControllerHandle
  )
{
  EFI_STATUS                         Status;
  UINTN                              ResourcesSize;
  NON_DISCOVERABLE_DEVICE            *NvNonDiscoverableDevice;
  NVIDIA_DISPLAY_CONTROLLER_CONTEXT  *Result     = NULL;
  CONST BOOLEAN                      UseDpOutput = FALSE;

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **)&NvNonDiscoverableDevice,
                  DriverHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to open NVIDIA non-discoverable device protocol: %r\r\n",
      __FUNCTION__,
      Status
      ));
    goto Exit;
  }

  Status = NvDisplayGetMmioRegions (DriverHandle, ControllerHandle, NULL, &ResourcesSize);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Result = (NVIDIA_DISPLAY_CONTROLLER_CONTEXT *)AllocateZeroPool (sizeof (*Result) + ResourcesSize);
  if (Result == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: could not allocate %u bytes for display controller context\r\n",
      __FUNCTION__,
      sizeof (*Result) + ResourcesSize
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Result->Signature        = DISPLAY_CONTROLLER_SIGNATURE;
  Result->DriverHandle     = DriverHandle;
  Result->ControllerHandle = ControllerHandle;
  Result->HandoffMode      = PcdGet8 (PcdSocDisplayHandoffMode);

  CopyMem (
    &Result->EdkiiNonDiscoverableDevice,
    NvNonDiscoverableDevice,
    sizeof (Result->EdkiiNonDiscoverableDevice)
    );

  Result->EdkiiNonDiscoverableDevice.Resources =
    (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)(Result + 1);

  Status = NvDisplayGetMmioRegions (
             DriverHandle,
             ControllerHandle,
             Result->EdkiiNonDiscoverableDevice.Resources,
             &ResourcesSize
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = ResetRequiredDisplayEngines (DriverHandle, ControllerHandle, FALSE);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Result->ResetsDeasserted = TRUE;

  Status = EnableRequiredDisplayClocks (DriverHandle, ControllerHandle, TRUE);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Result->ClocksEnabled = TRUE;

  Status = ConfigureOutputGpios (ControllerHandle, TRUE, UseDpOutput);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Result->OutputGpiosConfigured = TRUE;

  switch (Result->HandoffMode) {
    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_NEVER:
    default:
      break;

    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_ALWAYS:
    case NVIDIA_SOC_DISPLAY_HANDOFF_MODE_AUTO:
      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_CALLBACK,
                      UpdateFdtTableNotifyFunction,
                      Result,
                      &gFdtTableGuid,
                      &Result->OnFdtInstalledEvent
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to create OnFdtInstalled event: %r\r\n",
          __FUNCTION__,
          Status
          ));
        Result->OnFdtInstalledEvent = NULL;
        goto Exit;
      }

      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_CALLBACK,
                      UpdateFdtTableNotifyFunction,
                      Result,
                      &gEfiEventReadyToBootGuid,
                      &Result->OnReadyToBootEvent
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to create OnReadyToBoot event: %r\r\n",
          __FUNCTION__,
          Status
          ));
        Result->OnReadyToBootEvent = NULL;
        goto Exit;
      }

      break;
  }

  *Context = Result;

Exit:
  if (EFI_ERROR (Status)) {
    DisplayStop (Result);
  }

  return Status;
}

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
  EFI_STATUS                         Status;
  TEGRA_PLATFORM_TYPE                Platform;
  NON_DISCOVERABLE_DEVICE            *EdkiiNonDiscoverableDevice;
  NVIDIA_DISPLAY_CONTROLLER_CONTEXT  *Context;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingSupported:
      Platform = TegraGetPlatform ();
      if (Platform != TEGRA_PLATFORM_SILICON) {
        return EFI_UNSUPPORTED;
      }

      return EFI_SUCCESS;

    case DeviceDiscoveryDriverBindingStart:
      Status = DisplayStart (&Context, DriverHandle, ControllerHandle);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = gBS->InstallMultipleProtocolInterfaces (
                      &ControllerHandle,
                      &gEdkiiNonDiscoverableDeviceProtocolGuid,
                      &Context->EdkiiNonDiscoverableDevice,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to install non-discoverable device protocol: %r\r\n",
          __FUNCTION__,
          Status
          ));
        DisplayStop (Context);
      }

      return Status;

    case DeviceDiscoveryDriverBindingStop:
    case DeviceDiscoveryOnExit:
      Status = gBS->OpenProtocol (
                      ControllerHandle,
                      &gEdkiiNonDiscoverableDeviceProtocolGuid,
                      (VOID **)&EdkiiNonDiscoverableDevice,
                      DriverHandle,
                      ControllerHandle,
                      EFI_OPEN_PROTOCOL_GET_PROTOCOL
                      );
      if (Status == EFI_UNSUPPORTED) {
        return EFI_SUCCESS;
      } else if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to open non-discoverable device protocol: %r\r\n",
          __FUNCTION__,
          Status
          ));
        return Status;
      }

      Context = DISPLAY_CONTROLLER_CONTEXT_FROM_EDKII_DEVICE (EdkiiNonDiscoverableDevice);

      if (Phase == DeviceDiscoveryDriverBindingStop) {
        Status = gBS->UninstallMultipleProtocolInterfaces (
                        ControllerHandle,
                        &gEdkiiNonDiscoverableDeviceProtocolGuid,
                        EdkiiNonDiscoverableDevice,
                        NULL
                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: failed to uninstall non-discoverable device protocol: %r\r\n",
            __FUNCTION__,
            Status
            ));
          return Status;
        }

        return DisplayStop (Context);
      } else {
        /* Phase == DeviceDiscoveryOnExit */

        if (DisplayCheckPerformHandoff (Context)) {
          /* We should perform hand-off, leave the display running. */
          return EFI_ABORTED;
        } else {
          /* No hand-off, reset the display to a known good state. */
          Status = DisplayBypassSorClocks (Context);
          ASSERT_EFI_ERROR (Status);

          return DisplayStopOnExitBootServices (Context);
        }
      }

    default:
      return EFI_SUCCESS;
  }
}
