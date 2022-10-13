/** @file

  Platform To Driver Configuration Protocol private structures

  Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORMTODRIVERCONFIG_H_
#define PLATFORMTODRIVERCONFIG_H_

#include <Protocol/DeviceTreeNode.h>
#include <Protocol/PlatformToDriverConfiguration.h>

typedef
EFI_STATUS
(EFIAPI *QUERY)(
  IN OUT   VOID                                   **ParameterBlock,
  IN       UINTN                                  *ParameterBlockSize,
  IN       CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *Node
  );

typedef
EFI_STATUS
(EFIAPI *RESPONSE)(
  IN CONST  VOID                              *ParameterBlock,
  IN CONST  EFI_PLATFORM_CONFIGURATION_ACTION ConfigurationAction
  );

typedef struct {
  EFI_GUID    *DeviceGuid;
  QUERY       Query;
  RESPONSE    Response;
} GUID_DEVICEFUNCPTR_MAPPING;

#endif /* PLATFORMTODRIVERCONFIG_H_ */
