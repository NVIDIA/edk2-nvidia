/** @file
*
*  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
*  Portions provided under the following terms:
*  Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#ifndef __DEVICE_DISCOVERY_DRIVER_LIB_H__
#define __DEVICE_DISCOVERY_DRIVER_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Protocol/DeviceTreeNode.h>

///
/// Required to be implemented by user of library
///

///
/// Describes compatibility mapping regions
/// Terminated with NULL Compatibility string
///
typedef struct {
  CONST CHAR8            *Compatibility;
  EFI_GUID               *DeviceType;
} NVIDIA_COMPATIBILITY_MAPPING;
extern NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[];

typedef struct {
  CONST CHAR16           *DriverName;
  BOOLEAN                UseDriverBinding;
  BOOLEAN                AutoEnableClocks;
  BOOLEAN                AutoDeassertReset;
  BOOLEAN                AutoResetModule;
  BOOLEAN                AutoDeassertPg;
  BOOLEAN                SkipEdkiiNondiscoverableInstall;
  BOOLEAN                AutoDeinitControllerOnExitBootServices;
} NVIDIA_DEVICE_DISCOVERY_CONFIG;
extern NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig;

typedef enum {
  DeviceDiscoveryDriverStart,
  DeviceDiscoveryDeviceTreeCompatibility,
  DeviceDiscoveryDriverBindingSupported,
  DeviceDiscoveryDriverBindingStart,
  DeviceDiscoveryDriverBindingStop,
  DeviceDiscoveryMax
} NVIDIA_DEVICE_DISCOVERY_PHASES;

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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES         Phase,
  IN  EFI_HANDLE                             DriverHandle,
  IN  EFI_HANDLE                             ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode OPTIONAL
  );


///
/// Library service functions
///

/**
  Retrieves the count of MMIO regions on this controller

  @param[in]  ControllerHandle         Handle of the controller.
  @param[out] RegionCount              Pointer that will contain the number of MMIO regions

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryGetMmioRegionCount (
  IN  EFI_HANDLE ControllerHandle,
  OUT UINTN      *RegionCount
  );

/**
  Retrieves the info for a MMIO region on this controller

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  Region                   Region of interest.
  @param[out] RegionBase               Pointer that will contain the base address of the region.
  @param[out] RegionSize               Pointer that will contain the size of the region.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Region is not valid
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryGetMmioRegion (
  IN  EFI_HANDLE           ControllerHandle,
  IN  UINTN                Region,
  OUT EFI_PHYSICAL_ADDRESS *RegionBase,
  OUT UINTN                *RegionSize
  );

/**
  Retrieves the reset id for the specified reset name

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  ResetkName               String for the reset name.
  @param[out] ResetId                  Returns the reset id

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Reset name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryGetResetId (
  IN  EFI_HANDLE           ControllerHandle,
  IN  CONST CHAR8          *ResetName,
  OUT UINT32               *ResetId
);

/**
  Configures the reset with the specified reset name

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  ResetName                String for the reset name.
  @param[in]  Enable                   TRUE to enable, FALSE to disable.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Reset name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryConfigReset (
  IN  EFI_HANDLE           ControllerHandle,
  IN  CONST CHAR8          *ResetName,
  IN  BOOLEAN              Enable
);

/**
  Retrieves gets the clock id for the specified clock name

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  ClockName                String for the clock name.
  @param[out] ClockId                  Returns the clock id that can be used in the SCMI protocol.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Clock name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryGetClockId (
  IN  EFI_HANDLE           ControllerHandle,
  IN  CONST CHAR8          *ClockName,
  OUT UINT32               *ClockId
);

/**
  Enables the clock with the specified clock name

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  ClockName                String for the clock name.
  @param[in]  Enable                   TRUE to enable, FALSE to disable.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Clock name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryEnableClock (
  IN  EFI_HANDLE           ControllerHandle,
  IN  CONST CHAR8          *ClockName,
  IN  BOOLEAN              Enable
  );

/**
  Sets the clock frequency for the clock with the specified clock name

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  ClockName                String for the clock name.
  @param[in]  Frequency                Frequency in hertz.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Clock name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoverySetClockFreq (
  IN  EFI_HANDLE           ControllerHandle,
  IN  CONST CHAR8          *ClockName,
  IN  UINT64               Frequency
);

/**
  Gets the clock frequency for the clock with the specified clock name

  @param[in]  ControllerHandle   IN  UINT64               Frequency
          Handle of the controller.
  @param[in]  ClockName                String for the clock name.
  @param[out] Frequency                Frequency in hertz.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Clock name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryGetClockFreq (
  IN  EFI_HANDLE           ControllerHandle,
  IN  CONST CHAR8          *ClockName,
  OUT UINT64               *Frequency
  );
/**
  Sets the parent clock for a given clock with names

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  ClockName                String for the clock name.
  @param[in]  ParentClockName          String for the parent clock name.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Clock name not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoverySetClockParent (
  IN  EFI_HANDLE           ControllerHandle,
  IN  CONST CHAR8          *ClockName,
  IN  CONST CHAR8          *ParentClockName
  );


/**
  Enable device tree based prod settings

  @param[in]  ControllerHandle         Handle of the controller.
  @param[in]  DeviceTreeNode           Device tree information
  @param[in]  ProdSetting              String for the prod settings.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_NOT_FOUND            Prod setting not found on controller
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoverySetProd (
  IN  EFI_HANDLE                             ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode,
  IN  CONST CHAR8                            *ProdSetting
  );

/**
  Initialize the Device Discovery Driver

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
DeviceDiscoveryDriverInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  );

#endif //__DEVICE_DISCOVERY_DRIVER_LIB_H__
