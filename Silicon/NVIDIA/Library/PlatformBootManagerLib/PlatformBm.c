/** @file
  Implementation for PlatformBootManagerLib library class interfaces.

  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (C) 2015-2016, Red Hat, Inc.
  Copyright (c) 2014, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/Pci22.h>
#include <Library/BootLogoLib.h>
#include <Library/CapsuleLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/PlatformBootOrderLib.h>
#include <Library/BaseCryptLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <Library/DxeCapsuleLibFmp/CapsuleOnDisk.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Protocol/BootChainProtocol.h>
#include <Protocol/DevicePath.h>
#include <Protocol/EsrtManagement.h>
#include <Protocol/GenericMemoryTest.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/IpmiTransportProtocol.h>
#include <Protocol/PciIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PlatformBootManager.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Guid/EventGroup.h>
#include <Guid/RtPropertiesTable.h>
#include <Guid/TtyTerm.h>
#include <Guid/SerialPortLibVendor.h>
#include <IndustryStandard/Ipmi.h>
#include <libfdt.h>
#include "PlatformBm.h"
#include <NVIDIAConfiguration.h>

#define DP_NODE_LEN(Type)  { (UINT8)sizeof (Type), (UINT8)(sizeof (Type) >> 8) }

#pragma pack (1)
typedef struct {
  USB_CLASS_DEVICE_PATH       Keyboard;
  EFI_DEVICE_PATH_PROTOCOL    End;
} PLATFORM_USB_KEYBOARD;
#pragma pack ()

STATIC PLATFORM_USB_KEYBOARD  mUsbKeyboard = {
  //
  // USB_CLASS_DEVICE_PATH Keyboard
  //
  {
    {
      MESSAGING_DEVICE_PATH, MSG_USB_CLASS_DP,
      DP_NODE_LEN (USB_CLASS_DEVICE_PATH)
    },
    0xFFFF, // VendorId: any
    0xFFFF, // ProductId: any
    3,      // DeviceClass: HID
    1,      // DeviceSubClass: boot
    1       // DeviceProtocol: keyboard
  },

  //
  // EFI_DEVICE_PATH_PROTOCOL End
  //
  {
    END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE,
    DP_NODE_LEN (EFI_DEVICE_PATH_PROTOCOL)
  }
};

STATIC PLATFORM_CONFIGURATION_DATA  CurrentPlatformConfigData;

/**
  Check if the handle satisfies a particular condition.

  @param[in] Handle      The handle to check.
  @param[in] ReportText  A caller-allocated string passed in for reporting
                         purposes. It must never be NULL.

  @retval TRUE   The condition is satisfied.
  @retval FALSE  Otherwise. This includes the case when the condition could not
                 be fully evaluated due to an error.
**/

typedef
BOOLEAN
(EFIAPI *FILTER_FUNCTION)(
  IN EFI_HANDLE   Handle,
  IN CONST CHAR16 *ReportText
  );

/**
  Process a handle.

  @param[in] Handle      The handle to process.
  @param[in] ReportText  A caller-allocated string passed in for reporting
                         purposes. It must never be NULL.
**/

typedef
VOID
(EFIAPI *CALLBACK_FUNCTION)(
  IN EFI_HANDLE   Handle,
  IN CONST CHAR16 *ReportText
  );

/**
  Locate all handles that carry the specified protocol, filter them with a
  callback function, and pass each handle that passes the filter to another
  callback.

  @param[in] ProtocolGuid  The protocol to look for.

  @param[in] Filter        The filter function to pass each handle to. If this
                           parameter is NULL, then all handles are processed.

  @param[in] Process       The callback function to pass each handle to that
                           clears the filter.
**/
STATIC
VOID
FilterAndProcess (
  IN EFI_GUID           *ProtocolGuid,
  IN FILTER_FUNCTION    Filter         OPTIONAL,
  IN CALLBACK_FUNCTION  Process
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *Handles;
  UINTN       NoHandles;
  UINTN       Idx;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  ProtocolGuid,
                  NULL /* SearchKey */,
                  &NoHandles,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    //
    // This is not an error, just an informative condition.
    //
    DEBUG ((
      EFI_D_VERBOSE,
      "%a: %g: %r\n",
      __FUNCTION__,
      ProtocolGuid,
      Status
      ));
    return;
  }

  ASSERT (NoHandles > 0);
  for (Idx = 0; Idx < NoHandles; ++Idx) {
    CHAR16         *DevicePathText;
    STATIC CHAR16  Fallback[] = L"<device path unavailable>";

    //
    // The ConvertDevicePathToText() function handles NULL input transparently.
    //
    DevicePathText = ConvertDevicePathToText (
                       DevicePathFromHandle (Handles[Idx]),
                       FALSE, // DisplayOnly
                       FALSE  // AllowShortcuts
                       );
    if (DevicePathText == NULL) {
      DevicePathText = Fallback;
    }

    if ((Filter == NULL) || Filter (Handles[Idx], DevicePathText)) {
      Process (Handles[Idx], DevicePathText);
    }

    if (DevicePathText != Fallback) {
      FreePool (DevicePathText);
    }
  }

  gBS->FreePool (Handles);
}

