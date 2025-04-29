/** @file
  Parser to create network based pci devices for DBG2 creation.

  SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "Dbg2NetworkParser.h"
#include "../ConfigurationManagerDataRepoLib.h"
#include "Library/NvCmObjectDescUtility.h"
#include <Base.h>
#include <Library/BaseMemoryLib.h>
#include <Uefi/UefiBaseType.h>

#include <IndustryStandard/DebugPort2Table.h>
#include <IndustryStandard/Pci22.h>

#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Protocol/PciIo.h>

EFI_STATUS
EFIAPI
Dbg2NetworkParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                              Status;
  UINT32                                  Data;
  UINTN                                   DataSize;
  EFI_HANDLE                              *HandleBuffer = NULL;
  UINTN                                   NumberOfHandles;
  UINTN                                   Index;
  UINT32                                  BarIndex;
  EFI_PCI_IO_PROTOCOL                     *PciIo;
  PCI_TYPE00                              PciData;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR       *MmioDesc;
  UINTN                                   SegmentNumber;
  UINTN                                   BusNumber;
  UINTN                                   DeviceNumber;
  UINTN                                   FunctionNumber;
  CM_OBJ_DESCRIPTOR                       *Dbg2CmObjDesc;
  CM_ARCH_COMMON_DBG2_DEVICE_INFO         Dbg2DeviceInfo;
  CM_STD_OBJ_ACPI_TABLE_INFO              *AcpiTableHeader;
  CM_ARCH_COMMON_MEMORY_RANGE_DESCRIPTOR  MemoryRanges[PCI_MAX_BAR];
  UINTN                                   MemoryRangeCount;
  CM_OBJ_DESCRIPTOR                       *MemoryRangeCmObjDesc;

  Dbg2CmObjDesc    = NULL;
  MemoryRangeCount = 0;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  AcpiTableHeader = AllocatePool (sizeof (CM_STD_OBJ_ACPI_TABLE_INFO));
  if (AcpiTableHeader == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate memory for AcpiTableHeader\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = NvCreateCmObjDesc (
             CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjGenericDbg2DeviceInfo),
             1,
             &Dbg2DeviceInfo,
             sizeof (Dbg2DeviceInfo),
             &Dbg2CmObjDesc
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to create CM object descriptor for Dbg2DeviceInfo\n"));
    goto CleanupAndReturn;
  }

  ZeroMem (&Dbg2DeviceInfo, sizeof (Dbg2DeviceInfo));

  DataSize = sizeof (Data);
  Status   = gRT->GetVariable (
                    L"Dbg2NetworkDevice",
                    &gNVIDIAPublicVariableGuid,
                    NULL,
                    &DataSize,
                    &Data
                    );

  if (EFI_ERROR (Status) || (Data == MAX_UINT32)) {
    Status = EFI_NOT_FOUND;
    goto CleanupAndReturn;
  }

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiPciIoProtocolGuid, NULL, &NumberOfHandles, &HandleBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to locate PCI handles - %r\n", Status));
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < NumberOfHandles; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = PciIo->GetLocation (PciIo, &SegmentNumber, &BusNumber, &DeviceNumber, &FunctionNumber);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (Data != (SegmentNumber << 24 | BusNumber << 16 | DeviceNumber << 8 | FunctionNumber)) {
      continue;
    }

    Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint8, 0, sizeof (PciData), &PciData);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Dbg2DeviceInfo.PortType    = EFI_ACPI_DBG2_PORT_TYPE_NET;
    Dbg2DeviceInfo.PortSubtype = PciData.Hdr.VendorId;
    Dbg2DeviceInfo.AccessSize  = EFI_ACPI_6_4_DWORD;
    MemoryRangeCount           = 0;

    for (BarIndex = 0; BarIndex < PCI_MAX_BAR; BarIndex++) {
      Status = PciIo->GetBarAttributes (PciIo, BarIndex, NULL, (VOID **)&MmioDesc);
      if (EFI_ERROR (Status) ||
          (MmioDesc->ResType != ACPI_ADDRESS_SPACE_TYPE_MEM))
      {
        continue;
      }

      if (MmioDesc->AddrTranslationOffset != 0) {
        MemoryRangeCount = 0;
        DEBUG ((DEBUG_ERROR, "Dbg2: Address Translation Offset is not supported\n"));
        break;
      }

      MemoryRanges[MemoryRangeCount].BaseAddress = MmioDesc->AddrRangeMin;
      MemoryRanges[MemoryRangeCount].Length      = MmioDesc->AddrLen;
      MemoryRangeCount++;
    }

    break;
  }

  if (MemoryRangeCount == 0) {
    Status = EFI_NOT_FOUND;
    DEBUG ((DEBUG_ERROR, "Failed to find a valid PCI device for Dbg2\n"));
    goto CleanupAndReturn;
  }

  Status = NvCreateCmObjDesc (
             CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjMemoryRangeDescriptor),
             MemoryRangeCount,
             &MemoryRanges,
             sizeof (CM_ARCH_COMMON_MEMORY_RANGE_DESCRIPTOR) * MemoryRangeCount,
             &MemoryRangeCmObjDesc
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to create CM object descriptor for MemoryRanges\n"));
    goto CleanupAndReturn;
  }

  Status = NvAddMultipleCmObjGetTokens (
             ParserHandle,
             MemoryRangeCmObjDesc,
             NULL,
             &Dbg2DeviceInfo.AddressResourceToken
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to add MemoryRanges to ConfigManager: %r\n", Status));
    goto CleanupAndReturn;
  }

  Status = NvExtendCmObj (ParserHandle, Dbg2CmObjDesc, CM_NULL_TOKEN, NULL);
  if (Status == EFI_NOT_FOUND) {
    Status = NvAddMultipleCmObjGetTokens (ParserHandle, Dbg2CmObjDesc, NULL, NULL);
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to add Dbg2DeviceInfo to ConfigManager: %r\n", Status));
    goto CleanupAndReturn;
  }

  AcpiTableHeader->AcpiTableSignature = EFI_ACPI_6_4_DEBUG_PORT_2_TABLE_SIGNATURE;
  AcpiTableHeader->AcpiTableRevision  = EFI_ACPI_DEBUG_PORT_2_TABLE_REVISION;
  AcpiTableHeader->TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDbg2);
  AcpiTableHeader->AcpiTableData      = NULL;
  AcpiTableHeader->OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  AcpiTableHeader->OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader->MinorRevision      = 0;

  Status = NvAddAcpiTableGenerator (ParserHandle, AcpiTableHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to add Dbg2 to ConfigManager: %r\n", Status));
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  if (EFI_ERROR (Status)) {
    FREE_NON_NULL (AcpiTableHeader);
  }

  FREE_NON_NULL (Dbg2CmObjDesc);
  FREE_NON_NULL (HandleBuffer);
  return Status;
}

REGISTER_PARSER_FUNCTION (Dbg2NetworkParser, "skip-dbg2-table")
