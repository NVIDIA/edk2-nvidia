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
#include <Library/DeviceTreeHelperLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>

/** The platform configuration repository information.
*/
EDKII_PLATFORM_REPOSITORY_INFO  *mNVIDIAPlatformRepositoryInfo = NULL;

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
    case T264_CHIP_ID:
      break;

    default:
      DEBUG ((DEBUG_WARN, "%a: Config Manager not running because ChipId 0x%x isn't supported yet\n", __FUNCTION__, ChipID));
      return EFI_UNSUPPORTED;
  }

  NV_ASSERT_RETURN (mNVIDIAPlatformRepositoryInfo != NULL, return EFI_UNSUPPORTED, "Repo wasn't properly initialized!\n");

  Repo = mNVIDIAPlatformRepositoryInfo;

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
    DEBUG ((DEBUG_ERROR, "%a: Got %r from NvHwInfoParse. Attempting to continue anyway.\n", __FUNCTION__, Status));
  }

  // Server platform require all OemTableIds to reflect the board config, so update them here
  switch (ChipID) {
    case T194_CHIP_ID:
    case T234_CHIP_ID:
    case T264_CHIP_ID:
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
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InitializePlatformRepository returned %r. Attempting to continue anyway\n", __FUNCTION__, Status));
  }

 #if !defined (MDEPKG_NDEBUG)
  Status = mNVIDIAPlatformRepositoryInfo->TokenProtocol->SanityCheck (mNVIDIAPlatformRepositoryInfo->TokenProtocol, mNVIDIAPlatformRepositoryInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: SanityCheck returned %r\n", __FUNCTION__, Status));
    return Status;
  }

 #endif

  return gBS->InstallMultipleProtocolInterfaces (
                &ImageHandle,
                &gNVIDIAConfigurationManagerDataProtocolGuid,
                (VOID *)mNVIDIAPlatformRepositoryInfo,
                NULL
                );
}