/**
  Perform the memory test base on the memory test intensive level,
  and update the memory resource.

  @param  Level         The memory test intensive level.

  @retval EFI_STATUS    Success test all the system memory and update
                        the memory resource

**/
EFI_STATUS
MemoryTest (
  IN EXTENDMEM_COVERAGE_LEVEL  Level
  )
{
  EFI_STATUS                        Status;
  BOOLEAN                           RequireSoftECCInit;
  EFI_GENERIC_MEMORY_TEST_PROTOCOL  *GenMemoryTest;
  UINT64                            TestedMemorySize;
  UINT64                            TotalMemorySize;
  BOOLEAN                           ErrorOut;
  BOOLEAN                           TestAbort;

  TestedMemorySize = 0;
  TotalMemorySize  = 0;
  ErrorOut         = FALSE;
  TestAbort        = FALSE;

  RequireSoftECCInit = FALSE;

  Status = gBS->LocateProtocol (
                  &gEfiGenericMemTestProtocolGuid,
                  NULL,
                  (VOID **)&GenMemoryTest
                  );
  if (EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  Status = GenMemoryTest->MemoryTestInit (
                            GenMemoryTest,
                            Level,
                            &RequireSoftECCInit
                            );
  if (Status == EFI_NO_MEDIA) {
    //
    // The PEI codes also have the relevant memory test code to check the memory,
    // it can select to test some range of the memory or all of them. If PEI code
    // checks all the memory, this BDS memory test will has no not-test memory to
    // do the test, and then the status of EFI_NO_MEDIA will be returned by
    // "MemoryTestInit". So it does not need to test memory again, just return.
    //
    return EFI_SUCCESS;
  }

  do {
    Status = GenMemoryTest->PerformMemoryTest (
                              GenMemoryTest,
                              &TestedMemorySize,
                              &TotalMemorySize,
                              &ErrorOut,
                              TestAbort
                              );
    if (ErrorOut && (Status == EFI_DEVICE_ERROR)) {
      ASSERT (0);
    }
  } while (Status != EFI_NOT_FOUND);

  Status = GenMemoryTest->Finished (GenMemoryTest);

  return EFI_SUCCESS;
}

/**
  This FILTER_FUNCTION checks if a handle corresponds to a PCI display device.
**/
STATIC
BOOLEAN
EFIAPI
IsPciDisplay (
  IN EFI_HANDLE    Handle,
  IN CONST CHAR16  *ReportText
  )
{
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  PCI_TYPE00           Pci;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo
                  );
  if (EFI_ERROR (Status)) {
    //
    // This is not an error worth reporting.
    //
    return FALSE;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        0 /* Offset */,
                        sizeof Pci / sizeof (UINT32),
                        &Pci
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: %s: %r\n", __FUNCTION__, ReportText, Status));
    return FALSE;
  }

  return IS_PCI_DISPLAY (&Pci);
}

/**
  This CALLBACK_FUNCTION attempts to connect a handle non-recursively, asking
  the matching driver to produce all first-level child handles.
**/
STATIC
VOID
EFIAPI
Connect (
  IN EFI_HANDLE    Handle,
  IN CONST CHAR16  *ReportText
  )
{
  EFI_STATUS  Status;

  Status = gBS->ConnectController (
                  Handle, // ControllerHandle
                  NULL,   // DriverImageHandle
                  NULL,   // RemainingDevicePath -- produce all children
                  FALSE   // Recursive
                  );
  DEBUG ((
    EFI_ERROR (Status) ? EFI_D_ERROR : EFI_D_VERBOSE,
    "%a: %s: %r\n",
    __FUNCTION__,
    ReportText,
    Status
    ));
}

/**
  This CALLBACK_FUNCTION retrieves the EFI_DEVICE_PATH_PROTOCOL from the
  handle, and adds it to ConOut and ErrOut.
**/
STATIC
VOID
EFIAPI
AddOutput (
  IN EFI_HANDLE    Handle,
  IN CONST CHAR16  *ReportText
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

  DevicePath = DevicePathFromHandle (Handle);
  if (DevicePath == NULL) {
    DEBUG ((
      EFI_D_ERROR,
      "%a: %s: handle %p: device path not found\n",
      __FUNCTION__,
      ReportText,
      Handle
      ));
    return;
  }

  Status = EfiBootManagerUpdateConsoleVariable (ConOut, DevicePath, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      EFI_D_ERROR,
      "%a: %s: adding to ConOut: %r\n",
      __FUNCTION__,
      ReportText,
      Status
      ));
    return;
  }

  Status = EfiBootManagerUpdateConsoleVariable (ErrOut, DevicePath, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      EFI_D_ERROR,
      "%a: %s: adding to ErrOut: %r\n",
      __FUNCTION__,
      ReportText,
      Status
      ));
    return;
  }

  DEBUG ((
    EFI_D_VERBOSE,
    "%a: %s: added to ConOut and ErrOut\n",
    __FUNCTION__,
    ReportText
    ));
}

/**
  This CALLBACK_FUNCTION retrieves the vendor and device id of all pcie
  devices and prints it.
**/
STATIC
VOID
EFIAPI
ListPciDevices (
  IN EFI_HANDLE    Handle,
  IN CONST CHAR16  *ReportText
  )
{
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  PCI_TYPE00           Pci;
  UINTN                Segment;
  UINTN                Bus;
  UINTN                Device;
  UINTN                Function;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        0,
                        sizeof (Pci) / sizeof (UINT32),
                        &Pci
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: %s: %r\n", __FUNCTION__, ReportText, Status));
    return;
  }

  Status = PciIo->GetLocation (
                    PciIo,
                    &Segment,
                    &Bus,
                    &Device,
                    &Function
                    );

  DEBUG ((
    EFI_D_ERROR,
    "%a: Segment: %02x\t Bus: 0x%02x\t Device: 0x%02x\t Function: 0x%02x\tVendor ID: 0x%04x\tDevice ID:0x%04x\n",
    __FUNCTION__,
    Segment,
    Bus,
    Device,
    Function,
    Pci.Hdr.VendorId,
    Pci.Hdr.DeviceId
    ));
}

