/** @file
  TPM info parser.

  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "TpmInfoParser.h"
#include <Library/NVIDIADebugLib.h>
#include <Library/TpmMeasurementLib.h>
#include <Library/Tpm2CommandLib.h>
#include <IndustryStandard/UefiTcgPlatform.h>

#include "SsdtTpm_TH500.hex"

/** TPM info parser function.

  The following structure is populated:
  JDS TODO

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
TpmInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                  Status;
  UINT32                      ManufacturerID;
  CM_STD_OBJ_ACPI_TABLE_INFO  NewAcpiTable;
  CM_OBJ_DESCRIPTOR           Desc;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  if (!PcdGetBool (PcdTpmEnable)) {
    return EFI_SUCCESS;
  }

  //
  // Check if TPM is accessible
  //
  Status = Tpm2GetCapabilityManufactureID (&ManufacturerID);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: TPM is inaccessible - %r\n", __FUNCTION__, Status));
    return EFI_SUCCESS;
  }

  //
  // Measure to PCR[0] with event EV_POST_CODE ACPI DATA.
  // The measurement has to be done before any update.
  // Otherwise, the PCR record would be different after TPM FW update
  // or the PCD configuration change.
  //
  TpmMeasureAndLogData (
    0,
    EV_POST_CODE,
    EV_POSTCODE_INFO_ACPI_DATA,
    ACPI_DATA_LEN,
    ssdttpm_th500_aml_code,
    ((EFI_ACPI_DESCRIPTION_HEADER *)ssdttpm_th500_aml_code)->Length
    );

  //
  // Install SSDT with TPM node
  //
  NewAcpiTable.AcpiTableSignature = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
  NewAcpiTable.AcpiTableRevision  = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
  NewAcpiTable.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
  NewAcpiTable.AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)ssdttpm_th500_aml_code;
  NewAcpiTable.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  NewAcpiTable.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  NewAcpiTable.MinorRevision      = 0;

  Desc.ObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList);
  Desc.Size     = sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
  Desc.Count    = 1;
  Desc.Data     = &NewAcpiTable;

  Status = NvExtendCmObj (ParserHandle, &Desc, CM_NULL_TOKEN, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to add SSDT for TPM - %r\r\n", __FUNCTION__, Status));
    return Status;
  }

  return EFI_SUCCESS;
}
