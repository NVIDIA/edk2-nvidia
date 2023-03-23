/** @file
*
*  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-FileCopyrightText: Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES
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

typedef struct {
  CONST CHAR8    *CompatibleId;
  CHAR8          *Hid;
  CHAR8          *Cid;
  CHAR8          *Name;
  UINTN          Uid;
  BOOLEAN        Cca;
} ACPI_DEVICE_TABLE_INFO;

STATIC ACPI_DEVICE_TABLE_INFO  AcpiTableInfo[] = {
  {
    "virtio,mmio",
    "LNRO0005",
    NULL,
    "VIRx",
    0,
    FALSE
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

LIST_ENTRY  gDeviceList = INITIALIZE_LIST_HEAD_VARIABLE (gDeviceList);

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
  ACPI_DEVICE_ENTRY            *DeviceObject;

  ListHead = (LIST_ENTRY *)Context;

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&AcpiTableProtocol);
  if (EFI_ERROR (Status)) {
    return;
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
    return;
  }

  Status = AmlCodeGenScope (SB_SCOPE, RootNode, &ScopeNode);
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    return;
  }

  BASE_LIST_FOR_EACH (CurrentNode, ListHead) {
    DeviceObject = (ACPI_DEVICE_ENTRY *)CurrentNode;
    AddAcpiDevice (ScopeNode, &DeviceObject->AcpiDevice);
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
    }
  }
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

  DeviceListEntry = (ACPI_DEVICE_ENTRY *)AllocateZeroPool (sizeof (ACPI_DEVICE_ENTRY));
  if (DeviceListEntry == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Get resources
  RegisterArray      = NULL;
  NumberOfRegisters  = 0;
  InterruptArray     = 0;
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
  }

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
  }

  // Build Device
  // Write the name of the PCI device.
  CopyMem (DeviceListEntry->AcpiDevice.Name, DeviceInfo->Name, AML_NAME_SEG_SIZE + 1);
  DeviceListEntry->AcpiDevice.Name[AML_NAME_SEG_SIZE - 1] = AsciiFromHex (DeviceInfo->Uid & 0xF);
  if (DeviceInfo->Uid > 0xF) {
    DeviceListEntry->AcpiDevice.Name[AML_NAME_SEG_SIZE - 2] = AsciiFromHex ((DeviceInfo->Uid >> 4) & 0xF);
  }

  CopyMem (DeviceListEntry->AcpiDevice.Hid, DeviceInfo->Hid, ACPI_HID_CID_STR_SIZE);

  if (DeviceInfo->Cid != NULL) {
    CopyMem (DeviceListEntry->AcpiDevice.Cid, DeviceInfo->Cid, ACPI_HID_CID_STR_SIZE);
  } else {
    ZeroMem (DeviceListEntry->AcpiDevice.Cid, ACPI_HID_CID_STR_SIZE);
  }

  DeviceListEntry->AcpiDevice.Uid = DeviceInfo->Uid;
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
      DeviceListEntry->AcpiDevice.InterruptArray[Index].Interrupt = InterruptArray[Index].Interrupt;
      if (InterruptArray[Index].Type == INTERRUPT_SPI_TYPE) {
        DeviceListEntry->AcpiDevice.InterruptArray[Index].Interrupt += DEVICETREE_TO_ACPI_SPI_INTERRUPT_OFFSET;
      } else if (InterruptArray[Index].Type == INTERRUPT_PPI_TYPE) {
        DeviceListEntry->AcpiDevice.InterruptArray[Index].Interrupt += DEVICETREE_TO_ACPI_PPI_INTERRUPT_OFFSET;
      }

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
    }
  }

  DeviceInfo->Uid++;
  InsertTailList (ListHead, &DeviceListEntry->Link);

Exit:
  if (EFI_ERROR (Status)) {
    if (DeviceListEntry->AcpiDevice.MemoryRangeArray != NULL) {
      FreePool (DeviceListEntry->AcpiDevice.MemoryRangeArray);
    }

    if (DeviceListEntry->AcpiDevice.InterruptArray != NULL) {
      FreePool (DeviceListEntry->AcpiDevice.InterruptArray);
    }

    FreePool (DeviceListEntry);
  }

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
  UINT32      DeviceTypeIndex;
  UINT32      DeviceIndex;
  UINT32      NumberOfNodes;
  UINT32      *NodeHandles;

  for (DeviceTypeIndex = 0; DeviceTypeIndex < ARRAY_SIZE (AcpiTableInfo); DeviceTypeIndex++) {
    NumberOfNodes = 0;
    Status        = GetMatchingEnabledDeviceTreeNodes (AcpiTableInfo[DeviceTypeIndex].CompatibleId, NULL, &NumberOfNodes);
    if (Status != EFI_BUFFER_TOO_SMALL) {
      continue;
    }

    NodeHandles = (UINT32 *)AllocatePool (NumberOfNodes * sizeof (UINT32));
    if (NodeHandles == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate node handles\r\n", __FUNCTION__));
      ASSERT (FALSE);
      continue;
    }

    Status = GetMatchingEnabledDeviceTreeNodes (AcpiTableInfo[DeviceTypeIndex].CompatibleId, NodeHandles, &NumberOfNodes);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get node handles - %r\r\n", __FUNCTION__, Status));
      FreePool (NodeHandles);
      continue;
    }

    for (DeviceIndex = 0; DeviceIndex < NumberOfNodes; DeviceIndex++) {
      Status = AddDeviceObjectList (ListHead, NodeHandles[DeviceIndex], &AcpiTableInfo[DeviceTypeIndex]);
      if (!EFI_ERROR (Status)) {
        break;
      }
    }
  }

  if (!EFI_ERROR (Status)) {
    if (IsListEmpty (ListHead)) {
      Status = EFI_NOT_FOUND;
    }
  }

  return Status;
}

/**
  Initialize the SSDT DTB Generation Driver.

  @param[in]  ImageHandle   of the loaded driver
  @param[in]  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Initialization succeeded.
  @retval EFI_OUT_OF_RESOURCES  Not enough memory for initialization.
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

  Status = BuildDeviceList (&gDeviceList);
  if (EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  mAcpiNotificationEvent = EfiCreateProtocolNotifyEvent (
                             &gEfiAcpiTableProtocolGuid,
                             TPL_CALLBACK,
                             AcpiProtocolReady,
                             (VOID *)&gDeviceList,
                             &AcpiNotificationRegistration
                             );
  if (mAcpiNotificationEvent == NULL) {
    return EFI_DEVICE_ERROR;
  } else {
    return EFI_SUCCESS;
  }
}
