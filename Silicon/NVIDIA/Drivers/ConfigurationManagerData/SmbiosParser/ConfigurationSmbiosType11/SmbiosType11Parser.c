/** @file
  Configuration Manager Data of SMBIOS Type 11 table.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "SmbiosParserPrivate.h"
#include "SmbiosType11Parser.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType11 = {
  SMBIOS_TYPE_OEM_STRINGS,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType11),
  NULL
};

/**
  Install CM object for SMBIOS Type 11
  @param [in]  ParserHandle A handle to the parser instance.
  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
EFIAPI
InstallSmbiosType11Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  VOID                   *DtbBase = Private->DtbBase;
  CM_SMBIOS_OEM_STRINGS  *OemStrings;
  UINT32                 NumOemStrings;
  CONST CHAR8            *PropertyStr;
  INT32                  Length;
  UINTN                  Index;
  INT32                  NodeOffset;
  CHAR8                  **StringList;
  CHAR8                  PropertyName[] = "oem-stringsxxx";
  EFI_STATUS             Status;
  CM_OBJECT_TOKEN        *TokenMap;
  CM_OBJ_DESCRIPTOR      Desc;

  TokenMap      = NULL;
  NumOemStrings = 0;
  StringList    = NULL;
  OemStrings    = NULL;

  NodeOffset = fdt_subnode_offset (DtbBase, Private->DtbSmbiosOffset, "type11");
  if (NodeOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Device tree node for SMBIOS Type 11 not found.\n", __FUNCTION__));
    Status = RETURN_NOT_FOUND;
    goto CleanupAndReturn;
  }

  OemStrings = (CM_SMBIOS_OEM_STRINGS *)AllocateZeroPool (sizeof (CM_SMBIOS_OEM_STRINGS));
  if (OemStrings == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for OEM Strings\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  // Get oem-strings from DTB
  for (Index = 1; Index < 100; Index++) {
    AsciiSPrint (PropertyName, sizeof (PropertyName), "oem-strings%u", Index);
    PropertyStr = fdt_getprop (DtbBase, NodeOffset, PropertyName, &Length);
    if ((PropertyStr == NULL) || (Length <= 0)) {
      break;
    }

    StringList = ReallocatePool (
                   sizeof (CHAR8 *) * (NumOemStrings),
                   sizeof (CHAR8 *) * (NumOemStrings + 1),
                   StringList
                   );
    if (StringList == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: ReallocatePool failed\n", __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
      goto CleanupAndReturn;
    }

    Length++;
    StringList[NumOemStrings] = AllocateZeroPool (sizeof (CHAR8) * Length);
    if (StringList[NumOemStrings] == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate 0x%llx bytes for StringList[NumOemStrings]\n", __FUNCTION__, sizeof (CHAR8) * Length));
    } else {
      AsciiSPrint (StringList[NumOemStrings], Length, PropertyStr);
    }

    NumOemStrings++;
  }

  //
  // Add type 11 to SMBIOS table list for test
  //
  OemStrings->StringCount = NumOemStrings;
  OemStrings->StringTable = StringList;

  // Allocate Token Map
  Status = NvAllocateCmTokens (ParserHandle, 1, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 11: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  OemStrings->OemStringsToken = TokenMap[0];

  //
  // Install CM object for type 11
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjOemStrings);
  Desc.Size     = sizeof (CM_SMBIOS_OEM_STRINGS);
  Desc.Count    = 1;
  Desc.Data     = OemStrings;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 11 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType11,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

CleanupAndReturn:
  FREE_NON_NULL (TokenMap);
  FREE_NON_NULL (OemStrings);
  return Status;
}
