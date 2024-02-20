/** @file
  Trusted Computing Platform 2 Table (TPM2) Parser

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "Tpm2Parser.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/Tpm2DeviceLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/Tcg2Protocol.h>
#include <IndustryStandard/Tpm2Acpi.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

EFI_STATUS
EFIAPI
Tpm2Parser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                  Status;
  CM_STD_OBJ_ACPI_TABLE_INFO  AcpiTableHeader;
  EFI_TCG2_PROTOCOL           *Tcg2Protocol;
  CM_ARM_TPM2_INTERFACE_INFO  *TpmInfo;
  UINT8                       Tpm2TableRev;
  UINT8                       TpmInterfaceType;
  CM_OBJ_DESCRIPTOR           Desc;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  if (!PcdGetBool (PcdTpmEnable)) {
    return EFI_SUCCESS;
  }

  //
  // Check if TPM2 interface is supported
  //
  Status = gBS->LocateProtocol (&gEfiTcg2ProtocolGuid, NULL, (VOID **)&Tcg2Protocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: No TCG2 protocol. Skip installing TPM2 table.\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  //
  // Allocate and zero out TPM2 Interface Info
  //
  TpmInfo = AllocateZeroPool (sizeof (CM_ARM_TPM2_INTERFACE_INFO));
  if (TpmInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate TPM2 interface info.\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  Tpm2TableRev     = PcdGet8 (PcdTpm2AcpiTableRev);
  TpmInterfaceType = PcdGet8 (PcdActiveTpmInterfaceType);

  if (Tpm2TableRev >= EFI_TPM2_ACPI_TABLE_REVISION_4) {
    TpmInfo->PlatformClass = PcdGet8 (PcdTpmPlatformClass);
    TpmInfo->Laml          = PcdGet32 (PcdTpm2AcpiTableLaml);
    TpmInfo->Lasa          = PcdGet64 (PcdTpm2AcpiTableLasa);
  }

  switch (TpmInterfaceType) {
    case Tpm2PtpInterfaceTis:
      TpmInfo->AddressOfControlArea = 0;
      TpmInfo->StartMethod          = EFI_TPM2_ACPI_TABLE_START_METHOD_TIS;
      break;

    default:
      DEBUG ((DEBUG_ERROR, "%a: Unsupported TpmInterfaceType %u\n", __FUNCTION__, TpmInterfaceType));
      FreePool (TpmInfo);
      return EFI_DEVICE_ERROR;
  }

  //
  // Install CM object for TPM interface info
  //
  Status = NvAddSingleCmObj (
             ParserHandle,
             CREATE_CM_ARM_OBJECT_ID (EArmObjTpm2InterfaceInfo),
             TpmInfo,
             sizeof (CM_ARM_TPM2_INTERFACE_INFO),
             NULL
             );
  ASSERT_EFI_ERROR (Status);

  //
  // Create a ACPI Table Entry for TPM2
  //
  AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_4_TRUSTED_COMPUTING_PLATFORM_2_TABLE_SIGNATURE;
  AcpiTableHeader.AcpiTableRevision  = Tpm2TableRev;
  AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdTpm2);
  AcpiTableHeader.AcpiTableData      = NULL;
  AcpiTableHeader.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
  AcpiTableHeader.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader.MinorRevision      = 0;

  Desc.ObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList);
  Desc.Size     = sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
  Desc.Count    = 1;
  Desc.Data     = &AcpiTableHeader;

  Status = NvExtendCmObj (ParserHandle, &Desc, CM_NULL_TOKEN, NULL);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  return Status;
}
