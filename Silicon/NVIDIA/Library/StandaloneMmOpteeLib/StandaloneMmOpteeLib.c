/** @file
Misc Library for OPTEE related functions in Standalone MM.

SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Library/DebugLib.h"
#include <Base.h>
#include <PiMm.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Library/MmServicesTableLib.h>

#include <Library/IoLib.h>
#include <Library/HobLib.h>
#include <Library/ArmSvcLib.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <Library/BaseLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#define HIDREV_OFFSET             0x4
#define HIDREV_PRE_SI_PLAT_SHIFT  0x14
#define HIDREV_PRE_SI_PLAT_MASK   0xf
#define UEFI_VARS_SOCKET          0

EFIAPI
BOOLEAN
IsOpteePresent (
  VOID
  )
{
  return (FeaturePcdGet (PcdOpteePresent));
}

EFIAPI
EFI_STATUS
GetDeviceRegion (
  IN CHAR8                 *Name,
  OUT EFI_VIRTUAL_ADDRESS  *DeviceBase,
  OUT UINTN                *DeviceRegionSize
  )
{
  EFI_STATUS            Status           = EFI_NOT_FOUND;
  EFI_MM_DEVICE_REGION  *DeviceRegionMap = NULL;
  UINTN                 Index;
  EFI_HOB_GUID_TYPE     *GuidHob;

  GuidHob = GetFirstGuidHob (&gEfiStandaloneMmDeviceMemoryRegions);
  NV_ASSERT_RETURN (
    GuidHob != NULL,
    return Status,
    "%a: Unable to find HOB for gEfiStandaloneMmDeviceMemoryRegions\n",
    __FUNCTION__
    );

  DeviceRegionMap = GET_GUID_HOB_DATA (GuidHob);
  for (Index = 0; Index < MAX_DEVICE_REGIONS; Index++) {
    if (AsciiStrCmp (Name, DeviceRegionMap[Index].DeviceRegionName) == 0) {
      *DeviceBase       = DeviceRegionMap[Index].DeviceRegionStart;
      *DeviceRegionSize = DeviceRegionMap[Index].DeviceRegionSize;
      Status            = EFI_SUCCESS;
      break;
    }
  }

  return Status;
}

EFIAPI
BOOLEAN
IsDeviceTypePresent (
  CONST CHAR8  *DeviceType,
  UINT32       *NumRegions   OPTIONAL
  )
{
  EFI_MM_DEVICE_REGION  *DeviceRegionMap = NULL;
  UINTN                 Index;
  EFI_HOB_GUID_TYPE     *GuidHob;
  BOOLEAN               DeviceTypePresent = FALSE;
  UINT32                NumDevices;

  GuidHob = GetFirstGuidHob (&gEfiStandaloneMmDeviceMemoryRegions);
  NV_ASSERT_RETURN (
    GuidHob != NULL,
    return DeviceTypePresent,
    "%a: Unable to find HOB for gEfiStandaloneMmDeviceMemoryRegions\n",
    __FUNCTION__
    );

  DeviceRegionMap = GET_GUID_HOB_DATA (GuidHob);
  NumDevices      = 0;
  for (Index = 0; Index < MAX_DEVICE_REGIONS; Index++) {
    if (AsciiStrStr (DeviceRegionMap[Index].DeviceRegionName, DeviceType) != NULL) {
      DeviceTypePresent = TRUE;
      NumDevices++;
    }
  }

  if (NumRegions != NULL) {
    *NumRegions = NumDevices;
  }

  return DeviceTypePresent;
}

EFIAPI
BOOLEAN
IsQspi0Present (
  UINT32  *NumRegions
  )
{
  return IsDeviceTypePresent ("qspi0", NumRegions);
}

/**
  * GetDeviceTypeRegions
  * Get all the MMIO regions for a device type across all the sockets.
  *
  * @param[in]   DeviceType    Device type substring.
  * @param[out]  DeviceRegions Allocated region of device regions on success.
  * @param[out]  NumRegions    Number of device regions.
  *
  * @retval  EFI_SUCCESS           Found device type regions.
  *          EFI_NOT_FOUND         Device Memory HOB not found or no device
  *                                regions installed in the HOB.
  *          EFI_OUT_OF_RESOURCES  Failed to allocate the DeviceRegions Buffer.
 **/
EFIAPI
EFI_STATUS
GetDeviceTypeRegions (
  CONST CHAR8           *DeviceType,
  EFI_MM_DEVICE_REGION  **DeviceRegions,
  UINT32                *NumRegions
  )
{
  EFI_MM_DEVICE_REGION  *DeviceMmio;
  UINT32                NumDevices;
  EFI_STATUS            Status;
  UINTN                 Index;
  EFI_HOB_GUID_TYPE     *GuidHob;
  EFI_MM_DEVICE_REGION  *DeviceRegionMap = NULL;
  UINTN                 DeviceIndex;

  Status = EFI_SUCCESS;
  if (IsDeviceTypePresent (DeviceType, &NumDevices) == FALSE) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: There are no %a regions present\n",
      __FUNCTION__,
      DeviceType
      ));
    Status = EFI_NOT_FOUND;
    goto ExitGetDeviceTypeRegions;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: %u %a regions present\n",
    __FUNCTION__,
    NumDevices,
    DeviceType
    ));
  DeviceMmio = AllocateRuntimeZeroPool (sizeof (EFI_MM_DEVICE_REGION) * NumDevices);
  if (DeviceMmio == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to allocate %u bytes\n",
      __FUNCTION__,
      (NumDevices * sizeof (EFI_MM_DEVICE_REGION))
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitGetDeviceTypeRegions;
  }

  GuidHob = GetFirstGuidHob (&gEfiStandaloneMmDeviceMemoryRegions);
  if (GuidHob == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to lookup Device Memory Hob",
      __FUNCTION__
      ));
    Status = EFI_NOT_FOUND;
    FreePool (DeviceMmio);
    goto ExitGetDeviceTypeRegions;
  }

  DeviceRegionMap = GET_GUID_HOB_DATA (GuidHob);
  DeviceIndex     = 0;
  for (DeviceIndex = 0, Index = 0;
       (Index < MAX_DEVICE_REGIONS) && (DeviceIndex < NumDevices);
       Index++)
  {
    if (AsciiStrStr (DeviceRegionMap[Index].DeviceRegionName, DeviceType) != NULL) {
      CopyMem (
        &DeviceMmio[DeviceIndex++],
        &DeviceRegionMap[Index],
        sizeof (EFI_MM_DEVICE_REGION)
        );
    }
  }

  *DeviceRegions = DeviceMmio;
  *NumRegions    = NumDevices;
