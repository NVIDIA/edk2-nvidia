/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2021-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>

#include <Library/AcpiHelperLib.h>
#include <Library/AmlLib/AmlLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <PiDxe.h>
#include <Library/UefiBootServicesTableLib.h>

#include <ArmNameSpaceObjects.h>

#include <Protocol/AcpiTable.h>

// _SB scope of the AML namespace.
#define SB_SCOPE  "\\_SB_"

#define ACPI_HID_CID_STR_SIZE  (8+1)

#define FREE_NON_NULL(a) \
  if ((a) != NULL) { \
    FreePool ((a));  \
    (a) = NULL;      \
  }

typedef enum {
  UID_INDEX_VRT = 0,
  UID_INDEX_USB = 1,

  UID_INDEX_COUNT
} UID_INDEX;

STATIC UINTN  mUids[UID_INDEX_COUNT];

typedef struct {
  CONST CHAR8    *CompatibleId;
  CHAR8          *Hid;
  CHAR8          *Cid;
  CHAR8          *Name;
  UID_INDEX      UidIndex;
  BOOLEAN        Cca;
  UINT8          LimitMemoryRanges;
  UINT8          LimitInterrupts;
} ACPI_DEVICE_TABLE_INFO;

STATIC ACPI_DEVICE_TABLE_INFO  AcpiTableInfo[] = {
  {
    "virtio,mmio",
    "LNRO0005",
    NULL,
    "VIRx",
    UID_INDEX_VRT,
    FALSE,
    1,
    1
  },
  // USB
  {
    "nvidia,tegra186-xhci",
    "NVDA0214",
    "PNP0D10",
    "USBx",
    UID_INDEX_USB,
    FALSE,
    1,
    1
  },
  {
    "nvidia,tegra186-xusb",
    "NVDA0214",
    "PNP0D10",
    "USBx",
    UID_INDEX_USB,
    FALSE,
    1,
    1
  },
  {
    "nvidia,tegra194-xhci",
    "NVDA0214",
    "PNP0D10",
    "USBx",
    UID_INDEX_USB,
    FALSE,
    1,
    1
  },
  {
    "nvidia,tegra194-xusb",
    "NVDA0214",
    "PNP0D10",
    "USBx",
    UID_INDEX_USB,
    FALSE,
    1,
    1
  },
  {
    "nvidia,tegra234-xhci",
    "NVDA0214",
    "PNP0D10",
    "USBx",
    UID_INDEX_USB,
    FALSE,
    1,
    1
  },
  {
    "nvidia,tegra234-xusb",
    "NVDA0214",
    "PNP0D10",
    "USBx",
    UID_INDEX_USB,
    FALSE,
    1,
    1
  }
};

typedef struct {
  UINT64    BaseAddress;
  UINT64    Size;
} MEMORY_RANGE_INFO;

typedef struct {
  CHAR8                       Name[AML_NAME_SEG_SIZE + 1]; // < ACPI device name
  CHAR8                       Hid[ACPI_HID_CID_STR_SIZE];  // < HID of the device
  CHAR8                       Cid[ACPI_HID_CID_STR_SIZE];  // < CID of the device, "\0" will cause no CID to be generated
  UINT8                       Uid;                         // < Unique ID of the device (per HID)
  BOOLEAN                     Cca;                         // < Should CCA be set to 1
  UINT32                      MemoryRangeCount;            // < How many memory ranges
  MEMORY_RANGE_INFO           *MemoryRangeArray;           // < Pointer to memory range array
  UINT32                      InterruptArrayCount;         // < How many entries are in the interrupt array
  CM_ARM_GENERIC_INTERRUPT    *InterruptArray;             // < Pointer to interrupt array
} ACPI_DEVICE_OBJECT;

typedef struct {
  LIST_ENTRY            Link;
  ACPI_DEVICE_OBJECT    AcpiDevice;
} ACPI_DEVICE_ENTRY;

STATIC LIST_ENTRY  mDeviceList = INITIALIZE_LIST_HEAD_VARIABLE (mDeviceList);

/**
  Frees up the memory used by an ACPI_DEVICE_ENTRY

  @param[in]  DeviceListEntry   Object that describes the device

**/
STATIC
VOID
FreeDeviceEntry (
  IN ACPI_DEVICE_ENTRY  *DeviceListEntry
  )
{
  if (DeviceListEntry) {
    FREE_NON_NULL (DeviceListEntry->AcpiDevice.MemoryRangeArray);
    FREE_NON_NULL (DeviceListEntry->AcpiDevice.InterruptArray);
    FreePool (DeviceListEntry);
  }
}

