/** @file
*
*  Copyright (c) 2019-2020, NVIDIA CORPORATION. All rights reserved.
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

CHAR16 KernelCommandRemove[][NVIDIA_KERNEL_COMMAND_MAX_LEN] = {
  L"console="
};

/*
  Duplicates boot option. The caller is supposed to allocate buffer for destination boot
  option.

  @param[in] DestinationLoadOption Destination boot option buffer.

  @param[in] SourceLoadOption      Source boot option buffer.

  @retval EFI_SUCCESS              Load option duplicated successfully.

  @retval EFI_OUT_OF_RESOURCES     Memory allocation failed.

  @retval EFI_INVALID_PARAMETER    Input is not correct.
*/
STATIC
EFI_STATUS
DuplicateLoadOption (
  OUT EFI_BOOT_MANAGER_LOAD_OPTION *DestinationLoadOption,
  IN  EFI_BOOT_MANAGER_LOAD_OPTION *SourceLoadOption
  )
{
  EFI_STATUS Status;

  if (DestinationLoadOption == NULL ||
      SourceLoadOption == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  gBS->CopyMem (DestinationLoadOption, SourceLoadOption, sizeof (EFI_BOOT_MANAGER_LOAD_OPTION));

  Status = gBS->AllocatePool (EfiBootServicesData,
                              (StrLen (SourceLoadOption->Description) + 1) * sizeof (CHAR16),
                              (VOID **)&DestinationLoadOption->Description);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->CopyMem (DestinationLoadOption->Description, SourceLoadOption->Description, (StrLen (SourceLoadOption->Description) + 1) * sizeof (CHAR16));

  DestinationLoadOption->FilePath = DuplicateDevicePath (SourceLoadOption->FilePath);

  Status = gBS->AllocatePool (EfiBootServicesData,
                              SourceLoadOption->OptionalDataSize,
                              (VOID **)&DestinationLoadOption->OptionalData);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->CopyMem (DestinationLoadOption->OptionalData, SourceLoadOption->OptionalData, SourceLoadOption->OptionalDataSize);

  Status = gBS->AllocatePool (EfiBootServicesData,
                              SourceLoadOption->ExitDataSize,
                              (VOID **)&DestinationLoadOption->ExitData);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->CopyMem (DestinationLoadOption->ExitData, SourceLoadOption->ExitData, SourceLoadOption->ExitDataSize);

  return Status;
}

/*
  Get the kernel command line and its length to be patched in load option.

  @param[out] DestinationLoadOption Destination boot option buffer.

  @param[out] SourceLoadOption      Source boot option buffer.

  @retval EFI_SUCCESS              Command line and its length generated correctly.

  @retval EFI_OUT_OF_RESOURCES     Memory allocation failed.
*/
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
  VOID        *AcpiBase;
  UINT32      Count;

  DeviceTreeBase = NULL;
  DTBoot = FALSE;
  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (EFI_ERROR (Status)) {
    DTBoot = TRUE;
  }

  if (DTBoot) {
    Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &DeviceTreeBase);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  } else {
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
                &gNVIDIABmBootOptionGuid, sizeof (EFI_GUID));
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
                &gNVIDIABmBootOptionGuid, sizeof (EFI_GUID));
  DEBUG ((DEBUG_INFO, "%a: Formatted Kernel Command Line: %s\n", __FUNCTION__, CommandLine));

Error:
  gBS->FreePool (CommandLineDT);

  if (!EFI_ERROR (Status)) {
    *CmdLine = CommandLine;
    *CmdLen = ((FormattedCommandLineLength + 1) * sizeof (CHAR16)) + sizeof (EFI_GUID);
  } else {
    if (CommandLine != NULL) {
      gBS->FreePool (CommandLine);
    }
  }

  return Status;
}