ExitGetDeviceTypeRegions:
  return Status;
}

EFIAPI
EFI_STATUS
GetQspi0DeviceRegions (
  EFI_MM_DEVICE_REGION  **QspiRegions,
  UINT32                *NumRegions
  )
{
  return GetDeviceTypeRegions ("qspi0", QspiRegions, NumRegions);
}

EFIAPI
TEGRA_PLATFORM_TYPE
GetPlatformTypeMm (
  VOID
  )
{
  TEGRA_PLATFORM_TYPE  PlatformType;
  UINT64               MiscAddress;
  UINTN                MiscRegionSize;
  EFI_STATUS           Status;
  UINT32               HidRev;

  Status = GetDeviceRegion ("tegra-misc", &MiscAddress, &MiscRegionSize);
  if (EFI_ERROR (Status)) {
    PlatformType = TEGRA_PLATFORM_UNKNOWN;
  } else {
    HidRev       = MmioRead32 (MiscAddress + HIDREV_OFFSET);
    PlatformType = ((HidRev >> HIDREV_PRE_SI_PLAT_SHIFT) & HIDREV_PRE_SI_PLAT_MASK);
    if (PlatformType >= TEGRA_PLATFORM_UNKNOWN) {
      PlatformType =  TEGRA_PLATFORM_UNKNOWN;
    }
  }

  return PlatformType;
}

EFIAPI
BOOLEAN
InFbc (
  VOID
  )
{
  EFI_HOB_GUID_TYPE  *GuidHob;
  STMM_COMM_BUFFERS  *StmmCommBuffers;
  BOOLEAN            Fbc;

  GuidHob = GetFirstGuidHob (&gNVIDIAStMMBuffersGuid);
  if (GuidHob == NULL) {
    if (IsOpteePresent ()) {
      Fbc = TRUE;
      goto ExitInFbc;
    } else {
      ASSERT_EFI_ERROR (EFI_NOT_FOUND);
    }
  }

  StmmCommBuffers = (STMM_COMM_BUFFERS *)GET_GUID_HOB_DATA (GuidHob);
  Fbc             = StmmCommBuffers->Fbc;
ExitInFbc:
  return Fbc;
}

EFIAPI
TEGRA_BOOT_TYPE
GetBootType (
  VOID
  )
{
  EFI_HOB_GUID_TYPE             *GuidHob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;
  TEGRA_BOOT_TYPE               BootType;

  GuidHob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if (GuidHob == NULL) {
    if (IsOpteePresent ()) {
      BootType = TegrablBootInvalid;
      goto ExitBootType;
    } else {
      ASSERT_EFI_ERROR (EFI_NOT_FOUND);
    }
  }

  PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (GuidHob);
  BootType             = PlatformResourceInfo->BootType;
ExitBootType:
  return BootType;
}

/**
 * Get the CPU BL Params Address.
 *
 * @param[out] CpuBlAddr   Address for the CPU Bootloader Params..
 *
 * @retval  EFI_SUCCESS    Succesfully looked up the CS  value.
 *          EFI_NOT_FOUND  Couldn't find the GUID'd HOB that contains
 *                         the STMM Comm Buffers.
**/
EFIAPI
EFI_STATUS
GetCpuBlParamsAddrStMm (
  EFI_PHYSICAL_ADDRESS  *CpuBlAddr
  )
{
  EFI_HOB_GUID_TYPE  *GuidHob;
  STMM_COMM_BUFFERS  *StmmCommBuffers;

  GuidHob = GetFirstGuidHob (&gNVIDIAStMMBuffersGuid);
  NV_ASSERT_RETURN (
    GuidHob != NULL,
    return EFI_NOT_FOUND,
    "%a: Unable to find HOB for gNVIDIAStMMBuffersGuid\n",
    __FUNCTION__
    );

  StmmCommBuffers = (STMM_COMM_BUFFERS *)GET_GUID_HOB_DATA (GuidHob);
  *CpuBlAddr      = StmmCommBuffers->CpuBlParamsAddr;
  return EFI_SUCCESS;
}

/**
 * Look up the CS to be used for the Variable partition in the CPUBL Params.
 * This function is used when the Variable partition is not found in the GPT.
 *
 * @param[in, out] VarCs  Chipselect for the Variable partition.
 *
 * @retval  EFI_SUCCESS    Succesfully looked up the CS  value.
 *          EFI_NOT_FOUND  Couldn't lookup the CPUBL Params OR
 *                         the partition info for the Variable partition
 *                         isn't valid.
**/
STATIC
EFI_STATUS
GetVarStoreCsBlParams (
  IN OUT UINT8  *VarCs
  )
{
  EFI_STATUS            Status;
  UINT64                VarOffset;
  UINT64                VarSize;
  UINT16                DeviceInstance;
  EFI_PHYSICAL_ADDRESS  CpuBlAddr;
  TEGRA_PLATFORM_TYPE   Platform;

  Status = GetCpuBlParamsAddrStMm (&CpuBlAddr);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get CPUBL Addr %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitGetVarStoreCsBlParams;
  }

  Status = GetPartitionInfoStMm (
             (UINTN)CpuBlAddr,
             TEGRABL_VARIABLE_IMAGE_INDEX,
             &DeviceInstance,
             &VarOffset,
             &VarSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Variable partition Info %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitGetVarStoreCsBlParams;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a:PartitionIndex[%u] VarOffset %lu VarSize %lu"
    "Device Instance %x\n",
    __FUNCTION__,
    TEGRABL_VARIABLE_IMAGE_INDEX,
    VarOffset,
    VarSize,
    DeviceInstance
    ));
  if (VarSize != 0) {
    *VarCs = ((DeviceInstance & DEVICE_CS_MASK) >> DEVICE_CS_SHIFT);
  } else {
    Platform = GetPlatformTypeMm ();
    /* Unable to get the CS information from CPU BL Params */
    if (Platform == TEGRA_PLATFORM_SILICON) {
      *VarCs = NOR_FLASH_CHIP_SELECT_TH500_SIL;
    } else {
      *VarCs = NOR_FLASH_CHIP_SELECT_TH500_PRESIL;
    }
  }

