/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2017, Linaro Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiPei.h>
#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/HobLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/FwVariableLib.h>
#include <Library/PlatformBootOrderLib.h>
#include <Guid/GlobalVariable.h>
#include <Protocol/PciIo.h>
#include "InternalPlatformBootOrderLib.h"

NVIDIA_BOOT_ORDER_PRIORITY  mBootPriorityTemplate[BOOT_ORDER_TEMPLATE_CLASS_COUNT] = {
  { "scsi",     MAX_INT32, MESSAGING_DEVICE_PATH, MSG_SCSI_DP,           MAX_UINT8,                MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "usb",      MAX_INT32, MESSAGING_DEVICE_PATH, MSG_USB_DP,            MAX_UINT8,                MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "sata",     MAX_INT32, MESSAGING_DEVICE_PATH, MSG_SATA_DP,           MAX_UINT8,                MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "pxev4",    MAX_INT32, MESSAGING_DEVICE_PATH, MSG_IPv4_DP,           MAX_UINT8,                MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "httpv4",   MAX_INT32, MESSAGING_DEVICE_PATH, MSG_IPv4_DP,           NVIDIA_BOOT_TYPE_HTTP,    MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "pxev6",    MAX_INT32, MESSAGING_DEVICE_PATH, MSG_IPv6_DP,           MAX_UINT8,                MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "httpv6",   MAX_INT32, MESSAGING_DEVICE_PATH, MSG_IPv6_DP,           NVIDIA_BOOT_TYPE_HTTP,    MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "nvme",     MAX_INT32, MESSAGING_DEVICE_PATH, MSG_NVME_NAMESPACE_DP, MAX_UINT8,                MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "ufs",      MAX_INT32, MESSAGING_DEVICE_PATH, MSG_UFS_DP,            MAX_UINT8,                MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "sd",       MAX_INT32, MESSAGING_DEVICE_PATH, MSG_SD_DP,             MAX_UINT8,                MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "emmc",     MAX_INT32, MESSAGING_DEVICE_PATH, MSG_EMMC_DP,           MAX_UINT8,                MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "cdrom",    MAX_INT32, MEDIA_DEVICE_PATH,     MEDIA_CDROM_DP,        MAX_UINT8,                MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "boot.img", MAX_INT32, MAX_UINT8,             MAX_UINT8,             NVIDIA_BOOT_TYPE_BOOTIMG, MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "virtual",  MAX_INT32, MESSAGING_DEVICE_PATH, MSG_USB_DP,            NVIDIA_BOOT_TYPE_VIRTUAL, MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN },
  { "shell",    MAX_INT32, MAX_UINT8,             MAX_UINT8,             MAX_UINT8,                MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN }
};

STATIC  NVIDIA_BOOT_ORDER_PRIORITY  *mBootPriorityTable = NULL;
STATIC  UINTN                       mBootPriorityCount  = 0;

