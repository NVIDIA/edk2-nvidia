/** @file

  SMMUv3 Driver data structures and definitions

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SMMU_V3_DXE_PRIVATE_H_
#define _SMMU_V3_DXE_PRIVATE_H_

#include <Protocol/SmmuV3Protocol.h>

#define SMMU_V3_CONTROLLER_SIGNATURE  SIGNATURE_32('S','M','U','3')

#define FIELD_PREP(value, mask, shift)  (((value) & (mask)) << (shift))

#define SMMU_V3_CR0_OFFSET        0x20           // Control Register 0
#define SMMU_V3_CR0ACK_OFFSET     0x24           // Control Register 0 Acknowledgment
#define SMMU_V3_CR0_SMMUEN_SHIFT  0
#define SMMU_V3_CR0_SMMUEN_MASK   0x1
#define SMMU_V3_CR0_SMMUEN_BIT    0

#define SMMU_V3_GBPA_OFFSET         0x44         // Global Bypass
#define SMMU_V3_GBPA_UPDATE_SHIFT   31
#define SMMU_V3_GBPA_UPDATE_MASK    0x1
#define SMMU_V3_GBPA_ABORT_SHIFT    20
#define SMMU_V3_GBPA_ABORT_MASK     0x1
#define SMMU_V3_GBPA_INSTCFG_SHIFT  18
#define SMMU_V3_GBPA_INSTCFG_MASK   0x3
#define SMMU_V3_GBPA_PRIVCFG_SHIFT  16
#define SMMU_V3_GBPA_PRIVCFG_MASK   0x3
#define SMMU_V3_GBPA_SHCFG_SHIFT    12
#define SMMU_V3_GBPA_SHCFG_MASK     0x3
#define SMMU_V3_GBPA_ALLOCFG_SHIFT  8
#define SMMU_V3_GBPA_ALLOCFG_MASK   0xF
#define SMMU_V3_GBPA_MTCFG_SHIFT    4
#define SMMU_V3_GBPA_MTCFG_MASK     0x1

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