ExitGetVarStoreCsBlParams:
  return Status;
}

/**
 * Look up the CS to be used for the Variable partition.
 *
 * @param[out] VarCs  Chipselect for the Variable partition.
 *
 * @retval  EFI_SUCCESS    Succesfully looked up the CS  value.
 *          EFI_NOT_FOUND  Couldn't lookup the CPUBL Params OR
 *                         the partition info for the Variable partition
 *                         isn't valid.
**/
EFIAPI
EFI_STATUS
GetVarStoreCs (
  UINT8  *VarCs
  )
{
  EFI_STATUS  Status;
  UINTN       ChipId;

  if (IsOpteePresent ()) {
    /* For Jetson we always use CS 0 */
    *VarCs = NOR_FLASH_CHIP_SELECT_JETSON;
    Status = EFI_SUCCESS;
  } else {
    ChipId = TegraGetChipID ();

    /* Branch here for non-OPTEE based platforms
       As the GetChipID() is not available for OPTEE based platforms.
       For Non-OPTEE based platforms, Jetson SOCs will be using CS 0.
       For other platforms, we will use the CPUBL Params to get the CS value.
    */
    switch (ChipId) {
      case T264_CHIP_ID:
        *VarCs = NOR_FLASH_CHIP_SELECT_JETSON;
        Status = EFI_SUCCESS;
        break;
      default:
        Status = GetVarStoreCsBlParams (VarCs);
        break;
    }
  }

  return Status;
}

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
  )
{
  CHAR8   *SockStr;
  UINT32  SockNum;

  SockStr = AsciiStrStr (DeviceRegionName, "-socket");
  if (SockStr != NULL) {
    SockNum = AsciiStrDecimalToUintn ((SockStr + AsciiStrLen ("-socket")));
    if (SockNum >= MAX_SOCKETS) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: SockNum %u is out of range , max(%d)\n",
        __FUNCTION__,
        SockNum,
        MAX_SOCKETS
        ));
    }
  } else {
    SockNum = 0;
  }

  return SockNum;
}

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
  )
{
  EFI_STATUS  Status;
  UINTN       BufferSize;

  if ((NumberHandles == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  BufferSize     = 0;
  *NumberHandles = 0;
  *Buffer        = NULL;
  Status         = gMmst->MmLocateHandle (
                            ByProtocol,
                            Guid,
                            NULL,
                            &BufferSize,
                            *Buffer
                            );
  if (EFI_ERROR (Status) && (Status != EFI_BUFFER_TOO_SMALL)) {
    return EFI_NOT_FOUND;
  }

  *Buffer = AllocatePool (BufferSize);
  if (*Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gMmst->MmLocateHandle (
                    ByProtocol,
                    Guid,
                    NULL,
                    &BufferSize,
                    *Buffer
                    );

  *NumberHandles = BufferSize / sizeof (EFI_HANDLE);
  if (EFI_ERROR (Status)) {
    *NumberHandles = 0;
    FreePool (*Buffer);
    *Buffer = NULL;
  }

  return Status;
}

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
  )
{
  EFI_HANDLE  *HandleBuffer = NULL;
  UINTN       HandleCount;
  UINTN       Index;
  EFI_STATUS  Status;
  UINT32      *Socket;
  BOOLEAN     SocketMatchFound;

  /* Locate all the handles for the passed in protocol Guid. */
  Status = GetProtocolHandleBuffer (
             ProtocolGuid,
             &HandleCount,
             &HandleBuffer
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to find protocol Guid (%r)\r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitFindProtocolInSocket;
  }

  SocketMatchFound = FALSE;

  /* Find the Socket Id interface for each handle and match it to the passed
   * in socket number.
   */
  for (Index = 0; Index < HandleCount; Index++) {
    Socket = NULL;
    Status = gMmst->MmHandleProtocol (
                      HandleBuffer[Index],
                      &gNVIDIASocketIdProtocolGuid,
                      (VOID **)&Socket
                      );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to find SocketId installed on %p %r\n",
        __FUNCTION__,
        HandleBuffer[Index],
        Status
        ));
      continue;
    }

    if (SocketNum == *Socket) {
      SocketMatchFound = TRUE;
      break;
    }
  }

  if (SocketMatchFound == TRUE) {
    Status = gMmst->MmHandleProtocol (
                      HandleBuffer[Index],
                      ProtocolGuid,
                      ProtocolInterface
                      );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to find Protocol installed on %p %r\n",
        __FUNCTION__,
        HandleBuffer[Index],
        Status
        ));
    }
  }

