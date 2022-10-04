/** @file

  BR-BCT Update Protocol

  Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BR_BCT_UPDATE_PROTOCOL_H__
#define __BR_BCT_UPDATE_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>

#define NVIDIA_BR_BCT_UPDATE_PROTOCOL_GUID      \
  {0xec60b96c, 0x0796, 0x47a9, {0xbf, 0xe7, 0x6b, 0x83, 0xf2, 0xe7, 0xd1, 0x5d}}

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

// protocol structure
struct _NVIDIA_BR_BCT_UPDATE_PROTOCOL {
  BR_BCT_UPDATE_FW_CHAIN    UpdateFwChain;
};

extern EFI_GUID  gNVIDIABrBctUpdateProtocolGuid;

#endif
