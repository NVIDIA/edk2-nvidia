/** @file

  SMMUv3 Driver data structures and definitions

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SMMU_V3_DXE_PRIVATE_H_
#define _SMMU_V3_DXE_PRIVATE_H_

#include <Protocol/SmmuV3Protocol.h>

#define SMMU_V3_CONTROLLER_SIGNATURE  SIGNATURE_32('S','M','U','3')

typedef struct {
  UINT32                               Signature;
  EFI_PHYSICAL_ADDRESS                 BaseAddress;
  VOID                                 *DeviceTreeBase;
  INT32                                NodeOffset;
  EFI_EVENT                            ExitBootServicesEvent;
  NVIDIA_SMMUV3_CONTROLLER_PROTOCOL    SmmuV3ControllerProtocol;
} SMMU_V3_CONTROLLER_PRIVATE_DATA;

#define SMMU_V3_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL(a)  CR(a, SMMU_V3_CONTROLLER_PRIVATE_DATA, SmmuV3ControllerProtocol, SMMU_V3_CONTROLLER_SIGNATURE)

#endif