STATIC
EFI_PCI_IO_PROTOCOL *
GetBootOptPciIoProtocol (
  IN EFI_BOOT_MANAGER_LOAD_OPTION  *Option
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *LatestPciDevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *TempDevicePath;
  UINTN                     HandleCount;
  EFI_HANDLE                *HandleBuffer;
  EFI_STRING                DevicePathText;
  EFI_STRING                TempDevicePathText;
  EFI_STRING                PciIoDevicePathText;
  UINTN                     Index;
  UINTN                     TailingCharCount;
  UINTN                     FullLength;
  UINTN                     BootOptPciIoDevicePathTextLen;
  EFI_PCI_IO_PROTOCOL       *PciIo;
  EFI_STATUS                Status;

  DevicePath                    = Option->FilePath;
  LatestPciDevicePath           = NULL;
  TempDevicePath                = NULL;
  TailingCharCount              = 0;
  PciIo                         = NULL;
  TempDevicePathText            = NULL;
  PciIoDevicePathText           = NULL;
  HandleBuffer                  = NULL;
  BootOptPciIoDevicePathTextLen = 0;

  DevicePathText = ConvertDevicePathToText (DevicePath, TRUE, FALSE);
  if (DevicePathText == NULL) {
    goto Done;
  }

  FullLength = StrLen (DevicePathText);

  for ( ; !IsDevicePathEnd (DevicePath); DevicePath = NextDevicePathNode (DevicePath)) {
    if ((DevicePath->Type == HARDWARE_DEVICE_PATH) && (DevicePath->SubType == HW_PCI_DP)) {
      LatestPciDevicePath = DevicePath;
    }
  }

  if ((LatestPciDevicePath != NULL) && (LatestPciDevicePath != DevicePath)) {
    TempDevicePathText = ConvertDevicePathToText (
                           NextDevicePathNode (LatestPciDevicePath),
                           TRUE,    // DisplayOnly
                           FALSE    // AllowShortcuts
                           );
    if (TempDevicePathText != NULL) {
      TailingCharCount = StrLen (TempDevicePathText) + 1;
      FreePool (TempDevicePathText);
    }
  }

  BootOptPciIoDevicePathTextLen = FullLength - TailingCharCount;

  if (gBS->LocateHandleBuffer != NULL) {
    Status = gBS->LocateHandleBuffer (
                    ByProtocol,
                    &gEfiPciIoProtocolGuid,
                    NULL,
                    &HandleCount,
                    &HandleBuffer
                    );
    if (!EFI_ERROR (Status) && (HandleCount > 0)) {
      for (Index = 0; Index < HandleCount; Index++) {
        TempDevicePath = DevicePathFromHandle (HandleBuffer[Index]);
        if (TempDevicePath == NULL) {
          continue;
        }

        PciIoDevicePathText = ConvertDevicePathToText (
                                TempDevicePath,
                                TRUE, // DisplayOnly
                                FALSE // AllowShortcuts
                                );
        if (PciIoDevicePathText != NULL) {
          if (StrnCmp (PciIoDevicePathText, DevicePathText, BootOptPciIoDevicePathTextLen) == 0) {
            gBS->HandleProtocol (
                   HandleBuffer[Index],
                   &gEfiPciIoProtocolGuid,
                   (VOID **)&PciIo
                   );
            goto Done;
          }

          FreePool (PciIoDevicePathText);
          PciIoDevicePathText = NULL;
        }
      }
    }
  }

Done:

  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
  }

  if (DevicePathText != NULL) {
    FreePool (DevicePathText);
  }

  if (PciIoDevicePathText != NULL) {
    FreePool (PciIoDevicePathText);
  }

  return PciIo;
}

/**
  Extract Boot Order Sbdf text string to get PCI location information.

  @param   SbdfText      Pointer to Hex text string with colon sign as separator. Format is like <Seg>:<Bus>:<Dev>:<Func>
  @param   SbdfTextLen   Input Hex text string length.
  @param   Segment       The Boot Order's PCI segment number.
  @param   Bus           The Boot Order's PCI bus number.
  @param   Device        The Boot Order's PCI device number.
  @param   Function      The Boot Order's PCI function number.

  @return   VOID.
**/
VOID
EFIAPI
GetBootOrderSbdf (
  IN  CHAR8  *BootOrderSbdfText,
  IN  UINTN  BootOrderSbdfTextLen,
  OUT UINTN  *Segment,
  OUT UINTN  *Bus,
  OUT UINTN  *Device,
  OUT UINTN  *Function
  )
{
  EFI_STATUS  Status;
  CHAR8       *HexTextStart;
  CHAR8       *HexTextEnd;
  UINTN       Index;
  UINTN       SbdfNum[4] = { MAX_UINTN, MAX_UINTN, MAX_UINTN, MAX_UINTN };

  ASSERT (*BootOrderSbdfText == BOOT_ORDER_SBDF_STARTER);

  HexTextStart = BootOrderSbdfText + 1;

  for (Index = 0; Index < 4; Index++) {
    Status = AsciiStrHexToUintnS (HexTextStart, &HexTextEnd, &SbdfNum[Index]);

    ASSERT_EFI_ERROR (Status);

    if (EFI_ERROR (Status) || (*HexTextEnd != BOOT_ORDER_SBDF_SEPARATOR)) {
      break;
    }

    HexTextStart = HexTextEnd + 1;
  }

  *Segment  = SbdfNum[0];
  *Bus      = SbdfNum[1];
  *Device   = SbdfNum[2];
  *Function = SbdfNum[3];
}

