/** @file

  NV Display Controller Driver - T234

  SPDX-FileCopyrightText: Copyright (c) 2024-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/EmbeddedGpio.h>

#include "NvDisplay.h"
#include "NvDisplayController.h"

/* GPIO pin indices for T234 */
#define T234_GPIO_PIN_EN_VDD_HDMI        0
#define T234_GPIO_PIN_DP0_AUX_UART6_SEL  1
#define T234_GPIO_PIN_HDMI_DP0_MUX_SEL   2
#define T234_GPIO_PIN_DP0_AUX_I2C8_SEL   3
#define T234_GPIO_PIN_COUNT              4

#define NV_DISPLAY_CONTROLLER_HW_SIGNATURE  SIGNATURE_32('T','2','3','4')

typedef struct {
  UINT32                      Signature;
  NV_DISPLAY_CONTROLLER_HW    Hw;
  EFI_HANDLE                  DriverHandle;
  EFI_HANDLE                  ControllerHandle;
  BOOLEAN                     UseDpOutput;
  BOOLEAN                     ResetsDeasserted;
  BOOLEAN                     ClocksEnabled;
  BOOLEAN                     GpiosConfigured;
  UINT32                      MaxDispClkRateKhz[2];
  UINT32                      MaxHubClkRateKhz[1];
} NV_DISPLAY_CONTROLLER_HW_PRIVATE;

#define NV_DISPLAY_CONTROLLER_HW_PRIVATE_FROM_THIS(a)  CR(\
    a,                                                    \
    NV_DISPLAY_CONTROLLER_HW_PRIVATE,                     \
    Hw,                                                   \
    NV_DISPLAY_CONTROLLER_HW_SIGNATURE                    \
    )

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

  EFI_STATUS  Status;

  if (Enable) {
    Status = NvDisplaySetClockParents (DriverHandle, ControllerHandle, ClockParents);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    return NvDisplayEnableClocks (DriverHandle, ControllerHandle, Clocks);
  } else {
    return NvDisplayDisableAllClocks (DriverHandle, ControllerHandle);
  }
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
  STATIC CONST CHAR8 *CONST  SubnodeNames[T234_GPIO_PIN_COUNT + 1] = {
    [T234_GPIO_PIN_EN_VDD_HDMI]       = "en_vdd_hdmi_cvm",
    [T234_GPIO_PIN_DP0_AUX_UART6_SEL] = "dp0_aux_uart6_sel",
    [T234_GPIO_PIN_HDMI_DP0_MUX_SEL]  = "hdmi_dp0_mux_sel",
    [T234_GPIO_PIN_DP0_AUX_I2C8_SEL]  = "dp0_aux_i2c8_sel",
    [T234_GPIO_PIN_COUNT]             = NULL
  };

  EFI_STATUS          Status;
  EMBEDDED_GPIO       *EmbeddedGpio;
  UINT32              GpioPhandle;
  UINT32              Pins[T234_GPIO_PIN_COUNT];
  EMBEDDED_GPIO_PIN   GpioPin;
  EMBEDDED_GPIO_MODE  GpioMode;

  NV_ASSERT_RETURN (
    SubnodeNames[T234_GPIO_PIN_COUNT] == NULL,
    return EFI_INVALID_PARAMETER,
    "%a: SubnodeNames array not properly terminated\n",
    __FUNCTION__
    );

  Status = NvDisplayLookupGpioPins (
             DriverHandle,
             ControllerHandle,
             "ti,tca9539",
             SubnodeNames,
             &GpioPhandle,
             Pins
             );
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((
      DEBUG_INFO,
      "%a: could not find GPIO node in DT: not on SLT board?\r\n",
      __FUNCTION__
      ));
    /* Return success to avoid breaking boot on non-SLT boards. */
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

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

  GpioPin  = GPIO (GpioPhandle, Pins[T234_GPIO_PIN_EN_VDD_HDMI]);
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
    GpioPin  = GPIO (GpioPhandle, Pins[T234_GPIO_PIN_DP0_AUX_UART6_SEL]);
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

    GpioPin  = GPIO (GpioPhandle, Pins[T234_GPIO_PIN_HDMI_DP0_MUX_SEL]);
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

    GpioPin  = GPIO (GpioPhandle, Pins[T234_GPIO_PIN_DP0_AUX_I2C8_SEL]);
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
  Destroys T234 display hardware context.

  @param[in] This  Chip-specific display HW context.
*/
STATIC
VOID
EFIAPI
DestroyHwT234 (
  IN NV_DISPLAY_CONTROLLER_HW *CONST  This
  )
{
  NV_DISPLAY_CONTROLLER_HW_PRIVATE  *Private;

  if (This != NULL) {
    Private = NV_DISPLAY_CONTROLLER_HW_PRIVATE_FROM_THIS (This);

    FreePool (Private);
  }
}

