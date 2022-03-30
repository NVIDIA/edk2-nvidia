/** @file
  GOP Driver specific Query and Response functions

  Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _GOPDRIVERDATA_H_
#define _GOPDRIVERDATA_H_

#include <Protocol/DeviceTreeNode.h>
#include <Protocol/PlatformToDriverConfiguration.h>

/**

  GOP (display) driver specific Response function

  @param  ParameterBlock        ParameterBlock structure which
                                contains details about the
                                configuration parameters specific to
                                the ParameterTypeGuid.
  @param  ConfigurationAction   The driver tells the platform what
                                action is required for ParameterBlock to
                                take effect.
  @retval EFI_SUCCESS           The platform return parameter
                                information for ControllerHandle.
  @retval others                Error occurred

**/

EFI_STATUS
EFIAPI
ResponseGopParameters (
  IN CONST  VOID                              *ParameterBlock,
  IN CONST  EFI_PLATFORM_CONFIGURATION_ACTION ConfigurationAction
);

/**

  GOP (display) driver specific Query function

  @param ParameterBlock         ParameterBlock structure which
                                contains details about the
                                configuration parameters specific to
                                the ParameterTypeGuid.
  @param ParameterBlockSize     Size of ParameterBlock
  @param DtNode                 Device Tree Node used to extract
                                the parameter information
  @retval EFI_SUCCESS           The platform return parameter
                                information for ControllerHandle.
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
QueryGopParameters (
  IN OUT   VOID                                   **ParameterBlock,
  IN OUT   UINTN                                  *ParameterBlockSize,
  IN       CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DtNode
);

#endif /* _GOPDRIVERDATA_H_ */