VOID
PrintBootOrder (
  IN CONST UINTN   DebugPrintLevel,
  IN CONST CHAR16  *HeaderMessage,
  IN UINT16        *BootOrder,
  IN UINTN         BootOrderSize
  )
{
  EFI_STATUS                    Status;
  UINT16                        *LocalBootOrder;
  UINTN                         LocalBootOrderSize;
  UINTN                         BootOrderLength;
  UINTN                         BootOrderIndex;
  CHAR16                        OptionName[sizeof ("Boot####")];
  EFI_BOOT_MANAGER_LOAD_OPTION  Option;
  CONST CHAR8                   *AttributeString;

  if (!DebugPrintLevelEnabled (DebugPrintLevel)) {
    return;
  }

  LocalBootOrder = NULL;

  // Gather BootOrder if needed
  if ((BootOrder == NULL) || (BootOrderSize == 0)) {
    Status = GetEfiGlobalVariable2 (EFI_BOOT_ORDER_VARIABLE_NAME, (VOID **)&LocalBootOrder, &LocalBootOrderSize);
    if (Status == EFI_NOT_FOUND) {
      DEBUG ((DebugPrintLevel, "%a: No BootOrder found\n", __FUNCTION__));
      goto CleanupAndReturn;
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((DebugPrintLevel, "%a: Unable to determine BootOrder: %r\n", __FUNCTION__, Status));
      goto CleanupAndReturn;
    }

    BootOrder     = LocalBootOrder;
    BootOrderSize = LocalBootOrderSize;
  }

  if ((BootOrder == NULL) || (BootOrderSize == 0)) {
    DEBUG ((DebugPrintLevel, "%a: BootOrder is empty\n", __FUNCTION__));
    goto CleanupAndReturn;
  }

  BootOrderLength = BootOrderSize/sizeof (BootOrder[0]);

  DEBUG ((DebugPrintLevel, "%a: %s\n", __FUNCTION__, HeaderMessage));
  for (BootOrderIndex = 0; BootOrderIndex < BootOrderLength; BootOrderIndex++) {
    UnicodeSPrint (OptionName, sizeof (OptionName), L"Boot%04x", BootOrder[BootOrderIndex]);
    Status = EfiBootManagerVariableToLoadOption (OptionName, &Option);
    if (EFI_ERROR (Status)) {
      DEBUG ((DebugPrintLevel, "%a: Error getting boot option for BootOrder[%llu] = 0x%04x: %r\n", __FUNCTION__, BootOrderIndex, BootOrder[BootOrderIndex], Status));
      goto CleanupAndReturn;
    }

    AttributeString = "";
    if (Option.Attributes & LOAD_OPTION_HIDDEN) {
      AttributeString = "  [Hidden - will be skipped]";
    } else if (!(Option.Attributes & LOAD_OPTION_ACTIVE)) {
      AttributeString = "  [Inactive - will be skipped]";
    } else if (Option.Attributes & LOAD_OPTION_CATEGORY_APP) {
      AttributeString = "  [App - will be skipped]";
    }

    DEBUG ((DebugPrintLevel, "%a: BootOrder[%llu] = 0x%04x = %s %a\n", __FUNCTION__, BootOrderIndex, BootOrder[BootOrderIndex], Option.Description, AttributeString));

    EfiBootManagerFreeLoadOption (&Option);
  }

CleanupAndReturn:
  FREE_NON_NULL (LocalBootOrder);
}

