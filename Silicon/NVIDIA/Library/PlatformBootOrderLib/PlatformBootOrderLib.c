/** @file
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
*  Copyright (c) 2017, Linaro Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
*  Portions provided under the following terms:
*  Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>


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
  UINT16                       *NewOrder;
  UINT16                       *RemainBoots;
  UINT16                       *BootOrder;
  UINTN                        BootOrderSize;
  EFI_BOOT_MANAGER_LOAD_OPTION Option;
  CHAR16                       OptionName[sizeof ("Boot####")];
  UINTN                        Index;
  UINTN                        SelectCnt;
  UINTN                        RemainCnt;
  UINTN                        OptionalDataSize;

  VariableData = FALSE;
  VariableSize = sizeof (BOOLEAN);
  Status = gRT->GetVariable (L"PlatformBootOrderSet", &gNVIDIATokenSpaceGuid,
                             &VariableAttributes, &VariableSize, (VOID *)&VariableData);
  if (!EFI_ERROR (Status) && (VariableSize == sizeof (BOOLEAN))) {
    if (VariableData == TRUE) {
      return;
    }
  }

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
      VariableData = TRUE;
      Status = gRT->SetVariable (L"PlatformBootOrderSet", &gNVIDIATokenSpaceGuid,
                      EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                      sizeof (BOOLEAN), &VariableData);
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
