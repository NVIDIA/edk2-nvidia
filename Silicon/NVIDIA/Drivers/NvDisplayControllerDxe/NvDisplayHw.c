/** @file

  NV Display Controller Driver - HW

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/ClockNodeProtocol.h>

/**
  Assert or deassert display resets.

  The Resets array must be terminated by a NULL entry.

  @param[in] DriverHandle      Handle to the driver.
  @param[in] ControllerHandle  Handle to the controller.
  @param[in] Resets            Names of the resets.
  @param[in] Assert            Assert/deassert the reset signal.

  @retval EFI_SUCCESS  Operation successful.
  @retval others       Error(s) occurred.
*/
EFI_STATUS
NvDisplayAssertResets (
  IN CONST EFI_HANDLE    DriverHandle,
  IN CONST EFI_HANDLE    ControllerHandle,
  IN CONST CHAR8 *CONST  Resets[],
  IN CONST BOOLEAN       Assert
  )
{
  EFI_STATUS  Status;
  UINTN       Index;

  for (Index = 0; Resets[Index] != NULL; ++Index) {
    Status = DeviceDiscoveryConfigReset (ControllerHandle, Resets[Index], Assert);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to %a reset %a: %r \r\n",
        __FUNCTION__,
        Assert ? "assert" : "deassert",
        Resets[Index],
        Status
        ));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Enable or disable display clocks. In addition, set given clock
  parents before enable.

  Both Clocks and ClockParents arrays must be terminated by NULL
  entries.

  @param[in] DriverHandle      Handle to the driver.
  @param[in] ControllerHandle  Handle to the controller.
  @param[in] Clocks            Names of the clocks.
  @param[in] ClockParents      Child-parent clock pairs to set.
  @param[in] Enable            Enable/disable the clocks.

  @return EFI_SUCCESS    Clocks successfully enabled/disabled.
  @return !=EFI_SUCCESS  An error occurred.
*/
EFI_STATUS
NvDisplayEnableClocks (
  IN CONST EFI_HANDLE    DriverHandle,
  IN CONST EFI_HANDLE    ControllerHandle,
  IN CONST CHAR8 *CONST  Clocks[],
  IN CONST CHAR8 *CONST  ClockParents[][2],
  IN CONST BOOLEAN       Enable
  )
{
  EFI_STATUS                  Status;
  UINTN                       Index;
  NVIDIA_CLOCK_NODE_PROTOCOL  *ClockNodeProtocol;

  if (Enable) {
    for (Index = 0; (ClockParents[Index][0] != NULL) && (ClockParents[Index][1] != NULL); ++Index) {
      Status = DeviceDiscoverySetClockParent (
                 ControllerHandle,
                 ClockParents[Index][0],
                 ClockParents[Index][1]
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to set parent of clock '%a' to '%a': %r\r\n",
          __FUNCTION__,
          ClockParents[Index][0],
          ClockParents[Index][1],
          Status
          ));
        return Status;
      }
    }

    for (Index = 0; Clocks[Index] != NULL; ++Index) {
      Status = DeviceDiscoveryEnableClock (ControllerHandle, Clocks[Index], TRUE);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to enable clock '%a': %r\r\n",
          __FUNCTION__,
          Clocks[Index],
          Status
          ));
        return Status;
      }
    }
  } else {
    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gNVIDIAClockNodeProtocolGuid,
                    (VOID **)&ClockNodeProtocol,
                    DriverHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to lookup clock node protocol: %r\r\n",
        __FUNCTION__,
        Status
        ));
      return Status;
    }

    Status = ClockNodeProtocol->DisableAll (ClockNodeProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to disable clocks: %r\r\n",
        __FUNCTION__,
        Status
        ));
      return Status;
    }
  }

  return EFI_SUCCESS;
}
