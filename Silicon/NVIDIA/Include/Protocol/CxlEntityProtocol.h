/** @file
*
*  Cxl Entity Protocol definition.
*
*  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __CXL_ENTITY_PROTOCOL_H__
#define __CXL_ENTITY_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>

#include <Protocol/PciIo.h>

typedef enum {
  CXL_ROOT_PORT = 0,
  CXL_UPSTREAM_PORT,
  CXL_DOWNSTREAM_PORT,
  CXL_TYPE1_DEVICE,
  CXL_TYPE2_DEVICE,
  CXL_TYPE3_DEVICE,

  CXL_ENTITY_TYPE_MAX
} CXL_ENTITY_TYPE;

typedef struct {
  UINT32    Devices;             // DVSEC ID 0x0000
  UINT32    NonCxlFunctions;     // DVSEC ID 0x0002
  UINT32    ExtensionsForPorts;  // DVSEC ID 0x0003
  UINT32    GpfForPorts;         // DVSEC ID 0x0004
  UINT32    GpfForDevices;       // DVSEC ID 0x0005
  UINT32    FlexBus;             // DVSEC ID 0x0007
  UINT32    RegisterLocator;     // DVSEC ID 0x0008
  UINT32    MLD;                 // DVSEC ID 0x0009
  UINT32    TestCapability;      // DVSEC ID 0x000A
} DVSEC_OFFSETS;

typedef struct {
  UINT64    Base;
  UINT64    Length;
  //  UINT8   Flags; // To save Non-volatile, etc.
} MEMORY_RANGE_INFO;

//
// Define for forward reference.
//
typedef struct _CXL_ENTITY_PROTOCOL CXL_ENTITY_PROTOCOL;

// CXL_ENTITY_PROTOCOL protocol structure.
struct _CXL_ENTITY_PROTOCOL {
  EFI_HANDLE             Handle;
  CXL_ENTITY_TYPE        Type;

  // To save hierarchy
  //  CXL_ENTITY_PROTOCOL        *Parent;
  //  UINT32                     ChildCount;
  //  CXL_ENTITY_PROTOCOL        *Children;

  EFI_PCI_IO_PROTOCOL    *PciIo;
  DVSEC_OFFSETS          DvsecOffsets;

  // Proximity Info
  BOOLEAN                ProximityDomainValid;
  UINT32                 ProximityDomain;

  // Memory Info
  //  UINT32                     DpaRangeCount;
  //  MEMORY_RANGE_INFO          *DpaRanges;
  //  UINT32                     HpaRangeCount;
  //  MEMORY_RANGE_INFO          *HpaRanges;
};

#endif /* __CXL_ENTITY_PROTOCOL_H__ */