/*
  This function parses the input auto enumerated boot options and makes platform specific
  customizations to it. This function needs to allocate new boot options buffer that has the
  customized list.

  @param[in const] BootOptions             An array of auto enumerated platform boot options.
                                           This array will be freed by caller upon successful
                                           exit of this function and output array would be used.

  @param[in const] BootOptionsCount        The number of elements in BootOptions.

  @param[out]      UpdatedBootOptions      An array of boot options that have been customized
                                           for the platform on top of input boot options. This
                                           array would be allocated by REFRESH_ALL_BOOT_OPTIONS
                                           and would be freed by caller after consuming it.

  @param[out]      UpdatedBootOptionsCount The number of elements in UpdatedBootOptions.


  @retval EFI_SUCCESS                      Platform refresh to input BootOptions and
                                           BootCount have been done.

  @retval EFI_OUT_OF_RESOURCES             Memory allocation failed.

  @retval EFI_INVALID_PARAMETER            Input is not correct.

  @retval EFI_UNSUPPORTED                  Platform specific overrides are not supported.
*/
STATIC
EFI_STATUS
RefreshAutoEnumeratedBootOptions (
  IN  CONST EFI_BOOT_MANAGER_LOAD_OPTION *BootOptions,
  IN  CONST UINTN                        BootOptionsCount,
  OUT       EFI_BOOT_MANAGER_LOAD_OPTION **UpdatedBootOptions,
  OUT       UINTN                        *UpdatedBootOptionsCount
  )
{
  EFI_STATUS                   Status;
  CHAR16                       *CmdLine;
  UINTN                        CmdLen;
  EFI_HANDLE                   *Handles;
  UINTN                        HandleCount;
  EFI_BOOT_MANAGER_LOAD_OPTION *LoadOption;
  EFI_BOOT_MANAGER_LOAD_OPTION *UpdatedLoadOption;
  UINTN                        Index;
  UINTN                        Count;
  EFI_DEVICE_PATH_PROTOCOL     *CurrentDevicePath;
  BOOLEAN                      ValidBootMedia;

  if (BootOptions == NULL ||
      BootOptionsCount == 0 ||
      UpdatedBootOptions == NULL ||
      UpdatedBootOptionsCount == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CmdLine = NULL;
  Handles = NULL;

  CmdLen = 0;
  Status = GetPlatformCommandLine (&CmdLine, &CmdLen);
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  HandleCount = 0;
  Status = gBS->LocateHandleBuffer (ByProtocol,
                                    &gEfiLoadFileProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &Handles);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->AllocatePool (EfiBootServicesData,
                              BootOptionsCount * sizeof (EFI_BOOT_MANAGER_LOAD_OPTION),
                              (VOID **)UpdatedBootOptions);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->SetMem (*UpdatedBootOptions, BootOptionsCount * sizeof (EFI_BOOT_MANAGER_LOAD_OPTION), 0);

  *UpdatedBootOptionsCount = BootOptionsCount;
  LoadOption = (EFI_BOOT_MANAGER_LOAD_OPTION *)BootOptions;
  UpdatedLoadOption = *UpdatedBootOptions;

  for (Count = 0; Count < *UpdatedBootOptionsCount; Count++) {
    Status = DuplicateLoadOption (&UpdatedLoadOption[Count], &LoadOption[Count]);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  UpdatedLoadOption = *UpdatedBootOptions;
  for (Index = 0; Index < HandleCount; Index++) {
    for (Count = 0; Count < *UpdatedBootOptionsCount; Count++) {
      if ((CompareMem (UpdatedLoadOption[Count].FilePath,
                       DevicePathFromHandle (Handles[Index]),
                       GetDevicePathSize (UpdatedLoadOption[Count].FilePath)) == 0) &&
          (UpdatedLoadOption[Count].OptionalDataSize == sizeof (EFI_GUID)) &&
          (CompareGuid ((EFI_GUID *)UpdatedLoadOption[Count].OptionalData, &mBmAutoCreateBootOptionGuid))) {
        CurrentDevicePath = UpdatedLoadOption[Count].FilePath;
        ValidBootMedia = FALSE;
        while (IsDevicePathEnd (CurrentDevicePath) == FALSE) {
          if (CurrentDevicePath->SubType == MSG_EMMC_DP) {
            ValidBootMedia = TRUE;
            break;
          } else if (CurrentDevicePath->SubType == HW_VENDOR_DP) {
            VENDOR_DEVICE_PATH *VendorPath = (VENDOR_DEVICE_PATH *)CurrentDevicePath;
            if (CompareGuid (&VendorPath->Guid, &gNVIDIARamloadKernelGuid)) {
              ValidBootMedia = TRUE;
              break;
            }
          }
          CurrentDevicePath = NextDevicePathNode (CurrentDevicePath);
        }
        if (!ValidBootMedia) {
          continue;
        }
        UpdatedLoadOption[Count].OptionalDataSize = CmdLen;
        gBS->FreePool (UpdatedLoadOption[Count].OptionalData);
        Status = gBS->AllocatePool (EfiBootServicesData,
                                    CmdLen,
                                    (VOID **)&UpdatedLoadOption[Count].OptionalData);
        if (EFI_ERROR (Status)) {
          goto Error;
        }
        gBS->CopyMem (UpdatedLoadOption[Count].OptionalData, CmdLine, CmdLen);
      }
    }
  }

Error:
  if (CmdLine != NULL) {
    gBS->FreePool (CmdLine);
  }
  if (Handles != NULL) {
    gBS->FreePool (Handles);
  }

  return Status;
}

/**
  Return TRUE when the boot option is tegra specific.

  @param [in] BootOption Pointer to the boot option to check.


  @retval TRUE           The boot option is tegra created.

  @retval FALSE T        The boot option is not tegra added.
**/
STATIC
BOOLEAN
IsTegraBootOption (
  EFI_BOOT_MANAGER_LOAD_OPTION *BootOption
  )
{
  UINTN Length;

  if (BootOption->OptionalData == NULL ||
      BootOption->OptionalDataSize == 0) {
    return FALSE;
  }

  Length = StrLen ((CONST CHAR16 *)BootOption->OptionalData);

  if ((BootOption->OptionalDataSize == ((Length + 1) * sizeof (CHAR16)) + sizeof (EFI_GUID)) &&
      CompareGuid ((EFI_GUID *)((UINT8 *)BootOption->OptionalData + ((Length + 1) * sizeof (CHAR16))),
                   &gNVIDIABmBootOptionGuid)) {
    return TRUE;
  }

  return FALSE;
}

/**
  Return the index of the load option in the load option array.

  The function consider two load options match with changed configuration when the
  OptionType, Attributes, Description and FilePath are equal but OptionalData is
  different.

  @param [in] Key     Pointer to the load option to be found.

  @param [in] Array   Pointer to the array of load options to be found.

  @param [in] Count   Number of entries in the Array.


  @retval -1          Key wasn't found in the Array.

  @retval 0 ~ Count-1 The index of the Key in the Array.
**/
STATIC
INTN
EFIAPI
TegraBootManagerMatchLoadOptionConfigurationChange (
  IN CONST EFI_BOOT_MANAGER_LOAD_OPTION *Key,
  IN CONST EFI_BOOT_MANAGER_LOAD_OPTION *Array,
  IN UINTN                              Count
  )
{
  UINTN                             Index;

  for (Index = 0; Index < Count; Index++) {
    if ((Key->OptionType == Array[Index].OptionType) &&
        (Key->Attributes == Array[Index].Attributes) &&
        (StrCmp (Key->Description, Array[Index].Description) == 0) &&
        (CompareMem (Key->FilePath, Array[Index].FilePath, GetDevicePathSize (Key->FilePath)) == 0) &&
        (Key->OptionalDataSize != Array[Index].OptionalDataSize) &&
        (CompareMem (Key->OptionalData, Array[Index].OptionalData, Key->OptionalDataSize) != 0)) {
      return (INTN) Index;
    }
  }

  return -1;
}

/*
  This function refreshes NV boot options specific to the platform.
  1. This function finds NV options that have changed in terms of configuration data and
  updates the same NV option without modifying the boot order.
  2. This function finds NV options that are not valid any more and deletes them.

  @param[in] BootOptions        An array of updated auto enumerated platform boot options.

  @param[in] BootOptionsCount   The number of elements in BootOptions.


  @retval EFI_SUCCESS           Platform refresh to NV boot options have been done.

  @retval EFI_INVALID_PARAMETER Input is not correct.

  @retval EFI_UNSUPPORTED       Platform specific overrides are not supported.
*/
STATIC
EFI_STATUS
RefreshNvBootOptions (
  IN EFI_BOOT_MANAGER_LOAD_OPTION *BootOptions,
  IN UINTN                        BootOptionsCount
  )
{
  EFI_STATUS                   Status;
  EFI_BOOT_MANAGER_LOAD_OPTION *NvBootOptions;
  UINTN                        NvBootOptionsCount;
  UINTN                        Index;
  INTN                         Match;

  if (BootOptions == NULL ||
      BootOptionsCount == 0) {
    return EFI_INVALID_PARAMETER;
  }

  NvBootOptions = NULL;
  NvBootOptions = EfiBootManagerGetLoadOptions (&NvBootOptionsCount,
                                                LoadOptionTypeBoot);

  if (NvBootOptionsCount == 0 ||
      NvBootOptions == NULL) {
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < NvBootOptionsCount; Index++) {
    if ((DevicePathType (NvBootOptions[Index].FilePath) != BBS_DEVICE_PATH ||
         DevicePathSubType (NvBootOptions[Index].FilePath) != BBS_BBS_DP) &&
        IsTegraBootOption (&NvBootOptions[Index])) {
      Match = TegraBootManagerMatchLoadOptionConfigurationChange (&NvBootOptions[Index], BootOptions, BootOptionsCount);
      if (Match != -1) {
        BootOptions[Match].OptionNumber = NvBootOptions[Index].OptionNumber;
        Status = EfiBootManagerLoadOptionToVariable (&BootOptions[Match]);
        if (EFI_ERROR (Status)) {
          EfiBootManagerDeleteLoadOptionVariable (BootOptions[Match].OptionNumber,
                                                  BootOptions[Match].OptionType);
        }
        continue;
      }
      if (EfiBootManagerFindLoadOption (&NvBootOptions[Index], BootOptions, BootOptionsCount) == -1) {
        Status = EfiBootManagerDeleteLoadOptionVariable (NvBootOptions[Index].OptionNumber,
                                                         LoadOptionTypeBoot);
        if (EFI_ERROR (Status)) {
          Status = EFI_UNSUPPORTED;
          goto Error;
        }
      }
    }
  }

Error:
  EfiBootManagerFreeLoadOptions (NvBootOptions, NvBootOptionsCount);

  return Status;
}

/*
  This function allows platform to refresh all boot options specific to the platform. Within
  this function, platform can make modifications to the auto enumerated platform boot options
  as well as NV boot options.

  @param[in const] BootOptions             An array of auto enumerated platform boot options.
                                           This array will be freed by caller upon successful
                                           exit of this function and output array would be used.

  @param[in const] BootOptionsCount        The number of elements in BootOptions.

  @param[out]      UpdatedBootOptions      An array of boot options that have been customized
                                           for the platform on top of input boot options. This
                                           array would be allocated by REFRESH_ALL_BOOT_OPTIONS
                                           and would be freed by caller after consuming it.

  @param[out]      UpdatedBootOptionsCount The number of elements in UpdatedBootOptions.


  @retval EFI_SUCCESS                      Platform refresh to input BootOptions and
                                           BootCount have been done.

  @retval EFI_OUT_OF_RESOURCES             Memory allocation failed.

  @retval EFI_INVALID_PARAMETER            Input is not correct.

  @retval EFI_UNSUPPORTED                  Platform specific overrides are not supported.
*/
STATIC
EFI_STATUS
RefreshAllBootOptions (
  IN  CONST EFI_BOOT_MANAGER_LOAD_OPTION *BootOptions,
  IN  CONST UINTN                        BootOptionsCount,
  OUT       EFI_BOOT_MANAGER_LOAD_OPTION **UpdatedBootOptions,
  OUT       UINTN                        *UpdatedBootOptionsCount
  )
{
  EFI_STATUS Status;

  Status = RefreshAutoEnumeratedBootOptions (BootOptions,
                                             BootOptionsCount,
                                             UpdatedBootOptions,
                                             UpdatedBootOptionsCount);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = RefreshNvBootOptions (*UpdatedBootOptions,
                                 *UpdatedBootOptionsCount);

  return Status;
}

EDKII_PLATFORM_BOOT_MANAGER_PROTOCOL mPlatformBootManager = {
  EDKII_PLATFORM_BOOT_MANAGER_PROTOCOL_REVISION,
  RefreshAllBootOptions
};

EFI_STATUS
EFIAPI
PlatformBootManagerEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  return gBS->InstallMultipleProtocolInterfaces (&ImageHandle,
                                                 &gEdkiiPlatformBootManagerProtocolGuid,
                                                 &mPlatformBootManager,
                                                 NULL);
}
