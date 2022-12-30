/** @file

  Provides CompnonentName2 protocol for supported NVIDIA GPUs Controller
  as well as providing the NVIDIA GPU DSD AML Generation Protoocol.

  Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

// ComponentName2.c: Component Name 2 Protocol Handler.
//   Merge legacy ComponentName2Handler.cpp with ComponentName2 C sample code.
//   Only implements ComponentName2 protocol.
//
//////////////////////////////////////////////////////////////////////

#include <Uefi.h>

///
/// Libraries
///
#include <Library/UefiLib.h>

///
/// Protocols
///
#include <Protocol/ComponentName2.h>
#include <Protocol/PciIo.h>

#include "DriverBinding.h"

/* Need a character array to bypass coverity error with non-const literal assignment [cert_str30_c_violation] */
static CHAR8  langEN[] = "en";

GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE  g_efiUnicodeStringTableDriverNames[] =
{
  { (CHAR8 *)"en", (CHAR16 *)L"NVIDIA GPU UEFI Driver" },
  { NULL,          NULL                                }
};

GLOBAL_REMOVE_IF_UNREFERENCED CHAR16  g_efiUnicodeStringTableControllerNamesTemplateArray[] = L"NVIDIA GPU Controller";

// Controller Name will be updated by VBIOS OEM Product name (35 in length)
// Also VBIOS version will be added at the end of controller name.
GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE  g_efiUnicodeStringTableControllerNames[] =
{
  { (CHAR8 *)"en", (CHAR16 *)g_efiUnicodeStringTableControllerNamesTemplateArray },
  { NULL,          NULL                                                          }
};

///
/// Prototypes
///

EFI_STATUS
EFIAPI
ComponentName2GetDriverName (
  IN EFI_COMPONENT_NAME2_PROTOCOL  *This,
  IN CHAR8                         *Language,
  OUT CHAR16                       **DriverName
  );

EFI_STATUS
EFIAPI
ComponentName2GetControllerNameGPU (
  IN EFI_COMPONENT_NAME2_PROTOCOL  *This,
  IN EFI_HANDLE                    ControllerHandle,
  IN EFI_HANDLE                    ChildHandle OPTIONAL,
  IN CHAR8                         *Language,
  OUT CHAR16                       **ControllerName
  );

//
// EFI Component Name2 Protocol
//
EFI_COMPONENT_NAME2_PROTOCOL  gNVIDIAGpuDriverComponentName2Protocol =
{
  ComponentName2GetDriverName,
  ComponentName2GetControllerNameGPU,
  langEN
};

/** Retrieves a Unicode string that is the user readable name of the EFI Driver.

    @param  This        A pointer to the EFI_COMPONENT2_NAME_PROTOCOL instance.
    @param  Language    A pointer to a three character ISO 639-2 language identifier.
                This is the language of the driver name that that the caller
                is requesting, and it must match one of the languages specified
                in SupportedLanguages.  The number of languages supported by a
                driver is up to the driver writer.
    @param  DriverName  A pointer to the Unicode string to return.
                This Unicode string is the name of the driver specified by This
                in the language specified by Language.

    @retval status of request
        EFI_SUCCES            - The Unicode string for the Driver specified by This
                                and the language specified by Language was returned
                                in DriverName.
        EFI_INVALID_PARAMETER - Language is NULL.
        EFI_INVALID_PARAMETER - DriverName is NULL.
        EFI_UNSUPPORTED       - The driver specified by This does not support the
                                language specified by Language.

*/
EFI_STATUS
EFIAPI
ComponentName2GetDriverName (
  IN EFI_COMPONENT_NAME2_PROTOCOL  *This,
  IN CHAR8                         *Language,
  OUT CHAR16                       **DriverName
  )
{
  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           g_efiUnicodeStringTableDriverNames,
           DriverName,
           FALSE
           );
}

/** Retrieves a Unicode string that is the user readable name of the controller
    that is being managed by an EFI Driver (GPU Binding).

    @param  This        A pointer to the EFI_COMPONENT_NAME2_PROTOCOL instance.
    @param  ControllerHandle    The handle of a controller that the driver specified by This is managing.
                This handle specifies the controller whose name is to be returned.
    @param  ChildHandle The handle of the child controller to retrieve the name of.
                This is an optional parameter that may be NULL.  It
                will be NULL for device drivers.  It will also be NULL
                for a bus drivers that wish to retrieve the name of the
                bus controller.  It will not be NULL for a bus driver
                that wishes to retrieve the name of a child controller.
    @param  Language    A pointer to a three character ISO 639-2 language identifier.
                This is the language of the controller name that that the caller
                is requesting, and it must match one of the languages specified
                in SupportedLanguages.  The number of languages supported by a
                driver is up to the driver writer.
    @param  ControllerName  A pointer to the Unicode string to return.
                This Unicode string is the name of the controller specified by
                ControllerHandle and ChildHandle in the language specified
                by Language from the point of view of the driver specified
                by This.

    @retval status of request
        EFI_SUCCESS           - The Unicode string for the user readable name in the
                                language specified by Language for the driver
                                specified by This was returned in DriverName.
        EFI_INVALID_PARAMETER - ControllerHandle is not a valid EFI_HANDLE.
        EFI_INVALID_PARAMETER - ChildHandle is not NULL and it is not a valid EFI_HANDLE.
        EFI_INVALID_PARAMETER - Language is NULL.
        EFI_INVALID_PARAMETER - ControllerName is NULL.
        EFI_UNSUPPORTED       - The driver specified by This is not currently managing
                                the controller specified by ControllerHandle and
                                ChildHandle.
        EFI_UNSUPPORTED       - The driver specified by This does not support the
                                language specified by Language.

*/
EFI_STATUS
EFIAPI
ComponentName2GetControllerNameGPU (
  IN EFI_COMPONENT_NAME2_PROTOCOL  *This,
  IN EFI_HANDLE                    ControllerHandle,
  IN EFI_HANDLE                    ChildHandle OPTIONAL,
  IN CHAR8                         *Language,
  OUT CHAR16                       **ControllerName
  )
{
  EFI_STATUS  efistatus = EFI_SUCCESS;

  if (ControllerHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Language == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (ControllerHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Make sure this driver is currently managing ControllerHandle
  //
  if (gNVIDIAGpuDeviceLibDriverBinding != NULL) {
    efistatus =
      EfiTestManagedDevice (
        ControllerHandle,
        gNVIDIAGpuDeviceLibDriverBinding->DriverBindingHandle,
        &gEfiPciIoProtocolGuid
        );
  }

  if (EFI_ERROR (efistatus)) {
    return efistatus;
  }

  if (ChildHandle != NULL) {
    return EFI_UNSUPPORTED;
  }

  efistatus =
    LookupUnicodeString2 (
      Language,
      This->SupportedLanguages,
      g_efiUnicodeStringTableControllerNames,
      ControllerName,
      (BOOLEAN)(This == &gNVIDIAGpuDriverComponentName2Protocol)
      );

  return efistatus;
}
