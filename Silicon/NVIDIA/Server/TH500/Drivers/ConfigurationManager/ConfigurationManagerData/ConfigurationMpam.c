/** @file
  Configuration Manager Data of MPAM Table
  Memory System Resource Partitioning and Monitoring Table

  SPDX-FileCopyrightText: Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <libfdt.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <ConfigurationManagerDataPrivate.h>

#include <TH500/TH500Definitions.h>

#include <IndustryStandard/Mpam.h>

// Generate socket Id from Phy addr. Max 4 sockets supported
#define SOCKETID_FROM_PHYS_ADDR(phys)  (((phys) >> 43) & 0x3)

EFI_STATUS
EFIAPI
UpdateResourceNodeInfo (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo
  )
{
  UINT32                          Index;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  CM_ARM_RESOURCE_NODE_INFO       *ResourceNodeInfo;
  UINT32                          ResourceNodeCount;
  UINT32                          *ResourceNodeHandles;
  VOID                            *DeviceTreeBase;
  INT32                           NodeOffset;
  UINT32                          pHandle;
  CONST UINT32                    *MpamProp;
  EFI_STATUS                      Status;

  ResourceNodeInfo    = NULL;
  ResourceNodeHandles = NULL;
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

  ResourceNodeHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * ResourceNodeCount);
  if (ResourceNodeHandles == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = GetMatchingEnabledDeviceTreeNodes ("arm,mpam-cache", ResourceNodeHandles, &ResourceNodeCount);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  ResourceNodeInfo = (CM_ARM_RESOURCE_NODE_INFO *)AllocatePool (sizeof (CM_ARM_RESOURCE_NODE_INFO) * ResourceNodeCount);
  if (ResourceNodeInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate for Resource Nodes\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  for (Index = 0; Index < ResourceNodeCount; Index++) {
    ResourceNodeInfo[Index].RisIndex    = 0;
    ResourceNodeInfo[Index].LocatorType = EFI_ACPI_MPAM_LOCATION_TYPE_PROCESSOR_CACHE;

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
    ResourceNodeInfo[Index].Locator1 = GetCacheIdFrompHandle (SwapBytes32 (*MpamProp));
    ResourceNodeInfo[Index].Locator2 = 0;

    // TODO: Func Dependency List
    ResourceNodeInfo[Index].NumFuncDep = 0;
  }

  // Add Resource Nodes to repo
  Repo                = *PlatformRepositoryInfo;
  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjResNodeInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_ARM_RESOURCE_NODE_INFO) * ResourceNodeCount;
  Repo->CmObjectCount = ResourceNodeCount;
  Repo->CmObjectPtr   = ResourceNodeInfo;
  Repo++;

  *PlatformRepositoryInfo = Repo;

Exit:
  if (EFI_ERROR (Status)) {
    if (ResourceNodeInfo != NULL) {
      FreePool (ResourceNodeInfo);
    }
  }

  if (ResourceNodeHandles != NULL) {
    FreePool (ResourceNodeHandles);
  }

  return Status;
}

EFI_STATUS
EFIAPI
UpdateMscNodeInfo (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo
  )
{
  UINT32                             Index;
  EDKII_PLATFORM_REPOSITORY_INFO     *Repo;
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

  MscNodeInfo    = NULL;
  MscNodeHandles = NULL;
  RegisterData   = NULL;
  InterruptData  = NULL;
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

  MscNodeHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * MscNodeCount);
  if (MscNodeHandles == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = GetMatchingEnabledDeviceTreeNodes ("arm,mpam-msc", MscNodeHandles, &MscNodeCount);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  MscNodeInfo = (CM_ARM_MSC_NODE_INFO *)AllocatePool (sizeof (CM_ARM_MSC_NODE_INFO) * MscNodeCount);
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

      RegisterData = (NVIDIA_DEVICE_TREE_REGISTER_DATA *)AllocatePool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * RegisterSize);
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

      InterruptData = (NVIDIA_DEVICE_TREE_INTERRUPT_DATA *)AllocatePool (sizeof (NVIDIA_DEVICE_TREE_INTERRUPT_DATA) * InterruptSize);
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
          MscNodeInfo[Index].ErrorInterrupt = InterruptData[InterruptIndex].Interrupt +
                                              (InterruptData[InterruptIndex].Type == INTERRUPT_SPI_TYPE ?
                                               DEVICETREE_TO_ACPI_SPI_INTERRUPT_OFFSET :
                                               DEVICETREE_TO_ACPI_PPI_INTERRUPT_OFFSET);

          // Affinity routed to the socket ID
          MscNodeInfo[Index].ErrorInterruptAff = Index;

          if ((InterruptData[InterruptIndex].Flag == INTERRUPT_HI_LEVEL) ||
              (InterruptData[InterruptIndex].Flag == INTERRUPT_LO_LEVEL))
          {
            MscNodeInfo[Index].ErrorInterruptFlags = EFI_ACPI_MPAM_LEVEL_TRIG_INTERRUPT_MODE;
          } else {
            MscNodeInfo[Index].ErrorInterruptFlags = EFI_ACPI_MPAM_EDGE_TRIG_INTERRUPT_MODE;
          }
        } else if ((InterruptData[InterruptIndex].Name != NULL) &&
                   (AsciiStrCmp (InterruptData[InterruptIndex].Name, "overflow") == 0))
        {
          ASSERT (InterruptData[InterruptIndex].Type == INTERRUPT_SPI_TYPE);
          MscNodeInfo[Index].OverflowInterrupt = InterruptData[InterruptIndex].Interrupt +
                                                 (InterruptData[InterruptIndex].Type == INTERRUPT_SPI_TYPE ?
                                                  DEVICETREE_TO_ACPI_SPI_INTERRUPT_OFFSET :
                                                  DEVICETREE_TO_ACPI_PPI_INTERRUPT_OFFSET);

          // Affinity routed to the socket ID
          MscNodeInfo[Index].OverflowInterruptAff = Index;
          if ((InterruptData[InterruptIndex].Flag == INTERRUPT_HI_LEVEL) ||
              (InterruptData[InterruptIndex].Flag == INTERRUPT_LO_LEVEL))
          {
            MscNodeInfo[Index].OverflowInterruptFlags = EFI_ACPI_MPAM_LEVEL_TRIG_INTERRUPT_MODE;
          } else {
            MscNodeInfo[Index].OverflowInterruptFlags = EFI_ACPI_MPAM_EDGE_TRIG_INTERRUPT_MODE;
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
      if (fdt_node_check_compatible (DeviceTreeBase, SubNodeOffset, "arm,mpam-cache") == 0) {
        MscNodeInfo[Index].NumResourceNodes++;
      }
    }

    MscNodeEntries++;
  }

  // Add MSC Nodes to repo
  Repo                = *PlatformRepositoryInfo;
  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjMscNodeInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_ARM_MSC_NODE_INFO) * MscNodeEntries;
  Repo->CmObjectCount = MscNodeEntries;
  Repo->CmObjectPtr   = MscNodeInfo;
  Repo++;

  *PlatformRepositoryInfo = Repo;

Exit:
  if (EFI_ERROR (Status)) {
    if (MscNodeInfo != NULL) {
      FreePool (MscNodeInfo);
    }
  }

  if (MscNodeHandles != NULL) {
    FreePool (MscNodeHandles);
  }

  if (RegisterData != NULL) {
    FreePool (RegisterData);
  }

  if (InterruptData != NULL) {
    FreePool (InterruptData);
  }

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
InstallMpamTable (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  EFI_STATUS                      Status;
  CM_STD_OBJ_ACPI_TABLE_INFO      *NewAcpiTables;
  UINTN                           Index;

  // Create a ACPI Table Entry
  for (Index = 0; Index < PcdGet32 (PcdConfigMgrObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (
                                                      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize +
                                                      (sizeof (CM_STD_OBJ_ACPI_TABLE_INFO)),
                                                      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr
                                                      );

      if (NewAcpiTables == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_4_MEMORY_SYSTEM_RESOURCE_PARTITIONING_MONITORING_TABLE_STRUCTURE_SIGNATURE;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = EFI_ACPI_6_4_MEMORY_SYSTEM_RESOURCE_PARTITIONING_MONITORING_TABLE_REVISION;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMpam);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData      = NULL;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].MinorRevision      = 0;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }

  Repo = *PlatformRepositoryInfo;

  Status = UpdateMscNodeInfo (&Repo);
  if (Status == EFI_SUCCESS) {
    Status = UpdateResourceNodeInfo (&Repo);
  }

  ASSERT ((UINTN)Repo <= PlatformRepositoryInfoEnd);

  *PlatformRepositoryInfo = Repo;

  return Status;
}
