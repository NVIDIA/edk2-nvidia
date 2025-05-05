/** @file
  Implementation for PlatformBootManagerLib library class interfaces.

  SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (C) 2015-2016, Red Hat, Inc.
  Copyright (c) 2014, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/Pci22.h>
#include <Library/BootLogoLib.h>
#include <Library/CapsuleLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/PlatformBootManagerLib.h>
#include <Library/PlatformBootOrderLib.h>
#include <Library/PlatformBootOrderIpmiLib.h>
#include <Library/BaseCryptLib.h>
#include <Library/PerformanceLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <Library/DxeCapsuleLibFmp/CapsuleOnDisk.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/TimerLib.h>
#include <Library/Tcg2PhysicalPresenceLib.h>
#include <Library/TpmPlatformHierarchyLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/StatusRegLib.h>
#include <Protocol/AsyncDriverStatus.h>
#include <Protocol/BootChainProtocol.h>
#include <Protocol/DeferredImageLoad.h>
#include <Protocol/DevicePath.h>
#include <Protocol/EsrtManagement.h>
#include <Protocol/GenericMemoryTest.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/IpmiTransportProtocol.h>
#include <Protocol/MemoryTestConfig.h>
#include <Protocol/PciIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PlatformBootManager.h>
#include <Protocol/ReportStatusCodeHandler.h>
#include <Protocol/SavedCapsuleProtocol.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Uefi/UefiSpec.h>
#include <Guid/EventGroup.h>
#include <Guid/FirmwarePerformance.h>
#include <Guid/RtPropertiesTable.h>
#include <Guid/TtyTerm.h>
#include <Guid/SerialPortLibVendor.h>
#include <IndustryStandard/Ipmi.h>
#include <libfdt.h>
#include <NVIDIAConfiguration.h>
#include "PlatformBm.h"

#define WAIT_POLLED_PER_CYCLE_DELAY  1000      // 1 MS
#define MAX_STRING_SIZE              256

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
EFI_RSC_HANDLER_PROTOCOL            *mRscHandler = NULL;

EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *mForegroundColorPtr = NULL;
EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *mBackgroundColorPtr = NULL;
EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION  mForegroundColor     = { 0 };
EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION  mBackgroundColor     = { 0 };

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
  Prints a string to the console but skips the GOP consoles.

  @param[in] Format  The format string to print.
  @param[in] ...     The arguments to print.
**/
STATIC
VOID
EFIAPI
PrintNonGopConsoles (
  IN CONST CHAR16  *Format,
  ...
  )
{
  EFI_STATUS                       Status;
  EFI_HANDLE                       *Handles;
  UINTN                            NumberOfHandles;
  UINTN                            Index;
  VOID                             *GopInterface;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *SimpleTextOut;
  CHAR16                           String[MAX_STRING_SIZE];
  VA_LIST                          Marker;

  VA_START (Marker, Format);
  UnicodeVSPrint (String, sizeof (String), Format, Marker);
  VA_END (Marker);

  // Get all the console out devices
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiConsoleOutDeviceGuid,
                  NULL /* SearchKey */,
                  &NumberOfHandles,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  // Check if the console out device is a GOP
  for (Index = 0; Index < NumberOfHandles; Index++) {
    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gEfiGraphicsOutputProtocolGuid,
                    (VOID **)&GopInterface
                    );
    if (!EFI_ERROR (Status)) {
      continue;
    }

    // Get the simple text out protocol
    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gEfiSimpleTextOutProtocolGuid,
                    (VOID **)&SimpleTextOut
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    // Print the string to the console
    SimpleTextOut->OutputString (SimpleTextOut, String);
  }

  FreePool (Handles);
}

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
      DEBUG_VERBOSE,
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

  @retval EFI_STATUS    Success test all the system memory and update
                        the memory resource

