/** @file
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
*  Portions provided under the following terms:
*  Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#include <Library/IoLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PlatformResourceInternalLib.h>
#include "T194ResourceConfig.h"
#include "T234ResourceConfig.h"


STATIC
EFI_PHYSICAL_ADDRESS  TegraUARTBaseAddress = 0x0;

/**
  Set Tegra UART Base Address
**/
VOID
EFIAPI
SetTegraUARTBaseAddress (
  IN EFI_PHYSICAL_ADDRESS   UartBaseAddress
)
{
  TegraUARTBaseAddress = UartBaseAddress;
}

/**
  Retrieve Tegra UART Base Address

**/
EFI_PHYSICAL_ADDRESS
EFIAPI
GetTegraUARTBaseAddress (
  VOID
)
{
  UINTN                 ChipID;
  EFI_PHYSICAL_ADDRESS  TegraUARTBase;
  BOOLEAN               ValidPrivatePlatform;

  if (TegraUARTBaseAddress != 0x0) {
    return TegraUARTBaseAddress;
  }

  ValidPrivatePlatform = GetTegraUARTBaseAddressInternal (&TegraUARTBase);
  if (ValidPrivatePlatform) {
    return TegraUARTBase;
  }

  ChipID = TegraGetChipID();

  switch (ChipID) {
    case T194_CHIP_ID:
      return FixedPcdGet64(PcdTegra16550UartBaseT194);
    case T234_CHIP_ID:
      return FixedPcdGet64(PcdTegra16550UartBaseT234);
    default:
      return 0x0;
  }
}

/**
  It's to get the UART instance number that the trust-firmware hands over.
  Currently that chain is broken so temporarily override the UART instance number
  to the fixed known id based on the chip id.

**/
STATIC
BOOLEAN
GetSharedUARTInstanceId (
  IN  UINTN     ChipID,
  OUT UINT32    *UARTInstanceNumber
)
{
  switch (ChipID) {
    case T234_CHIP_ID:
      *UARTInstanceNumber = 1; // UART_A
      return TRUE;
    default:
      return FALSE;
  }
}

/**
  Retrieve the type and address of UART based on the instance Number

**/
EFI_STATUS
EFIAPI
GetUARTInstanceInfo (
  OUT UINT32                *UARTInstanceType,
  OUT EFI_PHYSICAL_ADDRESS  *UARTInstanceAddress
)
{
  UINTN   ChipID;
  UINT32  SharedUARTInstanceId;
  BOOLEAN ValidPrivatePlatform;

  *UARTInstanceType = TEGRA_UART_TYPE_NONE;
  *UARTInstanceAddress = 0x0;

  ValidPrivatePlatform = GetUARTInstanceInfoInternal (UARTInstanceType, UARTInstanceAddress);
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T194_CHIP_ID:
      *UARTInstanceType = TEGRA_UART_TYPE_TCU;
      *UARTInstanceAddress = (EFI_PHYSICAL_ADDRESS)FixedPcdGet64(PcdTegra16550UartBaseT194);
      return EFI_SUCCESS;
    case T234_CHIP_ID:
   	  if (!GetSharedUARTInstanceId (T234_CHIP_ID, &SharedUARTInstanceId)) {
   	    return EFI_UNSUPPORTED;
   	  }
      return T234UARTInstanceInfo (SharedUARTInstanceId, UARTInstanceType, UARTInstanceAddress);
    default:
      return EFI_UNSUPPORTED;
  }
}

