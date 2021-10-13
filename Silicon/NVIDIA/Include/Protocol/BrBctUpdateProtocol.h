/** @file

  BR-BCT Update Protocol

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

#ifndef __BR_BCT_UPDATE_PROTOCOL_H__
#define __BR_BCT_UPDATE_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>

#define NVIDIA_BR_BCT_UPDATE_PROTOCOL_GUID      \
  {0xec60b96c, 0x0796, 0x47a9, {0xbf, 0xe7, 0x6b, 0x83, 0xf2, 0xe7, 0xd1, 0x5d}}

typedef struct _NVIDIA_BR_BCT_UPDATE_PROTOCOL NVIDIA_BR_BCT_UPDATE_PROTOCOL;

/**
  Update BR-BCT data for inactive boot chain.

  @param[in]  This                  Instance to protocol
  @param[in]  Bytes                 Number of bytes available in Buffer
  @param[in]  Buffer                Address of update data buffer

  @retval EFI_SUCCESS               Operation successful
  @retval others                    Error occurred

**/
typedef
EFI_STATUS
(EFIAPI *BR_BCT_UPDATE_BCT)(
  IN  CONST NVIDIA_BR_BCT_UPDATE_PROTOCOL   *This,
  IN  UINTN                                 Bytes,
  IN  CONST VOID                            *Buffer
  );

/**
  Update BR-BCT FW boot chain selection.
  Switch to A: copy slot 2 to slot 0
  Switch to B: delete slot 0

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
  BR_BCT_UPDATE_BCT                     UpdateBct;
  BR_BCT_UPDATE_FW_CHAIN                UpdateFwChain;
};

extern EFI_GUID gNVIDIABrBctUpdateProtocolGuid;

#endif
