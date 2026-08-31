/** @file

  NV Display Controller Driver - T238

  SPDX-FileCopyrightText: Copyright (c) 2025-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/EmbeddedGpio.h>
#include <Protocol/BpmpIpc.h>
#include <Protocol/PowerGateNodeProtocol.h>

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
#define TEGRA_ICC_DISPLAY                  7

typedef enum {
  CmdBwmgrIntQueryAbi   = 1,
  CmdBwmgrIntCalcAndSet = 2,
  CmdBwmgrIntCapSet     = 3,
  CmdBwmgrIntMax
} MRQ_BWMGR_INT_COMMANDS;

typedef enum {
  BwmgrIntUnitKbps = 0,
  BwmgrIntUnitKhz  = 1,
} BWMGR_INT_UNIT;

#pragma pack (push, 1)
typedef struct {
  UINT32    Command;
  UINT32    ClientId;
  UINT32    NonIsoBandwidthKbps;
  UINT32    IsoBandwidthKbps;
  UINT32    MemclockFloor;
  UINT8     MemclockFloorUnit;
} MRQ_BWMGR_INT_CALC_AND_SET_REQUEST;

typedef struct {
  UINT64    MemclockRateHz;
} MRQ_BWMGR_INT_CALC_AND_SET_RESPONSE;
#pragma pack (pop)

#define NV_DISPLAY_CONTROLLER_HW_SIGNATURE  SIGNATURE_32('T','2','3','8')

typedef struct {
  UINT32                      Signature;
  NV_DISPLAY_CONTROLLER_HW    Hw;
  EFI_HANDLE                  DriverHandle;
  EFI_HANDLE                  ControllerHandle;
  BOOLEAN                     UseDpOutput;
  BOOLEAN                     MaxEmcFrequencyForced;
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
  STATIC CONST CHAR8 *CONST  EnableClockParents[][2] = {
    { "nvdisplay_disp_clk", "disp_root"          },
    { "disp_root",          "disppll_clk"        },
    { "nvdisplayhub_clk",   "hub_root"           },
    { "hub_root",           "sppll0_clkoutb_clk" },
    { NULL,                 NULL                 }
  };
  STATIC CONST CHAR8 *CONST  DisableClockParents[][2] = {
    { "rg0_clk",      "osc_clk" },
    { "sor0_clk",     "osc_clk" },
    { "sor0_ref_clk", "osc_clk" },
    { NULL,           NULL      }
  };

  EFI_STATUS  Status;

  if (Enable) {
    Status = NvDisplaySetClockParents (DriverHandle, ControllerHandle, EnableClockParents);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    return NvDisplayEnableClocks (DriverHandle, ControllerHandle, Clocks);
  } else {
    Status = NvDisplaySetClockParents (DriverHandle, ControllerHandle, DisableClockParents);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    return NvDisplayDisableAllClocks (DriverHandle, ControllerHandle);
  }
}

/**
  Set EMC frequency floor.

  @param[in] DriverHandle       Handle to the driver.
  @param[in] ControllerHandle   Handle to the controller.
  @param[in] IsoBandwidthKbps   ISO bandwidth is kBps.
  @param[in] MemclockFloorKbps  Memory clock floor in kBps.

  @return EFI_SUCCESS      EMC frequency successfully set.
  @return EFI_NOT_READY    Could not retrieve one or more required protocols.
  @return EFI_UNSUPPORTED  BPMP BWMGR is disabled.
  @return !=EFI_SUCCESS    Error occurred.
*/
STATIC
EFI_STATUS
SetupEmcFrequency (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle,
  IN CONST UINT32      IsoBandwidthKbps,
  IN CONST UINT32      MemclockFloorKbps
  )
{
  EFI_STATUS                           Status;
  NVIDIA_BPMP_IPC_PROTOCOL             *BpmpIpc;
  NVIDIA_POWER_GATE_NODE_PROTOCOL      *PgNode;
  INT32                                MessageError;
  MRQ_BWMGR_INT_CALC_AND_SET_REQUEST   Request;
  MRQ_BWMGR_INT_CALC_AND_SET_RESPONSE  Response;

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpc);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to locate BPMP IPC protocol: %r\r\n", __FUNCTION__, Status));
    return EFI_NOT_READY;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gNVIDIAPowerGateNodeProtocolGuid,
                  (VOID **)&PgNode,
                  DriverHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to retrieve powergate node protocol: %r\r\n", __FUNCTION__, Status));
    return EFI_NOT_READY;
  }

  ZeroMem (&Request, sizeof (Request));
  Request.Command           = CmdBwmgrIntCalcAndSet;
  Request.ClientId          = TEGRA_ICC_DISPLAY;
  Request.IsoBandwidthKbps  = IsoBandwidthKbps;
  Request.MemclockFloor     = MemclockFloorKbps;
  Request.MemclockFloorUnit = BwmgrIntUnitKbps;

  Status = BpmpIpc->Communicate (
                      BpmpIpc,
                      NULL,
                      PgNode->BpmpPhandle,
                      MRQ_BWMGR_INT,
                      &Request,
                      sizeof (Request),
                      &Response,
                      sizeof (Response),
                      &MessageError
                      );
  if ((Status == EFI_PROTOCOL_ERROR) && (MessageError == BPMP_ENODEV)) {
    return EFI_UNSUPPORTED;     /* BWMGR is disabled. */
  } else if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: BPMP IPC Communicate failed: %r (MessageError = %d)\r\n", __FUNCTION__, Status, MessageError));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Force maximum EMC frequency.

  @param[in] DriverHandle      Handle to the driver.
  @param[in] ControllerHandle  Handle to the controller.
  @param[in] Enable            Enable/disable the forced max frequency.

  @return EFI_SUCCESS    EMC frequency successfully forced.
  @return !=EFI_SUCCESS  Error occurred.
