/** @file
  C2C node protocol Protocol

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __C2C_NODE_PROTOCOL_H__
#define __C2C_NODE_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_C2C_NODE_PROTOCOL_GUID \
  { \
  0x50740212, 0xd769, 0x4b70, { 0xaf, 0x49, 0x3d, 0xa2, 0x95, 0x4b, 0xe2, 0xca } \
  }

typedef enum {
  CmdC2cPartitionNone = 0,
  CmdC2cPartition0    = 1,
  CmdC2cPartition1    = 2,
  CmdC2cPartitionBoth = 3,
  CmdC2cPartitionMax,
} MRQ_C2C_PARTITIONS;

//
// Define for forward reference.
//
typedef struct _NVIDIA_C2C_NODE_PROTOCOL NVIDIA_C2C_NODE_PROTOCOL;

/**
  This function allows for initialization of C2C.

  @param[in]     This                The instance of the NVIDIA_C2C_NODE_PROTOCOL.
  @param[in]     Partitions          Partitions to be initialized.

  @return EFI_SUCCESS                C2C initialized.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to initialize C2C.
**/
typedef
EFI_STATUS
(EFIAPI *C2C_NODE_INIT)(
  IN  NVIDIA_C2C_NODE_PROTOCOL   *This,
  IN  UINT8                      Partitions
  );

/// NVIDIA_C2C_NODE_PROTOCOL protocol structure.
struct _NVIDIA_C2C_NODE_PROTOCOL {
  C2C_NODE_INIT         Init;
  MRQ_C2C_PARTITIONS    Partitions;
};

extern EFI_GUID  gNVIDIAC2cNodeProtocolGuid;

#endif