/**
  Creates and adds a device to the ACPI SSDT scope

  @param[in]  ScopeNode      Node to add the device to
  @param[in]  DeviceObject   Object that describes the device

  @retval EFI_SUCCESS           Device added.
  @retval EFI_OUT_OF_RESOURCES  Not enough memory for initialization.
**/
STATIC
EFI_STATUS
EFIAPI
AddAcpiDevice (
  IN AML_OBJECT_NODE_HANDLE  *ScopeNode,
  IN ACPI_DEVICE_OBJECT      *DeviceObject
  )
{
  EFI_STATUS              Status;
  AML_OBJECT_NODE_HANDLE  DeviceNode;
  AML_OBJECT_NODE_HANDLE  CrsNode;
  BOOLEAN                 DeviceAttached;
  UINT32                  Index;
  BOOLEAN                 EdgeTriggered;
  BOOLEAN                 ActiveLow;

  DeviceAttached = FALSE;

  Status = AmlCodeGenDevice (DeviceObject->Name, ScopeNode, &DeviceNode);
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    goto Exit;
  }

  DeviceAttached = TRUE;

  Status = AmlCodeGenNameString (
             "_HID",
             DeviceObject->Hid,
             DeviceNode,
             NULL
             );
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    goto Exit;
  }

  if (DeviceObject->Cid[0] != '\0') {
    Status = AmlCodeGenNameString (
               "_CID",
               DeviceObject->Cid,
               DeviceNode,
               NULL
               );
    if (EFI_ERROR (Status)) {
      ASSERT (0);
      goto Exit;
    }
  }

  Status = AmlCodeGenNameInteger ("_UID", DeviceObject->Uid, DeviceNode, NULL);
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    goto Exit;
  }

  Status = AmlCodeGenNameInteger ("_CCA", DeviceObject->Cca, DeviceNode, NULL);
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    goto Exit;
  }

  if ((DeviceObject->MemoryRangeCount != 0) || (DeviceObject->InterruptArrayCount != 0)) {
    Status = AmlCodeGenNameResourceTemplate ("_CRS", DeviceNode, &CrsNode);
    if (EFI_ERROR (Status)) {
      ASSERT (0);
      goto Exit;
    }

    for (Index = 0; Index < DeviceObject->MemoryRangeCount; Index++) {
      Status = AmlCodeGenRdQWordMemory (
                 TRUE,
                 TRUE,
                 FALSE,
                 FALSE,
                 0,
                 TRUE,
                 0,
                 DeviceObject->MemoryRangeArray[Index].BaseAddress,
                 DeviceObject->MemoryRangeArray[Index].BaseAddress + DeviceObject->MemoryRangeArray[Index].Size - 1,
                 0,
                 DeviceObject->MemoryRangeArray[Index].Size,
                 0,
                 NULL,
                 0,
                 TRUE,
                 CrsNode,
                 NULL
                 );
      if (EFI_ERROR (Status)) {
        ASSERT (0);
        goto Exit;
      }
    }

    for (Index = 0; Index < DeviceObject->InterruptArrayCount; Index++) {
      EdgeTriggered = ((DeviceObject->InterruptArray[Index].Flags & BIT0) == BIT0);
      ActiveLow     = ((DeviceObject->InterruptArray[Index].Flags & BIT1) == BIT1);

      Status = AmlCodeGenRdInterrupt (
                 TRUE,
                 EdgeTriggered,
                 ActiveLow,
                 FALSE,
                 &DeviceObject->InterruptArray[Index].Interrupt,
                 1,
                 CrsNode,
                 NULL
                 );
      if (EFI_ERROR (Status)) {
        ASSERT (0);
        goto Exit;
      }
    }
  }

Exit:
  if (EFI_ERROR (Status) && DeviceAttached) {
    AmlDetachNode (DeviceNode);
    AmlDeleteTree (DeviceNode);
  }

  return Status;
}

