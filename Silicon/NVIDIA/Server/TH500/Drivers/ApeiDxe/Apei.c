/** @file

  SPDX-FileCopyrightText: Copyright (c) 2021-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Apei.h"

EFI_ACPI_TABLE_PROTOCOL  *AcpiTableProtocol;
RAS_FW_BUFFER            RasFwBufferInfo;
VOID                     *MMCommProtNotify;

STATIC
RAS_PCIE_DPC_COMM_BUF_INFO  *NVIDIARasNsCommPcieDpcData = NULL;

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
      .OemTableId      = PcdGet64 (PcdAcpiDefaultOemTableId),
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

STATIC CONST CHAR8  *PcieCompatibleInfo[] = {
  "nvidia,th500-pcie",
  NULL
};

/**
 * @brief Check if at least one GPU over C2C is enabled on socket 0, then communicate that result to RAS_FW.
 *
 * @param DtbBase           Base address of UEFI DTB
 * @param RasFwBufferInfo   Details about RAS_FW communication buffer
 */
STATIC
VOID
ApeiDxeNotifyC2cGpuPresence (
  VOID              *Dtb,
  IN RAS_FW_BUFFER  *RasFwBufferInfo
  )
{
  EFI_MM_COMMUNICATE_HEADER  CommunicationHeader;
  INT32                      NodeOffset;

  CopyGuid (&(CommunicationHeader.HeaderGuid), &gNVIDIAApeiSetRasFwFlag);
  CommunicationHeader.MessageLength = sizeof (BOOLEAN);
  BOOLEAN  *pC2cGpuPresent = (BOOLEAN *)(RasFwBufferInfo->CommBase + sizeof (CommunicationHeader.HeaderGuid) +
                                         sizeof (CommunicationHeader.MessageLength));

  *pC2cGpuPresent = FALSE;
  NodeOffset      = -1;
  while (DeviceTreeGetNextCompatibleNode (PcieCompatibleInfo, &NodeOffset) != EFI_NOT_FOUND) {
    CONST VOID  *Property;
    INT32       Length;

    Property = fdt_getprop (Dtb, NodeOffset, "c2c-partitions", &Length);
    if ((Property != NULL)) {
      *pC2cGpuPresent = TRUE;
      break;
    }
  }

  FfaGuidedCommunication (&CommunicationHeader, RasFwBufferInfo);
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
  VOID        *DtbBase;
  UINTN       DtbSize;
  INTN        NodeOffset;
  BOOLEAN     SkipSdei;
  BOOLEAN     SkipHest;
  BOOLEAN     SkipBert;
  BOOLEAN     SkipEinj;
  BOOLEAN     SkipErst;
  EFI_EVENT   MmCommunication2ReadyEvent;

  SkipSdei = FALSE;
  SkipHest = FALSE;
  SkipBert = FALSE;
  SkipEinj = FALSE;
  SkipErst = FALSE;

  Status = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NodeOffset = fdt_path_offset (DtbBase, "/firmware/uefi");
  if (NodeOffset >= 0) {
    if (NULL != fdt_get_property (DtbBase, NodeOffset, "skip-sdei-table", NULL)) {
      SkipSdei = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip SDEI Table\r\n", __FUNCTION__));
    }

    if (NULL != fdt_get_property (DtbBase, NodeOffset, "skip-hest-table", NULL)) {
      SkipHest = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip HEST Table\r\n", __FUNCTION__));
    }

    if (NULL != fdt_get_property (DtbBase, NodeOffset, "skip-bert-table", NULL)) {
      SkipBert = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip BERT Table\r\n", __FUNCTION__));
    }

    if (NULL != fdt_get_property (DtbBase, NodeOffset, "skip-einj-table", NULL)) {
      SkipEinj = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip EINJ Table\r\n", __FUNCTION__));
    }

    if (NULL != fdt_get_property (DtbBase, NodeOffset, "skip-erst-table", NULL)) {
      SkipErst = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip ERST Table\r\n", __FUNCTION__));
    }
  }

  ZeroMem ((UINT8 *)&RasFwBufferInfo, sizeof (RAS_FW_BUFFER));

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!SkipSdei) {
    Status = SdeiSetupTable ();
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  Status = FfaGetRasFwBuffer (&RasFwBufferInfo);
  if (!EFI_ERROR (Status)) {
    NVIDIARasNsCommPcieDpcData = (RAS_PCIE_DPC_COMM_BUF_INFO *)AllocateZeroPool (sizeof (RAS_PCIE_DPC_COMM_BUF_INFO));
    if (NVIDIARasNsCommPcieDpcData == NULL) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: RAS_FW NS Memory allocation for NVIDIARasNsCommPcieDpcData failed\r\n",
        __FUNCTION__
        ));
      return EFI_OUT_OF_RESOURCES;
    }

    ApeiDxeNotifyC2cGpuPresence (DtbBase, &RasFwBufferInfo);

    Status = HestBertSetupTables (&RasFwBufferInfo, SkipHest, SkipBert);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = SetTimeOfDay (&RasFwBufferInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to SetTimeOfDay, RTC might not be working: %r\n", __FUNCTION__, Status));
    }

    if (!SkipEinj) {
      Status = EinjSetupTable (&RasFwBufferInfo);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    NVIDIARasNsCommPcieDpcData->PcieBase = RasFwBufferInfo.PcieBase;
    NVIDIARasNsCommPcieDpcData->PcieSize = RasFwBufferInfo.PcieSize;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get RAS_FW NS shared mem: %r\n",
      __FUNCTION__,
      Status
      ));
  }

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

  // ERST uses MmCommunication2's buffer, so don't install till available
  if (!SkipErst) {
    MmCommunication2ReadyEvent = EfiCreateProtocolNotifyEvent (
                                   &gEfiMmCommunication2ProtocolGuid,
                                   TPL_CALLBACK,
                                   ErstSetupTable,
                                   NULL,
                                   &MMCommProtNotify
                                   );
    if (NULL == MmCommunication2ReadyEvent) {
      Status = EFI_OUT_OF_RESOURCES;
      return Status;
    }
  }

  return Status;
}
