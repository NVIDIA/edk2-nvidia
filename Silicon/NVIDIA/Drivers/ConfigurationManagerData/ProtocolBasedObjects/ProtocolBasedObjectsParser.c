/** @file
  Protocol based objects parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ProtocolBasedObjectsParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/UefiLib.h>
#include <Library/ConfigurationManagerDataLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

/** Protocol based objecsts parser function.

  The following structures are populated:
  - Unknown [whatever is put in the protocol]

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
ProtocolBasedObjectsParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                 Status;
  LEGACY_CM_PROTOCOL_OBJECT  *ToAddList;
  UINTN                      NumberOfProtocols;
  UINTN                      ProtocolIndex;
  LEGACY_CM_PROTOCOL_OBJECT  **ProtocolList;
  CM_OBJ_DESCRIPTOR          Desc;
  CM_OBJECT_TOKEN            InputToken;
  CM_OBJECT_TOKEN            ObjectToken;
  CM_OBJECT_TOKEN            *TokenMap;
  CM_OBJ_DESCRIPTOR          *TokenMapDesc;

  ProtocolList = NULL;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  Status = EfiLocateProtocolBuffer (
             &gNVIDIAConfigurationManagerDataObjectGuid,
             &NumberOfProtocols,
             (VOID ***)&ProtocolList
             );
  if (Status == EFI_NOT_FOUND) {
    Status            =  EFI_SUCCESS;
    NumberOfProtocols = 0;
  } else if (EFI_ERROR (Status)) {
    NumberOfProtocols = 0;
  }

  for (ProtocolIndex = 0; ProtocolIndex < NumberOfProtocols; ProtocolIndex++) {
    ToAddList = ProtocolList[ProtocolIndex];
    if (ToAddList == NULL) {
      continue;
    }

    while (ToAddList->CmObjectPtr != NULL) {
      Desc.ObjectId = ToAddList->CmObjectId;
      Desc.Size     = ToAddList->CmObjectSize;
      Desc.Count    = ToAddList->CmObjectCount;
      Desc.Data     = ToAddList->CmObjectPtr;
      InputToken    = ToAddList->CmObjectToken;

      if (InputToken != CM_NULL_TOKEN) {
        // Add the object to the Repo using the provided token/token map
        if (Desc.Count > 1) {
          if (Desc.ObjectId != CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef)) {
            // Token is the token for the TokenMap, so need to retrieve it
            Status = NvFindEntry (ParserHandle, CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef), InputToken, &TokenMapDesc);
            if (EFI_ERROR (Status)) {
              DEBUG ((DEBUG_ERROR, "%a: Got %r trying to find token 0x%x, used to map the items for ObjectId 0x%x\n", __FUNCTION__, Status, InputToken, Desc.ObjectId));
              goto CleanupAndReturn;
            }

            NV_ASSERT_RETURN (
              TokenMapDesc != NULL,
              { Status = EFI_DEVICE_ERROR;
                goto CleanupAndReturn;
              },
              "%a: CODE_BUG: Got a NULL token map descriptor.\n",
              __FUNCTION__
              );

            if (TokenMapDesc->Count != Desc.Count) {
              DEBUG ((DEBUG_ERROR, "%a: Trying to add %u objects with TokenMap (Token = 0x%x), but the map has %u tokens\n", __FUNCTION__, Desc.Count, InputToken, TokenMapDesc->Count));
              Status = EFI_INVALID_PARAMETER;
              goto CleanupAndReturn;
            }

            TokenMap    = TokenMapDesc->Data;
            ObjectToken = CM_NULL_TOKEN;
          } else {
            // Token is the token to use for storing the ArmObjCmRef object. It has multi-count, but is special with no TokenMap
            ObjectToken = InputToken;
            TokenMap    = NULL;
          }
        } else if (Desc.Count == 1) {
          TokenMap    = NULL;
          ObjectToken = InputToken;
        } else {
          DEBUG ((DEBUG_ERROR, "%a: Found an entry with count of zero; skipping it\n", __FUNCTION__));
          ToAddList++;
          continue;
        }

        Status = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, ObjectToken);
        if (EFI_ERROR (Status)) {
          goto CleanupAndReturn;
        }
      } else {
        // Attempt to extend an existing object with the provided data
        Status = NvExtendCmObj (ParserHandle, &Desc, CM_NULL_TOKEN, NULL);
        if (Status == EFI_NOT_FOUND) {
          // Not found, so attempt to add the provided data as a new object instead
          Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, NULL);
          if (EFI_ERROR (Status)) {
            goto CleanupAndReturn;
          }
        } else if (EFI_ERROR (Status)) {
          goto CleanupAndReturn;
        }
      }

      ToAddList++;
    }
  }

CleanupAndReturn:
  FREE_NON_NULL (ProtocolList);
  return Status;
}

REGISTER_PARSER_FUNCTION (ProtocolBasedObjectsParser, NULL)