**/
EFI_STATUS
MemoryTest (
  VOID
  )
{
  EFI_STATUS                          Status;
  EFI_STATUS                          KeyStatus;
  BOOLEAN                             RequireSoftECCInit;
  EFI_GENERIC_MEMORY_TEST_PROTOCOL    *GenMemoryTest;
  UINT64                              TestedMemorySize;
  UINT64                              TotalMemorySize;
  BOOLEAN                             ErrorOut;
  BOOLEAN                             TestAbort;
  EFI_INPUT_KEY                       Key;
  EXTENDMEM_COVERAGE_LEVEL            Level;
  UINT64                              StartTime;
  UINT64                              EndTime;
  UINT64                              TimeTaken;
  NVIDIA_MEMORY_TEST_OPTIONS          *MemoryTestOptions;
  UINTN                               SizeOfBuffer;
  UINT8                               Iteration;
  NVIDIA_MEMORY_TEST_CONFIG_PROTOCOL  *TestConfig;
  CONST CHAR8                         *TestName;

  TestedMemorySize   = 0;
  TotalMemorySize    = 0;
  ErrorOut           = FALSE;
  TestAbort          = FALSE;
  RequireSoftECCInit = FALSE;
  ZeroMem (&Key, sizeof (EFI_INPUT_KEY));

  MemoryTestOptions = PcdGetPtr (PcdMemoryTest);
  NV_ASSERT_RETURN (MemoryTestOptions != NULL, return EFI_DEVICE_ERROR, "Failed to get memory test info\r\n");
  Level = MemoryTestOptions->TestLevel;

  Status = gBS->LocateProtocol (
                  &gEfiGenericMemTestProtocolGuid,
                  NULL,
                  (VOID **)&GenMemoryTest
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to find memory test protocol\r\n"));
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (
                  &gNVIDIAMemoryTestConfig,
                  NULL,
                  (VOID **)&TestConfig
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to find gNVIDIAMemoryTestConfig protocol\r\n"));
    return EFI_SUCCESS;
  }

  if ((MemoryTestOptions->TestIterations < 0) ||
      (MemoryTestOptions->TestIterations > MAX_UINT8))
  {
    DEBUG ((DEBUG_ERROR, "TestIterations out of bounds\r\n"));
    return EFI_SUCCESS;
  }

  for (Iteration = 0; Iteration < MemoryTestOptions->TestIterations; Iteration++) {
    for (TestConfig->TestMode = MemoryTestWalking1Bit;
         TestConfig->TestMode < MemoryTestMaxTest;
         TestConfig->TestMode++)
    {
      switch (TestConfig->TestMode) {
        case MemoryTestWalking1Bit:
          if (!MemoryTestOptions->Walking1BitEnabled) {
            continue;
          }

          TestName = "Walking 1 bit";

          break;
        case MemoryTestAddressCheck:
          if (!MemoryTestOptions->AddressCheckEnabled) {
            continue;
          }

          TestName = "Address Check";

          break;
        case MemoryTestMovingInversions01:
          if (!MemoryTestOptions->MovingInversions01Enabled) {
            continue;
          }

          TestName = "Moving inversions, ones&zeros";

          break;
        case MemoryTestMovingInversions8Bit:
          if (!MemoryTestOptions->MovingInversions8BitEnabled) {
            continue;
          }

          TestName = "Moving inversions, 8 bit pattern";
          break;

        case MemoryTestMovingInversionsRandom:
          if (!MemoryTestOptions->MovingInversionsRandomEnabled) {
            continue;
          }

          TestName = "Moving inversions, random pattern";
          break;

        /*
                case MemoryTestBlockMode:
                  if (!MemoryTestOptions->BlockMoveEnabled) {
                    continue;
                  }

                  TestName = "Block move, 64 moves";
                  break;
        */
        case MemoryTestMovingInversions64Bit:
          if (!MemoryTestOptions->MovingInversions64BitEnabled) {
            continue;
          }

          TestName = "Moving inversions, 64 bit pattern";
          break;
        case MemoryTestRandomNumberSequence:
          if (!MemoryTestOptions->RandomNumberSequenceEnabled) {
            continue;
          }

          TestName = "Random number sequence";
          break;
        case MemoryTestModulo20Random:
          if (!MemoryTestOptions->Modulo20RandomEnabled) {
            continue;
          }

          TestName = "Modulo 20, random pattern";
          break;
        case MemoryTestBitFadeTest:
          if (!MemoryTestOptions->BitFadeEnabled) {
            continue;
          }

          TestName               = "Bit Fade";
          TestConfig->Parameter1 = MemoryTestOptions->BitFadePattern;
          TestConfig->Parameter2 = MemoryTestOptions->BitFadeWait;

          break;
        default:
          continue;
      }

      Print (L"[%03u] %a test starting\r\n", Iteration+1, TestName);
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

      if (MemoryTestOptions->NextBoot) {
        // Disable watchdog as memory tests can take a while.
        gBS->SetWatchdogTimer (0, 0, 0, NULL);
        StartTime = GetTimeInNanoSecond (GetPerformanceCounter ());
        Print (L"Perform memory test (ESC to skip).\r\n");

        do {
          Status = GenMemoryTest->PerformMemoryTest (
                                    GenMemoryTest,
                                    &TestedMemorySize,
                                    &TotalMemorySize,
                                    &ErrorOut,
                                    TestAbort
                                    );
          NV_ASSERT_RETURN (
            !(ErrorOut && (Status == EFI_DEVICE_ERROR)),
            return EFI_DEVICE_ERROR,
            "Memory Testing failed!\r\n"
            );

          Print (L"[%03u] Tested %8lld MB/%8lld MB\r", Iteration+1, TestedMemorySize / SIZE_1MB, TotalMemorySize / SIZE_1MB);

          if (!PcdGetBool (PcdConInConnectOnDemand)) {
            KeyStatus = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
            if (!EFI_ERROR (KeyStatus) && (Key.ScanCode == SCAN_ESC)) {
              if (!RequireSoftECCInit) {
                break;
              }

              TestAbort = TRUE;
            }
          }
        } while (Status != EFI_NOT_FOUND);

        EndTime   = GetTimeInNanoSecond (GetPerformanceCounter ());
        TimeTaken = EndTime - StartTime;
        Print (L"\r\n%llu bytes of system memory tested OK in %llu ms\r\n", TotalMemorySize, TimeTaken/1000000);
      }

      if (TestAbort) {
        break;
      }
    }

    if (TestAbort) {
      break;
    }
  }

  if (MemoryTestOptions->SingleBoot) {
    MemoryTestOptions->NextBoot = FALSE;
    SizeOfBuffer                = sizeof (NVIDIA_MEMORY_TEST_OPTIONS);
    PcdSetPtrS (PcdMemoryTest, &SizeOfBuffer, MemoryTestOptions);
  }

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
    DEBUG ((DEBUG_ERROR, "%a: %s: %r\n", __FUNCTION__, ReportText, Status));
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
    EFI_ERROR (Status) ? DEBUG_ERROR : DEBUG_VERBOSE,
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
      DEBUG_ERROR,
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
      DEBUG_ERROR,
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
      DEBUG_ERROR,
      "%a: %s: adding to ErrOut: %r\n",
      __FUNCTION__,
      ReportText,
      Status
      ));
    return;
  }

  DEBUG ((
    DEBUG_VERBOSE,
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
    DEBUG ((DEBUG_ERROR, "%a: %s: %r\n", __FUNCTION__, ReportText, Status));
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
    DEBUG_ERROR,
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
  Check if it's a Device Path pointing to a specific static app.

  @param  DevicePath     Input device path.
  @param  Guid           The GUID of the app to check.

  @retval TRUE   The device path is the specific static app File Device Path.
  @retval FALSE  The device path is NOT the specific static app File Device Path.
**/
BOOLEAN
IsStaticAppFilePath (
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath,
  EFI_GUID                  *Guid
  )
{
  EFI_HANDLE  FvHandle;
  VOID        *NameGuid;
  EFI_STATUS  Status;

  Status = gBS->LocateDevicePath (&gEfiFirmwareVolume2ProtocolGuid, &DevicePath, &FvHandle);
  if (!EFI_ERROR (Status)) {
    NameGuid = EfiGetNameGuidFromFwVolDevicePathNode ((CONST MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)DevicePath);
    if (NameGuid != NULL) {
      return CompareGuid (NameGuid, Guid);
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
      if (IsStaticAppFilePath (DevicePathFromHandle (Handles[Index]), PcdGetPtr (PcdBootMenuAppFile))) {
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
  Return the boot option number to the specified static app.

  @param[out] BootOption  Pointer to boot menu app boot option.
  @param[in]  Guid        The GUID of the app to check.

  @retval EFI_SUCCESS   Boot option of is found and returned.
  @retval EFI_NOT_FOUND Boot option of the specified app is not found.
  @retval Others        Error occurs.

**/
EFI_STATUS
EfiBootManagerGetStaticApp (
  OUT EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption,
  IN EFI_GUID                       *Guid
  )
{
  EFI_STATUS                    Status;
  UINTN                         BootOptionCount;
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions;
  UINTN                         Index;

  BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);

  for (Index = 0; Index < BootOptionCount; Index++) {
    if (IsStaticAppFilePath (BootOptions[Index].FilePath, Guid)) {
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
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
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
  EFI_STATUS  Status;

  Status = EfiBootManagerGetStaticApp (BootOption, PcdGetPtr (PcdBootMenuAppFile));

  //
  // Automatically create the Boot#### for Boot Menu App when not found.
  //
  if (Status == EFI_NOT_FOUND) {
    return BmRegisterBootMenuApp (BootOption);
  }

  return Status;
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
  EFI_STATUS                    Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *GraphicsOutput;
  CHAR16                        Buffer[150];
  UINTN                         ScreenWidthChars;
  UINTN                         PosX;
  UINTN                         PosY;
  UINTN                         StartLineX = EFI_GLYPH_WIDTH+2;
  UINTN                         LineDeltaY = EFI_GLYPH_HEIGHT+1;
  UINTN                         LineCount  = 0;
  BOOLEAN                       ShellHotkeySupported;

  CheckUefiShellLoadOption (&ShellHotkeySupported);
  if (ShellHotkeySupported && (PcdGet16 (PcdShellHotkey) == CHAR_NULL)) {
    ShellHotkeySupported = FALSE;
  }

  //
  // Display hotkey information at upper left corner.
  //

  //
  // Show NVIDIA Internal Banner.
  //
  if (PcdGetBool (PcdTegraPrintInternalBanner)) {
    Print (L"********** FOR NVIDIA INTERNAL USE ONLY **********\n");
  }

  //
  // firmware version.
  //
  //
  // Serial console only.
  //
  PrintNonGopConsoles (
    L"%s System firmware version %s date %s\n\r",
    (CHAR16 *)PcdGetPtr (PcdPlatformFamilyName),
    (CHAR16 *)PcdGetPtr (PcdFirmwareVersionString),
    (CHAR16 *)PcdGetPtr (PcdFirmwareReleaseDateString)
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
    // Determine the character width of the screen.  We cannot write more
    // characters than this.
    //
    ScreenWidthChars = GraphicsOutput->Mode->Info->HorizontalResolution / EFI_GLYPH_WIDTH;

    //
    // Don't assume our buffer is larger than the screen
    //
    ScreenWidthChars = MIN (ScreenWidthChars, sizeof (Buffer) / sizeof (CHAR16) - 1);

    //
    // Print the system name, version, and date on three separate lines to
    // avoid running out of space on small screens.
    //
    // We'll start from the top and center it up.
    //

    // System name
    PosY = 0;
    UnicodeSPrint (
      Buffer,
      sizeof (Buffer),
      L"%s System firmware",
      (CHAR16 *)PcdGetPtr (PcdPlatformFamilyName)
      );
    Buffer[ScreenWidthChars] = '\0';
    PosX                     = (GraphicsOutput->Mode->Info->HorizontalResolution -
                                StrLen (Buffer) * EFI_GLYPH_WIDTH) / 2;
    PrintXY (PosX, PosY, mForegroundColorPtr, mBackgroundColorPtr, Buffer);

    // Version
    PosY += LineDeltaY;
    UnicodeSPrint (
      Buffer,
      sizeof (Buffer),
      L"version %s",
      (CHAR16 *)PcdGetPtr (PcdFirmwareVersionString)
      );
    Buffer[ScreenWidthChars] = '\0';
    PosX                     = (GraphicsOutput->Mode->Info->HorizontalResolution -
                                StrLen (Buffer) * EFI_GLYPH_WIDTH) / 2;
    PrintXY (PosX, PosY, mForegroundColorPtr, mBackgroundColorPtr, Buffer);

    // Date
    PosY += LineDeltaY;
    UnicodeSPrint (
      Buffer,
      sizeof (Buffer),
      L"date %s",
      (CHAR16 *)PcdGetPtr (PcdFirmwareReleaseDateString)
      );
    Buffer[ScreenWidthChars] = '\0';
    PosX                     = (GraphicsOutput->Mode->Info->HorizontalResolution -
                                StrLen (Buffer) * EFI_GLYPH_WIDTH) / 2;
    PrintXY (PosX, PosY, mForegroundColorPtr, mBackgroundColorPtr, Buffer);

    PosY += LineDeltaY;

    PrintXY (StartLineX, PosY+LineDeltaY*LineCount, mForegroundColorPtr, mBackgroundColorPtr, L"ESC   to enter Setup.");
    LineCount++;
    PrintXY (StartLineX, PosY+LineDeltaY*LineCount, mForegroundColorPtr, mBackgroundColorPtr, L"F11   to enter Boot Manager Menu.");
    LineCount++;
    if (ShellHotkeySupported) {
      PrintXY (StartLineX, PosY+LineDeltaY*LineCount, mForegroundColorPtr, mBackgroundColorPtr, L"%c     to enter Shell.", PcdGet16 (PcdShellHotkey));
      LineCount++;
    }

    PrintXY (StartLineX, PosY+LineDeltaY*LineCount, mForegroundColorPtr, mBackgroundColorPtr, L"Enter to continue boot.");
    LineCount++;
  }

  //
  // If Timeout is 0, next message comes in same line as previous message.
  // Add a newline to maintain ordering and readability of logs.
  //
  if (PcdGet16 (PcdPlatformBootTimeOut) == 0) {
    PrintNonGopConsoles (L"\n\r");
  }

  PrintNonGopConsoles (L"ESC   to enter Setup.\n");
  PrintNonGopConsoles (L"F11   to enter Boot Manager Menu.\n");
  if (ShellHotkeySupported) {
    PrintNonGopConsoles (L"%c     to enter Shell.\n", PcdGet16 (PcdShellHotkey));
  }

  PrintNonGopConsoles (L"Enter to continue boot.\n");
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
  VOID                         *FdtBase;
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
                (CHAR16 *)PcdGetPtr (PcdFirmwareReleaseDateString)
                );

  //
  // Get OS Hardware Description
  //
  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    CurrentPlatformConfigData.OsHardwareDescription = OS_USE_ACPI;
  } else {
    Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &FdtBase);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get ACPI or FDT table\r\n", __FUNCTION__));
      ASSERT_EFI_ERROR (Status);
    }

    CurrentPlatformConfigData.OsHardwareDescription = OS_USE_DT;
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
    ZeroMem (&AddlCmdLine, AddlCmdLen);
    Status = gRT->GetVariable (L"KernelCommandLine", &gNVIDIAPublicVariableGuid, &AddlCmdLineAttributes, &AddlCmdLen, &AddlCmdLine);
    if (EFI_ERROR (Status)) {
      AddlCmdLineAttributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS;
      ZeroMem (&AddlCmdLine, sizeof (AddlCmdLine));
    }

    AddlCmdLenLast = sizeof (AddlCmdLineLast);
    ZeroMem (&AddlCmdLineLast, AddlCmdLenLast);
    Status = gRT->GetVariable (L"KernelCommandLineLast", &gNVIDIATokenSpaceGuid, NULL, &AddlCmdLenLast, &AddlCmdLineLast);
    if (EFI_ERROR (Status)) {
      ZeroMem (&AddlCmdLineLast, sizeof (AddlCmdLineLast));
    }

    if (CompareMem (&AddlCmdLine, &AddlCmdLineLast, sizeof (AddlCmdLine)) != 0) {
      PlatformConfigurationNeeded = TRUE;

      Status = gRT->SetVariable (L"KernelCommandLineLast", &gNVIDIATokenSpaceGuid, AddlCmdLineAttributes, AddlCmdLen, &AddlCmdLine);
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
    DEBUG ((DEBUG_ERROR, "%a: Error setting Platform Config data: %r\r\n", __FUNCTION__, Status));
    // TODO: Evaluate what should be done in this case.
  }
}

/**
  Update ConOut, ErrOut, ConIn variables to contain all available devices.
  For initial boot, all consoles are registered. Afterwards, only GOP consoles
  are registered, as external display devices are dynamically attached.

  @param[in] InitialConsoleRegistration  TRUE:  register all  available ConOut/ErrOut consoles
                                         FALSE: register just NvDisplay ConOut/ErrOut consoles
  @param  none
  @retval none
**/
STATIC
VOID
PlatformRegisterConsoles (
  BOOLEAN  InitialConsoleRegistration
  )
{
  EFI_STATUS                    Status;
  EFI_HANDLE                    *Handles;
  UINTN                         NoHandles;
  UINTN                         Count;
  EFI_DEVICE_PATH_PROTOCOL      *Interface;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;

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
                      &gEfiGraphicsOutputProtocolGuid,
                      (VOID **)&Gop
                      );
      if (!EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_INFO,
          "%a: GraphicsOutputProtocol supported on SimpleTextOutProtocol handle 0x%p\n",
          __FUNCTION__,
          Handles[Count]
          ));
      } else {
        Gop = NULL;
      }

      Status = gBS->HandleProtocol (
                      Handles[Count],
                      &gEfiDevicePathProtocolGuid,
                      (VOID **)&Interface
                      );
      if (!EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_INFO,
          "%a: DevicePathProtocol supported on SimpleTextOutProtocol handle 0x%p\n",
          __FUNCTION__,
          Handles[Count]
          ));
        if ((InitialConsoleRegistration == TRUE) || (Gop != NULL)) {
          EfiBootManagerUpdateConsoleVariable (ConOut, Interface, NULL);
          EfiBootManagerUpdateConsoleVariable (ErrOut, Interface, NULL);
        }
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

/**
  Checks if the image is an option ROM and it has been configured to be disabled.

  @param[in] DevicePath   Device path of the image being checked.

  @retval    TRUE         The image is configured to be disabled.
  @retval    FALSE        Other cases
**/
BOOLEAN
PciOpRomDisabled (
  IN EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  EFI_HANDLE           Handle;
  UINTN                Segment, Bus, Device, Function;
  UINT64               OpRomDis;
  UINTN                VarSize;
  CHAR16               *DevicePathText = NULL;

  Status = gBS->LocateDevicePath (&gEfiPciIoProtocolGuid, &DevicePath, &Handle);
  if (EFI_ERROR (Status) || (Handle == NULL)) {
    return FALSE;
  }

  Status = gBS->HandleProtocol (Handle, &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
  if (EFI_ERROR (Status) || (PciIo == NULL)) {
    return FALSE;
  }

  Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    return FALSE;
  }

  VarSize = sizeof (OpRomDis);
  Status  = gRT->GetVariable (L"OpRomDisSegMask", &gNVIDIAPublicVariableGuid, NULL, &VarSize, &OpRomDis);
  if (EFI_ERROR (Status) || (VarSize != sizeof (OpRomDis))) {
    return FALSE;
  }

  if ((OpRomDis & (1ULL << Segment)) == 0ULL) {
    return FALSE;
  }

  DevicePathText = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
  if (DevicePathText != NULL) {
    DEBUG ((
      DEBUG_INFO,
      "%a: Skip Loading Deferred Image - %s\n",
      __FUNCTION__,
      DevicePathText
      ));
    FreePool (DevicePathText);
  }

  return TRUE;
}

/**
  This function is copied from EfiBootManagerDispatchDeferredImages. But instead of
  dispatching all the deferred images, it checks and only dispatches the images
  that are not specified as disabled.

  @retval EFI_SUCCESS       At least one deferred image is loaded successfully and started.
  @retval EFI_NOT_FOUND     There is no deferred image.
  @retval EFI_ACCESS_DENIED There are deferred images but all of them are failed to load.
**/
EFI_STATUS
EFIAPI
VerifyAndDispatchDeferredImages (
  VOID
  )
{
  EFI_STATUS                        Status;
  EFI_DEFERRED_IMAGE_LOAD_PROTOCOL  *DeferredImage;
  UINTN                             HandleCount;
  EFI_HANDLE                        *Handles;
  UINTN                             Index;
  UINTN                             ImageIndex;
  EFI_DEVICE_PATH_PROTOCOL          *ImageDevicePath;
  VOID                              *Image;
  UINTN                             ImageSize;
  BOOLEAN                           BootOption;
  EFI_HANDLE                        ImageHandle;
  UINTN                             ImageCount;
  UINTN                             LoadCount;

  //
  // Find all the deferred image load protocols.
  //
  HandleCount = 0;
  Handles     = NULL;
  Status      = gBS->LocateHandleBuffer (
                       ByProtocol,
                       &gEfiDeferredImageLoadProtocolGuid,
                       NULL,
                       &HandleCount,
                       &Handles
                       );
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  ImageCount = 0;
  LoadCount  = 0;
  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (Handles[Index], &gEfiDeferredImageLoadProtocolGuid, (VOID **)&DeferredImage);
    if (EFI_ERROR (Status)) {
      continue;
    }

    for (ImageIndex = 0; ; ImageIndex++) {
      //
      // Load all the deferred images in this protocol instance.
      //
      Status = DeferredImage->GetImageInfo (
                                DeferredImage,
                                ImageIndex,
                                &ImageDevicePath,
                                (VOID **)&Image,
                                &ImageSize,
                                &BootOption
                                );
      if (EFI_ERROR (Status)) {
        break;
      }

      //
      // Skip loading option ROM if it is disabled
      //
      if (PciOpRomDisabled (ImageDevicePath)) {
        continue;
      }

      ImageCount++;
      //
      // Load and start the image.
      //
      Status = gBS->LoadImage (
                      BootOption,
                      gImageHandle,
                      ImageDevicePath,
                      NULL,
                      0,
                      &ImageHandle
                      );
      if (EFI_ERROR (Status)) {
        //
        // With EFI_SECURITY_VIOLATION retval, the Image was loaded and an ImageHandle was created
        // with a valid EFI_LOADED_IMAGE_PROTOCOL, but the image can not be started right now.
        // If the caller doesn't have the option to defer the execution of an image, we should
        // unload image for the EFI_SECURITY_VIOLATION to avoid resource leak.
        //
        if (Status == EFI_SECURITY_VIOLATION) {
          gBS->UnloadImage (ImageHandle);
        }
      } else {
        LoadCount++;
        //
        // Before calling the image, enable the Watchdog Timer for
        // a 5 Minute period
        //
        gBS->SetWatchdogTimer (5 * 60, 0x0000, 0x00, NULL);
        gBS->StartImage (ImageHandle, NULL, NULL);

        //
        // Clear the Watchdog Timer after the image returns.
        //
        gBS->SetWatchdogTimer (0x0000, 0x0000, 0x0000, NULL);
      }
    }
  }

  if (Handles != NULL) {
    FreePool (Handles);
  }

  if (ImageCount == 0) {
    return EFI_NOT_FOUND;
  } else {
    if (LoadCount == 0) {
      return EFI_ACCESS_DENIED;
    } else {
      return EFI_SUCCESS;
    }
  }
}

