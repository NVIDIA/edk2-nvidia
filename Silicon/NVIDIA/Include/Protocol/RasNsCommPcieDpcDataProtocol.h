/** @file
*
*  Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
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
