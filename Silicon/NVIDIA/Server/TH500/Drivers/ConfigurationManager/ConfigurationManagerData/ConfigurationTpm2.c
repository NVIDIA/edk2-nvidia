/** @file

  Configuration Manager Data of Trusted Computing Platform 2 Table (TPM2)

  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

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
#include "ConfigurationManagerDataPrivate.h"

/**
  Install the TPM2 table to Configuration Manager Data driver

  @param[in, out] PlatformRepositoryInfo      Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd   End address of the Platform Repository
  @param[in, out] PlatformRepositoryInfo      Pointer to the ACPI Table Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallTrustedComputingPlatform2Table (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  )
{
  EFI_STATUS                      Status;
  CM_STD_OBJ_ACPI_TABLE_INFO      *NewAcpiTables;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  EFI_TCG2_PROTOCOL               *Tcg2Protocol;
  CM_ARM_TPM2_INTERFACE_INFO      *TpmInfo;
  UINT32                          Index;
  UINT8                           Tpm2TableRev;
  UINT8                           TpmInterfaceType;

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
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate TPM2 interface info.\n"));
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
      DEBUG ((DEBUG_ERROR, "%a: Unsupported TpmInterfaceType %d\n", __FUNCTION__, TpmInterfaceType));
      FreePool (TpmInfo);
      return EFI_DEVICE_ERROR;
  }

  //
  // Install CM object for TPM interface info
  //
  Repo                = *PlatformRepositoryInfo;
  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjTpm2InterfaceInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_ARM_TPM2_INTERFACE_INFO);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = TpmInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= PlatformRepositoryInfoEnd);

  *PlatformRepositoryInfo = Repo;

  //
  // Create a ACPI Table Entry for TPM2
  //
  for (Index = 0; Index < PcdGet32 (PcdConfigMgrObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (
                                                      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize +
                                                      (sizeof (CM_STD_OBJ_ACPI_TABLE_INFO)),
                                                      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr
                                                      );

      if (NewAcpiTables == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_4_TRUSTED_COMPUTING_PLATFORM_2_TABLE_SIGNATURE;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = Tpm2TableRev;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdTpm2);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData      = NULL;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId         = 0;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }

  return EFI_SUCCESS;
}
