/** @file
  The main process for ClockUtil application.

  Copyright (c) 2018-2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

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

#include <Protocol/ArmScmiClock2Protocol.h>
#include <Protocol/ClockParents.h>

//
// Used for ShellCommandLineParseEx only
// and to ensure user inputs are in valid format
//
SHELL_PARAM_ITEM  mClockUtilParamList[] = {
  { L"--id",      TypeValue },
  { L"--name",    TypeValue },
  { L"--freq",    TypeValue },
  { L"--enable",  TypeFlag  },
  { L"--disable", TypeFlag  },
  { L"-?",        TypeFlag  },
  { NULL,         TypeMax   },
};

SCMI_CLOCK2_PROTOCOL           *mClockProtocol;
NVIDIA_CLOCK_PARENTS_PROTOCOL  *mClockParents;
EFI_HII_HANDLE                 mHiiHandle;
CHAR16                         mAppName[] = L"ClockUtil";

/**
  This is function enables, sets frequency, and/or disables specified clock

  @param[in] ClockId        Clock Id of the clock to change.
  @param[in] Enable         Enable the clock
  @param[in] Frequency      Frequency to set the clock to
  @param[in] Disable        Disable the clock

  @retval EFI_SUCCESS       The operation completed successfully.
  @retval others            Error occured

**/
EFI_STATUS
EFIAPI
UpdateClockState (
  IN UINT32   ClockId,
  IN BOOLEAN  Enable,
  IN BOOLEAN  Disable,
  IN UINT64   Frequency
  )
{
  EFI_STATUS  Status;

  if (Enable) {
    Status = mClockProtocol->Enable (mClockProtocol, ClockId, TRUE);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_ENABLE_ERROR), mHiiHandle, mAppName, Status);
      return Status;
    }
  }

  if (Frequency != MAX_UINT64) {
    Status = mClockProtocol->RateSet (mClockProtocol, ClockId, Frequency);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_SET_FREQ_ERROR), mHiiHandle, mAppName, Status);
      return Status;
    }
  }

  if (Disable) {
    Status = mClockProtocol->Enable (mClockProtocol, ClockId, FALSE);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISABLE_ERROR), mHiiHandle, mAppName, Status);
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  This is function displays the clock info for the given clock

  @param[in] ClockId        Clock Id of the clock to display.

