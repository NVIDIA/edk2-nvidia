/** @file
  Configuration Manager Data Dxe

  SPDX-FileCopyrightText: Copyright (c) 2019 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/

#include <Uefi.h>
#include <Library/ConfigurationManagerDataLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include "NvCmObjectDescUtility.h"

#include "ConfigurationManagerDataParserIncludes.h"

/** The platform configuration repository information.
*/
STATIC
EDKII_PLATFORM_REPOSITORY_INFO  *mNVIDIAPlatformRepositoryInfo = NULL;

/** The platform configuration manager information.
*/
STATIC
CM_STD_OBJ_CONFIGURATION_MANAGER_INFO  CmInfo = {
  CONFIGURATION_MANAGER_REVISION,
  CFG_MGR_OEM_ID
};

#define ADD_SINGLE_PARSER(PARSER) \
  do {\
    PARSER_INFO Parser = CREATE_PARSER (PARSER);\
    Status = Repo->ExtendEntry(\
      Repo,\
      CREATE_CM_OEM_OBJECT_ID(EOemObjCmParser),\
      sizeof(Parser),\
      1,\
      &Parser,\
      CM_NULL_TOKEN,\
      NULL\
    );\
    if (EFI_ERROR(Status)) {\
      return Status;\
    }\
  } while (FALSE);

