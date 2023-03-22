/** @file
*
*  Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/
#include "Apei.h"
#include <TH500/TH500Definitions.h>

/**
 * Query RAS_FW for error sources. Each RAS driver in RAS_FW should have
 * published its list of error sources during boot, and this function will
 * query them.
 *
 * @param  RasFwBufferInfo
 *
 * @return EFI_STATUS
 **/
STATIC
EFI_STATUS
GetErrorSources (
  IN RAS_FW_BUFFER  *RasFwBufferInfo
  )
{
  EFI_MM_COMMUNICATE_HEADER  CommunicationHeader;
  UINTN                      MaxSize;

  MaxSize = RasFwBufferInfo->CommSize - sizeof (EFI_MM_COMMUNICATE_HEADER);
  CopyGuid (&(CommunicationHeader.HeaderGuid), &gEfiApeiGetErrorSourcesGuid);
  CommunicationHeader.MessageLength = MaxSize;

  return FfaGuidedCommunication (&CommunicationHeader, RasFwBufferInfo);
}

/**
 * Fill the given notification structure.
 * Depending on its SourceId, it is possible to overwrite it in specific cases.
 *
 * @param  NotificationType            GHES Notification type to use.
 * @param  SourceId                    Unique SourceId of the error source.
 * @param  NotificationStructure[OUT]  The ACPI Notification structure to fill.
**/
STATIC
VOID
SetupNotificationStructure (
  IN EFI_APEI_ERROR_SOURCE                                   *ErrorSource,
  IN OUT EFI_ACPI_6_4_HARDWARE_ERROR_NOTIFICATION_STRUCTURE  *NotificationStructure
  )
{
  NotificationStructure->Type = ErrorSource->NotificationType;

  switch (NotificationStructure->Type) {
    case EFI_ACPI_6_4_HARDWARE_ERROR_NOTIFICATION_SOFTWARE_DELEGATED_EXCEPTION:
      NotificationStructure->Vector = ErrorSource->SourceIdSdei;
      break;

    case EFI_ACPI_6_4_HARDWARE_ERROR_NOTIFICATION_GSIV:
      NotificationStructure->Vector = TH500_SW_IO2_INTR;
      break;

    case EFI_ACPI_6_4_HARDWARE_ERROR_NOTIFICATION_POLLED:
      if (ErrorSource->PollInterval < MINIMUM_POLLING_INTERVAL) {
        NotificationStructure->PollInterval = MINIMUM_POLLING_INTERVAL;
      } else {
        NotificationStructure->PollInterval = ErrorSource->PollInterval;
      }

      break;

    default:
      DEBUG ((EFI_D_ERROR, "%a: Unsupported notification type=%d\n", __FUNCTION__, NotificationStructure->Type));
  }
}

