/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2019-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include <Library/UefiRuntimeServicesTableLib.h>
#include <libfdt.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/PlatformBootManager.h>
#include <Protocol/PciIo.h>
#include <Protocol/KernelCmdLineUpdate.h>
#include <Protocol/AndroidBootImg.h>
#include <Protocol/KernelArgsProtocol.h>
#include <Protocol/PlatformKernelArgsProtocol.h>

#include <NVIDIAConfiguration.h>

#define NVIDIA_KERNEL_COMMAND_MAX_LEN  25

extern EFI_GUID  mBmAutoCreateBootOptionGuid;

CHAR16  KernelCommandRemoveAcpi[][NVIDIA_KERNEL_COMMAND_MAX_LEN] = {
  L"console="
};

UINT8  RemovableMessagingDeviceSubType[] = {
  MSG_SD_DP,
  MSG_USB_DP,
  MSG_USB_CLASS_DP,
  MSG_USB_WWID_DP
};

UINT8  RemovableHardwareDeviceSubType[] = {
  HW_PCI_DP
};

/**
  Return TRUE when the boot option is auto-created instead of manually added.

  @param BootOption Pointer to the boot option to check.

  @retval TRUE  The boot option is auto-created.
  @retval FALSE The boot option is manually added.
**/
BOOLEAN
BmIsAutoCreateBootOption (
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption
  );

