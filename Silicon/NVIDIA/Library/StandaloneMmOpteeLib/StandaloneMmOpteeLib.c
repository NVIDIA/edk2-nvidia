/** @file
Misc Library for OPTEE related functions in Standalone MM.

SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

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