**/
VOID
EFIAPI
DisplayClockInfo (
  IN UINT32  ClockId
  )
{
  EFI_STATUS  Status;
  CHAR8       ClockName[SCMI_MAX_STR_LEN];
  BOOLEAN     Enabled;
  UINT32      ParentId;

  Status = mClockProtocol->GetClockAttributes (mClockProtocol, ClockId, &Enabled, ClockName);
  if (EFI_ERROR (Status)) {
    if (Status != EFI_NOT_FOUND) {
      DEBUG ((EFI_D_ERROR, "Failed to get clock attributes - %d: %r\r\n", ClockId, Status));
    }

    return;
  }

  Status = mClockParents->GetParent (mClockParents, ClockId, &ParentId);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to get parent for clock %d\r\n", ClockId));
    ParentId = MAX_UINT32;
  }

  if (Enabled) {
    UINT64  ClockRate;
    Status = mClockProtocol->RateGet (mClockProtocol, ClockId, &ClockRate);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_ENABLED_UNKNOWN), mHiiHandle, ClockId, ClockName, ParentId);
    } else {
      UINT64  MhzPart = ClockRate / 1000000;
      UINT64  KhzPart = (ClockRate % 1000000) / 1000;
      UINT64  HzPart  = ClockRate % 1000;
      if (MhzPart != 0) {
        if ((KhzPart != 0) || (HzPart != 0)) {
          if (HzPart != 0) {
            ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_ENABLED_HZ_MHZ), mHiiHandle, ClockId, ClockName, MhzPart, KhzPart, HzPart, ParentId);
          } else {
            ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_ENABLED_KHZ_MHZ), mHiiHandle, ClockId, ClockName, MhzPart, KhzPart, ParentId);
          }
        } else {
          ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_ENABLED_MHZ), mHiiHandle, ClockId, ClockName, MhzPart, ParentId);
        }
      } else if (KhzPart != 0) {
        if (HzPart != 0) {
          ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_ENABLED_HZ_KHZ), mHiiHandle, ClockId, ClockName, KhzPart, HzPart, ParentId);
        } else {
          ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_ENABLED_KHZ), mHiiHandle, ClockId, ClockName, KhzPart, ParentId);
        }
      } else {
        ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_ENABLED_HZ), mHiiHandle, ClockId, ClockName, HzPart, ParentId);
      }
    }
  } else {
    UINT64  ClockRate;
    Status = mClockProtocol->RateGet (mClockProtocol, ClockId, &ClockRate);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_DISABLED_UNKNOWN), mHiiHandle, ClockId, ClockName, ParentId);
    } else {
      UINT64  MhzPart = ClockRate / 1000000;
      UINT64  KhzPart = (ClockRate % 1000000) / 1000;
      UINT64  HzPart  = ClockRate % 1000;
      if (MhzPart != 0) {
        if ((KhzPart != 0) || (HzPart != 0)) {
          if (HzPart != 0) {
            ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_DISABLED_HZ_MHZ), mHiiHandle, ClockId, ClockName, MhzPart, KhzPart, HzPart, ParentId);
          } else {
            ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_DISABLED_KHZ_MHZ), mHiiHandle, ClockId, ClockName, MhzPart, KhzPart, ParentId);
          }
        } else {
          ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_DISABLED_MHZ), mHiiHandle, ClockId, ClockName, MhzPart, ParentId);
        }
      } else if (KhzPart != 0) {
        if (HzPart != 0) {
          ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_DISABLED_HZ_KHZ), mHiiHandle, ClockId, ClockName, KhzPart, HzPart, ParentId);
        } else {
          ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_DISABLED_KHZ), mHiiHandle, ClockId, ClockName, KhzPart, ParentId);
        }
      } else {
        ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_DISPLAY_DISABLED_HZ), mHiiHandle, ClockId, ClockName, HzPart, ParentId);
      }
    }
  }
}