ExitFindProtocolInSocket:
  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
    HandleBuffer = NULL;
  }

  return Status;
}

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
  )
{
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol;
  EFI_STATUS                 Status;

  NorFlashProtocol = NULL;
  Status           = FindProtocolInSocket (
                       SocketNum,
                       &gNVIDIANorFlashProtocolGuid,
                       (VOID **)&NorFlashProtocol
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to get NorFlash on Socket %u %r\n",
      __FUNCTION__,
      SocketNum,
      Status
      ));
  }

  return NorFlashProtocol;
}

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
  )
{
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *QspiControllerProtocol;
  EFI_STATUS                       Status;

  QspiControllerProtocol = NULL;
  Status                 = FindProtocolInSocket (
                             SocketNum,
                             &gNVIDIAQspiControllerProtocolGuid,
                             (VOID **)&QspiControllerProtocol
                             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to get NorFlashProto on Socket %u %r\n",
      __FUNCTION__,
      SocketNum,
      Status
      ));
  }

  return QspiControllerProtocol;
}

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
  )
{
  EFI_PHYSICAL_ADDRESS  CpuBlParamsAddr;
  EFI_STATUS            Status;
  UINT16                DeviceInstance;
  UINT64                PartitionByteOffset;
  UINT64                PartitionSize;

  Status = GetCpuBlParamsAddrStMm (&CpuBlParamsAddr);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get CpuBl Addr %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitGetPartitionData;
  }

  Status = GetPartitionInfoStMm (
             (UINTN)CpuBlParamsAddr,
             PartitionIndex,
             &DeviceInstance,
             &PartitionByteOffset,
             &PartitionSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to get %u PartitionInfo %r\n",
      __FUNCTION__,
      PartitionIndex,
      Status
      ));

    goto ExitGetPartitionData;
  }

  PartitionInfo->PartitionByteOffset = PartitionByteOffset;
  PartitionInfo->PartitionSize       = PartitionSize;
  PartitionInfo->PartitionIndex      = PartitionIndex;

  DEBUG ((
    DEBUG_ERROR,
    "%a: PartitionInfo Start 0x%lu Size %lu Idx %u\n",
    __FUNCTION__,
    PartitionInfo->PartitionByteOffset,
    PartitionInfo->PartitionSize,
    PartitionInfo->PartitionIndex
    ));
ExitGetPartitionData:
  return Status;
}

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
  )
{
  EFI_HOB_GUID_TYPE  *GuidHob;
  STMM_COMM_BUFFERS  *StmmCommBuffers;
  EFI_STATUS         Status;

  Status  = EFI_UNSUPPORTED;
  GuidHob = GetFirstGuidHob (&gNVIDIAStMMBuffersGuid);
  NV_ASSERT_RETURN (
    GuidHob != NULL,
    goto GetMboxAddrSize,
    "Failed to find Buffers GUID HOB"
    );

  StmmCommBuffers = (STMM_COMM_BUFFERS *)GET_GUID_HOB_DATA (GuidHob);
  if (SpId == RASFW_VMID) {
    *MboxStartAddr = StmmCommBuffers->RasMmBufferAddr;
    *MboxSize      = StmmCommBuffers->RasMmBufferSize;
    Status         = EFI_SUCCESS;
  } else if (SpId == SATMC_VMID) {
    *MboxStartAddr = StmmCommBuffers->SatMcMmBufferAddr;
    *MboxSize      = StmmCommBuffers->SatMcMmBufferSize;
    Status         = EFI_SUCCESS;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Unsupported VM %u\n", __FUNCTION__, SpId));
    goto GetMboxAddrSize;
  }

GetMboxAddrSize:
  return Status;
}

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
  UINTN   Buf,
  UINT16  SpId
  )
{
  BOOLEAN     IsBufInSpRange;
  UINT64      SecBufStart;
  UINT32      SecBufRange;
  UINT64      SecBufEnd;
  EFI_STATUS  Status;

  IsBufInSpRange = FALSE;

  Status = GetMboxAddrSize (SpId, &SecBufStart, &SecBufRange);
  NV_ASSERT_RETURN (
    !EFI_ERROR (Status),
    goto ExitIsBufInSecSpMbox,
    "%a: Failed to find Buffers for SP %u %r",
    __FUNCTION__,
    SpId,
    Status
    );
  SecBufEnd = SecBufStart + SecBufRange;

  if (ADDRESS_IN_RANGE (Buf, SecBufStart, SecBufEnd)) {
    IsBufInSpRange = TRUE;
  }

  DEBUG ((DEBUG_INFO, "%a:%d %u\n", __FUNCTION__, __LINE__, IsBufInSpRange));
ExitIsBufInSecSpMbox:
  return IsBufInSpRange;
}

BOOLEAN
EFIAPI
IsT234 (
  VOID
  )
{
  return (IsOpteePresent () && IsDeviceTypePresent ("-t234", NULL));
}

EFI_STATUS
EFIAPI
StmmGetActiveBootChain (
  UINT32  *BootChain
  )
{
  EFI_MM_DEVICE_REGION  *ScratchRegions;
  UINT32                NumRegions;
  EFI_STATUS            Status;

  if (IsT234 ()) {
    Status = GetDeviceTypeRegions ("scratch-t234", &ScratchRegions, &NumRegions);
    NV_ASSERT_RETURN ((!EFI_ERROR (Status) && NumRegions == 1), return EFI_DEVICE_ERROR, "%a: failed to get scratch region: %r\n", __FUNCTION__, Status);

    Status = GetActiveBootChainStMm (ScratchRegions[0].DeviceRegionStart, BootChain);
    ASSERT_EFI_ERROR (Status);

    return Status;
  }

  return EFI_UNSUPPORTED;
}

UINT32
EFIAPI
StmmGetBootChainForGpt (
  VOID
  )
{
  UINT32      BootChain;
  EFI_STATUS  Status;

  Status = StmmGetActiveBootChain (&BootChain);
  if (EFI_ERROR (Status)) {
    return 0;
  }

  return BootChain;
}

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
  )
{
  UINT32                      FvHeaderLength;
  UINT64                      FvHeaderOffset;
  NVIDIA_NOR_FLASH_PROTOCOL   *NorFlashProtocol;
  EFI_STATUS                  Status;
  EFI_FIRMWARE_VOLUME_HEADER  FvHeaderData;

  NorFlashProtocol = GetSocketNorFlashProtocol (UEFI_VARS_SOCKET);
  if (NorFlashProtocol == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get NorFlashProtocol for Socket 0\n",
      __FUNCTION__
      ));
    Status = EFI_UNSUPPORTED;
    goto ExitCorruptFvHeader;
  }

  FvHeaderOffset = FvPartitionOffset;
  FvHeaderLength = sizeof (EFI_FIRMWARE_VOLUME_HEADER);
  Status         = NorFlashProtocol->Read (
                                       NorFlashProtocol,
                                       FvHeaderOffset,
                                       FvHeaderLength,
                                       &FvHeaderData
                                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Read FV header %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitCorruptFvHeader;
  }

  /* Corrupt the signature/revision .*/
  FvHeaderData.Signature = FvHeaderData.Revision = 0;
  Status                 = NorFlashProtocol->Write (
                                               NorFlashProtocol,
                                               FvHeaderOffset,
                                               FvHeaderData.HeaderLength,
                                               &FvHeaderData
                                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to Write Partition header\r\n", __FUNCTION__));
    goto ExitCorruptFvHeader;
  }