VOID
PrintCurrentBootOrder (
  IN CONST UINTN  DebugPrintLevel
  )
{
  EFI_STATUS  Status;
  UINTN       BootNextSize;
  UINT16      *BootNext;

  // Print BootNext if it exists
  BootNext = NULL;
  Status   = GetEfiGlobalVariable2 (EFI_BOOT_NEXT_VARIABLE_NAME, (VOID **)&BootNext, &BootNextSize);
  if ((Status == EFI_SUCCESS) && (BootNextSize == sizeof (UINT16)) && (BootNext != NULL)) {
    PrintBootOrder (DebugPrintLevel, L"BootNext:", BootNext, BootNextSize);
  }

  if (BootNext != NULL) {
    FreePool (BootNext);
  }

  // Print the BootOrder
  PrintBootOrder (DebugPrintLevel, L"Current BootOrder:", NULL, 0);
}

NVIDIA_BOOT_ORDER_PRIORITY *
EFIAPI
GetBootClassOfOption (
  IN EFI_BOOT_MANAGER_LOAD_OPTION  *Option,
  IN NVIDIA_BOOT_ORDER_PRIORITY    *Table,
  IN UINTN                         Count
  )
{
  UINTN                         BootPriorityIndex;
  EFI_DEVICE_PATH_PROTOCOL      *DevicePathNode;
  UINT8                         ExtraSpecifier;
  EFI_PCI_IO_PROTOCOL           *PciIo;
  UINTN                         Segment;
  UINTN                         Bus;
  UINTN                         Device;
  UINTN                         Function;
  NVIDIA_BOOT_ORDER_PRIORITY    *Result;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;

  Result   = NULL;
  PciIo    = NULL;
  Segment  = MAX_UINTN;
  Bus      = MAX_UINTN;
  Device   = MAX_UINTN;
  Function = MAX_UINTN;
  if (StrCmp (Option->Description, L"UEFI Shell") == 0) {
    for (BootPriorityIndex = 0; BootPriorityIndex < Count; BootPriorityIndex++) {
      if (AsciiStrCmp (Table[BootPriorityIndex].OrderName, "shell") == 0) {
        Result = &Table[BootPriorityIndex];
        goto ReturnResult;
      }
    }
  }

  ExtraSpecifier = MAX_UINT8;

 #ifdef EDKII_UNIT_TEST_FRAMEWORK_ENABLED
  TEGRA_PLATFORM_RESOURCE_INFO  _PlatformResourceInfo = { .BootType = TegrablBootInvalid };
  PlatformResourceInfo = &_PlatformResourceInfo;
  (VOID)Hob;
 #else
  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get PlatformResourceInfo\n", __FUNCTION__));
    goto ReturnResult;
  }

 #endif

  if (PlatformResourceInfo->BootType == TegrablBootRcm) {
    for (BootPriorityIndex = 0; BootPriorityIndex < Count; BootPriorityIndex++) {
      if (Table[BootPriorityIndex].ExtraSpecifier == NVIDIA_BOOT_TYPE_BOOTIMG) {
        Result = &Table[BootPriorityIndex];
        goto ReturnResult;
      }
    }
  } else {
    DevicePathNode = Option->FilePath;
    while (!IsDevicePathEndType (DevicePathNode)) {
      if ((DevicePathType (DevicePathNode) == MESSAGING_DEVICE_PATH) &&
          (DevicePathSubType (DevicePathNode) == MSG_URI_DP))
      {
        ExtraSpecifier = NVIDIA_BOOT_TYPE_HTTP;
        break;
      } else if ((DevicePathType (DevicePathNode) == MESSAGING_DEVICE_PATH) &&
                 (DevicePathSubType (DevicePathNode) == MSG_USB_DP) &&
                 (StrStr (Option->Description, L"Virtual")))
      {
        ExtraSpecifier = NVIDIA_BOOT_TYPE_VIRTUAL;
        break;
      }

      DevicePathNode = NextDevicePathNode (DevicePathNode);
    }
  }

  PciIo = GetBootOptPciIoProtocol (Option);
  if (PciIo != NULL) {
    PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  }

  // Result will be the deepest device path node that matches
  // the earliest type in the table.
  DevicePathNode = Option->FilePath;
  while (!IsDevicePathEndType (DevicePathNode)) {
    for (BootPriorityIndex = 0; BootPriorityIndex < Count; BootPriorityIndex++) {
      if ((DevicePathType (DevicePathNode) == Table[BootPriorityIndex].Type) &&
          (DevicePathSubType (DevicePathNode) == Table[BootPriorityIndex].SubType) &&
          (ExtraSpecifier == Table[BootPriorityIndex].ExtraSpecifier))
      {
        //
        // Check Boot device PCI location. MAX_UINTN in Table queue means "don't check".
        //
        if (((Segment == Table[BootPriorityIndex].SegmentNum) || (MAX_UINTN == Table[BootPriorityIndex].SegmentNum)) &&
            ((Bus == Table[BootPriorityIndex].BusNum) || (MAX_UINTN == Table[BootPriorityIndex].BusNum)) &&
            ((Device == Table[BootPriorityIndex].DevNum) || (MAX_UINTN == Table[BootPriorityIndex].DevNum)) &&
            (((Function == Table[BootPriorityIndex].FuncNum) || (MAX_UINTN == Table[BootPriorityIndex].FuncNum))))
        {
          Result = &Table[BootPriorityIndex];
          break;
        }
      }
    }

    DevicePathNode = NextDevicePathNode (DevicePathNode);
  }

