/** @file
  Configuration Manager Data of Static Resource Affinity Table

  SPDX-FileCopyrightText: Copyright (c) 2020-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "SratParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/DebugLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NumaInfoLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/PciRootBridgeConfigurationIo.h>

#include <TH500/TH500Definitions.h>

/**
  Check whether a memory region is fully covered by an existing DRAM region.

  Firmware-usable carveouts are published to the OS as EFI_RESOURCE_MEMORY_RESERVED.
  Such ranges still enter memblock.memory on arm64. If one is not covered by a SRAT
  memory-affinity row the kernel treats it as an "invalid memblk node" and disables
  NUMA. Add one extra memory-affinity row for each such carveout.

  @param[in]  ResourceInfo  Platform resource info.
  @param[in]  Region        Region to test.

  @retval TRUE   Region is fully contained in some DramRegions[] entry.
  @retval FALSE  Region is not covered by any DramRegions[] entry.
**/
STATIC
BOOLEAN
IsRegionCoveredByDramRegion (
  IN CONST TEGRA_RESOURCE_INFO  *ResourceInfo,
  IN CONST NVDA_MEMORY_REGION   *Region
  )
{
  UINTN                 Index;
  EFI_PHYSICAL_ADDRESS  RegionStart;
  EFI_PHYSICAL_ADDRESS  RegionEnd;
  EFI_PHYSICAL_ADDRESS  DramStart;
  EFI_PHYSICAL_ADDRESS  DramEnd;

  RegionStart = Region->MemoryBaseAddress;
  RegionEnd   = Region->MemoryBaseAddress + Region->MemoryLength;

  for (Index = 0; Index < ResourceInfo->DramRegionsCount; Index++) {
    DramStart = ResourceInfo->DramRegions[Index].MemoryBaseAddress;
    DramEnd   = DramStart + ResourceInfo->DramRegions[Index].MemoryLength;
    if (DramEnd <= DramStart) {
      // Zero-length or wrapping DRAM region cannot cover anything; skip it.
      continue;
    }

    if ((RegionStart >= DramStart) && (RegionEnd <= DramEnd)) {
      // Containment in a DRAM region implies the same TH500_AMAP_GET_SOCKET() PXM.
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Decide whether a firmware-usable carveout needs its own SRAT memory-affinity row.

  Shared predicate used by both the counting and emitting passes so the two stay in
  lockstep (preserving the MemoryAffinityInfoIndex == MemoryAffinityInfoCount assert).
  A row is needed only for a non-empty, non-wrapping carveout that is not already
  covered by an existing DRAM memory-affinity row.

  @param[in]  ResourceInfo  Platform resource info.
  @param[in]  Region        Usable carveout region to test.

  @retval TRUE   The carveout needs an additional memory-affinity row.
  @retval FALSE  Skip it (empty, address-wrapping, or already covered).
**/
STATIC
BOOLEAN
UsableCarveoutNeedsAffinity (
  IN CONST TEGRA_RESOURCE_INFO  *ResourceInfo,
  IN CONST NVDA_MEMORY_REGION   *Region
  )
{
  if (Region->MemoryLength == 0) {
    return FALSE;
  }

  // Guard against an address range that wraps UINT64; we cannot reason about its
  // coverage, and it must not silently differ between the count and emit passes.
  if ((Region->MemoryBaseAddress + Region->MemoryLength) <= Region->MemoryBaseAddress) {
    return FALSE;
  }

  return !IsRegionCoveredByDramRegion (ResourceInfo, Region);
}

EFI_STATUS
EFIAPI
SratParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                                      Status;
  CM_STD_OBJ_ACPI_TABLE_INFO                      AcpiTableHeader;
  CM_ARCH_COMMON_MEMORY_AFFINITY_INFO             *MemoryAffinityInfo;
  UINTN                                           MemoryAffinityInfoCount;
  UINTN                                           MemoryAffinityInfoIndex;
  CM_ARCH_COMMON_GENERIC_INITIATOR_AFFINITY_INFO  *GenericInitiatorAffinityInfo;
  UINTN                                           GenericInitiatorAffinityInfoCount;
  UINTN                                           GenericInitiatorAffinityInfoIndex;
  CM_ARCH_COMMON_DEVICE_HANDLE_PCI                *DeviceHandlePciInfo;
  CM_OBJECT_TOKEN                                 *DeviceHandleTokenMap;
  VOID                                            *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO                    *PlatformResourceInfo;
  CM_OBJ_DESCRIPTOR                               Desc;
  UINT32                                          GiIndex;
  UINT32                                          ProximityDomain;
  UINT32                                          MaxProximityDomain;
  UINT32                                          NumberOfInitiatorDomains;
  UINT32                                          NumberOfTargetDomains;
  NUMA_INFO_DOMAIN_INFO                           DomainInfo;
  UINTN                                           UsableCarveoutIndex;
  UINTN                                           UsableCarveoutAffinityCount;
  NVDA_MEMORY_REGION                              *Region;

  // Get platform resource info
  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  } else {
    DEBUG ((DEBUG_ERROR, "Failed to get PlatformResourceInfo\n"));
    return EFI_NOT_FOUND;
  }

  Status = NumaInfoGetDomainLimits (&MaxProximityDomain, &NumberOfInitiatorDomains, &NumberOfTargetDomains);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: NumaInfoGetDomainLimits failed: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  // Create a ACPI Table Entry

  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_6_SYSTEM_RESOURCE_AFFINITY_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_6_6_SYSTEM_RESOURCE_AFFINITY_TABLE_REVISION;
  AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSrat);
  AcpiTableHeader.AcpiTableData      = NULL;
  AcpiTableHeader.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  AcpiTableHeader.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader.MinorRevision      = 0;

  Status = NvAddAcpiTableGenerator (ParserHandle, &AcpiTableHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add the SRAT SSDT table\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  MemoryAffinityInfoCount = PlatformResourceInfo->ResourceInfo->DramRegionsCount;
  // Count will include CPU targets that will not generate memory entries
  // These will be removed when processing the initiator domains later
  MemoryAffinityInfoCount += NumberOfTargetDomains;

  // Firmware-usable carveouts are published to the OS as EFI_RESOURCE_MEMORY_RESERVED
  // and still enter memblock.memory on arm64. Any usable carveout that is not covered
  // by a DRAM memory-affinity row (e.g. one landing in the gap an EGM carveout leaves
  // in a socket's DRAM coverage) has no NUMA node, which makes Linux 6.8
  // numa_register_nodes() reject it ("invalid memblk node") and disable NUMA, leaving
  // GPU PCIe numa_node=-1. Reserve one extra memory-affinity row for each such carveout.
  UsableCarveoutAffinityCount = 0;
  for (UsableCarveoutIndex = 0;
       UsableCarveoutIndex < PlatformResourceInfo->ResourceInfo->UsableCarveoutRegionsCount;
       UsableCarveoutIndex++)
  {
    Region = &PlatformResourceInfo->ResourceInfo->UsableCarveoutRegions[UsableCarveoutIndex];
    if (UsableCarveoutNeedsAffinity (PlatformResourceInfo->ResourceInfo, Region)) {
      UsableCarveoutAffinityCount++;
    }
  }

  MemoryAffinityInfoCount += UsableCarveoutAffinityCount;

  MemoryAffinityInfo = (CM_ARCH_COMMON_MEMORY_AFFINITY_INFO *)AllocateZeroPool (sizeof (CM_ARCH_COMMON_MEMORY_AFFINITY_INFO) * MemoryAffinityInfoCount);
  if (MemoryAffinityInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory affinity info\r\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  for (MemoryAffinityInfoIndex = 0; MemoryAffinityInfoIndex < PlatformResourceInfo->ResourceInfo->DramRegionsCount; MemoryAffinityInfoIndex++) {
    MemoryAffinityInfo[MemoryAffinityInfoIndex].ProximityDomain = TH500_AMAP_GET_SOCKET (PlatformResourceInfo->ResourceInfo->DramRegions[MemoryAffinityInfoIndex].MemoryBaseAddress);
    MemoryAffinityInfo[MemoryAffinityInfoIndex].BaseAddress     = PlatformResourceInfo->ResourceInfo->DramRegions[MemoryAffinityInfoIndex].MemoryBaseAddress;
    MemoryAffinityInfo[MemoryAffinityInfoIndex].Length          = PlatformResourceInfo->ResourceInfo->DramRegions[MemoryAffinityInfoIndex].MemoryLength;
    MemoryAffinityInfo[MemoryAffinityInfoIndex].Flags           = EFI_ACPI_6_6_MEMORY_ENABLED;
  }

  for (ProximityDomain = 0; ProximityDomain <= MaxProximityDomain; ProximityDomain++) {
    Status = NumaInfoGetDomainDetails (ProximityDomain, &DomainInfo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (DomainInfo.TargetDomain) {
      if (DomainInfo.DeviceType == NUMA_INFO_TYPE_CPU) {
        // CPU initiators do not generate memory entries, the dram regions already cover them
        MemoryAffinityInfoCount--;
        continue;
      }

      MemoryAffinityInfo[MemoryAffinityInfoIndex].ProximityDomain = ProximityDomain;
      MemoryAffinityInfo[MemoryAffinityInfoIndex].Flags           = EFI_ACPI_6_6_MEMORY_ENABLED|EFI_ACPI_6_6_MEMORY_HOT_PLUGGABLE;
      MemoryAffinityInfoIndex++;
    }
  }

  // Emit a memory-affinity row for each uncovered firmware-usable carveout so every
  // present memblock region has a valid NUMA node. The range stays reserved /
  // not OS-allocatable via its EFI_RESOURCE_MEMORY_RESERVED HOB; this only supplies
  // proximity metadata in the carveout's socket domain.
  for (UsableCarveoutIndex = 0;
       UsableCarveoutIndex < PlatformResourceInfo->ResourceInfo->UsableCarveoutRegionsCount;
       UsableCarveoutIndex++)
  {
    Region = &PlatformResourceInfo->ResourceInfo->UsableCarveoutRegions[UsableCarveoutIndex];
    if (!UsableCarveoutNeedsAffinity (PlatformResourceInfo->ResourceInfo, Region)) {
      continue;
    }

    MemoryAffinityInfo[MemoryAffinityInfoIndex].ProximityDomain = TH500_AMAP_GET_SOCKET (Region->MemoryBaseAddress);
    MemoryAffinityInfo[MemoryAffinityInfoIndex].BaseAddress     = Region->MemoryBaseAddress;
    MemoryAffinityInfo[MemoryAffinityInfoIndex].Length          = Region->MemoryLength;
    MemoryAffinityInfo[MemoryAffinityInfoIndex].Flags           = EFI_ACPI_6_4_MEMORY_ENABLED;
    MemoryAffinityInfoIndex++;
  }

  ASSERT (MemoryAffinityInfoIndex == MemoryAffinityInfoCount);

  if (MemoryAffinityInfoCount != 0) {
    Desc.ObjectId = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjMemoryAffinityInfo);
    Desc.Size     = sizeof (CM_ARCH_COMMON_MEMORY_AFFINITY_INFO) * MemoryAffinityInfoCount;
    Desc.Count    = MemoryAffinityInfoCount;
    Desc.Data     = MemoryAffinityInfo;

    Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, NULL);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }
  }

  // Build generic initiators
  GenericInitiatorAffinityInfoCount = NumberOfInitiatorDomains; // Will remove CPU nodes later
  GenericInitiatorAffinityInfo      = (CM_ARCH_COMMON_GENERIC_INITIATOR_AFFINITY_INFO *)AllocateZeroPool (sizeof (CM_ARCH_COMMON_GENERIC_INITIATOR_AFFINITY_INFO) * GenericInitiatorAffinityInfoCount);
  if (GenericInitiatorAffinityInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate generic initiator affinity info\r\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto CleanupAndReturn;
  }

  DeviceHandlePciInfo = (CM_ARCH_COMMON_DEVICE_HANDLE_PCI *)AllocateZeroPool (sizeof (CM_ARCH_COMMON_DEVICE_HANDLE_PCI) * GenericInitiatorAffinityInfoCount);
  if (DeviceHandlePciInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate device handle PCI info\r\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto CleanupAndReturn;
  }

  GenericInitiatorAffinityInfoIndex = 0;
  for (ProximityDomain = 0; ProximityDomain <= MaxProximityDomain; ProximityDomain++) {
    Status = NumaInfoGetDomainDetails (ProximityDomain, &DomainInfo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (DomainInfo.InitiatorDomain) {
      if (DomainInfo.DeviceType == NUMA_INFO_TYPE_CPU) {
        // CPU initiators do not generate generic initiator entries, there are GIC entries for them
        GenericInitiatorAffinityInfoCount--;
        continue;
      }

      // Only support PCI device handle type for now
      if (DomainInfo.DeviceHandleType != EFI_ACPI_6_6_PCI_DEVICE_HANDLE) {
        GenericInitiatorAffinityInfoCount--;
        continue;
      }

      GenericInitiatorAffinityInfo[GenericInitiatorAffinityInfoIndex].ProximityDomain  = ProximityDomain;
      GenericInitiatorAffinityInfo[GenericInitiatorAffinityInfoIndex].Flags            = EFI_ACPI_6_6_GENERIC_INITIATOR_AFFINITY_STRUCTURE_ENABLED|EFI_ACPI_6_6_GENERIC_INITIATOR_AFFINITY_STRUCTURE_ARCHITECTURAL_TRANSACTIONS;
      GenericInitiatorAffinityInfo[GenericInitiatorAffinityInfoIndex].DeviceHandleType = DomainInfo.DeviceHandleType;
      DeviceHandlePciInfo[GenericInitiatorAffinityInfoIndex].SegmentNumber             = DomainInfo.DeviceHandle.Pci.PciSegment;
      DeviceHandlePciInfo[GenericInitiatorAffinityInfoIndex].BusNumber                 = DomainInfo.DeviceHandle.Pci.PciBdfNumber & 0xFF;
      DeviceHandlePciInfo[GenericInitiatorAffinityInfoIndex].DeviceNumber              = (DomainInfo.DeviceHandle.Pci.PciBdfNumber >> 11) & 0x1F;
      DeviceHandlePciInfo[GenericInitiatorAffinityInfoIndex].FunctionNumber            = (DomainInfo.DeviceHandle.Pci.PciBdfNumber >> 8) & 0x7;
      GenericInitiatorAffinityInfoIndex++;
    }
  }

  ASSERT (GenericInitiatorAffinityInfoIndex == GenericInitiatorAffinityInfoCount);

  if (GenericInitiatorAffinityInfoCount != 0) {
    Desc.ObjectId = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjDeviceHandlePci);
    Desc.Size     = sizeof (CM_ARCH_COMMON_DEVICE_HANDLE_PCI) * GenericInitiatorAffinityInfoCount;
    Desc.Count    = GenericInitiatorAffinityInfoCount;
    Desc.Data     = DeviceHandlePciInfo;
    Status        = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, &DeviceHandleTokenMap, NULL);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    for (GiIndex = 0; GiIndex < GenericInitiatorAffinityInfoCount; GiIndex++) {
      GenericInitiatorAffinityInfo[GiIndex].DeviceHandleToken = DeviceHandleTokenMap[GiIndex];
    }

    Desc.ObjectId = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjGenericInitiatorAffinityInfo);
    Desc.Size     = sizeof (CM_ARCH_COMMON_GENERIC_INITIATOR_AFFINITY_INFO) * GenericInitiatorAffinityInfoCount;
    Desc.Count    = GenericInitiatorAffinityInfoCount;
    Desc.Data     = GenericInitiatorAffinityInfo;
    Status        = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, NULL);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }
  }

CleanupAndReturn:
  return Status;
}

REGISTER_PARSER_FUNCTION (SratParser, "skip-srat-table")
