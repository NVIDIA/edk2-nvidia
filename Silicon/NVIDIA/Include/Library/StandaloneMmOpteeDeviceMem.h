/** @file

SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef STANDLONEMM_OPTEE_DEVICE_MEM_H
#define STANDLONEMM_OPTEE_DEVICE_MEM_H

#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Protocol/NorFlash.h>
#include <Protocol/QspiController.h>
#include <Protocol/SequentialRecord.h>
#include <Library/Arm/ArmFfaMemMgmt.h>

#define DEVICE_REGION_NAME_MAX_LEN      32
#define MAX_DEVICE_REGIONS              10
#define OPTEE_OS_UID0                   0x384fb3e0
#define OPTEE_OS_UID1                   0xe7f811e3
#define OPTEE_OS_UID2                   0xaf630002
#define OPTEE_OS_UID3                   0xa5d5c51b
#define VERSION_STR_MAX                 0x100
#define RASFW_VMID                      0x8003
#define STMM_VMID                       0x8002
#define SATMC_VMID                      0x8001
#define RAS_FW_MM_RESET_REQ             0xC0270006
#define ARM_SVC_ID_FFA_SUCCESS_AARCH64  0xC4000061
// Setup UUID in accordance with the FF-A ABIs. (Byte order swapped from usual UUID format)
#define OPTEE_UID01                     0xe311f8e7e0786148
#define OPTEE_UID23                     0x1bc5d5a502005ebc
#define FFA_PARTITION_INFO_GET_REGS_64  0xC400008B
#define FFA_ID_GET                      0x84000069
#define FFA_SHARE_MEM_REQ_64            0xC4000073
#define FFA_SHARE_MEM_REQ_32            0x84000073
#define FFA_SUCCESS_AARCH64             0xC4000061
#define FFA_SUCCESS_AARCH32             0x84000061
#define FFA_ERROR                       0x84000060
#define ARM_FID_FFA_FEATURES            0x84000064

#define OPTEE_SIGNER_TA_UUID0  0xed32d53399e64209
#define OPTEE_SIGNER_TA_UUID1  0x9cc02d72cdd998a7
#define OPTEE_FFA_SERVICE_ID   0x6
#define OPTEE_FFA_SIGN_FID     0x1

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
  PHYSICAL_ADDRESS    NsPrm0BufferAddr;
  UINTN               NsPrm0BufferSize;
  PHYSICAL_ADDRESS    FfaTxBufferAddr;
  UINTN               FfaTxBufferSize;
  PHYSICAL_ADDRESS    FfaRxBufferAddr;
  UINTN               FfaRxBufferSize;
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
 * Get the Shared Memory Mailbox size/address of a given SP.
 *
 * @param[in]  SpId           SP Id used in FF-A messages.
 * @param[out] MboxStartAddr  Mailbox start address.
 * @param[out] MboxSize       Mailbox size.

 * @retval    EFI_SUCCESS      Found the Address/size of the Maibox for the SP.
 *            EFI_UNSUPPORTED  SP Id isn't known or the Hob having these
 *                             addresses isn't found.
 **/
EFIAPI
EFI_STATUS
GetMboxAddrSize (
  UINT16  SpId,
  UINT64  *MboxStartAddr,
  UINT32  *MboxSize
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
 * Get active boot chain.
 *
 * @param[out]  BootChain       Address to return boot chain
 *
 * @retval EFI_SUCCESS          Active boot chain returned
 * @retval Others               An error occurred
 *
 **/
EFI_STATUS
EFIAPI
StmmGetActiveBootChain (
  UINT32  *BootChain
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

/**
 * CorruptFvHeader.
 * Utility function to corrupt the UEFI Variable store by corrupting
 * the FV header forcing a re-build of the variable store during the next
 * boot.
 *
 * @params[in]   FvPartitionOffset Byte Offset of the Var Store Partition.
 * @params[in]   PartitionSize     Size of the Partition.
 *
 * @retval       EFI_SUCCESS      Succesfully corrupted the FV Header.
 *               Other            Failure to get the partition Info or while
 *                                transacting with the device.
 **/
EFI_STATUS
EFIAPI
CorruptFvHeader (
  UINT64  FvPartitionOffset,
  UINT64  PartitionSize
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
  VOID                           *PartitionData;
};

/*
 * Send an FF-A msg to RASFW to do an L2 Reset. Only supported on Hafnium
 * deployments.
 *
 * @retval EFI_SUCCESS   Succesfully reset (odds are you won't process this).
 *         Other         RASFW wasn't able to process the message.
 *
 **/
EFI_STATUS
EFIAPI
MmCommSendResetReq (
  VOID
  );

/**
 * Get the FFA TX/RX buffer addresses and sizes. This API only applies to Hafnium deployments.
 *
 * @param[out] FfaTxBufferAddr  FFA TX buffer address.
 * @param[out] FfaTxBufferSize  FFA TX buffer size.
 * @param[out] FfaRxBufferAddr  FFA RX buffer address.
 * @param[out] FfaRxBufferSize  FFA RX buffer size.
 */
EFI_STATUS
EFIAPI
FfaGetTxRxBuffer (
  UINT64  *FfaTxBufferAddr,
  UINT32  *FfaTxBufferSize,
  UINT64  *FfaRxBufferAddr,
  UINT32  *FfaRxBufferSize
  );

/*
 * GetOpteeVmId
 * Get the Optee VM ID from the SPMC.
 *
 * @param[out] OpteeVmId  Optee VM ID.
 *
 */
EFI_STATUS
EFIAPI
FfaGetOpteeVmId (
  OUT UINT16  *OpteeVmId
  );

/*
* GetMmVmId
* Get the MM VM ID from the SPMC.
*
* @param[out] MmVmId  MM VM ID.
*
*/
EFI_STATUS
EFIAPI
FfaGetMmVmId (
  OUT UINT16  *MmVmId
  );

/*
* PrepareFfaMemoryDescriptor
* Prepare the FfaMemoryDescriptor for the measurement.
*
* @param[in] Meas  Measurement buffer to be signed.
* @param[in] Size  Size of the measurement.
*
* @result EFI_SUCCESS Succesfully prepared the FfaMemoryDescriptor.
*/
EFI_STATUS
EFIAPI
PrepareFfaMemoryDescriptor (
  IN   UINT64  FfaTxBufferAddr,
  IN   UINT64  FfaTxBufferSize,
  IN   UINT8   *MeasurementBuffer,
  IN   UINT32  MeasurementBufferSize,
  IN   UINT16  SenderId,
  IN   UINT16  ReceiverId,
  OUT  UINT32  *TotalLength
  );

/*
* SendFfaShareCommand
* Send the FFA_SHARE_MEM_REQ_64/32 command.
*
* @param[in] TotalLength      Total length of the message.
* @param[in] FragmentLength   Fragment length of the message.
* @param[in] BufferAddr       Buffer address to share.
* @param[in] PageCount        Page count of the buffer.
* @param[out] Handle         Handle of the shared memory.
*/
EFI_STATUS
EFIAPI
FfaSendShareCommand (
  IN UINT32   TotalLength,
  IN UINT32   FragmentLength,
  IN UINT64   BufferAddr,
  IN UINT32   PageCount,
  OUT UINT64  *Handle
  );

#endif //STANDALONEMM_OPTEE_DEVICE_MEM_H
