/** @file
  Configuration Manager Data of SMBIOS Type 11 table

  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
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

#include "ConfigurationSmbiosPrivate.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType11 = {
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType11),
  NULL
};

/**
  Install CM object for SMBIOS Type 11

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
EFIAPI
InstallSmbiosType11Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo    = Private->Repo;
  VOID                            *DtbBase = Private->DtbBase;
  CM_STD_OEM_STRINGS              *OemStrings;
  UINT32                          NumOemStrings;
  CONST CHAR8                     *PropertyStr;
  INT32                           Length;
  UINTN                           Index;
  INT32                           NodeOffset;
  CHAR8                           **StringList;
  CHAR8                           PropertyName[] = "oem-stringsxxx";

  NumOemStrings = 0;
  StringList    = NULL;

  NodeOffset = fdt_subnode_offset (DtbBase, Private->DtbSmbiosOffset, "type11");
  if (NodeOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Device tree node for SMBIOS Type 11 not found.\n", __FUNCTION__));
    return RETURN_NOT_FOUND;
  }

  OemStrings = (CM_STD_OEM_STRINGS *)AllocateZeroPool (sizeof (CM_STD_OEM_STRINGS));
  if (OemStrings == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for OEM Strings\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
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
      return EFI_OUT_OF_RESOURCES;
    }

    Length++;
    StringList[NumOemStrings] = AllocateZeroPool (sizeof (CHAR8) * Length);
    AsciiSPrint (StringList[NumOemStrings], Length, PropertyStr);

    NumOemStrings++;
  }

  //
  // Add type 11 to SMBIOS table list for test
  //
  OemStrings->StringCount     = NumOemStrings;
  OemStrings->StringTable     = StringList;
  OemStrings->OemStringsToken = REFERENCE_TOKEN (OemStrings[0]);

  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType11,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

  Repo->CmObjectId    = CREATE_CM_STD_OBJECT_ID (EStdObjOemStrings);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_STD_OEM_STRINGS);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = OemStrings;
  Repo++;

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

  return EFI_SUCCESS;
}
