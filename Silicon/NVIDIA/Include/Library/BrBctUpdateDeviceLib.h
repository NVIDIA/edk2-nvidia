/** @file

  BR-BCT Update Device Library

  Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BR_BCT_UPDATE_DEVICE_LIB_H__
#define __BR_BCT_UPDATE_DEVICE_LIB_H__

#include <Library/FwPartitionDeviceLib.h>
#include <Protocol/BrBctUpdateProtocol.h>
#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define BR_BCT_UPDATE_PRIVATE_DATA_SIGNATURE  SIGNATURE_32 ('B','R','B','C')

// BR-BCT update private data structure
typedef struct {
  UINT32                           Signature;

  // BR-BCT info
  UINT32                           SlotSize;
  UINT32                           BrBctDataSize;
  UINTN                            BctPartitionSlots;
  FW_PARTITION_PRIVATE_DATA        *BrBctPartition;
  FW_PARTITION_PRIVATE_DATA        *BrBctBackupPartition;

  // Protocol info
  EFI_HANDLE                       Handle;
  NVIDIA_BR_BCT_UPDATE_PROTOCOL    Protocol;
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
  IN  FW_PARTITION_ADDRESS_CONVERT  ConvertFunction
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
  @param[in]  EraseBlockSize    Device erase block size in bytes

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
BrBctUpdateDeviceLibInit (
  IN  UINT32  ActiveBootChain,
  IN  UINT32  EraseBlockSize
  );

#endif
