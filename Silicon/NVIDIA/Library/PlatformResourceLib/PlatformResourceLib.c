/** @file
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/MceAriLib.h>
#include <Library/NvgLib.h>
#include "T194ResourceConfig.h"
#include "T234ResourceConfig.h"
#include "TH500ResourceConfig.h"

STATIC
EFI_PHYSICAL_ADDRESS  TegraUARTBaseAddress = 0x0;

/**
  Set Tegra UART Base Address
**/
VOID
EFIAPI
SetTegraUARTBaseAddress (
  IN EFI_PHYSICAL_ADDRESS  UartBaseAddress
  )
{
  TegraUARTBaseAddress = UartBaseAddress;
}

/**
  Retrieve Tegra UART Base Address

**/
EFI_PHYSICAL_ADDRESS
EFIAPI
GetTegraUARTBaseAddress (
  VOID
  )
{
  UINTN  ChipID;

  if (TegraUARTBaseAddress != 0x0) {
    return TegraUARTBaseAddress;
  }

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return FixedPcdGet64 (PcdTegra16550UartBaseT194);
    case T234_CHIP_ID:
      return FixedPcdGet64 (PcdTegra16550UartBaseT234);
    case TH500_CHIP_ID:
      return FixedPcdGet64 (PcdSbsaUartBaseTH500);
    default:
      return 0x0;
  }
}

/**
  It's to get the UART instance number that the trust-firmware hands over.
  Currently that chain is broken so temporarily override the UART instance number
  to the fixed known id based on the chip id.

**/
STATIC
BOOLEAN
GetSharedUARTInstanceId (
  IN  UINTN   ChipID,
  OUT UINT32  *UARTInstanceNumber
  )
{
  switch (ChipID) {
    case T234_CHIP_ID:
      *UARTInstanceNumber = 1; // UART_A
      return TRUE;
    case TH500_CHIP_ID:
      *UARTInstanceNumber = 1; // UART_0
      return TRUE;
    default:
      return FALSE;
  }
}

/**
  Retrieve the type and address of UART based on the instance Number

**/
EFI_STATUS
EFIAPI
GetUARTInstanceInfo (
  OUT UINT32                *UARTInstanceType,
  OUT EFI_PHYSICAL_ADDRESS  *UARTInstanceAddress
  )
{
  UINTN   ChipID;
  UINT32  SharedUARTInstanceId;

  *UARTInstanceType    = TEGRA_UART_TYPE_NONE;
  *UARTInstanceAddress = 0x0;

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T194_CHIP_ID:
      *UARTInstanceType    = TEGRA_UART_TYPE_TCU;
      *UARTInstanceAddress = (EFI_PHYSICAL_ADDRESS)FixedPcdGet64 (PcdTegra16550UartBaseT194);
      return EFI_SUCCESS;
    case T234_CHIP_ID:
      if (!GetSharedUARTInstanceId (ChipID, &SharedUARTInstanceId)) {
        return EFI_UNSUPPORTED;
      }

      return T234UARTInstanceInfo (SharedUARTInstanceId, UARTInstanceType, UARTInstanceAddress);
    case TH500_CHIP_ID:
      if (!GetSharedUARTInstanceId (ChipID, &SharedUARTInstanceId)) {
        return EFI_UNSUPPORTED;
      }

      return TH500UARTInstanceInfo (SharedUARTInstanceId, UARTInstanceType, UARTInstanceAddress);
    default:
      return EFI_UNSUPPORTED;
  }
}

/**
  Retrieve chip specific info for GIC

**/
BOOLEAN
EFIAPI
GetGicInfo (
  OUT TEGRA_GIC_INFO  *GicInfo
  )
{
  UINTN  ChipID;

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T194_CHIP_ID:
      // *CompatString = "arm,gic-v2";
      GicInfo->GicCompatString = "arm,gic-400";
      GicInfo->ItsCompatString = "";
      GicInfo->Version         = 2;
      break;
    case T234_CHIP_ID:
      GicInfo->GicCompatString = "arm,gic-v3";
      GicInfo->ItsCompatString = "arm,gic-v3-its";
      GicInfo->Version         = 3;
      break;
    case TH500_CHIP_ID:
      GicInfo->GicCompatString = "arm,gic-v3";
      GicInfo->ItsCompatString = "arm,gic-v3-its";
      GicInfo->Version         = 4;
      break;
    default:
      return FALSE;
  }

  return TRUE;
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
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;
  UINTN  CpuBootloaderAddressLo;
  UINTN  CpuBootloaderAddressHi;
  UINTN  SystemMemoryBaseAddress;

  ChipID = TegraGetChipID ();

  CpuBootloaderAddressLo = (UINTN)MmioRead32 (TegraGetBLInfoLocationAddress (ChipID));
  CpuBootloaderAddressHi = (UINTN)MmioRead32 (TegraGetBLInfoLocationAddress (ChipID) + sizeof (UINT32));
  CpuBootloaderAddress   = (CpuBootloaderAddressHi << 32) | CpuBootloaderAddressLo;

  if (ChipID == T194_CHIP_ID) {
    SystemMemoryBaseAddress = TegraGetSystemMemoryBaseAddress (ChipID);

    if (CpuBootloaderAddress < SystemMemoryBaseAddress) {
      CpuBootloaderAddress <<= 16;
    }
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
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetDTBBaseAddress (CpuBootloaderAddress);
    case T234_CHIP_ID:
      return T234GetDTBBaseAddress (CpuBootloaderAddress);
    case TH500_CHIP_ID:
      return TH500GetDTBBaseAddress (CpuBootloaderAddress);
    default:
      return 0x0;
  }
}