//
// BDS Platform Functions
//

VOID
EFIAPI
CheckUefiShellLoadOption (
  OUT BOOLEAN  *UefiShellEnabled
  )
{
  EFI_STATUS                    Status;
  NVIDIA_UEFI_SHELL_ENABLED     UefiShell;
  UINTN                         VariableSize;
  EFI_BOOT_MANAGER_LOAD_OPTION  BootOption;

  //
  // Get Embedded UEFI Shell Setup Option
  //
  VariableSize = sizeof (NVIDIA_UEFI_SHELL_ENABLED);
  Status       = gRT->GetVariable (
                        L"UefiShellEnabled",
                        &gNVIDIAPublicVariableGuid,
                        NULL,
                        &VariableSize,
                        (VOID *)&UefiShell
                        );
  if ((EFI_ERROR (Status) || UefiShell.Enabled) && (PcdGet8 (PcdUefiShellEnabled))) {
    *UefiShellEnabled = TRUE;
    return;
  }

  //
  // Remove Embedded UEFI Shell Setup Option
  //
  *UefiShellEnabled = FALSE;
  Status            = EfiBootManagerGetStaticApp (&BootOption, &gUefiShellFileGuid);
  if (!EFI_ERROR (Status)) {
    EfiBootManagerDeleteLoadOptionVariable (
      BootOption.OptionNumber,
      LoadOptionTypeBoot
      );
  }
}

