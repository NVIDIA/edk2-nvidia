/** @file
  SSDT table generator parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "SsdtTableGeneratorParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/ConfigurationManagerDataLib.h>
#include <Library/NVIDIADebugLib.h>

/** SSDT table generator parser function.

  The SSDT table generator creates and adds the generated tables

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
SsdtTableGeneratorParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                      Status;
  CM_STD_OBJ_ACPI_TABLE_INFO      AcpiTableHeader;
  EFI_ACPI_DESCRIPTION_HEADER     *TestTable;
  NVIDIA_AML_GENERATION_PROTOCOL  *GenerationProtocol;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  Status = NvGetCmGenerationProtocol (ParserHandle, &GenerationProtocol);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  // Only create & install the table if there's relevant data inside
  if (GenerationProtocol->DeviceCount > 0) {
    Status = GenerationProtocol->EndScope (GenerationProtocol);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    Status = GenerationProtocol->GetTable (GenerationProtocol, &TestTable);
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    // Extend ACPI table list with the new table header
    AcpiTableHeader.AcpiTableSignature = TestTable->Signature;
    AcpiTableHeader.AcpiTableRevision  = TestTable->Revision;
    AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
    AcpiTableHeader.AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)TestTable;
    AcpiTableHeader.OemTableId         = TestTable->OemTableId;
    AcpiTableHeader.OemRevision        = TestTable->OemRevision;
    AcpiTableHeader.MinorRevision      = 0;

    Status = NvAddAcpiTableGenerator (ParserHandle, &AcpiTableHeader);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to add generated SSDT ACPI table - %r\r\n", __func__, Status));
      goto CleanupAndReturn;
    }
  }

CleanupAndReturn:
  return Status;
}

REGISTER_PARSER_FUNCTION (SsdtTableGeneratorParser, NULL)
