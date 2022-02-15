/** @file

  Boot Chain Protocol

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef __BOOT_CHAIN_PROTOCOL_H__
#define __BOOT_CHAIN_PROTOCOL_H__

#include <Library/BootChainInfoLib.h>
#include <Uefi/UefiBaseType.h>

#define NVIDIA_BOOT_CHAIN_PROTOCOL_GUID    \
  {0xbbed2514, 0x140b, 0x4176, {0xa8, 0xf6, 0x51, 0x35, 0x8e, 0xbb, 0x21, 0xdf}}

typedef struct _NVIDIA_BOOT_CHAIN_PROTOCOL NVIDIA_BOOT_CHAIN_PROTOCOL;

// see BootChainInfoLib GetBootChainPartitionName() function
typedef
EFI_STATUS
(EFIAPI *BOOT_CHAIN_GET_PARTITION_NAME)(
  IN  CONST CHAR16      *BasePartitionName,
  IN  UINTN             BootChain,
  OUT CHAR16            *BootChainPartitionName
  );

/**
  Check if BootChainFwNext or BootChainFwStatus UEFI variables exist for a
  pending boot chain update and cancel it.

  Called by FMP CheckImage() function to reject the FW update and cancel a boot
  chain update if both are requested at the same time.

  @param[in]  This                  Instance to protocol
  @param[in]  Canceled              Pointer to return canceled flag if a
                                    boot chain update was pending

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *BOOT_CHAIN_CHECK_AND_CANCEL_UPDATE)(
  IN  NVIDIA_BOOT_CHAIN_PROTOCOL    *This,
  OUT BOOLEAN                       *Canceled
  );

/**
  Execute boot chain update algorithm.  Must be called after HandleCapsules()
  installs any mass media FW updates.

  @param[in]  This                  Instance to protocol

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *BOOT_CHAIN_EXECUTE_UPDATE)(
  IN  NVIDIA_BOOT_CHAIN_PROTOCOL    *This
);

// Protocol structure
struct _NVIDIA_BOOT_CHAIN_PROTOCOL {
  // boot chain info access
  UINT32                                ActiveBootChain;
  BOOT_CHAIN_GET_PARTITION_NAME         GetPartitionName;

  // boot chain update functions
  BOOT_CHAIN_CHECK_AND_CANCEL_UPDATE    CheckAndCancelUpdate;
  BOOT_CHAIN_EXECUTE_UPDATE             ExecuteUpdate;
};

extern EFI_GUID gNVIDIABootChainProtocolGuid;

#endif
