/** @file
  Ahci info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "AhciInfoParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/UefiBootServicesTableLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/PcdLib.h>
#include <Library/NVIDIADebugLib.h>

#include <Protocol/PciRootBridgeIo.h>

// #include "SsdtAhci.hex"
extern unsigned char  ssdtahci_aml_code[];

// Callback for AHCI controller connection
EFI_EVENT   EndOfDxeEvent;
EFI_HANDLE  PciControllerHandle = 0;

STATIC
BOOLEAN
EFIAPI
IsAGXXavier (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT32      NumberOfPlatformNodes;

  NumberOfPlatformNodes = 0;
  Status                = GetMatchingEnabledDeviceTreeNodes ("nvidia,p2972-0000", NULL, &NumberOfPlatformNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  NumberOfPlatformNodes = 0;
  Status                = GetMatchingEnabledDeviceTreeNodes ("nvidia,galen", NULL, &NumberOfPlatformNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  return FALSE;
}

// Callback to connect PCIe controller as this is needed if exposed as direct ACPI node and we didn't boot off it
STATIC
VOID
EFIAPI
OnEndOfDxe (
  EFI_EVENT  Event,
  VOID       *Context
  )
{
  gBS->ConnectController (PciControllerHandle, NULL, NULL, TRUE);
}

/** AHCI info parser function.

  The ACPI table is extend with a SSDT table containing the AHCI info

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
AhciInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                       Status;
  UINT32                           Index;
  CM_STD_OBJ_ACPI_TABLE_INFO       AcpiTableHeader;
  UINTN                            NumOfHandles;
  EFI_HANDLE                       *HandleBuffer;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  *RootBridgeIo;
  BOOLEAN                          PciControllerConnected = FALSE;

  HandleBuffer = NULL;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  if (!IsAGXXavier ()) {
    DEBUG ((DEBUG_INFO, "AHCI support not present on this platform\r\n"));
    Status = EFI_SUCCESS;
    goto CleanupAndReturn;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciRootBridgeIoProtocolGuid,
                  NULL,
                  &NumOfHandles,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status) || (NumOfHandles == 0)) {
    DEBUG ((DEBUG_ERROR, "%a, Failed to LocateHandleBuffer %r\r\n", __FUNCTION__, Status));
    Status = EFI_SUCCESS;
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < NumOfHandles; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiPciRootBridgeIoProtocolGuid,
                    (VOID **)&RootBridgeIo
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, Failed to handle protocol %r\r\n", __FUNCTION__, Status));
      continue;
    }

    if (RootBridgeIo->SegmentNumber == AHCI_PCIE_SEGMENT) {
      PciControllerHandle = HandleBuffer[Index];
      Status              = gBS->CreateEventEx (
                                   EVT_NOTIFY_SIGNAL,
                                   TPL_CALLBACK,
                                   OnEndOfDxe,
                                   NULL,
                                   &gEfiEndOfDxeEventGroupGuid,
                                   &EndOfDxeEvent
                                   );
      ASSERT_EFI_ERROR (Status);
      PciControllerConnected = TRUE;
      break;
    }
  }

  if (!PciControllerConnected) {
    Status = EFI_SUCCESS;
    goto CleanupAndReturn;
  }

  // Extend ACPI table list with the new table header
  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
  AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
  AcpiTableHeader.AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)ssdtahci_aml_code;
  AcpiTableHeader.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  AcpiTableHeader.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader.MinorRevision      = 0;

  Status = NvAddAcpiTableGenerator (ParserHandle, &AcpiTableHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add the Ahci SSDT table\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  FREE_NON_NULL (HandleBuffer);
  return Status;
}

REGISTER_PARSER_FUNCTION (AhciInfoParser, NULL)
