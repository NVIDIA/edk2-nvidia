/** @file

  BR-BCT Update Protocol

  SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BR_BCT_UPDATE_PROTOCOL_H__
#define __BR_BCT_UPDATE_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>

#define NVIDIA_BR_BCT_UPDATE_PROTOCOL_GUID      \
  {0xd341b73b, 0xd989, 0x4df3, {0xa7, 0xcb, 0xb5, 0xfc, 0xe3, 0xb8, 0x92, 0xfc}}

#define BR_BCT_BACKUP_PARTITION_NAME  L"BCT-boot-chain_backup"

typedef struct _NVIDIA_BR_BCT_UPDATE_PROTOCOL NVIDIA_BR_BCT_UPDATE_PROTOCOL;

/**
  Update BR-BCT for new FW boot chain selection.

  @param[in]  This                  Instance to protocol
  @param[in]  NewFwChain            New FW chain to boot (0=a, 1=b)

  @retval EFI_SUCCESS              Operation successful
  @retval others                   Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *BR_BCT_UPDATE_FW_CHAIN)(
  IN  CONST NVIDIA_BR_BCT_UPDATE_PROTOCOL   *This,
  IN  UINTN                                 NewFwChain
  );

/** Update BR-BCT backup partition data for inactive boot chain.

 @param[in]  This                  Instance to protocol
 @param[in]  Data                  Pointer to new data for all chains

 @retval EFI_SUCCESS              Operation successful
 @retval others                   Error occurred

**/

typedef
EFI_STATUS
(EFIAPI *BR_BCT_UPDATE_BACKUP_PARTITION)(
  IN  CONST NVIDIA_BR_BCT_UPDATE_PROTOCOL   *This,
  IN  CONST VOID                            *Data
  );

// protocol structure
struct _NVIDIA_BR_BCT_UPDATE_PROTOCOL {
  BR_BCT_UPDATE_FW_CHAIN            UpdateFwChain;
  BR_BCT_UPDATE_BACKUP_PARTITION    UpdateBackupPartition;
};

extern EFI_GUID  gNVIDIABrBctUpdateProtocolGuid;

#endif
