/** @file

  SPDX-FileCopyrightText: Copyright (c) 2021-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Apei.h"

EFI_ACPI_TABLE_PROTOCOL  *AcpiTableProtocol;
RAS_FW_BUFFER            *RasFwBufferInfo;
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
      .OemTableId      = PcdGet64 (PcdAcpiDefaultOemTableId),
      .OemRevision     = EFI_ACPI_OEM_REVISION,
      .CreatorId       = EFI_ACPI_CREATOR_ID,
      .CreatorRevision = EFI_ACPI_CREATOR_REVISION
    }
  };
  CopyMem (SdeiTable->Header.OemId, PcdGetPtr (PcdAcpiDefaultOemId), sizeof (SdeiTable->Header.OemId));

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
  VOID                 *Dtb,
  IN RAS_FW_INTERFACE  *RasFwInterface,
  IN UINT32            SocketCount
  )
{
  BOOLEAN  C2cGpuPresent = FALSE;
  INT32    NodeOffset;

  NodeOffset = -1;
  while (DeviceTreeGetNextCompatibleNode (PcieCompatibleInfo, &NodeOffset) != EFI_NOT_FOUND) {
    CONST VOID  *Property;
    INT32       Length;

    Property = FdtGetProp (Dtb, NodeOffset, "c2c-partitions", &Length);
    if ((Property != NULL)) {
      C2cGpuPresent = TRUE;
      break;
    }
  }

  for (UINT32 Socket = 0; Socket < SocketCount; Socket++) {
    EFI_MM_COMMUNICATE_HEADER  CommunicationHeader;

    CopyGuid (&(CommunicationHeader.HeaderGuid), &gNVIDIAApeiSetRasFwFlag);
    CommunicationHeader.MessageLength = sizeof (BOOLEAN);
    BOOLEAN  *pC2cGpuPresent = (BOOLEAN *)(RasFwInterface->RasFwBufferInfo[Socket].CommBase + sizeof (CommunicationHeader.HeaderGuid) +
                                           sizeof (CommunicationHeader.MessageLength));
    *pC2cGpuPresent = C2cGpuPresent;

    RasFwInterface->GuidedComm (Socket, &CommunicationHeader, &(RasFwInterface->RasFwBufferInfo[Socket]));
  }
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
  EFI_STATUS        Status;
  VOID              *DtbBase;
  UINTN             DtbSize;
  INTN              NodeOffset;
  BOOLEAN           SkipSdei;
  BOOLEAN           SkipHest;
  BOOLEAN           SkipBert;
  BOOLEAN           SkipEinj;
  BOOLEAN           SkipErst;
  EFI_EVENT         MmCommunication2ReadyEvent;
  UINT32            MaxSocketValue, SocketCount;
  RAS_FW_INTERFACE  *RasFwInterface;

  SkipSdei = FALSE;
  SkipHest = FALSE;
  SkipBert = FALSE;
  SkipEinj = FALSE;
  SkipErst = FALSE;

  Status = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  if (EFI_ERROR (Status)) {
    goto exit;
  }

  MpCoreInfoGetPlatformInfo (NULL, &MaxSocketValue, NULL, NULL, NULL);
  SocketCount = MaxSocketValue+1;

  NodeOffset = FdtPathOffset (DtbBase, "/firmware/uefi");
  if (NodeOffset >= 0) {
    if (NULL != FdtGetProperty (DtbBase, NodeOffset, "skip-sdei-table", NULL)) {
      SkipSdei = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip SDEI Table\r\n", __FUNCTION__));
    }

    if (NULL != FdtGetProperty (DtbBase, NodeOffset, "skip-hest-table", NULL)) {
      SkipHest = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip HEST Table\r\n", __FUNCTION__));
    }

    if (NULL != FdtGetProperty (DtbBase, NodeOffset, "skip-bert-table", NULL)) {
      SkipBert = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip BERT Table\r\n", __FUNCTION__));
    }

    if (NULL != FdtGetProperty (DtbBase, NodeOffset, "skip-einj-table", NULL)) {
      SkipEinj = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip EINJ Table\r\n", __FUNCTION__));
    }

    if (NULL != FdtGetProperty (DtbBase, NodeOffset, "skip-erst-table", NULL)) {
      SkipErst = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip ERST Table\r\n", __FUNCTION__));
    }
  }

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) {
    goto exit;
  }

  NVIDIARasNsCommPcieDpcData = (RAS_PCIE_DPC_COMM_BUF_INFO *)AllocateZeroPool (sizeof (RAS_PCIE_DPC_COMM_BUF_INFO));
  if (NVIDIARasNsCommPcieDpcData == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: RAS_FW NS Memory allocation for NVIDIARasNsCommPcieDpcData failed\r\n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto exit;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gNVIDIARasNsCommPcieDpcDataProtocolGuid,
                  (VOID *)NVIDIARasNsCommPcieDpcData,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to install NVIDIARasNsCommPcieDpcDataProtocol (%r)\r\n", __FUNCTION__, Status));
    Status = EFI_PROTOCOL_ERROR;
    goto exit;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: Successfully installed NVIDIARasNsCommPcieDpcDataProtocol (%r)\r\n", __FUNCTION__, Status));

  Status = RASFwInterfaceInit (SocketCount, &RasFwInterface);
  if (EFI_ERROR (Status)) {
    goto exit;
  }

  if (!SkipSdei) {
    Status = SdeiSetupTable ();
    if (EFI_ERROR (Status)) {
      goto exit;
    }
  }

  ApeiDxeNotifyC2cGpuPresence (DtbBase, RasFwInterface, SocketCount);
  Status = HestBertSetupTables (RasFwInterface, SkipHest, SkipBert, SocketCount);
  if (EFI_ERROR (Status)) {
    goto exit;
  }

  Status = SetTimeOfDay (RasFwInterface, SocketCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to SetTimeOfDay, RTC might not be working: %r\n", __FUNCTION__, Status));
  }

  NVIDIARasNsCommPcieDpcData->PcieBase = RasFwInterface->RasFwBufferInfo[0].PcieBase;
  NVIDIARasNsCommPcieDpcData->PcieSize = RasFwInterface->RasFwBufferInfo[0].PcieSize;

  if (!SkipEinj) {
    Status = EinjSetupTable (RasFwInterface);
    if (EFI_ERROR (Status)) {
      goto exit;
    }
  }

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
      goto exit;
    }
  }

exit:
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: APEI driver failed (%r)\r\n", __FUNCTION__, Status));
  }

  return EFI_SUCCESS;
}
