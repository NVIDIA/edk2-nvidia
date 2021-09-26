/** @file
  Configuration Manager Dxe

  Copyright (c) 2019 - 2021, NVIDIA Corporation. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2019-2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019-2020 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/
#include <ConfigurationManagerObject.h>

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/ConfigurationManagerDataProtocol.h>
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
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  * CONST This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     * CONST CmObject
  )
{
  CONST EDKII_PLATFORM_REPOSITORY_INFO   * PlatRepoInfo;
  UINT32                                   Index;
  UINT32                                   ElemOffset;
  UINT32                                   ElemSize;

  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  PlatRepoInfo = This->PlatRepoInfo;
  ASSERT (PlatRepoInfo != NULL);

  for (Index = 0; Index < PcdGet32 (PcdConfigMgrObjMax); Index++) {
    // If CmObjectPtr is NULL, we have reached the end of valid
    // entries, so stop looking.
    if (PlatRepoInfo[Index].CmObjectPtr == NULL) {
      break;
    }
    // CmObjectId must match, otherwise this entry is irrelevant and
    // we must keep looking.
    if (PlatRepoInfo[Index].CmObjectId != CmObjectId) {
      continue;
    }
    // If this entry has a non-null CmObjectToken, the user-supplied
    // token must match it exactly.
    if (PlatRepoInfo[Index].CmObjectToken != CM_NULL_TOKEN) {
      // If the user supplied a null Token and this entry has a
      // matching CmObjectId and a non-null CmObjectToken, we will
      // never find anything. If there is a single object with a
      // particular CmObjectId and non-null CmObjectToken, then all
      // objects sharing that CmObjectId must have a non-null (and
      // unique) CmObjectTokens as well. Therefore, if the
      // user-supplied a null Token, stop looking; otherwise, keep
      // looking for an entry with a matching CmObjectToken.
      if (Token == CM_NULL_TOKEN) {
        break;
      } else if (Token != PlatRepoInfo[Index].CmObjectToken) {
        continue;
      }
    }

    CmObject->ObjectId = CmObjectId;
    CmObject->Data = PlatRepoInfo[Index].CmObjectPtr;
    CmObject->Size = PlatRepoInfo[Index].CmObjectSize;
    CmObject->Count = PlatRepoInfo[Index].CmObjectCount;

    // If CmObjectId matches and the entry has no CmObjectToken, but
    // the user supplied a non-null Token, this is an array access;
    // instead of returning all the objects, return the single
    // requested element.
    if (PlatRepoInfo[Index].CmObjectToken == CM_NULL_TOKEN
        && Token != CM_NULL_TOKEN) {
      ElemOffset = Token - (CM_OBJECT_TOKEN)CmObject->Data;
      ElemSize = CmObject->Size / CmObject->Count;

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
      } else if (ElemOffset % ElemSize != 0) {
        DEBUG ((
          DEBUG_ERROR,
          "ERROR: Misaligned CmObject array access: ID = %x, Token = %x, Size = %d, Count = %d\n",
          CmObjectId,
          Token,
          CmObject->Size,
          CmObject->Count
        ));
        return EFI_INVALID_PARAMETER;
      }

      CmObject->Data = (UINT8*)CmObject->Data + ElemOffset;
      CmObject->Size = ElemSize;
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

  DEBUG ((
    DEBUG_ERROR,
    "ERROR: Not Found CmObject = 0x%x\n",
    CmObjectId
  ));
  return EFI_NOT_FOUND;
}

/** The SetObject function defines the interface implemented by the
    Configuration Manager Protocol for updating the Configuration
    Manager Objects.

  @param [in]      This        Pointer to the Configuration Manager Protocol.
  @param [in]      CmObjectId  The Configuration Manager Object ID.
  @param [in]      Token       An optional token identifying the object. If
                               unused this must be CM_NULL_TOKEN.
  @param [in]      CmObject    Pointer to the Configuration Manager Object
                               descriptor describing the Object.

  @retval EFI_UNSUPPORTED  This operation is not supported.
**/
EFI_STATUS
EFIAPI
NVIDIAPlatformSetObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  * CONST This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN        CM_OBJ_DESCRIPTOR                     * CONST CmObject
  )
{
  return EFI_UNSUPPORTED;
}

/** A structure describing the configuration manager protocol interface.
*/
STATIC
EDKII_CONFIGURATION_MANAGER_PROTOCOL NVIDIAPlatformConfigManagerProtocol = {
  CREATE_REVISION (1, 0),
  NVIDIAPlatformGetObject,
  NVIDIAPlatformSetObject,
  NULL
};

/**
  Entrypoint of Configuration Manager Dxe.

  @param  ImageHandle
  @param  SystemTable

  @return EFI_SUCCESS
  @return EFI_LOAD_ERROR
  @return EFI_OUT_OF_RESOURCES

**/
EFI_STATUS
EFIAPI
ConfigurationManagerDxeInitialize (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE  * SystemTable
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO   * PlatRepoInfo;
  EFI_STATUS                         Status;

  Status = gBS->LocateProtocol (
                  &gNVIDIAConfigurationManagerDataProtocolGuid,
                  NULL,
                  (VOID**)&PlatRepoInfo
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: Failed to get NVIDIA Configuration Manager Data Protocol." \
      " Status = %r\n",
      Status
      ));
    goto error_handler;
  }

  NVIDIAPlatformConfigManagerProtocol.PlatRepoInfo = PlatRepoInfo;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEdkiiConfigurationManagerProtocolGuid,
                  (VOID*)&NVIDIAPlatformConfigManagerProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: Failed to Install Configuration Manager Protocol." \
      " Status = %r\n",
      Status
      ));
    goto error_handler;
  }

error_handler:
  return Status;
}
