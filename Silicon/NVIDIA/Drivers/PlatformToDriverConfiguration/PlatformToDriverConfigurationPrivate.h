/** @file

  Platform To Driver Configuration Protocol private structures

  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

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
  EFI_GUID  *DeviceGuid;
  QUERY     Query;
  RESPONSE  Response;
} GUID_DEVICEFUNCPTR_MAPPING;

#endif /* PLATFORMTODRIVERCONFIG_H_ */
