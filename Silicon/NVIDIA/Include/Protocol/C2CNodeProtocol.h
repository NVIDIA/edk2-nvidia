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

#define C2C_STATUS_INIT_NOT_STARTED         0
#define C2C_STATUS_C2C_INVALID_SPEEDO_CODE  7
#define C2C_STATUS_C2C_INVALID_FREQ         8
#define C2C_STATUS_C2C_INVALID_LINK         9
#define C2C_STATUS_C2C0_REFPLL_FAIL         10
#define C2C_STATUS_C2C1_REFPLL_FAIL         11
#define C2C_STATUS_C2C0_PLLCAL_FAIL         12
#define C2C_STATUS_C2C1_PLLCAL_FAIL         13
#define C2C_STATUS_C2C0_CLKDET_FAIL         14
#define C2C_STATUS_C2C1_CLKDET_FAIL         15
#define C2C_STATUS_C2C0_TR_FAIL             16
#define C2C_STATUS_C2C1_TR_FAIL             17
#define C2C_STATUS_C2C_LINK_TRAIN_PASS      255

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
  @param[out]    C2cStatus           Status of init.

  @return EFI_SUCCESS                C2C initialized.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to initialize C2C.
**/
typedef
EFI_STATUS
(EFIAPI *C2C_NODE_INIT)(
  IN   NVIDIA_C2C_NODE_PROTOCOL   *This,
  IN   UINT8                      Partitions,
  OUT  UINT8                      *C2cStatus
  );

/// NVIDIA_C2C_NODE_PROTOCOL protocol structure.
struct _NVIDIA_C2C_NODE_PROTOCOL {
  C2C_NODE_INIT         Init;
  UINT32                BpmpPhandle;
  MRQ_C2C_PARTITIONS    Partitions;
};

extern EFI_GUID  gNVIDIAC2cNodeProtocolGuid;

#endif