/**
  Process TPM PPI commands
**/
VOID
ProcessTpmPhysicalPresence (
  VOID
  )
{
  if (!PcdGetBool (PcdTpmEnable)) {
    return;
  }

  Tcg2PhysicalPresenceLibProcessRequest (NULL);
}

/**
  Lock TPM platform hierarchy to prevent OS from changing TPM platform settings
**/
VOID
LockTpmPlatformHierarchy (
  VOID
  )
{
  if (!PcdGetBool (PcdTpmEnable)) {
    return;
  }

  ConfigureTpmPlatformHierarchy ();
}

/**
  Wait for all async drivers to complete
**/
VOID
EFIAPI
WaitForAsyncDrivers (
  VOID
  )
{
  EFI_STATUS                           Status;
  UINTN                                Handles;
  EFI_HANDLE                           *HandleBuffer;
  UINTN                                HandleIndex;
  NVIDIA_ASYNC_DRIVER_STATUS_PROTOCOL  *AsyncProtocol;
  BOOLEAN                              StillPending;
  BOOLEAN                              PrintedForDriver;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gNVIDIAAsyncDriverStatusProtocol, NULL, &Handles, &HandleBuffer);
  if (EFI_ERROR (Status)) {
    return;
  }

  PERF_START (&gEfiCallerIdGuid, "AsyncDriverWait", NULL, 0);
  for (HandleIndex = 0; HandleIndex < Handles; HandleIndex++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[HandleIndex],
                    &gNVIDIAAsyncDriverStatusProtocol,
                    (VOID **)&AsyncProtocol
                    );
    ASSERT_EFI_ERROR (Status);
    if (EFI_ERROR (Status)) {
      continue;
    }

    PrintedForDriver = FALSE;
    do {
      AsyncProtocol->GetStatus (AsyncProtocol, &StillPending);
      if (StillPending) {
        if (!PrintedForDriver) {
          DEBUG ((DEBUG_ERROR, "Waiting for driver %u of %u to complete\r\n.", HandleIndex+1, Handles));
          PrintedForDriver = TRUE;
        }

        CpuPause ();
      }
    } while (StillPending);
  }

  if (Handles != 0) {
    gDS->Dispatch ();
  }

  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
  }

  PERF_END (&gEfiCallerIdGuid, "AsyncDriverWait", NULL, 0);
}

