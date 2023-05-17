/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#define TEGRA_UART_TYPE_16550  0x00
#define TEGRA_UART_TYPE_SBSA   0x01
#define TEGRA_UART_TYPE_TCU    0xFE
#define TEGRA_UART_TYPE_NONE   0xFF

#define BIT(x)  (1 << (x))

#define MAX_EEPROM_DATA_SIZE          256
#define TEGRABL_VARIABLE_IMAGE_INDEX  (25U)
#define TEGRABL_FTW_IMAGE_INDEX       (26U)
#define TEGRABL_RAS_ERROR_LOGS        (24U)
#define TEGRABL_EARLY_BOOT_VARS       (16U)
#define TEGRABL_CMET                  (17U)
#define TEGRABL_ERST                  (29U)
#define DEVICE_CS_MASK                (0xFF00)
#define DEVICE_CS_SHIFT               (8)
#define MAX_SUPPORTED_CORES           1024U

typedef enum {
  TegrablBootInvalid,
  TegrablBootColdBoot,
  TegrablBootRcm,
  TegrablBootTypeMax,
} TEGRA_BOOT_TYPE;

typedef struct {
  NVDA_MEMORY_REGION    *DramRegions;
  UINTN                 DramRegionsCount;
  UINTN                 UefiDramRegionIndex;
  NVDA_MEMORY_REGION    *InputCarveoutRegions;
  NVDA_MEMORY_REGION    *CarveoutRegions;
  UINTN                 CarveoutRegionsCount;
  NVDA_MEMORY_REGION    *UsableCarveoutRegions;
  UINTN                 UsableCarveoutRegionsCount;
  UINTN                 DtbLoadAddress;
  NVDA_MEMORY_REGION    RamOopsRegion;
} TEGRA_RESOURCE_INFO;

typedef struct {
  EFI_PHYSICAL_ADDRESS    Base;
  UINTN                   Size;
} TEGRA_MMIO_INFO;

typedef struct {
  CHAR8     *Name;
  UINT32    Offset;
  UINT32    Value;
} TEGRA_FUSE_INFO;

typedef struct {
  UINT64    SerialNumber;
  UINT16    TotalWidth;
  UINT16    DataWidth;
  UINT8     ManufacturerId;
  UINT8     Rank;
  UINT64    Size;
  UINT32    SpeedKhz;
} TEGRA_DRAM_DEVICE_INFO;

typedef struct {
  UINTN              FuseBaseAddr;
  TEGRA_FUSE_INFO    *FuseList;
  UINTN              FuseCount;
  CHAR8              CvmProductId[TEGRA_PRODUCT_ID_LEN + 1];
  CHAR8              CvbProductId[TEGRA_PRODUCT_ID_LEN + 1];
  CHAR8              SerialNumber[TEGRA_SERIAL_NUM_LEN];
} TEGRA_BOARD_INFO;

#pragma pack(1)
typedef struct  {
  UINT8     CvmEepromData[MAX_EEPROM_DATA_SIZE];
  UINT8     CvbEepromData[MAX_EEPROM_DATA_SIZE];
  UINT32    CvmEepromDataSize;
  UINT32    CvbEepromDataSize;
} TEGRABL_EEPROM_DATA;
#pragma pack()

typedef struct {
  CHAR8     *GicCompatString;
  CHAR8     *ItsCompatString;
  UINT32    Version;
} TEGRA_GIC_INFO;

typedef struct {
  UINTN    Base;
  UINTN    Size;
} TEGRA_BASE_AND_SIZE_INFO;

typedef struct {
  UINT32                      SocketMask;
  UINT32                      MaxPossibleSockets;
  UINT32                      MaxPossibleClusters;
  UINT32                      MaxPossibleCoresPerCluster;
  UINT32                      MaxPossibleCores;
  UINT64                      EnabledCoresBitMap[ALIGN_VALUE (MAX_SUPPORTED_CORES, 64) / 64];
  BOOLEAN                     AffinityMpIdrSupported;
  UINT32                      NumberOfEnabledCores;
  UINT32                      ActiveBootChain;
  BOOLEAN                     BrBctUpdateFlag;
  TEGRA_RESOURCE_INFO         *ResourceInfo;
  TEGRA_MMIO_INFO             *MmioInfo;
  TEGRABL_EEPROM_DATA         *EepromData;
  TEGRA_BOARD_INFO            *BoardInfo;
  TEGRA_BASE_AND_SIZE_INFO    GrOutputInfo;
  TEGRA_BASE_AND_SIZE_INFO    FsiNsInfo;
  TEGRA_BASE_AND_SIZE_INFO    RamdiskOSInfo;
  TEGRA_BASE_AND_SIZE_INFO    RcmBlobInfo;
  TEGRA_BASE_AND_SIZE_INFO    PvaFwInfo;
  TEGRA_BASE_AND_SIZE_INFO    FrameBufferInfo;
  TEGRA_BASE_AND_SIZE_INFO    ProfilerInfo;
  TEGRA_BASE_AND_SIZE_INFO    CpublCoInfo;
  TEGRA_BOOT_TYPE             BootType;
  BOOLEAN                     HypervisorMode;
  TEGRA_BASE_AND_SIZE_INFO    *EgmMemoryInfo;
  UINT64                      PhysicalDramSize;
  TEGRA_DRAM_DEVICE_INFO      *DramDeviceInfo;
} TEGRA_PLATFORM_RESOURCE_INFO;

