/** @file
  Configuration Manager Data of SMBIOS Type 9 table.

  SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IpmiBaseLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/FruLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>

#include <IndustryStandard/Ipmi.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/PciIo.h>
#include <IndustryStandard/Pci22.h>

#include "SmbiosParserPrivate.h"
#include "SmbiosType9Parser.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType9 = {
  SMBIOS_TYPE_SYSTEM_SLOTS,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType09),
  NULL
};

/**
  Install CM object for SMBIOS Type 9
  @param [in]  ParserHandle A handle to the parser instance.
  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
EFIAPI
InstallSmbiosType9Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  VOID                         *DtbBase = Private->DtbBase;
  CM_SMBIOS_SYSTEM_SLOTS_INFO  *SystemSlotInfo;
  UINT32                       NumSystemSlots;
  CONST VOID                   *Property;
  CONST CHAR8                  *PropertyStr;
  INT32                        Length;
  UINT8                        Index;
  UINT8                        Index2;
  INT32                        NodeOffset;
  CHAR8                        Type9tNodeStr[] = "/firmware/smbios/type9@xx";
  EFI_STATUS                   Status;
  EFI_PCI_IO_PROTOCOL          *PciIo;
  UINTN                        HandleCount;
  EFI_HANDLE                   *HandleBuf;
  UINTN                        Segment;
  UINTN                        Bus;
  UINTN                        Device;
  UINTN                        Function;
  UINT32                       VendorDeviceId;
  VOID                         *Hob;
  UINT32                       SocketMask;
  UINT32                       SocketNum;
  UINT8                        PciClass;
  BOOLEAN                      IsPciClassPatternMatch;
  CM_OBJ_DESCRIPTOR            Desc;
  UINTN                        PciSlotAssociationIndex;
  PCI_SLOT_ASSOCIATION         PciSlotAssociation[] = {
    { PCI_CLASS_MASS_STORAGE, "NVMe" }
  };
  CM_OBJECT_TOKEN              *TokenMap;

  TokenMap       = NULL;
  SystemSlotInfo = NULL;
  HandleBuf      = NULL;
  Hob            = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);

  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    SocketMask = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->SocketMask;
  } else {
    ASSERT (FALSE);
    SocketMask = 0x1;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuf
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: locate EFI_REGULAR_EXPRESSION_PROTOCOL failure: %r\n", __FUNCTION__, Status));
    HandleCount = 0;
  }

  DEBUG ((DEBUG_INFO, "%a: PCIIO HandleCount = %u\n", __FUNCTION__, HandleCount));

  NumSystemSlots          = 0;
  IsPciClassPatternMatch  = FALSE;
  PciSlotAssociationIndex = 0;

  for (Index = 0; Index < 100; Index++) {
    AsciiSPrint (Type9tNodeStr, sizeof (Type9tNodeStr), "/firmware/smbios/type9@%u", Index);
    NodeOffset = fdt_path_offset (DtbBase, Type9tNodeStr);
    if (NodeOffset < 0) {
      continue;
    }

    //
    // Evaluate 'condition' of each Type 9 node and skip it if condition is not met
    //
    Status = EvaluateDtbNodeCondition (Private, NodeOffset);
    ASSERT (Status != EFI_INVALID_PARAMETER);
    if (Status == EFI_UNSUPPORTED) {
      continue;
    }

    SystemSlotInfo = ReallocatePool (
                       sizeof (CM_SMBIOS_SYSTEM_SLOTS_INFO) * (NumSystemSlots),
                       sizeof (CM_SMBIOS_SYSTEM_SLOTS_INFO) * (NumSystemSlots + 1),
                       SystemSlotInfo
                       );
    if (SystemSlotInfo == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: ReallocatePool failed\n", __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
      goto CleanupAndReturn;
    }

    PropertyStr = fdt_getprop (DtbBase, NodeOffset, "slot-designation", &Length);
    if (PropertyStr != NULL) {
      SystemSlotInfo[NumSystemSlots].SlotDesignation = AllocateZeroPool (Length + 1);
      if (SystemSlotInfo[NumSystemSlots].SlotDesignation != NULL) {
        AsciiSPrint (SystemSlotInfo[NumSystemSlots].SlotDesignation, Length + 1, PropertyStr);
      }
    } else {
      SystemSlotInfo[NumSystemSlots].SlotDesignation = NULL;
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "slot-type", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].SlotType = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "slot-data-bus-width", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].SlotDataBusWidth = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "slot-length", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].SlotLength = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "slot-id", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].SlotID = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "slot-characteristics1", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].SlotCharacteristics1 = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "slot-characteristics2", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].SlotCharacteristics2 = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "segment-group-number", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].SegmentGroupNum = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "bus-number", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].BusNum = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "device-function-number", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].DevFuncNum = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "data-bus-width", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].DataBusWidth = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "peer-grouping-count", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].PeerGroupingCount = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "slot-information", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].SlotInformation = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "slot-physical-width", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].SlotPhysicalWidth = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "slot-pitch", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].SlotPitch = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "slot-height", &Length);
    if (Property != NULL) {
      SystemSlotInfo[NumSystemSlots].SlotHeight = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    SocketNum = (SystemSlotInfo[NumSystemSlots].SegmentGroupNum) >> 4;
    if (SocketMask & (0x01 << SocketNum)) {
      SystemSlotInfo[NumSystemSlots].CurrentUsage = SlotUsageAvailable;
    } else {
      SystemSlotInfo[NumSystemSlots].CurrentUsage = SlotUsageUnavailable;
    }

    for (Index2 = 0; Index2 < HandleCount; Index2++) {
      Status = gBS->HandleProtocol (
                      HandleBuf[Index2],
                      &gEfiPciIoProtocolGuid,
                      (VOID **)&PciIo
                      );
      if (!EFI_ERROR (Status)) {
        Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
        if (!EFI_ERROR (Status) &&
            (SystemSlotInfo[NumSystemSlots].SegmentGroupNum == Segment) &&
            (SystemSlotInfo[NumSystemSlots].BusNum == Bus) &&
            (SystemSlotInfo[NumSystemSlots].DevFuncNum == (UINT8)((Device << 3) | Function)))
        {
          IsPciClassPatternMatch = FALSE;

          for (PciSlotAssociationIndex = 0;
               (PciSlotAssociationIndex < ARRAY_SIZE (PciSlotAssociation));
               PciSlotAssociationIndex++)
          {
            if (AsciiStrStr (
                  SystemSlotInfo[NumSystemSlots].SlotDesignation,
                  PciSlotAssociation[PciSlotAssociationIndex].SlotDescription
                  ) != NULL)
            {
              IsPciClassPatternMatch = TRUE;
              break;
            }
          }

          PciClass = 0;
          Status   = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, 0x00, 1, &VendorDeviceId);

          if (!EFI_ERROR (Status) && (VendorDeviceId != 0xFFFFFFFF)) {
            if (IsPciClassPatternMatch) {
              Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint8, 0x0B, 1, &PciClass);
              if (!EFI_ERROR (Status) && (PciClass == PciSlotAssociation[PciSlotAssociationIndex].PciClass)) {
                SystemSlotInfo[NumSystemSlots].CurrentUsage = SlotUsageInUse;
              }
            } else {
              SystemSlotInfo[NumSystemSlots].CurrentUsage = SlotUsageInUse;
            }
          }
        }
      }
    }

    NumSystemSlots++;
  }

  DEBUG ((DEBUG_INFO, "%a: NumSystemSlots = %u\n", __FUNCTION__, NumSystemSlots));

  if (NumSystemSlots == 0) {
    Status = EFI_NOT_FOUND;
    goto CleanupAndReturn;
  }

  if (SystemSlotInfo == NULL) {
    Status = EFI_NOT_FOUND;
    goto CleanupAndReturn;
  }

  // Allocate Token Map
  Status = NvAllocateCmTokens (ParserHandle, NumSystemSlots, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 9: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < NumSystemSlots; Index++) {
    SystemSlotInfo[Index].SystemSlotInfoToken =   TokenMap[Index];
  }

  //
  // Install CM object for type 9
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjSystemSlotInfo);
  Desc.Size     = NumSystemSlots * sizeof (CM_SMBIOS_SYSTEM_SLOTS_INFO);
  Desc.Count    = NumSystemSlots;
  Desc.Data     = SystemSlotInfo;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 9 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  //
  // Add type 9 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType9,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

CleanupAndReturn:
  FREE_NON_NULL (TokenMap);
  FREE_NON_NULL (SystemSlotInfo);
  FREE_NON_NULL (HandleBuf);
  return Status;
}