STATIC
VOID
PlatformRegisterFvBootOption (
  CONST EFI_GUID                     *FileGuid,
  CHAR16                             *Description,
  UINT32                             Attributes,
  EFI_BOOT_MANAGER_LOAD_OPTION_TYPE  LoadOptionType
  )
{
  EFI_STATUS                         Status;
  INTN                               OptionIndex;
  EFI_BOOT_MANAGER_LOAD_OPTION       NewOption;
  EFI_BOOT_MANAGER_LOAD_OPTION       *BootOptions;
  UINTN                              BootOptionCount;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  FileNode;
  EFI_LOADED_IMAGE_PROTOCOL          *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL           *DevicePath;

  Status = gBS->HandleProtocol (
                  gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  ASSERT_EFI_ERROR (Status);

  EfiInitializeFwVolDevicepathNode (&FileNode, FileGuid);
  DevicePath = DevicePathFromHandle (LoadedImage->DeviceHandle);
  ASSERT (DevicePath != NULL);
  DevicePath = AppendDevicePathNode (
                 DevicePath,
                 (EFI_DEVICE_PATH_PROTOCOL *)&FileNode
                 );
  ASSERT (DevicePath != NULL);

  Status = EfiBootManagerInitializeLoadOption (
             &NewOption,
             LoadOptionNumberUnassigned,
             LoadOptionType,
             Attributes,
             Description,
             DevicePath,
             NULL,
             0
             );
  ASSERT_EFI_ERROR (Status);
  FreePool (DevicePath);

  BootOptions = EfiBootManagerGetLoadOptions (
                  &BootOptionCount,
                  LoadOptionType
                  );

  OptionIndex = EfiBootManagerFindLoadOption (
                  &NewOption,
                  BootOptions,
                  BootOptionCount
                  );

  if (OptionIndex == -1) {
    Status = EfiBootManagerAddLoadOptionVariable (&NewOption, MAX_UINTN);
    ASSERT_EFI_ERROR (Status);
  }

  EfiBootManagerFreeLoadOption (&NewOption);
  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
}

STATIC
VOID
GetPlatformOptions (
  VOID
  )
{
  EFI_STATUS                      Status;
  EFI_BOOT_MANAGER_LOAD_OPTION    *CurrentBootOptions;
  EFI_BOOT_MANAGER_LOAD_OPTION    *BootOptions;
  EFI_INPUT_KEY                   *BootKeys;
  PLATFORM_BOOT_MANAGER_PROTOCOL  *PlatformBootManager;
  UINTN                           CurrentBootOptionCount;
  UINTN                           Index;
  UINTN                           BootCount;

  Status = gBS->LocateProtocol (
                  &gPlatformBootManagerProtocolGuid,
                  NULL,
                  (VOID **)&PlatformBootManager
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = PlatformBootManager->GetPlatformBootOptionsAndKeys (
                                  &BootCount,
                                  &BootOptions,
                                  &BootKeys
                                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  //
  // Fetch the existent boot options. If there are none, CurrentBootCount
  // will be zeroed.
  //
  CurrentBootOptions = EfiBootManagerGetLoadOptions (
                         &CurrentBootOptionCount,
                         LoadOptionTypeBoot
                         );
  //
  // Process the platform boot options.
  //
  for (Index = 0; Index < BootCount; Index++) {
    INTN   Match;
    UINTN  BootOptionNumber;

    //
    // If there are any preexistent boot options, and the subject platform boot
    // option is already among them, then don't try to add it. Just get its
    // assigned boot option number so we can associate a hotkey with it. Note
    // that EfiBootManagerFindLoadOption() deals fine with (CurrentBootOptions
    // == NULL) if (CurrentBootCount == 0).
    //
    Match = EfiBootManagerFindLoadOption (
              &BootOptions[Index],
              CurrentBootOptions,
              CurrentBootOptionCount
              );
    if (Match >= 0) {
      BootOptionNumber = CurrentBootOptions[Match].OptionNumber;
    } else {
      //
      // Add the platform boot options as a new one, at the end of the boot
      // order. Note that if the platform provided this boot option with an
      // unassigned option number, then the below function call will assign a
      // number.
      //
      Status = EfiBootManagerAddLoadOptionVariable (
                 &BootOptions[Index],
                 MAX_UINTN
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to register \"%s\": %r\n",
          __FUNCTION__,
          BootOptions[Index].Description,
          Status
          ));
        continue;
      }

      BootOptionNumber = BootOptions[Index].OptionNumber;
    }

    //
    // Register a hotkey with the boot option, if requested.
    //
    if (BootKeys[Index].UnicodeChar == L'\0') {
      continue;
    }

    Status = EfiBootManagerAddKeyOptionVariable (
               NULL,
               BootOptionNumber,
               0,
               &BootKeys[Index],
               NULL
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to register hotkey for \"%s\": %r\n",
        __FUNCTION__,
        BootOptions[Index].Description,
        Status
        ));
    }
  }

  EfiBootManagerFreeLoadOptions (CurrentBootOptions, CurrentBootOptionCount);
  EfiBootManagerFreeLoadOptions (BootOptions, BootCount);
  FreePool (BootKeys);
}

/**
  Check if it's a Device Path pointing to BootManagerMenuApp.

  @param  DevicePath     Input device path.

  @retval TRUE   The device path is BootManagerMenuApp File Device Path.
  @retval FALSE  The device path is NOT BootManagerMenuApp File Device Path.
**/
BOOLEAN
IsBootManagerMenuAppFilePath (
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  EFI_HANDLE  FvHandle;
  VOID        *NameGuid;
  EFI_STATUS  Status;

  Status = gBS->LocateDevicePath (&gEfiFirmwareVolume2ProtocolGuid, &DevicePath, &FvHandle);
  if (!EFI_ERROR (Status)) {
    NameGuid = EfiGetNameGuidFromFwVolDevicePathNode ((CONST MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)DevicePath);
    if (NameGuid != NULL) {
      return CompareGuid (NameGuid, PcdGetPtr (PcdBootMenuAppFile));
    }
  }

  return FALSE;
}

/**
  Register boot option for boot menu app and return its boot option instance.

  @param[out]  BootOption     Boot option of boot menu app.

  @retval EFI_SUCCESS             Boot option of boot menu app is registered.
  @retval EFI_NOT_FOUND           No boot menu app is found.
  @retval EFI_INVALID_PARAMETER   BootOption is NULL.
  @retval Others                  Error occurs.
**/
EFI_STATUS
BmRegisterBootMenuApp (
  OUT EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption
  )
{
  EFI_STATUS                Status;
  CHAR16                    *Description;
  UINTN                     DescriptionLength;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  UINTN                     HandleCount;
  EFI_HANDLE                *Handles;
  UINTN                     Index;

  if (BootOption == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  HandleCount = 0;
  Handles     = NULL;
  DevicePath  = NULL;
  Description = NULL;

  //
  // Try to find BootMenu from LoadFile protocol
  //
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiLoadFileProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (!EFI_ERROR (Status)) {
    for (Index = 0; Index < HandleCount; Index++) {
      if (IsBootManagerMenuAppFilePath (DevicePathFromHandle (Handles[Index]))) {
        DevicePath  = DuplicateDevicePath (DevicePathFromHandle (Handles[Index]));
        Description = BmGetBootDescription (Handles[Index]);
        break;
      }
    }

    if (HandleCount != 0) {
      FreePool (Handles);
      Handles = NULL;
    }
  }

  //
  // Not found in LoadFile protocol. Search FV.
  //
  if (DevicePath == NULL) {
    Status = GetFileDevicePathFromAnyFv (
               PcdGetPtr (PcdBootMenuAppFile),
               EFI_SECTION_PE32,
               0,
               &DevicePath
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a: [Bds]Boot Menu App FFS section can not be found, skip its boot option registration\n", __FUNCTION__));
      return EFI_NOT_FOUND;
    }

    ASSERT (DevicePath != NULL);
    //
    // Get BootManagerMenu application's description from EFI User Interface Section.
    //
    Status = GetSectionFromAnyFv (
               PcdGetPtr (PcdBootMenuAppFile),
               EFI_SECTION_USER_INTERFACE,
               0,
               (VOID **)&Description,
               &DescriptionLength
               );
    if (EFI_ERROR (Status)) {
      if (Description != NULL) {
        FreePool (Description);
        Description = NULL;
      }
    }
  }

  //
  // Create new boot option
  //
  Status = EfiBootManagerInitializeLoadOption (
             BootOption,
             LoadOptionNumberUnassigned,
             LoadOptionTypeBoot,
             LOAD_OPTION_CATEGORY_APP | LOAD_OPTION_ACTIVE | LOAD_OPTION_HIDDEN,
             (Description != NULL) ? Description : L"Boot Manager Menu",
             DevicePath,
             NULL,
             0
             );
  ASSERT_EFI_ERROR (Status);

  //
  // Release resource
  //
  if (DevicePath != NULL) {
    FreePool (DevicePath);
  }

  if (Description != NULL) {
    FreePool (Description);
  }

  DEBUG_CODE (
    EFI_BOOT_MANAGER_LOAD_OPTION    *BootOptions;
    UINTN                           BootOptionCount;

    BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);
    ASSERT (EfiBootManagerFindLoadOption (BootOption, BootOptions, BootOptionCount) == -1);
    EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
    );

  return EfiBootManagerAddLoadOptionVariable (BootOption, (UINTN)-1);
}

