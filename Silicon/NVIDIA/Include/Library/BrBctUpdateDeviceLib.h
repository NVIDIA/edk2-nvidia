/** @file

  BR-BCT Update Device Library

  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __BR_BCT_UPDATE_DEVICE_LIB_H__
#define __BR_BCT_UPDATE_DEVICE_LIB_H__

#include <Library/FwPartitionDeviceLib.h>
#include <Protocol/BrBctUpdateProtocol.h>
#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define BR_BCT_UPDATE_PRIVATE_DATA_SIGNATURE SIGNATURE_32 ('B','R','B','C')

/**
  Erase data from device.

  @param[in]  DeviceInfo        Pointer to device info struct
  @param[in]  Offset            Offset to begin erase
  @param[in]  Bytes             Number of bytes to erase

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *BR_BCT_UPDATE_DEVICE_ERASE)(
  IN  FW_PARTITION_DEVICE_INFO          *DeviceInfo,
  IN  UINT64                            Offset,
  IN  UINTN                             Bytes
  );

// BR-BCT update private data structure
typedef struct {
  UINT32                            Signature;

  // BR-BCT info
  UINT32                            SlotSize;
  FW_PARTITION_PRIVATE_DATA         *BrBctPartition;

  // Erase function
  BR_BCT_UPDATE_DEVICE_ERASE        DeviceErase;

  // Protocol info
  EFI_HANDLE                        Handle;
  NVIDIA_BR_BCT_UPDATE_PROTOCOL     Protocol;
} BR_BCT_UPDATE_PRIVATE_DATA;

/**
  Convert all pointer addresses within BrBctUpdateDeviceLib to
  support runtime execution.

  @param[in]  ConvertFunction   Function used to convert a pointer

  @retval None

**/
VOID
EFIAPI
BrBctUpdateAddressChangeHandler (
  IN  FW_PARTITION_ADDRESS_CONVERT ConvertFunction
  );

/**
  Get a pointer to the BR_BCT_UPDATE_PRIVATE_DATA structure.

  @retval BR_BCT_UPDATE_PRIVATE_DATA *  Pointer to data structure

**/
BR_BCT_UPDATE_PRIVATE_DATA *
EFIAPI
BrBctUpdateGetPrivate (
  VOID
  );

/**
  De-initialize the BrBctUpdateDeviceLib, freeing all resources.
  The caller should uninstall any installed protocols before
  calling this function.

  @retval None

**/
VOID
EFIAPI
BrBctUpdateDeviceLibDeinit (
  VOID
  );

/**
  Initialize the BrBctUpdateDeviceLib.

  @param[in]  ActiveBootChain   The active FW boot chain (0=a, 1=b)
  @param[in]  DeviceErase       Device erase function
  @param[in]  EraseBlockSize    Device erase block size in bytes

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
BrBctUpdateDeviceLibInit (
  IN  UINT32                        ActiveBootChain,
  IN  BR_BCT_UPDATE_DEVICE_ERASE    DeviceErase,
  IN  UINT32                        EraseBlockSize
  );

#endif