/**
 * Given a pointer to the error sources, build the HEST table.
 *
 * @param  ErrorSourceInfo             Error source information from RAS_FW.
**/
STATIC
VOID
HESTCreateAcpiTable (
  EFI_APEI_ERROR_SOURCE_INFO  *ErrorSourceInfo
  )
{
  EFI_ACPI_6_4_HARDWARE_ERROR_SOURCE_TABLE_HEADER                 *HestTable;
  UINTN                                                           HestTableSize;
  UINT8                                                           Checksum;
  UINTN                                                           AcpiTableHandle;
  UINTN                                                           i;
  EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE  GHESv2Instance;
  UINT8                                                           *ErrorSourceStructure;
  EFI_APEI_ERROR_SOURCE                                           *ErrorSource;
  EFI_STATUS                                                      Status              = EFI_SUCCESS;
  EFI_ACPI_TABLE_PROTOCOL                                         *AcpiTableProtocol  = NULL;
  UINT32                                                          TotalNumErrorSource = ErrorSourceInfo->NumErrorSource;

  ErrorSource = (EFI_APEI_ERROR_SOURCE *)((UINTN)ErrorSourceInfo + sizeof (EFI_APEI_ERROR_SOURCE_INFO));

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );

  HestTableSize = sizeof (EFI_ACPI_6_4_HARDWARE_ERROR_SOURCE_TABLE_HEADER);

  for (i = 0; i < TotalNumErrorSource; i++) {
    if (ErrorSource[i].GhesType == EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_VERSION_2) {
      if (ErrorSource[i].EventId != BERT_EVENT_ID) {
        /* Each error source can lead to 2 entries to support dynamic GSIV/SDEI */
        HestTableSize += sizeof (EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE) * 2;
      }
    } else {
      DEBUG ((EFI_D_ERROR, "%a: Unsupported type=%d\n", __FUNCTION__, ErrorSource[i].GhesType));
    }
  }

  /* Allocate enough space for the header and error sources */
  HestTable = AllocateReservedZeroPool (HestTableSize);

  *HestTable = (EFI_ACPI_6_4_HARDWARE_ERROR_SOURCE_TABLE_HEADER) {
    .Header = {
      .Signature       = EFI_ACPI_6_4_HARDWARE_ERROR_SOURCE_TABLE_SIGNATURE,
      .Length          = sizeof (EFI_ACPI_6_4_HARDWARE_ERROR_SOURCE_TABLE_HEADER),
      .Revision        = EFI_ACPI_OEM_REVISION,
      .OemId           = EFI_ACPI_OEM_ID,
      .OemTableId      = PcdGet64 (PcdAcpiDefaultOemTableId),
      .OemRevision     = EFI_ACPI_OEM_REVISION,
      .CreatorId       = EFI_ACPI_CREATOR_ID,
      .CreatorRevision = EFI_ACPI_CREATOR_REVISION
    },
    .ErrorSourceCount = 0
  };

  /* Assume all error sources are GHESv2 compliant for now */
  HestTable->ErrorSourceCount = 0;
  ErrorSourceStructure        = ((UINT8 *)HestTable + sizeof (EFI_ACPI_6_4_HARDWARE_ERROR_SOURCE_TABLE_HEADER));

  for (i = 0; i < TotalNumErrorSource; i++) {
    if (ErrorSource[i].GhesType != EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_VERSION_2) {
      continue;
    }

    if (ErrorSource[i].EventId == BERT_EVENT_ID) {
      continue;
    }

    ZeroMem (&GHESv2Instance, sizeof (EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE));

    GHESv2Instance = (EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE) {
      .Type            = ErrorSource[i].GhesType,
      .SourceId        = ErrorSource[i].SourceId,
      .RelatedSourceId = 0xFFFF,
      .Flags           = 0,
      .Enabled         = 1,

      .NumberOfRecordsToPreAllocate = ErrorSource[i].NumberRecordstoPreAllocate,
      .MaxSectionsPerRecord         = ErrorSource[i].MaxSectionsPerRecord,
      .MaxRawDataLength             = 0,
      .ErrorStatusBlockLength       = ErrorSource[i].MaxRawDataLength + sizeof (EFI_ACPI_6_4_GENERIC_ERROR_STATUS_STRUCTURE),

      /* Error entry address info */
      .ErrorStatusAddress = {
        .AddressSpaceId    = ErrorSource[i].ErrorStatusAddress.AddrerssSpaceId,
        .RegisterBitWidth  = ErrorSource[i].ErrorStatusAddress.RegisterBitWidth,
        .RegisterBitOffset = ErrorSource[i].ErrorStatusAddress.RegisterBitOffset,
        .AccessSize        = ErrorSource[i].ErrorStatusAddress.AccessSize,
        .Address           = ErrorSource[i].ErrorStatusAddress.Address,
      },

      /* Acknowledgment register info */
      .ReadAckRegister = {
        .AddressSpaceId    = ErrorSource[i].ReadAckRegister.AddrerssSpaceId,
        .RegisterBitWidth  = ErrorSource[i].ReadAckRegister.RegisterBitWidth,
        .RegisterBitOffset = ErrorSource[i].ReadAckRegister.RegisterBitOffset,
        .AccessSize        = ErrorSource[i].ReadAckRegister.AccessSize,
        .Address           = ErrorSource[i].ReadAckRegister.Address
      },
      .ReadAckPreserve = ErrorSource[i].ReadAckPreserve,
      .ReadAckWrite    = ErrorSource[i].ReadAckWrite
    };

    SetupNotificationStructure (&(ErrorSource[i]), &(GHESv2Instance.NotificationStructure));

    CopyMem (
      ErrorSourceStructure,
      (UINT8 *)&GHESv2Instance,
      sizeof (EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE)
      );

    ErrorSourceStructure     += sizeof (EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE);
    HestTable->Header.Length += sizeof (EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE);
    HestTable->ErrorSourceCount++;

    DEBUG ((
      EFI_D_INFO,
      "%a: Added GHES entry for SourceId=%d. ErrStatusAddress=0x%llx\n",
      __FUNCTION__,
      GHESv2Instance.SourceId,
      GHESv2Instance.ErrorStatusAddress.Address
      ));

    /*
     * The default notification for uncorrected errors is SDEI. In case the OS does not support it
     * or if SDEI support is not enabled, there is a need to fallback to GSIV.
     * For each GSIV error source, create a duplicate SDEI entry. When an error needs to be reported,
     * system firmware will attempt to assert SDEI, and in case of failure, will fallback to GSIV.
     */
    if ((ErrorSource[i].NotificationType == EFI_ACPI_6_4_HARDWARE_ERROR_NOTIFICATION_GSIV) &&
        (ErrorSource[i].SourceIdSdei != 0))
    {
      ErrorSource[i].NotificationType = EFI_ACPI_6_4_HARDWARE_ERROR_NOTIFICATION_SOFTWARE_DELEGATED_EXCEPTION;
      GHESv2Instance.SourceId         = ErrorSource[i].SourceIdSdei;

      SetupNotificationStructure (&(ErrorSource[i]), &(GHESv2Instance.NotificationStructure));

      CopyMem (
        ErrorSourceStructure,
        (UINT8 *)&GHESv2Instance,
        sizeof (EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE)
        );

      ErrorSourceStructure     += sizeof (EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE);
      HestTable->Header.Length += sizeof (EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE);
      HestTable->ErrorSourceCount++;

      DEBUG ((
        EFI_D_INFO,
        "%a: Added duplicate SDEI entry for SourceId=%d SDEI=%d. ErrStatusAddress=0x%llx\n",
        __FUNCTION__,
        GHESv2Instance.SourceId,
        ErrorSource[i].SourceIdSdei,
        GHESv2Instance.ErrorStatusAddress.Address
        ));
    }
  }

  Checksum                   = CalculateCheckSum8 ((UINT8 *)(HestTable), HestTable->Header.Length);
  HestTable->Header.Checksum = Checksum;

  Status = AcpiTableProtocol->InstallAcpiTable (
                                AcpiTableProtocol,
                                HestTable,
                                HestTable->Header.Length,
                                &AcpiTableHandle
                                );
  ASSERT_EFI_ERROR (Status);

  return;
}