/**
  Wait for polled enumeration to finish

  This is used to wait for any enumeration that is polled, for example USB devices.
**/
STATIC
VOID
WaitForPolledEnumeration (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       BufferSize;
  UINTN       HandleCount;
  UINTN       PriorHandleCount;
  UINTN       OriginalHandleCount;
  UINTN       TotalTimeout;
  UINTN       CurrentTimeout;
  UINTN       EnumerationTimeout;

  EnumerationTimeout = PcdGet32 (PcdEnumerationTimeoutMs) * 1000ULL;
  if (EnumerationTimeout == 0) {
    return;
  }

  PriorHandleCount = 0;
  TotalTimeout     = 0;
  CurrentTimeout   = 0;

  BufferSize = 0;
  Status     = gBS->LocateHandle (
                      AllHandles,
                      NULL,
                      NULL,
                      &BufferSize,
                      NULL
                      );
  if (Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG ((DEBUG_ERROR, "LocateHandle failed %r - expected BUFFER_TO_SMALL\r\n", Status));
    return;
  }

  OriginalHandleCount = BufferSize / sizeof (EFI_HANDLE);
  DEBUG ((DEBUG_ERROR, "Start new device enumeration polling\r\n"));
  PriorHandleCount = OriginalHandleCount;
  HandleCount      = OriginalHandleCount;

  //
  // Wait for any polled enumeration to finish
  //
  while (TRUE) {
    BufferSize = 0;
    Status     = gBS->LocateHandle (
                        AllHandles,
                        NULL,
                        NULL,
                        &BufferSize,
                        NULL
                        );
    if (Status != EFI_BUFFER_TOO_SMALL) {
      DEBUG ((DEBUG_ERROR, "LocateHandle failed %r - expected BUFFER_TO_SMALL\r\n", Status));
      break;
    }

    HandleCount = BufferSize / sizeof (EFI_HANDLE);
    if ((PriorHandleCount != 0) && (HandleCount != PriorHandleCount)) {
      DEBUG ((DEBUG_ERROR, "New device found after %u ms\r\n", CurrentTimeout / 1000));
      PriorHandleCount = HandleCount;
      CurrentTimeout   = 0;
    } else if (CurrentTimeout >= EnumerationTimeout) {
      break;
    }

    gBS->Stall (WAIT_POLLED_PER_CYCLE_DELAY);
    TotalTimeout   += WAIT_POLLED_PER_CYCLE_DELAY;
    CurrentTimeout += WAIT_POLLED_PER_CYCLE_DELAY;
  }

  DEBUG ((DEBUG_ERROR, "Polled enumeration took %u ms, found %u devices\r\n", TotalTimeout / 1000, HandleCount - OriginalHandleCount));
}

