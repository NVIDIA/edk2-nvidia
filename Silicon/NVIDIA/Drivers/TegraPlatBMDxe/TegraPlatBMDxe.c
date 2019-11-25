/** @file
*
*  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <libfdt.h>

#include <Protocol/PlatformBootManager.h>

#define NVIDIA_KERNEL_COMMAND_MAX_LEN   25

extern EFI_GUID mBmAutoCreateBootOptionGuid;
EFI_GUID mNVIDIABmBootOptionGuid  = { 0xfaa91113, 0x6cfa, 0x4c14, { 0xad, 0xd7, 0x3e, 0x25, 0x4b, 0x93, 0x38, 0xae } };

CHAR16 Description[] = { L"UEFI NVIDIA L4T" };

CHAR16 KernelCommandRemove[][NVIDIA_KERNEL_COMMAND_MAX_LEN] = {
  L"console="
};

STATIC
EFI_STATUS
GetPlatformCommandLine (
  OUT CHAR16 **CmdLine,
  OUT UINTN  *CmdLen
  )
{
  EFI_STATUS  Status;
  VOID        *DeviceTreeBase;
  UINTN       DeviceTreeSize;
  INT32       NodeOffset;
  CONST CHAR8 *CommandLineEntry;
  INT32       CommandLineLength;
  INT32       CommandLineBytes;
  CHAR16      *CommandLineDT;
  CHAR16      *CommandLine;
  UINTN       FormattedCommandLineLength;
  CHAR16      *CommandLineOptionStart;
  CHAR16      *CommandLineOptionEnd;
  UINTN       CommandLineOptionLength;
  UINTN       CommandLineOptionOffset;
  BOOLEAN     DTBoot;
  UINT32      Count;

  DeviceTreeBase = NULL;
  DTBoot = FALSE;
  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &DeviceTreeBase);
  if (!EFI_ERROR (Status)) {
    DTBoot = TRUE;
  }

  if (!DTBoot) {
    Status = DtPlatformLoadDtb (&DeviceTreeBase, &DeviceTreeSize);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  NodeOffset = fdt_path_offset (DeviceTreeBase, "/chosen");
  if (NodeOffset < 0) {
    return EFI_NOT_FOUND;
  }

  CommandLineEntry = NULL;
  CommandLineEntry = (CONST CHAR8*)fdt_getprop (DeviceTreeBase, NodeOffset, "bootargs", &CommandLineLength);
  if (NULL == CommandLineEntry) {
    return EFI_NOT_FOUND;
  }

  CommandLineBytes = (CommandLineLength * sizeof (CHAR16)) + sizeof (EFI_GUID);

  CommandLineDT = NULL;
  Status = gBS->AllocatePool (EfiBootServicesData,
                              CommandLineBytes,
                              (VOID **)&CommandLineDT);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->SetMem (CommandLineDT, CommandLineBytes, 0);

  AsciiStrToUnicodeStrS (CommandLineEntry, CommandLineDT, CommandLineLength * sizeof (CHAR16));
  gBS->CopyMem ((CHAR8 *)CommandLineDT + (CommandLineLength * sizeof (CHAR16)),
                &mNVIDIABmBootOptionGuid, sizeof (EFI_GUID));
  DEBUG ((DEBUG_INFO, "%a: Kernel Command Line in DT: %s\n", __FUNCTION__, CommandLineDT));

  if (DTBoot) {
    *CmdLine = CommandLineDT;
    *CmdLen = CommandLineBytes;
    return Status;
  }

  CommandLine = NULL;
  Status = gBS->AllocatePool (EfiBootServicesData,
                              CommandLineBytes,
                              (VOID **)&CommandLine);
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  gBS->SetMem (CommandLine, CommandLineBytes, 0);

  gBS->CopyMem (CommandLine, CommandLineDT, CommandLineLength * sizeof (CHAR16));

  for (Count = 0; Count < sizeof (KernelCommandRemove)/sizeof (KernelCommandRemove[0]); Count++) {
    FormattedCommandLineLength = 0;
    FormattedCommandLineLength = StrLen (CommandLine);
    CommandLineOptionStart = NULL;
    CommandLineOptionStart = StrStr (CommandLine, KernelCommandRemove[Count]);
    while (CommandLineOptionStart != NULL) {
      CommandLineOptionEnd = NULL;
      CommandLineOptionEnd = StrStr (CommandLineOptionStart, L" ");
      if (CommandLineOptionEnd == NULL) {
        if (CommandLineOptionStart == CommandLine) {
          gBS->SetMem (CommandLineOptionStart, CommandLineBytes, 0);
        } else {
          CommandLineOptionLength = StrLen (CommandLineOptionStart);
          gBS->SetMem (CommandLineOptionStart, CommandLineOptionLength * sizeof (CHAR16), 0);
        }
        break;
      }
      CommandLineOptionLength = (UINTN)(CommandLineOptionEnd - CommandLineOptionStart) + 1;
      CommandLineOptionOffset = (UINTN)(CommandLineOptionStart - CommandLine);
      gBS->CopyMem (CommandLineOptionStart,
                    CommandLineOptionEnd + 1,
                    (FormattedCommandLineLength - (CommandLineOptionLength + CommandLineOptionOffset) + 1) * sizeof (CHAR16));
      CommandLineOptionStart = StrStr (CommandLineOptionStart, KernelCommandRemove[Count]);
    }
  }
  FormattedCommandLineLength = StrLen (CommandLine);
  gBS->CopyMem ((CHAR8 *)CommandLine + ((FormattedCommandLineLength + 1) * sizeof (CHAR16)),
                &mNVIDIABmBootOptionGuid, sizeof (EFI_GUID));
  DEBUG ((DEBUG_INFO, "%a: Formatted Kernel Command Line: %s\n", __FUNCTION__, CommandLine));

Error:
  gBS->FreePool (CommandLineDT);

  if (!EFI_ERROR (Status)) {
    *CmdLine = CommandLine;
    *CmdLen = CommandLineBytes;
  } else {
    if (CommandLine != NULL) {
      gBS->FreePool (CommandLine);
    }
  }

  return Status;
}

STATIC
EFI_STATUS
GetPlatformNewBootOptions (
  OUT UINTN                        *BootCount,
  OUT EFI_BOOT_MANAGER_LOAD_OPTION **BootOptions,
  OUT EFI_INPUT_KEY                **BootKeys,
  IN  CHAR16                       *CmdLine,
  IN  UINTN                        CmdLen
  )
{
  EFI_STATUS                   Status;
  UINT32                       Size;
  UINTN                        HandleCount;
  EFI_HANDLE                   *Handles;
  UINT32                       Index;
  UINT32                       BootOptionsCount;
  EFI_BOOT_MANAGER_LOAD_OPTION *LoadOptions;
  EFI_DEVICE_PATH_PROTOCOL     *CurrentDevicePath;
  BOOLEAN                      ValidBootMedia;
  UINTN                        LoadCount;
  UINT32                       Count;
  UINT32                       Track;
  UINTN                        MaxSuffixSize;
  UINT16                       *DescriptionString;

  Handles = NULL;
  *BootOptions = NULL;
  *BootKeys = NULL;
  LoadOptions = NULL;
  DescriptionString = NULL;

  HandleCount = 0;
  Status = gBS->LocateHandleBuffer (ByProtocol,
                                    &gEfiLoadFileProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &Handles);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Size = sizeof (EFI_BOOT_MANAGER_LOAD_OPTION) * HandleCount;
  Status = gBS->AllocatePool (EfiBootServicesData,
                              Size,
                              (VOID **)BootOptions);
  if (EFI_ERROR (Status)) {
    goto Error;
  }
  gBS->SetMem (*BootOptions, Size, 0);

  Size = sizeof (EFI_INPUT_KEY) * HandleCount;
  Status = gBS->AllocatePool (EfiBootServicesData,
                              Size,
                              (VOID **)BootKeys);
  if (EFI_ERROR (Status)) {
    goto Error;
  }
  gBS->SetMem (*BootKeys, Size, 0);

  LoadCount = 0;
  LoadOptions = EfiBootManagerGetLoadOptions (&LoadCount,
                                              LoadOptionTypeBoot);

  BootOptionsCount = 0;
  for (Index = 0; Index < HandleCount; Index++) {
    for (Count = 0; Count < LoadCount; Count++) {
      if ((CompareMem (LoadOptions[Count].FilePath,
                       DevicePathFromHandle (Handles[Index]),
                       GetDevicePathSize (LoadOptions[Count].FilePath)) == 0) &&
          ((LoadOptions[Count].Attributes & LOAD_OPTION_ACTIVE) == LOAD_OPTION_ACTIVE) &&
          (LoadOptions[Count].OptionalDataSize == sizeof (EFI_GUID)) &&
          (CompareGuid ((EFI_GUID *)LoadOptions[Count].OptionalData, &mBmAutoCreateBootOptionGuid))) {
        CurrentDevicePath = LoadOptions[Count].FilePath;
        ValidBootMedia = FALSE;
        while (IsDevicePathEnd (CurrentDevicePath) == FALSE) {
          if (CurrentDevicePath->SubType == MSG_EMMC_DP) {
            ValidBootMedia = TRUE;
            break;
          }
          CurrentDevicePath = NextDevicePathNode (CurrentDevicePath);
        }
        if (!ValidBootMedia) {
          continue;
        }
        LoadOptions[Count].Attributes &= ~LOAD_OPTION_ACTIVE;
        LoadOptions[Count].Attributes |= LOAD_OPTION_HIDDEN;
        Status = EfiBootManagerLoadOptionToVariable (&LoadOptions[Count]);
        if (EFI_ERROR (Status)) {
          goto Error;
        }
        DEBUG ((DEBUG_INFO, "%a: Option Marked Hidden: %s\n", __FUNCTION__, LoadOptions[Count].Description));
        MaxSuffixSize = sizeof (CHAR16);
        for (Track = BootOptionsCount; Track != 0; Track = Track / 10) {
          MaxSuffixSize += sizeof (CHAR16);
        }
        Status = gBS->AllocatePool (EfiBootServicesData,
                                    sizeof (Description) + MaxSuffixSize,
                                    (VOID **)&DescriptionString);
        if (EFI_ERROR (Status)) {
          goto Error;
        }
        UnicodeSPrint (DescriptionString, sizeof (Description) + MaxSuffixSize,
                       L"%s %d", Description, BootOptionsCount + 1);
        Status = EfiBootManagerInitializeLoadOption (&(*BootOptions)[BootOptionsCount],
                                                     LoadOptionNumberUnassigned,
                                                     LoadOptionTypeBoot,
                                                     LOAD_OPTION_ACTIVE,
                                                     (BootOptionsCount > 0) ? DescriptionString : Description,
                                                     DevicePathFromHandle (Handles[Index]),
                                                     (UINT8 *)CmdLine,
                                                     CmdLen);
        if (EFI_ERROR (Status)) {
          goto Error;
        }
        gBS->FreePool (DescriptionString);
        DescriptionString = NULL;
        BootOptionsCount++;
      }
    }
  }

Error:
  if (HandleCount != 0) {
    gBS->FreePool (Handles);
  }
  if (EFI_ERROR(Status)) {
    if (*BootOptions != NULL) {
      gBS->FreePool (*BootOptions);
    }
    if (*BootKeys != NULL) {
      gBS->FreePool (*BootKeys);
    }
  } else {
    *BootCount = BootOptionsCount;
  }
  if (LoadCount != 0) {
    EfiBootManagerFreeLoadOptions (LoadOptions, LoadCount);
  }
  if (DescriptionString != NULL) {
    gBS->FreePool (DescriptionString);
  }
  return Status;
}

STATIC
EFI_STATUS
UpdatePlatformBootOptions (
  IN  CHAR16 *CmdLine,
  IN  UINTN  CmdLen
  )
{
  EFI_STATUS                   Status;
  EFI_BOOT_MANAGER_LOAD_OPTION *LoadOptions;
  EFI_BOOT_MANAGER_LOAD_OPTION *NewLoadOption;
  UINTN                        LoadCount;
  UINT32                       Count;
  UINTN                        OldCmdLen;

  LoadCount = 0;
  LoadOptions = NULL;
  LoadOptions = EfiBootManagerGetLoadOptions (&LoadCount,
                                              LoadOptionTypeBoot);

  for (Count = 0; Count < LoadCount; Count++) {
    if (LoadOptions[Count].OptionalDataSize == 0) {
      continue;
    }
    OldCmdLen = StrLen ((CHAR16 *)LoadOptions[Count].OptionalData);
    if (LoadOptions[Count].OptionalDataSize >= ((OldCmdLen + 1) * sizeof (CHAR16)) + sizeof (EFI_GUID) &&
        CompareGuid ((EFI_GUID *)(LoadOptions[Count].OptionalData + ((OldCmdLen + 1) * sizeof (CHAR16))),
                     &mNVIDIABmBootOptionGuid)) {
      if (StrnCmp ((CHAR16 *)LoadOptions[Count].OptionalData,
                   CmdLine,
                   LoadOptions[Count].OptionalDataSize <= CmdLen ? LoadOptions[Count].OptionalDataSize : CmdLen) != 0) {
        DEBUG ((DEBUG_INFO, "%a: Option Needs Update: %s\n", __FUNCTION__, LoadOptions[Count].Description));
        NewLoadOption = NULL;
        Status = gBS->AllocatePool (EfiBootServicesData,
                                    sizeof (EFI_BOOT_MANAGER_LOAD_OPTION),
                                    (VOID **)&NewLoadOption);
        if (EFI_ERROR (Status)) {
          goto Error;
        }

        gBS->SetMem (NewLoadOption, sizeof (EFI_BOOT_MANAGER_LOAD_OPTION), 0);
        gBS->CopyMem (NewLoadOption, &LoadOptions[Count], sizeof (EFI_BOOT_MANAGER_LOAD_OPTION));

        NewLoadOption->OptionalDataSize = CmdLen;
        NewLoadOption->OptionalData = (UINT8 *)CmdLine;

        Status = EfiBootManagerLoadOptionToVariable (NewLoadOption);
        gBS->FreePool(NewLoadOption);
        if (EFI_ERROR (Status)) {
          goto Error;
        }
      }
    }
  }

Error:
  if (LoadCount != 0) {
    EfiBootManagerFreeLoadOptions (LoadOptions, LoadCount);
  }

  return Status;
}

STATIC
EFI_STATUS
GetPlatformBootOptions (
  OUT UINTN                              *BootCount,
  OUT EFI_BOOT_MANAGER_LOAD_OPTION       **BootOptions,
  OUT EFI_INPUT_KEY                      **BootKeys
  )
{
  EFI_STATUS                   Status;
  CHAR16                       *CmdLine;
  UINTN                        CmdLen;

  Status = GetPlatformCommandLine (&CmdLine, &CmdLen);
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  Status = GetPlatformNewBootOptions (BootCount, BootOptions, BootKeys, CmdLine, CmdLen);
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  Status = UpdatePlatformBootOptions (CmdLine, CmdLen);
  if (EFI_ERROR (Status)) {
    goto Error;
  }

Error:
  if (CmdLine != NULL) {
    gBS->FreePool (CmdLine);
  }

  return Status;
}

PLATFORM_BOOT_MANAGER_PROTOCOL mPlatformBootManager = {
  GetPlatformBootOptions
};

EFI_STATUS
EFIAPI
PlatformBootManagerEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  return gBS->InstallProtocolInterface (&ImageHandle,
                                        &gPlatformBootManagerProtocolGuid,
                                        EFI_NATIVE_INTERFACE,
                                        &mPlatformBootManager);
}
