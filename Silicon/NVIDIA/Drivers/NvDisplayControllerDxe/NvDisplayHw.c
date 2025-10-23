/** @file

  NV Display Controller Driver - HW

  SPDX-FileCopyrightText: Copyright (c) 2024-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/ClockNodeProtocol.h>

#define DISPLAY_HEAD_COUNT     8
#define DISPLAY_SOR_COUNT      8
#define DISPLAY_FE_SW_SYS_CAP  0x00030000
#define DISPLAY_FE_CMGR_CLK_RG(Index)   (0x00002200 + (Index) * SIZE_2KB)
#define DISPLAY_FE_CMGR_CLK_SOR(Index)  (0x00002300 + (Index) * SIZE_2KB)
#define DISPLAY_FE_CMGR_CLK_SF(Index)   (0x00002420 + (Index) * SIZE_2KB)

#define GetDisplayFeSysCapHeadExists(FeSysCap, Index)         (BOOLEAN)BitFieldRead32 ((FeSysCap), (Index), (Index))
#define GetDisplayFeSysCapSorExists(FeSysCap, Index)          (BOOLEAN)BitFieldRead32 ((FeSysCap), (Index) + 8, (Index) + 8)
#define SetDisplayFeCmgrClkRgForceSafeEnable(FeCmgrClkRg)     BitFieldWrite32 ((FeCmgrClkRg), 11, 11, 1)
#define SetDisplayFeCmgrClkSfSafeCtrlBypass(FeCmgrClkSf)      BitFieldWrite32 ((FeCmgrClkSf), 16, 17, 1)
#define SetDisplayFeCmgrClkSorModeBypassDpSafe(FeCmgrClkSor)  BitFieldWrite32 ((FeCmgrClkSor), 16, 17, 2)

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
  Reset given clock parent to safe osc clock. In addition, set the
  child clock frequency to match osc in order to ensure any clock
  divider configuration is reset to 1:1 as well.

  @param[in] DriverHandle      Handle to the driver.
  @param[in] ControllerHandle  Handle to the controller.
  @param[in] ClockName         Name of the clock.

  @retval EFI_SUCCESS    Clock reset successfully.
  @retval EFI_NOT_FOUND  The osc clock was not found on the controller.
  @retval EFI_NOT_FOUND  The clock name not found on controller.
  @retval others         Other errors occurred.
*/
STATIC
EFI_STATUS
ResetClockToOsc (
  IN CONST EFI_HANDLE    DriverHandle,
  IN CONST EFI_HANDLE    ControllerHandle,
  IN CONST CHAR8 *CONST  Clock
  )
{
  EFI_STATUS  Status;
  UINT64      OscRateHz;

  Status = DeviceDiscoveryGetClockFreq (ControllerHandle, "osc_clk", &OscRateHz);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to retrieve frequency of 'osc_clk': %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  Status = DeviceDiscoverySetClockParent (ControllerHandle, Clock, "osc_clk");
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to switch parent of '%a' to 'osc_clk': %r\r\n",
      __FUNCTION__,
      Clock,
      Status
      ));
    return Status;
  }

  Status = DeviceDiscoverySetClockFreq (ControllerHandle, Clock, OscRateHz);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set frequency of '%a' to %lu Hz: %r\r\n",
      __FUNCTION__,
      Clock,
      OscRateHz,
      Status
      ));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Retrieve rates of the given clock with specified parent clocks, then
  reset the clock parent and rate to safe osc clock.

  @param[in]  DriverHandle      Handle to the driver.
  @param[in]  ControllerHandle  Handle to the controller.
  @param[in]  ClockName         Name of the clock.
  @param[in]  ParentClockNames  Name of the parent clocks.
  @param[out] RatesKhz          Rates of the clock with corresponding parents.

  @retval EFI_SUCCESS    Rates retrieved successfully.
  @retval EFI_NOT_FOUND  The osc clock was not found on the controller.
  @retval EFI_NOT_FOUND  The clock name not found on controller.
  @retval EFI_NOT_FOUND  Parent clock name not found on controller.
  @retval others         Other errors occurred.
*/
EFI_STATUS
NvDisplayGetClockRatesWithParentsAndReset (
  IN  CONST EFI_HANDLE           DriverHandle,
  IN  CONST EFI_HANDLE           ControllerHandle,
  IN  CONST CHAR8        *CONST  ClockName,
  IN  CONST CHAR8 *CONST *CONST  ParentClockNames,
  OUT UINT32             *CONST  RatesKhz
  )
{
  EFI_STATUS  Status;
  UINTN       Index;
  UINT64      RateHz;

  for (Index = 0; ParentClockNames[Index] != NULL; ++Index) {
    Status = ResetClockToOsc (DriverHandle, ControllerHandle, ClockName);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = DeviceDiscoverySetClockParent (
               ControllerHandle,
               ClockName,
               ParentClockNames[Index]
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to switch parent of '%a' to '%a': %r\r\n",
        __FUNCTION__,
        ClockName,
        ParentClockNames[Index],
        Status
        ));
      return Status;
    }

    Status = DeviceDiscoveryGetClockFreq (ControllerHandle, ClockName, &RateHz);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to retrieve frequency of '%a': %r\r\n",
        __FUNCTION__,
        ClockName,
        Status
        ));
      return Status;
    }

    RatesKhz[Index] = RateHz / 1000;
  }

  return ResetClockToOsc (DriverHandle, ControllerHandle, ClockName);
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

/**
  Shutdown active display HW before reset to prevent a lingering bad
  state.

  @param[in] DriverHandle      Handle to the driver.
  @param[in] ControllerHandle  Handle to the controller.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Error(s) occurred.
*/
EFI_STATUS
NvDisplayHwShutdown (
  IN CONST EFI_HANDLE  DriverHandle,
  IN CONST EFI_HANDLE  ControllerHandle
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  Base;
  UINTN                 Size;
  UINTN                 Index;
  UINT32                FeSysCap, Data32;
  CONST UINTN           DisplayRegion = 0;

  Status = DeviceDiscoveryGetMmioRegion (
             ControllerHandle,
             DisplayRegion,
             &Base,
             &Size
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

  FeSysCap = MmioRead32 (Base + DISPLAY_FE_SW_SYS_CAP);

  for (Index = 0; Index < DISPLAY_HEAD_COUNT; ++Index) {
    if (GetDisplayFeSysCapHeadExists (FeSysCap, Index)) {
      Data32 = MmioRead32 (Base + DISPLAY_FE_CMGR_CLK_RG (Index));
      Data32 = SetDisplayFeCmgrClkRgForceSafeEnable (Data32);
      MmioWrite32 (Base + DISPLAY_FE_CMGR_CLK_RG (Index), Data32);

      Data32 = MmioRead32 (Base + DISPLAY_FE_CMGR_CLK_SF (Index));
      Data32 = SetDisplayFeCmgrClkSfSafeCtrlBypass (Data32);
      MmioWrite32 (Base + DISPLAY_FE_CMGR_CLK_SF (Index), Data32);
    }
  }

  for (Index = 0; Index < DISPLAY_SOR_COUNT; ++Index) {
    if (GetDisplayFeSysCapSorExists (FeSysCap, Index)) {
      Data32 = MmioRead32 (Base + DISPLAY_FE_CMGR_CLK_SOR (Index));
      Data32 = SetDisplayFeCmgrClkSorModeBypassDpSafe (Data32);
      MmioWrite32 (Base + DISPLAY_FE_CMGR_CLK_SOR (Index), Data32);
    }
  }

  return EFI_SUCCESS;
}