/*
  Checks whether the auto-enumerated boot option is valid for the platform.

  @param[in] LoadOption            Load option buffer.

  @retval TRUE                     Load option valid.

  @retval FALSE                    Load option invalid.
*/
STATIC
BOOLEAN
IsValidLoadOption (
  IN  EFI_BOOT_MANAGER_LOAD_OPTION  *LoadOption
  )
{
  EFI_STATUS                        Status;
  EFI_HANDLE                        Handle;
  EFI_DEVICE_PATH                   *DevicePath;
  VOID                              *DevicePathNode;
  CONTROLLER_DEVICE_PATH            *Controller;
  UFS_DEVICE_PATH                   *Ufs;
  EFI_PCI_IO_PROTOCOL               *PciIo;
  NVIDIA_ENABLED_PCIE_NIC_TOPOLOGY  *EnabledPcieNicTopology;
  BOOLEAN                           NicFilteringEnabled;
  UINTN                             Segment;
  UINTN                             Bus;
  UINTN                             Device;
  UINTN                             Function;

  if (CompareGuid ((EFI_GUID *)LoadOption->OptionalData, &mBmAutoCreateBootOptionGuid)) {
    DevicePath = LoadOption->FilePath;

    // Load options with FirmwareVolume2 protocol are not supported
    // on the platform.
    Handle = NULL;
    Status = gBS->LocateDevicePath (&gEfiFirmwareVolume2ProtocolGuid, &DevicePath, &Handle);
    if (!EFI_ERROR (Status)) {
      return FALSE;
    }

    EnabledPcieNicTopology = PcdGetPtr (PcdEnabledPcieNicTopology);
    NicFilteringEnabled    = FALSE;
    if ((EnabledPcieNicTopology != NULL) &&
        (EnabledPcieNicTopology->Enabled))
    {
      NicFilteringEnabled = TRUE;
    }

    DevicePathNode = DevicePath;
    while (!IsDevicePathEndType (DevicePathNode)) {
      // Look for eMMC and ignore the non-user partitions
      if (DevicePathType (DevicePathNode) == MESSAGING_DEVICE_PATH) {
        if (DevicePathSubType (DevicePathNode) == MSG_EMMC_DP) {
          DevicePathNode = NextDevicePathNode (DevicePathNode);
          if ((DevicePathType (DevicePathNode) == HARDWARE_DEVICE_PATH) &&
              (DevicePathSubType (DevicePathNode) == HW_CONTROLLER_DP))
          {
            Controller = (CONTROLLER_DEVICE_PATH *)DevicePathNode;
            if (Controller->ControllerNumber != 0) {
              return FALSE;
            }
          }

          break;
        } else if (DevicePathSubType (DevicePathNode) == MSG_UFS_DP) {
          Ufs = (UFS_DEVICE_PATH *)DevicePathNode;
          if ((IsDevicePathEndType (NextDevicePathNode (DevicePathNode))) &&
              (Ufs->Pun == 0xFF))
          {
            return FALSE;
          }

          break;
        }
      }

      if (NicFilteringEnabled) {
        if ((DevicePathType (DevicePathNode) == MESSAGING_DEVICE_PATH) &&
            (DevicePathSubType (DevicePathNode) == MSG_MAC_ADDR_DP))
        {
          while (TRUE) {
            Handle = NULL;
            Status = gBS->LocateDevicePath (&gEfiPciIoProtocolGuid, &DevicePath, &Handle);
            if (EFI_ERROR (Status)) {
              break;
            }

            Status = gBS->HandleProtocol (Handle, &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
            if (EFI_ERROR (Status)) {
              break;
            }

            Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
            if (EFI_ERROR (Status)) {
              break;
            }

            if (EnabledPcieNicTopology->Segment == ENABLED_PCIE_ALLOW_ALL) {
              Segment = ENABLED_PCIE_ALLOW_ALL;
            }

            if (EnabledPcieNicTopology->Bus == ENABLED_PCIE_ALLOW_ALL) {
              Bus = ENABLED_PCIE_ALLOW_ALL;
            }

            if (EnabledPcieNicTopology->Device == ENABLED_PCIE_ALLOW_ALL) {
              Device = ENABLED_PCIE_ALLOW_ALL;
            }

            if (EnabledPcieNicTopology->Function == ENABLED_PCIE_ALLOW_ALL) {
              Function = ENABLED_PCIE_ALLOW_ALL;
            }

            if ((EnabledPcieNicTopology->Segment != Segment) ||
                (EnabledPcieNicTopology->Bus != Bus) ||
                (EnabledPcieNicTopology->Device != Device) ||
                (EnabledPcieNicTopology->Function != Function))
            {
              return FALSE;
            }

            break;
          }

          break;
        }
      }

      DevicePathNode = NextDevicePathNode (DevicePathNode);
    }
  }

  return TRUE;
}

/*
  Checks whether the auto-enumerated boot option is removable or not.

  @param[in] LoadOption            Load option buffer.

  @retval TRUE                     Load option valid.

  @retval FALSE                    Load option invalid.
*/
STATIC
BOOLEAN
IsRemovableLoadOption (
  IN  EFI_BOOT_MANAGER_LOAD_OPTION  *LoadOption
  )
{
  EFI_DEVICE_PATH  *DevicePath;
  VOID             *DevicePathNode;
  UINTN            Count;

  DevicePath     = LoadOption->FilePath;
  DevicePathNode = DevicePath;
  while (!IsDevicePathEndType (DevicePathNode)) {
    if (DevicePathType (DevicePathNode) == MESSAGING_DEVICE_PATH) {
      for (Count = 0; Count < sizeof (RemovableMessagingDeviceSubType)/sizeof (RemovableMessagingDeviceSubType[0]); Count++) {
        if (DevicePathSubType (DevicePathNode) == RemovableMessagingDeviceSubType[Count]) {
          return TRUE;
        }
      }
    } else if (DevicePathType (DevicePathNode) == HARDWARE_DEVICE_PATH) {
      for (Count = 0; Count < sizeof (RemovableHardwareDeviceSubType)/sizeof (RemovableHardwareDeviceSubType[0]); Count++) {
        if (DevicePathSubType (DevicePathNode) == RemovableHardwareDeviceSubType[Count]) {
          return TRUE;
        }
      }
    }

    DevicePathNode = NextDevicePathNode (DevicePathNode);
  }

  return FALSE;
}

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
  OUT EFI_BOOT_MANAGER_LOAD_OPTION  *DestinationLoadOption,
  IN  EFI_BOOT_MANAGER_LOAD_OPTION  *SourceLoadOption
  )
{
  EFI_STATUS  Status;

  if ((DestinationLoadOption == NULL) ||
      (SourceLoadOption == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  gBS->CopyMem (DestinationLoadOption, SourceLoadOption, sizeof (EFI_BOOT_MANAGER_LOAD_OPTION));

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  (StrLen (SourceLoadOption->Description) + 1) * sizeof (CHAR16),
                  (VOID **)&DestinationLoadOption->Description
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->CopyMem (DestinationLoadOption->Description, SourceLoadOption->Description, (StrLen (SourceLoadOption->Description) + 1) * sizeof (CHAR16));

  DestinationLoadOption->FilePath = DuplicateDevicePath (SourceLoadOption->FilePath);

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  SourceLoadOption->OptionalDataSize,
                  (VOID **)&DestinationLoadOption->OptionalData
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->CopyMem (DestinationLoadOption->OptionalData, SourceLoadOption->OptionalData, SourceLoadOption->OptionalDataSize);

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  SourceLoadOption->ExitDataSize,
                  (VOID **)&DestinationLoadOption->ExitData
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->CopyMem (DestinationLoadOption->ExitData, SourceLoadOption->ExitData, SourceLoadOption->ExitDataSize);

  return Status;
}

/*
  Get the kernel command line from DT.

  @param[out] OutCmdLine Kernel command line from DTB.

  @retval EFI_SUCCESS    Command line retrieved correctly.

  @retval Others         Failure.
*/
STATIC
EFI_STATUS
GetDtbCommandLine (
  OUT CHAR16  **OutCmdLine
  )
{
  EFI_STATUS   Status;
  VOID         *DeviceTreeBase;
  UINTN        DeviceTreeSize;
  INT32        NodeOffset;
  CONST CHAR8  *CommandLineEntry;
  INT32        CommandLineLength;
  INT32        CommandLineBytes;
  CHAR16       *CmdLineDtb;
  BOOLEAN      DTBoot;
  VOID         *AcpiBase;

  DTBoot = FALSE;
  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (EFI_ERROR (Status)) {
    DTBoot = TRUE;
  }

  DeviceTreeBase = NULL;
  CmdLineDtb     = NULL;
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
  CommandLineEntry = (CONST CHAR8 *)fdt_getprop (DeviceTreeBase, NodeOffset, "bootargs", &CommandLineLength);
  if (NULL == CommandLineEntry) {
    return EFI_NOT_FOUND;
  }

  CommandLineBytes = CommandLineLength * sizeof (CHAR16);

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  CommandLineBytes,
                  (VOID **)&CmdLineDtb
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->SetMem (CmdLineDtb, CommandLineBytes, 0);

  AsciiStrToUnicodeStrS (CommandLineEntry, CmdLineDtb, CommandLineBytes);

  DEBUG ((DEBUG_INFO, "%a: DTB Kernel Command Line: %s\n", __FUNCTION__, CmdLineDtb));

  *OutCmdLine = CmdLineDtb;
  return EFI_SUCCESS;
}

/*
  Remove kernel command line.

  @param[in]  InCmdLine  Input Kernel command line.
  @param[out] DelCmdLine Kernel command option to be deleted.

  @retval EFI_SUCCESS    Command option removed correctly.

  @retval Others         Failure.
*/
STATIC
VOID
RemoveKernelCommandLine (
  IN CHAR16  *InCmdLine,
  IN CHAR16  *DelCmdLine
  )
{
  CHAR16  *ExistingCommandLineArgumentStart;
  CHAR16  *ExistingCommandLineArgumentEnd;
  UINTN   Length;
  CHAR16  *TempBuffer;

  ExistingCommandLineArgumentStart = NULL;
  ExistingCommandLineArgumentStart = StrStr (InCmdLine, DelCmdLine);
  while (ExistingCommandLineArgumentStart != NULL) {
    ExistingCommandLineArgumentEnd = NULL;
    ExistingCommandLineArgumentEnd = StrStr (ExistingCommandLineArgumentStart, L" ");

    if (ExistingCommandLineArgumentEnd == NULL) {
      gBS->SetMem (ExistingCommandLineArgumentStart, StrSize (ExistingCommandLineArgumentStart), 0);
      break;
    }

    TempBuffer = NULL;
    Length     = StrSize (ExistingCommandLineArgumentEnd + 1);

    gBS->AllocatePool (
           EfiBootServicesData,
           Length,
           (VOID **)&TempBuffer
           );

    gBS->SetMem (TempBuffer, Length, 0);

    gBS->CopyMem (
           TempBuffer,
           ExistingCommandLineArgumentEnd + 1,
           Length
           );

    gBS->SetMem (
           ExistingCommandLineArgumentStart,
           StrSize (ExistingCommandLineArgumentStart),
           0
           );

    gBS->CopyMem (
           ExistingCommandLineArgumentStart,
           TempBuffer,
           Length
           );

    gBS->FreePool (TempBuffer);

    ExistingCommandLineArgumentStart = StrStr (ExistingCommandLineArgumentStart, DelCmdLine);
  }
}

/*
  Update kernel command line.

  @param[in]  InCmdLine  Input Kernel command line.
  @param[out] OutCmdLine Output Kernel command line.

  @retval EFI_SUCCESS    Command line retrieved correctly.

  @retval Others         Failure.
*/
STATIC
EFI_STATUS
UpdateKernelCommandLine (
  IN  CHAR16  *InCmdLine,
  OUT CHAR16  **OutCmdLine
  )
{
  EFI_STATUS                              Status;
  UINTN                                   Length;
  UINTN                                   NumOfHandles;
  EFI_HANDLE                              *HandleBuffer = NULL;
  UINTN                                   Count;
  NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL  *Interface;
  CHAR16                                  *CmdLine = NULL;
  NVIDIA_KERNEL_COMMAND_LINE              AddlCmdLine;
  UINTN                                   AddlCmdLen;
  VOID                                    *AcpiBase;

  AddlCmdLen = sizeof (AddlCmdLine);
  Status     = gRT->GetVariable (L"KernelCommandLine", &gNVIDIAPublicVariableGuid, NULL, &AddlCmdLen, &AddlCmdLine);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get additional command line - %r\r\n", __FUNCTION__, Status));
    ZeroMem (&AddlCmdLine, sizeof (AddlCmdLine));
  }

  Length = StrSize (InCmdLine) + StrSize (AddlCmdLine.KernelCommand);

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIAKernelCmdLineUpdateGuid,
                  NULL,
                  &NumOfHandles,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    NumOfHandles = 0;
  }

  for (Count = 0; Count < NumOfHandles; Count++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Count],
                    &gNVIDIAKernelCmdLineUpdateGuid,
                    (VOID **)&Interface
                    );
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    if (Interface->NewCommandLineArgument != NULL) {
      Length += StrSize (Interface->NewCommandLineArgument);
    }
  }

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  Length,
                  (VOID **)&CmdLine
                  );
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

  gBS->SetMem (CmdLine, Length, 0);

  gBS->CopyMem (CmdLine, InCmdLine, StrSize (InCmdLine));

  for (Count = 0; Count < NumOfHandles; Count++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Count],
                    &gNVIDIAKernelCmdLineUpdateGuid,
                    (VOID **)&Interface
                    );
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    if (Interface->ExistingCommandLineArgument != NULL) {
      RemoveKernelCommandLine (CmdLine, Interface->ExistingCommandLineArgument);
    }
  }

  for (Count = 0; Count < NumOfHandles; Count++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Count],
                    &gNVIDIAKernelCmdLineUpdateGuid,
                    (VOID **)&Interface
                    );
    if (EFI_ERROR (Status)) {
      goto CleanupAndReturn;
    }

    if (Interface->NewCommandLineArgument != NULL) {
      UnicodeSPrint (CmdLine, Length, L"%s %s", CmdLine, Interface->NewCommandLineArgument);
    }
  }

  UnicodeSPrint (CmdLine, Length, L"%s %s", CmdLine, AddlCmdLine.KernelCommand);

  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    for (Count = 0; Count < sizeof (KernelCommandRemoveAcpi)/sizeof (KernelCommandRemoveAcpi[0]); Count++) {
      RemoveKernelCommandLine (CmdLine, KernelCommandRemoveAcpi[Count]);
    }
  }

  *OutCmdLine = CmdLine;
  Status      = EFI_SUCCESS;

