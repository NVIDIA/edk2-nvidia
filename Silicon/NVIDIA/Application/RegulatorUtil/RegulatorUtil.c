/** @file
  The main process for RegulatorUtil application.

  Copyright (c) 2018, 2020, NVIDIA CORPORATION. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2018-2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2018-2020 NVIDIA CORPORATION & AFFILIATES
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

#include <Protocol/Regulator.h>

//
// Used for ShellCommandLineParseEx only
// and to ensure user inputs are in valid format
//
SHELL_PARAM_ITEM    mRegulatorUtilParamList[] = {
  { L"--id",                  TypeValue },
  { L"--name",                TypeValue },
  { L"--enable",              TypeFlag },
  { L"--disable",             TypeFlag  },
  { L"--voltage",             TypeValue  },
  { L"-?",                    TypeFlag  },
  { NULL,                     TypeMax   },
};

NVIDIA_REGULATOR_PROTOCOL    *mRegulator;
EFI_HII_HANDLE               mHiiHandle;
CHAR16                       mAppName[]          = L"RegulatorUtil";

/**
  This is function displays the regulator info for the given regulator

  @param[in] Regulator        Regulator Id to display.

**/
VOID
EFIAPI
DisplayRegulatorInfo (
  IN UINT32  RegulatorId
  )
{
  EFI_STATUS         Status;
  REGULATOR_INFO     Info;

  Status = mRegulator->GetInfo (mRegulator, RegulatorId, &Info);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_DISPLAY_GET_INFO_ERROR), mHiiHandle, mAppName, RegulatorId, Status);
    return;
  }
  if (Info.IsAvailable) {
    if (Info.AlwaysEnabled) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_DISPLAY_ALWAYS_ON_INFO),
                       mHiiHandle,
                       RegulatorId,
                       Info.Name,
                       Info.CurrentMicrovolts,
                       Info.MinMicrovolts,
                       Info.MaxMicrovolts,
                       Info.MicrovoltStep
                       );
    } else {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_DISPLAY_INFO),
                       mHiiHandle,
                       RegulatorId,
                       Info.Name,
                       Info.IsEnabled,
                       Info.CurrentMicrovolts,
                       Info.MinMicrovolts,
                       Info.MaxMicrovolts,
                       Info.MicrovoltStep
                       );
    }
  } else {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_DISPLAY_NOT_READY),
                     mHiiHandle,
                     RegulatorId,
                     Info.Name
                     );
  }
  return;
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
InitializeRegulatorUtil (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE    *SystemTable
  )
{
  EFI_STATUS                    Status;
  LIST_ENTRY                    *ParamPackage;
  CONST CHAR16                  *ValueStr;
  CHAR16                        *ProblemParam;
  EFI_HII_PACKAGE_LIST_HEADER   *PackageList;

  BOOLEAN                       Enable  = FALSE;
  BOOLEAN                       Disable = FALSE;
  UINTN                         Microvolts = MAX_UINTN;
  UINT32                        RegulatorId = MAX_UINT32;
  UINTN                         RegulatorCount;
  UINTN                         RegulatorIndex;
  UINT32                        *RegulatorArray = NULL;

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

  Status = ShellCommandLineParseEx (mRegulatorUtilParamList, &ParamPackage, &ProblemParam, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_UNKNOWN_OPERATION), mHiiHandle, ProblemParam);
    goto Done;
  }

  Status = gBS->LocateProtocol (&gNVIDIARegulatorProtocolGuid, NULL, (VOID **) &mRegulator);
  if (EFI_ERROR (Status) || mRegulator == NULL) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_PROTOCOL_NONEXISTENT), mHiiHandle, mAppName);
    goto Done;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"-?")) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_HELP), mHiiHandle, mAppName);
    goto Done;
  }

  Enable = ShellCommandLineGetFlag (ParamPackage, L"--enable");
  Disable = ShellCommandLineGetFlag (ParamPackage, L"--disable");

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--voltage");
  if (NULL != ValueStr) {
    Microvolts = ShellStrToUintn (ValueStr);
    if (Microvolts == MAX_UINTN) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_BAD_VOLTAGE), mHiiHandle, mAppName);
      goto Done;
    }
  }

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--id");
  if (NULL != ValueStr) {
    RegulatorId = (UINT32)ShellStrToUintn (ValueStr);
  }

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--name");
  if (NULL != ValueStr) {
    CHAR8      *AsciiRegulatorName;
    AsciiRegulatorName = AllocatePool (StrLen (ValueStr) + 1);
    if (NULL == AsciiRegulatorName) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_BAD_ALLOCATION), mHiiHandle, mAppName);
      goto Done;
    }
    UnicodeStrToAsciiStrS (ValueStr, AsciiRegulatorName, StrLen (ValueStr) + 1);
    Status = mRegulator->GetIdFromName (mRegulator, AsciiRegulatorName, &RegulatorId);
    FreePool (AsciiRegulatorName);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_NAME_LOOKUP_FAIL), mHiiHandle, mAppName, Status);
      goto Done;
    }
  }

  if (Enable && Disable) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_ENABLE_DISABLE), mHiiHandle, mAppName);
    goto Done;
  }

  if ((Enable || Disable || (Microvolts != MAX_UINTN)) && (RegulatorId == MAX_UINT32)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_MODIFY_NO_ID), mHiiHandle, mAppName);
    goto Done;
  }

  if (Disable) {
    Status = mRegulator->Enable (mRegulator, RegulatorId, FALSE);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_DISABLE_ERROR), mHiiHandle, mAppName, RegulatorId, Status);
      goto Done;
    }
  }

  if (Microvolts != MAX_UINTN) {
    Status = mRegulator->SetVoltage (mRegulator, RegulatorId, Microvolts);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_SET_VOLTAGE_ERROR), mHiiHandle, mAppName, RegulatorId, Microvolts, Status);
      goto Done;
    }
  }

  if (Enable) {
    Status = mRegulator->Enable (mRegulator, RegulatorId, TRUE);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_ENABLE_ERROR), mHiiHandle, mAppName, RegulatorId, Status);
      goto Done;
    }
  }

  if (RegulatorId == MAX_UINT32) {
    UINTN BufferSize = 0;
    Status = mRegulator->GetRegulators (mRegulator, &BufferSize, NULL);
    if (EFI_BUFFER_TOO_SMALL != Status) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_LIST_LOOKUP_ERROR), mHiiHandle, mAppName, Status);
      goto Done;
    }

    RegulatorArray = (UINT32 *)AllocatePool (BufferSize);
    if (NULL == RegulatorArray) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_BAD_ALLOCATION), mHiiHandle, mAppName);
      goto Done;
    }

    Status = mRegulator->GetRegulators (mRegulator, &BufferSize, RegulatorArray);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_REGULATOR_UTIL_LIST_LOOKUP_ERROR), mHiiHandle, mAppName, Status);
      goto Done;
    }
    RegulatorCount = BufferSize / sizeof (UINT32);

  } else {
    RegulatorArray = &RegulatorId;
    RegulatorCount = 1;
  }

  for (RegulatorIndex = 0; RegulatorIndex < RegulatorCount; RegulatorIndex++) {
    DisplayRegulatorInfo (RegulatorArray[RegulatorIndex]);
  }

Done:
  if ((RegulatorArray != NULL) &&
      (RegulatorArray != &RegulatorId)) {
    FreePool (RegulatorArray);
  }
  ShellCommandLineFreeVarList (ParamPackage);
  HiiRemovePackages (mHiiHandle);

  return EFI_SUCCESS;
}
