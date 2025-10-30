/** @file

  NV Display Controller Driver - T264

  SPDX-FileCopyrightText: Copyright (c) 2021-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/BpmpIpc.h>
#include <Protocol/PowerGateNodeProtocol.h>

#include "NvDisplay.h"
#include "NvDisplayController.h"

#define TEGRA264_BWMGR_DISPLAY  11

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

#define NV_DISPLAY_CONTROLLER_HW_SIGNATURE  SIGNATURE_32('T','2','6','4')

typedef struct {
  UINT32                      Signature;
  NV_DISPLAY_CONTROLLER_HW    Hw;
  EFI_HANDLE                  DriverHandle;
  EFI_HANDLE                  ControllerHandle;
  BOOLEAN                     MaxEmcFrequencyForced;
  BOOLEAN                     ResetsDeasserted;
  BOOLEAN                     ClocksEnabled;
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
  Assert or deassert T264 display resets.

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
  Enable or disable required T264 display clocks.

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
    "nvdisplayhub_clk",
    "maud_clk",
    "aza_2xbit_clk",
    "dpaux0_clk",
    NULL
  };
  STATIC CONST CHAR8 *CONST  ClockParents[][2] = {
    { "disp_root",        "disppll_clk"        },
    { "nvdisplayhub_clk", "sppll0_clkoutb_clk" },
    { NULL,               NULL                 }
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
  Request.ClientId          = TEGRA264_BWMGR_DISPLAY;
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
    IsoBandwidthKbps  = 20 << 20; /* 20 GB/s */
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
  Destroys T264 display hardware context.

  @param[in] This  Chip-specific display HW context.
*/
STATIC
VOID
EFIAPI
DestroyHwT264 (
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
  Enables or disables T264 display hardware.

  @param[in] This    Chip-specific display HW context.
  @param[in] Enable  TRUE to enable, FALSE to disable.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Operation failed.
*/
STATIC
EFI_STATUS
EFIAPI
EnableHwT264 (
  IN NV_DISPLAY_CONTROLLER_HW *CONST  This,
  IN CONST BOOLEAN                    Enable
  )
{
  EFI_STATUS                               Status, Status1;
  NV_DISPLAY_CONTROLLER_HW_PRIVATE *CONST  Private = NV_DISPLAY_CONTROLLER_HW_PRIVATE_FROM_THIS (This);

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
  } else {
    /* Shutdown display HW if and only if we were called to disable
       the display. */
    Status = NvDisplayHwShutdown (
               Private->DriverHandle,
               Private->ControllerHandle
               );

Disable:
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
  Starts the NV T264 display controller driver on the given
  controller handle.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.

  @retval EFI_SUCCESS          Operation successful.
  @retval EFI_ALREADY_STARTED  Driver has already been started on the given handle.
  @retval !=EFI_SUCCESS        Operation failed.
*/
EFI_STATUS
NvDisplayControllerStartT264 (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle
  )
{
  STATIC CONST CHAR8 *CONST  DispRootParents[] = {
    "disppll_clk",
    "sppll0_clkouta_clk",
    NULL
  };
  STATIC CONST CHAR8 *CONST  HubClkParents[] = {
    "sppll0_clkoutb_clk",
    NULL
  };

  EFI_STATUS                        Status;
  NV_DISPLAY_CONTROLLER_HW_PRIVATE  *Private = NULL;

  Private = AllocateZeroPool (sizeof (*Private));
  if (Private == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Private->Signature              = NV_DISPLAY_CONTROLLER_HW_SIGNATURE;
  Private->Hw.Destroy             = DestroyHwT264;
  Private->Hw.Enable              = EnableHwT264;
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
             DispRootParents,
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

  Status  = NvDisplayControllerStart (DriverHandle, ControllerHandle, &Private->Hw);
  Private = NULL;

Exit:
  if (Private != NULL) {
    Private->Hw.Destroy (&Private->Hw);
  }

  return Status;
}