/**
  Return the boot option number to the boot menu app. If not found it in the
  current boot option, create a new one.

  @param[out] BootOption  Pointer to boot menu app boot option.

  @retval EFI_SUCCESS   Boot option of boot menu app is found and returned.
  @retval Others        Error occurs.

**/
EFI_STATUS
EfiBootManagerGetBootMenuApp (
  OUT EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption
  )
{
  EFI_STATUS                    Status;
  UINTN                         BootOptionCount;
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions;
  UINTN                         Index;

  BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);

  for (Index = 0; Index < BootOptionCount; Index++) {
    if (IsBootManagerMenuAppFilePath (BootOptions[Index].FilePath)) {
      Status = EfiBootManagerInitializeLoadOption (
                 BootOption,
                 BootOptions[Index].OptionNumber,
                 BootOptions[Index].OptionType,
                 BootOptions[Index].Attributes,
                 BootOptions[Index].Description,
                 BootOptions[Index].FilePath,
                 BootOptions[Index].OptionalData,
                 BootOptions[Index].OptionalDataSize
                 );
      ASSERT_EFI_ERROR (Status);
      break;
    }
  }

  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);

  //
  // Automatically create the Boot#### for Boot Menu App when not found.
  //
  if (Index >= BootOptionCount) {
    return BmRegisterBootMenuApp (BootOption);
  }

  return EFI_SUCCESS;
}

/**
  Register the platform boot options and its hotkeys.

  Supported hotkey:
    ENTER: continue boot
    ESC:   Boot manager menu
    F11:   Boot menu app

**/
VOID
PlatformRegisterOptionsAndKeys (
  VOID
  )
{
  EFI_STATUS                    Status;
  EFI_INPUT_KEY                 Enter;
  EFI_INPUT_KEY                 F11;
  EFI_INPUT_KEY                 Esc;
  EFI_BOOT_MANAGER_LOAD_OPTION  BootOption;

  GetPlatformOptions ();

  //
  // Register ENTER as CONTINUE key
  //
  Enter.ScanCode    = SCAN_NULL;
  Enter.UnicodeChar = CHAR_CARRIAGE_RETURN;
  Status            = EfiBootManagerRegisterContinueKeyOption (0, &Enter, NULL);
  ASSERT_EFI_ERROR (Status);

  //
  // Map ESC to Boot Manager Menu
  //
  Esc.ScanCode    = SCAN_ESC;
  Esc.UnicodeChar = CHAR_NULL;
  Status          = EfiBootManagerGetBootManagerMenu (&BootOption);
  ASSERT_EFI_ERROR (Status);

  Status = EfiBootManagerAddKeyOptionVariable (
             NULL,
             (UINT16)BootOption.OptionNumber,
             0,
             &Esc,
             NULL
             );
  ASSERT (Status == EFI_SUCCESS || Status == EFI_ALREADY_STARTED);

  //
  // Map F11 to Boot Menu App (defined by PcdBootMenuAppFile)
  //
  F11.ScanCode    = SCAN_F11;
  F11.UnicodeChar = CHAR_NULL;
  Status          = EfiBootManagerGetBootMenuApp (&BootOption);
  ASSERT_EFI_ERROR (Status);

  Status = EfiBootManagerAddKeyOptionVariable (
             NULL,
             (UINT16)BootOption.OptionNumber,
             0,
             &F11,
             NULL
             );
  ASSERT (Status == EFI_SUCCESS || Status == EFI_ALREADY_STARTED);
}

/**
  System information is displayed at center of screen and hotkey
  information is displayed at upper left corner when GOP is
  available.

**/
VOID
DisplaySystemAndHotkeyInformation (
  VOID
  )
{
  EFI_STATUS                     Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *GraphicsOutput;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Black;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  White;
  CHAR16                         Buffer[100];
  UINTN                          CharCount;
  UINTN                          PosX;
  UINTN                          PosY;
  UINTN                          StartLineX = EFI_GLYPH_WIDTH+2;
  UINTN                          StartLineY = EFI_GLYPH_HEIGHT+1;
  UINTN                          LineDeltaY = EFI_GLYPH_HEIGHT+1;

  //
  // Display hotkey information at upper left corner.
  //
  Black.Blue = Black.Green = Black.Red = Black.Reserved = 0;
  White.Blue = White.Green = White.Red = White.Reserved = 0xFF;

  //
  // Show NVIDIA Internal Banner.
  //
  if (PcdGetBool (PcdTegraPrintInternalBanner)) {
    Print (L"********** FOR NVIDIA INTERNAL USE ONLY **********\n");
  }

  //
  // firmware version.
  //
  CharCount = UnicodeSPrint (
                Buffer,
                sizeof (Buffer),
                L"%s UEFI firmware (version %s built on %s)\n\r",
                (CHAR16 *)PcdGetPtr (PcdPlatformFamilyName),
                (CHAR16 *)PcdGetPtr (PcdFirmwareVersionString),
                (CHAR16 *)PcdGetPtr (PcdFirmwareDateTimeBuiltString)
                );

  //
  // Check and see if GOP is available.
  //
  Status = gBS->HandleProtocol (
                  gST->ConsoleOutHandle,
                  &gEfiGraphicsOutputProtocolGuid,
                  (VOID **)&GraphicsOutput
                  );
  if (!EFI_ERROR (Status)) {
    //
    // Find the center position on screen.
    //
    PosX = (GraphicsOutput->Mode->Info->HorizontalResolution -
            StrLen (Buffer) * EFI_GLYPH_WIDTH) / 2;
    PosY = 0;

    PrintXY (PosX, PosY, NULL, NULL, Buffer);
    PrintXY (StartLineX, StartLineY+LineDeltaY*0, &White, &Black, L"ESC   to enter Setup.                              ");
    PrintXY (StartLineX, StartLineY+LineDeltaY*1, &White, &Black, L"F11   to enter Boot Manager Menu.");
    PrintXY (StartLineX, StartLineY+LineDeltaY*2, &White, &Black, L"Enter to continue boot.");
  }

  //
  // Serial console only.
  //
  Print (Buffer);

  //
  // If Timeout is 0, next message comes in same line as previous message.
  // Add a newline to maintain ordering and readability of logs.
  //
  if (PcdGet16 (PcdPlatformBootTimeOut) == 0) {
    Print (L"\n\r");
  }

  Print (L"ESC   to enter Setup.\n");
  Print (L"F11   to enter Boot Manager Menu.\n");
  Print (L"Enter to continue boot.\n");
}

