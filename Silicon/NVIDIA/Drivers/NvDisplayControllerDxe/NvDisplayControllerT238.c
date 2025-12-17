/** @file

  NV Display Controller Driver - T238

  SPDX-FileCopyrightText: Copyright (c) 2025-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/EmbeddedGpio.h>

#include "NvDisplay.h"
#include "NvDisplayController.h"

#define DPAUX_HYBRID_SPARE                  0x00000134
#define DPAUX_HYBRID_SPARE_AUX_USBC_EN_BIT  2

/* GPIO pin indices for T238 */
#define T238_GPIO_PIN_HDMI_DP_MUX_SEL      0
#define T238_GPIO_PIN_DP_AUX_I2C8_SEL      1
#define T238_GPIO_PIN_AUX_I2C6_UART6_SEL0  2
#define T238_GPIO_PIN_AUX_I2C6_UART6_SEL1  3
#define T238_GPIO_PIN_DP_AUX_CCG_SEL0      4
#define T238_GPIO_PIN_DP_AUX_CCG_SEL1      5
#define T238_GPIO_PIN_COUNT                6

#define NV_DISPLAY_CONTROLLER_HW_SIGNATURE  SIGNATURE_32('T','2','3','8')

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
    { "nvdisplay_disp_clk", "disp_root"          },
    { "disp_root",          "disppll_clk"        },
    { "nvdisplayhub_clk",   "hub_root"           },
    { "hub_root",           "sppll0_clkoutb_clk" },
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
  STATIC CONST CHAR8 *CONST  SubnodeNames[T238_GPIO_PIN_COUNT + 1] = {
    [T238_GPIO_PIN_HDMI_DP_MUX_SEL]     = "hdmi_dp_mux_sel",
    [T238_GPIO_PIN_DP_AUX_I2C8_SEL]     = "dp_aux_i2c8_sel",
    [T238_GPIO_PIN_AUX_I2C6_UART6_SEL0] = "aux_i2c6_uart6_sel0",
    [T238_GPIO_PIN_AUX_I2C6_UART6_SEL1] = "aux_i2c6_uart6_sel1",
    [T238_GPIO_PIN_DP_AUX_CCG_SEL0]     = "dp_aux_ccg_sel0",
    [T238_GPIO_PIN_DP_AUX_CCG_SEL1]     = "dp_aux_ccg_sel1",
    [T238_GPIO_PIN_COUNT]               = NULL
  };

  EFI_STATUS          Status;
  EMBEDDED_GPIO       *EmbeddedGpio;
  UINT32              GpioPhandle;
  UINT32              Pins[T238_GPIO_PIN_COUNT];
  EMBEDDED_GPIO_PIN   GpioPin;
  EMBEDDED_GPIO_MODE  GpioMode;

  NV_ASSERT_RETURN (
    SubnodeNames[T238_GPIO_PIN_COUNT] == NULL,
    return EFI_INVALID_PARAMETER,
    "%a: SubnodeNames array not properly terminated\n",
    __FUNCTION__
    );

  Status = NvDisplayLookupGpioPins (
             DriverHandle,
             ControllerHandle,
             "nxp,pca9535",
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

  if (Enable) {
    GpioPin  = GPIO (GpioPhandle, Pins[T238_GPIO_PIN_HDMI_DP_MUX_SEL]);
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

    GpioPin  = GPIO (GpioPhandle, Pins[T238_GPIO_PIN_DP_AUX_I2C8_SEL]);
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

    GpioPin  = GPIO (GpioPhandle, Pins[T238_GPIO_PIN_AUX_I2C6_UART6_SEL0]);
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

    GpioPin  = GPIO (GpioPhandle, Pins[T238_GPIO_PIN_AUX_I2C6_UART6_SEL1]);
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

    GpioPin  = GPIO (GpioPhandle, Pins[T238_GPIO_PIN_DP_AUX_CCG_SEL0]);
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

    GpioPin  = GPIO (GpioPhandle, Pins[T238_GPIO_PIN_DP_AUX_CCG_SEL1]);
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
  Enable or disable AUX over USB-C.

  Configures the DPAUX_HYBRID_SPARE register to enable or disable
  AUX channel routing over USB-C connector.

  @param[in] ControllerHandle  Handle to the controller.
  @param[in] Enable            TRUE to enable, FALSE to disable.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Failed to retrieve DPAUX MMIO region.
*/
STATIC
EFI_STATUS
EnableAuxOverUsbc (
  IN CONST EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Enable
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  DpauxBase;
  UINTN                 DpauxSize;
  CONST UINTN           DpauxRegion = 1;

  Status = DeviceDiscoveryGetMmioRegion (
             ControllerHandle,
             DpauxRegion,
             &DpauxBase,
             &DpauxSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to retrieve dpaux region: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  if (DpauxSize < (DPAUX_HYBRID_SPARE + sizeof (UINT32))) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: DPAUX region too small: %u bytes, need at least %u\r\n",
      __FUNCTION__,
      (UINT32)DpauxSize,
      (UINT32)(DPAUX_HYBRID_SPARE + sizeof (UINT32))
      ));
    return EFI_DEVICE_ERROR;
  }

  MmioAndThenOr32 (
    DpauxBase + DPAUX_HYBRID_SPARE,
    ~(1U << DPAUX_HYBRID_SPARE_AUX_USBC_EN_BIT),
    Enable ? (1U << DPAUX_HYBRID_SPARE_AUX_USBC_EN_BIT) : 0
    );

  return EFI_SUCCESS;
}

/**
  Destroys T238 display hardware context.

  @param[in] This  Chip-specific display HW context.
*/
STATIC
VOID
EFIAPI
DestroyHwT238 (
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
  Enables or disables T238 display hardware.

  @param[in] This    Chip-specific display HW context.
  @param[in] Enable  TRUE to enable, FALSE to disable.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Operation failed.
*/
STATIC
EFI_STATUS
EFIAPI
EnableHwT238 (
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

    Status = EnableAuxOverUsbc (Private->ControllerHandle, FALSE);
    if (EFI_ERROR (Status)) {
      goto Disable;
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
   Starts the NV T238 display controller driver on the given
   controller handle.

   @param[in] DriverHandle      The driver handle.
   @param[in] ControllerHandle  The controller handle.

   @retval EFI_SUCCESS    Operation successful.
   @retval !=EFI_SUCCESS  Operation failed.
*/
EFI_STATUS
NvDisplayControllerStartT238 (
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
  Private->Hw.Destroy             = DestroyHwT238;
  Private->Hw.Enable              = EnableHwT238;
  Private->Hw.MaxDispClkRateKhz   = Private->MaxDispClkRateKhz;
  Private->Hw.MaxDispClkRateCount = ARRAY_SIZE (Private->MaxDispClkRateKhz);
  Private->Hw.MaxHubClkRateKhz    = Private->MaxHubClkRateKhz;
  Private->Hw.MaxHubClkRateCount  = ARRAY_SIZE (Private->MaxHubClkRateKhz);
  Private->DriverHandle           = DriverHandle;
  Private->ControllerHandle       = ControllerHandle;

  Status = NvDisplayGetClockRatesWithParentsAndReset (
             DriverHandle,
             ControllerHandle,
             "disp_root",
             DispClkParents,
             Private->MaxDispClkRateKhz
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = NvDisplayGetClockRatesWithParentsAndReset (
             DriverHandle,
             ControllerHandle,
             "hub_root",
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