/**
  Enables or disables T234 display hardware.

  @param[in] This    Chip-specific display HW context.
  @param[in] Enable  TRUE to enable, FALSE to disable.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Operation failed.
*/
STATIC
EFI_STATUS
EFIAPI
EnableHwT234 (
  IN NV_DISPLAY_CONTROLLER_HW *CONST  This,
  IN CONST BOOLEAN                    Enable
  )
{
  EFI_STATUS                               Status, Status1;
  NV_DISPLAY_CONTROLLER_HW_PRIVATE *CONST  Private = NV_DISPLAY_CONTROLLER_HW_PRIVATE_FROM_THIS (This);

  if (Enable) {
    if (!Private->ResetsDeasserted) {
      Status = AssertResets (
                 Private->DriverHandle,
                 Private->ControllerHandle,
                 FALSE
                 );
      if (EFI_ERROR (Status)) {
        goto Disable;
      }

      Private->ResetsDeasserted = TRUE;
    }

    if (!Private->ClocksEnabled) {
      Status = EnableClocks (
                 Private->DriverHandle,
                 Private->ControllerHandle,
                 TRUE
                 );
      if (EFI_ERROR (Status)) {
        goto Disable;
      }

      Private->ClocksEnabled = TRUE;
    }

    if (!Private->GpiosConfigured) {
      Status = ConfigureGpios (
                 Private->DriverHandle,
                 Private->ControllerHandle,
                 TRUE,
                 Private->UseDpOutput
                 );
      if (EFI_ERROR (Status)) {
        goto Disable;
      }

      Private->GpiosConfigured = TRUE;
    }
  } else {
    /* Shutdown display HW if and only if we were called to disable
       the display. */
    Status = NvDisplayHwShutdown (
               Private->DriverHandle,
               Private->ControllerHandle
               );

Disable:
    if (Private->GpiosConfigured) {
      Status1 = ConfigureGpios (
                  Private->DriverHandle,
                  Private->ControllerHandle,
                  FALSE,
                  Private->UseDpOutput
                  );
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      Private->GpiosConfigured = FALSE;
    }

    if (Private->ClocksEnabled) {
      Status1 = EnableClocks (
                  Private->DriverHandle,
                  Private->ControllerHandle,
                  FALSE
                  );
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      Private->ClocksEnabled = FALSE;
    }

    if (Private->ResetsDeasserted) {
      Status1 = AssertResets (
                  Private->DriverHandle,
                  Private->ControllerHandle,
                  TRUE
                  );
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      Private->ResetsDeasserted = FALSE;
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
  STATIC CONST CHAR8 *CONST  DispClkParents[] = {
    "disppll_clk",
    "sppll0_clkouta_clk",
    NULL
  };
  STATIC CONST CHAR8 *CONST  HubClkParents[] = {
    "sppll0_clkoutb_clk",
    NULL
  };

  EFI_STATUS                        Status;
  NV_DISPLAY_CONTROLLER_HW_PRIVATE  *Private;

  Private = AllocateZeroPool (sizeof (*Private));
  if (Private == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Private->Signature              = NV_DISPLAY_CONTROLLER_HW_SIGNATURE;
  Private->Hw.Destroy             = DestroyHwT234;
  Private->Hw.Enable              = EnableHwT234;
  Private->Hw.MaxDispClkRateKhz   = Private->MaxDispClkRateKhz;
  Private->Hw.MaxDispClkRateCount = ARRAY_SIZE (Private->MaxDispClkRateKhz);
  Private->Hw.MaxHubClkRateKhz    = Private->MaxHubClkRateKhz;
  Private->Hw.MaxHubClkRateCount  = ARRAY_SIZE (Private->MaxHubClkRateKhz);
  Private->DriverHandle           = DriverHandle;
  Private->ControllerHandle       = ControllerHandle;

  Status = NvDisplayGetClockRatesWithParentsAndReset (
             DriverHandle,
             ControllerHandle,
             "nvdisplay_disp_clk",
             DispClkParents,
             Private->MaxDispClkRateKhz
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = NvDisplayGetClockRatesWithParentsAndReset (
             DriverHandle,
             ControllerHandle,
             "nvdisplayhub_clk",
             HubClkParents,
             Private->MaxHubClkRateKhz
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = NvDisplayControllerStart (DriverHandle, ControllerHandle, &Private->Hw);
  if (!EFI_ERROR (Status)) {
    Private = NULL;
  }

Exit:
  if (Private != NULL) {
    Private->Hw.Destroy (&Private->Hw);
  }

  return Status;
}