STATIC
BOOLEAN
IsPlatformConfigurationNeeded (
  VOID
  )
{
  EFI_STATUS                   Status;
  BOOLEAN                      PlatformConfigurationNeeded;
  PLATFORM_CONFIGURATION_DATA  StoredPlatformConfigData;
  UINTN                        VariableSize;
  VOID                         *DTBBase;
  UINTN                        DTBSize;
  VOID                         *AcpiBase;
  UINTN                        CharCount;
  NVIDIA_KERNEL_COMMAND_LINE   AddlCmdLine;
  UINTN                        AddlCmdLen;
  NVIDIA_KERNEL_COMMAND_LINE   AddlCmdLineLast;
  UINTN                        AddlCmdLenLast;
  UINT32                       AddlCmdLineAttributes;

  //
  // If platform has been configured already, do not do it again
  //
  PlatformConfigurationNeeded = FALSE;
  gBS->SetMem (&CurrentPlatformConfigData, sizeof (CurrentPlatformConfigData), 0);

  //
  // Get Current DTB Hash
  //
  Status = DtPlatformLoadDtb (&DTBBase, &DTBSize);
  if (!EFI_ERROR (Status)) {
    Sha256HashAll ((VOID *)DTBBase, DTBSize, CurrentPlatformConfigData.DtbHash);
  }

  //
  // Get Current UEFI Version
  //
  CharCount = AsciiSPrint (
                CurrentPlatformConfigData.UEFIVersion,
                UEFI_VERSION_STRING_SIZE,
                "%s %s",
                (CHAR16 *)PcdGetPtr (PcdFirmwareVersionString),
                (CHAR16 *)PcdGetPtr (PcdFirmwareDateTimeBuiltString)
                );

  //
  // Get OS Hardware Description
  //
  CurrentPlatformConfigData.OsHardwareDescription = OS_USE_DT;
  Status                                          = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    CurrentPlatformConfigData.OsHardwareDescription = OS_USE_ACPI;
  }

  //
  // Get Stored Platform Configuration Data
  //
  VariableSize = sizeof (PLATFORM_CONFIGURATION_DATA);
  Status       = gRT->GetVariable (
                        PLATFORM_CONFIG_DATA_VARIABLE_NAME,
                        &gNVIDIATokenSpaceGuid,
                        NULL,
                        &VariableSize,
                        (VOID *)&StoredPlatformConfigData
                        );
  if (EFI_ERROR (Status) ||
      (VariableSize != sizeof (PLATFORM_CONFIGURATION_DATA)))
  {
    PlatformConfigurationNeeded = TRUE;
  } else {
    if (CompareMem (&StoredPlatformConfigData, &CurrentPlatformConfigData, sizeof (PLATFORM_CONFIGURATION_DATA)) != 0) {
      PlatformConfigurationNeeded = TRUE;
    }
  }

  if (FeaturePcdGet (PcdQuickBootSupported)) {
    if (PcdGet8 (PcdQuickBootEnabled) == 0) {
      PlatformConfigurationNeeded = TRUE;
    }
  } else {
    PlatformConfigurationNeeded = TRUE;
  }

  if (!PlatformConfigurationNeeded) {
    AddlCmdLen = sizeof (AddlCmdLine);
    Status     = gRT->GetVariable (L"KernelCommandLine", &gNVIDIAPublicVariableGuid, &AddlCmdLineAttributes, &AddlCmdLen, &AddlCmdLine);
    if (EFI_ERROR (Status)) {
      AddlCmdLineAttributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS;
      ZeroMem (&AddlCmdLine, sizeof (AddlCmdLine));
    }

    AddlCmdLenLast = sizeof (AddlCmdLineLast);
    Status         = gRT->GetVariable (L"KernelCommandLineLast", &gNVIDIATokenSpaceGuid, NULL, &AddlCmdLenLast, &AddlCmdLineLast);
    if (EFI_ERROR (Status)) {
      ZeroMem (&AddlCmdLenLast, sizeof (AddlCmdLineLast));
    }

    if (CompareMem (&AddlCmdLine, &AddlCmdLineLast, sizeof (AddlCmdLine)) != 0) {
      PlatformConfigurationNeeded = TRUE;
      AddlCmdLenLast              = sizeof (AddlCmdLineLast);
      Status                      = gRT->SetVariable (L"KernelCommandLineLast", &gNVIDIATokenSpaceGuid, AddlCmdLineAttributes, AddlCmdLenLast, &AddlCmdLineLast);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to update stored command line %r\r\n", __FUNCTION__, Status));
      }
    }
  }

  return PlatformConfigurationNeeded;
}

STATIC
VOID
PlatformConfigured (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = gRT->SetVariable (
                  PLATFORM_CONFIG_DATA_VARIABLE_NAME,
                  &gNVIDIATokenSpaceGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  sizeof (PLATFORM_CONFIGURATION_DATA),
                  &CurrentPlatformConfigData
                  );
  if (EFI_ERROR (Status)) {
    // TODO: Evaluate what should be done in this case.
  }
}

