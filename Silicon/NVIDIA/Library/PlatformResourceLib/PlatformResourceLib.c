/** @file
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include <Library/IoLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PlatformResourcePrivateLib.h>
#include "T194ResourceConfig.h"
#include "T186ResourceConfig.h"


/**
  Retrieve Tegra UART Base Address

**/
UINTN
EFIAPI
GetTegraUARTBaseAddress (
  IN  BOOLEAN ConsolePort
)
{
  UINTN   ChipID;
  UINTN   TegraUARTBase;
  BOOLEAN ValidPrivatePlatform;

  ValidPrivatePlatform = FALSE;
  ValidPrivatePlatform = GetTegraUARTBaseAddressPrivate (ConsolePort, &TegraUARTBase);
  if (ValidPrivatePlatform) {
    return TegraUARTBase;
  }

  ChipID = TegraGetChipID();

  switch (ChipID) {
    case T186_CHIP_ID:
      return FixedPcdGet64(PcdTegra16550UartBaseT186);
    case T194_CHIP_ID:
      if (ConsolePort) {
        return FixedPcdGet64(PcdTegra16550UartBaseT194B);
      } else {
        return FixedPcdGet64(PcdTegra16550UartBaseT194C);
      }
    default:
      return 0x0;
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

  ValidPrivatePlatform = FALSE;
  ValidPrivatePlatform = GetCPUBLBaseAddressPrivate (&CpuBootloaderAddress);
  if (ValidPrivatePlatform) {
    return CpuBootloaderAddress;
  }

  ChipID = TegraGetChipID();
  CpuBootloaderAddress = (UINTN)MmioRead32 (TegraGetBLInfoLocationAddress(ChipID));
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

  ValidPrivatePlatform = FALSE;
  ValidPrivatePlatform = GetDTBBaseAddressPrivate (&DTBBaseAddress);
  if (ValidPrivatePlatform) {
    return DTBBaseAddress;
  }

  ChipID = TegraGetChipID();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T186_CHIP_ID:
      return T186GetDTBBaseAddress(CpuBootloaderAddress);
    case T194_CHIP_ID:
      return T194GetDTBBaseAddress(CpuBootloaderAddress);
    default:
      return 0x0;
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

  ValidPrivatePlatform = FALSE;
  ValidPrivatePlatform = GetCPUBLBaseAddressPrivate (&CpuBootloaderAddress);
  if (ValidPrivatePlatform) {
    return GetResourceConfigPrivate (CpuBootloaderAddress, PlatformInfo);;
  }

  ChipID = TegraGetChipID();

  switch (ChipID) {
    case T186_CHIP_ID:
      return T186ResourceConfig(GetCPUBLBaseAddress (), PlatformInfo);
    case T194_CHIP_ID:
      return T194ResourceConfig(GetCPUBLBaseAddress (), PlatformInfo);
    default:
      PlatformInfo = NULL;
      return EFI_UNSUPPORTED;
  }
}
