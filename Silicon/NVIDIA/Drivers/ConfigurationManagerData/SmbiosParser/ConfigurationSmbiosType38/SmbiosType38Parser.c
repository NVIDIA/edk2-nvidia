/** @file
  Configuration Manager Data of SMBIOS Type 38 table.

  SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "SmbiosParserPrivate.h"
#include "SmbiosType38Parser.h"

extern BOOLEAN  mIpmiDevCmInstalled;

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType38 = {
  SMBIOS_TYPE_IPMI_DEVICE_INFORMATION,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType38),
  NULL
};

/**
  Install CM object for SMBIOS Type 38
  @param [in]  ParserHandle A handle to the parser instance.
  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
EFIAPI
InstallSmbiosType38Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  if (!mIpmiDevCmInstalled) {
    DEBUG ((DEBUG_INFO, "%a: No IPMI Device. Skip installing Smbios Type 38.\n", __FUNCTION__));
    return EFI_SUCCESS;
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
