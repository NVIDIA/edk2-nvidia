/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/PlatformResourceLib.h>

/**
  Retrieve CPU BL Address

**/
UINTN
EFIAPI
GetCPUBLBaseAddress (
  VOID
  )
{
  return 0;
}

/**
  Set Tegra UART Base Address

  @param[in]    UART base address

**/
VOID
EFIAPI
SetTegraUARTBaseAddress (
  IN EFI_PHYSICAL_ADDRESS  UartBaseAddress
  )
{
  return;
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
  return 0;
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
  return EFI_UNSUPPORTED;
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
  return FALSE;
}

/**
  Retrieve Dram Page Blacklist Info Address

**/
NVDA_MEMORY_REGION *
EFIAPI
GetDramPageBlacklistInfoAddress (
  VOID
  )
{
  return NULL;
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
  return 0;
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
  return 0;
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
  return EFI_UNSUPPORTED;
}

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
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Get Platform Resource Information
  Does not update the CPU info structures.

**/
EFI_STATUS
EFIAPI
GetPlatformResourceInformation (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Update info in Platform Resource Information

**/
EFI_STATUS
EFIAPI
UpdatePlatformResourceInformation (
  VOID
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Get Rootfs Status Register Value

**/
EFI_STATUS
EFIAPI
GetRootfsStatusReg (
  IN UINT32  *RegisterValue
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Set Rootfs Status Register

**/
EFI_STATUS
EFIAPI
SetRootfsStatusReg (
  IN UINT32  RegisterValue
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Get Platform Resource Information in StMM image

**/
EFI_STATUS
EFIAPI
GetPlatformResourceInformationStandaloneMm (
  IN TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo,
  IN PHYSICAL_ADDRESS              CpuBootloaderAddress
  )
{
  return EFI_UNSUPPORTED;
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
  return EFI_UNSUPPORTED;
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
  return EFI_UNSUPPORTED;
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
  return 0x1;
}

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
  )
{
  if (SocketNum == 0) {
    return TRUE;
  } else {
    return FALSE;
  }
}

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
  )
{
  return FALSE;
}

/**
  Set next boot into recovery

**/
VOID
EFIAPI
SetNextBootRecovery (
  IN  VOID
  )
{
  return;
}

/**
  Retrieve Active Boot Chain Information for StMm.

  @param[in]  ChipID                Chip ID
  @param[in]  ScratchBase           Base address of scratch register space.
  @param[out] BootChain             Active boot chain (0=A, 1=B).
 *
 * @retval  EFI_SUCCESS             Boot chain retrieved successfully.
 * @retval  others                  Error retrieving boot chain.
**/
EFI_STATUS
EFIAPI
GetActiveBootChainStMm (
  IN  UINTN   ChipID,
  IN  UINTN   ScratchBase,
  OUT UINT32  *BootChain
  )
{
  return EFI_UNSUPPORTED;
}