ExitCorruptFvHeader:
  return Status;
}

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
  )
{
  EFI_STATUS         Status;
  EFI_HOB_GUID_TYPE  *GuidHob;
  STMM_COMM_BUFFERS  *StmmCommBuffers;

  Status = EFI_UNSUPPORTED;

  if (IsOpteePresent ()) {
    goto ExitGetFfaTxRxBuffer;
  }

  GuidHob = GetFirstGuidHob (&gNVIDIAStMMBuffersGuid);
  NV_ASSERT_RETURN (
    GuidHob != NULL,
    goto ExitGetFfaTxRxBuffer,
    "Failed to find Buffers GUID HOB"
    );

  StmmCommBuffers  = (STMM_COMM_BUFFERS *)GET_GUID_HOB_DATA (GuidHob);
  *FfaTxBufferAddr = StmmCommBuffers->FfaTxBufferAddr;
  *FfaTxBufferSize = StmmCommBuffers->FfaTxBufferSize;
  *FfaRxBufferAddr = StmmCommBuffers->FfaRxBufferAddr;
  *FfaRxBufferSize = StmmCommBuffers->FfaRxBufferSize;
  Status           = EFI_SUCCESS;

ExitGetFfaTxRxBuffer:
  return Status;
}

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
  )
{
  ARM_SVC_ARGS  SvcArgs;
  EFI_STATUS    Status;

  ZeroMem (&SvcArgs, sizeof (SvcArgs));

  SvcArgs.Arg0 = FFA_PARTITION_INFO_GET_REGS_64;
  SvcArgs.Arg1 = OPTEE_UID01;
  SvcArgs.Arg2 = OPTEE_UID23;
  SvcArgs.Arg3 = 0;

  ArmCallSvc (&SvcArgs);

  if ((SvcArgs.Arg0 != FFA_SUCCESS_AARCH64) && (SvcArgs.Arg0 != FFA_SUCCESS_AARCH32)) {
    Status = EFI_UNSUPPORTED;
    DEBUG ((DEBUG_ERROR, "FFA_PARTITION_INFO_GET_REGS_64 failed Arg0 0x%lx\n", SvcArgs.Arg0));
    goto ExitGetOpteeVmId;
  }

  *OpteeVmId = (UINT16)(SvcArgs.Arg3 & 0xFFFF);
  DEBUG ((DEBUG_INFO, "Got Optee VM ID: 0x%x\n", *OpteeVmId));
  DEBUG ((DEBUG_INFO, "SvcArgs.Arg0 0x%lx Arg1 0x%lx Arg2 0x%lx Arg3 0x%lx\n", SvcArgs.Arg0, SvcArgs.Arg1, SvcArgs.Arg2, SvcArgs.Arg3));
  DEBUG ((DEBUG_INFO, "SvcArgs.Arg4 0x%lx Arg5 0x%lx Arg6 0x%lx Arg7 0x%lx\n", SvcArgs.Arg4, SvcArgs.Arg5, SvcArgs.Arg6, SvcArgs.Arg7));
  Status = EFI_SUCCESS;

ExitGetOpteeVmId:
  return Status;
}

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
  )
{
  ARM_SVC_ARGS  SvcArgs;
  EFI_STATUS    Status;

  ZeroMem (&SvcArgs, sizeof (SvcArgs));

  SvcArgs.Arg0 = FFA_ID_GET;

  ArmCallSvc (&SvcArgs);

  if ((SvcArgs.Arg0 != FFA_SUCCESS_AARCH64) && (SvcArgs.Arg0 != FFA_SUCCESS_AARCH32)) {
    Status = EFI_UNSUPPORTED;
    DEBUG ((DEBUG_ERROR, "FFA_ID_GET failed Arg0 0x%lx\n", SvcArgs.Arg0));
    goto ExitGetMmVmId;
  }

  *MmVmId = (UINT16)(SvcArgs.Arg2 & 0xFFFF);
  DEBUG ((DEBUG_INFO, "Got MM VM ID: 0x%x\n", *MmVmId));
  DEBUG ((DEBUG_INFO, "SvcArgs.Arg0 0x%lx Arg1 0x%lx Arg2 0x%lx Arg3 0x%lx\n", SvcArgs.Arg0, SvcArgs.Arg1, SvcArgs.Arg2, SvcArgs.Arg3));
  DEBUG ((DEBUG_INFO, "SvcArgs.Arg4 0x%lx Arg5 0x%lx Arg6 0x%lx Arg7 0x%lx\n", SvcArgs.Arg4, SvcArgs.Arg5, SvcArgs.Arg6, SvcArgs.Arg7));
  Status = EFI_SUCCESS;

ExitGetMmVmId:
  return Status;
}

/*
 * DumpFfaMemoryDescriptor
 * Dump the FfaMemoryDescriptor. Debug only.
 *
 * @param[in] FfaMemoryTransactionDescriptor  FfaMemoryTransactionDescriptor.
 * @param[in] FfaTxBufferAddr                 FfaTxBufferAddr.
 */
