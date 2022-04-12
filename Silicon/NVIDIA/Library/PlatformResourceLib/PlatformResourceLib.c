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
  Retrieve chip specific info for GIC

**/
BOOLEAN
EFIAPI
GetGicInfo (
  OUT TEGRA_GIC_INFO *GicInfo
)
{
  UINTN           ChipID;
  BOOLEAN         ValidPrivatePlatform;

  ValidPrivatePlatform = GetGicInfoInternal (GicInfo);
  if (ValidPrivatePlatform) {
    return TRUE;
  }

  ChipID = TegraGetChipID();

  switch (ChipID) {
     case T194_CHIP_ID:
       //*CompatString = "arm,gic-v2";
       GicInfo->GicCompatString = "arm,gic-400";
       GicInfo->ItsCompatString = "";
       GicInfo->Version = 2;
       break;
     case T234_CHIP_ID:
       GicInfo->GicCompatString = "arm,gic-v3";
       GicInfo->ItsCompatString = "arm,gic-v3-its";
       GicInfo->Version = 3;
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
  UINTN   ChipID;
  UINTN   CpuBootloaderAddress;
  UINTN   CpuBootloaderAddressLo;
  UINTN   CpuBootloaderAddressHi;
  UINTN   SystemMemoryBaseAddress;
  BOOLEAN ValidPrivatePlatform;

  ValidPrivatePlatform = GetCPUBLBaseAddressInternal (&CpuBootloaderAddress);
  if (ValidPrivatePlatform) {
    return CpuBootloaderAddress;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddressLo = (UINTN)MmioRead32 (TegraGetBLInfoLocationAddress(ChipID));
  CpuBootloaderAddressHi = (UINTN)MmioRead32 (TegraGetBLInfoLocationAddress(ChipID) + sizeof (UINT32));
  CpuBootloaderAddress = (CpuBootloaderAddressHi << 32) | CpuBootloaderAddressLo;

  if (ChipID == T194_CHIP_ID) {
    SystemMemoryBaseAddress = TegraGetSystemMemoryBaseAddress(ChipID);

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
  Retrieve Carveout Info

**/
EFI_STATUS
EFIAPI
GetCarveoutInfo (
  IN TEGRA_CARVEOUT_TYPE Type,
  IN UINTN               *Base,
  IN UINT32              *Size
)
{
  EFI_STATUS Status;
  UINTN      ChipID;
  UINTN      CpuBootloaderAddress;

  Status = GetCarveoutInfoInternal (Type, Base, Size);
  if (!EFI_ERROR (Status)) {
    return Status;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetCarveoutInfo(CpuBootloaderAddress, Type, Base, Size);
    case T234_CHIP_ID:
      return T234GetCarveoutInfo(CpuBootloaderAddress, Type, Base, Size);
    default:
      return EFI_UNSUPPORTED;
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
      return T194ValidateActiveBootChain(CpuBootloaderAddress);
    default:
      return EFI_UNSUPPORTED;
  }
}

/**
  Get Ramloaded OS Base and Size

**/
BOOLEAN
EFIAPI
GetRamdiskOSBaseAndSize (
  OUT UINTN *Base,
  OUT UINTN *Size
)
{
  BOOLEAN ValidPrivatePlatform;

  ValidPrivatePlatform = GetRamdiskOSBaseAndSizeInternal (Base, Size);
  if (ValidPrivatePlatform) {
    return ValidPrivatePlatform;
  }

  return FALSE;
}

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
GetPlatformResourceInformation (
  IN TEGRA_PLATFORM_RESOURCE_INFO *PlatformResourceInfo
)
{
  UINTN           ChipID;
  UINTN           CpuBootloaderAddress;
  BOOLEAN         ValidPrivatePlatform;

  PlatformResourceInfo->ResourceInfo = AllocatePool (sizeof (TEGRA_RESOURCE_INFO));
  if (PlatformResourceInfo->ResourceInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  PlatformResourceInfo->BoardInfo = AllocatePool (sizeof (TEGRA_BOARD_INFO));
  if (PlatformResourceInfo->BoardInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ValidPrivatePlatform = GetPlatformResourceInformationInternal(PlatformResourceInfo);
  if (ValidPrivatePlatform) {
    return EFI_SUCCESS;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234GetPlatformResourceInformation(CpuBootloaderAddress, PlatformResourceInfo);
    case T194_CHIP_ID:
      return T194GetPlatformResourceInformation(CpuBootloaderAddress, PlatformResourceInfo);
    default:
      return EFI_UNSUPPORTED;
  }
}
