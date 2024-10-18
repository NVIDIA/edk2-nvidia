/** @file
  Configuration Manager Get

  SPDX-FileCopyrightText: Copyright (c) 2019 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/
#include <ConfigurationManagerObject.h>

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/ConfigurationManagerDataLib.h>

#include <Protocol/ConfigurationManagerProtocol.h>

/** The GetObject function defines the interface implemented by the
    Configuration Manager Protocol for returning the Configuration
    Manager Objects.

  @param [in]      This        Pointer to the Configuration Manager Protocol.
  @param [in]      CmObjectId  The Configuration Manager Object ID.
  @param [in]      Token       An optional token identifying the object. If
                               unused this must be CM_NULL_TOKEN.
  @param [in, out] CmObject    Pointer to the Configuration Manager Object
                               descriptor describing the requested Object.

  @retval EFI_SUCCESS           Success.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_NOT_FOUND         The required object information is not found.
**/
EFI_STATUS
EFIAPI
NVIDIAPlatformGetObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EFI_STATUS                            Status;
  EDKII_PLATFORM_REPOSITORY_INFO        *PlatRepoInfo;
  UINTN                                 ElemIndex;
  UINTN                                 ElemOffset;
  UINTN                                 ElemSize;
  EDKII_PLATFORM_REPOSITORY_INFO_ENTRY  *Entry;

  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  PlatRepoInfo = This->PlatRepoInfo;
  ASSERT (PlatRepoInfo != NULL);

  Status = PlatRepoInfo->FindEntry (PlatRepoInfo, CmObjectId, Token, &Entry);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CopyMem (CmObject, &Entry->CmObjectDesc, sizeof (Entry->CmObjectDesc));

  // If the user specified an element token, we want just a single entry
  if ((Entry->CmObjectDesc.Count > 1) &&
      (Token != CM_NULL_TOKEN) &&
      (Token != Entry->Token) &&
      (Entry->ElementTokenMap != NULL))
  {
    ElemSize = CmObject->Size / CmObject->Count;
    for (ElemIndex = 0; ElemIndex < Entry->CmObjectDesc.Count; ElemIndex++) {
      if (Token == Entry->ElementTokenMap[ElemIndex]) {
        break;
      }
    }

    ASSERT (ElemIndex < Entry->CmObjectDesc.Count);
    ElemOffset = ElemIndex * ElemSize;

    if (!(ElemOffset < CmObject->Size)) {
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: Out-of-bounds CmObject array access: ID = %x, Token = %x, Size = %d, Count = %d\n",
        CmObjectId,
        Token,
        CmObject->Size,
        CmObject->Count
        ));
      return EFI_INVALID_PARAMETER;
    }

    CmObject->Data  = (UINT8 *)CmObject->Data + ElemOffset;
    CmObject->Size  = ElemSize;
    CmObject->Count = 1;
  }

  DEBUG ((
    DEBUG_INFO,
    "CmObject: ID = %x, Token = %x, Data = 0x%p, Size = %d, Count = %d\n",
    CmObjectId,
    Token,
    CmObject->Data,
    CmObject->Size,
    CmObject->Count
    ));
  return EFI_SUCCESS;
}