STATIC
VOID
DumpFfaMemoryDescriptor (
  IN FFA_MEMORY_TRANSACTION_DESCRIPTOR  *FfaMemoryTransactionDescriptor,
  IN UINT64                             FfaTxBufferAddr
  )
{
  DEBUG_CODE_BEGIN ();
  FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR  *FfaEndpointMemoryAccessDescriptor;
  FFA_COMPOSITE_MEMORY_REGION            *FfaCompositeMemoryRegion;
  FFA_MEMORY_REGION_CONSTITUENT          *FfaConstituentMemoryRegion;

  DEBUG ((DEBUG_ERROR, "FfaMemoryTransactionDescriptor->SenderId: 0x%x\n", FfaMemoryTransactionDescriptor->SenderId));
  DEBUG ((DEBUG_ERROR, "FfaMemoryTransactionDescriptor->Attributes.Reserved: 0x%x\n", FfaMemoryTransactionDescriptor->Attributes.Reserved));
  DEBUG ((DEBUG_ERROR, "FfaMemoryTransactionDescriptor->Attributes.Shareability: 0x%x\n", FfaMemoryTransactionDescriptor->Attributes.Shareability));
  DEBUG ((DEBUG_ERROR, "FfaMemoryTransactionDescriptor->Attributes.Cacheability: 0x%x\n", FfaMemoryTransactionDescriptor->Attributes.Cacheability));
  DEBUG ((DEBUG_ERROR, "FfaMemoryTransactionDescriptor->Attributes.Security: 0x%x\n", FfaMemoryTransactionDescriptor->Attributes.Security));
  DEBUG ((DEBUG_ERROR, "FfaMemoryTransactionDescriptor->Attributes.Type: 0x%x\n", FfaMemoryTransactionDescriptor->Attributes.Type));
  DEBUG ((DEBUG_ERROR, "FfaMemoryTransactionDescriptor->Flags: 0x%x\n", FfaMemoryTransactionDescriptor->Flags));
  DEBUG ((DEBUG_ERROR, "FfaMemoryTransactionDescriptor->Handle: 0x%lx\n", FfaMemoryTransactionDescriptor->Handle));
  DEBUG ((DEBUG_ERROR, "FfaMemoryTransactionDescriptor->Tag: 0x%lx\n", FfaMemoryTransactionDescriptor->Tag));
  DEBUG ((DEBUG_ERROR, "FfaMemoryTransactionDescriptor->ReceiverCount: 0x%x\n", FfaMemoryTransactionDescriptor->ReceiverCount));
  DEBUG ((
    DEBUG_ERROR,
    "FfaMemoryTransactionDescriptor->ReceiversOffset: 0x%lx(FfaTxBuffer %p size %x)\n",
    FfaMemoryTransactionDescriptor->ReceiversOffset,
    FfaTxBufferAddr,
    sizeof (FFA_MEMORY_TRANSACTION_DESCRIPTOR)
    ));
  DEBUG ((DEBUG_ERROR, "FfaMemoryTransactionDescriptor->MemoryAccessDescSize: 0x%x\n", FfaMemoryTransactionDescriptor->MemoryAccessDescSize));

  FfaEndpointMemoryAccessDescriptor = (FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR *)(FfaTxBufferAddr + FfaMemoryTransactionDescriptor->ReceiversOffset);
  DEBUG ((DEBUG_ERROR, "FfaEndpointMemoryAccessDescriptor: 0x%lx\n", FfaEndpointMemoryAccessDescriptor));
  DEBUG ((DEBUG_ERROR, "FfaEndpointMemoryAccessDescriptor->CompositeMemoryRegionOffset: 0x%x\n", FfaEndpointMemoryAccessDescriptor->CompositeMemoryRegionOffset));
  DEBUG ((DEBUG_ERROR, "FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.ReceiverId: 0x%x\n", FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.ReceiverId));
  DEBUG ((DEBUG_ERROR, "FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.Permissions.DataAccess: 0x%x\n", FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.Permissions.DataAccess));
  DEBUG ((DEBUG_ERROR, "FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.Permissions.InstructionAccess: 0x%x\n", FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.Permissions.InstructionAccess));
  DEBUG ((DEBUG_ERROR, "FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.Permissions.Reservd: 0x%x\n", FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.Permissions.Reservd));
  DEBUG ((DEBUG_ERROR, "FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.Flags: 0x%x\n", FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.Flags));

  FfaCompositeMemoryRegion = (FFA_COMPOSITE_MEMORY_REGION *)(FfaTxBufferAddr + FfaEndpointMemoryAccessDescriptor->CompositeMemoryRegionOffset);
  DEBUG ((DEBUG_ERROR, "FfaCompositeMemoryRegion: 0x%lx\n", FfaCompositeMemoryRegion));
  DEBUG ((DEBUG_ERROR, "FfaCompositeMemoryRegion->TotalPageCount: 0x%x\n", FfaCompositeMemoryRegion->TotalPageCount));
  DEBUG ((DEBUG_ERROR, "FfaCompositeMemoryRegion->ConstituentCount: 0x%x\n", FfaCompositeMemoryRegion->ConstituentCount));
  DEBUG ((DEBUG_ERROR, "FfaCompositeMemoryRegion->Reserved: 0x%lx\n", FfaCompositeMemoryRegion->Reserved));
  DEBUG ((DEBUG_ERROR, "FfaCompositeMemoryRegion->Constituents: 0x%lx\n", FfaCompositeMemoryRegion->Constituents));

  FfaConstituentMemoryRegion = (FFA_MEMORY_REGION_CONSTITUENT *)(FfaCompositeMemoryRegion->Constituents);
  DEBUG ((DEBUG_ERROR, "FfaConstituentMemoryRegion: 0x%lx\n", FfaConstituentMemoryRegion));
  DEBUG ((DEBUG_ERROR, "FfaConstituentMemoryRegion->Address: 0x%lx\n", FfaConstituentMemoryRegion->Address));
  DEBUG ((DEBUG_ERROR, "FfaConstituentMemoryRegion->PageCount: 0x%x\n", FfaConstituentMemoryRegion->PageCount));
  DEBUG ((DEBUG_ERROR, "FfaConstituentMemoryRegion->Reserved: 0x%x\n", FfaConstituentMemoryRegion->Reserved));
  DEBUG_CODE_END ();
}

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
  )
{
  EFI_STATUS                         Status;
  FFA_MEMORY_TRANSACTION_DESCRIPTOR  *FfaMemoryTransactionDescriptor;

 #if FixedPcdGetBool (PcdFfaMinorV2Supported)
  FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR  *FfaEndpointMemoryAccessDescriptor;
 #else
  FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR_V1_1  *FfaEndpointMemoryAccessDescriptorV1_1;
 #endif
  FFA_COMPOSITE_MEMORY_REGION    *FfaCompositeMemoryRegion;
  FFA_MEMORY_REGION_CONSTITUENT  *FfaConstituentMemoryRegion;

  *TotalLength = 0;

  FfaMemoryTransactionDescriptor = (FFA_MEMORY_TRANSACTION_DESCRIPTOR *)FfaTxBufferAddr;
  ZeroMem (FfaMemoryTransactionDescriptor, sizeof (FFA_MEMORY_TRANSACTION_DESCRIPTOR));

  FfaMemoryTransactionDescriptor->SenderId            = SenderId;
  FfaMemoryTransactionDescriptor->Attributes.Reserved = 0;

  FfaMemoryTransactionDescriptor->Attributes.Shareability = FFA_MEMORY_INNER_SHAREABLE;
  FfaMemoryTransactionDescriptor->Attributes.Cacheability = FFA_MEMORY_CACHE_WRITE_BACK;
  FfaMemoryTransactionDescriptor->Attributes.Security     = FFA_MEMORY_SECURITY_SECURE;
  FfaMemoryTransactionDescriptor->Attributes.Type         = FFA_MEMORY_NORMAL_MEM;

  FfaMemoryTransactionDescriptor->Flags  = 0;
  FfaMemoryTransactionDescriptor->Handle = 0;
  FfaMemoryTransactionDescriptor->Tag    = 0;  // Zero this for now, we could use this as a transaction id.

  FfaMemoryTransactionDescriptor->ReceiverCount   = 1;
  FfaMemoryTransactionDescriptor->ReceiversOffset = sizeof (FFA_MEMORY_TRANSACTION_DESCRIPTOR);

  *TotalLength += sizeof (FFA_MEMORY_TRANSACTION_DESCRIPTOR);
  /* Add this section incase Hafnium won't accept the FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR_V1_1.*/
 #if FixedPcdGetBool (PcdFfaMinorV2Supported)
  FfaMemoryTransactionDescriptor->MemoryAccessDescSize = sizeof (FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR);
  FfaEndpointMemoryAccessDescriptor                    = (FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR *)(FfaTxBufferAddr + FfaMemoryTransactionDescriptor->ReceiversOffset);
  ZeroMem (FfaEndpointMemoryAccessDescriptor, sizeof (FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR));

  FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.ReceiverId                    = ReceiverId;
  FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.Permissions.DataAccess        = FFA_DATA_ACCESS_RW;
  FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.Permissions.InstructionAccess = FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED;
  FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.Permissions.Reservd           = 0;
  FfaEndpointMemoryAccessDescriptor->ReceiverPermissions.Flags                         = 0;

  *TotalLength += sizeof (FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR);

  FfaEndpointMemoryAccessDescriptor->CompositeMemoryRegionOffset = sizeof (FFA_MEMORY_TRANSACTION_DESCRIPTOR) + sizeof (FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR);
  *TotalLength                                                  += sizeof (FFA_COMPOSITE_MEMORY_REGION);
  FfaCompositeMemoryRegion                                       = (FFA_COMPOSITE_MEMORY_REGION *)(FfaTxBufferAddr + FfaEndpointMemoryAccessDescriptor->CompositeMemoryRegionOffset);
 #else
  FfaMemoryTransactionDescriptor->MemoryAccessDescSize = sizeof (FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR_V1_1);
  FfaEndpointMemoryAccessDescriptorV1_1                = (FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR_V1_1 *)(FfaTxBufferAddr + FfaMemoryTransactionDescriptor->ReceiversOffset);
  ZeroMem (FfaEndpointMemoryAccessDescriptorV1_1, sizeof (FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR_V1_1));

  FfaEndpointMemoryAccessDescriptorV1_1->ReceiverPermissions.ReceiverId                    = ReceiverId;
  FfaEndpointMemoryAccessDescriptorV1_1->ReceiverPermissions.Permissions.DataAccess        = FFA_DATA_ACCESS_RW;
  FfaEndpointMemoryAccessDescriptorV1_1->ReceiverPermissions.Permissions.InstructionAccess = FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED;
  FfaEndpointMemoryAccessDescriptorV1_1->ReceiverPermissions.Permissions.Reservd           = 0;
  FfaEndpointMemoryAccessDescriptorV1_1->ReceiverPermissions.Flags                         = 0;
  FfaEndpointMemoryAccessDescriptorV1_1->CompositeMemoryRegionOffset                       = sizeof (FFA_MEMORY_TRANSACTION_DESCRIPTOR) + sizeof (FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR_V1_1);
  *TotalLength                                                                            += sizeof (FFA_ENDPOINT_MEMORY_ACCESS_DESCRIPTOR_V1_1);
  FfaCompositeMemoryRegion                                                                 = (FFA_COMPOSITE_MEMORY_REGION *)(FfaTxBufferAddr + FfaEndpointMemoryAccessDescriptorV1_1->CompositeMemoryRegionOffset);
 #endif

  ZeroMem (FfaCompositeMemoryRegion, sizeof (FFA_COMPOSITE_MEMORY_REGION));
  *TotalLength += sizeof (FFA_COMPOSITE_MEMORY_REGION);

  FfaCompositeMemoryRegion->TotalPageCount   = EFI_SIZE_TO_PAGES (MeasurementBufferSize);
  FfaCompositeMemoryRegion->ConstituentCount = 1;

  FfaConstituentMemoryRegion = (FFA_MEMORY_REGION_CONSTITUENT *)(FfaCompositeMemoryRegion->Constituents);
  ZeroMem (FfaConstituentMemoryRegion, sizeof (FFA_MEMORY_REGION_CONSTITUENT));

  FfaConstituentMemoryRegion->Address   = (UINT64)MeasurementBuffer;
  FfaConstituentMemoryRegion->PageCount = EFI_SIZE_TO_PAGES (MeasurementBufferSize);
  FfaConstituentMemoryRegion->Reserved  = 0;

  *TotalLength += sizeof (FFA_MEMORY_REGION_CONSTITUENT);
  Status        = EFI_SUCCESS;
  DumpFfaMemoryDescriptor (FfaMemoryTransactionDescriptor, FfaTxBufferAddr);

  return Status;
}

