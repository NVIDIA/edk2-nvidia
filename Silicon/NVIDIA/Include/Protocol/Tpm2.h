/** @file
  NVIDIA TPM2 over QSPI protocol

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVIDIA_TPM2_PROTOCOL_H__
#define __NVIDIA_TPM2_PROTOCOL_H__

#define NVIDIA_TPM2_PROTOCOL_GUID \
  { \
    0x5fa7d7ca, 0x4b3f, 0x11ed, { 0xa2, 0xef, 0x6b, 0x88, 0x75, 0xf0, 0x98, 0x2d } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_TPM2_PROTOCOL NVIDIA_TPM2_PROTOCOL;

#define TPM_MAX_TRANSFER_SIZE  64

/**
  Performs read/write data transfer to/from TPM over QSPI bus

  @param  This         pointer to NVIDIA_TPM2_PROTOCOL
  @param  ReadAccess   TRUE:  for read request; FALSE: for write request
  @param  Addr         TPM register address
  @param  Data         pointer to the data buffer
  @param  DataSize     data size in bytes, must not be greater than 64

  @retval EFI_SUCCESS           The transfer completes successfully.
  @retval EFI_INVALID_PARAMETER Data size is out of range.
  @retval Others                Data transmission failed.
**/
typedef
EFI_STATUS
(EFIAPI *TPM2_TRANSFER)(
  IN     NVIDIA_TPM2_PROTOCOL *This,
  IN     BOOLEAN              ReadAccess,
  IN     UINT16               Addr,
  IN OUT UINT8                *Data,
  IN     UINT16               DataSize
  );

/// NVIDIA_TPM2_PROTOCOL protocol structure.
struct _NVIDIA_TPM2_PROTOCOL {
  TPM2_TRANSFER    Transfer;
};

extern EFI_GUID  gNVIDIATpm2ProtocolGuid;

#endif
