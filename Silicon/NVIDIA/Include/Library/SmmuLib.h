/** @file

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SMMU_LIB_H__
#define __SMMU_LIB_H__

#include <Protocol/IoMmu.h>
#include <Protocol/SmmuV3Protocol.h>

typedef struct {
  UINT32    StreamId;
  UINT32    SmmuV3pHandle;
} SOURCE_ID;

EFI_STATUS
EFIAPI
GetSourceIdFromPciHandle (
  IN  EFI_HANDLE                PciDeviceHandle,
  OUT SOURCE_ID                 *SourceId,
  OUT SMMU_V3_TRANSLATION_MODE  *TranslationMode
  );

// Max Legal 48 bit addressable space
#define DMA_MEMORY_TOP  MAX_ALLOC_ADDRESS

#define MAP_INFO_SIGNATURE  SIGNATURE_32 ('D', 'M', 'A', 'P')
typedef struct {
  UINT32                   Signature;
  LIST_ENTRY               Link;
  EDKII_IOMMU_OPERATION    Operation;
  UINTN                    NumberOfBytes;
  UINTN                    NumberOfPages;
  EFI_PHYSICAL_ADDRESS     HostAddress;
  EFI_PHYSICAL_ADDRESS     DeviceAddress;
} MAP_INFO;
#define MAP_INFO_FROM_LINK(a)  CR (a, MAP_INFO, Link, MAP_INFO_SIGNATURE)

#endif //__SMMU_LIB_H__
