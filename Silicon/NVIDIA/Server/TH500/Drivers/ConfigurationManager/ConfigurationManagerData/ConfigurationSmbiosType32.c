/**
  Configuration Manager Data of SMBIOS Type 32 table.

  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include "ConfigurationSmbiosPrivate.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType32 = {
  SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType32),
  NULL
};

/**
  Install CM object for SMBIOS Type 32

   @param[in, out] Private   Pointer to the private data of SMBIOS creators

   @return EFI_SUCCESS       Successful installation
   @retval !(EFI_SUCCESS)    Other errors

 **/
EFI_STATUS
EFIAPI
InstallSmbiosType32Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  CM_SMBIOS_SYSTEM_BOOT_INFO      *SystemBootInfo;
  EFI_STATUS                      Status;

  Repo   = Private->Repo;
  Status =  EFI_SUCCESS;

  SystemBootInfo = (CM_SMBIOS_SYSTEM_BOOT_INFO *)AllocateZeroPool (sizeof (CM_SMBIOS_SYSTEM_BOOT_INFO));
  if (SystemBootInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: memory allocation failed\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  SystemBootInfo->BootStatus = BootInformationStatusNoError;

  SystemBootInfo->SystemBootInfoToken = REFERENCE_TOKEN (SystemBootInfo[0]);

  //
  // Add type 32 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType32,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;
  //
  // Install CM object for type 32
  //
  Repo->CmObjectId    = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjSystemBootInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_SMBIOS_SYSTEM_BOOT_INFO);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = SystemBootInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

  return Status;
}
