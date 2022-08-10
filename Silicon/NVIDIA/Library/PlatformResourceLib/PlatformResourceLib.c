/** @file
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/MceAriLib.h>
#include <Library/NvgLib.h>
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
  IN EFI_PHYSICAL_ADDRESS  UartBaseAddress
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

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return FixedPcdGet64 (PcdTegra16550UartBaseT194);
    case T234_CHIP_ID:
      return FixedPcdGet64 (PcdTegra16550UartBaseT234);
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
  IN  UINTN   ChipID,
  OUT UINT32  *UARTInstanceNumber
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
  UINTN    ChipID;
  UINT32   SharedUARTInstanceId;
  BOOLEAN  ValidPrivatePlatform;

  *UARTInstanceType    = TEGRA_UART_TYPE_NONE;
  *UARTInstanceAddress = 0x0;

  ValidPrivatePlatform = GetUARTInstanceInfoInternal (UARTInstanceType, UARTInstanceAddress);
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T194_CHIP_ID:
      *UARTInstanceType    = TEGRA_UART_TYPE_TCU;
      *UARTInstanceAddress = (EFI_PHYSICAL_ADDRESS)FixedPcdGet64 (PcdTegra16550UartBaseT194);
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
  Retrieve chip specific info for GIC

**/
BOOLEAN
EFIAPI
GetGicInfo (
  OUT TEGRA_GIC_INFO  *GicInfo
  )
{
  UINTN    ChipID;
  BOOLEAN  ValidPrivatePlatform;

  ValidPrivatePlatform = GetGicInfoInternal (GicInfo);
  if (ValidPrivatePlatform) {
    return TRUE;
  }

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T194_CHIP_ID:
      // *CompatString = "arm,gic-v2";
      GicInfo->GicCompatString = "arm,gic-400";
      GicInfo->ItsCompatString = "";
      GicInfo->Version         = 2;
      break;
    case T234_CHIP_ID:
      GicInfo->GicCompatString = "arm,gic-v3";
      GicInfo->ItsCompatString = "arm,gic-v3-its";
      GicInfo->Version         = 3;
      break;
    default:
      return FALSE;
  }

  return TRUE;
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
  UINTN    ChipID;
  UINTN    CpuBootloaderAddress;
  UINTN    CpuBootloaderAddressLo;
  UINTN    CpuBootloaderAddressHi;
  UINTN    SystemMemoryBaseAddress;
  BOOLEAN  ValidPrivatePlatform;

  ValidPrivatePlatform = GetCPUBLBaseAddressInternal (&CpuBootloaderAddress);
  if (ValidPrivatePlatform) {
    return CpuBootloaderAddress;
  }

  ChipID = TegraGetChipID ();

  CpuBootloaderAddressLo = (UINTN)MmioRead32 (TegraGetBLInfoLocationAddress (ChipID));
  CpuBootloaderAddressHi = (UINTN)MmioRead32 (TegraGetBLInfoLocationAddress (ChipID) + sizeof (UINT32));
  CpuBootloaderAddress   = (CpuBootloaderAddressHi << 32) | CpuBootloaderAddressLo;

  if (ChipID == T194_CHIP_ID) {
    SystemMemoryBaseAddress = TegraGetSystemMemoryBaseAddress (ChipID);

    if (CpuBootloaderAddress < SystemMemoryBaseAddress) {
      CpuBootloaderAddress <<= 16;
    }
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
  UINTN    ChipID;
  UINTN    CpuBootloaderAddress;
  UINT64   DTBBaseAddress;
  BOOLEAN  ValidPrivatePlatform;

  ValidPrivatePlatform = GetDTBBaseAddressInternal (&DTBBaseAddress);
  if (ValidPrivatePlatform) {
    return DTBBaseAddress;
  }

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetDTBBaseAddress (CpuBootloaderAddress);
    case T234_CHIP_ID:
      return T234GetDTBBaseAddress (CpuBootloaderAddress);
    default:
      return 0x0;
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
  UINTN    ChipID;
  UINTN    CpuBootloaderAddress;
  UINT64   GRBlobBaseAddress;
  BOOLEAN  ValidPrivatePlatform;

  ValidPrivatePlatform = GetGRBlobBaseAddressInternal (&GRBlobBaseAddress);
  if (ValidPrivatePlatform) {
    return GRBlobBaseAddress;
  }

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetGRBlobBaseAddress (CpuBootloaderAddress);
    case T234_CHIP_ID:
      return T234GetGRBlobBaseAddress (CpuBootloaderAddress);
    default:
      return 0x0;
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
  UINTN    ChipID;
  UINTN    CpuBootloaderAddress;
  BOOLEAN  ValidPrivatePlatform;

  ValidPrivatePlatform = ValidateActiveBootChainInternal ();
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234ValidateActiveBootChain (CpuBootloaderAddress);
    case T194_CHIP_ID:
      return T194ValidateActiveBootChain (CpuBootloaderAddress);
    default:
      return EFI_UNSUPPORTED;
  }
}

/**
  Set next boot chain

**/
EFI_STATUS
EFIAPI
SetNextBootChain (
  IN  UINT32  BootChain
  )
{
  UINTN    ChipID;
  BOOLEAN  ValidPrivatePlatform;

  ValidPrivatePlatform = SetNextBootChainInternal (BootChain);
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234SetNextBootChain (BootChain);
      break;
    case T194_CHIP_ID:
      return T194SetNextBootChain (BootChain);
      break;
    default:
      return EFI_UNSUPPORTED;
      break;
  }
}

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
GetPlatformResourceInformation (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  UINTN    ChipID;
  UINTN    CpuBootloaderAddress;
  BOOLEAN  ValidPrivatePlatform;

  PlatformResourceInfo->ResourceInfo = AllocateZeroPool (sizeof (TEGRA_RESOURCE_INFO));
  if (PlatformResourceInfo->ResourceInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  PlatformResourceInfo->BoardInfo = AllocateZeroPool (sizeof (TEGRA_BOARD_INFO));
  if (PlatformResourceInfo->BoardInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ValidPrivatePlatform = GetPlatformResourceInformationInternal (PlatformResourceInfo);
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234GetPlatformResourceInformation (CpuBootloaderAddress, PlatformResourceInfo);
    case T194_CHIP_ID:
      return T194GetPlatformResourceInformation (CpuBootloaderAddress, PlatformResourceInfo);
    default:
      return EFI_UNSUPPORTED;
  }
}

EFI_STATUS
EFIAPI
GetRootfsStatusReg (
  IN UINT32  *RegisterValue
  )
{
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234GetRootfsStatusReg (CpuBootloaderAddress, RegisterValue);
    case T194_CHIP_ID:
      return T194GetRootfsStatusReg (CpuBootloaderAddress, RegisterValue);
    default:
      return EFI_UNSUPPORTED;
  }
}

EFI_STATUS
EFIAPI
SetRootfsStatusReg (
  IN UINT32  RegisterValue
  )
{
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234SetRootfsStatusReg (CpuBootloaderAddress, RegisterValue);
    case T194_CHIP_ID:
      return T194SetRootfsStatusReg (CpuBootloaderAddress, RegisterValue);
    default:
      return EFI_UNSUPPORTED;
  }
}