/**
  This is converts name to id

  @param[in]  ClockName      Name of the clock
  @param[in]  TotalClocks    Total number of clocks
  @param[out] ClockId        Clock Id of the clock that has the specified name.

  @retval     EFI_SUCCESS    Clock Id found
  @retval     EFI_NOT_FOUND  Name was not found in clocks

**/
EFI_STATUS
EFIAPI
GetIdFromName (
  IN  CONST CHAR16  *ClockName,
  IN  UINT32        TotalClocks,
  OUT UINT32        *ClockId
  )
{
  EFI_STATUS  Status;
  UINT32      ClockIndex;
  CHAR8       *AsciiClockName;
  CHAR8       FoundClockName[SCMI_MAX_STR_LEN];
  BOOLEAN     Enabled;

  AsciiClockName = AllocatePool (StrLen (ClockName) + 1);
  if (NULL == AsciiClockName) {
    return EFI_OUT_OF_RESOURCES;
  }

  UnicodeStrToAsciiStrS (ClockName, AsciiClockName, StrLen (ClockName) + 1);

  for (ClockIndex = 0; ClockIndex < TotalClocks; ClockIndex++) {
    Status = mClockProtocol->GetClockAttributes (mClockProtocol, ClockIndex, &Enabled, FoundClockName);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (0 == AsciiStrnCmp (AsciiClockName, FoundClockName, SCMI_MAX_STR_LEN-1)) {
      *ClockId = ClockIndex;
      FreePool (AsciiClockName);
      return EFI_SUCCESS;
    }
  }

  FreePool (AsciiClockName);
  return EFI_NOT_FOUND;
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
InitializeClockUtil (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                   Status;
  LIST_ENTRY                   *ParamPackage;
  CONST CHAR16                 *ValueStr;
  CHAR16                       *ProblemParam;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;

  BOOLEAN  Enable    = FALSE;
  BOOLEAN  Disable   = FALSE;
  UINT64   Frequency = MAX_UINT64;
  UINTN    Value;
  UINT32   ClockId = MAX_UINT32;
  UINT32   TotalClocks;
  UINT32   MinClock;
  UINT32   MaxClock;

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
                           &mHiiHandle
                           );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (mHiiHandle != NULL);

  Status = ShellCommandLineParseEx (mClockUtilParamList, &ParamPackage, &ProblemParam, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_UNKNOWN_OPERATION), mHiiHandle, ProblemParam);
    goto Done;
  }

  Status = gBS->LocateProtocol (&gArmScmiClock2ProtocolGuid, NULL, (VOID **)&mClockProtocol);
  if (EFI_ERROR (Status) || (mClockProtocol == NULL)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_PROTOCOL_NONEXISTENT), mHiiHandle, mAppName);
    goto Done;
  }

  Status = gBS->LocateProtocol (&gNVIDIAClockParentsProtocolGuid, NULL, (VOID **)&mClockParents);
  if (EFI_ERROR (Status) || (mClockParents == NULL)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_PROTOCOL_NONEXISTENT), mHiiHandle, mAppName);
    goto Done;
  }

  Status = mClockProtocol->GetTotalClocks (mClockProtocol, &TotalClocks);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_TOTAL_CLOCKS_ERROR), mHiiHandle, mAppName);
    goto Done;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"-?")) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_HELP), mHiiHandle, mAppName);
    goto Done;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"--enable")) {
    Enable = TRUE;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"--disable")) {
    Disable = TRUE;
  }

  if (Enable && Disable) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_ENABLE_DISABLE), mHiiHandle, mAppName);
    goto Done;
  }

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--id");
  if (NULL != ValueStr) {
    Value = ShellStrToUintn (ValueStr);
    // In case of error on conversion to UINTN value will be (UINTN)-1 which will cause below to be TRUE
    if (Value >= TotalClocks) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_BAD_ID), mHiiHandle, mAppName);
      goto Done;
    }

    ClockId = (UINT32)Value;
  }

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--name");
  if (NULL != ValueStr) {
    if (ClockId != MAX_UINT32) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_NAME_AND_ID), mHiiHandle, mAppName);
      goto Done;
    }

    Status = GetIdFromName (ValueStr, TotalClocks, &ClockId);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_BAD_NAME), mHiiHandle, mAppName);
      goto Done;
    }
  }

  ValueStr = ShellCommandLineGetValue (ParamPackage, L"--freq");
  if (NULL != ValueStr) {
    Value = ShellStrToUintn (ValueStr);
    // In case of error on conversion to UINTN value will be (UINTN)-1 which will cause below to be TRUE
    if (Value == MAX_UINTN) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_BAD_FREQ), mHiiHandle, mAppName);
      goto Done;
    }

    Frequency = Value;
  }

  if ((Enable || Disable || (Frequency != MAX_UINT64))) {
    if (ClockId == MAX_UINT32) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_REQUEST_MODIFY_ALL), mHiiHandle, mAppName);
      goto Done;
    }

    Status = UpdateClockState (ClockId, Enable, Disable, Frequency);
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_CLOCK_UTIL_UPDATE_CLOCK_STATE), mHiiHandle, mAppName);
      goto Done;
    }
  }

  if (ClockId != MAX_UINT32) {
    MinClock = ClockId;
    MaxClock = ClockId;
  } else {
    MinClock = 0;
    MaxClock = TotalClocks;
  }

  for (ClockId = MinClock; ClockId <= MaxClock; ClockId++) {
    DisplayClockInfo (ClockId);
  }

Done:
  ShellCommandLineFreeVarList (ParamPackage);
  HiiRemovePackages (mHiiHandle);

  return EFI_SUCCESS;
}