/*
 * IsFfaMemShareSupported
 * Check if the FFA_SHARE_MEM_REQ_64/32 is supported.
 *
 * @param[out] FfaMemShareSupportedId  FFA_SHARE_MEM_REQ_64/32 supported ID.
 *
 * @return  TRUE  FFA_SHARE_MEM_REQ_64/32 is supported.
 *          FALSE FFA_SHARE_MEM_REQ_64/32 is not supported.
 */
BOOLEAN
EFIAPI
IsFfaMemShareSupported (
  OUT UINTN  *FfaMemShareSupportedId
  )
{
  ARM_SVC_ARGS  SvcArgs;
  BOOLEAN       Supported = FALSE;

  ZeroMem (&SvcArgs, sizeof (SvcArgs));
  SvcArgs.Arg0 = ARM_FID_FFA_FEATURES;
  SvcArgs.Arg1 = FFA_SHARE_MEM_REQ_64;
  SvcArgs.Arg2 = 0;

  ArmCallSvc (&SvcArgs);

  if ((SvcArgs.Arg0 == FFA_ERROR)) {
    DEBUG ((DEBUG_INFO, "FFA_SHARE_MEM_REQ_64 not supported 0x%lx\n", SvcArgs.Arg0));
  } else {
    Supported               = TRUE;
    *FfaMemShareSupportedId = FFA_SHARE_MEM_REQ_64;
    goto ExitIsFfaMemShareSupported;
  }

  ZeroMem (&SvcArgs, sizeof (SvcArgs));
  SvcArgs.Arg0 = ARM_FID_FFA_FEATURES;
  SvcArgs.Arg1 = FFA_SHARE_MEM_REQ_32;
  SvcArgs.Arg2 = 0;

  ArmCallSvc (&SvcArgs);

  if ((SvcArgs.Arg0 == FFA_ERROR)) {
    DEBUG ((DEBUG_ERROR, "FFA_SHARE_MEM_REQ_32/64 not supported 0x%lx\n", SvcArgs.Arg0));
  } else {
    Supported               = TRUE;
    *FfaMemShareSupportedId = FFA_SHARE_MEM_REQ_32;
    goto ExitIsFfaMemShareSupported;
  }

ExitIsFfaMemShareSupported:
  return Supported;
}

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
  )
{
  EFI_STATUS    Status;
  ARM_SVC_ARGS  SvcArgs;
  UINTN         FfaMemShareSupportedId;

  /* Check which FFA_SHARE_MEM_REQ is supported, 64 or 32 */
  if (IsFfaMemShareSupported (&FfaMemShareSupportedId) == FALSE) {
    DEBUG ((DEBUG_ERROR, "FFA_SHARE_MEM_REQ_64/32 not supported\n"));
    *Handle = 0U;
    Status  = EFI_UNSUPPORTED;
    goto ExitSendFfaShareCommand;
  }

  if ((FfaMemShareSupportedId == FFA_SHARE_MEM_REQ_32) && (BufferAddr > MAX_UINT32)) {
    DEBUG ((DEBUG_ERROR, " WARNING: BufferAddr 0x%lx is greater than MAX_UINT32\n", BufferAddr));
  }

  DEBUG ((DEBUG_INFO, "FFA_SHARE_MEM_REQ_64/32 supported ID 0x%x\n", FfaMemShareSupportedId));

  ZeroMem (&SvcArgs, sizeof (SvcArgs));

  SvcArgs.Arg0 = FfaMemShareSupportedId;
  SvcArgs.Arg1 = TotalLength;
  SvcArgs.Arg2 = FragmentLength;
  SvcArgs.Arg3 = 0;
  SvcArgs.Arg4 = 0;

  DEBUG ((DEBUG_INFO, "Making FFA_SHARE_MEM_REQ call\n"));
  DEBUG ((
    DEBUG_INFO,
    "SvcArgs.Arg0 0x%lx Arg1 0x%lx Arg2 0x%lx Arg3 0x%lx Arg4 0x%lx\n",
    SvcArgs.Arg0,
    SvcArgs.Arg1,
    SvcArgs.Arg2,
    SvcArgs.Arg3,
    SvcArgs.Arg4
    ));

  ArmCallSvc (&SvcArgs);

  if ((SvcArgs.Arg0 == FFA_ERROR)) {
    *Handle = 0U;
    DEBUG ((DEBUG_ERROR, "FFA_SHARE_MEM_REQ_64 failed Arg2 0x%lx Arg3 0x%lx\n", SvcArgs.Arg2, SvcArgs.Arg3));
    Status = EFI_UNSUPPORTED;
    goto ExitSendFfaShareCommand;
  }

  *Handle = ((UINT64)SvcArgs.Arg3 << 32) | SvcArgs.Arg2;
  Status  = EFI_SUCCESS;
  DEBUG ((DEBUG_INFO, "FFA_SHARE_MEM_REQ_64 successful Arg0 0x%lx Arg1 0x%lx Arg2 0x%lx Arg3 0x%lx Arg4 0x%lx\n", SvcArgs.Arg0, SvcArgs.Arg1, SvcArgs.Arg2, SvcArgs.Arg3, SvcArgs.Arg4));
  DEBUG ((DEBUG_INFO, "Storing Handle 0x%lx\n", *Handle));
ExitSendFfaShareCommand:
  return Status;
}