STATIC
EFIAPI
EFI_STATUS
AddParsers (
  IN EDKII_PLATFORM_REPOSITORY_INFO  *Repo,
  IN UINTN                           ChipID
  )
{
  EFI_STATUS   Status;
  BOOLEAN      SkipSlit;
  BOOLEAN      SkipSrat;
  BOOLEAN      SkipHmat;
  BOOLEAN      SkipIort;
  BOOLEAN      SkipMpam;
  BOOLEAN      SkipApmt;
  BOOLEAN      SkipSpmi;
  BOOLEAN      SkipTpm2;
  INT32        NodeOffset;
  PARSER_INFO  StandardParsers[] = {
    CREATE_PARSER (AcpiTableListParser),
    CREATE_PARSER (BootArchInfoParser), // ArmBootArchInfoParser,
    CREATE_PARSER (FixedFeatureFlagsParser),
    CREATE_PARSER (GenericTimerParser),      // ArmGenericTimerInfoParser,
    CREATE_PARSER (GenericWatchdogInfoParser),
    CREATE_PARSER (PowerManagementProfileParser),
    CREATE_PARSER (SerialPortInfoParser),
    CREATE_PARSER (ProcHierarchyInfoParser), // also includes LpiInfo, CacheInfo, GicCInfo, EtInfo, and CpcInfo
    CREATE_PARSER (EthernetInfoParser),
    CREATE_PARSER (DsdtPatcher),
    CREATE_PARSER (TelemetryInfoParser),
    CREATE_PARSER (TpmInfoParser),
    CREATE_PARSER (ThermalZoneInfoParser),
    CREATE_PARSER (ProtocolBasedObjectsParser),
    CREATE_PARSER (AhciInfoParser),
    CREATE_PARSER (SdhciInfoParser), // Uses SSDT Table Generator
    CREATE_PARSER (I2cInfoParser),   // Uses SSDT Table Generator
    CREATE_PARSER (HdaInfoParser),
    CREATE_PARSER (FanInfoParser),
  };

  NV_ASSERT_RETURN (Repo != NULL, return EFI_INVALID_PARAMETER, "%a: Repo pointer can't be NULL\n", __FUNCTION__);

  // Determine if there are parsers we should skip
  SkipSlit = FALSE;
  SkipSrat = FALSE;
  SkipHmat = FALSE;
  SkipIort = FALSE;
  SkipMpam = FALSE;
  SkipApmt = FALSE;
  SkipSpmi = FALSE;
  SkipTpm2 = FALSE;

  Status = DeviceTreeGetNodeByPath ("/firmware/uefi", &NodeOffset);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_WARN, "%a: Not using /firmware/uefi to skip ACPI tables\n", __FUNCTION__));
  } else if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to determine which ACPI table to enable\n", __FUNCTION__, Status));
    return Status;
  } else if (NodeOffset >= 0) {
    Status = DeviceTreeGetNodeProperty (NodeOffset, "skip-slit-table", NULL, NULL);
    if (Status == EFI_SUCCESS) {
      SkipSlit = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip SLIT Table\n", __FUNCTION__));
    }

    Status = DeviceTreeGetNodeProperty (NodeOffset, "skip-srat-table", NULL, NULL);
    if (Status == EFI_SUCCESS) {
      SkipSrat = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip SRAT Table\n", __FUNCTION__));
    }

    Status = DeviceTreeGetNodeProperty (NodeOffset, "skip-hmat-table", NULL, NULL);
    if (Status == EFI_SUCCESS) {
      SkipHmat = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip HMAT Table\n", __FUNCTION__));
    }

    Status = DeviceTreeGetNodeProperty (NodeOffset, "skip-iort-table", NULL, NULL);
    if (Status == EFI_SUCCESS) {
      SkipIort = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip IORT Table\n", __FUNCTION__));
    }

    Status = DeviceTreeGetNodeProperty (NodeOffset, "skip-mpam-table", NULL, NULL);
    if (Status == EFI_SUCCESS) {
      SkipMpam = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip MPAM Table\n", __FUNCTION__));
    }

    Status = DeviceTreeGetNodeProperty (NodeOffset, "skip-apmt-table", NULL, NULL);
    if (Status == EFI_SUCCESS) {
      SkipApmt = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip APMT Table\n", __FUNCTION__));
    }

    Status = DeviceTreeGetNodeProperty (NodeOffset, "skip-spmi-table", NULL, NULL);
    if (Status == EFI_SUCCESS) {
      SkipSpmi = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip SPMI Table\n", __FUNCTION__));
    }

    Status = DeviceTreeGetNodeProperty (NodeOffset, "skip-tpm2-table", NULL, NULL);
    if (Status == EFI_SUCCESS) {
      SkipTpm2 = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip TPM2 Table\n", __FUNCTION__));
    }
  }

  // Init with the standard parsers list
  Status = Repo->NewEntry (
                   Repo,
                   CREATE_CM_OEM_OBJECT_ID (EOemObjCmParser),
                   sizeof (StandardParsers),
                   ARRAY_SIZE (StandardParsers),
                   StandardParsers,
                   NULL,
                   NULL
                   );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Add additional parsers

  // GIC
  // Note: T194 does GicD here and the rest as part of ProcHierarchyInfo
  if (ChipID == T194_CHIP_ID) {
    ADD_SINGLE_PARSER (GicDParserT194);
  } else {
    ADD_SINGLE_PARSER (GicDParser);
    ADD_SINGLE_PARSER (GicRedistributorParser);
    ADD_SINGLE_PARSER (GicItsParser);
    ADD_SINGLE_PARSER (GicMsiFrameParser);
  }

  if (!SkipIort) {
    ADD_SINGLE_PARSER (IortInfoParser);
  }

  // SSDT table generator - note: should not be run until parsers that add to it are complete!
  ADD_SINGLE_PARSER (SsdtTableGeneratorParser);

  return EFI_SUCCESS;
}

