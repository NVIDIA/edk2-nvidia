/** @file
  Configuration Manager Data of Static Resource Affinity Table

  SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
  UINT32                                          Index;
  UINT32                                          MaxProximityDomain;
  UINT32                                          NumberOfInitiatorDomains;
  UINT32                                          NumberOfTargetDomains;
  NUMA_INFO_DOMAIN_INFO                           DomainInfo;

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

  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_4_SYSTEM_RESOURCE_AFFINITY_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_6_4_SYSTEM_RESOURCE_AFFINITY_TABLE_REVISION;
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

  MemoryAffinityInfo = (CM_ARCH_COMMON_MEMORY_AFFINITY_INFO *)AllocateZeroPool (sizeof (CM_ARCH_COMMON_MEMORY_AFFINITY_INFO) * MemoryAffinityInfoCount);
  if (MemoryAffinityInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory affinity info\r\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  for (MemoryAffinityInfoIndex = 0; MemoryAffinityInfoIndex < PlatformResourceInfo->ResourceInfo->DramRegionsCount; MemoryAffinityInfoIndex++) {
    MemoryAffinityInfo[MemoryAffinityInfoIndex].ProximityDomain = TH500_AMAP_GET_SOCKET (PlatformResourceInfo->ResourceInfo->DramRegions[MemoryAffinityInfoIndex].MemoryBaseAddress);
    MemoryAffinityInfo[MemoryAffinityInfoIndex].BaseAddress     = PlatformResourceInfo->ResourceInfo->DramRegions[MemoryAffinityInfoIndex].MemoryBaseAddress;
    MemoryAffinityInfo[MemoryAffinityInfoIndex].Length          = PlatformResourceInfo->ResourceInfo->DramRegions[MemoryAffinityInfoIndex].MemoryLength;
    MemoryAffinityInfo[MemoryAffinityInfoIndex].Flags           = EFI_ACPI_6_4_MEMORY_ENABLED;
  }

  for (Index = 0; Index <= MaxProximityDomain; Index++) {
    Status = NumaInfoGetDomainDetails (Index, &DomainInfo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (DomainInfo.TargetDomain) {
      if (DomainInfo.DeviceType == NUMA_INFO_TYPE_CPU) {
        // CPU initiators do not generate memory entries, the dram regions already cover them
        MemoryAffinityInfoCount--;
        continue;
      }

      MemoryAffinityInfo[MemoryAffinityInfoIndex].ProximityDomain = Index;
      MemoryAffinityInfo[MemoryAffinityInfoIndex].Flags           = EFI_ACPI_6_4_MEMORY_ENABLED|EFI_ACPI_6_4_MEMORY_HOT_PLUGGABLE;
      MemoryAffinityInfoIndex++;
    }
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
  for (Index = 0; Index <= MaxProximityDomain; Index++) {
    Status = NumaInfoGetDomainDetails (Index, &DomainInfo);
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
      if (DomainInfo.DeviceHandleType != EFI_ACPI_6_4_PCI_DEVICE_HANDLE) {
        GenericInitiatorAffinityInfoCount--;
        continue;
      }

      GenericInitiatorAffinityInfo[GenericInitiatorAffinityInfoIndex].ProximityDomain  = Index;
      GenericInitiatorAffinityInfo[GenericInitiatorAffinityInfoIndex].Flags            = EFI_ACPI_6_4_GENERIC_INITIATOR_AFFINITY_STRUCTURE_ENABLED|EFI_ACPI_6_4_GENERIC_INITIATOR_AFFINITY_STRUCTURE_ARCHITECTURAL_TRANSACTIONS;
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

    for (Index = 0; Index < GenericInitiatorAffinityInfoCount; Index++) {
      GenericInitiatorAffinityInfo[Index].DeviceHandleToken = DeviceHandleTokenMap[Index];
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