/**
  Deterimes if the single boot path should be taken and returns the app to
  launch if that is the case.
  This also detects if the system in RCM mode and willl return that app if
  specified at build time.

  @param[out] - Guid to boot if single boot path should be taken

  @retval TRUE  - Single Boot path should be taken
  @retval FALSE - Normal Boot path should be taken.
 */
BOOLEAN
EFIAPI
PlatformGetSingleBootApp (
  EFI_GUID  **AppGuid OPTIONAL
  )
{
  VOID                          *Hob;
  EFI_GUID                      *LocalAppGuid;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
    if (PlatformResourceInfo->BootType == TegrablBootRcm) {
      LocalAppGuid = FixedPcdGetPtr (PcdRcmBootApplicationGuid);
      if (!IsZeroGuid (LocalAppGuid)) {
        if (AppGuid != NULL) {
          *AppGuid = LocalAppGuid;
        }

        return TRUE;
      }
    }
  } else {
    DEBUG ((DEBUG_ERROR, "Failed to get PlatformResourceInfo\n"));
  }

  if (FeaturePcdGet (PcdSingleBootSupport)) {
    LocalAppGuid = FixedPcdGetPtr (PcdSingleBootApplicationGuid);
    if (!IsZeroGuid (LocalAppGuid)) {
      if (AppGuid != NULL) {
        *AppGuid = LocalAppGuid;
      }

      return TRUE;
    }
  }

  return FALSE;
}