/**
  Callback when ACPI protocol is ready.

  @param[in]  Event     Event that triggered this callback
  @param[in]  Context   Head of the list

**/
STATIC
VOID
EFIAPI
AcpiProtocolReady (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                   Status;
  EFI_ACPI_TABLE_PROTOCOL      *AcpiTableProtocol;
  AML_ROOT_NODE_HANDLE         RootNode;
  AML_OBJECT_NODE_HANDLE       ScopeNode;
  UINT64                       OemTableId;
  UINT64                       OemId;
  EFI_ACPI_DESCRIPTION_HEADER  *AcpiTable;
  UINTN                        TableHandle;
  LIST_ENTRY                   *ListHead;
  LIST_ENTRY                   *CurrentNode;
  LIST_ENTRY                   *NextNode;
  ACPI_DEVICE_ENTRY            *DeviceEntry;

  ListHead  = (LIST_ENTRY *)Context;
  RootNode  = NULL;
  ScopeNode = NULL;
  AcpiTable = NULL;

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&AcpiTableProtocol);
  if (EFI_ERROR (Status)) {
    goto Cleanup;
  }

  gBS->CloseEvent (Event);

  CopyMem (&OemId, PcdGetPtr (PcdAcpiDefaultOemId), sizeof (OemId));
  OemTableId = PcdGet64 (PcdAcpiDefaultOemTableId);

  Status = AmlCodeGenDefinitionBlock (
             "SSDT",
             (CONST CHAR8 *)&OemId,
             (CONST CHAR8 *)&OemTableId,
             FixedPcdGet64 (PcdAcpiDefaultOemRevision),
             &RootNode
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create SSDT header - %r\r\n", __FUNCTION__, Status));
    ASSERT_EFI_ERROR (Status);
    goto Cleanup;
  }

  Status = AmlCodeGenScope (SB_SCOPE, RootNode, &ScopeNode);
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    goto Cleanup;
  }

  BASE_LIST_FOR_EACH (CurrentNode, ListHead) {
    DeviceEntry = (ACPI_DEVICE_ENTRY *)CurrentNode;
    AddAcpiDevice (ScopeNode, &DeviceEntry->AcpiDevice);
  }

  // Serialize the tree.
  Status = AmlSerializeDefinitionBlock (
             RootNode,
             &AcpiTable
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ERROR: SSDT-DT: Failed to Serialize SSDT Table Data."
      " Status = %r\n",
      __FUNCTION__,
      Status
      ));
    ASSERT_EFI_ERROR (Status);
  } else {
    Status = AcpiTableProtocol->InstallAcpiTable (
                                  AcpiTableProtocol,
                                  AcpiTable,
                                  AcpiTable->Length,
                                  &TableHandle
                                  );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: ERROR: Failed to Install ACPI Table. Status = %r\n",
        __FUNCTION__,
        Status
        ));
      ASSERT_EFI_ERROR (Status);
    }
  }

Cleanup:
  BASE_LIST_FOR_EACH_SAFE (CurrentNode, NextNode, ListHead) {
    FreeDeviceEntry ((ACPI_DEVICE_ENTRY *)CurrentNode);
  }

  if (EFI_ERROR (Status)) {
    FREE_NON_NULL (AcpiTable);
  }

  FREE_NON_NULL (ScopeNode);
  FREE_NON_NULL (RootNode);
}

