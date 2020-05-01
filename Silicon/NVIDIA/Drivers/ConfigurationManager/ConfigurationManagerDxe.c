/** @file
  Configuration Manager Dxe

  Copyright (c) 2019 - 2020, NVIDIA Corporation. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/
#include <ConfigurationManagerObject.h>

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <Protocol/ConfigurationManagerProtocol.h>

STATIC
EDKII_PLATFORM_REPOSITORY_INFO *mNVIDIAPlatformRepositoryInfo;

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
  EFI_STATUS  Status;
  UINT32      Index;
  BOOLEAN     DataFound;

  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  DataFound = FALSE;

  for (Index = 0; Index < EStdObjMax + EArmObjMax; Index++) {
    if (mNVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }

    if (mNVIDIAPlatformRepositoryInfo[Index].CmObjectId == CmObjectId) {
      DataFound = TRUE;
      break;
    }
  }

  if (DataFound) {
    Status = EFI_SUCCESS;
    CmObject->Size = mNVIDIAPlatformRepositoryInfo[Index].CmObjectSize;
    CmObject->Data = mNVIDIAPlatformRepositoryInfo[Index].CmObjectPtr;
    CmObject->ObjectId = mNVIDIAPlatformRepositoryInfo[Index].CmObjectId;
    CmObject->Count = mNVIDIAPlatformRepositoryInfo[Index].CmObjectCount;
    DEBUG ((
      DEBUG_INFO,
      "CmObject: ID = %d, Ptr = 0x%p, Size = %d, Count = %d\n",
      CmObject->ObjectId,
      CmObject->Data,
      CmObject->Size,
      CmObject->Count
      ));
  } else {
    Status = EFI_NOT_FOUND;
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: Not Found CmObject = 0x%x. Status = %r\n",
      CmObjectId,
      Status
      ));
  }

  return Status;
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
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (
                  &gNVIDIAConfigurationManagerDataProtocolGuid,
                  NULL,
                  (VOID**)&mNVIDIAPlatformRepositoryInfo
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

  NVIDIAPlatformConfigManagerProtocol.PlatRepoInfo = mNVIDIAPlatformRepositoryInfo;

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
