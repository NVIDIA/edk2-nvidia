/** @file

  This driver implements and installs the IOMMU protocol

  Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/PciIo.h>
#include <Library/SmmuLib.h>
#include <Protocol/SmmuV3Protocol.h>

typedef struct {
  NVIDIA_SMMUV3_CONTROLLER_PROTOCOL    *SmmuV3CtlrProtocolInterface;
  UINT32                               SmmuV3pHandle;
} SMMUV3_PROTOCOL_INFO;

SMMUV3_PROTOCOL_INFO  *SmmuV3ProtocolInfo = NULL;
UINTN                 NumSmmus            = 0;

LIST_ENTRY  gMaps = INITIALIZE_LIST_HEAD_VARIABLE (gMaps);

EFI_STATUS
EFIAPI
IoMmuAllocateBuffer (
  IN     EDKII_IOMMU_PROTOCOL  *This,
  IN     EFI_ALLOCATE_TYPE     Type,
  IN     EFI_MEMORY_TYPE       MemoryType,
  IN     UINTN                 Pages,
  IN OUT VOID                  **HostAddress,
  IN     UINT64                Attributes
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  PhysicalAddress;

  DEBUG ((DEBUG_INFO, "IoMmuAllocateBuffer: ==> 0x%08x\n", Pages));

  // Validate Attributes
  if ((Attributes & EDKII_IOMMU_ATTRIBUTE_INVALID_FOR_ALLOCATE_BUFFER) != 0) {
    DEBUG ((DEBUG_ERROR, "IoMmuAllocateBuffer: %r\n", EFI_UNSUPPORTED));
    return EFI_UNSUPPORTED;
  }

  // Check for invalid inputs
  if (HostAddress == NULL) {
    DEBUG ((DEBUG_ERROR, "IoMmuAllocateBuffer: %r\n", EFI_INVALID_PARAMETER));
    return EFI_INVALID_PARAMETER;
  }

  // The only valid memory types are EfiBootServicesData and
  // EfiRuntimeServicesData
  if ((MemoryType != EfiBootServicesData) &&
      (MemoryType != EfiRuntimeServicesData))
  {
    DEBUG ((DEBUG_ERROR, "IoMmuAllocateBuffer: %r\n", EFI_INVALID_PARAMETER));
    return EFI_INVALID_PARAMETER;
  }

  PhysicalAddress = DMA_MEMORY_TOP;
  if ((Attributes & EDKII_IOMMU_ATTRIBUTE_DUAL_ADDRESS_CYCLE) == 0) {
    // Limit allocations to memory below 4GB
    PhysicalAddress = MIN (PhysicalAddress, SIZE_4GB - 1);
  }

  Status = gBS->AllocatePages (
                  AllocateMaxAddress,
                  MemoryType,
                  Pages,
                  &PhysicalAddress
                  );
  if (!EFI_ERROR (Status)) {
    *HostAddress = (VOID *)(UINTN)PhysicalAddress;
  } else {
    DEBUG ((DEBUG_ERROR, "IoMmuAllocateBuffer failed with %r\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "IoMmuAllocateBuffer: 0x%08x <==\n", *HostAddress));

  return Status;
}

EFI_STATUS
EFIAPI
IoMmuFreeBuffer (
  IN  EDKII_IOMMU_PROTOCOL  *This,
  IN  UINTN                 Pages,
  IN  VOID                  *HostAddress
  )
{
  DEBUG ((DEBUG_INFO, "IoMmuFreeBuffer: 0x%x\n", Pages));
  return gBS->FreePages ((EFI_PHYSICAL_ADDRESS)(UINTN)HostAddress, Pages);
}

STATIC
EFI_STATUS
EFIAPI
IoMmuSetAttribute (
  IN EDKII_IOMMU_PROTOCOL  *This,
  IN EFI_HANDLE            DeviceHandle,
  IN VOID                  *Mapping,
  IN UINT64                IoMmuAccess
  )
{
  EFI_STATUS  Status;
  SOURCE_ID   *SourceId;
  UINTN       Index;

  if (SmmuV3ProtocolInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a SmmuV3ProtocolInfo is NULL. Exiting \n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  SourceId = (SOURCE_ID *)AllocateZeroPool (sizeof (SOURCE_ID));
  if (SourceId == NULL) {
    DEBUG ((DEBUG_ERROR, "%a SourceId could not be allocated: %r\n", __FUNCTION__, EFI_OUT_OF_RESOURCES));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  Status = GetSourceIdFromPciHandle (DeviceHandle, SourceId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Could not get Source Id from PCI Handle %r ", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < NumSmmus; Index++) {
    if (SmmuV3ProtocolInfo[Index].SmmuV3pHandle == SourceId->SmmuV3pHandle) {
      if (SmmuV3ProtocolInfo[Index].SmmuV3CtlrProtocolInterface == NULL) {
        DEBUG ((DEBUG_ERROR, "%a SmmuV3ProtocolInfo instance is NULL. Exiting \n", __FUNCTION__));
        Status = EFI_NOT_FOUND;
        goto CleanupAndReturn;
      }

      DEBUG ((
        DEBUG_INFO,
        "%a Calling SetAttribute for SmmuV3ProtocolInfo->SmmuV3pHandle = 0x%X and StreamID = 0x%X \n",
        __FUNCTION__,
        SmmuV3ProtocolInfo[Index].SmmuV3pHandle,
        SourceId->StreamId
        ));
      Status = SmmuV3ProtocolInfo[Index].SmmuV3CtlrProtocolInterface->SetAttribute (SmmuV3ProtocolInfo[Index].SmmuV3CtlrProtocolInterface, Mapping, IoMmuAccess, SourceId->StreamId);
      break;
    }
  }

CleanupAndReturn:

  if (SourceId != NULL) {
    FreePool (SourceId);
    SourceId = NULL;
  }

  return Status;
}

EFI_STATUS
EFIAPI
IoMmuMap (
  IN EDKII_IOMMU_PROTOCOL       *This,
  IN     EDKII_IOMMU_OPERATION  Operation,
  IN     VOID                   *HostAddress,
  IN OUT UINTN                  *NumberOfBytes,
  OUT    EFI_PHYSICAL_ADDRESS   *DeviceAddress,
  OUT    VOID                   **Mapping
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  PhysicalAddress;
  MAP_INFO              *MapInfo;
  EFI_PHYSICAL_ADDRESS  DmaMemoryTop;
  BOOLEAN               NeedRemap;

  if ((HostAddress == NULL) ||
      (NumberOfBytes == NULL) ||
      (DeviceAddress == NULL) ||
      (Mapping == NULL))
  {
    Status = EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "IoMmuMap: ==> 0x%X - 0x%X (%x)\n", (UINTN)HostAddress, *NumberOfBytes, Operation));

  // Make sure that Operation is valid
  if ((UINT32)Operation >= EdkiiIoMmuOperationMaximum) {
    DEBUG ((DEBUG_ERROR, "IoMmuMap: %r\n", EFI_INVALID_PARAMETER));
    return EFI_INVALID_PARAMETER;
  }

  NeedRemap       = FALSE;
  PhysicalAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)HostAddress;

  DmaMemoryTop = DMA_MEMORY_TOP;

  // Alignment check
  if ((*NumberOfBytes != ALIGN_VALUE (*NumberOfBytes, SIZE_4KB)) ||
      (PhysicalAddress != ALIGN_VALUE (PhysicalAddress, SIZE_4KB)))
  {
    if ((Operation == EdkiiIoMmuOperationBusMasterCommonBuffer) ||
        (Operation == EdkiiIoMmuOperationBusMasterCommonBuffer64))
    {
      // The input buffer might be a subset from IoMmuAllocateBuffer.
      // Skip the check.
    } else {
      NeedRemap = TRUE;
    }
  }

  if ((PhysicalAddress + *NumberOfBytes) >= DMA_MEMORY_TOP) {
    NeedRemap = TRUE;
  }

  if ((((Operation != EdkiiIoMmuOperationBusMasterRead64) &&
        (Operation != EdkiiIoMmuOperationBusMasterWrite64) &&
        (Operation != EdkiiIoMmuOperationBusMasterCommonBuffer64))) &&
      ((PhysicalAddress + *NumberOfBytes) > SIZE_4GB))
  {
    // If the root bridge or the device cannot handle performing DMA above
    // 4GB but any part of the DMA transfer being mapped is above 4GB, then
    // map the DMA transfer to a buffer below 4GB.
    NeedRemap    = TRUE;
    DmaMemoryTop = MIN (DmaMemoryTop, SIZE_4GB - 1);
  }

  if ((Operation == EdkiiIoMmuOperationBusMasterCommonBuffer) ||
      (Operation == EdkiiIoMmuOperationBusMasterCommonBuffer64))
  {
    if (NeedRemap) {
      // Common Buffer operations can not be remapped.  If the common buffer
      // is above 4GB, then it is not possible to generate a mapping, so return
      // an error.
      DEBUG ((DEBUG_ERROR, "IoMmuMap: %r\n", EFI_UNSUPPORTED));
      return EFI_UNSUPPORTED;
    }
  }

  // Allocate a MAP_INFO structure to remember the mapping when Unmap() is
  // called later.
  MapInfo = AllocateZeroPool (sizeof (MAP_INFO));
  if (MapInfo == NULL) {
    *NumberOfBytes = 0;
    DEBUG ((DEBUG_ERROR, "IoMmuMap: %r\n", EFI_OUT_OF_RESOURCES));
    return EFI_OUT_OF_RESOURCES;
  }

  InitializeListHead (&MapInfo->Link);

  // Initialize the MAP_INFO structure
  MapInfo->Signature     = MAP_INFO_SIGNATURE;
  MapInfo->Operation     = Operation;
  MapInfo->NumberOfBytes = *NumberOfBytes;
  MapInfo->NumberOfPages = EFI_SIZE_TO_PAGES (MapInfo->NumberOfBytes);
  MapInfo->HostAddress   = PhysicalAddress;
  MapInfo->DeviceAddress = DmaMemoryTop;

  // Allocate a buffer below 4GB to map the transfer to.
  if (NeedRemap) {
    Status = gBS->AllocatePages (
                    AllocateMaxAddress,
                    EfiBootServicesData,
                    MapInfo->NumberOfPages,
                    &MapInfo->DeviceAddress
                    );
    if (EFI_ERROR (Status)) {
      FreePool (MapInfo);
      *NumberOfBytes = 0;
      DEBUG ((DEBUG_INFO, "IoMmuMap: %r\n", Status));
      return Status;
    }

    // If this is a read operation from the Bus Master's point of view,
    // then copy the contents of the real buffer into the mapped buffer
    // so the Bus Master can read the contents of the real buffer.
    if ((Operation == EdkiiIoMmuOperationBusMasterRead) ||
        (Operation == EdkiiIoMmuOperationBusMasterRead64))
    {
      CopyMem (
        (VOID *)(UINTN)MapInfo->DeviceAddress,
        (VOID *)(UINTN)MapInfo->HostAddress,
        MapInfo->NumberOfBytes
        );
    }
  } else {
    MapInfo->DeviceAddress = MapInfo->HostAddress;
  }

  InsertHeadList (&gMaps, &MapInfo->Link);

  // The DeviceAddress is the address of the maped buffer below 4GB
  *DeviceAddress = MapInfo->DeviceAddress;

  // Return a pointer to the MAP_INFO structure in Mapping
  *Mapping = MapInfo;

  DEBUG ((DEBUG_INFO, "IoMmuMap: 0x%08x - 0x%08x <==\n", *DeviceAddress, *Mapping));

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
IoMmuUnmap (
  IN  EDKII_IOMMU_PROTOCOL  *This,
  IN  VOID                  *Mapping
  )
{
  MAP_INFO    *MapInfo;
  LIST_ENTRY  *Link;

  DEBUG ((DEBUG_INFO, "IoMmuUnmap: 0x%X\n", (UINTN)Mapping));

  if (Mapping == NULL) {
    DEBUG ((DEBUG_ERROR, "IoMmuUnmap: %r\n", EFI_INVALID_PARAMETER));
    return EFI_INVALID_PARAMETER;
  }

  MapInfo = NULL;
  for (Link = GetFirstNode (&gMaps)
       ; Link != &gMaps
       ; Link = GetNextNode (&gMaps, Link)
       )
  {
    MapInfo = MAP_INFO_FROM_LINK (Link);
    if (MapInfo == Mapping) {
      break;
    }
  }

  // Mapping is not a valid value returned by Map()
  if (MapInfo != Mapping) {
    DEBUG ((DEBUG_ERROR, "IoMmuUnmap: %r\n", EFI_INVALID_PARAMETER));
    return EFI_INVALID_PARAMETER;
  }

  RemoveEntryList (&MapInfo->Link);

  if (MapInfo->DeviceAddress != MapInfo->HostAddress) {
    // If this is a write operation from the Bus Master's point of view,
    // then copy the contents of the mapped buffer into the real buffer
    // so the processor can read the contents of the real buffer.
    if ((MapInfo->Operation == EdkiiIoMmuOperationBusMasterWrite) ||
        (MapInfo->Operation == EdkiiIoMmuOperationBusMasterWrite64))
    {
      CopyMem (
        (VOID *)(UINTN)MapInfo->HostAddress,
        (VOID *)(UINTN)MapInfo->DeviceAddress,
        MapInfo->NumberOfBytes
        );
    }

    // Free the mapped buffer and the MAP_INFO structure.
    gBS->FreePages (MapInfo->DeviceAddress, MapInfo->NumberOfPages);
  }

  FreePool (Mapping);
  return EFI_SUCCESS;
}

EDKII_IOMMU_PROTOCOL  mIoMmuProtocol = {
  EDKII_IOMMU_PROTOCOL_REVISION,
  IoMmuSetAttribute,
  IoMmuMap,
  IoMmuUnmap,
  IoMmuAllocateBuffer,
  IoMmuFreeBuffer,
};

EFI_STATUS
EFIAPI
SmmuDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *HandleBuffer;
  UINTN       Count;

  // Locate handle buffers for all instances of NvidiaSMMUV3 protocol installed
  HandleBuffer = NULL;
  NumSmmus     = 0;
  Status       = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gNVIDIASmmuV3ProtocolGuid,
                        NULL,
                        &NumSmmus,
                        &HandleBuffer
                        );
  if (EFI_ERROR (Status) || (NumSmmus == 0) || (HandleBuffer == NULL)) {
    Status = EFI_UNSUPPORTED;
    goto ErrorExit;
  }

  SmmuV3ProtocolInfo = (SMMUV3_PROTOCOL_INFO *)AllocateZeroPool (sizeof (SMMUV3_PROTOCOL_INFO) * NumSmmus);
  if (SmmuV3ProtocolInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "Could not allocate SmmuV3ProtocolInfo: %r\n", EFI_OUT_OF_RESOURCES));
    goto ErrorExit;
  }

  // For all handles, save the phandle of the SMMUv3 protocol and the SMMUv3 Protocol's interface itself
  for (Count = 0; Count < NumSmmus; Count++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Count],
                    &gNVIDIASmmuV3ProtocolGuid,
                    (VOID **)&SmmuV3ProtocolInfo[Count].SmmuV3CtlrProtocolInterface
                    );
    if (EFI_ERROR (Status)) {
      goto ErrorExit;
    }

    SmmuV3ProtocolInfo[Count].SmmuV3pHandle = SmmuV3ProtocolInfo[Count].SmmuV3CtlrProtocolInterface->PHandle;
  }

  // Install the IOMMU protocol GUID
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEdkiiIoMmuProtocolGuid,
                  &mIoMmuProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error installing gEdkiiIoMmuProtocolGuid\n", __FUNCTION__));
    goto ErrorExit;
  }

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (SmmuV3ProtocolInfo != NULL) {
      FreePool (SmmuV3ProtocolInfo);
      SmmuV3ProtocolInfo = NULL;
    }

    if (HandleBuffer != NULL) {
      FreePool (HandleBuffer);
    }
  }

  return Status;
}