/**
  Initialize the SSDT DTB Generation Driver.

  @param[in]  ListHead      Head of the list to add to
  @param[in]  DeviceHandle  DeviceTreeHelper handle of the device
  @param[in]  DeviceInfo    Pointer to the dtb->acpi mapping table

  @retval EFI_SUCCESS           Initialization succeeded.
  @retval EFI_OUT_OF_RESOURCES  Not enough memory for initialization.
**/
STATIC
EFI_STATUS
EFIAPI
AddDeviceObjectList (
  LIST_ENTRY                 *ListHead,
  IN UINT32                  DeviceHandle,
  IN ACPI_DEVICE_TABLE_INFO  *DeviceInfo
  )
{
  EFI_STATUS                         Status;
  ACPI_DEVICE_ENTRY                  *DeviceListEntry;
  NVIDIA_DEVICE_TREE_REGISTER_DATA   *RegisterArray;
  UINT32                             NumberOfRegisters;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA  *InterruptArray;
  UINT32                             NumberOfInterrupts;
  UINT32                             Index;
  UINTN                              Uid;

  DeviceListEntry = (ACPI_DEVICE_ENTRY *)AllocateZeroPool (sizeof (ACPI_DEVICE_ENTRY));
  if (DeviceListEntry == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Get resources
  RegisterArray      = NULL;
  NumberOfRegisters  = 0;
  InterruptArray     = NULL;
  NumberOfInterrupts = 0;

  Status = GetDeviceTreeRegisters (DeviceHandle, NULL, &NumberOfRegisters);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    RegisterArray = (NVIDIA_DEVICE_TREE_REGISTER_DATA *)AllocatePool (NumberOfRegisters * sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA));
    if (RegisterArray == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate register array\r\n", __FUNCTION__));
      ASSERT (FALSE);
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    Status = GetDeviceTreeRegisters (DeviceHandle, RegisterArray, &NumberOfRegisters);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get register array - %r\r\n", __FUNCTION__, Status));
      goto Exit;
    }
  } else if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "%a: Device has no registers\n", __FUNCTION__));
    Status = EFI_SUCCESS;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Failed to determine number of registers - %r\r\n", __FUNCTION__, Status));
    goto Exit;
  }

  NumberOfRegisters = MIN (NumberOfRegisters, DeviceInfo->LimitMemoryRanges);

  Status = GetDeviceTreeInterrupts (DeviceHandle, NULL, &NumberOfInterrupts);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    InterruptArray = (NVIDIA_DEVICE_TREE_INTERRUPT_DATA *)AllocatePool (NumberOfInterrupts * sizeof (NVIDIA_DEVICE_TREE_INTERRUPT_DATA));
    if (InterruptArray == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate interrupt array\r\n", __FUNCTION__));
      ASSERT (FALSE);
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    Status = GetDeviceTreeInterrupts (DeviceHandle, InterruptArray, &NumberOfInterrupts);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get interrupt array - %r\r\n", __FUNCTION__, Status));
      goto Exit;
    }
  } else if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "%a: Device has no interrupts\n", __FUNCTION__));
    Status = EFI_SUCCESS;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Failed to determine number of interrupts - %r\r\n", __FUNCTION__, Status));
    goto Exit;
  }

  NumberOfInterrupts = MIN (NumberOfInterrupts, DeviceInfo->LimitInterrupts);

  // Build Device
  // Write the name of the device.
  Uid = mUids[DeviceInfo->UidIndex];
  CopyMem (DeviceListEntry->AcpiDevice.Name, DeviceInfo->Name, AML_NAME_SEG_SIZE + 1);
  DeviceListEntry->AcpiDevice.Name[AML_NAME_SEG_SIZE - 1] = AsciiFromHex (Uid & 0xF);
  if (Uid > 0xF) {
    DeviceListEntry->AcpiDevice.Name[AML_NAME_SEG_SIZE - 2] = AsciiFromHex ((Uid >> 4) & 0xF);
  }

  CopyMem (DeviceListEntry->AcpiDevice.Hid, DeviceInfo->Hid, ACPI_HID_CID_STR_SIZE);

  if (DeviceInfo->Cid != NULL) {
    CopyMem (DeviceListEntry->AcpiDevice.Cid, DeviceInfo->Cid, ACPI_HID_CID_STR_SIZE);
  } else {
    ZeroMem (DeviceListEntry->AcpiDevice.Cid, ACPI_HID_CID_STR_SIZE);
  }

  DeviceListEntry->AcpiDevice.Uid = Uid;
  DeviceListEntry->AcpiDevice.Cca = DeviceInfo->Cca;

  if (NumberOfRegisters != 0) {
    DeviceListEntry->AcpiDevice.MemoryRangeArray = AllocatePool (NumberOfRegisters * sizeof (MEMORY_RANGE_INFO));
    if (DeviceListEntry->AcpiDevice.MemoryRangeArray == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    DeviceListEntry->AcpiDevice.MemoryRangeCount = NumberOfRegisters;
    for (Index = 0; Index < NumberOfRegisters; Index++) {
      DeviceListEntry->AcpiDevice.MemoryRangeArray[Index].BaseAddress = RegisterArray[Index].BaseAddress;
      DeviceListEntry->AcpiDevice.MemoryRangeArray[Index].Size        = RegisterArray[Index].Size;
      DEBUG ((
        DEBUG_INFO,
        "%a: Added Register %a 0x%llx++0x%llx\n",
        __FUNCTION__,
        RegisterArray[Index].Name,
        DeviceListEntry->AcpiDevice.MemoryRangeArray[Index].BaseAddress,
        DeviceListEntry->AcpiDevice.MemoryRangeArray[Index].Size
        ));
    }
  }

  if (NumberOfInterrupts != 0) {
    DeviceListEntry->AcpiDevice.InterruptArray = AllocatePool (NumberOfInterrupts * sizeof (CM_ARM_GENERIC_INTERRUPT));
    if (DeviceListEntry->AcpiDevice.InterruptArray == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    DeviceListEntry->AcpiDevice.InterruptArrayCount = NumberOfInterrupts;
    for (Index = 0; Index < NumberOfInterrupts; Index++) {
      // Ignore the PMC interrupts
      if (InterruptArray[Index].ControllerCompatible && (AsciiStrStr (InterruptArray[Index].ControllerCompatible, "pmc") == NULL)) {
        DeviceListEntry->AcpiDevice.InterruptArray[Index].Interrupt = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptArray[Index]);

        DeviceListEntry->AcpiDevice.InterruptArray[Index].Flags = 0;
        if ((InterruptArray[Index].Flag == INTERRUPT_LO_TO_HI_EDGE) ||
            (InterruptArray[Index].Flag == INTERRUPT_HI_TO_LO_EDGE))
        {
          DeviceListEntry->AcpiDevice.InterruptArray[Index].Flags |= BIT0;
        }

        if ((InterruptArray[Index].Flag == INTERRUPT_LO_LEVEL) ||
            (InterruptArray[Index].Flag == INTERRUPT_HI_TO_LO_EDGE))
        {
          DeviceListEntry->AcpiDevice.InterruptArray[Index].Flags |= BIT1;
        }

        DEBUG ((
          DEBUG_INFO,
          "%a: Added Interrupt %a %d, Flags %d\n",
          __FUNCTION__,
          InterruptArray[Index].Name,
          DeviceListEntry->AcpiDevice.InterruptArray[Index].Interrupt,
          DeviceListEntry->AcpiDevice.InterruptArray[Index].Flags
          ));
      } else {
        DEBUG ((DEBUG_INFO, "%a: Skipping interrupt for controller %a\n", __FUNCTION__, InterruptArray[Index].ControllerCompatible));
      }
    }
  }

  mUids[DeviceInfo->UidIndex]++;
  InsertTailList (ListHead, &DeviceListEntry->Link);

Exit:
  if (EFI_ERROR (Status)) {
    FreeDeviceEntry (DeviceListEntry);
  }

  FREE_NON_NULL (RegisterArray);
  FREE_NON_NULL (InterruptArray);

  return Status;
}

