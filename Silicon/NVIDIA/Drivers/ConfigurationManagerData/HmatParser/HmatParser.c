/** @file
  Heterogeneous Memory Attribute Table (HMAT) Parser

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "HmatParser.h"
#include "../ConfigurationManagerDataRepoLib.h"
#include "Base.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NumaInfoLib.h>
#include <Library/PcdLib.h>

#include <TH500/TH500Definitions.h>

STATIC UINT16  InfoDataType[] = {
  READ_LATENCY_DATATYPE,
  WRITE_LATENCY_DATATYPE,
  ACCESS_BANDWIDTH_DATATYPE
};

UINT64
EFIAPI
GetSizeOfLatencyAndBandwidthInfoStruct (
  UINT32  NumInitProxDmns,
  UINT32  NumTarProxDmns
  )
{
  UINT64  Size;

  Size = sizeof (EFI_ACPI_6_5_HMAT_STRUCTURE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO) +
         sizeof (UINT32) * NumInitProxDmns +
         sizeof (UINT32) * NumTarProxDmns +
         sizeof (UINT16) * NumInitProxDmns * NumTarProxDmns;

  return Size;
}

EFI_STATUS
EFIAPI
HmatParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_ACPI_6_5_HMAT_STRUCTURE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO  *LatBwInfoStruct;
  EFI_ACPI_6_5_HMAT_STRUCTURE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO  *NextLatBwInfoStruct;
  EFI_ACPI_6_5_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE_HEADER                *HmatTable;
  UINT32                                                                  HmatTableSize;
  UINTN                                                                   NumLatBwInfoStruct;
  UINT32                                                                  MaxProximityDomain;
  UINT32                                                                  NumInitProxDmns;
  UINT32                                                                  NumTarProxDmns;
  UINT32                                                                  InfoStructIdx;
  UINT32                                                                  *ProximityDomainValue;
  UINT32                                                                  ReadIndex;
  UINT16                                                                  ReadLatency;
  UINT32                                                                  WriteIndex;
  UINT16                                                                  WriteLatency;
  UINT32                                                                  AccessBandwidthIndex;
  UINT16                                                                  AccessBandwidth;
  UINT16                                                                  ValueOffset;
  UINT32                                                                  *InitiatorProximityDomainList;
  UINT32                                                                  *TargetProximityDomainList;
  UINT32                                                                  Index;
  UINT32                                                                  IndexInit;
  UINT32                                                                  IndexTarget;
  CM_STD_OBJ_ACPI_TABLE_INFO                                              AcpiTableHeader;
  EFI_STATUS                                                              Status = EFI_SUCCESS;
  NUMA_INFO_DOMAIN_INFO                                                   DomainInfo;
  UINT16                                                                  *LatencyBandwidthValueArray[ARRAY_SIZE (InfoDataType)];

  ReadIndex                    = MAX_UINT32;
  WriteIndex                   = MAX_UINT32;
  AccessBandwidthIndex         = MAX_UINT32;
  TargetProximityDomainList    = NULL;
  InitiatorProximityDomainList = NULL;

  // Number of Latency Bandwidth Information Structures
  // Read Latency, Write Latency and Access Bandwidth structures
  NumLatBwInfoStruct = ARRAY_SIZE (InfoDataType);

  // Proximity Domains
  Status = NumaInfoGetDomainLimits (&MaxProximityDomain, &NumInitProxDmns, &NumTarProxDmns);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: NumaInfoGetDomainLimits failed: %r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  // Generate and populate Initiator proximity domain list
  InitiatorProximityDomainList = (UINT32 *)AllocateZeroPool (sizeof (UINT32) * NumInitProxDmns);
  if (InitiatorProximityDomainList == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate Initiator Proximity DomainList entry\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  // Generate and populate Target proximity domain list
  TargetProximityDomainList = (UINT32 *)AllocateZeroPool (sizeof (UINT32) * NumTarProxDmns);
  if (TargetProximityDomainList == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate Target Proximity DomainList entry\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  IndexInit   = 0;
  IndexTarget = 0;
  for (Index = 0; Index <= MaxProximityDomain; Index++) {
    Status = NumaInfoGetDomainDetails (Index, &DomainInfo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (DomainInfo.InitiatorDomain) {
      InitiatorProximityDomainList[IndexInit] = Index;
      IndexInit++;
    }

    if (DomainInfo.TargetDomain) {
      TargetProximityDomainList[IndexTarget] = Index;
      IndexTarget++;
    }
  }

  ASSERT (IndexInit == NumInitProxDmns);
  ASSERT (IndexTarget == NumTarProxDmns);

  // Calculate the size of the Table to be allocated
  HmatTableSize = sizeof (EFI_ACPI_6_5_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE_HEADER) +
                  (NumLatBwInfoStruct * GetSizeOfLatencyAndBandwidthInfoStruct (NumInitProxDmns, NumTarProxDmns));

  // Allocate table
  HmatTable = AllocateZeroPool (HmatTableSize);
  if (HmatTable == NULL) {
    ASSERT (FALSE);
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  // Populate Header
  HmatTable->Header.Signature = EFI_ACPI_6_5_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE_SIGNATURE;
  HmatTable->Header.Length    = HmatTableSize;
  HmatTable->Header.Revision  = EFI_ACPI_6_5_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE_REVISION;
  CopyMem (HmatTable->Header.OemId, PcdGetPtr (PcdAcpiDefaultOemId), sizeof (HmatTable->Header.OemId));
  HmatTable->Header.OemTableId      = PcdGet64 (PcdAcpiDefaultOemTableId);
  HmatTable->Header.OemRevision     = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  HmatTable->Header.CreatorId       = FixedPcdGet64 (PcdAcpiDefaultCreatorId);
  HmatTable->Header.CreatorRevision = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  HmatTable->Reserved[0]            = EFI_ACPI_RESERVED_BYTE;
  HmatTable->Reserved[1]            = EFI_ACPI_RESERVED_BYTE;
  HmatTable->Reserved[2]            = EFI_ACPI_RESERVED_BYTE;
  HmatTable->Reserved[3]            = EFI_ACPI_RESERVED_BYTE;

  // Starting location of HMAT structures
  LatBwInfoStruct = (EFI_ACPI_6_5_HMAT_STRUCTURE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO *)(HmatTable + 1);

  // Populate Latency Bandwidth Info structures
  for (InfoStructIdx = 0; InfoStructIdx < NumLatBwInfoStruct; InfoStructIdx++ ) {
    LatBwInfoStruct->Type                  = (UINT16)(EFI_ACPI_6_5_HMAT_TYPE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO);
    LatBwInfoStruct->Reserved[0]           = EFI_ACPI_RESERVED_BYTE;
    LatBwInfoStruct->Reserved[1]           = EFI_ACPI_RESERVED_BYTE;
    LatBwInfoStruct->Length                = (UINT32)(GetSizeOfLatencyAndBandwidthInfoStruct (NumInitProxDmns, NumTarProxDmns));
    LatBwInfoStruct->Flags.MemoryHierarchy = 0x0;
    LatBwInfoStruct->DataType              = InfoDataType[InfoStructIdx];

    LatBwInfoStruct->MinTransferSize                   = 1;
    LatBwInfoStruct->Reserved1                         = EFI_ACPI_RESERVED_BYTE;
    LatBwInfoStruct->NumberOfInitiatorProximityDomains = NumInitProxDmns;
    LatBwInfoStruct->NumberOfTargetProximityDomains    = NumTarProxDmns;
    LatBwInfoStruct->EntryBaseUnit                     = ENTRY_BASE_UNIT_NANO_SEC_TO_PICO_SEC;

    // assigning proximity domain values at an offset
    ProximityDomainValue = (UINT32 *)((UINT8 *)LatBwInfoStruct +
                                      sizeof (EFI_ACPI_6_5_HMAT_STRUCTURE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO));
    CopyMem (ProximityDomainValue, InitiatorProximityDomainList, sizeof (UINT32) * NumInitProxDmns);
    ProximityDomainValue += NumInitProxDmns;
    CopyMem (ProximityDomainValue, TargetProximityDomainList, sizeof (UINT32) * NumTarProxDmns);

    // assigning latency or bandwidth values at an offset
    LatencyBandwidthValueArray[InfoStructIdx] = (UINT16 *)((UINT8 *)LatBwInfoStruct +
                                                           sizeof (EFI_ACPI_6_5_HMAT_STRUCTURE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO) +
                                                           sizeof (UINT32) * NumInitProxDmns +
                                                           sizeof (UINT32) * NumTarProxDmns);

    switch (InfoDataType[InfoStructIdx]) {
      case READ_LATENCY_DATATYPE:
        ReadIndex = InfoStructIdx;
        break;
      case WRITE_LATENCY_DATATYPE:
        WriteIndex = InfoStructIdx;
        break;
      case ACCESS_BANDWIDTH_DATATYPE:
        AccessBandwidthIndex = InfoStructIdx;
        break;
      default:
        break;
    }

    // Next HMAT structure
    NextLatBwInfoStruct = (EFI_ACPI_6_5_HMAT_STRUCTURE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO *)((UINT8 *)LatBwInfoStruct
                                                                                                     + LatBwInfoStruct->Length);
    LatBwInfoStruct = NextLatBwInfoStruct;
  }

  for (IndexInit = 0; IndexInit < NumInitProxDmns; IndexInit++) {
    for (IndexTarget = 0; IndexTarget < NumTarProxDmns; IndexTarget++) {
      ValueOffset = IndexInit * NumTarProxDmns + IndexTarget;
      Status      = NumaInfoGetDistances (
                      InitiatorProximityDomainList[IndexInit],
                      TargetProximityDomainList[IndexTarget],
                      NULL,
                      &ReadLatency,
                      &WriteLatency,
                      &AccessBandwidth
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: NumaInfoGetDistances failed: %r\n", __FUNCTION__, Status));
        ReadLatency     = HMAT_INVALID_VALUE_ENTRY;
        WriteLatency    = HMAT_INVALID_VALUE_ENTRY;
        AccessBandwidth = HMAT_INVALID_VALUE_ENTRY;
      }

      if (ReadIndex != MAX_UINT32) {
        LatencyBandwidthValueArray[ReadIndex][ValueOffset] = ReadLatency;
      }

      if (WriteIndex != MAX_UINT32) {
        LatencyBandwidthValueArray[WriteIndex][ValueOffset] = WriteLatency;
      }

      if (AccessBandwidthIndex != MAX_UINT32) {
        LatencyBandwidthValueArray[AccessBandwidthIndex][ValueOffset] = AccessBandwidth;
      }
    }
  }

  // Install HMAT Table

  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_5_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_6_5_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE_REVISION;
  AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdRaw);
  AcpiTableHeader.AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)HmatTable;
  AcpiTableHeader.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  AcpiTableHeader.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader.MinorRevision      = 0;

  Status = NvAddAcpiTableGenerator (ParserHandle, &AcpiTableHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add the HMAT SSDT table\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (HmatTable != NULL) {
      FreePool (HmatTable);
      HmatTable = NULL;
    }
  }

  if (InitiatorProximityDomainList != NULL) {
    FreePool (InitiatorProximityDomainList);
    InitiatorProximityDomainList = NULL;
  }

  if (TargetProximityDomainList != NULL) {
    FreePool (TargetProximityDomainList);
    TargetProximityDomainList = NULL;
  }

  return Status;
}

REGISTER_PARSER_FUNCTION (HmatParser, "skip-hmat-table")
