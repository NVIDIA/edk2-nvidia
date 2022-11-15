/** @file
Misc Library for OPTEE related functions in Standalone MM.

Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#define HIDREV_OFFSET             0x4
#define HIDREV_PRE_SI_PLAT_SHIFT  0x14
#define HIDREV_PRE_SI_PLAT_MASK   0xf
#define MAX_SOCKETS               4

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
  if (GuidHob == NULL) {
    return Status;
  }

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
IsQspi0Present (
  UINT32  *NumRegions
  )
{
  EFI_STATUS            Status           = EFI_NOT_FOUND;
  EFI_MM_DEVICE_REGION  *DeviceRegionMap = NULL;
  UINTN                 Index;
  EFI_HOB_GUID_TYPE     *GuidHob;
  BOOLEAN               QspiPresent = FALSE;
  UINT32                NumQspi;

  GuidHob = GetFirstGuidHob (&gEfiStandaloneMmDeviceMemoryRegions);
  if (GuidHob == NULL) {
    return Status;
  }

  DeviceRegionMap = GET_GUID_HOB_DATA (GuidHob);
  NumQspi         = 0;
  for (Index = 0; Index < MAX_DEVICE_REGIONS; Index++) {
    if (AsciiStrStr (DeviceRegionMap[Index].DeviceRegionName, "qspi0") != NULL) {
      QspiPresent = TRUE;
      NumQspi++;
    }
  }

  if (NumRegions != NULL) {
    *NumRegions = NumQspi;
  }

  return QspiPresent;
}

/**
  * GetQspi0DeviceRegions
  * Get all the MMIO regions for QSPI controller 0 across all the sockets.
  *
  * @param[out]  QspiRegions  Allocated region of Qspi0 regions on success.
  * @param[out]  NumRegions   Number of Qspi device regions.
  *
  * @retval  EFI_SUCCESS           Found QSPI regions.
  *          EFI_NOT_FOUND         Device Memory HOB not found or no Qspi0
  *                                regions installed in the HOB.
  *          EFI_OUT_OF_RESOURCES  Failed to allocate the QspiRegions Buffer.
 **/
EFIAPI
EFI_STATUS
GetQspi0DeviceRegions (
  EFI_MM_DEVICE_REGION  **QspiRegions,
  UINT32                *NumRegions
  )
{
  EFI_MM_DEVICE_REGION  *QspiMmio;
  UINT32                NumQspi;
  EFI_STATUS            Status;
  UINTN                 Index;
  EFI_HOB_GUID_TYPE     *GuidHob;
  EFI_MM_DEVICE_REGION  *DeviceRegionMap = NULL;
  UINTN                 QspiIndex;

  Status = EFI_SUCCESS;
  if (IsQspi0Present (&NumQspi) == FALSE) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: There are no QSPI0 regions present\n",
      __FUNCTION__
      ));
    Status = EFI_NOT_FOUND;
    goto ExitGetQspiDeviceRegions;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: %u QSPI0 regions present\n",
    __FUNCTION__,
    NumQspi
    ));
  QspiMmio = AllocateRuntimeZeroPool (sizeof (EFI_MM_DEVICE_REGION) * NumQspi);
  if (QspiMmio == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to allocate %u bytes\n",
      __FUNCTION__,
      (NumQspi * sizeof (EFI_MM_DEVICE_REGION))
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitGetQspiDeviceRegions;
  }

  GuidHob = GetFirstGuidHob (&gEfiStandaloneMmDeviceMemoryRegions);
  if (GuidHob == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to lookup Device Memory Hob",
      __FUNCTION__
      ));
    Status = EFI_NOT_FOUND;
    FreePool (QspiMmio);
    goto ExitGetQspiDeviceRegions;
  }

  DeviceRegionMap = GET_GUID_HOB_DATA (GuidHob);
  QspiIndex       = 0;
  for (QspiIndex = 0, Index = 0;
       (Index < MAX_DEVICE_REGIONS) && (QspiIndex < NumQspi);
       Index++)
  {
    if (AsciiStrStr (DeviceRegionMap[Index].DeviceRegionName, "qspi0") != NULL) {
      CopyMem (
        &QspiMmio[QspiIndex++],
        &DeviceRegionMap[Index],
        sizeof (EFI_MM_DEVICE_REGION)
        );
    }
  }

  *QspiRegions = QspiMmio;
  *NumRegions  = NumQspi;
ExitGetQspiDeviceRegions:
  return Status;
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
  if (GuidHob == NULL) {
    return EFI_NOT_FOUND;
  }

  StmmCommBuffers = (STMM_COMM_BUFFERS *)GET_GUID_HOB_DATA (GuidHob);
  *CpuBlAddr      = StmmCommBuffers->CpuBlParamsAddr;
  return EFI_SUCCESS;
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
  EFI_STATUS            Status;
  UINT64                VarOffset;
  UINT64                VarSize;
  UINT16                DeviceInstance;
  EFI_PHYSICAL_ADDRESS  CpuBlAddr;
  TEGRA_PLATFORM_TYPE   Platform;

  if (IsOpteePresent ()) {
    /* For Jetson we always use CS 0 */
    *VarCs = NOR_FLASH_CHIP_SELECT_JETSON;
    Status = EFI_SUCCESS;
  } else {
    Status = GetCpuBlParamsAddrStMm (&CpuBlAddr);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get CPUBL Addr %r\n",
        __FUNCTION__,
        Status
        ));
      goto ExitVarStoreCs;
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
      goto ExitVarStoreCs;
    }

    DEBUG ((
      DEBUG_ERROR,
      "%a:PartitionIndex[%u] VarOffset %lu VarSize %lu"
      "Device Instance %lx\n",
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
  }

ExitVarStoreCs:
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
        "%a: SockNum %u is out of range , max(%u)\n",
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
  EFI_HANDLE  *HandleBuffer;
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
        "%a: Failed to find SocketId installed on %u %r\n",
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
        "%a: Failed to find Protocol installed on %u %r\n",
        __FUNCTION__,
        HandleBuffer[Index],
        Status
        ));
    }
  }

ExitFindProtocolInSocket:
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
