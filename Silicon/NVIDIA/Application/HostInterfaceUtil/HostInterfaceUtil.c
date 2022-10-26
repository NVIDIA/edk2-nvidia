/** @file
  The main process for HostInterfaceUtil application.

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Protocol/EdkIIRedfishCredential.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/HiiLib.h>
#include <Library/RedfishCredentialLib.h>

//
// Used for ShellCommandLineParseEx only
// and to ensure user inputs are in valid format
//
SHELL_PARAM_ITEM  mHostInterfaceUtilParamList[] = {
  { L"-disable", TypeFlag },
  { L"-help",    TypeFlag },
  { NULL,        TypeMax  },
};

EFI_HII_HANDLE  mHiiHandle;
CHAR16          mAppName[] = L"HostInterfaceUtil";

/**
  Retrieve HII package list from ImageHandle and publish to HII database.

  @param ImageHandle            The image handle of the process.

  @return HII handle.
**/
EFI_HII_HANDLE
InitializeHiiPackage (
  EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS                   Status;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;
  EFI_HII_HANDLE               HiiHandle;

  //
  // Retrieve HII package list from ImageHandle
  //
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiHiiPackageListProtocolGuid,
                  (VOID **)&PackageList,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  //
  // Publish HII package list to HII Database.
  //
  Status = gHiiDatabase->NewPackageList (
                           gHiiDatabase,
                           PackageList,
                           NULL,
                           &HiiHandle
                           );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return HiiHandle;
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers, including
  both device drivers and bus drivers.

  The entry point for ClockUtil application that parse the command line input and call an SCMI Clock command.

  @param[in] ImageHandle    The image handle of this application.
  @param[in] SystemTable    The pointer to the EFI System Table.

  @retval EFI_SUCCESS    The operation completed successfully.

**/
EFI_STATUS
EFIAPI
InitializeHostInterfaceUtil (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                         Status;
  LIST_ENTRY                         *ParamPackage;
  CHAR16                             *ProblemParam;
  BOOLEAN                            DisableBootstrapService;
  EDKII_REDFISH_CREDENTIAL_PROTOCOL  *RedfishCredential;
  EDKII_REDFISH_AUTH_METHOD          AuthMethod;
  CHAR8                              *Username;
  CHAR8                              *Password;

  DisableBootstrapService = FALSE;
  Username                = NULL;
  Password                = NULL;

  //
  // Publish HII package list to HII Database.
  //
  mHiiHandle = InitializeHiiPackage (ImageHandle);
  ASSERT (mHiiHandle != NULL);

  Status = ShellCommandLineParseEx (mHostInterfaceUtilParamList, &ParamPackage, &ProblemParam, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_HOST_INTERFACE_UTIL_UNKNOWN_OPERATION), mHiiHandle, ProblemParam);
    goto Done;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"-help")) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_HOST_INTERFACE_UTIL_HELP), mHiiHandle, mAppName);
    goto Done;
  }

  DisableBootstrapService = ShellCommandLineGetFlag (ParamPackage, L"-disable");

  //
  // Check and see if Redfish credential protocol is ready or not
  //
  Status = gBS->LocateProtocol (
                  &gEdkIIRedfishCredentialProtocolGuid,
                  NULL,
                  (VOID **)&RedfishCredential
                  );
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_HOST_INTERFACE_UTIL_PROTOCOL_NOT_READY), mHiiHandle, Status);
    goto Done;
  }

  //
  // Disable credential service is requested
  //
  if (DisableBootstrapService) {
    Status = LibStopRedfishService (RedfishCredential, ServiceStopTypeExitBootService);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_HOST_INTERFACE_UTIL_STOP_FAILED), mHiiHandle, Status);
      goto Done;
    }

    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_HOST_INTERFACE_UTIL_STOP_SUCCESS), mHiiHandle);
  }

  //
  // Get credential from BMC
  //
  Status = LibCredentialGetAuthInfo (RedfishCredential, &AuthMethod, &Username, &Password);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_HOST_INTERFACE_UTIL_GET_CREDENTIAL_FAILED), mHiiHandle, Status);
    goto Done;
  }

  //
  // Display credentail
  //
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_HOST_INTERFACE_UTIL_SHOW_USERNAME), mHiiHandle, Username);
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_HOST_INTERFACE_UTIL_SHOW_PASSWD), mHiiHandle, Password);

Done:

  //
  // Release resource
  //
  if (Username != NULL) {
    FreePool (Username);
  }

  if (Password != NULL) {
    FreePool (Password);
  }

  ShellCommandLineFreeVarList (ParamPackage);
  HiiRemovePackages (mHiiHandle);

  return EFI_SUCCESS;
}
