/** @file
  Heterogeneous Memory Attribute Table (HMAT) Parser

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "HmatParser.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/FloorSweepingLib.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

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

VOID
EFIAPI
ObtainLatencyBandwidthInfo (
  UINT16  *ReadLatencyList,
  UINT16  *WriteLatencyList,
  UINT16  *AccessBandwidthList,
  UINT32  NumInitProxDmns,
  UINT32  NumTarProxDmns
  )
{
  // Populate with max latency or least bandwidth
  for (UINTN InitIndex = 0; InitIndex < NumInitProxDmns; InitIndex++) {
    for (UINTN TargIndex = 0; TargIndex < NumTarProxDmns; TargIndex++) {
      ReadLatencyList[InitIndex * NumInitProxDmns + TargIndex]     = NORMALIZED_UNREACHABLE_LATENCY;
      WriteLatencyList[InitIndex * NumInitProxDmns + TargIndex]    = NORMALIZED_UNREACHABLE_LATENCY;
      AccessBandwidthList[InitIndex * NumInitProxDmns + TargIndex] = NORMALIZED_UNREACHABLE_BANDWIDTH;
    }
  }

  // cpu to local and remote cpus
  for (UINTN InitIndex = 0; InitIndex < PcdGet32 (PcdTegraMaxSockets); InitIndex++) {
    // check if socket enabled for this Index
    if (!IsSocketEnabled (InitIndex)) {
      continue;
    }

    for (UINTN TargIndex = 0; TargIndex <  PcdGet32 (PcdTegraMaxSockets); TargIndex++) {
      if (!IsSocketEnabled (TargIndex)) {
        continue;
      }

      if (InitIndex == TargIndex) {
        // cpu to local cpu
        ReadLatencyList[InitIndex * NumInitProxDmns + TargIndex]     = PcdGet32 (PcdCpuToLocalCpuReadLatency);
        WriteLatencyList[InitIndex * NumInitProxDmns + TargIndex]    = PcdGet32 (PcdCpuToLocalCpuWriteLatency);
        AccessBandwidthList[InitIndex * NumInitProxDmns + TargIndex] = PcdGet32 (PcdCpuToLocalCpuAccessBandwidth);
      } else {
        // cpu to remote cpu
        ReadLatencyList[InitIndex * NumInitProxDmns + TargIndex]     = PcdGet32 (PcdCpuToRemoteCpuReadLatency);
        WriteLatencyList[InitIndex * NumInitProxDmns + TargIndex]    = PcdGet32 (PcdCpuToRemoteCpuWriteLatency);
        AccessBandwidthList[InitIndex * NumInitProxDmns + TargIndex] = PcdGet32 (PcdCpuToRemoteCpuAccessBandwidth);
      }
    }
  }

  // cpu to local gpu hbm and remote gpu hbm
  for (UINTN InitIndex = 0; InitIndex < PcdGet32 (PcdTegraMaxSockets); InitIndex++) {
    // check if socket enabled for this Index
    if (!IsSocketEnabled (InitIndex)) {
      continue;
    }

    for (UINTN TargIndex = TH500_GPU_HBM_PXM_DOMAIN_START; TargIndex < NumTarProxDmns; TargIndex++) {
      if (!IsGpuEnabledOnSocket ((TargIndex - TH500_GPU_HBM_PXM_DOMAIN_START)/TH500_GPU_MAX_NR_MEM_PARTITIONS)) {
        continue;
      }

      if ((TargIndex >= TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID (InitIndex)) &&
          (TargIndex < (TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID (InitIndex) + TH500_GPU_MAX_NR_MEM_PARTITIONS)))
      {
        // local hbm
        ReadLatencyList[InitIndex * NumInitProxDmns + TargIndex]     = PcdGet32 (PcdCpuToLocalHbmReadLatency);
        WriteLatencyList[InitIndex * NumInitProxDmns + TargIndex]    = PcdGet32 (PcdCpuToLocalHbmWriteLatency);
        AccessBandwidthList[InitIndex * NumInitProxDmns + TargIndex] = PcdGet32 (PcdCpuToLocalHbmAccessBandwidth);
      } else {
        // remote hbm
        ReadLatencyList[InitIndex * NumInitProxDmns + TargIndex]     = PcdGet32 (PcdCpuToRemoteHbmReadLatency);
        WriteLatencyList[InitIndex * NumInitProxDmns + TargIndex]    = PcdGet32 (PcdCpuToRemoteHbmWriteLatency);
        AccessBandwidthList[InitIndex * NumInitProxDmns + TargIndex] = PcdGet32 (PcdCpuToRemoteHbmAccessBandwidth);
      }
    }
  }

  // gpu to local hbm and remote hbm
  for (UINTN InitIndex = TH500_GPU_PXM_DOMAIN_START; InitIndex < TH500_GPU_PXM_DOMAIN_START + PcdGet32 (PcdTegraMaxSockets); InitIndex++) {
    // check if CPU socket enabled for this GPU Index
    if (!IsGpuEnabledOnSocket (InitIndex - TH500_GPU_PXM_DOMAIN_START)) {
      continue;
    }

    // for all proximity domains
    for (UINTN TargIndex = TH500_GPU_HBM_PXM_DOMAIN_START;
         TargIndex < NumTarProxDmns;
         TargIndex++)
    {
      if (!IsGpuEnabledOnSocket ((TargIndex - TH500_GPU_HBM_PXM_DOMAIN_START)/TH500_GPU_MAX_NR_MEM_PARTITIONS)) {
        continue;
      }

      if ((TargIndex >= TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID (InitIndex-TH500_GPU_PXM_DOMAIN_START)) &&
          (TargIndex < TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID (InitIndex-TH500_GPU_PXM_DOMAIN_START) + TH500_GPU_MAX_NR_MEM_PARTITIONS))
      {
        // local hbm
        ReadLatencyList[InitIndex * NumInitProxDmns + TargIndex]     = PcdGet32 (PcdGpuToLocalHbmReadLatency);
        WriteLatencyList[InitIndex * NumInitProxDmns + TargIndex]    = PcdGet32 (PcdGpuToLocalHbmWriteLatency);
        AccessBandwidthList[InitIndex * NumInitProxDmns + TargIndex] = PcdGet32 (PcdGpuToLocalHbmAccessBandwidth);
      } else {
        // remote hbm
        ReadLatencyList[InitIndex * NumInitProxDmns + TargIndex]     = PcdGet32 (PcdGpuToRemoteHbmReadLatency);
        WriteLatencyList[InitIndex * NumInitProxDmns + TargIndex]    = PcdGet32 (PcdGpuToRemoteHbmWriteLatency);
        AccessBandwidthList[InitIndex * NumInitProxDmns + TargIndex] = PcdGet32 (PcdGpuToRemoteHbmAccessBandwidth);
      }
    }
  }

  // gpu to local cpu and remote cpu
  for (UINTN InitIndex = TH500_GPU_PXM_DOMAIN_START; InitIndex < TH500_GPU_PXM_DOMAIN_START + PcdGet32 (PcdTegraMaxSockets); InitIndex++) {
    // check if CPU socket enabled for this GPU Index
    if (!IsGpuEnabledOnSocket (InitIndex - TH500_GPU_PXM_DOMAIN_START)) {
      continue;
    }

    for (UINTN TargIndex = 0; TargIndex <  PcdGet32 (PcdTegraMaxSockets); TargIndex++) {
      if (!IsSocketEnabled (TargIndex)) {
        continue;
      }

      if ((InitIndex - TH500_GPU_PXM_DOMAIN_START) == TargIndex) {
        // local cpu
        ReadLatencyList[InitIndex * NumInitProxDmns + TargIndex]     = PcdGet32 (PcdGpuToLocalCpuReadLatency);
        WriteLatencyList[InitIndex * NumInitProxDmns + TargIndex]    = PcdGet32 (PcdGpuToLocalCpuWriteLatency);
        AccessBandwidthList[InitIndex * NumInitProxDmns + TargIndex] = PcdGet32 (PcdGpuToLocalCpuAccessBandwidth);
      } else {
        // remote cpu
        ReadLatencyList[InitIndex * NumInitProxDmns + TargIndex]     = PcdGet32 (PcdGpuToRemoteCpuReadLatency);
        WriteLatencyList[InitIndex * NumInitProxDmns + TargIndex]    = PcdGet32 (PcdGpuToRemoteCpuWriteLatency);
        AccessBandwidthList[InitIndex * NumInitProxDmns + TargIndex] = PcdGet32 (PcdGpuToRemoteCpuAccessBandwidth);
      }
    }
  }
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
  UINT32                                                                  NumInitProxDmns;
  UINT32                                                                  NumTarProxDmns;
  UINT32                                                                  InfoStructIdx;
  UINT32                                                                  *ProximityDomainValue;
  UINT16                                                                  *LatencyBandwidthValue;
  UINT16                                                                  *ReadLatencyList;
  UINT16                                                                  *WriteLatencyList;
  UINT16                                                                  *LatencyBandwidthEntries;
  UINT16                                                                  *AccessBandwidthList;
  UINT32                                                                  *InitiatorProximityDomainList;
  UINT32                                                                  *TargetProximityDomainList;
  UINT32                                                                  Index;
  // CM_STD_OBJ_ACPI_TABLE_INFO                                              *NewAcpiTables;
  CM_STD_OBJ_ACPI_TABLE_INFO  AcpiTableHeader;
  EFI_STATUS                  Status = EFI_SUCCESS;
  CM_OBJ_DESCRIPTOR           Desc;

  TargetProximityDomainList    = NULL;
  InitiatorProximityDomainList = NULL;
  ReadLatencyList              = NULL;
  WriteLatencyList             = NULL;
  AccessBandwidthList          = NULL;

  // Number of Latency Bandwidth Information Structures
  // Read Latency, Write Latency and Access Bandwidth structures
  NumLatBwInfoStruct = 3;
  InfoStructIdx      = 0;

  // Proximity Domains
  NumInitProxDmns = GetMaxPxmDomains ();
  NumTarProxDmns  = GetMaxPxmDomains ();

  // Generate and populate Initiator proximity domain list
  InitiatorProximityDomainList = (UINT32 *)AllocateZeroPool (sizeof (UINT32) * NumInitProxDmns);
  if (InitiatorProximityDomainList == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate Initiator Proximity DomainList entry\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  for (Index = 0; Index < NumInitProxDmns; Index++) {
    InitiatorProximityDomainList[Index] = Index;
  }

  // Generate and populate Target proximity domain list
  TargetProximityDomainList = (UINT32 *)AllocateZeroPool (sizeof (UINT32) * NumTarProxDmns);
  if (TargetProximityDomainList == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate Target Proximity DomainList entry\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  for (Index = 0; Index < NumTarProxDmns; Index++) {
    TargetProximityDomainList[Index] = Index;
  }

  // Collect Read/Write Latency and bandwidth info
  ReadLatencyList = (UINT16 *)AllocateZeroPool (sizeof (UINT16) * NumInitProxDmns * NumTarProxDmns);
  if (ReadLatencyList == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate Read Latency list entry\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  WriteLatencyList = (UINT16 *)AllocateZeroPool (sizeof (UINT16) * NumInitProxDmns * NumTarProxDmns);
  if (WriteLatencyList == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate Write Latency list entry\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  AccessBandwidthList = (UINT16 *)AllocateZeroPool (sizeof (UINT16) * NumInitProxDmns * NumTarProxDmns);
  if (AccessBandwidthList == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate Access Bandwidth list entry\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  ObtainLatencyBandwidthInfo (
    ReadLatencyList,
    WriteLatencyList,
    AccessBandwidthList,
    NumInitProxDmns,
    NumTarProxDmns
    );

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
  for ( ; InfoStructIdx < NumLatBwInfoStruct; InfoStructIdx++ ) {
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
    for (UINT32 DomainIndex = 0; DomainIndex < NumInitProxDmns; DomainIndex++) {
      *ProximityDomainValue = InitiatorProximityDomainList[DomainIndex];
      ProximityDomainValue++;
    }

    for (UINT32 DomainIndex = 0; DomainIndex < NumTarProxDmns; DomainIndex++) {
      *ProximityDomainValue = TargetProximityDomainList[DomainIndex];
      ProximityDomainValue++;
    }

    // assigning latency or bandwidth values at an offset
    LatencyBandwidthValue = (UINT16 *)((UINT8 *)LatBwInfoStruct +
                                       sizeof (EFI_ACPI_6_5_HMAT_STRUCTURE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO) +
                                       sizeof (UINT32) * NumInitProxDmns +
                                       sizeof (UINT32) * NumTarProxDmns);
    switch (LatBwInfoStruct->DataType) {
      case READ_LATENCY_DATATYPE:
        LatencyBandwidthEntries = ReadLatencyList;
        break;
      case WRITE_LATENCY_DATATYPE:
        LatencyBandwidthEntries = WriteLatencyList;
        break;
      case ACCESS_BANDWIDTH_DATATYPE:
        LatencyBandwidthEntries = AccessBandwidthList;
        break;
    }

    for (UINT32 ValIndex = 0; ValIndex < NumInitProxDmns * NumTarProxDmns; ValIndex++) {
      *LatencyBandwidthValue = LatencyBandwidthEntries[ValIndex];
      LatencyBandwidthValue++;
    }

    // Next HMAT structure
    NextLatBwInfoStruct = (EFI_ACPI_6_5_HMAT_STRUCTURE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO *)((UINT8 *)LatBwInfoStruct
                                                                                                     + LatBwInfoStruct->Length);
    LatBwInfoStruct = NextLatBwInfoStruct;
  }

  // Install HMAT Table

  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_5_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_6_5_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE_REVISION;
  AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdRaw);
  AcpiTableHeader.AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)HmatTable;
  AcpiTableHeader.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  AcpiTableHeader.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader.MinorRevision      = 0;

  Desc.ObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList);
  Desc.Size     = sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
  Desc.Count    = 1;
  Desc.Data     = &AcpiTableHeader;

  Status = NvExtendCmObj (ParserHandle, &Desc, CM_NULL_TOKEN, NULL);
  if (EFI_ERROR (Status)) {
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

  if (ReadLatencyList != NULL) {
    FreePool (ReadLatencyList);
    ReadLatencyList = NULL;
  }

  if (WriteLatencyList != NULL) {
    FreePool (WriteLatencyList);
    WriteLatencyList = NULL;
  }

  if (AccessBandwidthList != NULL) {
    FreePool (AccessBandwidthList);
    AccessBandwidthList = NULL;
  }

  return Status;
}
