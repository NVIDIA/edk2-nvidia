/** @file
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __PLATFORM_RESOURCE_LIB_H__
#define __PLATFORM_RESOURCE_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Library/DramCarveoutLib.h>
#include <NVIDIABoardConfiguration.h>

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

#define MAX_EEPROM_DATA_SIZE      256

typedef enum {
  TegrablBootInvalid,
  TegrablBootColdBoot,
  TegrablBootRcm,
  TegrablBootTypeMax,
} TEGRA_BOOT_TYPE;

typedef enum {
  TegraRcmCarveout,
  TegraCarveoutMax,
} TEGRA_CARVEOUT_TYPE;

typedef struct {
  NVDA_MEMORY_REGION   *DramRegions;
  UINTN                DramRegionsCount;
  UINTN                UefiDramRegionsCount;
  NVDA_MEMORY_REGION   *CarveoutRegions;
  UINTN                CarveoutRegionsCount;
  UINTN                DtbLoadAddress;
} TEGRA_RESOURCE_INFO;

typedef struct {
  EFI_PHYSICAL_ADDRESS Base;
  UINTN                Size;
} TEGRA_MMIO_INFO;

typedef struct {
  CHAR8               *Name;
  UINT32              Offset;
  UINT32              Value;
} TEGRA_FUSE_INFO;

typedef struct {
  UINTN           FuseBaseAddr;
  TEGRA_FUSE_INFO *FuseList;
  UINTN           FuseCount;
  CHAR8           CvmBoardId[BOARD_ID_LEN + 1];
  CHAR8           CvbBoardId[BOARD_ID_LEN + 1];
  CHAR8           CvmProductId[PRODUCT_ID_LEN + 1];
  CHAR8           CvbProductId[PRODUCT_ID_LEN + 1];
  CHAR8           SerialNumber[SERIAL_NUM_LEN];
} TEGRA_BOARD_INFO;

typedef struct  {
  UINT8  CvmEepromData[MAX_EEPROM_DATA_SIZE];
  UINT8  CvbEepromData[MAX_EEPROM_DATA_SIZE];
  UINT32 CvmEepromDataSize;
  UINT32 CvbEepromDataSize;
} TEGRABL_EEPROM_DATA;

typedef struct {
  CHAR8              *GicCompatString;
  CHAR8              *ItsCompatString;
  UINT32              Version;
} TEGRA_GIC_INFO;

typedef struct {
  UINT32 NumSockets;
  UINT32 ActiveBootChain;
} TEGRA_PLATFORM_RESOURCE_INFO;

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
  Retrieve chip specific info for GIC

**/
BOOLEAN
EFIAPI
GetGicInfo (
  OUT TEGRA_GIC_INFO *GicInfo
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
  Retrieve Carveout Info

**/
EFI_STATUS
EFIAPI
GetCarveoutInfo (
  IN TEGRA_CARVEOUT_TYPE Type,
  IN UINTN               *Base,
  IN UINT32              *Size
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
  Retrieve EEPROM Data

**/
TEGRABL_EEPROM_DATA*
EFIAPI
GetEepromData (
  VOID
);

/**
  Retrieve Board Information

**/
EFI_STATUS
EFIAPI
GetBoardInfo (
  OUT TEGRA_BOARD_INFO *BoardInfo
);

/**
  Validate Active Boot Chain

**/
EFI_STATUS
EFIAPI
ValidateActiveBootChain (
  VOID
);

/**
  Get Ramloaded OS Base and Size

**/
BOOLEAN
EFIAPI
GetRamdiskOSBaseAndSize (
  OUT UINTN *Base,
  OUT UINTN *Size
);

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
GetPlatformResourceInformation (
  IN TEGRA_PLATFORM_RESOURCE_INFO *PlatformResourceInfo
);

#endif //__PLATFORM_RESOURCE_LIB_H__
