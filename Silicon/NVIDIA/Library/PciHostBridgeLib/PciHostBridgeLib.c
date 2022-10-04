/** @file
*  PCI Host Bridge Library instance for NVIDIA platforms
*
*  Copyright (c) 2018, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2017, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiDxe.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/PciRootBridgeIo.h>

#ifndef MDEPKG_NDEBUG
STATIC CONST CHAR16  mPciHostBridgeLibAcpiAddressSpaceTypeStr[][4] = {
  L"Mem", L"I/O", L"Bus"
};
#endif

/**
  Return all the root bridge instances in an array.

  @param Count  Return the count of root bridge instances.

  @return All the root bridge instances in an array.
          The array should be passed into PciHostBridgeFreeRootBridges()
          when it's not used.
**/
PCI_ROOT_BRIDGE *
EFIAPI
PciHostBridgeGetRootBridges (
  UINTN  *Count
  )
{
  PCI_ROOT_BRIDGE  *RootBridges = NULL;
  EFI_STATUS       Status;
  UINTN            NumberOfHandles;
  EFI_HANDLE       *Handles = NULL;
  UINTN            CurrentHandle;

  if (Count == NULL) {
    return NULL;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIAPciHostBridgeProtocolGuid,
                  NULL,
                  &NumberOfHandles,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to locate host bridge protocols, %r.\r\n", __FUNCTION__, Status));
    goto Done;
  }

  RootBridges = (PCI_ROOT_BRIDGE *)AllocatePool (sizeof (PCI_ROOT_BRIDGE) * NumberOfHandles);
  if (RootBridges == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate root bridge array.\r\n", __FUNCTION__));
    goto Done;
  }

  for (CurrentHandle = 0; CurrentHandle < NumberOfHandles; CurrentHandle++) {
    PCI_ROOT_BRIDGE  *RootBridge = NULL;
    Status = gBS->HandleProtocol (
                    Handles[CurrentHandle],
                    &gNVIDIAPciHostBridgeProtocolGuid,
                    (VOID **)&RootBridge
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        EFI_D_ERROR,
        "%a: Failed to get protocol for handle %p, %r.\r\n",
        __FUNCTION__,
        Handles[CurrentHandle],
        Status
        ));
      goto Done;
    }

    CopyMem (&RootBridges[CurrentHandle], RootBridge, sizeof (PCI_ROOT_BRIDGE));
  }

Done:
  if (Handles != NULL) {
    FreePool (Handles);
    Handles = NULL;
  }

  if (EFI_ERROR (Status)) {
    NumberOfHandles = 0;
    if (RootBridges != NULL) {
      FreePool (RootBridges);
      RootBridges = NULL;
    }
  }

  *Count = NumberOfHandles;
  return RootBridges;
}

/**
  Free the root bridge instances array returned from PciHostBridgeGetRootBridges().

  @param Bridges The root bridge instances array.
  @param Count   The count of the array.
**/
VOID
EFIAPI
PciHostBridgeFreeRootBridges (
  PCI_ROOT_BRIDGE  *Bridges,
  UINTN            Count
  )
{
  FreePool (Bridges);
}

/**
  Inform the platform that the resource conflict happens.

  @param HostBridgeHandle Handle of the Host Bridge.
  @param Configuration    Pointer to PCI I/O and PCI memory resource
                          descriptors. The Configuration contains the resources
                          for all the root bridges. The resource for each root
                          bridge is terminated with END descriptor and an
                          additional END is appended indicating the end of the
                          entire resources. The resource descriptor field
                          values follow the description in
                          EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
                          .SubmitResources().
**/
VOID
EFIAPI
PciHostBridgeResourceConflict (
  EFI_HANDLE  HostBridgeHandle,
  VOID        *Configuration
  )
{
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Descriptor;
  UINTN                              RootBridgeIndex;

  DEBUG ((DEBUG_ERROR, "PciHostBridge: Resource conflict happens!\n"));

  RootBridgeIndex = 0;
  Descriptor      = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Configuration;
  while (Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR) {
    DEBUG ((DEBUG_ERROR, "RootBridge[%d]:\n", RootBridgeIndex++));
    for ( ; Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR; Descriptor++) {
      ASSERT (
        Descriptor->ResType <
        (sizeof (mPciHostBridgeLibAcpiAddressSpaceTypeStr) /
         sizeof (mPciHostBridgeLibAcpiAddressSpaceTypeStr[0])
        )
        );
      DEBUG ((
        DEBUG_ERROR,
        " %s: Length/Alignment = 0x%lx / 0x%lx\n",
        mPciHostBridgeLibAcpiAddressSpaceTypeStr[Descriptor->ResType],
        Descriptor->AddrLen,
        Descriptor->AddrRangeMax
        ));
      if (Descriptor->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM) {
        DEBUG ((
          DEBUG_ERROR,
          "     Granularity/SpecificFlag = %ld / %02x%s\n",
          Descriptor->AddrSpaceGranularity,
          Descriptor->SpecificFlag,
          ((Descriptor->SpecificFlag &
            EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE
            ) != 0) ? L" (Prefetchable)" : L""
          ));
      }
    }

    //
    // Skip the END descriptor for root bridge
    //
    ASSERT (Descriptor->Desc == ACPI_END_TAG_DESCRIPTOR);
    Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)(
                                                       (EFI_ACPI_END_TAG_DESCRIPTOR *)Descriptor + 1
                                                       );
  }
}
