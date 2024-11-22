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
  EFI_STATUS                           Status;
  CM_STD_OBJ_ACPI_TABLE_INFO           AcpiTableHeader;
  CM_ARCH_COMMON_MEMORY_AFFINITY_INFO  *MemoryAffinityInfo;
  UINTN                                MemoryAffinityInfoCount;
  UINTN                                MemoryAffinityInfoIndex;
  VOID                                 *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO         *PlatformResourceInfo;
  CM_OBJ_DESCRIPTOR                    Desc;
  UINT32                               Index;
  UINT32                               MaxProximityDomain;
  UINT32                               NumberOfInitiatorDomains;
  UINT32                               NumberOfTargetDomains;
  NUMA_INFO_DOMAIN_INFO                DomainInfo;

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

CleanupAndReturn:
  return Status;
}

REGISTER_PARSER_FUNCTION (SratParser, "skip-srat-table")