/**
 * Given a pointer to the error sources, build the BERT table.
 * Note: BERT error data is simply a special error source within the RAS_FW error sources.
 *
 * @param  ErrorSourceInfo             Error source information from RAS_FW.
**/
STATIC
VOID
BERTCreateAcpiTable (
  EFI_APEI_ERROR_SOURCE_INFO  *ErrorSourceInfo
  )
{
  EFI_STATUS                                   Status = EFI_SUCCESS;
  EFI_ACPI_6_4_BOOT_ERROR_RECORD_TABLE_HEADER  *BertTable;
  UINTN                                        BertSize;
  UINT8                                        Checksum;
  UINTN                                        AcpiTableHandle;
  UINTN                                        i;
  UINT32                                       TotalNumErrorSource;
  EFI_ACPI_6_4_GENERIC_ERROR_STATUS_STRUCTURE  *Gess;
  EFI_ACPI_TABLE_PROTOCOL                      *AcpiTableProtocol   = NULL;
  EFI_APEI_ERROR_SOURCE                        *ErrorSource         = NULL;
  EFI_APEI_ERROR_SOURCE                        *BERTErrorSourceInfo = NULL;
  UINTN                                        *BootErrorRegionRegister;

  TotalNumErrorSource = ErrorSourceInfo->NumErrorSource;
  ErrorSource         = (EFI_APEI_ERROR_SOURCE *)
                        ((UINT8 *)ErrorSourceInfo +
                         sizeof (EFI_APEI_ERROR_SOURCE_INFO));

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );

  for (i = 0; i < TotalNumErrorSource; i++) {
    if (ErrorSource[i].EventId == BERT_EVENT_ID) {
      BERTErrorSourceInfo = &(ErrorSource[i]);
    }
  }

  if (BERTErrorSourceInfo == NULL) {
    DEBUG ((EFI_D_ERROR, "%a: BERT error source missing. Cannot create BERT table.\n", __FUNCTION__));
    return;
  }

  BootErrorRegionRegister = (UINTN *)BERTErrorSourceInfo->ErrorStatusAddress.Address;
  Gess                    = (EFI_ACPI_6_4_GENERIC_ERROR_STATUS_STRUCTURE *)*BootErrorRegionRegister;

  /* Allocate enough space for the header and error sources */
  BertSize  = sizeof (EFI_ACPI_6_4_BOOT_ERROR_RECORD_TABLE_HEADER);
  BertTable = AllocateReservedZeroPool (BertSize);

  *BertTable = (EFI_ACPI_6_4_BOOT_ERROR_RECORD_TABLE_HEADER) {
    .Header = {
      .Signature       = EFI_ACPI_6_4_BOOT_ERROR_RECORD_TABLE_SIGNATURE,
      .Length          = sizeof (EFI_ACPI_6_4_BOOT_ERROR_RECORD_TABLE_HEADER),
      .Revision        = EFI_ACPI_6_4_BOOT_ERROR_RECORD_TABLE_REVISION,
      .OemId           = EFI_ACPI_OEM_ID,
      .OemTableId      = PcdGet64 (PcdAcpiDefaultOemTableId),
      .OemRevision     = EFI_ACPI_OEM_REVISION,
      .CreatorId       = EFI_ACPI_CREATOR_ID,
      .CreatorRevision = EFI_ACPI_CREATOR_REVISION
    },
    .BootErrorRegion       = (UINTN)Gess,
    .BootErrorRegionLength = Gess->DataLength + sizeof (EFI_ACPI_6_4_GENERIC_ERROR_STATUS_STRUCTURE)
  };

  Checksum                   = CalculateCheckSum8 ((UINT8 *)(BertTable), BertTable->Header.Length);
  BertTable->Header.Checksum = Checksum;

  Status = AcpiTableProtocol->InstallAcpiTable (
                                AcpiTableProtocol,
                                BertTable,
                                BertTable->Header.Length,
                                &AcpiTableHandle
                                );
  ASSERT_EFI_ERROR (Status);

  return;
}

