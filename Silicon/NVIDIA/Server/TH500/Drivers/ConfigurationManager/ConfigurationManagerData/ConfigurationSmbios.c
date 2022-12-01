/** @file
  Configuration Manager Data of SMBIOS tables

  Copyright (c) 2022 - 2023, NVIDIA CORPORATION. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/FruLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <libfdt.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "ConfigurationSmbiosPrivate.h"

/**
  Find FRU by FRU description

  @param[in] Private          Pointer to the private data of SMBIOS creators
  @param[in] FruDescPattern   FRU description pattern to look for.
                              Wildcard '?' can be used to match any characters.

  @return A pointer to the FRU record if found, or NULL if not found.

**/
FRU_DEVICE_INFO *
FindFruByDescription (
  IN  CM_SMBIOS_PRIVATE_DATA  *Private,
  IN  CHAR8                   *FruDescPattern
  )
{
  FRU_DEVICE_INFO  **FruInfo = Private->FruInfo;
  UINT8            FruCount  = Private->FruCount;
  UINT8            Index;
  CHAR8            *FruDesc;
  CHAR8            *Pattern;

  for (Index = 0; Index < FruCount; Index++) {
    FruDesc = FruInfo[Index]->FruDeviceDescription;
    Pattern = FruDescPattern;

    while ((*FruDesc != '\0') &&
           ((*Pattern == '?') || (*FruDesc == *Pattern)))
    {
      FruDesc++;
      Pattern++;
    }

    if (*FruDesc == *Pattern) {
      return FruInfo[Index];
    }
  }

  return NULL;
}

/**
  Allocate and copy string

  @param[in] String     String to be copied

  @return A pointer to the copied string if succeeds, or NULL if fails.

**/
CHAR8 *
AllocateCopyString (
  IN  CHAR8  *String
  )
{
  if (String == NULL) {
    return NULL;
  }

  return (CHAR8 *)AllocateCopyPool (AsciiStrLen (String) + 1, String);
}

/**
  Install the SMBIOS tables to Configuration Manager Data driver

  @param[in, out] PlatformRepositoryInfo      Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd   End address of the Platform Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallCmSmbiosTableList (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd
  )
{
  CM_SMBIOS_PRIVATE_DATA          *Private;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  EFI_STATUS                      Status;

  Private = AllocatePool (sizeof (CM_SMBIOS_PRIVATE_DATA));
  if (Private == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Private->CmSmbiosTableCount = 0;
  Private->Repo               = *PlatformRepositoryInfo;
  Private->RepoEnd            = PlatformRepositoryInfoEnd;

  //
  // Load device tree SMBIOS node
  //
  Status = DtPlatformLoadDtb (&Private->DtbBase, &Private->DtbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to load device tree.\n", __FUNCTION__));
    FreePool (Private);
    return Status;
  }

  Private->DtbSmbiosOffset = fdt_path_offset (Private->DtbBase, "/firmware/smbios");
  if (Private->DtbSmbiosOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Device tree node for SMBIOS not found.\n", __FUNCTION__));
    // Continue anyway to install SMBIOS tables that do not need DTB
  }

  //
  // Read all FRUs
  //
  Status = ReadAllFrus (&Private->FruInfo, &Private->FruCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to read FRUs.\n", __FUNCTION__));
    // Continue anyway to install SMBIOS tables that do not need FRU info
    Private->FruCount = 0;
  }

  //
  // Install CM object for each SMBIOS table
  //
  Status = InstallSmbiosType1Cm (Private);
  DEBUG ((DEBUG_INFO, "%a: Install SMBIOS Type 1 - %r.\n", __FUNCTION__, Status));

  Status = InstallSmbiosType2Cm (Private);
  DEBUG ((DEBUG_INFO, "%a: Install SMBIOS Type 2 - %r.\n", __FUNCTION__, Status));

  Status = InstallSmbiosType9Cm (Private);
  DEBUG ((DEBUG_INFO, "%a: Install SMBIOS Type 9 - %r.\n", __FUNCTION__, Status));

  Status = InstallSmbiosType38Cm (Private);
  DEBUG ((DEBUG_INFO, "%a: Install SMBIOS Type 38 - %r.\n", __FUNCTION__, Status));

  Status = InstallSmbiosType43Cm (Private);
  DEBUG ((DEBUG_INFO, "%a: Install SMBIOS Type 43 - %r.\n", __FUNCTION__, Status));

  //
  // Free all FRUs
  //
  Status = FreeAllFruRecords ();
  ASSERT_EFI_ERROR (Status);

  //
  // Install CM object for SMBIOS table list
  //
  if (Private->CmSmbiosTableCount > 0) {
    Repo                = Private->Repo;
    Repo->CmObjectId    = CREATE_CM_STD_OBJECT_ID (EStdObjSmbiosTableList);
    Repo->CmObjectToken = CM_NULL_TOKEN;
    Repo->CmObjectSize  = sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO) * Private->CmSmbiosTableCount;
    Repo->CmObjectCount = Private->CmSmbiosTableCount;
    Repo->CmObjectPtr   = AllocateCopyPool (Repo->CmObjectSize, &Private->CmSmbiosTableList);
    Repo++;

    ASSERT ((UINTN)Repo <= PlatformRepositoryInfoEnd);

    *PlatformRepositoryInfo = Repo;
  }

  FreePool (Private);
  return EFI_SUCCESS;
}
