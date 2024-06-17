/** @file
  TestInjectAssert

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/ResetSystemLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/HiiLib.h>

//
// Used for ShellCommandLineParseEx only
// and to ensure user inputs are in valid format
//
SHELL_PARAM_ITEM  TestInjectAssertParamList[] = {
  { L"--swassert",  TypeFlag },
  { L"--exception", TypeFlag },
  { L"-?",          TypeFlag },
  { NULL,           TypeMax  },
};

STATIC CHAR16          AppName[] = L"TestInjectAssert";
STATIC EFI_HII_HANDLE  HiiHandle;

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers, including
  both device drivers and bus drivers.

  The entry point for StackCheck application that should casue an abort due to stack overwrite.

  @param[in] ImageHandle    The image handle of this application.
  @param[in] SystemTable    The pointer to the EFI System Table.

  @retval EFI_SUCCESS    The operation completed successfully.

**/
EFI_STATUS
EFIAPI
TestInjectAssert (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  LIST_ENTRY                   *ParamPackage;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;
  CHAR16                       *ProblemParam;
  BOOLEAN                      SwAssertInject;
  BOOLEAN                      ExceptionInject;
  EFI_STATUS                   Status;
  UINT8                        *TestPtr;

  SwAssertInject  = FALSE;
  ExceptionInject = FALSE;
  TestPtr         = NULL;

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
  if (EFI_ERROR (Status)) {
    return Status;
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
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (HiiHandle != NULL);

  Status = ShellCommandLineParseEx (TestInjectAssertParamList, &ParamPackage, &ProblemParam, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TEST_INJECT_ASSERT_UNKNOWN), HiiHandle, ProblemParam);
    goto TestInjectAssertDone;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"-?")) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TEST_INJECT_ASSERT_HELP), HiiHandle, AppName);
    goto TestInjectAssertDone;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"--swassert")) {
    SwAssertInject = TRUE;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"--exception")) {
    ExceptionInject = TRUE;
  }

  if (SwAssertInject && ExceptionInject) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_TEST_INJECT_ASSERT_EXCEPTION), HiiHandle, AppName);
    goto TestInjectAssertDone;
  }

  if (SwAssertInject == TRUE) {
    ErrorPrint (L"%a: INJECTING AN ASSERT \r\n", __FUNCTION__);
    InValidateActiveBootChain ();
    ASSERT (FALSE);
  } else if (ExceptionInject == TRUE) {
    InValidateActiveBootChain ();
    *TestPtr = 8;
  }

TestInjectAssertDone:
  ShellCommandLineFreeVarList (ParamPackage);
  HiiRemovePackages (HiiHandle);

  return EFI_SUCCESS;
}
