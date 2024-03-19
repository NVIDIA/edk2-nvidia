/** @file
  Arm Performance Monitoring Unit Table (APMT) parser.

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ApmtParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <libfdt.h>

#include <IndustryStandard/ArmPerformanceMonitoringUnitTable.h>

EFI_STATUS
EFIAPI
ApmtParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                                             Status;
  UINT32                                                 *DeviceTreeHandles;
  UINT32                                                 NumberOfHandles;
  UINT32                                                 Index;
  UINT32                                                 MaxNumberOfApmtEntries;
  UINT32                                                 NumberOfDeviceHandles;
  VOID                                                   *DeviceTreeBase;
  INT32                                                  NodeOffset;
  UINT32                                                 DeviceIndex;
  UINT32                                                 DeviceHandle;
  INT32                                                  DeviceOffset;
  INT32                                                  PropertyLength;
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

  // Figure out how big to make the table, may return more then needed.
  NumberOfHandles = 0;
  Status          = GetMatchingEnabledDeviceTreeNodes (TH500_APMU_COMPAT, NULL, &NumberOfHandles);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return EFI_SUCCESS;
  }

  DeviceTreeHandles = AllocatePool (sizeof (UINT32) * NumberOfHandles);
  if (DeviceTreeHandles == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate handles\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetMatchingEnabledDeviceTreeNodes (TH500_APMU_COMPAT, DeviceTreeHandles, &NumberOfHandles);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get APMU handles\r\n", __FUNCTION__));
    return Status;
  }

  MaxNumberOfApmtEntries = 0;
  for (Index = 0; Index < NumberOfHandles; Index++) {
    Status = GetDeviceTreeNode (DeviceTreeHandles[Index], &DeviceTreeBase, &NodeOffset);
    if (EFI_ERROR (Status)) {
      ASSERT_EFI_ERROR (Status);
      return Status;
    }

    Property                = fdt_getprop (DeviceTreeBase, NodeOffset, "devices", &PropertyLength);
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
  for (Index = 0; Index < NumberOfHandles; Index++) {
    Status = GetDeviceTreeNode (DeviceTreeHandles[Index], &DeviceTreeBase, &NodeOffset);
    if (EFI_ERROR (Status)) {
      ASSERT_EFI_ERROR (Status);
      return Status;
    }

    ParentOffset = fdt_parent_offset (DeviceTreeBase, NodeOffset);
    if (ParentOffset < 0) {
      DEBUG ((DEBUG_ERROR, "%a: No reg in parent of apmu node\r\n", __FUNCTION__));
      continue;
    }

    Property = fdt_getprop (
                 DeviceTreeBase,
                 ParentOffset,
                 "reg",
                 NULL
                 );
    if (Property == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: No reg in parent of apmu node\r\n", __FUNCTION__));
      continue;
    }

    Socket = SwapBytes32 (*(CONST UINT32 *)Property);

    Property              = fdt_getprop (DeviceTreeBase, NodeOffset, "devices", &PropertyLength);
    NumberOfDeviceHandles = PropertyLength/sizeof (UINT32);
    DeviceHandleArray     = (CONST UINT32 *)Property;
    for (DeviceIndex = 0; DeviceIndex < NumberOfDeviceHandles; DeviceIndex++) {
      DeviceHandle = SwapBytes32 (DeviceHandleArray[DeviceIndex]);
      DeviceOffset = fdt_node_offset_by_phandle (DeviceTreeBase, DeviceHandle);
      if (DeviceOffset < 0) {
        continue;
      }

      NumberOfRegisters = 1;
      Status            = GetDeviceTreeRegisters (DeviceTreeHandles[Index], &Register, &NumberOfRegisters);
      if (EFI_ERROR (Status)) {
        continue;
      }

      NumberOfInterrupts = 1;
      Status             = GetDeviceTreeInterrupts (DeviceTreeHandles[Index], &Interrupt, &NumberOfInterrupts);
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
      ApmtNodes[ApmtNodeIndex].ProcessorAffinity      = Socket;
      ApmtNodes[ApmtNodeIndex].ImplementationId       = 0;

      Property = fdt_getprop (DeviceTreeBase, NodeOffset, "implementation_id", NULL);
      if (Property != NULL) {
        ApmtNodes[ApmtNodeIndex].ImplementationId = SwapBytes32 (*(CONST UINT32 *)Property);
      }

      Property = fdt_getprop (DeviceTreeBase, DeviceOffset, "device_type", NULL);
      if (Property == NULL) {
        // Correct DTB has "compatible" = "cache" for cache nodes
        Property = fdt_getprop (DeviceTreeBase, DeviceOffset, "compatible", NULL);
        if ((Property != NULL) && (AsciiStrCmp (Property, "cache") == 0)) {
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
        } else {
          continue;
        }
      }

      if (AsciiStrCmp (Property, "pci") == 0) {
        ApmtNodes[ApmtNodeIndex].NodeType = EFI_ACPI_APMT_NODE_TYPE_PCIE_ROOT_COMPLEX;

        Property = fdt_getprop (DeviceTreeBase, DeviceOffset, "linux,pci-domain", NULL);
        if (Property == NULL) {
          continue;
        }

        ApmtNodes[ApmtNodeIndex].NodeInstancePrimary   = SwapBytes32 (*(CONST UINT32 *)Property);
        ApmtNodes[ApmtNodeIndex].NodeInstanceSecondary = 0;
      } else if (AsciiStrCmp (Property, "cache") == 0) {
        // Old DTB has "device_type" = "cache" for cache nodes
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
      } else if (AsciiStrCmp (Property, "acpi") == 0) {
        ApmtNodes[ApmtNodeIndex].NodeType = EFI_ACPI_APMT_NODE_TYPE_ACPI_DEVICE;
        Property                          = fdt_getprop (DeviceTreeBase, DeviceOffset, "nvidia,hid", NULL);
        if (Property == NULL) {
          continue;
        }

        CopyMem (&ApmtNodes[ApmtNodeIndex].NodeInstancePrimary, Property, sizeof (UINT64));

        Property = fdt_getprop (DeviceTreeBase, DeviceOffset, "nvidia,uid", NULL);
        if (Property == NULL) {
          continue;
        }

        ApmtNodes[ApmtNodeIndex].NodeInstanceSecondary = SwapBytes32 (*(CONST UINT32 *)Property);
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
