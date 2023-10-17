/** @file

SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef STANDLONEMM_OPTEE_DEVICE_MEM_H
#define STANDLONEMM_OPTEE_DEVICE_MEM_H

#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Protocol/NorFlash.h>
#include <Protocol/QspiController.h>
#include <Protocol/SequentialRecord.h>

#define DEVICE_REGION_NAME_MAX_LEN  32
#define MAX_DEVICE_REGIONS          10
#define OPTEE_OS_UID0               0x384fb3e0
#define OPTEE_OS_UID1               0xe7f811e3
#define OPTEE_OS_UID2               0xaf630002
#define OPTEE_OS_UID3               0xa5d5c51b
#define VERSION_STR_MAX             0x100
#define RASFW_VMID                  0x8003
#define SATMC_VMID                  0x8001

#define ADDRESS_IN_RANGE(addr, min, max)  (((addr) > (min)) && ((addr) < (max)))

typedef struct _NVIDIA_VAR_INT_PROTOCOL NVIDIA_VAR_INT_PROTOCOL;

typedef struct _EFI_MM_DEVICE_REGION {
  EFI_VIRTUAL_ADDRESS    DeviceRegionStart;
  UINT32                 DeviceRegionSize;
  CHAR8                  DeviceRegionName[DEVICE_REGION_NAME_MAX_LEN];
} EFI_MM_DEVICE_REGION;

typedef struct {
  PHYSICAL_ADDRESS    NsBufferAddr;
  UINTN               NsBufferSize;
  PHYSICAL_ADDRESS    NsErstUncachedBufAddr;
  UINTN               NsErstUncachedBufSize;
  PHYSICAL_ADDRESS    NsErstCachedBufAddr;
  UINTN               NsErstCachedBufSize;
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
IsDeviceTypePresent (
  CONST CHAR8  *DeviceType,
  UINT32       *NumRegions   OPTIONAL
  );

EFIAPI
EFI_STATUS
GetDeviceTypeRegions (
  CONST CHAR8           *DeviceType,
  EFI_MM_DEVICE_REGION  **DeviceRegions,
  UINT32                *NumRegions
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

/**
 * GetPartitionData for a given Partition Index by looking up the CPUBL Params.
 *
 * @params[in]   PartitionIndex  Index into CPU BL's partition Info structure.
 * @params[out]  Partitioninfo   Data structure containing offset and size.
 *
 * @retval       EFI_SUCCESS     Successfully looked up partition info.
 *               OTHER           From the StandaloneMmOpteeLib (trying to get
 *                               CPU BL params) or PlatformResourceLib trying
 *                               to look up partition info in the CPU BL
 *                               Params).
 **/
EFI_STATUS
GetPartitionData (
  IN  UINT32          PartitionIndex,
  OUT PARTITION_INFO  *PartitionInfo
  );

/**
 * Check if a Buffer address is in the Mailbox of a given SP.
 *
 * @param[in] Buf   Buffer Address to check.
 * @param[in] SpId  SP Id used in FF-A messages.

 * @retval    TRUE    Buffer is in Range of the Mailbox.
 *            FALSE   Buffer is not in the Mailbox.
 **/
EFIAPI
BOOLEAN
IsBufInSecSpMbox (
  IN UINTN   Buf,
  IN UINT16  SpId
  );

/**
 * Check if system is T234
 *
 * @retval    TRUE    System is T234
 *            FALSE   System is not T234
 **/
BOOLEAN
EFIAPI
IsT234 (
  VOID
  );

/**
 * Get boot chain value to use for GPT location.  If system does not
 * support per-boot-chain GPT, 0 is returned.
 *
 * @retval UINT32     Boot chain value to use for GPT location
 *
 **/
UINT32
EFIAPI
StmmGetBootChainForGpt (
  VOID
  );

typedef
EFI_STATUS
(EFIAPI *VAR_INT_COMPUTE_MEASUREMENT)(
  IN  NVIDIA_VAR_INT_PROTOCOL     *This,
  IN  CHAR16                      *VariableName,
  IN  EFI_GUID                    *VendorGuid,
  IN  UINT32                      Attributes,
  IN  VOID                        *Data,
  IN  UINTN                       Size
  );

typedef
EFI_STATUS
(EFIAPI *VAR_INT_FUNCTION)(
  IN NVIDIA_VAR_INT_PROTOCOL    *This
  );

typedef
EFI_STATUS
(EFIAPI *VAR_INVALIDATE_FUNCTION)(
  IN  NVIDIA_VAR_INT_PROTOCOL   *This,
  IN  CHAR16                    *VariableName,
  IN  EFI_GUID                  *VendorGuid,
  IN  EFI_STATUS                PreviousResult
  );

struct _NVIDIA_VAR_INT_PROTOCOL {
  VAR_INT_COMPUTE_MEASUREMENT    ComputeNewMeasurement;
  VAR_INT_FUNCTION               WriteNewMeasurement;
  VAR_INVALIDATE_FUNCTION        InvalidateLast;
  VAR_INT_FUNCTION               Validate;
  UINT64                         PartitionByteOffset;
  UINT64                         PartitionSize;
  NVIDIA_NOR_FLASH_PROTOCOL      *NorFlashProtocol;
  UINT64                         BlockSize;
  UINT8                          *CurMeasurement;
  UINT32                         MeasurementSize;
};

#endif //STANDALONEMM_OPTEE_DEVICE_MEM_H
