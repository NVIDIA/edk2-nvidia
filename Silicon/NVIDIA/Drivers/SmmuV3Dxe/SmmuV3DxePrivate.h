/** @file

  SMMUv3 Driver data structures and definitions

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SMMU_V3_DXE_PRIVATE_H_
#define _SMMU_V3_DXE_PRIVATE_H_

typedef struct {
  EFI_PHYSICAL_ADDRESS    BaseAddress;
  VOID                    *DeviceTreeBase;
  INT32                   NodeOffset;
  UINT32                  PHandle;
} SMMU_V3_CONTROLLER_PRIVATE_DATA;

#endif