/**
  Retrieve GR Blob Address

**/
UINT64
EFIAPI
GetGRBlobBaseAddress (
  VOID
  )
{
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T194_CHIP_ID:
      return T194GetGRBlobBaseAddress (CpuBootloaderAddress);
    case T234_CHIP_ID:
      return T234GetGRBlobBaseAddress (CpuBootloaderAddress);
    default:
      return 0x0;
  }
}

/**
  Validate Active Boot Chain

**/
EFI_STATUS
EFIAPI
ValidateActiveBootChain (
  VOID
  )
{
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234ValidateActiveBootChain (CpuBootloaderAddress);
    case T194_CHIP_ID:
      return T194ValidateActiveBootChain (CpuBootloaderAddress);
    case TH500_CHIP_ID:
      return TH500ValidateActiveBootChain (CpuBootloaderAddress);
    default:
      return EFI_UNSUPPORTED;
  }
}

/**
  Set next boot chain

**/
EFI_STATUS
EFIAPI
SetNextBootChain (
  IN  UINT32  BootChain
  )
{
  UINTN  ChipID;

  ChipID = TegraGetChipID ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234SetNextBootChain (BootChain);
      break;
    case T194_CHIP_ID:
      return T194SetNextBootChain (BootChain);
      break;
    default:
      return EFI_UNSUPPORTED;
      break;
  }
}

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
GetPlatformResourceInformation (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;

  PlatformResourceInfo->ResourceInfo = AllocateZeroPool (sizeof (TEGRA_RESOURCE_INFO));
  if (PlatformResourceInfo->ResourceInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  PlatformResourceInfo->BoardInfo = AllocateZeroPool (sizeof (TEGRA_BOARD_INFO));
  if (PlatformResourceInfo->BoardInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234GetPlatformResourceInformation (CpuBootloaderAddress, PlatformResourceInfo);
    case T194_CHIP_ID:
      return T194GetPlatformResourceInformation (CpuBootloaderAddress, PlatformResourceInfo);
    case TH500_CHIP_ID:
      return TH500GetPlatformResourceInformation (CpuBootloaderAddress, PlatformResourceInfo, FALSE);
    default:
      return EFI_UNSUPPORTED;
  }
}

EFI_STATUS
EFIAPI
GetRootfsStatusReg (
  IN UINT32  *RegisterValue
  )
{
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234GetRootfsStatusReg (CpuBootloaderAddress, RegisterValue);
    case T194_CHIP_ID:
      return T194GetRootfsStatusReg (CpuBootloaderAddress, RegisterValue);
    default:
      return EFI_UNSUPPORTED;
  }
}

EFI_STATUS
EFIAPI
SetRootfsStatusReg (
  IN UINT32  RegisterValue
  )
{
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case T234_CHIP_ID:
      return T234SetRootfsStatusReg (CpuBootloaderAddress, RegisterValue);
    case T194_CHIP_ID:
      return T194SetRootfsStatusReg (CpuBootloaderAddress, RegisterValue);
    default:
      return EFI_UNSUPPORTED;
  }
}

/**
  Get Platform Resource Information

**/
EFI_STATUS
EFIAPI
GetPlatformResourceInformationStandaloneMm (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo,
  IN PHYSICAL_ADDRESS              CpuBootloaderAddress
  )
{
  return TH500GetPlatformResourceInformation (CpuBootloaderAddress, PlatformResourceInfo, TRUE);
}

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
  )
{
  UINTN  ChipID;
  UINTN  CpuBootloaderAddress;

  ChipID = TegraGetChipID ();

  CpuBootloaderAddress = GetCPUBLBaseAddress ();

  switch (ChipID) {
    case TH500_CHIP_ID:
      return TH500GetPartitionInfo (
               CpuBootloaderAddress,
               PartitionIndex,
               DeviceInstance,
               PartitionStartByte,
               PartitionSizeBytes
               );
    default:
      return EFI_UNSUPPORTED;
  }
}

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
  )
{
  return TH500GetPartitionInfo (
           CpuBlAddress,
           PartitionIndex,
           DeviceInstance,
           PartitionStartByte,
           PartitionSizeBytes
           );
}

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
  )
{
  UINT32  SocketMask;

  if (CpuBlAddress == 0) {
    SocketMask = 0x1;
  } else {
    SocketMask = TH500GetSocketMask (CpuBlAddress);
  }

  return SocketMask;
}

/**
 * Check if socket is enabled in the CPU BL Params's socket mask.
 * This API is usually only called from StMM.
 *
 * @param[in] CpuBlAddress          Address of the CPU BL params.
 * @param[in] SocketNum             Socket to check.
 *
 * @retval  TRUE                    Socket is enabled.
 * @retval  FALSE                   Socket is not enabled.
**/
BOOLEAN
EFIAPI
IsSocketEnabledStMm (
  IN UINTN   CpuBlAddress,
  IN UINT32  SocketNum
  )
{
  UINT32   SocketMask;
  BOOLEAN  SocketEnabled;

  SocketMask = GetSocketMaskStMm (CpuBlAddress);

  SocketEnabled = ((SocketMask & (1U << SocketNum)) ? TRUE : FALSE);
  return SocketEnabled;
}
