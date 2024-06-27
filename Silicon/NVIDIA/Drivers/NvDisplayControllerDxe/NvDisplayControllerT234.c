/** @file

  NV Display Controller Driver - T234

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/EmbeddedGpio.h>

#include <libfdt.h>

#include "NvDisplay.h"
#include "NvDisplayController.h"

#define DISPLAY_SOR_COUNT      8
#define DISPLAY_FE_SW_SYS_CAP  0x00030000
#define DISPLAY_FE_SW_SYS_CAP_SOR_EXISTS_GET(x, i)     (BOOLEAN)(BitFieldRead32 ((x), 8 + (i), 8 + (i)) != 0)
#define DISPLAY_FE_CMGR_CLK_SOR(i)                     (0x00002300 + (i) * SIZE_2KB)
#define DISPLAY_FE_CMGR_CLK_SOR_MODE_BYPASS_SET(x, v)  BitFieldWrite32 ((x), 16, 17, (v))
#define DISPLAY_FE_CMGR_CLK_SOR_MODE_BYPASS_DP_SAFE  2

/**
  Assert or deassert display resets.

  @param[in] DriverHandle      Handle to the driver.
  @param[in] ControllerHandle  Handle to the controller.
  @param[in] Assert            Assert/deassert the reset signal.

  @retval EFI_SUCCESS  Operation successful.
  @retval others       Error(s) occurred.
*/
STATIC
EFI_STATUS
AssertResets (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Assert
  )
{
  STATIC CONST CHAR8 *CONST  Resets[] = {
    "nvdisplay_reset",
    "dpaux0_reset",
    NULL
  };

  return NvDisplayAssertResets (
           DriverHandle,
           ControllerHandle,
           Resets,
           Assert
           );
}

/**
  Modeled after dispTegraSocEnableRequiredClks_v04_02 and
  dispTegraSocInitMaxFreqForDispHubClks_v04_02 in
  <gpu/drv/drivers/resman/src/physical/gpu/disp/arch/v04/disp_0402.c>.

  @param[in] DriverHandle      Handle to the driver.
  @param[in] ControllerHandle  Handle to the controller.
  @param[in] Enable            Enable/disable the clocks.

  @return EFI_SUCCESS    Clocks successfully enabled/disabled.
  @return !=EFI_SUCCESS  An error occurred.
*/
STATIC
EFI_STATUS
EnableClocks (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle,
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
  IN  VOID        *CONST  DeviceTreeBase,
  IN  CONST INT32         NodeOffset,
  IN  CONST CHAR8 *CONST  SubnodeName,
  OUT UINT32      *CONST  Pin
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
ConfigureGpios (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Enable,
  IN CONST BOOLEAN     UseDpOutput
  )
{
  EFI_STATUS                        Status;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode;
  VOID                              *DeviceTreeBase;
  EMBEDDED_GPIO                     *EmbeddedGpio;
  CONST CHAR8               *CONST  GpioCompatible = "ti,tca9539";
  INT32                             GpioOffset;
  UINT32                            GpioPhandle;
  UINT32                            EnVddHdmiPin;
  UINT32                            Dp0AuxUart6SelPin;
  UINT32                            HdmiDp0MuxSelPin;
  UINT32                            Dp0AuxI2c8SelPin;
  EMBEDDED_GPIO_PIN                 GpioPin;
  EMBEDDED_GPIO_MODE                GpioMode;

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gNVIDIADeviceTreeNodeProtocolGuid,
                  (VOID **)&DeviceTreeNode,
                  DriverHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
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

   @param[in] DriverHandle      Handle to the driver.
   @param[in] ControllerHandle  Handle to the controller.

   @retval EFI_SUCCESS    Operation successful.
   @retval !=EFI_SUCCESS  Error(s) occurred.
*/
STATIC
EFI_STATUS
BypassSorClocks (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  DisplayBase;
  UINTN                 DisplaySize;
  UINTN                 SorIndex;
  UINT32                FeSwSysCap, FeCmgrClkSor;
  CONST UINTN           DisplayRegion = 0;

  Status = DeviceDiscoveryGetMmioRegion (
             ControllerHandle,
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
  Enables or disables T234 display hardware.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.
  @param[in] Enable            TRUE to enable, FALSE to disable.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Operation failed.
*/
STATIC
EFI_STATUS
EnableHwT234 (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Enable
  )
{
  EFI_STATUS     Status, Status1;
  BOOLEAN        ResetsDeasserted = !Enable;
  BOOLEAN        ClocksEnabled    = !Enable;
  BOOLEAN        GpiosConfigured  = !Enable;
  CONST BOOLEAN  UseDpOutput      = FALSE;

  if (Enable) {
    Status = AssertResets (DriverHandle, ControllerHandle, FALSE);
    if (EFI_ERROR (Status)) {
      goto Disable;
    }

    ResetsDeasserted = TRUE;

    Status = EnableClocks (DriverHandle, ControllerHandle, TRUE);
    if (EFI_ERROR (Status)) {
      goto Disable;
    }

    ClocksEnabled = TRUE;

    Status = ConfigureGpios (DriverHandle, ControllerHandle, TRUE, UseDpOutput);
    if (EFI_ERROR (Status)) {
      goto Disable;
    }

    GpiosConfigured = TRUE;
  } else {
    /* Bypass SOR clocks if and only if we were called to disable
       display HW. */
    Status = BypassSorClocks (DriverHandle, ControllerHandle);

Disable:
    if (GpiosConfigured) {
      Status1 = ConfigureGpios (DriverHandle, ControllerHandle, FALSE, UseDpOutput);
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      GpiosConfigured = FALSE;
    }

    if (ClocksEnabled) {
      Status1 = EnableClocks (DriverHandle, ControllerHandle, FALSE);
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      ClocksEnabled = FALSE;
    }

    if (ResetsDeasserted) {
      Status1 = AssertResets (DriverHandle, ControllerHandle, TRUE);
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      ResetsDeasserted = FALSE;
    }
  }

  return Status;
}

/**
   Starts the NV T234 display controller driver on the given
   controller handle.

   @param[in] DriverHandle      The driver handle.
   @param[in] ControllerHandle  The controller handle.

   @retval EFI_SUCCESS    Operation successful.
   @retval !=EFI_SUCCESS  Operation failed.
*/
EFI_STATUS
NvDisplayControllerStartT234 (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle
  )
{
  return NvDisplayControllerStart (
           DriverHandle,
           ControllerHandle,
           EnableHwT234
           );
}
