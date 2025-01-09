/** @file

  NVIDIA GPU support structures and prototypes.

  SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

///
/// GPUMemoryInfo.h: GPU Memory information.
///

#ifndef __GPU_MEMORY_INFO_H__
#define __GPU_MEMORY_INFO_H__

enum {
  MAX_GPU_MEMORY_INFO_PROPERTY_ENTRIES = 8
};

enum {
  GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_BASE_PA            = 0,
  GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_PXM_START          = 1,
  GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_PXM_COUNT          = 2,
  GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_SIZE               = 3,
  GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_BASE_PA            = 4,
  GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_SIZE               = 5,
  GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_PXM                = 6,
  GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_RETIRED_PAGES_ADDR = 7
};

typedef struct PciLocationInfo {
  UINTN    Segment;
  UINTN    Bus;
  UINTN    Device;
  UINTN    Function;
} PCI_LOCATION_INFO;

typedef struct ATSRangeInfo {
  ///
  /// HBM memory range for connected GPU over C2C
  ///
  EFI_PHYSICAL_ADDRESS    HbmRangeStart;
  UINTN                   HbmRangeSize;
  UINT8                   ProximityDomainStart;
  UINT8                   NumProximityDomains;
} ATS_RANGE_INFO;

/* Use Configuration Manager style memory info structure */
typedef struct CmArmNvdaGpuMemoryInfoPropertyEntry {
  /// Property Name string
  CHAR8     *PropertyName;

  /// Property Value
  UINT64    PropertyValue;
} CM_ARM_NVDA_GPU_MEMORY_INFO_PROPERTY_INFO;

typedef struct CmArmNvdaGpuMemoryInfo {
  /// GPU Segment Number
  UINT8                                        SegmentNumber;

  /// Number of MemoryInfoPropertyEntries
  UINT8                                        PropertyEntryCount;

  // Array of GPU MemoryInfo Property Entries
  CM_ARM_NVDA_GPU_MEMORY_INFO_PROPERTY_INFO    Entry[MAX_GPU_MEMORY_INFO_PROPERTY_ENTRIES];
} CM_ARM_NVDA_GPU_MEMORY_INFO;

typedef CM_ARM_NVDA_GPU_MEMORY_INFO GPU_MEMORY_INFO;

/** Returns the PCI Location infromation for the controller

  @param ControllerHandle Controller handle to get PCI Location Information for.
  @param PciLocationInfo  Handle for PciLocationInfo
  @retval Status
            EFI_SUCCESS
            EFI_INVALID_PARAMETER
            (passthrough OpenProtocol)
            (passthrough PciIo->GetLocation)
*/
EFI_STATUS
EFIAPI
GetGPUPciLocation (
  IN EFI_HANDLE          ControllerHandle,
  OUT PCI_LOCATION_INFO  **PciLocationInfo
  );

/** Returns the Memory Size for the GPU

    @retval UINT64 containing the GPU MemSize
*/
UINT64
EFIAPI
GetGPUMemSize (
  IN EFI_HANDLE  ControllerHandle
  );

/** Allocate and configure GPU Memory Info structure
    @param  ControllerHandle  Controller Handle to retrieve memory information for
    @param  MemInfo           Memory Information structure for the GPU
    @retval Status
            EFI_SUCCESS  Memory Information successful
            EFI_NOT_FOUND Controller information invalid
            EFI_OUT_OF_RESORUCES
*/
EFI_STATUS
EFIAPI
GetGPUMemoryInfo (
  IN EFI_HANDLE        ControllerHandle,
  OUT GPU_MEMORY_INFO  **MemInfo
  );

/* Retrieve the ATS infromation from the plaform
    @param  ControllerHandle  Controller Handle to retrieve memory information for
    @param  MemInfo           Memory Information structure for the GPU
    @retval Status
            EFI_SUCCESS  Memory Information successful
            EFI_NOT_FOUND Controller information invalid
            EFI_OUT_OF_RESORUCES
*/
EFI_STATUS
EFIAPI
GetControllerATSRangeInfo (
  IN EFI_HANDLE       ControllerHandle,
  OUT ATS_RANGE_INFO  *ATSRangeInfoData
  );

#endif //__GPU_MEMORY_INFO_H__
