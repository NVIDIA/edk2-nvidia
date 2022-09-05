/** @file
*
*  Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "Apei.h"

EFI_ACPI_TABLE_PROTOCOL  *AcpiTableProtocol;
RAS_FW_BUFFER            RasFwBufferInfo;

STATIC
RAS_PCIE_DPC_COMM_BUF_INFO  *NVIDIARasNsCommPcieDpcData;

/*
 * Setup the ARM defined SDEI table to enable SDEI support in the OS. SDEI can
 * be used as a notification mechanism for some error sources.
 */
STATIC
EFI_STATUS
SdeiSetupTable (
  )
{
  EFI_STATUS               Status = EFI_SUCCESS;
  EFI_ACPI_6_X_SDEI_TABLE  *SdeiTable;
  UINTN                    SdeiSize;
  UINT8                    Checksum;
  UINTN                    AcpiTableHandle;

  /* Allocate enough space for the header and error sources */
  SdeiSize  = sizeof (EFI_ACPI_6_X_SDEI_TABLE);
  SdeiTable = AllocateReservedZeroPool (SdeiSize);

  *SdeiTable = (EFI_ACPI_6_X_SDEI_TABLE) {
    .Header = {
      .Signature       = EFI_ACPI_6_X_SDEI_TABLE_SIGNATURE,
      .Length          = sizeof (EFI_ACPI_6_X_SDEI_TABLE),
      .Revision        = EFI_ACPI_6_X_SDEI_TABLE_REVISION,
      .OemId           = EFI_ACPI_OEM_ID,
      .OemTableId      = EFI_ACPI_OEM_TABLE_ID,
      .OemRevision     = EFI_ACPI_OEM_REVISION,
      .CreatorId       = EFI_ACPI_CREATOR_ID,
      .CreatorRevision = EFI_ACPI_CREATOR_REVISION
    }
  };

  Checksum                   = CalculateCheckSum8 ((UINT8 *)(SdeiTable), SdeiTable->Header.Length);
  SdeiTable->Header.Checksum = Checksum;

  Status = AcpiTableProtocol->InstallAcpiTable (
                                AcpiTableProtocol,
                                SdeiTable,
                                SdeiTable->Header.Length,
                                &AcpiTableHandle
                                );
  return Status;
}

/**
 * Entry point of the driver.
 *
 * @param ImageHandle     The image handle.
 * @param SystemTable     The system table.
**/
EFI_STATUS
EFIAPI
ApeiDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  ZeroMem ((UINT8 *)&RasFwBufferInfo, sizeof (RAS_FW_BUFFER));

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdeiSetupTable ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = FfaGetRasFwBuffer (&RasFwBufferInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      EFI_D_ERROR,
      "%a: Failed to get RAS_FW NS shared mem: %d\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  gDS->AddMemorySpace (
         EfiGcdMemoryTypeReserved,
         RasFwBufferInfo.Base,
         RasFwBufferInfo.Size,
         EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
         );

  gDS->SetMemorySpaceAttributes (
         RasFwBufferInfo.Base,
         RasFwBufferInfo.Size,
         EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
         );

  NVIDIARasNsCommPcieDpcData = (RAS_PCIE_DPC_COMM_BUF_INFO *)AllocateZeroPool (sizeof (RAS_PCIE_DPC_COMM_BUF_INFO));
  if (NVIDIARasNsCommPcieDpcData == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: RAS_FW NS Memory allocation for NVIDIARasNsCommPcieDpcData failed\r\n",
      __FUNCTION__
      ));
    return EFI_OUT_OF_RESOURCES;
  }

  NVIDIARasNsCommPcieDpcData->PcieBase = RasFwBufferInfo.PcieBase;
  NVIDIARasNsCommPcieDpcData->PcieSize = RasFwBufferInfo.PcieSize;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gNVIDIARasNsCommPcieDpcDataProtocolGuid,
                  (VOID *)NVIDIARasNsCommPcieDpcData,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to install NVIDIARasNsCommPcieDpcDataProtocol (%r)\r\n", __FUNCTION__, Status));
    return EFI_PROTOCOL_ERROR;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: Successfully installed NVIDIARasNsCommPcieDpcDataProtocol (%r)\r\n", __FUNCTION__, Status));

  Status = HestBertSetupTables (&RasFwBufferInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = EinjSetupTable (&RasFwBufferInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}
