/** @file
  Configuration Manager Data of SMBIOS Type 32 table.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>

#include <ConfigurationManagerObject.h>
#include "SmbiosParserPrivate.h"
#include "SmbiosType32Parser.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType32 = {
  SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType32),
  NULL
};

/**
  Install CM object for SMBIOS Type 32
   @param [in]  ParserHandle A handle to the parser instance.
   @param[in, out] Private   Pointer to the private data of SMBIOS creators

   @return EFI_SUCCESS       Successful installation
   @retval !(EFI_SUCCESS)    Other errors

 **/
EFI_STATUS
EFIAPI
InstallSmbiosType32Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  CM_SMBIOS_SYSTEM_BOOT_INFO  *SystemBootInfo;
  EFI_STATUS                  Status;
  CM_OBJECT_TOKEN             *TokenMap;
  CM_OBJ_DESCRIPTOR           Desc;

  TokenMap       = NULL;
  SystemBootInfo = NULL;
  Status         = EFI_SUCCESS;

  SystemBootInfo = (CM_SMBIOS_SYSTEM_BOOT_INFO *)AllocateZeroPool (sizeof (CM_SMBIOS_SYSTEM_BOOT_INFO));
  if (SystemBootInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: memory allocation failed\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  SystemBootInfo->BootStatus = BootInformationStatusNoError;

  // Allocate Token Map
  Status = NvAllocateCmTokens (ParserHandle, 1, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 32: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  SystemBootInfo->SystemBootInfoToken = TokenMap[0];

  //
  // Install CM object for type 32
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjSystemBootInfo);
  Desc.Size     = sizeof (CM_SMBIOS_SYSTEM_BOOT_INFO);
  Desc.Count    = 1;
  Desc.Data     = SystemBootInfo;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 32 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  //
  // Add type 32 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType32,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

CleanupAndReturn:
  FREE_NON_NULL (TokenMap);
  FREE_NON_NULL (SystemBootInfo);
  return Status;
}