STATIC
VOID
PlatformRegisterConsoles (
  VOID
  )
{
  EFI_STATUS                Status;
  EFI_HANDLE                *Handles;
  UINTN                     NoHandles;
  UINTN                     Count;
  EFI_DEVICE_PATH_PROTOCOL  *Interface;

  ASSERT (FixedPcdGet8 (PcdDefaultTerminalType) == 4);
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleTextOutProtocolGuid,
                  NULL,
                  &NoHandles,
                  &Handles
                  );
  if (!EFI_ERROR (Status)) {
    for (Count = 0; Count < NoHandles; Count++) {
      Status = gBS->HandleProtocol (
                      Handles[Count],
                      &gEfiDevicePathProtocolGuid,
                      (VOID **)&Interface
                      );
      if (!EFI_ERROR (Status)) {
        EfiBootManagerUpdateConsoleVariable (ConOut, Interface, NULL);
        EfiBootManagerUpdateConsoleVariable (ErrOut, Interface, NULL);
      }
    }

    gBS->FreePool (Handles);
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleTextInProtocolGuid,
                  NULL,
                  &NoHandles,
                  &Handles
                  );
  if (!EFI_ERROR (Status)) {
    for (Count = 0; Count < NoHandles; Count++) {
      Status = gBS->HandleProtocol (
                      Handles[Count],
                      &gEfiDevicePathProtocolGuid,
                      (VOID **)&Interface
                      );
      if (!EFI_ERROR (Status)) {
        EfiBootManagerUpdateConsoleVariable (ConIn, Interface, NULL);
      }
    }

    gBS->FreePool (Handles);
  }
}

//
// BDS Platform Functions
//

/**
  Do the platform init, can be customized by OEM/IBV
  Possible things that can be done in PlatformBootManagerBeforeConsole:
  > Update console variable: 1. include hot-plug devices;
  >                          2. Clear ConIn and add SOL for AMT
  > Register new Driver#### or Boot####
  > Register new Key####: e.g.: F12
  > Signal ReadyToLock event
  > Authentication action: 1. connect Auth devices;
  >                        2. Identify auto logon user.
**/
VOID
EFIAPI
PlatformBootManagerBeforeConsole (
  VOID
  )
{
  UINT8       *EnrollDefaultKeys;
  EFI_STATUS  Status;
  EFI_HANDLE  BdsHandle = NULL;

  //
  // Signal EndOfDxe PI Event
  //
  EfiEventGroupSignal (&gEfiEndOfDxeEventGroupGuid);

  //
  // Dispatch deferred images after EndOfDxe event.
  //
  EfiBootManagerDispatchDeferredImages ();

  //
  // Locate the PCI root bridges and make the PCI bus driver connect each,
  // non-recursively. This will produce a number of child handles with PciIo on
  // them.
  //
  FilterAndProcess (&gEfiPciRootBridgeIoProtocolGuid, NULL, Connect);

  //
  // Find all display class PCI devices (using the handles from the previous
  // step), and connect them non-recursively. This should produce a number of
  // child handles with GOPs on them.
  //
  FilterAndProcess (&gEfiPciIoProtocolGuid, IsPciDisplay, Connect);

  //
  // Now add the device path of all handles with GOP on them to ConOut and
  // ErrOut.
  //
  FilterAndProcess (&gEfiGraphicsOutputProtocolGuid, NULL, AddOutput);

  //
  // Find all PCI devices (using the handles from the previous step), and
  // list their vendor and device id.
  //
  FilterAndProcess (&gEfiPciIoProtocolGuid, NULL, ListPciDevices);

  if (IsPlatformConfigurationNeeded ()) {
    //
    // Connect the rest of the devices.
    //
    EfiBootManagerConnectAll ();

    //
    // Enumerate all possible boot options.
    //
    EfiBootManagerRefreshAllBootOption ();

    //
    // Register platform-specific boot options and keyboard shortcuts.
    //
    PlatformRegisterOptionsAndKeys ();

    //
    // Register EnrollDefaultKeysApp as a SysPrep Option.
    //
    Status = GetVariable2 (
               L"EnrollDefaultSecurityKeys",
               &gNVIDIAPublicVariableGuid,
               (VOID **)&EnrollDefaultKeys,
               NULL
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: No Default keys to enroll %r.\n",
        __FUNCTION__,
        Status
        ));
    } else {
      if (*EnrollDefaultKeys == 1) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Enroll default keys. %r\n",
          __FUNCTION__,
          Status
          ));
        PlatformRegisterFvBootOption (
          &gEnrollFromDefaultKeysAppFileGuid,
          L"Enroll Default Keys App",
          LOAD_OPTION_ACTIVE,
          LoadOptionTypeSysPrep
          );
      }
    }

    //
    // Register UEFI Shell
    //
    PlatformRegisterFvBootOption (
      &gUefiShellFileGuid,
      L"UEFI Shell",
      LOAD_OPTION_ACTIVE,
      LoadOptionTypeBoot
      );

    //
    // Set Boot Order
    //
    SetBootOrder ();

    //
    // Set platform has been configured
    //
    PlatformConfigured ();
  }

  //
  // Add the hardcoded short-form USB keyboard device path to ConIn.
  //
  EfiBootManagerUpdateConsoleVariable (
    ConIn,
    (EFI_DEVICE_PATH_PROTOCOL *)&mUsbKeyboard,
    NULL
    );

  //
  // Register all available consoles.
  //
  PlatformRegisterConsoles ();

  // Install protocol to indicate that devices are connected
  gBS->InstallMultipleProtocolInterfaces (
         &BdsHandle,
         &gNVIDIABdsDeviceConnectCompleteGuid,
         NULL,
         NULL
         );
  gDS->Dispatch ();
}

STATIC
VOID
HandleCapsules (
  VOID
  )
{
  ESRT_MANAGEMENT_PROTOCOL  *EsrtManagement;
  EFI_PEI_HOB_POINTERS      HobPointer;
  EFI_CAPSULE_HEADER        *CapsuleHeader;
  BOOLEAN                   NeedReset;
  EFI_STATUS                Status;

  DEBUG ((DEBUG_INFO, "%a: processing capsules ...\n", __FUNCTION__));

  Status = gBS->LocateProtocol (
                  &gEsrtManagementProtocolGuid,
                  NULL,
                  (VOID **)&EsrtManagement
                  );
  if (!EFI_ERROR (Status)) {
    EsrtManagement->SyncEsrtFmp ();
  }

  //
  // Find all capsule images from hob
  //
  HobPointer.Raw = GetHobList ();
  NeedReset      = FALSE;
  while ((HobPointer.Raw = GetNextHob (
                             EFI_HOB_TYPE_UEFI_CAPSULE,
                             HobPointer.Raw
                             )) != NULL)
  {
    CapsuleHeader = (VOID *)(UINTN)HobPointer.Capsule->BaseAddress;

    Status = ProcessCapsuleImage (CapsuleHeader);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to process capsule %p - %r\n",
        __FUNCTION__,
        CapsuleHeader,
        Status
        ));
      return;
    }

    NeedReset      = TRUE;
    HobPointer.Raw = GET_NEXT_HOB (HobPointer);
  }

  //
  // Check for capsules on disk
  //
  if (CoDCheckCapsuleOnDiskFlag ()) {
    NeedReset = TRUE;
    Status    = CoDRelocateCapsule (0);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: CoDRelocateCapsule failed: %r\n",
        __FUNCTION__,
        Status
        ));
    }
  }

  //
  // Activate new FW if any capsules installed
  //
  if (NeedReset) {
    switch (PcdGet8 (PcdActivateFwMethod)) {
      case 0x0:
        DEBUG ((DEBUG_WARN, "%a: resetting to activate new firmware ...\n", __FUNCTION__));
        gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
        break;
      case 0x1:
        Print (L"Waiting for power cycle.\n");
        break;
      default:
        ASSERT (FALSE);
        break;
    }

    CpuDeadLoop ();
  }
}