CleanupAndReturn:
  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
  }

  if ((CmdLine != NULL) && (EFI_ERROR (Status))) {
    FreePool (CmdLine);
  }

  return Status;
}

// Append platform specific commands
STATIC
EFI_STATUS
EFIAPI
PlatformAppendKernelArgs (
  IN CHAR16  *Args,
  IN UINTN   Size
  )
{
  EFI_STATUS  Status;
  CHAR16      *NewArgs;

  Status = UpdateKernelCommandLine (Args, &NewArgs);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (StrSize (NewArgs) > Size) {
    DEBUG ((DEBUG_ERROR, "%a: New command line too long: %d\r\n", __FUNCTION__, StrSize (NewArgs)));
    Status = EFI_DEVICE_ERROR;
  } else {
    Status = StrCpyS (Args, Size / sizeof (CHAR16), NewArgs);
  }

  gBS->FreePool (NewArgs);
  NewArgs = NULL;
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
AndroidBootImgAppendKernelArgs (
  IN CHAR16  *Args,
  IN UINTN   Size
  )
{
  CHAR16      *DtbKernelArgs = NULL;
  EFI_STATUS  Status;

  if ((Args == NULL) || (Size == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if (PcdGetBool (PcdBootAndroidImage)) {
    DEBUG ((DEBUG_ERROR, "%a: Using DTB Kernel Command Line\n", __FUNCTION__));
    Status = GetDtbCommandLine (&DtbKernelArgs);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: no DTB cmdline\n", __FUNCTION__));
      *Args = L'\0';
    } else {
      Status = StrCpyS (Args, Size / sizeof (CHAR16), DtbKernelArgs);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: cmdline copy failed\n", __FUNCTION__));
        goto Error;
      }
    }

    DEBUG ((DEBUG_ERROR, "DTB cmdline: %s\n", Args));
  }

  Status = PlatformAppendKernelArgs (Args, Size);

Error:
  if (DtbKernelArgs != NULL) {
    gBS->FreePool (DtbKernelArgs);
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
  IN  CONST EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions,
  IN  CONST UINTN                         BootOptionsCount,
  OUT       EFI_BOOT_MANAGER_LOAD_OPTION  **UpdatedBootOptions,
  OUT       UINTN                         *UpdatedBootOptionsCount
  )
{
  EFI_STATUS                    Status;
  EFI_BOOT_MANAGER_LOAD_OPTION  *LoadOption;
  EFI_BOOT_MANAGER_LOAD_OPTION  *UpdatedLoadOption;
  UINTN                         Count;

  DEBUG ((DEBUG_VERBOSE, "%a: entered\n", __FUNCTION__));
  DEBUG ((DEBUG_VERBOSE, "%a: BootOptions is %a\n", __FUNCTION__, BootOptions ? "not NULL" : "NULL"));
  DEBUG ((DEBUG_VERBOSE, "%a: BootOptionsCount is %lu\n", __FUNCTION__, BootOptionsCount));
  DEBUG ((DEBUG_VERBOSE, "%a: UpdatedBootOptions is %a\n", __FUNCTION__, UpdatedBootOptions ? "not NULL" : "NULL"));
  DEBUG ((DEBUG_VERBOSE, "%a: UpdatedBootOptionsCount is %a\n", __FUNCTION__, UpdatedBootOptionsCount ? "not NULL" : "NULL"));
  if ((BootOptions == NULL) ||
      (BootOptionsCount == 0) ||
      (UpdatedBootOptions == NULL) ||
      (UpdatedBootOptionsCount == NULL))
  {
    DEBUG ((DEBUG_ERROR, "%a: found invalid parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  BootOptionsCount * sizeof (EFI_BOOT_MANAGER_LOAD_OPTION),
                  (VOID **)UpdatedBootOptions
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to allocate space for UpdatedBootOptions\n", __FUNCTION__, Status));
    return Status;
  }

  gBS->SetMem (*UpdatedBootOptions, BootOptionsCount * sizeof (EFI_BOOT_MANAGER_LOAD_OPTION), 0);

  LoadOption               = (EFI_BOOT_MANAGER_LOAD_OPTION *)BootOptions;
  UpdatedLoadOption        = *UpdatedBootOptions;
  *UpdatedBootOptionsCount = 0;
  for (Count = 0; Count < BootOptionsCount; Count++) {
    if (IsValidLoadOption (&LoadOption[Count])) {
      Status = DuplicateLoadOption (&UpdatedLoadOption[*UpdatedBootOptionsCount], &LoadOption[Count]);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Got %r trying to DuplicateLoadOption\n", __FUNCTION__, Status));
        return Status;
      }

      (*UpdatedBootOptionsCount)++;
    }
  }

  DEBUG ((DEBUG_VERBOSE, "%a: *UpdatedBootOptionsCount = %lu\n", __FUNCTION__, *UpdatedBootOptionsCount));

  return Status;
}

/*
  This function refreshes NV boot options specific to the platform.
  1. This function detects if a new android or removable boot option is detected
     and adds it to the top of boot order.
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
  IN EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions,
  IN UINTN                         BootOptionsCount
  )
{
  EFI_STATUS                    Status;
  EFI_BOOT_MANAGER_LOAD_OPTION  *NvBootOptions;
  UINTN                         NvBootOptionsCount;
  UINTN                         Index;

  Status = EFI_SUCCESS;

  if ((BootOptions == NULL) ||
      (BootOptionsCount == 0))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (PcdGet8 (PcdNewDeviceHierarchy)) {
    NvBootOptions = NULL;
    NvBootOptions = EfiBootManagerGetLoadOptions (
                      &NvBootOptionsCount,
                      LoadOptionTypeBoot
                      );

    if ((NvBootOptionsCount == 0) ||
        (NvBootOptions == NULL))
    {
      return EFI_SUCCESS;
    }

    for (Index = 0; Index < BootOptionsCount; Index++) {
      if (EfiBootManagerFindLoadOption (&BootOptions[Index], NvBootOptions, NvBootOptionsCount) == -1) {
        if (PcdGetBool (PcdBootAndroidImage) || IsRemovableLoadOption (&BootOptions[Index])) {
          Status = EfiBootManagerAddLoadOptionVariable (&BootOptions[Index], 0);
          if (EFI_ERROR (Status)) {
            goto Error;
          }
        }
      }
    }

    EfiBootManagerFreeLoadOptions (NvBootOptions, NvBootOptionsCount);
  }

  NvBootOptions = EfiBootManagerGetLoadOptions (
                    &NvBootOptionsCount,
                    LoadOptionTypeBoot
                    );

  if ((NvBootOptionsCount == 0) ||
      (NvBootOptions == NULL))
  {
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < NvBootOptionsCount; Index++) {
    if (((DevicePathType (NvBootOptions[Index].FilePath) != BBS_DEVICE_PATH) ||
         (DevicePathSubType (NvBootOptions[Index].FilePath) != BBS_BBS_DP)) &&
        BmIsAutoCreateBootOption (&NvBootOptions[Index]))
    {
      if (EfiBootManagerFindLoadOption (&NvBootOptions[Index], BootOptions, BootOptionsCount) == -1) {
        Status = EfiBootManagerDeleteLoadOptionVariable (
                   NvBootOptions[Index].OptionNumber,
                   LoadOptionTypeBoot
                   );
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
  IN  CONST EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions,
  IN  CONST UINTN                         BootOptionsCount,
  OUT       EFI_BOOT_MANAGER_LOAD_OPTION  **UpdatedBootOptions,
  OUT       UINTN                         *UpdatedBootOptionsCount
  )
{
  EFI_STATUS  Status;

  Status = RefreshAutoEnumeratedBootOptions (
             BootOptions,
             BootOptionsCount,
             UpdatedBootOptions,
             UpdatedBootOptionsCount
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = RefreshNvBootOptions (
             *UpdatedBootOptions,
             *UpdatedBootOptionsCount
             );

  return Status;
}

EDKII_PLATFORM_BOOT_MANAGER_PROTOCOL  mPlatformBootManager = {
  EDKII_PLATFORM_BOOT_MANAGER_PROTOCOL_REVISION,
  RefreshAllBootOptions
};

STATIC ANDROID_BOOTIMG_PROTOCOL  mAndroidBootImgProtocol = { AndroidBootImgAppendKernelArgs, NULL };

STATIC NVIDIA_PLATFORM_KERNEL_ARGS_PROTOCOL  mPlatformKernelArgsProtocol = {
  PlatformAppendKernelArgs
};

EFI_STATUS
EFIAPI
PlatformBootManagerEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return gBS->InstallMultipleProtocolInterfaces (
                &ImageHandle,
                &gEdkiiPlatformBootManagerProtocolGuid,
                &mPlatformBootManager,
                &gAndroidBootImgProtocolGuid,
                &mAndroidBootImgProtocol,
                &gNVIDIAPlatformKernelArgsProtocolGuid,
                &mPlatformKernelArgsProtocol,
                NULL
                );
}