*/
STATIC
EFI_STATUS
ForceMaxEmcFrequency (
  IN  CONST EFI_HANDLE  DriverHandle,
  IN  CONST EFI_HANDLE  ControllerHandle,
  IN  CONST BOOLEAN     Enable,
  OUT UINT32 *CONST     IsoBandwidthKbytesPerSec   OPTIONAL,
  OUT UINT32 *CONST     MemclockFloorKbytesPerSec  OPTIONAL
  )
{
  EFI_STATUS  Status;
  UINT32      IsoBandwidthKbps, MemclockFloorKbps;

  if (Enable) {
    IsoBandwidthKbps  = 4500U * 1000U; /* 4.5 GB/s decimal */
    MemclockFloorKbps = MAX_UINT32;
  } else {
    /*
     * WAR: Do not remove the EMC frequency floor in case an active
     * child GOP protocol is installed.
     *
     * In this case, the display should already be shut down, have its
     * clocks disabled and be in reset. However, removing the floor
     * after the display was active in UEFI is causing problems in
     * BPMP-FW.
     */
    Status = NvDisplayLocateActiveChildGop (DriverHandle, ControllerHandle, NULL);
    if (!EFI_ERROR (Status)) {
      return EFI_SUCCESS;
    }

    IsoBandwidthKbps  = 0;
    MemclockFloorKbps = 0;
  }

  Status = SetupEmcFrequency (
             DriverHandle,
             ControllerHandle,
             IsoBandwidthKbps,
             MemclockFloorKbps
             );
  if (!EFI_ERROR (Status)) {
    if (IsoBandwidthKbytesPerSec != NULL) {
      *IsoBandwidthKbytesPerSec = IsoBandwidthKbps;
    }

    if (MemclockFloorKbytesPerSec != NULL) {
      *MemclockFloorKbytesPerSec = MemclockFloorKbps;
    }
  } else if (Status == EFI_UNSUPPORTED) {
    /* If BWMGR is disabled, we cannot control the EMC frequency; in
       this case, just return success. */
    Status = EFI_SUCCESS;
  }

  return Status;
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
  Configure SOR link settings from Device Tree.

  @param[in] DriverHandle      Handle to the driver.
  @param[in] ControllerHandle  Handle to the controller.

  @retval EFI_SUCCESS    SOR link settings configured successfully,
                         or non-fatal error which does not require
                         disabling display.
  @retval !=EFI_SUCCESS  Operation failed.
*/
STATIC
EFI_STATUS
ConfigureSorLinks (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle
  )
{
  EFI_STATUS  Status;
  BOOLEAN     EnableSorLanePolarityInversion;

  EnableSorLanePolarityInversion = FALSE;
  Status                         = NvDisplayLookupSorLanes (
                                     DriverHandle,
                                     ControllerHandle,
                                     &EnableSorLanePolarityInversion
                                     );
  // DT property not found is non-fatal
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PcdSetExBoolS (
             &gNVIDIATokenSpaceGuid,
             PcdTegraSorLinkPolarityInv,
             EnableSorLanePolarityInversion
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set PcdTegraSorLinkPolarityInv: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  return Status;
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
  EFI_STATUS                               Status;
  EFI_STATUS                               Status1;
  NV_DISPLAY_CONTROLLER_HW_PRIVATE *CONST  Private = NV_DISPLAY_CONTROLLER_HW_PRIVATE_FROM_THIS (This);

  Status  = EFI_SUCCESS;
  Status1 = EFI_SUCCESS;

  if (Enable) {
    if (!Private->MaxEmcFrequencyForced) {
      Status = ForceMaxEmcFrequency (
                 Private->DriverHandle,
                 Private->ControllerHandle,
                 TRUE,
                 &Private->Hw.IsoBandwidthKbytesPerSec,
                 &Private->Hw.MemclockFloorKbytesPerSec
                 );
      if (EFI_ERROR (Status)) {
        goto Disable;
      }

      Private->MaxEmcFrequencyForced = TRUE;
    }

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

    Status = ConfigureSorLinks (
               Private->DriverHandle,
               Private->ControllerHandle
               );
    if (EFI_ERROR (Status)) {
      goto Disable;
    }

    Status = EnableAuxOverUsbc (Private->ControllerHandle, FALSE);
    if (EFI_ERROR (Status)) {
      goto Disable;
    }
  } else {
    /* Shutdown display HW if and only if we were called to disable
       the display, and clocks were enabled */
    if (Private->ClocksEnabled) {
      Status = NvDisplayHwShutdown (
                 Private->DriverHandle,
                 Private->ControllerHandle
                 );
    }

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

    if (Private->MaxEmcFrequencyForced) {
      Status1 = ForceMaxEmcFrequency (
                  Private->DriverHandle,
                  Private->ControllerHandle,
                  FALSE,
                  &Private->Hw.IsoBandwidthKbytesPerSec,
                  &Private->Hw.MemclockFloorKbytesPerSec
                  );
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      Private->MaxEmcFrequencyForced = FALSE;
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

  Status  = NvDisplayControllerStart (DriverHandle, ControllerHandle, &Private->Hw);
  Private = NULL;

Exit:
  if (Private != NULL) {
    Private->Hw.Destroy (&Private->Hw);
  }

  return Status;
}
