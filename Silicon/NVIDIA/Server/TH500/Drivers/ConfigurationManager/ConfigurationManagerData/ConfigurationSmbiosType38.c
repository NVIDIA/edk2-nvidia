/**
  Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "ConfigurationManagerDataPrivate.h"
#include "ConfigurationSmbiosPrivate.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType38 = {
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType38),
  NULL
};

/**
  Install CM object for SMBIOS Type 38

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
EFIAPI
InstallSmbiosType38Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  Status = InstallIpmiDeviceInformationCm (&Private->Repo, Private->RepoEnd);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to install IPMI Device CM\n", __FUNCTION__));
    return Status;
  }

  //
  // Add type 38 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType38,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

  return EFI_SUCCESS;
}
