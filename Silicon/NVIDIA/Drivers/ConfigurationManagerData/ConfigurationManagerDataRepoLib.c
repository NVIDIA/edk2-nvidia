/** @file
  Configuration Manager Data Repo Lib

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/ConfigurationManagerDataLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/PcdLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/NVIDIADebugLib.h>

/** The platform configuration repository information.
*/
extern EDKII_PLATFORM_REPOSITORY_INFO  *mNVIDIAPlatformRepositoryInfo;

/** The platform configuration manager information.
*/
STATIC
CM_STD_OBJ_CONFIGURATION_MANAGER_INFO  CmInfo = {
  CONFIGURATION_MANAGER_REVISION,
  CFG_MGR_OEM_ID // Note: This gets overwritten with PcdAcpiDefaultOemId
};

EFI_STATUS
EFIAPI
ConfigManagerDataRepoRegisterParser (
  PARSER_INFO  *Parser,
  CHAR8        *ParserSkipString
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  INT32                           NodeOffset;

  Repo = mNVIDIAPlatformRepositoryInfo;

  if (ParserSkipString != NULL) {
    Status = DeviceTreeGetNodeByPath ("/firmware/uefi", &NodeOffset);
    if (Status == EFI_NOT_FOUND) {
      DEBUG ((DEBUG_WARN, "%a: Can't find /firmware/uefi to check for %a\n", __FUNCTION__, ParserSkipString));
    } else if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to check for %a\n", __FUNCTION__, Status, ParserSkipString));
      return Status;
    } else if (NodeOffset >= 0) {
      Status = DeviceTreeGetNodeProperty (NodeOffset, ParserSkipString, NULL, NULL);
      if (Status == EFI_SUCCESS) {
        DEBUG ((DEBUG_ERROR, "%a: Skipping %a due to seeing %a\n", __FUNCTION__, Parser->ParserName, ParserSkipString));
        return EFI_SUCCESS;
      }
    }
  }

  Status = Repo->ExtendEntry (
                   Repo,
                   CREATE_CM_OEM_OBJECT_ID (EOemObjCmParser),
                   sizeof (*Parser),
                   1,
                   Parser,
                   CM_NULL_TOKEN,
                   NULL
                   );
  if (Status == EFI_NOT_FOUND) {
    Status = Repo->NewEntry (
                     Repo,
                     CREATE_CM_OEM_OBJECT_ID (EOemObjCmParser),
                     sizeof (*Parser),
                     1,
                     Parser,
                     CM_NULL_TOKEN,
                     NULL
                     );
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to register the %a parser: %r\n", __FUNCTION__, Parser->ParserName, Status));
  }

  return Status;
}

/** Initialize the platform configuration repository.
  @retval EFI_SUCCESS   Success
**/
EFI_STATUS
EFIAPI
InitializePlatformRepository (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                      Status;
  UINTN                           ChipID;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;

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

  CopyMem (CmInfo.OemId, PcdGetPtr (PcdAcpiDefaultOemId), sizeof (CmInfo.OemId));

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

  return Status;
}