typedef struct {
  UINT8    StaticAddresses;
  UINT8    DynamicAddresses;
  UINT8    Flags;
} IPMI_LAN_IPV6_STATUS;

STATIC
VOID
PrintBmcIpAddresses (
  VOID
  )
{
  EFI_STATUS                                      Status;
  IPMI_TRANSPORT                                  *IpmiTransport;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_REQUEST   GetLanConfigRequest;
  UINT8                                           ResponseData[32];
  UINT32                                          ResponseDataSize;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE  *GetLanConfigResponse;
  IPMI_LAN_IP_ADDRESS                             *IpV4Address;
  IPMI_LAN_IPV6_STATUS                            *IpV6Status;
  IPMI_LAN_IPV6_STATIC_ADDRESS                    *IpV6Address;
  UINT8                                           Index;
  UINT8                                           Index2;

  IpmiTransport        = NULL;
  GetLanConfigResponse = (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *)ResponseData;
  IpV4Address          = (IPMI_LAN_IP_ADDRESS *)GetLanConfigResponse->ParameterData;
  IpV6Status           = (IPMI_LAN_IPV6_STATUS *)GetLanConfigResponse->ParameterData;
  IpV6Address          = (IPMI_LAN_IPV6_STATIC_ADDRESS *)GetLanConfigResponse->ParameterData;

  Status = gBS->LocateProtocol (&gIpmiTransportProtocolGuid, NULL, (VOID **)&IpmiTransport);
  if (EFI_ERROR (Status)) {
    // No IPMI present this is not an error
    return;
  }

  GetLanConfigRequest.ChannelNumber.Uint8 = 0;
  GetLanConfigRequest.ParameterSelector   = IpmiLanIpAddress;
  GetLanConfigRequest.SetSelector         = 0;
  GetLanConfigRequest.BlockSelector       = 0;

  ResponseDataSize = sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE) + sizeof (IPMI_LAN_IPV6_STATUS);
  Status           = IpmiTransport->IpmiSubmitCommand (
                                      IpmiTransport,
                                      IPMI_NETFN_TRANSPORT,
                                      0,
                                      IPMI_TRANSPORT_GET_LAN_CONFIG_PARAMETERS,
                                      (UINT8 *)&GetLanConfigRequest,
                                      sizeof (GetLanConfigRequest),
                                      ResponseData,
                                      &ResponseDataSize
                                      );
  if (EFI_ERROR (Status) || (GetLanConfigResponse->CompletionCode != IPMI_COMP_CODE_NORMAL)) {
    Print (L"Failed to get BMC IPv4 Address\r\n");
  } else {
    Print (L"BMC IPv4 Address: %d.%d.%d.%d\r\n", IpV4Address->IpAddress[0], IpV4Address->IpAddress[1], IpV4Address->IpAddress[2], IpV4Address->IpAddress[3]);
  }

  GetLanConfigRequest.ChannelNumber.Uint8 = 0;
  GetLanConfigRequest.ParameterSelector   = IpmiIpv6Status;
  GetLanConfigRequest.SetSelector         = 0;
  GetLanConfigRequest.BlockSelector       = 0;

  ResponseDataSize = sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE) + sizeof (IPMI_LAN_IPV6_STATUS);
  Status           = IpmiTransport->IpmiSubmitCommand (
                                      IpmiTransport,
                                      IPMI_NETFN_TRANSPORT,
                                      0,
                                      IPMI_TRANSPORT_GET_LAN_CONFIG_PARAMETERS,
                                      (UINT8 *)&GetLanConfigRequest,
                                      sizeof (GetLanConfigRequest),
                                      ResponseData,
                                      &ResponseDataSize
                                      );
  if (EFI_ERROR (Status) || (GetLanConfigResponse->CompletionCode != IPMI_COMP_CODE_NORMAL)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get IPv6 Address count\r\n", __FUNCTION__));
    return;
  }

  // Get Static addresses
  for (Index = 0; Index < IpV6Status->StaticAddresses; Index++) {
    GetLanConfigRequest.ChannelNumber.Uint8 = 0;
    GetLanConfigRequest.ParameterSelector   = IpmiIpv6StaticAddress;
    GetLanConfigRequest.SetSelector         = Index;
    GetLanConfigRequest.BlockSelector       = 0;

    ResponseDataSize = sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE) + sizeof (IPMI_LAN_IPV6_STATIC_ADDRESS);
    Status           = IpmiTransport->IpmiSubmitCommand (
                                        IpmiTransport,
                                        IPMI_NETFN_TRANSPORT,
                                        0,
                                        IPMI_TRANSPORT_GET_LAN_CONFIG_PARAMETERS,
                                        (UINT8 *)&GetLanConfigRequest,
                                        sizeof (GetLanConfigRequest),
                                        ResponseData,
                                        &ResponseDataSize
                                        );
    if (!EFI_ERROR (Status) && (GetLanConfigResponse->CompletionCode == IPMI_COMP_CODE_NORMAL)) {
      if (IpV6Address->AddressStatus == 0) {
        Print (L"BMC IPv6 Static Address: ");
        for (Index2 = 0; Index2 < sizeof (IpV6Address->Ipv6Address); Index2++) {
          if (Index2 != 0) {
            Print (L":");
          }

          Print (L"%02x", IpV6Address->Ipv6Address[Index2]);
        }

        Print (L"\r\n");
      }
    }
  }

  for (Index = 0; Index < IpV6Status->DynamicAddresses; Index++) {
    GetLanConfigRequest.ChannelNumber.Uint8 = 0;
    GetLanConfigRequest.ParameterSelector   = IpmiIpv6DhcpAddress;
    GetLanConfigRequest.SetSelector         = Index;
    GetLanConfigRequest.BlockSelector       = 0;

    ResponseDataSize = sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE) + sizeof (IPMI_LAN_IPV6_STATIC_ADDRESS);
    Status           = IpmiTransport->IpmiSubmitCommand (
                                        IpmiTransport,
                                        IPMI_NETFN_TRANSPORT,
                                        0,
                                        IPMI_TRANSPORT_GET_LAN_CONFIG_PARAMETERS,
                                        (UINT8 *)&GetLanConfigRequest,
                                        sizeof (GetLanConfigRequest),
                                        ResponseData,
                                        &ResponseDataSize
                                        );
    if (!EFI_ERROR (Status) && (GetLanConfigResponse->CompletionCode == IPMI_COMP_CODE_NORMAL)) {
      if (IpV6Address->AddressStatus == 0) {
        Print (L"BMC IPv6 Dynamic Address: ");
        for (Index2 = 0; Index2 < sizeof (IpV6Address->Ipv6Address); Index2++) {
          if (Index2 != 0) {
            Print (L":");
          }

          Print (L"%02x", IpV6Address->Ipv6Address[Index2]);
        }

        Print (L"\r\n");
      }
    }
  }
}

