/** @file
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2017, Linaro Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/SortLib.h>
#include <Library/DebugLib.h>

#define NVIDIA_BOOT_TYPE_HTTP     0
#define NVIDIA_BOOT_TYPE_BOOTIMG  1

typedef struct {
  CHAR8    *OrderName;
  INT32    PriorityOrder;
  UINT8    Type;
  UINT8    SubType;
  UINT8    ExtraSpecifier;
} NVIDIA_BOOT_ORDER_PRIORITY;

STATIC NVIDIA_BOOT_ORDER_PRIORITY  mBootPriority[] = {
  { "scsi",     MAX_INT32, MESSAGING_DEVICE_PATH, MSG_SCSI_DP,           MAX_UINT8                },
  { "usb",      MAX_INT32, MESSAGING_DEVICE_PATH, MSG_USB_DP,            MAX_UINT8                },
  { "sata",     MAX_INT32, MESSAGING_DEVICE_PATH, MSG_SATA_DP,           MAX_UINT8                },
  { "pxev4",    MAX_INT32, MESSAGING_DEVICE_PATH, MSG_IPv4_DP,           MAX_UINT8                },
  { "httpv4",   MAX_INT32, MESSAGING_DEVICE_PATH, MSG_IPv4_DP,           NVIDIA_BOOT_TYPE_HTTP    },
  { "pxev6",    MAX_INT32, MESSAGING_DEVICE_PATH, MSG_IPv6_DP,           MAX_UINT8                },
  { "httpv6",   MAX_INT32, MESSAGING_DEVICE_PATH, MSG_IPv6_DP,           NVIDIA_BOOT_TYPE_HTTP    },
  { "nvme",     MAX_INT32, MESSAGING_DEVICE_PATH, MSG_NVME_NAMESPACE_DP, MAX_UINT8                },
  { "ufs",      MAX_INT32, MESSAGING_DEVICE_PATH, MSG_UFS_DP,            MAX_UINT8                },
  { "sd",       MAX_INT32, MESSAGING_DEVICE_PATH, MSG_SD_DP,             MAX_UINT8                },
  { "emmc",     MAX_INT32, MESSAGING_DEVICE_PATH, MSG_EMMC_DP,           MAX_UINT8                },
  { "cdrom",    MAX_INT32, MEDIA_DEVICE_PATH,     MEDIA_CDROM_DP,        MAX_UINT8                },
  { "boot.img", MAX_INT32, MAX_UINT8,             MAX_UINT8,             NVIDIA_BOOT_TYPE_BOOTIMG },
};

#define DEFAULT_BOOT_ORDER_STRING  "boot.img,usb,sd,emmc,ufs"

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
  UINTN                         OptionalDataSize;
  UINTN                         BootPriorityIndex;
  EFI_DEVICE_PATH_PROTOCOL      *DevicePathNode;
  UINT8                         ExtraSpecifier;

  UnicodeSPrint (OptionName, sizeof (OptionName), L"Boot%04x", BootOption);
  Status = EfiBootManagerVariableToLoadOption (OptionName, &Option);
  if (EFI_ERROR (Status)) {
    return MAX_INT32;
  }

  OptionalDataSize = 0;
  if (Option.OptionalData != NULL) {
    OptionalDataSize = StrSize ((CONST CHAR16 *)Option.OptionalData);
  }

  if ((Option.OptionalData != NULL) &&
      (Option.OptionalDataSize == OptionalDataSize + sizeof (EFI_GUID)) &&
      CompareGuid (
        (EFI_GUID *)((UINT8 *)Option.OptionalData + OptionalDataSize),
        &gNVIDIABmBootOptionGuid
        ))
  {
    for (BootPriorityIndex = 0; BootPriorityIndex < ARRAY_SIZE (mBootPriority); BootPriorityIndex++) {
      if (mBootPriority[BootPriorityIndex].ExtraSpecifier == NVIDIA_BOOT_TYPE_BOOTIMG) {
        DEBUG ((DEBUG_VERBOSE, "Found %s priority to be %d\r\n", ConvertDevicePathToText (Option.FilePath, TRUE, FALSE), mBootPriority[BootPriorityIndex].PriorityOrder));
        return mBootPriority[BootPriorityIndex].PriorityOrder;
      }
    }
  }

  ExtraSpecifier = MAX_UINT8;
  DevicePathNode = Option.FilePath;
  while (!IsDevicePathEndType (DevicePathNode)) {
    if ((DevicePathType (DevicePathNode) == MESSAGING_DEVICE_PATH) &&
        (DevicePathSubType (DevicePathNode) == MSG_URI_DP))
    {
      ExtraSpecifier = NVIDIA_BOOT_TYPE_HTTP;
      break;
    }

    DevicePathNode = NextDevicePathNode (DevicePathNode);
  }

  DevicePathNode = Option.FilePath;
  while (!IsDevicePathEndType (DevicePathNode)) {
    for (BootPriorityIndex = 0; BootPriorityIndex < ARRAY_SIZE (mBootPriority); BootPriorityIndex++) {
      if ((DevicePathType (DevicePathNode) == mBootPriority[BootPriorityIndex].Type) &&
          (DevicePathSubType (DevicePathNode) == mBootPriority[BootPriorityIndex].SubType) &&
          (ExtraSpecifier == mBootPriority[BootPriorityIndex].ExtraSpecifier))
      {
        DEBUG ((DEBUG_VERBOSE, "Found %s priority to be %d\r\n", ConvertDevicePathToText (Option.FilePath, TRUE, FALSE), mBootPriority[BootPriorityIndex].PriorityOrder));
        return mBootPriority[BootPriorityIndex].PriorityOrder;
      }
    }

    DevicePathNode = NextDevicePathNode (DevicePathNode);
  }

  DEBUG ((DEBUG_VERBOSE, "Found %s priority to be %d\r\n", ConvertDevicePathToText (Option.FilePath, TRUE, FALSE), MAX_INT32));
  return MAX_INT32;
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
SetBootOrder (
  VOID
  )
{
  EFI_STATUS  Status;
  BOOLEAN     VariableData;
  UINTN       VariableSize;
  UINT32      VariableAttributes;
  UINT16      *BootOrder;
  UINTN       BootOrderSize;
  UINT16      *NewOrder;
  INT32       Priority;
  CHAR8       *DefaultBootOrder;
  UINTN       DefaultBootOrderSize;
  CHAR8       *CurrentBootPriorityStr;
  CHAR8       *CurrentBootPriorityEnd;
  UINTN       CurrentBootPriorityLen;
  UINTN       BootPriorityMatchLen;
  UINTN       BootPriorityIndex;

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
    CurrentBootPriorityEnd = CurrentBootPriorityStr;
    while (CurrentBootPriorityEnd < (DefaultBootOrder + DefaultBootOrderSize)) {
      if ((*CurrentBootPriorityEnd == ',') ||
          (*CurrentBootPriorityEnd == '\0'))
      {
        break;
      }

      CurrentBootPriorityEnd++;
    }

    CurrentBootPriorityLen = CurrentBootPriorityEnd - CurrentBootPriorityStr;

    for (BootPriorityIndex = 0; BootPriorityIndex < ARRAY_SIZE (mBootPriority); BootPriorityIndex++) {
      BootPriorityMatchLen = AsciiStrLen (mBootPriority[BootPriorityIndex].OrderName);
      if ((BootPriorityMatchLen == CurrentBootPriorityLen) &&
          (CompareMem (CurrentBootPriorityStr, mBootPriority[BootPriorityIndex].OrderName, CurrentBootPriorityLen) == 0))
      {
        DEBUG ((DEBUG_INFO, "Setting %a priority to %d\r\n", mBootPriority[BootPriorityIndex].OrderName, Priority));
        mBootPriority[BootPriorityIndex].PriorityOrder = Priority;
        Priority++;
        break;
      }
    }

    CurrentBootPriorityStr += CurrentBootPriorityLen + 1;
  }

  GetEfiGlobalVariable2 (L"BootOrder", (VOID **)&BootOrder, &BootOrderSize);
  if (BootOrder == NULL) {
    return;
  }

  NewOrder = AllocateCopyPool (BootOrderSize, BootOrder);
  PerformQuickSort (NewOrder, BootOrderSize/sizeof (UINT16), sizeof (UINT16), BootOrderSortCompare);
  if (CompareMem (NewOrder, BootOrder, BootOrderSize) != 0) {
    Status = gRT->SetVariable (
                    L"BootOrder",
                    &gEfiGlobalVariableGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    BootOrderSize,
                    NewOrder
                    );
    if (EFI_ERROR (Status)) {
      return;
    }
  }

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
