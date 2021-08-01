/** @file

  Tegra Platform Init Driver.

  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>

STATIC
EFI_STATUS
EFIAPI
UseEmulatedVariableStore (
  IN EFI_HANDLE        ImageHandle
  )
{
  EFI_STATUS Status;

  PcdSetBoolS(PcdEmuVariableNvModeEnable, TRUE);
  Status = gBS->InstallMultipleProtocolInterfaces (
             &ImageHandle,
             &gEdkiiNvVarStoreFormattedGuid,
             NULL,
             NULL
             );
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error installing EmuVariableNvModeEnableProtocol\n", __FUNCTION__));
  }

  return Status;
}


/**
  Runtime Configuration Of Tegra Platform.
**/
EFI_STATUS
EFIAPI
TegraPlatformInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS              Status;
  UINTN                   ChipID;
  TEGRA_PLATFORM_TYPE     PlatformType;
  UINTN                   EmmcMagic;
  BOOLEAN                 EmulatedVariablesUsed;

  EmulatedVariablesUsed = FALSE;

  ChipID = TegraGetChipID();
  DEBUG ((DEBUG_INFO, "%a: Tegra Chip ID:  0x%x\n", __FUNCTION__, ChipID));

  PlatformType = TegraGetPlatform();

  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    if (ChipID == T194_CHIP_ID) {
      LibPcdSetSku (T194_SKU);
    } else if (ChipID == T234_CHIP_ID) {
      LibPcdSetSku (T234_SKU);
    }
  } else {
    // Override boot timeout for pre-si platforms
    EmmcMagic = * ((UINTN *) (TegraGetSystemMemoryBaseAddress(ChipID) + SYSIMG_EMMC_MAGIC_OFFSET));
    if ((EmmcMagic != SYSIMG_EMMC_MAGIC) && (EmmcMagic == SYSIMG_DEFAULT_MAGIC)) {
      EmulatedVariablesUsed = TRUE;
    }
  }

  if (GetBootType () == TegrablBootRcm) {
    EmulatedVariablesUsed = TRUE;
  }

  if (EmulatedVariablesUsed) {
    // Enable emulated variable NV mode in variable driver when ram loading images and emmc
    // is not enabled.
    Status = UseEmulatedVariableStore (ImageHandle);
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}
