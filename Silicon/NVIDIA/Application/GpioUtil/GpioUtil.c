/** @file
  The main process for GpioUtil application.

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

#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/HiiLib.h>

#include <Protocol/EmbeddedGpio.h>

//
// Used for ShellCommandLineParseEx only
// and to ensure user inputs are in valid format
//
SHELL_PARAM_ITEM    mClockUtilParamList[] = {
  { L"--id",                  TypeValue },
  { L"--output",              TypeValue },
  { L"--input",               TypeFlag  },
  { L"-?",                    TypeFlag  },
  { NULL,                     TypeMax   },
};

PLATFORM_GPIO_CONTROLLER     *mGpioController;
EMBEDDED_GPIO                *mGpioProtocol;
EFI_HII_HANDLE               mHiiHandle;
CHAR16                       mAppName[]          = L"GpioUtil";

/**
  This is function displays the gpio info for the given pin

  @param[in] Gpio        Gpio Id of the pin to display.

**/
VOID
EFIAPI
DisplayGpioInfo (
  IN EMBEDDED_GPIO_PIN  Gpio
  )
{
  EFI_STATUS         Status;
  EMBEDDED_GPIO_MODE Mode;
  UINTN              Value;

  Status = mGpioProtocol->Get (mGpioProtocol, Gpio, &Value);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GPIO_UTIL_DISPLAY_GET_VALUE_ERROR), mHiiHandle, mAppName, Gpio, Status);
    return;
  }
  Status = mGpioProtocol->GetMode (mGpioProtocol, Gpio, &Mode);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GPIO_UTIL_DISPLAY_GET_MODE_ERROR), mHiiHandle, mAppName, Gpio, Status);
    return;
  }

  switch (Mode) {
  case GPIO_MODE_INPUT:
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GPIO_UTIL_DISPLAY_INPUT), mHiiHandle, Gpio, Value);
    return;

  case GPIO_MODE_OUTPUT_0:
  case GPIO_MODE_OUTPUT_1:
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GPIO_UTIL_DISPLAY_OUTPUT), mHiiHandle, Gpio, Value);
    return;

  default:
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GPIO_UTIL_DISPLAY_UNKNOWN_MODE), mHiiHandle, Gpio, Value);
    return;
  }
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
InitializeGpioUtil (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE    *SystemTable
  )
{
  EFI_STATUS                    Status;
  LIST_ENTRY                    *ParamPackage;
  CONST CHAR16                  *ValueStr;
  CHAR16                        *ProblemParam;
  EFI_HII_PACKAGE_LIST_HEADER   *PackageList;

  BOOLEAN                       Input  = FALSE;
  BOOLEAN                       Output = FALSE;
  EMBEDDED_GPIO_MODE            Mode;
  EMBEDDED_GPIO_PIN             Gpio = MAX_UINT64;
  UINTN                         ControllerIndex;

  //
  // Retrieve HII package list from ImageHandle
  //
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiHiiPackageListProtocolGuid,
                  (VOID **) &PackageList,
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
                          &mHiiHandle
                          );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (mHiiHandle != NULL);

  Status = ShellCommandLineParseEx (mClockUtilParamList, &ParamPackage, &ProblemParam, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GPIO_UTIL_UNKNOWN_OPERATION), mHiiHandle, ProblemParam);
    goto Done;
  }

  Status = gBS->LocateProtocol (&gEmbeddedGpioProtocolGuid, NULL, (VOID **) &mGpioProtocol);
  if (EFI_ERROR (Status) || mGpioProtocol == NULL) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GPIO_UTIL_PROTOCOL_NONEXISTENT), mHiiHandle, mAppName);
    goto Done;
  }

  Status = gBS->LocateProtocol (&gPlatformGpioProtocolGuid, NULL, (VOID **) &mGpioController);
  if (EFI_ERROR (Status) || mGpioController == NULL) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GPIO_UTIL_PLATFORM_PROTOCOL_NONEXISTENT), mHiiHandle, mAppName);
    goto Done;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"-?")) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GPIO_UTIL_HELP), mHiiHandle, mAppName);
    goto Done;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"--input")) {
    Input = TRUE;
    Mode = GPIO_MODE_INPUT;
  }

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--output");
  if (NULL != ValueStr) {
    UINTN Value = ShellStrToUintn (ValueStr);
    if (Input) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GPIO_UTIL_INPUT_OUTPUT), mHiiHandle, mAppName);
      goto Done;
    }
    if (Value == 0) {
      Output = TRUE;
      Mode = GPIO_MODE_OUTPUT_0;
    } else if (Value == 1) {
      Output = TRUE;
      Mode = GPIO_MODE_OUTPUT_1;
    } else {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GPIO_UTIL_BAD_OUTPUT_VALUE), mHiiHandle, mAppName);
      goto Done;
    }
  }

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--id");
  if (NULL != ValueStr) {
    Gpio = ShellStrToUintn (ValueStr);
  }

  if (Input || Output) {
    if (Gpio == MAX_UINT64) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GPIO_UTIL_MODIFY_NO_ID), mHiiHandle, mAppName);
      goto Done;
    }
    Status = mGpioProtocol->Set (mGpioProtocol, Gpio, Mode);
  }

  if (Gpio != MAX_UINT64) {
    DisplayGpioInfo (Gpio);
  } else {
    for (ControllerIndex = 0; ControllerIndex < mGpioController->GpioControllerCount; ControllerIndex++) {
      UINTN GpioIndex;
      for (GpioIndex = 0;GpioIndex < mGpioController->GpioController[ControllerIndex].InternalGpioCount; GpioIndex++) {
        DisplayGpioInfo (mGpioController->GpioController[ControllerIndex].GpioIndex + GpioIndex);
      }
    }
  }

Done:
  ShellCommandLineFreeVarList (ParamPackage);
  HiiRemovePackages (mHiiHandle);

  return EFI_SUCCESS;
}
