/** @file

Copyright (c) 2022, NVIDIA Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef STANDLONEMM_OPTEE_DEVICE_MEM_H
#define STANDLONEMM_OPTEE_DEVICE_MEM_H

#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Protocol/NorFlash.h>
#include <Protocol/QspiController.h>

#define DEVICE_REGION_NAME_MAX_LEN  32
#define MAX_DEVICE_REGIONS          10
#define OPTEE_OS_UID0               0x384fb3e0
#define OPTEE_OS_UID1               0xe7f811e3
#define OPTEE_OS_UID2               0xaf630002
#define OPTEE_OS_UID3               0xa5d5c51b

typedef struct _EFI_MM_DEVICE_REGION {
  EFI_VIRTUAL_ADDRESS    DeviceRegionStart;
  UINT32                 DeviceRegionSize;
  CHAR8                  DeviceRegionName[DEVICE_REGION_NAME_MAX_LEN];
} EFI_MM_DEVICE_REGION;

typedef struct {
  PHYSICAL_ADDRESS    NsBufferAddr;
  UINTN               NsBufferSize;
  PHYSICAL_ADDRESS    SecBufferAddr;
  UINTN               SecBufferSize;
  PHYSICAL_ADDRESS    DTBAddress;
  PHYSICAL_ADDRESS    CpuBlParamsAddr;
  UINTN               CpuBlParamsSize;
  PHYSICAL_ADDRESS    RasMmBufferAddr;
  UINTN               RasMmBufferSize;
  PHYSICAL_ADDRESS    SatMcMmBufferAddr;
  UINTN               SatMcMmBufferSize;
  BOOLEAN             Fbc;
} STMM_COMM_BUFFERS;

EFIAPI
EFI_STATUS
GetDeviceRegion (
  IN CHAR8                 *Name,
  OUT EFI_VIRTUAL_ADDRESS  *DeviceBase,
  OUT UINTN                *DeviceRegionSize
  );

EFIAPI
BOOLEAN
IsOpteePresent (
  VOID
  );

EFIAPI
BOOLEAN
IsQspi0Present (
  UINT32  *NumQspi
  );

EFIAPI
EFI_STATUS
GetQspi0DeviceRegions (
  EFI_MM_DEVICE_REGION  **QspiRegions,
  UINT32                *NumRegions
  );

EFIAPI
TEGRA_PLATFORM_TYPE
GetPlatformTypeMm (
  VOID
  );

EFIAPI
TEGRA_BOOT_TYPE
GetBootType (
  VOID
  );

EFIAPI
BOOLEAN
InFbc (
  VOID
  );

EFIAPI
EFI_STATUS
GetVarStoreCs (
  UINT8  *VarCs
  );

EFIAPI
EFI_STATUS
GetCpuBlParamsAddrStMm (
  EFI_PHYSICAL_ADDRESS  *CpuBlAddr
  );

/**
 * GetDeviceSocketNum
 * Util function to get the socket number from the device region name.
 *
 * @param[in] DeviceRegionName Name of the device region.
 *
 * @retval Socket number.
 */
EFIAPI
UINT32
GetDeviceSocketNum (
  CONST CHAR8  *DeviceRegionName
  );

/**
 * GetProtocolHandleBuffer
 * Util function to get the socket number from the device region name.
 * Ideally this should be part of the MMST , just as BS provides this service.
 *
 * @param[in]  Guid          Protocol GUID to search.
 * @param[out] NumberHandles Number of handles installed.
 * @param[out] Buffer        Buffer of the handles installed.
 *
 * @retval Socket number.
 */
EFIAPI
EFI_STATUS
GetProtocolHandleBuffer (
  IN  EFI_GUID    *Guid,
  OUT UINTN       *NumberHandles,
  OUT EFI_HANDLE  **Buffer
  );

/**
 * Locate the Protocol interface installed on the socket.
 *
 * @param[in] SocketNum  Socket Number for which the protocol is requested.
 *
 * @retval    EFI_SUCCESS      On Success.
 *            OTHER            On Failure to lookup if the protocol is installed.
 *                             Or if there wasn't any socketId protocol installed.
 **/
EFIAPI
EFI_STATUS
FindProtocolInSocket (
  IN  UINT32    SocketNum,
  IN  EFI_GUID  *ProtocolGuid,
  OUT VOID      **ProtocolInterface
  );

/**
 * Get the NorFlashProtocol for a given socket.
 *
 * @param[in] SocketNum  Socket Number for which the NOR Flash protocol
 *                       is requested.
 * @retval    NorFlashProtocol    On Success.
 *            NULL                On Failure.
 **/
EFIAPI
NVIDIA_NOR_FLASH_PROTOCOL *
GetSocketNorFlashProtocol (
  UINT32  SocketNum
  );

/**
 * Get the QSPI Protocol for a given socket.
 *
 * @param[in] SocketNum  Socket Number for which the NOR Flash protocol
 *                       is requested.
 * @retval    QspiControllerProtocol    On Success.
 *            NULL                      On Failure.
 **/
EFIAPI
NVIDIA_QSPI_CONTROLLER_PROTOCOL  *
GetSocketQspiProtocol (
  UINT32  SocketNum
  );

#endif //STANDALONEMM_OPTEE_DEVICE_MEM_H
