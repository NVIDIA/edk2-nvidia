/** @file
  UEFI Component Name(2) protocol implementation for BPMP IPC driver.

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2018 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2018 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#include "BpmpIpcDxePrivate.h"


//
/// Driver Name Strings
///
GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE mBpmpIpcDriverNameTable[] = {
  {
    "eng;en",
    (CHAR16 *)L"NVIDIA BPMP-FW IPC Driver"
  },
  {
    NULL,
    NULL
  }
};

///
/// Controller Name Strings
///
GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE mBpmpControllerNameTable[] = {
  {
    "eng;en",
    (CHAR16 *)L"NVIDIA BPMP Controller"
  },
  {
    NULL,
    NULL
  }
};

GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE mHspTopControllerNameTable[] = {
  {
    "eng;en",
    (CHAR16 *)L"NVIDIA HSP Controller"
  },
  {
    NULL,
    NULL
  }
};

/**
  Retrieves a Unicode string that is the user readable name of the UEFI Driver.

  @param This           A pointer to the EFI_COMPONENT_NAME_PROTOCOL instance.
  @param Language       A pointer to a three character ISO 639-2 language identifier.
                        This is the language of the driver name that that the caller
                        is requesting, and it must match one of the languages specified
                        in SupportedLanguages.  The number of languages supported by a
                        driver is up to the driver writer.
  @param DriverName     A pointer to the Unicode string to return.  This Unicode string
                        is the name of the driver specified by This in the language
                        specified by Language.

  @retval EFI_SUCCESS           The Unicode string for the Driver specified by This
                                and the language specified by Language was returned
                                in DriverName.
  @retval EFI_INVALID_PARAMETER Language is NULL.
  @retval EFI_INVALID_PARAMETER DriverName is NULL.
  @retval EFI_UNSUPPORTED       The driver specified by This does not support the
                                language specified by Language.
**/
EFI_STATUS
EFIAPI
BpmpIpcComponentNameGetDriverName (
  IN EFI_COMPONENT_NAME_PROTOCOL    *This,
  IN CHAR8                          *Language,
  OUT CHAR16                        **DriverName
  )
{
  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           mBpmpIpcDriverNameTable,
           DriverName,
           (BOOLEAN)(This == &gBpmpIpcComponentName)
           );
}

/**
  Retrieves a Unicode string that is the user readable name of the controller
  that is being managed by an UEFI Driver.

  @param This                   A pointer to the EFI_COMPONENT_NAME_PROTOCOL instance.
  @param ControllerHandle       The handle of a controller that the driver specified by
                                This is managing.  This handle specifies the controller
                                whose name is to be returned.
  @param ChildHandle OPTIONAL   The handle of the child controller to retrieve the name
                                of.  This is an optional parameter that may be NULL.  It
                                will be NULL for device drivers.  It will also be NULL
                                for a bus drivers that wish to retrieve the name of the
                                bus controller.  It will not be NULL for a bus driver
                                that wishes to retrieve the name of a child controller.
  @param Language               A pointer to a three character ISO 639-2 language
                                identifier.  This is the language of the controller name
                                that that the caller is requesting, and it must match one
                                of the languages specified in SupportedLanguages.  The
                                number of languages supported by a driver is up to the
                                driver writer.
  @param ControllerName         A pointer to the Unicode string to return.  This Unicode
                                string is the name of the controller specified by
                                ControllerHandle and ChildHandle in the language
                                specified by Language from the point of view of the
                                driver specified by This.

  @retval EFI_SUCCESS           The Unicode string for the user readable name in the
                                language specified by Language for the driver
                                specified by This was returned in DriverName.
  @retval EFI_INVALID_PARAMETER ControllerHandle is not a valid EFI_HANDLE.
  @retval EFI_INVALID_PARAMETER ChildHandle is not NULL and it is not a valid
                                EFI_HANDLE.
  @retval EFI_INVALID_PARAMETER Language is NULL.
  @retval EFI_INVALID_PARAMETER ControllerName is NULL.
  @retval EFI_UNSUPPORTED       The driver specified by This is not currently
                                managing the controller specified by
                                ControllerHandle and ChildHandle.
  @retval EFI_UNSUPPORTED       The driver specified by This does not support the
                                language specified by Language.
**/
EFI_STATUS
EFIAPI
BpmpIpcComponentNameGetControllerName (
  IN EFI_COMPONENT_NAME_PROTOCOL    *This,
  IN EFI_HANDLE                     ControllerHandle,
  IN EFI_HANDLE                     ChildHandle OPTIONAL,
  IN CHAR8                          *Language,
  OUT CHAR16                        **ControllerName
  )
{
  EFI_STATUS               Status;
  NON_DISCOVERABLE_DEVICE  *NonDiscoverableProtocol = NULL;
  EFI_UNICODE_STRING_TABLE *StringTable = NULL;

  //
  // Make sure this driver is currently managing ControllHandle
  //
  Status = EfiTestManagedDevice (
             ControllerHandle,
             gBpmpIpcDriverBinding.DriverBindingHandle,
             &gNVIDIANonDiscoverableDeviceProtocolGuid
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (ChildHandle != NULL) {
    return EFI_UNSUPPORTED;
  }

  Status = gBS->LocateProtocol (
                  ControllerHandle,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **)&NonDiscoverableProtocol
                  );
  if (EFI_ERROR (Status)) {
    //This should never be possible due to EfiTestManagedDeviceCall
    return EFI_UNSUPPORTED;
  }

  if (CompareGuid (NonDiscoverableProtocol->Type, &gNVIDIANonDiscoverableBpmpDeviceGuid)) {
    StringTable = mBpmpControllerNameTable;
  } else if (CompareGuid (NonDiscoverableProtocol->Type, &gNVIDIANonDiscoverableHspTopDeviceGuid)) {
    StringTable = mHspTopControllerNameTable;
  } else {
    return EFI_UNSUPPORTED;
  }

  return LookupUnicodeString2 (
          Language,
          This->SupportedLanguages,
          StringTable,
          ControllerName,
          (BOOLEAN)(This == &gBpmpIpcComponentName)
          );
}


//
/// EFI Component Name Protocol
///
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME_PROTOCOL  gBpmpIpcComponentName = {
  BpmpIpcComponentNameGetDriverName,
  BpmpIpcComponentNameGetControllerName,
  "eng"
};

//
/// EFI Component Name 2 Protocol
///
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME2_PROTOCOL gBpmpIpcComponentName2 = {
  (EFI_COMPONENT_NAME2_GET_DRIVER_NAME) BpmpIpcComponentNameGetDriverName,
  (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME) BpmpIpcComponentNameGetControllerName,
  "en"
};
