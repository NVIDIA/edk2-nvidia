/** @file
  Arm Performance Monitoring Unit Table (APMT) parser.

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ApmtParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include "../ProcHierarchyInfo/ProcHierarchyInfoParser.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>

#include <IndustryStandard/ArmPerformanceMonitoringUnitTable.h>

EFI_STATUS
EFIAPI
ApmtParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                                             Status;
  UINT32                                                 MaxNumberOfApmtEntries;
  UINT32                                                 NumberOfDeviceHandles;
  INT32                                                  NodeOffset;
  UINT32                                                 DeviceIndex;
  UINT32                                                 DeviceHandle;
  INT32                                                  DeviceOffset;
  UINT32                                                 PropertyLength;
  CONST VOID                                             *Property;
  CONST UINT32                                           *DeviceHandleArray;
  EFI_ACPI_ARM_PERFORMANCE_MONITORING_UNIT_TABLE_HEADER  *ApmtHeader;
  EFI_ACPI_ARM_PERFORMANCE_MONITORING_UNIT_NODE          *ApmtNodes;
  INT32                                                  ParentOffset;
  UINT32                                                 Socket;
  UINT32                                                 ApmtNodeIndex;
  NVIDIA_DEVICE_TREE_REGISTER_DATA                       Register;
  UINT32                                                 NumberOfRegisters;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA                      Interrupt;
  UINT32                                                 NumberOfInterrupts;
  CM_STD_OBJ_ACPI_TABLE_INFO                             AcpiTableHeader;
  UINT32                                                 pHandle;
  CONST CHAR8                                            *CompatArray[2];
  BOOLEAN                                                IsCacheDevice;

  CompatArray[0] = TH500_APMU_COMPAT;
  CompatArray[1] = NULL;

  MaxNumberOfApmtEntries = 0;
  NodeOffset             = -1;

  // Determine the number of apmu devices in the device tree
  while (!EFI_ERROR (DeviceTreeGetNextCompatibleNode (CompatArray, &NodeOffset))) {
    Status = DeviceTreeGetNodeProperty (NodeOffset, "devices", &Property, &PropertyLength);
    if (EFI_ERROR (Status)) {
      // No devices under this node
      continue;
    }

    MaxNumberOfApmtEntries += PropertyLength/sizeof (UINT32);
  }

  // Allocate table
  ApmtHeader = AllocatePool (
                 sizeof (EFI_ACPI_ARM_PERFORMANCE_MONITORING_UNIT_TABLE_HEADER) +
                 MaxNumberOfApmtEntries * sizeof (EFI_ACPI_ARM_PERFORMANCE_MONITORING_UNIT_NODE)
                 );
  if (ApmtHeader == NULL) {
    ASSERT (FALSE);
    return EFI_OUT_OF_RESOURCES;
  }

  ApmtNodes     = (EFI_ACPI_ARM_PERFORMANCE_MONITORING_UNIT_NODE *)(ApmtHeader + 1);
  ApmtNodeIndex = 0;

  // Populate header
  ApmtHeader->Header.Signature = EFI_ACPI_6_4_ARM_PERFORMANCE_MONITORING_UNIT_TABLE_SIGNATURE;
  ApmtHeader->Header.Revision  = EFI_ACPI_ARM_PERFORMANCE_MONITORING_UNIT_TABLE_REVISION;
  CopyMem (ApmtHeader->Header.OemId, PcdGetPtr (PcdAcpiDefaultOemId), sizeof (ApmtHeader->Header.OemId));
  ApmtHeader->Header.OemTableId      = PcdGet64 (PcdAcpiDefaultOemTableId);
  ApmtHeader->Header.OemRevision     = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  ApmtHeader->Header.CreatorId       = FixedPcdGet64 (PcdAcpiDefaultCreatorId);
  ApmtHeader->Header.CreatorRevision = FixedPcdGet64 (PcdAcpiDefaultOemRevision);

  // Populate entries
  NodeOffset = -1;
  while (!EFI_ERROR (DeviceTreeGetNextCompatibleNode (CompatArray, &NodeOffset))) {
    Status = DeviceTreeGetParentOffset (NodeOffset, &ParentOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: No reg in parent of apmu node\r\n", __FUNCTION__));
      continue;
    }

    // Get socket id
    Status = DeviceTreeGetNodePropertyValue32 (ParentOffset, "reg", &Socket);

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: No reg in parent of apmu node\r\n", __FUNCTION__));
      continue;
    }

    Status = DeviceTreeGetNodeProperty (NodeOffset, "devices", &Property, &PropertyLength);
    if (EFI_ERROR (Status)) {
      continue;
    }

    NumberOfDeviceHandles = PropertyLength/sizeof (UINT32);
    DeviceHandleArray     = (CONST UINT32 *)Property;
    for (DeviceIndex = 0; DeviceIndex < NumberOfDeviceHandles; DeviceIndex++) {
      DeviceHandle = SwapBytes32 (DeviceHandleArray[DeviceIndex]);
      Status       = DeviceTreeGetNodeByPHandle (DeviceHandle, &DeviceOffset);
      if (EFI_ERROR (Status)) {
        continue;
      }

      NumberOfRegisters = 1;
      Status            = DeviceTreeGetRegisters (NodeOffset, &Register, &NumberOfRegisters);
      if (EFI_ERROR (Status)) {
        continue;
      }

      NumberOfInterrupts = 1;
      Status             = DeviceTreeGetInterrupts (NodeOffset, &Interrupt, &NumberOfInterrupts);
      if (EFI_ERROR (Status)) {
        continue;
      }

      ApmtNodes[ApmtNodeIndex].Length                 = sizeof (EFI_ACPI_ARM_PERFORMANCE_MONITORING_UNIT_NODE);
      ApmtNodes[ApmtNodeIndex].NodeFlags              = EFI_ACPI_APMT_PROCESSOR_AFFINITY_TYPE_CONTAINER;
      ApmtNodes[ApmtNodeIndex].Identifier             = ApmtNodeIndex;
      ApmtNodes[ApmtNodeIndex].BaseAddress0           = Register.BaseAddress;
      ApmtNodes[ApmtNodeIndex].BaseAddress1           = 0;
      ApmtNodes[ApmtNodeIndex].OverflowInterrupt      = DEVICETREE_TO_ACPI_INTERRUPT_NUM (Interrupt);
      ApmtNodes[ApmtNodeIndex].Reserved1              = 0;
      ApmtNodes[ApmtNodeIndex].OverflowInterruptFlags = EFI_ACPI_APMT_INTERRUPT_MODE_LEVEL_TRIGGERED;
      // ProcessorAffinity should be the UID of the Socket in the ProcHierarchyInfo
      ApmtNodes[ApmtNodeIndex].ProcessorAffinity = GEN_CONTAINER_UID (1, Socket, 0, 0);

      Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "implementation_id", &ApmtNodes[ApmtNodeIndex].ImplementationId);
      if (EFI_ERROR (Status)) {
        ApmtNodes[ApmtNodeIndex].ImplementationId = 0;
      }

      IsCacheDevice = FALSE;
      Status        = DeviceTreeCheckNodeSingleCompatibility ("cache", DeviceOffset);
      if (!EFI_ERROR (Status)) {
        IsCacheDevice = TRUE;
      } else {
        // Old DTB has "device_type" = "cache" for cache nodes
        Status = DeviceTreeGetNodeProperty (DeviceOffset, "device_type", &Property, NULL);
        if (EFI_ERROR (Status)) {
          continue;
        } else {
          if (AsciiStrCmp (Property, "cache") == 0) {
            IsCacheDevice = TRUE;
          }
        }
      }

      // Either IsCacheDevice is set or Property is the device_type string
      if (IsCacheDevice) {
        ApmtNodes[ApmtNodeIndex].NodeType            = EFI_ACPI_APMT_NODE_TYPE_CPU_CACHE;
        ApmtNodes[ApmtNodeIndex].NodeInstancePrimary = 0;
        Status                                       = DeviceTreeGetNodePHandle (DeviceOffset, &pHandle);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get Cache pHandle for Socket %u Cache\n", __FUNCTION__, Status, Socket));
          goto CleanupAndReturn;
        }

        Status = NvFindCacheIdByPhandle (ParserHandle, pHandle, FALSE, &ApmtNodes[ApmtNodeIndex].NodeInstanceSecondary);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get CacheId for Socket %u Cache\n", __FUNCTION__, Status, Socket));
          goto CleanupAndReturn;
        }
      } else if (AsciiStrCmp (Property, "pci") == 0) {
        ApmtNodes[ApmtNodeIndex].NodeType = EFI_ACPI_APMT_NODE_TYPE_PCIE_ROOT_COMPLEX;
        Status                            = DeviceTreeGetNodePropertyValue64 (DeviceOffset, "linux,pci-domain", &ApmtNodes[ApmtNodeIndex].NodeInstancePrimary);
        if (EFI_ERROR (Status)) {
          continue;
        }

        ApmtNodes[ApmtNodeIndex].NodeInstanceSecondary = 0;
      } else if (AsciiStrCmp (Property, "acpi") == 0) {
        ApmtNodes[ApmtNodeIndex].NodeType = EFI_ACPI_APMT_NODE_TYPE_ACPI_DEVICE;
        Status                            = DeviceTreeGetNodeProperty (DeviceOffset, "nvidia,hid", &Property, NULL);
        if (EFI_ERROR (Status)) {
          continue;
        }

        CopyMem (&ApmtNodes[ApmtNodeIndex].NodeInstancePrimary, Property, sizeof (UINT64));

        Status = DeviceTreeGetNodePropertyValue32 (DeviceOffset, "nvidia,uid", &ApmtNodes[ApmtNodeIndex].NodeInstanceSecondary);
        if (EFI_ERROR (Status)) {
          continue;
        }
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Unknown device type %a\n", __FUNCTION__, (CONST CHAR8 *)Property));
        continue;
      }

      ApmtNodeIndex++;
    }
  }

  ApmtHeader->Header.Length = sizeof (EFI_ACPI_ARM_PERFORMANCE_MONITORING_UNIT_TABLE_HEADER) +
                              ApmtNodeIndex * sizeof (EFI_ACPI_ARM_PERFORMANCE_MONITORING_UNIT_NODE);

  // Install Table
  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_4_ARM_PERFORMANCE_MONITORING_UNIT_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_ARM_PERFORMANCE_MONITORING_UNIT_TABLE_REVISION;
  AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdRaw);
  AcpiTableHeader.AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)ApmtHeader;
  AcpiTableHeader.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  AcpiTableHeader.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader.MinorRevision      = 0;

  Status = NvAddAcpiTableGenerator (ParserHandle, &AcpiTableHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add the APMT SSDT table\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  return Status;
}

REGISTER_PARSER_FUNCTION (ApmtParser, "skip-apmt-table")