/**
  Initialize the List of ACPI devices.

  @param[in]  ListHead   Head pointer of the list

  @retval EFI_SUCCESS           List created.
  @retval EFI_OUT_OF_RESOURCES  Not enough memory for initialization.
  @retval EFI_NOT_FOUND         No devices found
**/
STATIC
EFI_STATUS
BuildDeviceList (
  LIST_ENTRY  *ListHead
  )
{
  EFI_STATUS  Status;
  EFI_STATUS  ReturnStatus;
  UINT32      DeviceTypeIndex;
  UINT32      NumberOfNodes[ARRAY_SIZE (AcpiTableInfo)];
  UINT32      TotalNumberOfNodes;
  UINT32      NodeBaseIndex;
  UINT32      NodeIndex;
  UINT32      CompareNodeIndex;
  UINT32      *NodeHandles;
  UINT32      NodeHandle;
  UINT32      UidIndex;

  NodeHandles = NULL;

  for (UidIndex = 0; UidIndex < UID_INDEX_COUNT; UidIndex++) {
    mUids[UidIndex] = 0;
  }

  // Count total nodes
  TotalNumberOfNodes = 0;
  for (DeviceTypeIndex = 0; DeviceTypeIndex < ARRAY_SIZE (AcpiTableInfo); DeviceTypeIndex++) {
    NumberOfNodes[DeviceTypeIndex] = 0;
    Status                         = GetMatchingEnabledDeviceTreeNodes (AcpiTableInfo[DeviceTypeIndex].CompatibleId, NULL, &NumberOfNodes[DeviceTypeIndex]);
    if (Status != EFI_BUFFER_TOO_SMALL) {
      continue;
    }

    TotalNumberOfNodes += NumberOfNodes[DeviceTypeIndex];
  }

  // Get all the nodes
  NodeHandles = (UINT32 *)AllocateZeroPool (TotalNumberOfNodes * sizeof (UINT32));
  if (NodeHandles == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate node handles\r\n", __FUNCTION__)); // JDS TODO - use new asserts
    ASSERT (FALSE);
    return EFI_OUT_OF_RESOURCES;
  }

  NodeBaseIndex = 0;
  for (DeviceTypeIndex = 0; DeviceTypeIndex < ARRAY_SIZE (AcpiTableInfo); DeviceTypeIndex++) {
    if (NumberOfNodes[DeviceTypeIndex] > 0) {
      Status = GetMatchingEnabledDeviceTreeNodes (AcpiTableInfo[DeviceTypeIndex].CompatibleId, &NodeHandles[NodeBaseIndex], &NumberOfNodes[DeviceTypeIndex]);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to get node handles for %a - %r\r\n", __FUNCTION__, AcpiTableInfo[DeviceTypeIndex].CompatibleId, Status));
      }

      NodeBaseIndex += NumberOfNodes[DeviceTypeIndex];
    }
  }

  // Add only the unique nodes
  ReturnStatus  = EFI_SUCCESS;
  NodeBaseIndex = 0;
  for (DeviceTypeIndex = 0; DeviceTypeIndex < ARRAY_SIZE (AcpiTableInfo); DeviceTypeIndex++) {
    for (NodeIndex = 0; NodeIndex < NumberOfNodes[DeviceTypeIndex]; NodeIndex++) {
      NodeHandle = NodeHandles[NodeBaseIndex + NodeIndex];
      if (NodeHandle == 0) {
        continue;
      } else {
        // Search previous nodes for duplicate, and skip this if found. Assume no duplicates within DeviceType
        for (CompareNodeIndex = 0; CompareNodeIndex < NodeBaseIndex; CompareNodeIndex++) {
          if (NodeHandles[CompareNodeIndex] == NodeHandle) {
            DEBUG ((DEBUG_INFO, "%a: Skipping %a Node %u as duplicate of previously added node\n", __FUNCTION__, AcpiTableInfo[DeviceTypeIndex].CompatibleId, NodeIndex));
            break;
          }
        }

        // Stop processing this node if we found a duplicate
        if (CompareNodeIndex < NodeBaseIndex) {
          continue;
        }
      }

      DEBUG ((DEBUG_INFO, "%a: Adding %a Node %u\n", __FUNCTION__, AcpiTableInfo[DeviceTypeIndex].CompatibleId, NodeIndex));
      Status = AddDeviceObjectList (ListHead, NodeHandle, &AcpiTableInfo[DeviceTypeIndex]);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error adding %a Node %u: %r\n", __FUNCTION__, AcpiTableInfo[DeviceTypeIndex].CompatibleId, NodeIndex, Status));
        if (!EFI_ERROR (ReturnStatus)) {
          ReturnStatus = Status;
        }
      }
    }

    NodeBaseIndex += NodeIndex;
  }

  FREE_NON_NULL (NodeHandles);

  if (!EFI_ERROR (ReturnStatus)) {
    if (IsListEmpty (ListHead)) {
      ReturnStatus = EFI_NOT_FOUND;
    }
  }

  return ReturnStatus;
}

/**
  Initialize the SSDT DTB Generation Driver.

  @param[in]  ImageHandle   of the loaded driver
  @param[in]  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Initialization succeeded.
  @retval EFI_DEVICE_ERROR      Failed to register callback
**/
EFI_STATUS
AcpiDtbSsdtGeneratorEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   mAcpiNotificationEvent;
  VOID        *AcpiNotificationRegistration;

  Status = BuildDeviceList (&mDeviceList);
  if (EFI_ERROR (Status)) {
    if (Status == EFI_NOT_FOUND) {
      return EFI_SUCCESS;
    } else {
      return Status;
    }
  }

  mAcpiNotificationEvent = EfiCreateProtocolNotifyEvent (
                             &gEfiAcpiTableProtocolGuid,
                             TPL_CALLBACK,
                             AcpiProtocolReady,
                             (VOID *)&mDeviceList,
                             &AcpiNotificationRegistration
                             );
  if (mAcpiNotificationEvent == NULL) {
    return EFI_DEVICE_ERROR;
  } else {
    return EFI_SUCCESS;
  }
}
