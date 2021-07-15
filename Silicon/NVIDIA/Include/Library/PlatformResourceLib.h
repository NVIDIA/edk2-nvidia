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
**/

#ifndef __PLATFORM_RESOURCE_LIB_H__
#define __PLATFORM_RESOURCE_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Library/DramCarveoutLib.h>

/**
 * @ingroup TEGRA_UART_TYPE
 * @name Tegra UART TYPE
 * These are the UARTs available on the Tegra platform
 */
#define TEGRA_UART_TYPE_16550     0x00
#define TEGRA_UART_TYPE_SBSA      0x01
#define TEGRA_UART_TYPE_TCU       0xFE
#define TEGRA_UART_TYPE_NONE      0xFF

#define BIT(x)   (1 << (x))

typedef enum {
  TegrablBootInvalid,
  TegrablBootColdBoot,
  TegrablBootRcm,
  TegrablBootTypeMax,
} TEGRA_BOOT_TYPE;

typedef struct {
  NVDA_MEMORY_REGION   *CarveoutRegions;
  UINTN                CarveoutRegionsCount;
  UINTN                SdramSize;
  UINTN                DtbLoadAddress;
} TEGRA_RESOURCE_INFO;

typedef struct {
  EFI_PHYSICAL_ADDRESS Base;
  UINTN                Size;
} TEGRA_MMIO_INFO;

/**
  Set Tegra UART Base Address

  @param[in]    UART base address

**/
VOID
EFIAPI
SetTegraUARTBaseAddress (
  IN EFI_PHYSICAL_ADDRESS   UartBaseAddress
);

/**
  Retrieve Tegra UART Base Address

**/
EFI_PHYSICAL_ADDRESS
EFIAPI
GetTegraUARTBaseAddress (
  VOID
);

/**
  Retrieve the type and address of UART based on the instance Number

**/
EFI_STATUS
EFIAPI
GetUARTInstanceInfo (
  OUT UINT32                *UARTInstanceType,
  OUT EFI_PHYSICAL_ADDRESS  *UARTInstanceAddress
);

/**
  Retrieve CPU BL Address

**/
UINTN
EFIAPI
GetCPUBLBaseAddress (
  VOID
);

/**
  Retrieve DTB Address

**/
UINT64
EFIAPI
GetDTBBaseAddress (
  VOID
);

/**
  Retrieve RCM Blob Address

**/
UINT64
EFIAPI
GetRCMBaseAddress (
  VOID
);

/**
  Retrieve Boot Type

**/
TEGRA_BOOT_TYPE
EFIAPI
GetBootType (
  VOID
);

/**
  Retrieve Resource Config

**/
EFI_STATUS
EFIAPI
GetResourceConfig (
  OUT TEGRA_RESOURCE_INFO *PlatformInfo
);

/**
  Retrieve GR Blob Address

**/
UINT64
EFIAPI
GetGRBlobBaseAddress (
  VOID
);

/**
  Retrieve GR Output Base and Size

**/
BOOLEAN
EFIAPI
GetGROutputBaseAndSize (
  OUT UINTN *Base,
  OUT UINTN *Size
);

/**
  Retrieve FSI NS Base and Size

**/
BOOLEAN
EFIAPI
GetFsiNsBaseAndSize (
  OUT UINTN *Base,
  OUT UINTN *Size
);

/**
  Retrieve MMIO Base and Size

**/
TEGRA_MMIO_INFO*
EFIAPI
GetMmioBaseAndSize (
  VOID
);

/**
  Retrieve CVM EEPROM Data

**/
UINT32
EFIAPI
GetCvmEepromData (
  OUT UINT8 **Data
);

#endif //__PLATFORM_RESOURCE_LIB_H__
