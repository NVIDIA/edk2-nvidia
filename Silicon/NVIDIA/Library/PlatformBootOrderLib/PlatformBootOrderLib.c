/** @file
*
*  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2017, Linaro Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IpmiBaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/SortLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/FwVariableLib.h>
#include <Library/PlatformResourceLib.h>
#include <IndustryStandard/Ipmi.h>
#include <Guid/GlobalVariable.h>
#include <Protocol/PciIo.h>
#include "InternalPlatformBootOrderLib.h"

// Moves the element at INDEX to the start of the ARRAY
#define MOVE_INDEX_TO_START(ARRAY, INDEX)                            \
do {                                                                 \
  __typeof__((ARRAY)[0]) Value;                                      \
  if ((INDEX) > 0) {                                                 \
    Value = (ARRAY)[(INDEX)];                                        \
    CopyMem (&((ARRAY)[1]), &((ARRAY)[0]), (INDEX)*sizeof(Value));   \
    (ARRAY)[0] = Value;                                              \
  }                                                                  \
} while (FALSE);

STATIC NVIDIA_BOOT_ORDER_PRIORITY  mBootPriorityTemplate[] = {
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

STATIC  NVIDIA_BOOT_ORDER_PRIORITY      *mBootPriorityTable   = NULL;
STATIC  UINTN                           mBootPriorityCount    = 0;
STATIC  IPMI_GET_BOOT_OPTIONS_RESPONSE  *mBootOptionsResponse = NULL;
STATIC  IPMI_SET_BOOT_OPTIONS_REQUEST   *mBootOptionsRequest  = NULL;

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

STATIC
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

    DEBUG ((DebugPrintLevel, "%a: BootOrder[%llu] = 0x%04x = %s\n", __FUNCTION__, BootOrderIndex, BootOrder[BootOrderIndex], Option.Description));
    EfiBootManagerFreeLoadOption (&Option);
  }

CleanupAndReturn:
  FREE_NON_NULL (LocalBootOrder);
}

NVIDIA_BOOT_ORDER_PRIORITY *
EFIAPI
GetBootClassOfOption (
  IN EFI_BOOT_MANAGER_LOAD_OPTION  *Option,
  IN NVIDIA_BOOT_ORDER_PRIORITY    *Table,
  IN UINTN                         Count
  )
{
  UINTN                       OptionalDataSize;
  UINTN                       BootPriorityIndex;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePathNode;
  UINT8                       ExtraSpecifier;
  EFI_PCI_IO_PROTOCOL         *PciIo;
  UINTN                       Segment;
  UINTN                       Bus;
  UINTN                       Device;
  UINTN                       Function;
  NVIDIA_BOOT_ORDER_PRIORITY  *Result;

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

  OptionalDataSize = 0;
  if (Option->OptionalData != NULL) {
    OptionalDataSize = StrSize ((CONST CHAR16 *)Option->OptionalData);
  }

  if ((Option->OptionalData != NULL) &&
      (Option->OptionalDataSize == OptionalDataSize + sizeof (EFI_GUID)) &&
      CompareGuid (
        (EFI_GUID *)((UINT8 *)Option->OptionalData + OptionalDataSize),
        &gNVIDIABmBootOptionGuid
        ))
  {
    for (BootPriorityIndex = 0; BootPriorityIndex < Count; BootPriorityIndex++) {
      if (Table[BootPriorityIndex].ExtraSpecifier == NVIDIA_BOOT_TYPE_BOOTIMG) {
        Result = &Table[BootPriorityIndex];
        goto ReturnResult;
      }
    }
  }

  ExtraSpecifier = MAX_UINT8;
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

  PciIo = GetBootOptPciIoProtocol (Option);
  if (PciIo != NULL) {
    PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  }

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

STATIC
EFI_STATUS
GetIPMIBootOrderParameter (
  IN UINT8                            ParameterSelector,
  OUT IPMI_GET_BOOT_OPTIONS_RESPONSE  *BootOptionsResponse
  )
{
  EFI_STATUS                     Status;
  IPMI_GET_BOOT_OPTIONS_REQUEST  BootOptionsRequest;
  UINT32                         ResponseSize;

  BootOptionsRequest.ParameterSelector.Bits.ParameterSelector = ParameterSelector;
  BootOptionsRequest.SetSelector                              = 0;
  BootOptionsRequest.BlockSelector                            = 0;

  ResponseSize = sizeof (IPMI_GET_BOOT_OPTIONS_RESPONSE);
  if (ParameterSelector == IPMI_BOOT_OPTIONS_PARAMETER_BOOT_INFO_ACK) {
    ResponseSize += sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4);
  } else if (ParameterSelector == IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS) {
    ResponseSize += sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5);
  }

  Status = IpmiSubmitCommand (
             IPMI_NETFN_CHASSIS,
             IPMI_CHASSIS_GET_SYSTEM_BOOT_OPTIONS,
             (VOID *)&BootOptionsRequest,
             sizeof (BootOptionsRequest),
             (VOID *)BootOptionsResponse,
             &ResponseSize
             );

  if (EFI_ERROR (Status) ||
      (BootOptionsResponse->CompletionCode != IPMI_COMP_CODE_NORMAL) ||
      (BootOptionsResponse->ParameterValid.Bits.ParameterValid == IPMI_GET_BOOT_OPTIONS_PARAMETER_INVALID) ||
      (BootOptionsResponse->ParameterValid.Bits.ParameterSelector != ParameterSelector) ||
      (BootOptionsResponse->ParameterVersion.Bits.ParameterVersion != IPMI_PARAMETER_VERSION))
  {
    DEBUG ((DEBUG_ERROR, "Failed to get BMC Boot Options Parameter %u (IPMI CompCode = 0x%x)\r\n", ParameterSelector, BootOptionsResponse->CompletionCode));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SetIPMIBootOrderParameter (
  IN UINT8                          ParameterSelector,
  IN IPMI_SET_BOOT_OPTIONS_REQUEST  *BootOptionsRequest
  )
{
  EFI_STATUS                      Status;
  IPMI_SET_BOOT_OPTIONS_RESPONSE  BootOptionsResponse;
  UINT32                          RequestSize;
  UINT32                          ResponseSize;

  BootOptionsRequest->ParameterValid.Bits.MarkParameterInvalid = 0;
  BootOptionsRequest->ParameterValid.Bits.ParameterSelector    = ParameterSelector;

  RequestSize = sizeof (IPMI_SET_BOOT_OPTIONS_REQUEST);
  if (ParameterSelector == IPMI_BOOT_OPTIONS_PARAMETER_BOOT_INFO_ACK) {
    RequestSize += sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4);
  } else if (ParameterSelector == IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS) {
    RequestSize += sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5);
  }

  ResponseSize = sizeof (IPMI_SET_BOOT_OPTIONS_RESPONSE);

  Status = IpmiSubmitCommand (
             IPMI_NETFN_CHASSIS,
             IPMI_CHASSIS_SET_SYSTEM_BOOT_OPTIONS,
             (VOID *)BootOptionsRequest,
             RequestSize,
             (VOID *)&BootOptionsResponse,
             &ResponseSize
             );

  if (EFI_ERROR (Status) ||
      (BootOptionsResponse.CompletionCode != IPMI_COMP_CODE_NORMAL))
  {
    DEBUG ((DEBUG_ERROR, "Failed to set BMC Boot Options Parameter %u (IPMI CompCode = 0x%x)\r\n", ParameterSelector, BootOptionsResponse.CompletionCode));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
INTN
EFIAPI
Uint16SortCompare (
  IN CONST VOID  *Buffer1,
  IN CONST VOID  *Buffer2
  )
{
  return *(UINT16 *)Buffer1 - *(UINT16 *)Buffer2;
}

VOID
EFIAPI
CheckIPMIForBootOrderUpdates (
  VOID
  )
{
  EFI_STATUS                    Status;
  IPMI_BOOT_OPTIONS_PARAMETERS  *BootOptionsResponseParameters;
  IPMI_BOOT_OPTIONS_PARAMETERS  *BootOptionsRequestParameters;

  mBootOptionsResponse = AllocateZeroPool (sizeof (IPMI_GET_BOOT_OPTIONS_RESPONSE) + sizeof (IPMI_BOOT_OPTIONS_PARAMETERS));
  if (mBootOptionsResponse == NULL) {
    DEBUG ((DEBUG_ERROR, "Unable to allocate memory for handling IPMI BootOrder Responses\n"));
    goto CleanupAndReturn;
  }

  mBootOptionsRequest = AllocateZeroPool (sizeof (IPMI_GET_BOOT_OPTIONS_REQUEST) + sizeof (IPMI_BOOT_OPTIONS_PARAMETERS));
  if (mBootOptionsRequest == NULL) {
    DEBUG ((DEBUG_ERROR, "Unable to allocate memory for handling IPMI BootOrder Requests\n"));
    goto CleanupAndReturn;
  }

  Status = GetIPMIBootOrderParameter (IPMI_BOOT_OPTIONS_PARAMETER_BOOT_INFO_ACK, mBootOptionsResponse);
  if (EFI_ERROR (Status)) {
    if (Status != EFI_UNSUPPORTED) {
      DEBUG ((DEBUG_ERROR, "Error checking for IPMI BOOT_INFO_ACK: %r\n", Status));
    }

    goto CleanupAndReturn;
  }

  BootOptionsResponseParameters = (IPMI_BOOT_OPTIONS_PARAMETERS *)&mBootOptionsResponse->ParameterData[0];
  if (BootOptionsResponseParameters->Parm4.BootInitiatorAcknowledgeData & BOOT_OPTION_HANDLED_BY_BIOS) {
    Status = GetIPMIBootOrderParameter (IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS, mBootOptionsResponse);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Error checking if IPMI boot options were already processed: %r\n", Status));
      goto CleanupAndReturn;
    }

    if (!BootOptionsResponseParameters->Parm5.Data1.Bits.BootFlagValid) {
      goto Done;
    }

    if (BootOptionsResponseParameters->Parm5.Data2.Bits.CmosClear) {
      DEBUG ((DEBUG_ERROR, "IPMI requested a CMOS clear\n"));

      Status = FwVariableDeleteAll ();
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Error clearing CMOS: %r\n", Status));
        goto CleanupAndReturn;
      }

      // Clear CmosClear bit but leave the rest to be processed after reset
      BootOptionsRequestParameters = (IPMI_BOOT_OPTIONS_PARAMETERS *)&mBootOptionsRequest->ParameterData[0];
      CopyMem (BootOptionsRequestParameters, BootOptionsResponseParameters, sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5));
      BootOptionsRequestParameters->Parm5.Data2.Bits.CmosClear = 0;

      Status = SetIPMIBootOrderParameter (IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS, mBootOptionsRequest);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "Error clearing IPMI CmosClear bit: %r\n", Status));
      }

      // Mark existing boot chain as good.
      ValidateActiveBootChain ();

      // Reset
      gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
      ASSERT (FALSE);
    }

    // Don't free up allocations; they will be used and freed by ProcessIPMIBootOrderUpdates
    goto Done;
  }

CleanupAndReturn:
  FREE_NON_NULL (mBootOptionsRequest);
  FREE_NON_NULL (mBootOptionsResponse);

Done:
  return;
}

// When IPMI requests a temporary BootOrder change, we save the BootOrder
// and then move one element (or class) to the beginning. This function will
// restore the original BootOrder unless additional modifications have been made.
VOID
EFIAPI
RestoreBootOrder (
  EFI_EVENT  Event,
  VOID       *Context
  )
{
  UINT16                      *SavedBootOrder;
  UINT16                      *SavedBootOrderCopy;
  UINTN                       SavedBootOrderSize;
  UINTN                       SavedBootOrderLength;
  UINT16                      *BootOrder;
  UINTN                       BootOrderSize;
  UINTN                       BootOrderLength;
  UINT16                      *BootCurrent;
  UINTN                       BootCurrentSize;
  UINTN                       ReorderedIndex;
  UINT16                      ReorderedBootNum;
  EFI_STATUS                  Status;
  NVIDIA_BOOT_ORDER_PRIORITY  *BootClass;
  NVIDIA_BOOT_ORDER_PRIORITY  *SavedBootClass;
  NVIDIA_BOOT_ORDER_PRIORITY  *VirtualBootClass;
  INTN                        SavedBootOrderIndex;
  UINTN                       VirtualCount;
  UINTN                       MovedItemCount;
  UINT8                       *SavedBootOrderFlags;
  UINTN                       SavedBootOrderFlagsSize;
  BOOLEAN                     VirtualFlag;
  BOOLEAN                     AllInstancesFlag;

  SavedBootOrder      = NULL;
  BootOrder           = NULL;
  SavedBootOrderCopy  = NULL;
  SavedBootOrderFlags = NULL;

  // Gather SavedBootOrder
  Status = GetVariable2 (SAVED_BOOT_ORDER_VARIABLE_NAME, &gNVIDIATokenSpaceGuid, (VOID **)&SavedBootOrder, &SavedBootOrderSize);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "%a: No SavedBootOrder found to be restored\n", __FUNCTION__));
    goto CleanupAndReturn;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to determine SavedBootOrder: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  if ((SavedBootOrder == NULL) || (SavedBootOrderSize == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: SavedBootOrder is empty. Not restoring boot order\n", __FUNCTION__));
    goto DeleteSaveAndCleanup;
  }

  SavedBootOrderLength = SavedBootOrderSize/sizeof (SavedBootOrder[0]);

  // Gather BootOrder
  Status = GetEfiGlobalVariable2 (EFI_BOOT_ORDER_VARIABLE_NAME, (VOID **)&BootOrder, &BootOrderSize);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "%a: No BootOrder found. Not restoring boot order\n", __FUNCTION__));
    goto DeleteSaveAndCleanup;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to determine BootOrder: %r\n", __FUNCTION__, Status));
    goto DeleteSaveAndCleanup;
  }

  if ((BootOrder == NULL) || (BootOrderSize == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: BootOrder is empty. Not restoring boot order\n", __FUNCTION__));
    goto DeleteSaveAndCleanup;
  }

  BootOrderLength = BootOrderSize/sizeof (BootOrder[0]);

  // Make sure BootOrder only has one device moved to the front vs. SavedBootOrder,
  // or has all instances moved to the start
  if (SavedBootOrderLength != BootOrderLength) {
    DEBUG ((DEBUG_WARN, "%a: BootOrder (len=%u) and SavedBootOrder (len=%u) differ in size. Not restoring boot order\n", __FUNCTION__, BootOrderLength, SavedBootOrderLength));
    goto DeleteSaveAndCleanup;
  }

  ReorderedBootNum = BootOrder[0];
  for (ReorderedIndex = 0; ReorderedIndex < BootOrderLength; ReorderedIndex++) {
    if (SavedBootOrder[ReorderedIndex] == ReorderedBootNum) {
      break;
    }
  }

  if (ReorderedIndex >= BootOrderLength) {
    DEBUG ((DEBUG_WARN, "%a: First BootOrder device is not in SavedBootOrder. Not restoring boot order\n", __FUNCTION__));
    goto DeleteSaveAndCleanup;
  }

  // Parse the flags, if present
  VirtualFlag      = FALSE;
  AllInstancesFlag = FALSE;
  Status           = GetVariable2 (SAVED_BOOT_ORDER_FLAGS_VARIABLE_NAME, &gNVIDIATokenSpaceGuid, (VOID **)&SavedBootOrderFlags, &SavedBootOrderFlagsSize);
  if (!EFI_ERROR (Status) && (SavedBootOrderFlags != NULL) && (SavedBootOrderFlagsSize == sizeof (UINT8))) {
    if (*SavedBootOrderFlags & SAVED_BOOT_ORDER_VIRTUAL_FLAG) {
      VirtualFlag = TRUE;
    }

    if (*SavedBootOrderFlags & SAVED_BOOT_ORDER_ALL_INSTANCES_FLAG) {
      AllInstancesFlag = TRUE;
    }
  }

  SavedBootOrderCopy = AllocateCopyPool (SavedBootOrderSize, SavedBootOrder);
  if (!AllInstancesFlag) {
    // See if we can recreate BootOrder by simply moving one item from SavedBootOrder to the front
    MOVE_INDEX_TO_START (SavedBootOrderCopy, ReorderedIndex);
  } else {
    // See if we can recreate BootOrder by moving all items of the class from SavedBootOrder to the front
    Status = GetBootClassOfOptionNum (BootOrder[0], &BootClass, mBootPriorityTemplate, ARRAY_SIZE (mBootPriorityTemplate));
    if (Status == EFI_NOT_FOUND) {
      BootClass = NULL; // eg. the "ubuntu" option
    } else if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a: Error (%r) checking if we can restore boot order. Not restoring boot order\n", __FUNCTION__, Status));
      goto DeleteSaveAndCleanup;
    }

    // If VirtualFlag is set, then we moved "virtual" to start and "usb" to after virtual
    VirtualCount = 0;
    if (VirtualFlag && BootClass && (AsciiStrCmp (BootClass->OrderName, "virtual") == 0)) {
      VirtualBootClass = BootClass;
      BootClass        = GetBootClassOfName ("usb", AsciiStrLen ("usb"), mBootPriorityTemplate, ARRAY_SIZE (mBootPriorityTemplate));
    } else {
      VirtualBootClass = NULL;
    }

    MovedItemCount = 0;
    for (SavedBootOrderIndex = SavedBootOrderLength-1; SavedBootOrderIndex >= MovedItemCount; ) {
      Status = GetBootClassOfOptionNum (SavedBootOrderCopy[SavedBootOrderIndex], &SavedBootClass, mBootPriorityTemplate, ARRAY_SIZE (mBootPriorityTemplate));
      if (Status == EFI_NOT_FOUND) {
        SavedBootClass = NULL; // eg. the "ubuntu" option
      } else if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "%a: Error (%r) checking if we can restore boot order. Not restoring boot order\n", __FUNCTION__, Status));
        goto DeleteSaveAndCleanup;
      }

      if (SavedBootClass == BootClass) {
        MOVE_INDEX_TO_START (&SavedBootOrderCopy[VirtualCount], SavedBootOrderIndex);
        MovedItemCount++;
      } else if (SavedBootClass && (SavedBootClass == VirtualBootClass)) {
        MOVE_INDEX_TO_START (SavedBootOrderCopy, SavedBootOrderIndex);
        MovedItemCount++;
        VirtualCount++;
      } else {
        SavedBootOrderIndex--;
      }
    }
  }

  if (CompareMem (BootOrder, SavedBootOrderCopy, BootOrderSize) != 0) {
    DEBUG ((DEBUG_WARN, "%a: BootOrder and SavedBootOrder have more changes than expected. Not restoring boot order\n", __FUNCTION__));
    PrintBootOrder (DEBUG_WARN, L"CurrentBootOrder:", BootOrder, BootOrderSize);
    PrintBootOrder (DEBUG_WARN, L"SavedBootOrder:", SavedBootOrder, SavedBootOrderSize);
    goto DeleteSaveAndCleanup;
  }

  // At this point, we've confirmed that BootOrder equals SavedBootOrder except with one device or class moved to the beginning

  if (Event != NULL) {
    Status = GetEfiGlobalVariable2 (L"BootCurrent", (VOID **)&BootCurrent, &BootCurrentSize);
    if (EFI_ERROR (Status) || (BootCurrent == NULL) || (BootCurrentSize != sizeof (UINT16))) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to determine BootCurrent: %r\n", __FUNCTION__, Status));
      goto DeleteSaveAndCleanup;
    }

    if (BootCurrent[0] != ReorderedBootNum) {
      DEBUG ((DEBUG_WARN, "%a: Attempted to restore BootOrder when BootCurrent wasn't the temporary BootNum. Not restoring boot order\n", __FUNCTION__));
      goto DeleteSaveAndCleanup;
    }
  }

  // Restore BootOrder
  Status = gRT->SetVariable (
                  EFI_BOOT_ORDER_VARIABLE_NAME,
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  SavedBootOrderSize,
                  SavedBootOrder
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error restoring BootOrder: %r\n", __FUNCTION__, Status));
  } else {
    DEBUG ((DEBUG_INFO, "%a: BootOrder successfully restored\n", __FUNCTION__));
  }

DeleteSaveAndCleanup:
  Status = gRT->SetVariable (
                  SAVED_BOOT_ORDER_VARIABLE_NAME,
                  &gNVIDIATokenSpaceGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  0,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error deleting SavedBootOrder: %r\n", __FUNCTION__, Status));
  }

  Status = gRT->SetVariable (
                  SAVED_BOOT_ORDER_FLAGS_VARIABLE_NAME,
                  &gNVIDIATokenSpaceGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  0,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error deleting SavedBootOrderFlags: %r\n", __FUNCTION__, Status));
  }

CleanupAndReturn:
  FREE_NON_NULL (SavedBootOrder);
  FREE_NON_NULL (BootOrder);
  FREE_NON_NULL (SavedBootOrderCopy);
  FREE_NON_NULL (SavedBootOrderFlags);
  if (Event != NULL) {
    gBS->CloseEvent (Event);
  }
}

VOID
EFIAPI
ProcessIPMIBootOrderUpdates (
  VOID
  )
{
  EFI_STATUS                    Status;
  IPMI_BOOT_OPTIONS_PARAMETERS  *BootOptionsParameters;
  NVIDIA_BOOT_ORDER_PRIORITY    *RequestedBootClass;
  NVIDIA_BOOT_ORDER_PRIORITY    *OptionBootClass;
  CHAR8                         *RequestedClassName;
  UINT8                         RequestedInstance;
  UINTN                         BootOrderIndex;
  UINT16                        *BootOrder;
  UINTN                         BootOrderSize;
  UINTN                         BootOrderLength;
  UINT16                        *ClassInstanceList;
  UINTN                         ClassInstanceLength;
  UINT64                        OsIndications;
  UINTN                         OsIndicationsSize;
  BOOLEAN                       IPv6;
  EFI_EVENT                     ReadyToBootEvent;
  NVIDIA_BOOT_ORDER_PRIORITY    *VirtualBootClass;
  UINT16                        *VirtualInstanceList;
  UINTN                         VirtualInstanceLength;
  UINT16                        DesiredOptionNumber;
  BOOLEAN                       WillModifyBootOrder;
  UINT8                         BootOrderFlags;

  ClassInstanceList   = NULL;
  BootOrder           = NULL;
  VirtualBootClass    = NULL;
  VirtualInstanceList = NULL;

  if ((mBootOptionsResponse == NULL) || (mBootOptionsRequest == NULL)) {
    goto CleanupAndReturn;
  }

  if (PcdGet8 (PcdIpmiNetworkBootMode) == 1) {
    IPv6 = TRUE;
  } else {
    IPv6 = FALSE;
  }

  BootOptionsParameters = (IPMI_BOOT_OPTIONS_PARAMETERS *)&mBootOptionsResponse->ParameterData[0];

  if (!BootOptionsParameters->Parm5.Data1.Bits.BootFlagValid) {
    goto AcknowledgeAndCleanup;
  }

  // TODO update UEFI verbosity based on BootOptionsParameters->Parm5.Data3.Bits.BiosVerbosity?

  switch (BootOptionsParameters->Parm5.Data2.Bits.BootDeviceSelector) {
    case IPMI_BOOT_DEVICE_SELECTOR_NO_OVERRIDE:
      DEBUG ((DEBUG_ERROR, "IPMI requested no change to BootOrder\n"));
      goto AcknowledgeAndCleanup;
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_PXE:
      RequestedClassName = IPv6 ? "pxev6" : "pxev4";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_HARDDRIVE:
      RequestedClassName = "nvme";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_HARDDRIVE_SAFE_MODE:
      DEBUG ((DEBUG_WARN, "Ignoring unsupported boot device selector IPMI_BOOT_DEVICE_SELECTOR_HARDDRIVE_SAFE_MODE\n"));
      goto AcknowledgeAndCleanup;
    case IPMI_BOOT_DEVICE_SELECTOR_DIAGNOSTIC_PARTITION:
      DEBUG ((DEBUG_WARN, "Ignoring unsupported boot device selector IPMI_BOOT_DEVICE_SELECTOR_DIAGNOSTIC_PARTITION\n"));
      goto AcknowledgeAndCleanup;
    case IPMI_BOOT_DEVICE_SELECTOR_CD_DVD:
      RequestedClassName = "cdrom";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_BIOS_SETUP:
      OsIndications     = 0;
      OsIndicationsSize = sizeof (OsIndications);
      Status            = gRT->GetVariable (
                                 EFI_OS_INDICATIONS_VARIABLE_NAME,
                                 &gEfiGlobalVariableGuid,
                                 NULL,
                                 &OsIndicationsSize,
                                 &OsIndications
                                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Error getting OsIndications to request to boot to UEFI menu: %r\n", Status));
      }

      OsIndications |= EFI_OS_INDICATIONS_BOOT_TO_FW_UI;
      Status         = gRT->SetVariable (
                              EFI_OS_INDICATIONS_VARIABLE_NAME,
                              &gEfiGlobalVariableGuid,
                              EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                              sizeof (OsIndications),
                              &OsIndications
                              );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Error setting OsIndications to request to boot to UEFI menu: %r\n", Status));
      }

      DEBUG ((DEBUG_ERROR, "IPMI requested to boot to UEFI Menu\n"));

      goto AcknowledgeAndCleanup;
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_REMOTE_FLOPPY:
      RequestedClassName = "sata";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_REMOTE_CD_DVD:
      RequestedClassName = IPv6 ? "httpv6" : "httpv4";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_PRIMARY_REMOTE_MEDIA:
      DEBUG ((DEBUG_WARN, "Ignoring unsupported boot device selector IPMI_BOOT_DEVICE_SELECTOR_PRIMARY_REMOTE_MEDIA\n"));
      goto AcknowledgeAndCleanup;
    case IPMI_BOOT_DEVICE_SELECTOR_REMOTE_HARDDRIVE:
      RequestedClassName = "scsi";
      break;
    case IPMI_BOOT_DEVICE_SELECTOR_FLOPPY:
      RequestedClassName = "usb";
      // Redfish wants "usb" to treat "virtual" as higher priority USB devices than normal USB devices
      VirtualBootClass = GetBootClassOfName ("virtual", AsciiStrLen ("virtual"), mBootPriorityTemplate, ARRAY_SIZE (mBootPriorityTemplate));
      break;
    default:
      DEBUG ((DEBUG_WARN, "Ignoring unknown boot device selector %d\n", BootOptionsParameters->Parm5.Data2.Bits.BootDeviceSelector));
      goto AcknowledgeAndCleanup;
  }

  RequestedBootClass = GetBootClassOfName (RequestedClassName, AsciiStrLen (RequestedClassName), mBootPriorityTemplate, ARRAY_SIZE (mBootPriorityTemplate));
  if (RequestedBootClass == NULL) {
    DEBUG ((DEBUG_WARN, "Ignoring unsupported boot class \"%a\"\n", RequestedClassName));
    goto AcknowledgeAndCleanup;
  }

  RequestedInstance = BootOptionsParameters->Parm5.Data5.Bits.DeviceInstanceSelector;
  // Note: bit 4 selects between external(0) and internal(1) device instances, but we don't have that distinction, so ignore it
  RequestedInstance &= 0x0F;

  Status = GetEfiGlobalVariable2 (EFI_BOOT_ORDER_VARIABLE_NAME, (VOID **)&BootOrder, &BootOrderSize);
  if (EFI_ERROR (Status) || (BootOrder == NULL)) {
    DEBUG ((DEBUG_ERROR, "Unable to determine BootOrder (Status %r) - ignoring request to prioritize %a instance %u\n", Status, RequestedClassName, RequestedInstance));
    goto AcknowledgeAndCleanup;
  }

  BootOrderLength = BootOrderSize/sizeof (BootOrder[0]);

  ClassInstanceLength = 0;
  ClassInstanceList   = AllocatePool (BootOrderSize);
  if (ClassInstanceList == NULL) {
    DEBUG ((DEBUG_ERROR, "Unable to allocate memory to process BootOrder - ignoring request to prioritize %a instance %u\n", RequestedClassName, RequestedInstance));
    goto AcknowledgeAndCleanup;
  }

  VirtualInstanceLength = 0;
  if (VirtualBootClass != NULL) {
    VirtualInstanceList = AllocatePool (BootOrderSize);
    if (VirtualInstanceList == NULL) {
      DEBUG ((DEBUG_ERROR, "Unable to allocate memory to process virtual devices - ignoring request to prioritize %a instance %u\n", RequestedClassName, RequestedInstance));
      goto AcknowledgeAndCleanup;
    }
  }

  // Find the list of instances of RequestedClass in BootOrder
  for (BootOrderIndex = 0; BootOrderIndex < BootOrderLength; BootOrderIndex++) {
    Status = GetBootClassOfOptionNum (BootOrder[BootOrderIndex], &OptionBootClass, mBootPriorityTemplate, ARRAY_SIZE (mBootPriorityTemplate));
    if (Status == EFI_NOT_FOUND) {
      OptionBootClass = NULL; // eg. the "ubuntu" option
    } else if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Error (%r) parsing BootOrder - ignoring request to prioritize %a instance %u\n", Status, RequestedClassName, RequestedInstance));
      goto AcknowledgeAndCleanup;
    }

    if (VirtualBootClass && (OptionBootClass == VirtualBootClass)) {
      VirtualInstanceList[VirtualInstanceLength] = BootOrder[BootOrderIndex];
      VirtualInstanceLength++;
    } else if (OptionBootClass == RequestedBootClass) {
      ClassInstanceList[ClassInstanceLength] = BootOrder[BootOrderIndex];
      ClassInstanceLength++;
    }
  }

  if ((ClassInstanceLength == 0) && (VirtualInstanceLength == 0)) {
    DEBUG ((DEBUG_ERROR, "Unable to find any instance of %a in BootOrder - Ignoring boot order change request from IPMI\n", RequestedClassName));
    goto AcknowledgeAndCleanup;
  }

  // Find the index of the N-th occurance of RequestedClass in BootOrder when sorted by number
  if (RequestedInstance == 0) {
    // We will move all instances to the start of boot order, going through the list backwards to preserve relative ordering
    // We want to end with Virtual classes at the front of the list, so start with real classes if available
    if (ClassInstanceLength > 0) {
      DesiredOptionNumber = ClassInstanceList[--ClassInstanceLength];
    } else {
      DesiredOptionNumber = VirtualInstanceList[--VirtualInstanceLength];
    }
  } else if (RequestedInstance-1 < VirtualInstanceLength) {
    PerformQuickSort (VirtualInstanceList, VirtualInstanceLength, sizeof (VirtualInstanceList[0]), Uint16SortCompare);
    DesiredOptionNumber = VirtualInstanceList[RequestedInstance-1];
  } else if ((RequestedInstance - 1 - VirtualInstanceLength) < ClassInstanceLength) {
    PerformQuickSort (ClassInstanceList, ClassInstanceLength, sizeof (ClassInstanceList[0]), Uint16SortCompare);
    DesiredOptionNumber = ClassInstanceList[RequestedInstance - 1 - VirtualInstanceLength];
  } else {
    DEBUG ((DEBUG_WARN, "Unable to find requested instance %u of %a - Using all instances instead\n", RequestedInstance, RequestedClassName));
    RequestedInstance = 0;
    if (ClassInstanceLength > 0) {
      DesiredOptionNumber = ClassInstanceList[--ClassInstanceLength];
    } else {
      DesiredOptionNumber = VirtualInstanceList[--VirtualInstanceLength];
    }
  }

  for (BootOrderIndex = 0; BootOrderIndex < BootOrderLength; BootOrderIndex++) {
    if (BootOrder[BootOrderIndex] == DesiredOptionNumber) {
      break;
    }
  }

  WillModifyBootOrder = (BootOrderIndex > 0) || ((RequestedInstance == 0) && (ClassInstanceLength + VirtualInstanceLength > 1));

  // At this point BootOrderIndex is the entry to move to the start of the list first
  if (BootOptionsParameters->Parm5.Data1.Bits.PersistentOptions) {
    if (RequestedInstance == 0) {
      DEBUG ((DEBUG_ERROR, "IPMI requested to move all instances of %a to the start of BootOrder\n", RequestedClassName));
    } else {
      DEBUG ((DEBUG_ERROR, "IPMI requested to move %a instance %u to the start of BootOrder\n", RequestedClassName, RequestedInstance));
    }
  } else {
    if (RequestedInstance == 0) {
      DEBUG ((DEBUG_ERROR, "IPMI requested to use all instances of %a for this boot\n", RequestedClassName));
    } else {
      DEBUG ((DEBUG_ERROR, "IPMI requested to use %a instance %u for this boot\n", RequestedClassName, RequestedInstance));
    }

    // Prepare to restore BootOrder after this boot
    if (WillModifyBootOrder) {
      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_CALLBACK,
                      RestoreBootOrder,
                      NULL,
                      &gEfiEventReadyToBootGuid,
                      &ReadyToBootEvent
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error registering ReadyToBoot event handler to restore BootOrder: %r\n", __FUNCTION__, Status));
        goto AcknowledgeAndCleanup;
      }

      Status = gRT->SetVariable (
                      // This is not getting saved correctly so that it can be looked up!
                      SAVED_BOOT_ORDER_VARIABLE_NAME,
                      &gNVIDIATokenSpaceGuid,
                      EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                      BootOrderSize,
                      BootOrder
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error saving current BootOrder: %r\n", __FUNCTION__, Status));
        goto AcknowledgeAndCleanup;
      }

      BootOrderFlags = 0;
      if (RequestedInstance == 0) {
        BootOrderFlags |= SAVED_BOOT_ORDER_ALL_INSTANCES_FLAG;
      }

      if (VirtualBootClass != NULL) {
        BootOrderFlags |= SAVED_BOOT_ORDER_VIRTUAL_FLAG;
      }

      if (BootOrderFlags != 0) {
        // Save some flags for Restore
        Status = gRT->SetVariable (
                        SAVED_BOOT_ORDER_FLAGS_VARIABLE_NAME,
                        &gNVIDIATokenSpaceGuid,
                        EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                        sizeof (BootOrderFlags),
                        &BootOrderFlags
                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Error saving BootOrder flags: %r\n", __FUNCTION__, Status));
          goto AcknowledgeAndCleanup;
        }
      }
    }
  }

  // Finally, update the BootOrder if necessary
  if (WillModifyBootOrder) {
    if (BootOrderIndex > 0) {
      MOVE_INDEX_TO_START (BootOrder, BootOrderIndex);
    }

    if (RequestedInstance == 0) {
      // Note: In this case the unmoved ClassInstanceList and VirtualInstanceList elements are ordered
      // in the same order as in BootOrder, so we can be smart when searching BootOrder for them
      while (ClassInstanceLength > 0) {
        DesiredOptionNumber = ClassInstanceList[--ClassInstanceLength];
        while ((BootOrderIndex > 0) && (BootOrder[BootOrderIndex] != DesiredOptionNumber)) {
          BootOrderIndex--;
        }

        NV_ASSERT_RETURN (BootOrderIndex > 0, goto AcknowledgeAndCleanup, "%a: Failed to parse BootOrder correctly to find ClassInstance\n", __FUNCTION__);
        MOVE_INDEX_TO_START (BootOrder, BootOrderIndex);
      }

      BootOrderIndex = BootOrderLength;
      while (VirtualInstanceLength > 0) {
        DesiredOptionNumber = VirtualInstanceList[--VirtualInstanceLength];
        while ((BootOrderIndex > 0) && (BootOrder[BootOrderIndex] != DesiredOptionNumber)) {
          BootOrderIndex--;
        }

        NV_ASSERT_RETURN (BootOrderIndex > 0, goto AcknowledgeAndCleanup, "%a: Failed to parse BootOrder correctly to find VirtualInstance\n", __FUNCTION__);
        MOVE_INDEX_TO_START (BootOrder, BootOrderIndex);
      }
    }

    Status = gRT->SetVariable (
                    EFI_BOOT_ORDER_VARIABLE_NAME,
                    &gEfiGlobalVariableGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    BootOrderSize,
                    BootOrder
                    );
    if (EFI_ERROR (Status)) {
      if (RequestedInstance == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Error moving all instances of %a to the start of BootOrder: %r\n", __FUNCTION__, RequestedClassName, Status));
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Error moving %a instance %u to the start of BootOrder: %r\n", __FUNCTION__, RequestedClassName, RequestedInstance, Status));
      }
    }

    PrintBootOrder (DEBUG_INFO, L"BootOrder after IPMI-requested change:", NULL, 0);
  } else {
    DEBUG ((DEBUG_INFO, "%a: IPMI request doesn't modify BootOrder\n", __FUNCTION__));
  }

AcknowledgeAndCleanup:
  BootOptionsParameters = (IPMI_BOOT_OPTIONS_PARAMETERS *)&mBootOptionsRequest->ParameterData[0];

  SetMem (BootOptionsParameters, sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4), 0);
  BootOptionsParameters->Parm4.WriteMask                    = BOOT_OPTION_HANDLED_BY_BIOS;
  BootOptionsParameters->Parm4.BootInitiatorAcknowledgeData = 0;
  Status                                                    = SetIPMIBootOrderParameter (IPMI_BOOT_OPTIONS_PARAMETER_BOOT_INFO_ACK, mBootOptionsRequest);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Error acknowledging IPMI boot order request: %r\n", Status));
  }

  SetMem (BootOptionsParameters, sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5), 0);
  Status = SetIPMIBootOrderParameter (IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS, mBootOptionsRequest);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Error clearing IPMI boot flags: %r\n", Status));
  }

CleanupAndReturn:
  FREE_NON_NULL (mBootOptionsRequest);
  FREE_NON_NULL (mBootOptionsResponse);
  FREE_NON_NULL (ClassInstanceList);
  FREE_NON_NULL (VirtualInstanceList);
  FREE_NON_NULL (BootOrder);

  return;
}