ReturnResult:
  if (Result != NULL) {
    DEBUG ((DEBUG_VERBOSE, "Option %d (%s) has class %a\n", Option->OptionNumber, Option->Description, Result->OrderName));
  } else {
    DEBUG ((DEBUG_VERBOSE, "Option %d (%s) has unknown class\n", Option->OptionNumber, Option->Description));
  }

  return Result;
}

/**
  Gets the class name of the specified device

  Gets the name of the type of device that is passed in

  @param[in]  FilePath            DevicePath of the device.
  @param[out] DeviceClass         Pointer to a string that describes the device type.

  @retval EFI_SUCCESS             Class name returned.
  @retval EFI_NOT_FOUND           Device type not found.
  @retval EFI_INVALID_PARAMETER   FilePath is NULL.
  @retval EFI_INVALID_PARAMETER   DeviceClass is NULL.
**/
EFI_STATUS
EFIAPI
GetBootDeviceClass (
  IN EFI_DEVICE_PATH_PROTOCOL  *FilePath,
  OUT CONST CHAR8              **DeviceClass
  )
{
  EFI_BOOT_MANAGER_LOAD_OPTION  Option;
  NVIDIA_BOOT_ORDER_PRIORITY    *BootOrder;

  if ((FilePath == NULL) || (DeviceClass == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Option.Description  = L"Test";
  Option.OptionalData = NULL;
  Option.FilePath     = FilePath;
  Option.OptionNumber = 0;

  BootOrder = GetBootClassOfOption (&Option, mBootPriorityTemplate, ARRAY_SIZE (mBootPriorityTemplate));
  if (BootOrder == NULL) {
    return EFI_NOT_FOUND;
  } else {
    *DeviceClass = BootOrder->OrderName;
    return EFI_SUCCESS;
  }
}

EFI_STATUS
EFIAPI
GetBootClassOfOptionNum (
  IN UINT16                          OptionNum,
  IN OUT NVIDIA_BOOT_ORDER_PRIORITY  **Class,
  IN NVIDIA_BOOT_ORDER_PRIORITY      *Table,
  IN UINTN                           Count
  )
{
  EFI_STATUS                    Status;
  CHAR16                        OptionName[sizeof ("Boot####")];
  EFI_BOOT_MANAGER_LOAD_OPTION  Option;

  if (Class == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  UnicodeSPrint (OptionName, sizeof (OptionName), L"Boot%04x", OptionNum);
  Status = EfiBootManagerVariableToLoadOption (OptionName, &Option);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got error (%r) getting BootOption for %s\n", __FUNCTION__, Status, OptionName));
    return Status;
  }

  *Class = GetBootClassOfOption (&Option, Table, Count);
  EfiBootManagerFreeLoadOption (&Option);
  if (*Class == NULL) {
    return EFI_NOT_FOUND;
  } else {
    return EFI_SUCCESS;
  }
}

STATIC
EFI_STATUS
EFIAPI
AppendBootOrderPriority (
  CHAR8                       *ClassName,
  UINTN                       ClassNameLen,
  NVIDIA_BOOT_ORDER_PRIORITY  **BootPriority
  )
{
  EFI_STATUS                  Status;
  UINTN                       BootPriorityIndex;
  UINTN                       BootPriorityMatchLen;
  NVIDIA_BOOT_ORDER_PRIORITY  *NewBuffer;

  NewBuffer     = NULL;
  *BootPriority = NULL;
  Status        = EFI_NOT_FOUND;

  for (BootPriorityIndex = 0; BootPriorityIndex < ARRAY_SIZE (mBootPriorityTemplate); BootPriorityIndex++) {
    BootPriorityMatchLen = AsciiStrLen (mBootPriorityTemplate[BootPriorityIndex].OrderName);
    if ((BootPriorityMatchLen == ClassNameLen) &&
        (CompareMem (ClassName, mBootPriorityTemplate[BootPriorityIndex].OrderName, ClassNameLen) == 0))
    {
      NewBuffer = (NVIDIA_BOOT_ORDER_PRIORITY *)ReallocatePool (
                                                  mBootPriorityCount * sizeof (NVIDIA_BOOT_ORDER_PRIORITY),
                                                  (mBootPriorityCount + 1) * sizeof (NVIDIA_BOOT_ORDER_PRIORITY),
                                                  (VOID *)mBootPriorityTable
                                                  );

      if (NewBuffer == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to allocate new buffer\r\n", __FUNCTION__));
        return EFI_OUT_OF_RESOURCES;
      }

      mBootPriorityTable = NewBuffer;
      CopyMem (
        &mBootPriorityTable[mBootPriorityCount],
        &mBootPriorityTemplate[BootPriorityIndex],
        sizeof (mBootPriorityTemplate[BootPriorityIndex])
        );
      *BootPriority = &mBootPriorityTable[mBootPriorityCount++];

      Status = EFI_SUCCESS;
      break;
    }
  }

  return Status;
}

NVIDIA_BOOT_ORDER_PRIORITY *
EFIAPI
GetBootClassOfName (
  CHAR8                          *ClassName,
  UINTN                          ClassNameLen,
  IN NVIDIA_BOOT_ORDER_PRIORITY  *Table,
  IN UINTN                       Count
  )
{
  EFI_STATUS                  Status;
  UINTN                       BootPriorityIndex;
  UINTN                       BootPriorityMatchLen;
  NVIDIA_BOOT_ORDER_PRIORITY  *ClassBootPriority;

  for (BootPriorityIndex = 0; BootPriorityIndex < Count; BootPriorityIndex++) {
    BootPriorityMatchLen = AsciiStrLen (Table[BootPriorityIndex].OrderName);
    if ((BootPriorityMatchLen == ClassNameLen) &&
        (CompareMem (ClassName, Table[BootPriorityIndex].OrderName, ClassNameLen) == 0))
    {
      return &Table[BootPriorityIndex];
    }
  }

  ClassBootPriority = NULL;
  Status            = AppendBootOrderPriority (ClassName, ClassNameLen, &ClassBootPriority);
  if (Status == EFI_NOT_FOUND) {
    //
    // The boot device type is not in mBootPriorityTemplate.
    //
    DEBUG ((DEBUG_ERROR, "Unable to determine class of boot device type \"%a\"\r\n", ClassName));
  } else if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Fail to append boot class %a, Status= %r\r\n", ClassName, Status));
  } else {
    return ClassBootPriority;
  }

  return NULL;
}

// Note: This function only works when called after ParseDefaultBootPriority() has created mBootPriorityTable
STATIC
INT32
EFIAPI
GetDevicePriority (
  IN UINT16  BootOption
  )
{
  EFI_STATUS                    Status;
  CHAR16                        OptionName[sizeof ("Boot####")];
  EFI_BOOT_MANAGER_LOAD_OPTION  Option;
  NVIDIA_BOOT_ORDER_PRIORITY    *BootPriorityClass;
  INT32                         DevicePriority;

  UnicodeSPrint (OptionName, sizeof (OptionName), L"Boot%04x", BootOption);
  Status = EfiBootManagerVariableToLoadOption (OptionName, &Option);
  if (EFI_ERROR (Status)) {
    return MAX_INT32;
  }

  BootPriorityClass = GetBootClassOfOption (&Option, mBootPriorityTable, mBootPriorityCount);

  if (BootPriorityClass == NULL) {
    DevicePriority = MAX_INT32;
  } else {
    DevicePriority = BootPriorityClass->PriorityOrder;
  }

  DEBUG ((DEBUG_VERBOSE, "Found %s priority to be %d\r\n", ConvertDevicePathToText (Option.FilePath, TRUE, FALSE), DevicePriority));
  EfiBootManagerFreeLoadOption (&Option);
  return DevicePriority;
}

STATIC
INTN
EFIAPI
BootOrderSortCompare (
  IN CONST VOID  *Buffer1,
  IN CONST VOID  *Buffer2
  )
{
  INT32  Priority1;
  INT32  Priority2;

  Priority1 = GetDevicePriority (*(UINT16 *)Buffer1);
  Priority2 = GetDevicePriority (*(UINT16 *)Buffer2);

  return Priority1 - Priority2;
}

STATIC
VOID
EFIAPI
ParseDefaultBootPriority (
  VOID
  )
{
  EFI_STATUS                  Status;
  INT32                       Priority;
  CHAR8                       *DefaultBootOrder;
  UINTN                       DefaultBootOrderSize;
  CHAR8                       *CurrentBootPriorityStr;
  CHAR8                       *CurrentBootPriorityEnd;
  UINTN                       BootPriorityClassLen;
  NVIDIA_BOOT_ORDER_PRIORITY  *ClassBootPriority;
  CHAR8                       *BootPriorityClass;
  CHAR8                       *BootPriorityClassStart;
  CHAR8                       *BootPriorityClassEnd;
  CHAR8                       *BootPrioritySbdfStart;
  CHAR8                       *BootPrioritySbdfEnd;
  UINTN                       BootPrioritySbdfLen;
  UINTN                       BootClassIndex;

  Priority = 0;
  // Process the priority order
  Status = GetVariable2 (
             L"DefaultBootPriority",
             &gNVIDIATokenSpaceGuid,
             (VOID **)&DefaultBootOrder,
             &DefaultBootOrderSize
             );
  if (EFI_ERROR (Status)) {
    DefaultBootOrder     = DEFAULT_BOOT_ORDER_STRING;
    DefaultBootOrderSize = sizeof (DEFAULT_BOOT_ORDER_STRING);
  }

  CurrentBootPriorityStr = DefaultBootOrder;
  while (CurrentBootPriorityStr < (DefaultBootOrder + DefaultBootOrderSize)) {
    BootPriorityClassStart = CurrentBootPriorityStr;
    CurrentBootPriorityEnd = CurrentBootPriorityStr;
    while (CurrentBootPriorityEnd < (DefaultBootOrder + DefaultBootOrderSize)) {
      if ((*CurrentBootPriorityEnd == BOOT_ORDER_CLASS_SEPARATOR) ||
          (*CurrentBootPriorityEnd == BOOT_ORDER_SBDF_STARTER) ||
          (*CurrentBootPriorityEnd == BOOT_ORDER_TERMINATOR))
      {
        BootPriorityClassEnd  = CurrentBootPriorityEnd;
        BootPrioritySbdfStart = CurrentBootPriorityEnd;
        while (CurrentBootPriorityEnd < (DefaultBootOrder + DefaultBootOrderSize) &&
               !((*CurrentBootPriorityEnd == BOOT_ORDER_CLASS_SEPARATOR) ||
                 (*CurrentBootPriorityEnd == BOOT_ORDER_TERMINATOR)))
        {
          CurrentBootPriorityEnd++;
        }

        BootPrioritySbdfEnd = CurrentBootPriorityEnd;
        break;
      }

      CurrentBootPriorityEnd++;
    }

    BootPriorityClassLen = BootPriorityClassEnd - BootPriorityClassStart;
    BootPrioritySbdfLen  = BootPrioritySbdfEnd  - BootPrioritySbdfStart;

    //
    // Build default Boot Priority table by DefaultBootOrder.
    //
    Status = AppendBootOrderPriority (CurrentBootPriorityStr, BootPriorityClassLen, &ClassBootPriority);
    if (EFI_ERROR (Status)) {
      DEBUG_CODE_BEGIN ();
      BootPriorityClass = (CHAR8 *)AllocateZeroPool (BootPriorityClassLen + 1);

      if (BootPriorityClass != NULL) {
        CopyMem (BootPriorityClass, BootPriorityClassStart, BootPriorityClassLen);
        DEBUG ((DEBUG_ERROR, "Fail to append boot class %a, Status= %r\r\n", BootPriorityClass, Status));
        FreePool (BootPriorityClass);
        BootPriorityClass = NULL;
      }

      DEBUG_CODE_END ();
    } else {
      if (ClassBootPriority != NULL) {
        DEBUG ((DEBUG_INFO, "Setting %a priority to %d\r\n", ClassBootPriority->OrderName, Priority));
        ClassBootPriority->PriorityOrder = Priority;

        if (BootPrioritySbdfLen != 0) {
          GetBootOrderSbdf (
            BootPrioritySbdfStart,
            BootPrioritySbdfLen,
            &ClassBootPriority->SegmentNum,
            &ClassBootPriority->BusNum,
            &ClassBootPriority->DevNum,
            &ClassBootPriority->FuncNum
            );
        }

        Priority++;
      } else {
        *CurrentBootPriorityEnd = BOOT_ORDER_TERMINATOR;
        DEBUG_CODE_BEGIN ();
        BootPriorityClass = (CHAR8 *)AllocateZeroPool (BootPriorityClassLen + 1);

        if (BootPriorityClass != NULL) {
          CopyMem (BootPriorityClass, BootPriorityClassStart, BootPriorityClassLen);
          DEBUG ((DEBUG_ERROR, "Ignoring unknown boot class %a\r\n", BootPriorityClass));
          FreePool (BootPriorityClass);
          BootPriorityClass = NULL;
        }

        DEBUG_CODE_END ();
      }
    }

    CurrentBootPriorityStr += BootPriorityClassLen + BootPrioritySbdfLen + 1;
  }

  // Add all the template classes to the table with default priority, so that they won't accidentally
  // be treated as the wrong class in lookups if they were not in the default boot order
  for (BootClassIndex = 0; BootClassIndex < ARRAY_SIZE (mBootPriorityTemplate); BootClassIndex++) {
    BootPriorityClassLen = AsciiStrLen (mBootPriorityTemplate[BootClassIndex].OrderName);

    Status = AppendBootOrderPriority (mBootPriorityTemplate[BootClassIndex].OrderName, BootPriorityClassLen, &ClassBootPriority);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Fail to append boot class %a, Status= %r\r\n", mBootPriorityTemplate[BootClassIndex].OrderName, Status));
    }
  }

  return;
}

VOID
EFIAPI
SetBootOrder (
  VOID
  )
{
  EFI_STATUS  Status;
  BOOLEAN     VariableData;
  UINTN       VariableSize;
  UINT32      VariableAttributes;

  VariableData = FALSE;
  VariableSize = sizeof (BOOLEAN);
  Status       = gRT->GetVariable (
                        L"PlatformBootOrderSet",
                        &gNVIDIATokenSpaceGuid,
                        &VariableAttributes,
                        &VariableSize,
                        (VOID *)&VariableData
                        );
  if (!EFI_ERROR (Status) && (VariableSize == sizeof (BOOLEAN))) {
    if (VariableData == TRUE) {
      return;
    }
  }

  ParseDefaultBootPriority ();

  EfiBootManagerSortLoadOptionVariable (LoadOptionTypeBoot, BootOrderSortCompare);

  VariableData = TRUE;
  gRT->SetVariable (
         L"PlatformBootOrderSet",
         &gNVIDIATokenSpaceGuid,
         EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
         sizeof (BOOLEAN),
         &VariableData
         );

  return;
}