/**
  Detect a boot failure in single boot mode and halt it that occurs.
 */
STATIC
EFI_STATUS
SingleBootStatusCodeCallback (
  IN EFI_STATUS_CODE_TYPE   CodeType,
  IN EFI_STATUS_CODE_VALUE  Value,
  IN UINT32                 Instance,
  IN EFI_GUID               *CallerId,
  IN EFI_STATUS_CODE_DATA   *Data
  )
{
  if (((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_ERROR_CODE)  &&
      ((Value == (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_EC_BOOT_OPTION_LOAD_ERROR)) ||
       (Value == (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_EC_BOOT_OPTION_FAILED))))
  {
    DEBUG ((DEBUG_ERROR, "Single Boot/RCM Failure detected, halting system\n"));
    CpuDeadLoop ();
  }

  if (((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_PROGRESS_CODE) &&
      (Value == (EFI_SOFTWARE_EFI_BOOT_SERVICE | EFI_SW_BS_PC_EXIT_BOOT_SERVICES)))
  {
    if (mRscHandler != NULL) {
      mRscHandler->Unregister (SingleBootStatusCodeCallback);
    }
  }

  return EFI_SUCCESS;
}

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
  EFI_STATUS                    Status;
  EFI_HANDLE                    BdsHandle = NULL;
  BOOLEAN                       UefiShellEnabled;
  BOOLEAN                       PlatformReconfigured = FALSE;
  EFI_BOOT_MANAGER_LOAD_OPTION  BootOption;
  EFI_INPUT_KEY                 ShellKey;
  BOOLEAN                       SingleBoot;
  EFI_GUID                      *SingleBootAppGuid;

  if (FeaturePcdGet (PcdMemoryTestsSupported)) {
    // Attempt to delete variable to prevent forced allocation at targeted address.
    // This can fail causing memory promotion to fail.
    gRT->SetVariable (
           EFI_FIRMWARE_PERFORMANCE_VARIABLE_NAME,
           &gEfiFirmwarePerformanceGuid,
           EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
           0,
           NULL
           );
  }

  SingleBoot = PlatformGetSingleBootApp (&SingleBootAppGuid);

  if (!SingleBoot) {
    //
    // Check Embedded UEFI Shell Setup Option
    //
    CheckUefiShellLoadOption (&UefiShellEnabled);

    //
    // Check IPMI for BootOrder commands, and clear and reset CMOS here if requested
    //
    CheckIPMIForBootOrderUpdates ();

    //
    // Restore the BootOrder if we temporarily changed it during the previous boot and haven't restored it yet
    //
    RestoreBootOrder (NULL, NULL);
  }

  //
  // Wait for all async drivers to complete
  //
  WaitForAsyncDrivers ();

  //
  // Signal EndOfDxe PI Event
  //
  EfiEventGroupSignal (&gEfiEndOfDxeEventGroupGuid);

  //
  // Dispatch deferred images after EndOfDxe event.
  // Call customized version of EfiBootManagerDispatchDeferredImages to bypass
  // pre-specified PCI option ROMs.
  //
  VerifyAndDispatchDeferredImages ();

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

  if (!SingleBoot) {
    if (IsPlatformConfigurationNeeded ()) {
      PlatformReconfigured = TRUE;

      //
      // Connect the rest of the devices.
      //
      EfiBootManagerConnectAll ();

      //
      // Wait for any polled enumeration to finish
      //
      WaitForPolledEnumeration ();

      //
      // Signal ConnectComplete Event
      //
      EfiEventGroupSignal (&gNVIDIAConnectCompleteEventGuid);

      //
      // Enumerate all possible boot options.
      //
      EfiBootManagerRefreshAllBootOption ();

      //
      // Register platform-specific boot options and keyboard shortcuts.
      //
      PlatformRegisterOptionsAndKeys ();

      //
      // Register UEFI Shell
      //
      if (UefiShellEnabled) {
        PlatformRegisterFvBootOption (
          &gUefiShellFileGuid,
          L"UEFI Shell",
          LOAD_OPTION_ACTIVE,
          LoadOptionTypeBoot
          );
        Status = EfiBootManagerGetStaticApp (&BootOption, &gUefiShellFileGuid);
        if (!EFI_ERROR (Status)) {
          ShellKey.ScanCode    = SCAN_NULL;
          ShellKey.UnicodeChar = PcdGet16 (PcdShellHotkey);
          if (ShellKey.UnicodeChar != CHAR_NULL) {
            EfiBootManagerAddKeyOptionVariable (
              NULL,
              (UINT16)BootOption.OptionNumber,
              0,
              &ShellKey,
              NULL
              );
          }
        }
      }

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
    // Process IPMI-directed BootOrder updates
    //
    ProcessIPMIBootOrderUpdates ();
  } else {
    //
    // Connect the rest of the devices.
    //
    EfiBootManagerConnectAll ();

    //
    // Signal ConnectComplete Event
    //
    EfiEventGroupSignal (&gNVIDIAConnectCompleteEventGuid);

    // Don't wait for timeout
    PcdSet16S (PcdPlatformBootTimeOut, 0);

    PlatformRegisterFvBootOption (
      SingleBootAppGuid,
      L"Boot Application",
      LOAD_OPTION_ACTIVE,
      LoadOptionTypeBoot
      );

    Status = gBS->LocateProtocol (&gEfiRscHandlerProtocolGuid, NULL, (VOID **)&mRscHandler);
    ASSERT_EFI_ERROR (Status);
    if (!EFI_ERROR (Status)) {
      mRscHandler->Register (SingleBootStatusCodeCallback, TPL_CALLBACK);
    }
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
  // Register all available consoles during intitial
  // boot, then set PCD to FALSE afterwards.
  //
  PlatformRegisterConsoles (
    PcdGetBool (PcdDoInitialConsoleRegistration) ||
    PlatformReconfigured
    );
  if (PcdGetBool (PcdDoInitialConsoleRegistration) == TRUE) {
    PcdSetBoolS (PcdDoInitialConsoleRegistration, FALSE);
  }

  //
  // Signal BeforeConsoleEvent.
  //
  EfiEventGroupSignal (&gNVIDIABeforeConsoleEventGuid);

  //
  // Process TPM PPI
  //
  ProcessTpmPhysicalPresence ();

  // Install protocol to indicate that devices are connected
  gBS->InstallMultipleProtocolInterfaces (
         &BdsHandle,
         &gNVIDIABdsDeviceConnectCompleteGuid,
         NULL,
         NULL
         );
  Status = gDS->Dispatch ();
  // Connect drivers if new driver was dispatched.
  // Do this if the platform is doing full connects
  if (PlatformReconfigured && !EFI_ERROR (Status)) {
    EfiBootManagerConnectAll ();
  }
}

STATIC
VOID
EFIAPI
HandleSavedCapsules (
  OUT BOOLEAN  *NeedReset
  )
{
  EFI_STATUS                     Status;
  NVIDIA_SAVED_CAPSULE_PROTOCOL  *Protocol;
  EFI_CAPSULE_HEADER             *CapsuleHeader;

  Status = gBS->LocateProtocol (
                  &gNVIDIASavedCapsuleProtocolGuid,
                  NULL,
                  (VOID **)&Protocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: no protocol: %r\n", __FUNCTION__, Status));
    return;
  }

  Status = Protocol->GetCapsule (Protocol, &CapsuleHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetCapsule failed\n", __FUNCTION__));
    return;
  }

  DEBUG ((DEBUG_INFO, "%a: installing capsule bytes=%u guid=%g\n", __FUNCTION__, CapsuleHeader->CapsuleImageSize, &CapsuleHeader->CapsuleGuid));

  ValidateActiveBootChain ();
  Status = gRT->UpdateCapsule (&CapsuleHeader, 1, (UINTN)NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: UpdateCapsule failed: %r\n", __FUNCTION__, Status));
  }

  *NeedReset = 1;
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
    // Mark existing boot chain as good.
    ValidateActiveBootChain ();

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
  // Check for saved capsules
  //
  HandleSavedCapsules (&NeedReset);

  //
  // Activate new FW if any capsules installed
  //
  if (NeedReset) {
    DEBUG ((DEBUG_WARN, "%a: resetting to activate new firmware ...\n", __FUNCTION__));

    StatusRegReset ();
    gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
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

  GetLanConfigRequest.ChannelNumber.Uint8 = 1;
  GetLanConfigRequest.ParameterSelector   = IpmiLanIpAddress;
  GetLanConfigRequest.SetSelector         = 0;
  GetLanConfigRequest.BlockSelector       = 0;

  ResponseDataSize = sizeof (IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE) + sizeof (IPMI_LAN_IP_ADDRESS);
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
  if (Status == EFI_UNSUPPORTED) {
    // IPMI is not actually supported
    return;
  } else if (EFI_ERROR (Status) || (GetLanConfigResponse->CompletionCode != IPMI_COMP_CODE_NORMAL)) {
    Print (L"Failed to get BMC IPv4 Address\r\n");
  } else {
    Print (L"BMC IPv4 Address: %d.%d.%d.%d\r\n", IpV4Address->IpAddress[0], IpV4Address->IpAddress[1], IpV4Address->IpAddress[2], IpV4Address->IpAddress[3]);
  }

  GetLanConfigRequest.ChannelNumber.Uint8 = 1;
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
    GetLanConfigRequest.ChannelNumber.Uint8 = 1;
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
    GetLanConfigRequest.ChannelNumber.Uint8 = 1;
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
  // Set the foreground and background colors if custom colors are enabled
  if (PcdGetBool (PcdBootManagerCustomColors)) {
    mForegroundColor.Raw = PcdGet32 (PcdBootManagerForegroundColor);
    mBackgroundColor.Raw = PcdGet32 (PcdBootManagerBackgroundColor);
    mForegroundColorPtr  = &mForegroundColor.Pixel;
    mBackgroundColorPtr  = &mBackgroundColor.Pixel;
  }

  // Print the BootOrder information
  PrintCurrentBootOrder (DEBUG_ERROR);

  //
  // Show the splash screen.
  //
  BootLogoEnableLogo ();

  //
  // Display system and hotkey information after console is ready.
  //
  if (!PlatformGetSingleBootApp (NULL)) {
    DisplaySystemAndHotkeyInformation ();
  }

  //
  // Run memory test
  //
  MemoryTest ();

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

  //
  // Lock TPM platform hierarchy
  //
  LockTpmPlatformHierarchy ();

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