/** Initialize the platform configuration repository.
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
InitializePlatformRepository (
  )
{
  EFI_STATUS                            Status;
  UINTN                                 ChipID;
  EDKII_PLATFORM_REPOSITORY_INFO        *Repo;
  HW_INFO_PARSER_HANDLE                 Parser;
  VOID                                  *DtbBase;
  UINTN                                 DtbSize;
  EDKII_PLATFORM_REPOSITORY_INFO_ENTRY  *Entry;
  UINT32                                Index;

  ChipID = TegraGetChipID ();
  switch (ChipID) {
    case T194_CHIP_ID:
    case T234_CHIP_ID:
    case TH500_CHIP_ID:
      break;

    default:
      DEBUG ((DEBUG_WARN, "%a: New Config Manager not running because ChipId 0x%x isn't supported yet\n", __FUNCTION__, ChipID));
      return EFI_UNSUPPORTED;
  }

  // Allocate and initialize the data store
  Status = ConfigurationManagerDataInit (PcdGet32 (PcdConfigMgrObjMax), &mNVIDIAPlatformRepositoryInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r from ConfigManagerDataInit\n", __FUNCTION__, Status));
    return Status;
  }

  NV_ASSERT_RETURN (mNVIDIAPlatformRepositoryInfo != NULL, return EFI_UNSUPPORTED, "Error initializing the CM Repo\n");

  Repo = mNVIDIAPlatformRepositoryInfo;

  // Add the version information for the repo
  Status = Repo->NewEntry (
                   Repo,
                   CREATE_CM_STD_OBJECT_ID (EStdObjCfgMgrInfo),
                   sizeof (CmInfo),
                   sizeof (CmInfo) / sizeof (CM_STD_OBJ_CONFIGURATION_MANAGER_INFO),
                   &CmInfo,
                   NULL,
                   NULL
                   );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r from adding EStdObjCfgMgrInfo\n", __FUNCTION__, Status));
    return Status;
  }

  // Creates the list of parsers to use
  //    A combination of Nvidia parsers and ARM parsers
  //    Allows runtime selection of which parsers to use
  Status = AddParsers (Repo, ChipID);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r from AddParsers\n", __FUNCTION__, Status));
    return Status;
  }

  // Locate the DTB for the parsers to use if needed
  Status = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r from DtPlatformLoadDtb\n", __FUNCTION__, Status));
    return Status;
  }

  // Init the HwInfo parser
  Status = HwInfoParserInit (DtbBase, Repo, NvHwInfoAdd, &Parser);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r from HwInfoParserInit\n", __FUNCTION__, Status));
    return Status;
  }

  // Run all the parsers in the list
  Status = Repo->FindEntry (Repo, CREATE_CM_OEM_OBJECT_ID (EOemObjCmParser), CM_NULL_TOKEN, &Entry);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r from FindEntry(EOemObjCmParser)\n", __FUNCTION__, Status));
    return Status;
  }

  Status = NvHwInfoParse (Parser, -1, Entry->CmObjectDesc.Data, Entry->CmObjectDesc.Count);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r from NvHwInfoParse\n", __FUNCTION__, Status));
    return Status;
  }

  // Server platform require all OemTableIds to reflect the board config, so update them here
  switch (ChipID) {
    case T194_CHIP_ID:
    case T234_CHIP_ID:
      // Don't modify the OemTableIds
      break;

    case TH500_CHIP_ID:
      Status = Repo->FindEntry (Repo, CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList), CM_NULL_TOKEN, &Entry);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get the AcpiTableList\n", __FUNCTION__, Status));
        return Status;
      }

      // Fix up the OemTableId
      for (Index = 0; Index < Entry->CmObjectDesc.Count; Index++) {
        ((CM_STD_OBJ_ACPI_TABLE_INFO *)Entry->CmObjectDesc.Data)[Index].OemTableId = PcdGet64 (PcdAcpiDefaultOemTableId);
      }

      break;

    default:
      NV_ASSERT_RETURN (FALSE, return EFI_UNSUPPORTED, "%a: Don't know if ChipId 0x%x should have OemTableIds modified or not\n", __FUNCTION__, ChipID);
  }

  return Status;
}

/**
  Entrypoint of Configuration Manager Data Dxe.

  @param  ImageHandle
  @param  SystemTable

  @return EFI_SUCCESS
  @return EFI_LOAD_ERROR
  @return EFI_OUT_OF_RESOURCES

**/
EFI_STATUS
EFIAPI
ConfigurationManagerDataDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = InitializePlatformRepository ();
  if (Status == EFI_UNSUPPORTED) {
    DEBUG ((DEBUG_ERROR, "%a: InitializePlatformRepository returned EFI_UNSUPPORTED\n", __FUNCTION__));
    return EFI_SUCCESS;
  } else if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InitializePlatformRepository returned %r\n", __FUNCTION__, Status));
    return Status;
  }

  return gBS->InstallMultipleProtocolInterfaces (
                &ImageHandle,
                &gNVIDIAConfigurationManagerDataProtocolGuid,
                (VOID *)mNVIDIAPlatformRepositoryInfo,
                NULL
                );
}
