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


UINT8 RemovableReversePriorityBootOrder[] = {
  MSG_SD_DP,
  MSG_USB_DP
};


UINT8 NonRemovableReversePriorityBootOrder[] = {
  MSG_UFS_DP,
  MSG_EMMC_DP
};


VOID
EFIAPI
SetMediaBootPriority (
  IN UINT8 DeviceSubType
  )
{
  EFI_STATUS                   Status;
  UINT16                       *BootOrder;
  UINTN                        BootOrderSize;
  UINT16                       *NewOrder;
  UINT16                       *RemainBoots;
  UINTN                        SelectCnt;
  UINTN                        RemainCnt;
  UINTN                        Index;
  BOOLEAN                      NewOrderFound;
  CHAR16                       OptionName[sizeof ("Boot####")];
  EFI_BOOT_MANAGER_LOAD_OPTION Option;
  EFI_DEVICE_PATH_PROTOCOL     *DevicePathNode;

  GetEfiGlobalVariable2 (L"BootOrder", (VOID **) &BootOrder, &BootOrderSize);
  if (BootOrder == NULL) {
    return;
  }

  NewOrder = AllocatePool (BootOrderSize);
  RemainBoots = AllocatePool (BootOrderSize);
  if ((NewOrder == NULL) || (RemainBoots == NULL)) {
    goto Exit;
  }

  SelectCnt = 0;
  RemainCnt = 0;

  for (Index = 0; Index < BootOrderSize / sizeof (UINT16); Index++) {
    NewOrderFound = FALSE;
    UnicodeSPrint (OptionName, sizeof (OptionName), L"Boot%04x", BootOrder[Index]);
    Status = EfiBootManagerVariableToLoadOption (OptionName, &Option);
    if (EFI_ERROR (Status)) {
      continue;
    }

    DevicePathNode = Option.FilePath;
    while (!IsDevicePathEndType (DevicePathNode)) {
      if ((DevicePathType (DevicePathNode) == MESSAGING_DEVICE_PATH) &&
          (DevicePathSubType (DevicePathNode) == DeviceSubType)) {
        NewOrder[SelectCnt++] = BootOrder[Index];
        NewOrderFound = TRUE;
        break;
      }
      DevicePathNode = NextDevicePathNode (DevicePathNode);
    }

    if (!NewOrderFound) {
      RemainBoots[RemainCnt++] = BootOrder[Index];
    }
  }

  if (SelectCnt != 0) {
    for (Index = 0; Index < RemainCnt; Index++) {
      NewOrder[SelectCnt + Index] = RemainBoots[Index];
    }

    if (CompareMem (NewOrder, BootOrder, BootOrderSize) != 0) {
      Status = gRT->SetVariable (L"BootOrder", &gEfiGlobalVariableGuid,
                      EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                      BootOrderSize, NewOrder);
      if (EFI_ERROR (Status)) {
        goto Exit;
      }
    }
  }

Exit:
  FreePool (BootOrder);
  if (NewOrder != NULL) {
    FreePool (NewOrder);
  }
  if (RemainBoots != NULL) {
    FreePool (RemainBoots);
  }
}


VOID
EFIAPI
SetAndroidBootPriority (
  VOID
  )
{
  EFI_STATUS                   Status;
  UINT16                       *BootOrder;
  UINTN                        BootOrderSize;
  UINT16                       *NewOrder;
  UINT16                       *RemainBoots;
  UINTN                        SelectCnt;
  UINTN                        RemainCnt;
  EFI_BOOT_MANAGER_LOAD_OPTION Option;
  CHAR16                       OptionName[sizeof ("Boot####")];
  UINTN                        Index;
  UINTN                        OptionalDataSize;

  GetEfiGlobalVariable2 (L"BootOrder", (VOID **) &BootOrder, &BootOrderSize);
  if (BootOrder == NULL) {
    return;
  }

  NewOrder = AllocatePool (BootOrderSize);
  RemainBoots = AllocatePool (BootOrderSize);
  if ((NewOrder == NULL) || (RemainBoots == NULL)) {
    goto Exit;
  }

  SelectCnt = 0;
  RemainCnt = 0;

  for (Index = 0; Index < BootOrderSize / sizeof (UINT16); Index++) {
    UnicodeSPrint (OptionName, sizeof (OptionName), L"Boot%04x", BootOrder[Index]);
    Status = EfiBootManagerVariableToLoadOption (OptionName, &Option);
    if (EFI_ERROR (Status)) {
      continue;
    }

    OptionalDataSize = 0;
    if (Option.OptionalData != NULL) {
      OptionalDataSize = StrSize ((CONST CHAR16 *)Option.OptionalData);
    }
    if (Option.OptionalData == NULL ||
        Option.OptionalDataSize != OptionalDataSize + sizeof (EFI_GUID) ||
        !CompareGuid ((EFI_GUID *)((UINT8 *)Option.OptionalData + OptionalDataSize),
                      &gNVIDIABmBootOptionGuid)) {
      RemainBoots[RemainCnt++] = BootOrder[Index];
    } else {
      NewOrder[SelectCnt++] = BootOrder[Index];
    }
  }

  if (SelectCnt != 0) {
    for (Index = 0; Index < RemainCnt; Index++) {
      NewOrder[SelectCnt + Index] = RemainBoots[Index];
    }

    if (CompareMem (NewOrder, BootOrder, BootOrderSize) != 0) {
      Status = gRT->SetVariable (L"BootOrder", &gEfiGlobalVariableGuid,
                      EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                      BootOrderSize, NewOrder);
      if (EFI_ERROR (Status)) {
        goto Exit;
      }
    }
  }

Exit:
  FreePool (BootOrder);
  if (NewOrder != NULL) {
    FreePool (NewOrder);
  }
  if (RemainBoots != NULL) {
    FreePool (RemainBoots);
  }
}


VOID
EFIAPI
SetBootOrder (
  VOID
  )
{
  EFI_STATUS                   Status;
  BOOLEAN                      VariableData;
  UINTN                        VariableSize;
  UINT32                       VariableAttributes;
  UINTN                        Count;

  VariableData = FALSE;
  VariableSize = sizeof (BOOLEAN);
  Status = gRT->GetVariable (L"PlatformBootOrderSet", &gNVIDIATokenSpaceGuid,
                             &VariableAttributes, &VariableSize, (VOID *)&VariableData);
  if (!EFI_ERROR (Status) && (VariableSize == sizeof (BOOLEAN))) {
    if (VariableData == TRUE) {
      return;
    }
  }

  for (Count = 0;
       Count < sizeof (NonRemovableReversePriorityBootOrder) /
               sizeof (NonRemovableReversePriorityBootOrder[0]);
       Count++) {
    SetMediaBootPriority (NonRemovableReversePriorityBootOrder[Count]);
  }

  for (Count = 0;
       Count < sizeof (RemovableReversePriorityBootOrder) /
               sizeof (RemovableReversePriorityBootOrder[0]);
       Count++) {
    SetMediaBootPriority (RemovableReversePriorityBootOrder[Count]);
  }

  SetAndroidBootPriority ();

  VariableData = TRUE;
  gRT->SetVariable (L"PlatformBootOrderSet", &gNVIDIATokenSpaceGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    sizeof (BOOLEAN), &VariableData);

  return;
}
