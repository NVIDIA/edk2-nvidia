/** @file

  Shell application HiiKeywordUtil.

  This application is used to set and get Hii Keyword information for the platform.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/ShellLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/HiiConfigKeyword.h>

//
// Used for ShellCommandLineParseEx only
// and to ensure user inputs are in valid format
//
SHELL_PARAM_ITEM  mClockUtilParamList[] = {
  { L"--format",    TypeValue },
  { L"--namespace", TypeValue },
  { NULL,           TypeMax   },
};

VOID
EFIAPI
PrintUsage (
  VOID
  )
{
  Print (L"Usage: HiiKeywordUtil [Options] [Keyword] [Value]\n");
  Print (L"  --format <string,hex,dec,auto>  Format of the value\n");
  Print (L"                                  If not specified auto will be used\n");
  Print (L"                                  auto will try to determine the format\n");
  Print (L"                                  checks if cleanly converts to decimal, then hex, and then assumes a string\n");
  Print (L"  --namespace <namespace>         Namespace to use\n");
  Print (L"                                  If not specified all x-UEFI namespaces are returned\n");
  Print (L"  Keyword                         Keyword to get or set\n");
  Print (L"                                  If no keyword is specified, all keywords are listed\n");
  Print (L"  Value                           Value to set\n");
  Print (L"                                  If no value is specified, current value is returned\n");
  Print (L"\n");
  Print (L"Examples:\n");
  Print (L"  HiiKeywordUtil MyKeyword                         - Gets the value of MyKeyword\n");
  Print (L"  HiiKeywordUtil MyKeyword string                  - Sets the value of MyKeyword to string\n");
  Print (L"  HiiKeywordUtil MyKeyword 0x1234                  - Sets the value of MyKeyword to 0x1234 as a hex value\n");
  Print (L"  HiiKeywordUtil --format string MyKeyword 0x1234  - Sets the value of MyKeyword to \"0x1234\" as a string\n");
}

INTN
EFIAPI
ShellAppMain (
  IN UINTN   Argc,
  IN CHAR16  **Argv
  )
{
  EFI_STATUS                           Status;
  INTN                                 ReturnCode;
  LIST_ENTRY                           *ParamPackage;
  CHAR16                               *ProblemParam;
  EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL  *KeywordHandler;
  CHAR16                               *Keyword;
  CONST CHAR16                         *KeywordName;
  CONST CHAR16                         *KeywordValue;
  CONST CHAR16                         *FormatString;
  CONST CHAR16                         *NamespaceParam;
  UINT8                                *CurrentValueBuffer;
  CHAR16                               *ValueString;
  CHAR16                               *CurrentValueString;
  VOID                                 *ValueBuffer;
  UINTN                                ValueLength;
  UINT32                               ProgressErr;
  CHAR16                               MultiKeywordReq[256]; // Adjust size as needed
  CHAR16                               *MultiKeywordResp;
  BOOLEAN                              HexFlag;
  BOOLEAN                              DecimalFlag;
  BOOLEAN                              StringFlag;
  BOOLEAN                              AutoFlag;
  UINTN                                NumericValue;
  CHAR16                               *EndPointer;
  UINT8                                *SourceBuffer;
  UINTN                                SourceSize;
  CHAR16                               *StringStart;
  CHAR16                               *StringEnd;
  UINTN                                Index;
  CHAR16                               ByteValue[3];
  UINTN                                ParamCount;
  CHAR16                               NamespaceString[256];
  CHAR16                               *NamespaceArg;

  MultiKeywordResp = NULL;
  ReturnCode       = 0;
  Status           = ShellCommandLineParseEx (mClockUtilParamList, &ParamPackage, &ProblemParam, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    Print (L"Error: Invalid parameter: %s\n", ProblemParam);
    PrintUsage ();
    ReturnCode = -1;
    goto Done;
  }

  ParamCount = ShellCommandLineGetCount (ParamPackage);
  if (ParamCount > 3) {
    Print (L"Error: too many arguments\n");
    PrintUsage ();
    ReturnCode = -1;
    goto Done;
  }

  KeywordName    = ShellCommandLineGetRawValue (ParamPackage, 1);
  KeywordValue   = ShellCommandLineGetRawValue (ParamPackage, 2);
  FormatString   = ShellCommandLineGetValue (ParamPackage, L"--format");
  NamespaceParam = ShellCommandLineGetValue (ParamPackage, L"--namespace");

  AutoFlag    = FALSE;
  StringFlag  = FALSE;
  HexFlag     = FALSE;
  DecimalFlag = FALSE;
  if ((FormatString == NULL) || (StrCmp (FormatString, L"auto") == 0)) {
    AutoFlag = TRUE;
  } else if (StrCmp (FormatString, L"string") == 0) {
    StringFlag = TRUE;
  } else if (StrCmp (FormatString, L"hex") == 0) {
    HexFlag = TRUE;
  } else if (StrCmp (FormatString, L"dec") == 0) {
    DecimalFlag = TRUE;
  } else {
    Print (L"Error: Invalid format\n");
    PrintUsage ();
    ReturnCode = -1;
    goto Done;
  }

  NamespaceArg = NULL;
  if (NamespaceParam != NULL) {
    UnicodeSPrint (NamespaceString, sizeof (NamespaceString), L"NAMESPACE=%s", NamespaceParam);
    NamespaceArg = NamespaceString;
  }

  // Locate the protocol
  Status = gBS->LocateProtocol (&gEfiConfigKeywordHandlerProtocolGuid, NULL, (VOID **)&KeywordHandler);
  if (EFI_ERROR (Status)) {
    Print (L"Unable to locate Config Keyword Handler Protocol\n");
    ReturnCode = -1;
    goto Done;
  }

  if (KeywordValue == NULL) {
    if (KeywordName != NULL) {
      // Construct the MultiKeywordResp string
      UnicodeSPrint (MultiKeywordReq, sizeof (MultiKeywordReq), L"KEYWORD=%s", KeywordName);

      // Get the keyword value
      Status = KeywordHandler->GetData (KeywordHandler, NamespaceArg, MultiKeywordReq, &Keyword, &ProgressErr, &MultiKeywordResp);
      if (EFI_ERROR (Status)) {
        Print (L"Error getting keyword value: %r %x\n", Status, ProgressErr);
        ReturnCode = -1;
        goto Done;
      }
    } else {
      Status = KeywordHandler->GetData (KeywordHandler, NamespaceArg, NULL, &Keyword, &ProgressErr, &MultiKeywordResp);
      if (EFI_ERROR (Status)) {
        Print (L"Error getting all keyword values: %r %x\n", Status, ProgressErr);
        ReturnCode = -1;
        goto Done;
      }
    }

    StringStart = MultiKeywordResp;
    while (StringStart != NULL) {
      StringStart = StrStr (StringStart, L"KEYWORD=");
      if (StringStart == NULL) {
        break;
      }

      StringStart += 8; // Skip past "KEYWORD="

      StringEnd = StrStr (StringStart, L"&");
      if (StringEnd != NULL) {
        *StringEnd = '\0';
      } else {
        Print (L"Error: no parameters after keyword\n");
        break;
      }

      KeywordName = StringStart;
      StringStart = StringEnd+1;
      StringStart = StrStr (StringStart, L"VALUE=");
      if (StringStart == NULL) {
        Print (L"Error: keyword %s has no value\n", KeywordName);
        break;
      }

      StringStart += 6; // Skip past "VALUE="

      StringEnd = StrStr (StringStart, L"&");
      if (StringEnd != NULL) {
        *StringEnd = '\0';
      }

      KeywordValue = StringStart;
      ValueLength  = StrLen (KeywordValue);
      ValueBuffer  = AllocateZeroPool (MAX (ValueLength/2, 8) * sizeof (UINT8));
      if (ValueBuffer == NULL) {
        Print (L"Error: Unable to allocate memory for keyword value\n");
        ReturnCode = -1;
        goto Done;
      }

      if (AutoFlag) {
        if ((KeywordValue[0] != L'0') &&
            (KeywordValue[1] != L'0'))
        {
          // No NUL terminator, assume value
          HexFlag    = TRUE;
          StringFlag = FALSE;
        } else if ((ValueLength == 2) || (ValueLength == 4) || (ValueLength == 8) || (ValueLength == 16)) {
          // Size is 1, 2, 4, or 8 bytes, assume value
          HexFlag    = TRUE;
          StringFlag = FALSE;
        } else {
          StringFlag = TRUE;
        }
      }

      CurrentValueBuffer = ValueBuffer;
      // Format is byte string in reverse order.
      while (ValueLength >= 2) {
        ByteValue[0]        = KeywordValue[ValueLength-2];
        ByteValue[1]        = KeywordValue[ValueLength-1];
        ByteValue[2]        = '\0';
        *CurrentValueBuffer = (UINT8)StrHexToUintn (ByteValue);
        CurrentValueBuffer++;
        ValueLength -= 2;
      }

      if (StringFlag) {
        Print (L"Keyword: %s=\"%s\"\n", KeywordName, (CHAR16 *)ValueBuffer);
      } else if (HexFlag) {
        Print (L"Keyword: %s=0x%llx\n", KeywordName, *(UINTN *)ValueBuffer);
      } else if (DecimalFlag) {
        Print (L"Keyword: %s=%u\n", KeywordName, *(UINTN *)ValueBuffer);
      }

      FreePool (ValueBuffer);

      if (StringEnd != NULL) {
        StringStart = StringEnd+1;
      } else {
        break;
      }
    }
  } else {
    if (KeywordName == NULL) {
      Print (L"Error: Must specify a keyword to set\n");
      ReturnCode = -1;
      goto Done;
    }

    if (KeywordValue == NULL) {
      Print (L"Error: Must specify a value to set\n");
      ReturnCode = -1;
      goto Done;
    }

    // Construct the MultiKeywordRequest string
    UnicodeSPrint (MultiKeywordReq, sizeof (MultiKeywordReq), L"KEYWORD=%s", KeywordName);

    // Get the MultiKeywordResp string
    Status = KeywordHandler->GetData (KeywordHandler, NamespaceArg, MultiKeywordReq, &Keyword, &ProgressErr, &MultiKeywordResp);
    if (EFI_ERROR (Status)) {
      Print (L"Error getting MultiKeywordResp: %r %x, %s\n", Status, ProgressErr, Keyword);
      ReturnCode = -1;
      goto Done;
    }

    // Extract the path from the MultiKeywordResp string
    // This assumes that the path is the first component of the string
    ValueString = StrStr (MultiKeywordResp, L"VALUE=");
    if (ValueString != NULL) {
      ValueString += 6; // Skip past "VALUE="
    } else {
      Print (L"Error extracting value from MultiKeywordResp\n");
      ReturnCode = -1;
      goto Done;
    }

    ValueLength = StrLen (ValueString);

    ValueLength        = 0;
    CurrentValueString = ValueString;
    while ((*CurrentValueString != L'\0') && (*CurrentValueString != L'&')) {
      *CurrentValueString = L'0';
      ValueLength++;
      CurrentValueString++;
    }

    if (AutoFlag) {
      Status = StrDecimalToUintnS (KeywordValue, &EndPointer, &NumericValue);
      if (EFI_ERROR (Status) || (*EndPointer != L'\0')) {
        Status = StrHexToUintnS (KeywordValue, &EndPointer, &NumericValue);
        if (EFI_ERROR (Status) || (*EndPointer != L'\0')) {
          SourceBuffer = (UINT8 *)KeywordValue;
          SourceSize   = StrSize (KeywordValue);
        } else {
          SourceBuffer = (UINT8 *)&NumericValue;
          SourceSize   = sizeof (UINTN);
        }
      } else {
        SourceBuffer = (UINT8 *)&NumericValue;
        SourceSize   = sizeof (UINTN);
      }
    } else if (HexFlag) {
      NumericValue = (UINTN)StrHexToUintn (KeywordValue);
      SourceBuffer = (UINT8 *)&NumericValue;
      SourceSize   = sizeof (UINTN);
    } else if (DecimalFlag) {
      NumericValue = (UINTN)StrDecimalToUintn (KeywordValue);
      SourceBuffer = (UINT8 *)&NumericValue;
      SourceSize   = sizeof (UINTN);
    } else {
      SourceBuffer = (UINT8 *)KeywordValue;
      SourceSize   = StrSize (KeywordValue);
    }

    // Set the keyword value
    CurrentValueString = ValueString;
    if (SourceSize*2 > ValueLength) {
      if (StringFlag) {
        Print (L"Error: Not enough room to store string value\n");
        ReturnCode = -1;
        goto Done;
      } else {
        for (Index = (ValueLength/2); Index < SourceSize; Index++) {
          if (SourceBuffer[Index] != 0) {
            Print (L"Error: Not enough room to store value\n");
            ReturnCode = -1;
            goto Done;
          }
        }
      }

      SourceSize = ValueLength/2;
    } else {
      CurrentValueString += (ValueLength-(SourceSize*2));
    }

    // Format is byte string in reverse order.
    for (Index = 0; Index < SourceSize; Index++) {
      UnicodeValueToStringS (
        CurrentValueString,
        3 * sizeof (CHAR16),
        PREFIX_ZERO | RADIX_HEX,
        SourceBuffer[SourceSize-Index-1],
        2
        );
      CurrentValueString += 2;
    }

    Status = KeywordHandler->SetData (KeywordHandler, MultiKeywordResp, &Keyword, &ProgressErr);
    if (EFI_ERROR (Status)) {
      Print (L"Error setting keyword value: %r %x %s\n", Status, ProgressErr, Keyword);
      ReturnCode = -1;
      goto Done;
    }

    Print (L"Keyword value set successfully\n");
  }

Done:
  SHELL_FREE_NON_NULL (MultiKeywordResp);

  return ReturnCode;
}