/**
  Retrieve CPU BL Address

**/
UINTN
EFIAPI
GetCPUBLBaseAddress (
  VOID
)
{
  UINTN   ChipID;
  UINTN   CpuBootloaderAddress;
  UINTN   SystemMemoryBaseAddress;
  BOOLEAN ValidPrivatePlatform;

  ValidPrivatePlatform = GetCPUBLBaseAddressInternal (&CpuBootloaderAddress);
  if (ValidPrivatePlatform) {
    return CpuBootloaderAddress;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = (UINTN)MmioRead32 (TegraGetBLInfoLocationAddress(ChipID));

  if (ChipID == T234_CHIP_ID) {
    return CpuBootloaderAddress;
  }

  SystemMemoryBaseAddress = TegraGetSystemMemoryBaseAddress(ChipID);

  if (CpuBootloaderAddress < SystemMemoryBaseAddress) {
    CpuBootloaderAddress <<= 16;
  }

  return CpuBootloaderAddress;
}

/**
  Retrieve DTB Address

**/
UINT64
EFIAPI
GetDTBBaseAddress (
  VOID
)
{
  UINTN   ChipID;
  UINTN   CpuBootloaderAddress;
  UINT64  DTBBaseAddress;
  BOOLEAN ValidPrivatePlatform;

  ValidPrivatePlatform = GetDTBBaseAddressInternal (&DTBBaseAddress);
  if (ValidPrivatePlatform) {
    return DTBBaseAddress;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetDTBBaseAddress(CpuBootloaderAddress);
    case T234_CHIP_ID:
      return T234GetDTBBaseAddress(CpuBootloaderAddress);
    default:
      return 0x0;
  }
}

/**
  Retrieve RCM Blob Address

**/
UINT64
EFIAPI
GetRCMBaseAddress (
  VOID
)
{
  UINTN   ChipID;
  UINTN   CpuBootloaderAddress;
  UINT64  RCMBaseAddress;
  BOOLEAN ValidPrivatePlatform;

  ValidPrivatePlatform = GetRCMBaseAddressInternal (&RCMBaseAddress);
  if (ValidPrivatePlatform) {
    return RCMBaseAddress;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetRCMBaseAddress(CpuBootloaderAddress);
    case T234_CHIP_ID:
      return T234GetRCMBaseAddress(CpuBootloaderAddress);
    default:
      return 0x0;
  }
}

/**
  Retrieve Boot Type

**/
TEGRA_BOOT_TYPE
EFIAPI
GetBootType (
  VOID
)
{
  UINTN           ChipID;
  UINTN           CpuBootloaderAddress;
  TEGRA_BOOT_TYPE BootType;
  BOOLEAN         ValidPrivatePlatform;

  ValidPrivatePlatform = GetBootTypeInternal (&BootType);
  if (ValidPrivatePlatform) {
    return BootType;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetBootType(CpuBootloaderAddress);
    case T234_CHIP_ID:
      return T234GetBootType(CpuBootloaderAddress);
    default:
      return TegrablBootTypeMax;
  }
}

/**
  Retrieve Resource Config

**/
EFI_STATUS
EFIAPI
GetResourceConfig (
  OUT TEGRA_RESOURCE_INFO *PlatformInfo
)
{
  UINTN   ChipID;
  UINTN   CpuBootloaderAddress;
  BOOLEAN ValidPrivatePlatform;

  ValidPrivatePlatform = GetCPUBLBaseAddressInternal (&CpuBootloaderAddress);
  if (ValidPrivatePlatform) {
    return GetResourceConfigInternal (CpuBootloaderAddress, PlatformInfo);;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194ResourceConfig(CpuBootloaderAddress, PlatformInfo);
    case T234_CHIP_ID:
      return T234ResourceConfig(CpuBootloaderAddress, PlatformInfo);
    default:
      PlatformInfo = NULL;
      return EFI_UNSUPPORTED;
  }
}

/**
  Retrieve GR Blob Address

**/
UINT64
EFIAPI
GetGRBlobBaseAddress (
  VOID
)
{
  UINTN   ChipID;
  UINTN   CpuBootloaderAddress;
  UINT64  GRBlobBaseAddress;
  BOOLEAN ValidPrivatePlatform;

  ValidPrivatePlatform = GetGRBlobBaseAddressInternal (&GRBlobBaseAddress);
  if (ValidPrivatePlatform) {
    return GRBlobBaseAddress;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetGRBlobBaseAddress(CpuBootloaderAddress);
    case T234_CHIP_ID:
      return T234GetGRBlobBaseAddress(CpuBootloaderAddress);
    default:
      return 0x0;
  }
}

/**
  Retrieve GR Output Base and Size

**/
BOOLEAN
EFIAPI
GetGROutputBaseAndSize (
  OUT UINTN *Base,
  OUT UINTN *Size
)
{
  UINTN   ChipID;
  UINTN   CpuBootloaderAddress;
  BOOLEAN ValidPrivatePlatform;

  ValidPrivatePlatform = GetGROutputBaseAndSizeInternal (Base, Size);
  if (ValidPrivatePlatform) {
    return ValidPrivatePlatform;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetGROutputBaseAndSize(CpuBootloaderAddress, Base, Size);
    case T234_CHIP_ID:
      return T234GetGROutputBaseAndSize(CpuBootloaderAddress, Base, Size);
    default:
      return FALSE;
  }
}

/**
  Retrieve FSI NS Base and Size

**/
BOOLEAN
EFIAPI
GetFsiNsBaseAndSize (
  OUT UINTN *Base,
  OUT UINTN *Size
)
{
  UINTN   ChipID;
  UINTN   CpuBootloaderAddress;
  BOOLEAN ValidPrivatePlatform;

  ValidPrivatePlatform = GetFsiNsBaseAndSizeInternal (Base, Size);
  if (ValidPrivatePlatform) {
    return ValidPrivatePlatform;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234GetFsiNsBaseAndSize(CpuBootloaderAddress, Base, Size);
    default:
      return FALSE;
  }
}

/**
  Retrieve MMIO Base and Size

**/
TEGRA_MMIO_INFO*
EFIAPI
GetMmioBaseAndSize (
  VOID
)
{
  UINTN           ChipID;
  TEGRA_MMIO_INFO *MmioInfo;

  MmioInfo = GetMmioBaseAndSizeInternal ();
  if (MmioInfo != NULL) {
    return MmioInfo;
  }

  ChipID = TegraGetChipID();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetMmioBaseAndSize();
    case T234_CHIP_ID:
      return T234GetMmioBaseAndSize();
    default:
      return NULL;
  }
}

