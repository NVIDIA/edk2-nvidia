/** @file

  NV Display Controller Driver - T264

  SPDX-FileCopyrightText: Copyright (c) 2021-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
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

  return NvDisplayEnableClocks (
           DriverHandle,
           ControllerHandle,
           Clocks,
           ClockParents,
           Enable
           );
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
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Enable
  )
{
  EFI_STATUS  Status;
  UINT32      IsoBandwidthKbps, MemclockFloorKbps;

  if (Enable) {
    IsoBandwidthKbps  = 20 << 20; /* 20 GB/s */
    MemclockFloorKbps = MAX_UINT32;
  } else {
    IsoBandwidthKbps  = 0;
    MemclockFloorKbps = 0;
  }

  Status = SetupEmcFrequency (
             DriverHandle,
             ControllerHandle,
             IsoBandwidthKbps,
             MemclockFloorKbps
             );
  if (Status == EFI_UNSUPPORTED) {
    /* If BWMGR is disabled, we cannot control the EMC frequency; in
       this case, just return success. */
    Status = EFI_SUCCESS;
  }

  return Status;
}

/**
  Enables or disables T264 display hardware.

  @param[in] DriverHandle      The driver handle.
  @param[in] ControllerHandle  The controller handle.
  @param[in] Enable            TRUE to enable, FALSE to disable.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Operation failed.
*/
STATIC
EFI_STATUS
EnableHwT264 (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Enable
  )
{
  EFI_STATUS  Status, Status1;
  BOOLEAN     MaxEmcFrequencyForced = !Enable;
  BOOLEAN     ResetsDeasserted      = !Enable;
  BOOLEAN     ClocksEnabled         = !Enable;

  if (Enable) {
    Status = ForceMaxEmcFrequency (DriverHandle, ControllerHandle, TRUE);
    if (EFI_ERROR (Status)) {
      goto Disable;
    }

    MaxEmcFrequencyForced = TRUE;

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
  } else {
    /* Shutdown display HW if and only if we were called to disable
       the display. */
    Status = NvDisplayHwShutdown (DriverHandle, ControllerHandle);

Disable:
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

    if (MaxEmcFrequencyForced) {
      Status1 = ForceMaxEmcFrequency (DriverHandle, ControllerHandle, FALSE);
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      MaxEmcFrequencyForced = FALSE;
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
  IN EFI_HANDLE  DriverHandle,
  IN EFI_HANDLE  ControllerHandle
  )
{
  return NvDisplayControllerStart (
           DriverHandle,
           ControllerHandle,
           EnableHwT264
           );
}