STATIC
VOID
EFIAPI
HandleBootChainUpdate (
  VOID
  )
{
  NVIDIA_BOOT_CHAIN_PROTOCOL  *BootChainProtocol;
  EFI_STATUS                  Status;

  Status = gBS->LocateProtocol (
                  &gNVIDIABootChainProtocolGuid,
                  NULL,
                  (VOID **)&BootChainProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "Boot Chain Protocol Guid=%g not found: %r\n",
      &gNVIDIABootChainProtocolGuid,
      Status
      ));
    return;
  }

  BootChainProtocol->ExecuteUpdate (BootChainProtocol);
}

STATIC
VOID
EFIAPI
VerifyAcpiSanity (
  VOID
  )
{
  EFI_STATUS              Status;
  EFI_ACPI_SDT_PROTOCOL   *AcpiTableProtocol;
  EFI_ACPI_SDT_HEADER     *Table;
  EFI_ACPI_TABLE_VERSION  TableVersion;
  UINTN                   TableKey;
  UINTN                   Count;
  BOOLEAN                 DsdtFound;

  Status = gBS->LocateProtocol (&gEfiAcpiSdtProtocolGuid, NULL, (VOID **)&AcpiTableProtocol);
  if (EFI_ERROR (Status)) {
    return;
  }

  DsdtFound = FALSE;
  for (Count = 0; ; Count++) {
    Status = AcpiTableProtocol->GetAcpiTable (Count, &Table, &TableVersion, &TableKey);
    if (EFI_ERROR (Status)) {
      break;
    }

    if (Table->Signature != EFI_ACPI_6_4_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) {
      continue;
    } else {
      DsdtFound = TRUE;
      break;
    }
  }

  if (DsdtFound != TRUE) {
    DEBUG ((DEBUG_ERROR, "!!!!ACPI Corrupted!!!!\n"));
    ASSERT (FALSE);
  }
}

/**
  Do the platform specific action after the console is ready
  Possible things that can be done in PlatformBootManagerAfterConsole:
  > Console post action:
    > Dynamically switch output mode from 100x31 to 80x25 for certain scenario
    > Signal console ready platform customized event
  > Run diagnostics like memory testing
  > Connect certain devices
  > Dispatch additional option roms
  > Special boot: e.g.: USB boot, enter UI
**/
VOID
EFIAPI
PlatformBootManagerAfterConsole (
  VOID
  )
{
  //
  // Show the splash screen.
  //
  BootLogoEnableLogo ();

  //
  // Display system and hotkey information after console is ready.
  //
  DisplaySystemAndHotkeyInformation ();

  //
  // Run Sparse memory test
  //
  MemoryTest (SPARSE);

  // Ipmi communication
  PrintBmcIpAddresses ();

  //
  // On ARM, there is currently no reason to use the phased capsule
  // update approach where some capsules are dispatched before EndOfDxe
  // and some are dispatched after. So just handle all capsules here,
  // when the console is up and we can actually give the user some
  // feedback about what is going on.
  //
  HandleCapsules ();

  HandleBootChainUpdate ();

  // Validate acpi to be present
  VerifyAcpiSanity ();
}

/**
  This function is called each second during the boot manager waits the
  timeout.

  @param TimeoutRemain  The remaining timeout.
**/
VOID
EFIAPI
PlatformBootManagerWaitCallback (
  UINT16  TimeoutRemain
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION  Black;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION  White;
  UINT16                               Timeout;
  EFI_STATUS                           Status;
  EFI_STRING                           ProgressTitle;

  ProgressTitle = NULL;
  Timeout       = PcdGet16 (PcdPlatformBootTimeOut);
  ProgressTitle = (EFI_STRING)PcdGetPtr (PcdBootManagerWaitMessage);

  ASSERT (ProgressTitle != NULL);

  //
  // BootLogoUpdateProgress does not expect empty string
  //
  if ((ProgressTitle == NULL) || (StrLen (ProgressTitle) == 0)) {
    ProgressTitle = L" ";
  }

  Black.Raw = 0x00000000;
  White.Raw = 0x00FFFFFF;

  Status = BootLogoUpdateProgress (
             White.Pixel,
             Black.Pixel,
             ProgressTitle,
             White.Pixel,
             (Timeout - TimeoutRemain) * 100 / Timeout,
             0
             );
  if (EFI_ERROR (Status)) {
    Print (L".");
  }
}

/**
  The function is called when no boot option could be launched,
  including platform recovery options and options pointing to applications
  built into firmware volumes.

  If this function returns, BDS attempts to enter an infinite loop.
**/
VOID
EFIAPI
PlatformBootManagerUnableToBoot (
  VOID
  )
{
  return;
}

/**
BDS Entry  - DXE phase complete, BDS Entered.
**/
VOID
EFIAPI
PlatformBootManagerBdsEntry (
  VOID
  )
{
  return;
}

/**
 HardKeyBoot
**/
VOID
EFIAPI
PlatformBootManagerPriorityBoot (
  UINT16  **BootNext
  )
{
  return;
}

/**
 This is called from BDS right before going into front page
 when no bootable devices/options found
**/
VOID
EFIAPI
PlatformBootManagerProcessBootCompletion (
  IN EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption
  )
{
  return;
}

/**
  OnDemandConInConnect
**/
VOID
EFIAPI
PlatformBootManagerOnDemandConInConnect (
  VOID
  )
{
  return;
}
