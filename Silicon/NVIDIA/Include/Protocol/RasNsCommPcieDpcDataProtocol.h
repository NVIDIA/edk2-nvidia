/** @file
*
*  Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef RAS_NS_COMM_PCIE_DPC_DATA_PROTOCOL_H__
#define RAS_NS_COMM_PCIE_DPC_DATA_PROTOCOL_H__

#include <Server/RASNSInterface.h>

#define MAX_SOCKETS      4
#define PCIE_PER_SOCKET  10

#define NVIDIA_RAS_NS_COMM_PCIE_DPC_DATA_PROTOCOL_GUID \
  { \
  0x97dafc36, 0xc3bc, 0x427e, { 0xa9, 0xf4, 0x71, 0xcd, 0x3f, 0xba, 0x61, 0x95 } \
  }

typedef struct RasPcieDpcCommBuf {
  UINT64    PcieBase;
  UINT64    PcieSize;
} RAS_PCIE_DPC_COMM_BUF_INFO;

extern EFI_GUID  gNVIDIARasNsCommPcieDpcDataProtocolGuid;

#endif // RAS_NS_COMM_PCIE_DPC_DATA_PROTOCOL_H__
