/** @file
  Memory System Resource Partitioning and Monitoring Table Parser

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "MpamParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <libfdt.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MemoryAllocationLib.h>

#include <IndustryStandard/Mpam.h>

EFI_STATUS
EFIAPI
UpdateResourceNodeInfo (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle
  )
{
  UINT32                     Index;
  CM_ARM_RESOURCE_NODE_INFO  *ResourceNodeInfo;
  UINT32                     ResourceNodeCount;
  UINT32                     *ResourceNodeHandles;
  VOID                       *DeviceTreeBase;
  INT32                      NodeOffset;
  UINT32                     pHandle;
  CONST UINT32               *MpamProp;
  EFI_STATUS                 Status;
  CM_OBJ_DESCRIPTOR          Desc;
  UINT32                     CacheId;
  CM_OBJECT_TOKEN            *TokenMap;

  ResourceNodeInfo    = NULL;
  ResourceNodeHandles = NULL;
  TokenMap            = NULL;
  ResourceNodeCount   = 0;

  // Get Resource node count from the device tree
  Status = GetMatchingEnabledDeviceTreeNodes ("arm,mpam-cache", NULL, &ResourceNodeCount);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "No Resource nodes found\r\n"));
    Status = EFI_SUCCESS;
    goto Exit;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  Status = NvAllocateCmTokens (ParserHandle, ResourceNodeCount, &TokenMap);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  ResourceNodeHandles = (UINT32 *)AllocateZeroPool (sizeof (UINT32) * ResourceNodeCount);
  if (ResourceNodeHandles == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = GetMatchingEnabledDeviceTreeNodes ("arm,mpam-cache", ResourceNodeHandles, &ResourceNodeCount);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  ResourceNodeInfo = (CM_ARM_RESOURCE_NODE_INFO *)AllocateZeroPool (sizeof (CM_ARM_RESOURCE_NODE_INFO) * ResourceNodeCount);
  if (ResourceNodeInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate for Resource Nodes\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  for (Index = 0; Index < ResourceNodeCount; Index++) {
    ResourceNodeInfo[Index].Token       = TokenMap[Index];
    ResourceNodeInfo[Index].RisIndex    = 0;
    ResourceNodeInfo[Index].LocatorType = EFI_ACPI_MPAM_LOCATION_PROCESSOR_CACHE;

    // Gather Locator info
    Status = GetDeviceTreeNode (ResourceNodeHandles[Index], &DeviceTreeBase, &NodeOffset);
    if (EFI_ERROR (Status)) {
      goto Exit;
    }

    // using pHandle as unique identifier
    pHandle                            = fdt_get_phandle (DeviceTreeBase, NodeOffset);
    ResourceNodeInfo[Index].Identifier = pHandle;

    MpamProp = fdt_getprop (DeviceTreeBase, NodeOffset, "arm,mpam-device", NULL);
    if (MpamProp == NULL) {
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }

    // Assign the locator to match the Cache ID assigned in the PPTT table
    Status = NvFindCacheIdByPhandle (ParserHandle, SwapBytes32 (*MpamProp), CACHE_TYPE_UNIFIED, &CacheId);
    if (EFI_ERROR (Status)) {
      goto Exit;
    }

    ResourceNodeInfo[Index].Locator.Descriptor1 = CacheId;

    // TODO: Func Dependency List
    ResourceNodeInfo[Index].NumFuncDep = 0;
  }

  // Add Resource Nodes to repo
  Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjResNodeInfo);
  Desc.Size     = sizeof (CM_ARM_RESOURCE_NODE_INFO) * ResourceNodeCount;
  Desc.Count    = ResourceNodeCount;
  Desc.Data     = ResourceNodeInfo;

  Status = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add ResourceNodes for MPAM\n", __FUNCTION__, Status));
    goto Exit;
  }

Exit:
  if (EFI_ERROR (Status)) {
    FREE_NON_NULL (ResourceNodeInfo);
  }

  FREE_NON_NULL (ResourceNodeHandles);
  FREE_NON_NULL (TokenMap);

  return Status;
}

EFI_STATUS
EFIAPI
UpdateMscNodeInfo (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle
  )
{
  UINT32                             Index;
  CM_ARM_MSC_NODE_INFO               *MscNodeInfo;
  UINT32                             MscNodeCount;
  UINTN                              MscNodeEntries;
  UINT32                             *MscNodeHandles;
  NVIDIA_DEVICE_TREE_REGISTER_DATA   *RegisterData;
  UINT32                             RegisterSize;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA  *InterruptData;
  UINT32                             InterruptIndex;
  UINT32                             InterruptSize;
  UINT32                             pHandle;
  VOID                               *DeviceTreeBase;
  INT32                              NodeOffset;
  CONST UINT32                       *MpamProp;
  EFI_STATUS                         Status;
  INT32                              SubNodeOffset;
  INT32                              PrevSubNodeOffset;
  CM_OBJ_DESCRIPTOR                  Desc;
  CM_OBJECT_TOKEN                    *TokenMap;

  MscNodeInfo    = NULL;
  MscNodeHandles = NULL;
  RegisterData   = NULL;
  InterruptData  = NULL;
  TokenMap       = NULL;
  MscNodeCount   = 0;
  MscNodeEntries = 0;

  // Get MSC node count from the device tree
  Status = GetMatchingEnabledDeviceTreeNodes ("arm,mpam-msc", NULL, &MscNodeCount);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "No MSC nodes found\r\n"));
    goto Exit;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  Status = NvAllocateCmTokens (ParserHandle, MscNodeCount, &TokenMap);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  MscNodeHandles = (UINT32 *)AllocateZeroPool (sizeof (UINT32) * MscNodeCount);
  if (MscNodeHandles == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = GetMatchingEnabledDeviceTreeNodes ("arm,mpam-msc", MscNodeHandles, &MscNodeCount);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  MscNodeInfo = (CM_ARM_MSC_NODE_INFO *)AllocateZeroPool (sizeof (CM_ARM_MSC_NODE_INFO) * MscNodeCount);
  if (MscNodeInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate for MSC Nodes\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  // Populate the MSC Node info
  for (Index = 0; Index < MscNodeCount; Index++) {
    RegisterSize = 0;
    Status       = GetDeviceTreeRegisters (MscNodeHandles[Index], RegisterData, &RegisterSize);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      if (RegisterData != NULL) {
        FreePool (RegisterData);
        RegisterData = NULL;
      }

      RegisterData = (NVIDIA_DEVICE_TREE_REGISTER_DATA *)AllocateZeroPool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * RegisterSize);
      if (RegisterData == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      Status = GetDeviceTreeRegisters (MscNodeHandles[Index], RegisterData, &RegisterSize);
      if (EFI_ERROR (Status)) {
        goto Exit;
      }
    } else if (EFI_ERROR (Status)) {
      goto Exit;
    }

    if (RegisterData == NULL) {
      ASSERT (FALSE);
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    MscNodeInfo[Index].Token       = TokenMap[Index];
    MscNodeInfo[Index].BaseAddress = RegisterData->BaseAddress;
    MscNodeInfo[Index].MmioSize    =  RegisterData->Size;

    // Get Interrupts information
    InterruptSize = 0;
    Status        = GetDeviceTreeInterrupts (MscNodeHandles[Index], InterruptData, &InterruptSize);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      if (InterruptData != NULL) {
        FreePool (InterruptData);
        InterruptData = NULL;
      }

      InterruptData = (NVIDIA_DEVICE_TREE_INTERRUPT_DATA *)AllocateZeroPool (sizeof (NVIDIA_DEVICE_TREE_INTERRUPT_DATA) * InterruptSize);
      if (InterruptData == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      Status = GetDeviceTreeInterrupts (MscNodeHandles[Index], InterruptData, &InterruptSize);
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      if (InterruptData == NULL) {
        ASSERT (FALSE);
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      // loop through interrupt data to find the names of interrupts present
      for (InterruptIndex = 0; InterruptIndex < InterruptSize; InterruptIndex++) {
        if ((InterruptData[InterruptIndex].Name != NULL) &&
            (AsciiStrCmp (InterruptData[InterruptIndex].Name, "error") == 0))
        {
          ASSERT (InterruptData[InterruptIndex].Type == INTERRUPT_SPI_TYPE);
          MscNodeInfo[Index].ErrorInterrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData[InterruptIndex]);

          // Affinity routed to the socket ID
          MscNodeInfo[Index].ErrorInterruptAff = Index;

          if ((InterruptData[InterruptIndex].Flag == INTERRUPT_HI_LEVEL) ||
              (InterruptData[InterruptIndex].Flag == INTERRUPT_LO_LEVEL))
          {
            MscNodeInfo[Index].ErrorInterruptFlags = EFI_ACPI_MPAM_INTERRUPT_LEVEL_TRIGGERED;
          } else {
            MscNodeInfo[Index].ErrorInterruptFlags = EFI_ACPI_MPAM_INTERRUPT_EDGE_TRIGGERED;
          }
        } else if ((InterruptData[InterruptIndex].Name != NULL) &&
                   (AsciiStrCmp (InterruptData[InterruptIndex].Name, "overflow") == 0))
        {
          ASSERT (InterruptData[InterruptIndex].Type == INTERRUPT_SPI_TYPE);
          MscNodeInfo[Index].OverflowInterrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData[InterruptIndex]);

          // Affinity routed to the socket ID
          MscNodeInfo[Index].OverflowInterruptAff = Index;
          if ((InterruptData[InterruptIndex].Flag == INTERRUPT_HI_LEVEL) ||
              (InterruptData[InterruptIndex].Flag == INTERRUPT_LO_LEVEL))
          {
            MscNodeInfo[Index].OverflowInterruptFlags = EFI_ACPI_MPAM_INTERRUPT_LEVEL_TRIGGERED;
          } else {
            MscNodeInfo[Index].OverflowInterruptFlags = EFI_ACPI_MPAM_INTERRUPT_EDGE_TRIGGERED;
          }
        }
      }
    }

    // Gather Not Ready Signal time
    Status = GetDeviceTreeNode (MscNodeHandles[Index], &DeviceTreeBase, &NodeOffset);
    if (EFI_ERROR (Status)) {
      goto Exit;
    }

    MpamProp = fdt_getprop (DeviceTreeBase, NodeOffset, "arm,not-ready-us", NULL);
    if (MpamProp == NULL) {
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }

    MscNodeInfo[Index].MaxNRdyUsec = SwapBytes32 (*MpamProp);

    // using pHandle as unique identifier
    pHandle                       = fdt_get_phandle (DeviceTreeBase, NodeOffset);
    MscNodeInfo[Index].Identifier = pHandle;

    // Assign HID and UID based on socket ID
    MscNodeInfo[Index].LinkedDeviceHwId = SOCKETID_FROM_PHYS_ADDR (MscNodeInfo[Index].BaseAddress);
    /// Linked Device Instance ID
    MscNodeInfo[Index].LinkedDeviceInstanceHwId = SOCKETID_FROM_PHYS_ADDR (MscNodeInfo[Index].BaseAddress);

    MscNodeInfo[Index].NumResourceNodes = 0;
    // Count all resource nodes for this MSC node
    SubNodeOffset     = 0;
    PrevSubNodeOffset = 0;
    for (SubNodeOffset = fdt_first_subnode (DeviceTreeBase, NodeOffset);
         SubNodeOffset > 0;
         SubNodeOffset = fdt_next_subnode (DeviceTreeBase, PrevSubNodeOffset))
    {
      PrevSubNodeOffset = SubNodeOffset;
      if (!EFI_ERROR (DeviceTreeCheckNodeSingleCompatibility ("arm,mpam-cache", SubNodeOffset))) {
        MscNodeInfo[Index].NumResourceNodes++;
      }
    }

    MscNodeEntries++;
  }

  // Add MSC Nodes to repo
  Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjMscNodeInfo);
  Desc.Size     = sizeof (CM_ARM_MSC_NODE_INFO) * MscNodeEntries;
  Desc.Count    = MscNodeEntries;
  Desc.Data     = MscNodeInfo;

  Status = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add MSC Nodes for MPAM\n", __FUNCTION__, Status));
    goto Exit;
  }

Exit:
  if (EFI_ERROR (Status)) {
    FREE_NON_NULL (MscNodeInfo);
  }

  FREE_NON_NULL (MscNodeHandles);
  FREE_NON_NULL (RegisterData);
  FREE_NON_NULL (InterruptData);
  FREE_NON_NULL (TokenMap);

  return Status;
}

BOOLEAN
EFIAPI
IsMpamEnabled (
  )
{
  EFI_STATUS  Status;
  UINT32      NumberOfMscNodes;

  NumberOfMscNodes = 0;

  Status = GetMatchingEnabledDeviceTreeNodes ("arm,mpam-msc", NULL, &NumberOfMscNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  return FALSE;
}

EFI_STATUS
EFIAPI
MpamParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                  Status;
  CM_STD_OBJ_ACPI_TABLE_INFO  AcpiTableHeader;

  if (!IsMpamEnabled ()) {
    return EFI_SUCCESS;
  }

  Status = UpdateMscNodeInfo (ParserHandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateResourceNodeInfo (ParserHandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Create a ACPI Table Entry
  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_MEMORY_SYSTEM_RESOURCE_PARTITIONING_AND_MONITORING_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_MEMORY_SYSTEM_RESOURCE_PARTITIONING_AND_MONITORING_TABLE_REVISION;
  AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMpam);
  AcpiTableHeader.AcpiTableData      = NULL;
  AcpiTableHeader.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  AcpiTableHeader.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader.MinorRevision      = 0;

  Status = NvAddAcpiTableGenerator (ParserHandle, &AcpiTableHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add the MPAM SSDT table\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  return Status;
}

REGISTER_PARSER_FUNCTION (MpamParser, "skip-mpam-table")