EFI_STATUS
HestBertSetupTables (
  RAS_FW_BUFFER  *RasFwBufferInfo,
  BOOLEAN        SkipHestTable,
  BOOLEAN        SkipBertTable
  )
{
  EFI_MM_COMMUNICATE_HEADER   *CommunicationHeader;
  EFI_APEI_ERROR_SOURCE_INFO  *ErrorSourceInfo;
  EFI_STATUS                  Status;

  Status = GetErrorSources (RasFwBufferInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to get ErrorSourceInfo: %u\n", __FUNCTION__, Status));
    return Status;
  }

  CommunicationHeader = (EFI_MM_COMMUNICATE_HEADER *)RasFwBufferInfo->CommBase;

  if (CommunicationHeader->MessageLength == 0) {
    DEBUG ((EFI_D_ERROR, "%a: No data from RAS_FW\n", __FUNCTION__));
    return Status;
  }

  ErrorSourceInfo = (EFI_APEI_ERROR_SOURCE_INFO *)AllocateZeroPool (RasFwBufferInfo->CommSize);
  MmioReadBuffer64 ((UINTN)&(CommunicationHeader->Data), RasFwBufferInfo->CommSize, (UINT64 *)ErrorSourceInfo);

  DEBUG ((
    EFI_D_INFO,
    "%a: ErrorRecordRegion 0x%p (Size: 0x%x) Entries : %d\n",
    __FUNCTION__,
    ErrorSourceInfo->ErrorRecordsRegionBase,
    ErrorSourceInfo->ErrorRecordsRegionSize,
    ErrorSourceInfo->NumErrorSource
    ));

  if (!SkipHestTable) {
    HESTCreateAcpiTable (ErrorSourceInfo);
  }

  if (!SkipBertTable) {
    BERTCreateAcpiTable (ErrorSourceInfo);
  }

  FreePool (ErrorSourceInfo);

  return Status;
}
