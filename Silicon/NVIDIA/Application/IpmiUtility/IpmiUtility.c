/** @file
  The main process for IPMI utility application.

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/HiiLib.h>
#include <Library/IpmiBaseLib.h>

//
// Used for ShellCommandLineParseEx only
// and to ensure user inputs are in valid format
//
SHELL_PARAM_ITEM  mIpmiUtilityParamList[] = {
  { L"-help", TypeFlag },
  { L"-?",    TypeFlag },
  { NULL,     TypeMax  },
};

EFI_HII_HANDLE  mHiiHandle;
CHAR16          mAppName[] = L"IpmiUtility";

#define IPMI_UTILITY_RETURN_BUFFER_SIZE  128

/**
  Dump the byte data in Buffer.

  @param[in]  ErrorLevel        Error output level
  @param[in]  Buffer            Buffer to dump
  @param[in]  Length            Buffer length in byte.

  @retval EFI_SUCCESS           Buffer dump successfully.
  @retval Others                Error occurs.

**/
VOID
DumpBuffer (
  UINT8  *Buffer,
  UINTN  BufferSize
  )
{
  UINTN  Index;

  if ((Buffer == NULL) || (BufferSize == 0)) {
    ShellPrintEx (-1, -1, L"No data\n");
    return;
  }

  for (Index = 0; Index < BufferSize; Index++) {
    ShellPrintEx (-1, -1, L"%02X ", Buffer[Index]);
    if ((Index + 1) % 16 == 0) {
      ShellPrintEx (-1, -1, L"\n");
    } else if ((Index + 1) % 4 == 0) {
      ShellPrintEx (-1, -1, L" ");
    }
  }

  ShellPrintEx (-1, -1, L"\n");
}

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
InitializeIpmiUtility (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS    Status;
  LIST_ENTRY    *ParamPackage;
  CHAR16        *ProblemParam;
  UINTN         ParameterSize;
  UINT8         NetFunction;
  UINT8         Command;
  CONST CHAR16  *Buffer;
  UINT8         *InputData;
  UINT32        InputDataSize;
  UINT8         *OutputData;
  UINT32        OutputDataSize;
  UINTN         Index;

  ParameterSize  = 0;
  Buffer         = NULL;
  InputData      = NULL;
  InputDataSize  = 0;
  OutputData     = NULL;
  OutputDataSize = 0;

  //
  // Publish HII package list to HII Database.
  //
  mHiiHandle = InitializeHiiPackage (ImageHandle);
  ASSERT (mHiiHandle != NULL);

  Status = ShellCommandLineParseEx (mIpmiUtilityParamList, &ParamPackage, &ProblemParam, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_IPMI_UTILITY_UNKNOWN_OPERATION), mHiiHandle, ProblemParam);
    goto Done;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"-help") || ShellCommandLineGetFlag (ParamPackage, L"-?")) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_IPMI_UTILITY_HELP), mHiiHandle, mAppName);
    goto Done;
  }

  ParameterSize = ShellCommandLineGetCount (ParamPackage);
  if (ParameterSize < 2) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_IPMI_UTILITY_INVALID_PARAMETER), mHiiHandle, mAppName);
    goto Done;
  }

  //
  // NetFn
  //
  Buffer = ShellCommandLineGetRawValue (ParamPackage, 1);
  if ((Buffer == NULL) || !ShellIsHexOrDecimalNumber (Buffer, TRUE, TRUE)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_IPMI_UTILITY_INVALID_FORMAT), mHiiHandle, mAppName);
    goto Done;
  }

  NetFunction = (UINT8)ShellHexStrToUintn (Buffer);

  //
  // Command
  //
  Buffer = ShellCommandLineGetRawValue (ParamPackage, 2);
  if ((Buffer == NULL) || !ShellIsHexOrDecimalNumber (Buffer, TRUE, TRUE)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_IPMI_UTILITY_INVALID_FORMAT), mHiiHandle, mAppName);
    goto Done;
  }

  Command = (UINT8)ShellHexStrToUintn (Buffer);

  //
  // Input data
  //
  InputDataSize = ParameterSize - 3;
  if (InputDataSize > 0) {
    InputData = AllocateZeroPool (sizeof (UINT8) * InputDataSize);
    if (InputData == NULL) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_IPMI_UTILITY_OUT_OF_RESOURCE), mHiiHandle, mAppName);
      goto Done;
    }

    for (Index = 0; Index < InputDataSize; Index++) {
      Buffer = ShellCommandLineGetRawValue (ParamPackage, Index + 3);
      if ((Buffer == NULL) || !ShellIsHexOrDecimalNumber (Buffer, TRUE, TRUE)) {
        ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_IPMI_UTILITY_INVALID_FORMAT), mHiiHandle, mAppName);
        goto Done;
      }

      InputData[Index] = (UINT8)ShellHexStrToUintn (Buffer);
    }
  }

  //
  // Output data
  //
  OutputDataSize = IPMI_UTILITY_RETURN_BUFFER_SIZE;
  if (OutputDataSize > 0) {
    OutputData = AllocateZeroPool (sizeof (UINT8) * OutputDataSize);
    if (OutputData == NULL) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_IPMI_UTILITY_OUT_OF_RESOURCE), mHiiHandle, mAppName);
      goto Done;
    }
  }

  //
  // Display input data
  //
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_IPMI_UTILITY_INPUT_DATA), mHiiHandle, NetFunction, Command, InputDataSize);
  DumpBuffer (InputData, InputDataSize);

  Status = IpmiSubmitCommand (
             NetFunction,
             Command,
             InputData,
             InputDataSize,
             OutputData,
             &OutputDataSize
             );
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_IPMI_UTILITY_CMD_FAILED), mHiiHandle, Status);
    goto Done;
  }

  //
  // Display output data
  //
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_IPMI_UTILITY_OUTPUT_DATA), mHiiHandle, OutputDataSize);
  DumpBuffer (OutputData, OutputDataSize);

Done:

  if (InputData != NULL) {
    FreePool (InputData);
  }

  if (OutputData != NULL) {
    FreePool (OutputData);
  }

  ShellCommandLineFreeVarList (ParamPackage);
  HiiRemovePackages (mHiiHandle);

  return EFI_SUCCESS;
}