/**
  Retrieve CPU BL Address

**/
UINTN
EFIAPI
GetCPUBLBaseAddress (
  VOID
  );

/**
  Set Tegra UART Base Address

  @param[in]    UART base address

**/
VOID
EFIAPI
SetTegraUARTBaseAddress (
  IN EFI_PHYSICAL_ADDRESS  UartBaseAddress
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
  OUT TEGRA_GIC_INFO  *GicInfo
  );

/**
  Retrieve Dram Page Blacklist Info Address

**/
NVDA_MEMORY_REGION *
EFIAPI
GetDramPageBlacklistInfoAddress (
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
  Retrieve GR Blob Address

**/
UINT64
EFIAPI
GetGRBlobBaseAddress (
  VOID
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
  Update boot chain scratch register to boot given boot chain on next reset

  @param[in]  BootChain             Boot chain

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
SetNextBootChain (
  IN  UINT32  BootChain
  );

/**
  Get Platform Resource Information
  Does not update the CPU info structures.

**/
EFI_STATUS
EFIAPI
GetPlatformResourceInformation (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  );

/**
  Update CPU info in Platform Resource Information

**/
EFI_STATUS
EFIAPI
UpdatePlatformResourceCpuInformation (
  VOID
  );

/**
  Get Rootfs Status Register Value

**/
EFI_STATUS
EFIAPI
GetRootfsStatusReg (
  IN UINT32  *RegisterValue
  );

/**
  Set Rootfs Status Register

**/
EFI_STATUS
EFIAPI
SetRootfsStatusReg (
  IN UINT32  RegisterValue
  );

/**
  Get Platform Resource Information in StMM image

**/
EFI_STATUS
EFIAPI
GetPlatformResourceInformationStandaloneMm (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo,
  IN PHYSICAL_ADDRESS              CpuBootloaderAddress
  );

/**
 * Get Partition Info in Dxe.
 *
 * @param[in] PartitionIndex        Index into the Partition info array, usually
 *                                  defined by the early BLs..
 * @param[out] DeviceInstance       Value that conveys the device/CS for the
 *                                  partition..
 * @param[out] PartitionStartByte   Start byte offset for the partition..
 * @param[out] PartitionSizeBytes   Size of the partition in bytes.
 *
 * @retval  EFI_SUCCESS             Success in looking up partition.
 * @retval  EFI_INVALID_PARAMETER   Invalid partition Index.
**/
EFI_STATUS
EFIAPI
GetPartitionInfo (
  IN  UINT32  PartitionIndex,
  OUT UINT16  *DeviceInstance,
  OUT UINT64  *PartitionStartByte,
  OUT UINT64  *PartitionSizeBytes
  );

/**
 * Get Partition Info in Standalone MM image.
 *
 * @param[in] CpuBlAddress          Address of the CPU BL params.
 * @param[in] PartitionIndex        Index into the Partition info array, usually
 *                                  defined by the early BLs..
 * @param[out] DeviceInstance       Value that conveys the device/CS for the
 *                                  partition..
 * @param[out] PartitionStartByte   Start byte offset for the partition..
 * @param[out] PartitionSizeBytes   Size of the partition in bytes.
 *
 * @retval  EFI_SUCCESS             Success in looking up partition.
 * @retval  EFI_INVALID_PARAMETER   Invalid partition Index.
**/
EFI_STATUS
EFIAPI
GetPartitionInfoStMm (
  IN  UINTN   CpuBlAddress,
  IN  UINT32  PartitionIndex,
  OUT UINT16  *DeviceInstance,
  OUT UINT64  *PartitionStartByte,
  OUT UINT64  *PartitionSizeBytes
  );

/**
 * Get the sockets Enabled Bit Mask.
 *
 * @param[in] CpuBlAddress          Address of the CPU BL params.
 *
 * @retval  Bitmask of enabled sockets (0x1 if CPUBL is 0).
**/
UINT32
EFIAPI
GetSocketMaskStMm (
  IN UINTN  CpuBlAddress
  );

/**
 * Check if socket is enabled in the CPU BL Params's socket mask.
 * This API is usually only called from StMM.
 *
 * @param[in] CpuBlAddress          Address of the CPU BL params.
 * @param[in] SocketNum             Socket to check..
 *
 * @retval  TRUE                    Socket is enabled.
 * @retval  FALSE                   Socket is not enabled.
**/
BOOLEAN
EFIAPI
IsSocketEnabledStMm (
  IN UINTN   CpuBlAddress,
  IN UINT32  SocketNum
  );

/**
 * Check if TPM is requested to be enabled.
 *
 * @retval  TRUE      TPM is enabled.
 * @retval  FALSE     TPM is disabled.
**/
BOOLEAN
EFIAPI
IsTpmToBeEnabled (
  VOID
  );

#endif //__PLATFORM_RESOURCE_LIB_H__