/**
  Retrieve CVM EEPROM Data

**/
UINT32
EFIAPI
GetCvmEepromData (
  OUT UINT8 **Data
)
{
  UINTN   ChipID;
  UINTN   CpuBootloaderAddress;

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234GetCvmEepromData(CpuBootloaderAddress, Data);
    default:
      return 0;
  }
}

/**
  Retrieve Board Information

**/
EFI_STATUS
EFIAPI
GetBoardInfo (
  OUT TEGRA_BOARD_INFO *BoardInfo
)
{
  UINTN           ChipID;
  UINTN           CpuBootloaderAddress;
  BOOLEAN         ValidPrivatePlatform;

  ValidPrivatePlatform = GetBoardInfoInternal(BoardInfo);
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234GetBoardInfo(CpuBootloaderAddress, BoardInfo);
    case T194_CHIP_ID:
      return EFI_UNSUPPORTED;
    default:
      return EFI_UNSUPPORTED;
  }
}

/**
  Retrieve Active Boot Chain Information

**/
EFI_STATUS
EFIAPI
GetActiveBootChain (
  OUT UINT32 *BootChain
)
{
  UINTN           ChipID;
  UINTN           CpuBootloaderAddress;
  BOOLEAN         ValidPrivatePlatform;

  ValidPrivatePlatform = GetActiveBootChainInternal(BootChain);
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234GetActiveBootChain(CpuBootloaderAddress, BootChain);
    case T194_CHIP_ID:
      return EFI_UNSUPPORTED;
    default:
      return EFI_UNSUPPORTED;
  }
}

/**
  Validate Active Boot Chain

**/
EFI_STATUS
EFIAPI
ValidateActiveBootChain (
  VOID
)
{
  UINTN           ChipID;
  UINTN           CpuBootloaderAddress;
  BOOLEAN         ValidPrivatePlatform;

  ValidPrivatePlatform = ValidateActiveBootChainInternal();
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234ValidateActiveBootChain(CpuBootloaderAddress);
    case T194_CHIP_ID:
      return EFI_UNSUPPORTED;
    default:
      return EFI_UNSUPPORTED;
  }
}
