/** @file
  NVIDIA Secure Engine Random number generator Protocol

  Copyright (c) 2019, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SE_RNG_PROTOCOL_H__
#define __SE_RNG_PROTOCOL_H__

#include <PiDxe.h>
#include <Uefi/UefiSpec.h>

#define NVIDIA_SE_RNG_PROTOCOL_GUID \
  { \
  0xbb34a29d, 0x0d3c, 0x43c9, { 0x8c, 0xc7, 0x64, 0x73, 0x80, 0x24, 0xd6, 0x57 } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_SE_RNG_PROTOCOL NVIDIA_SE_RNG_PROTOCOL;

/**
  Gets 128-bits of random data from SE.

  @param[in]     This                The instance of the NVIDIA_SE_RNG_PROTOCOL.
  @param[in]     Buffer              Buffer to place data into

  @return EFI_SUCCESS               The data was returned.
  @return EFI_INVALID_PARAMETER     Buffer is NULL.
  @return EFI_DEVICE_ERROR          Failed to get random data.
**/
typedef
EFI_STATUS
(EFIAPI *SE_RNG_GET_RANDOM)(
  IN  NVIDIA_SE_RNG_PROTOCOL   *This,
  IN  UINT64                   *Buffer
  );

/// NVIDIA_SE_RNG_PROTOCOL protocol structure.
struct _NVIDIA_SE_RNG_PROTOCOL {
  SE_RNG_GET_RANDOM    GetRandom128;
};

extern EFI_GUID  gNVIDIASeRngProtocolGuid;

#endif
