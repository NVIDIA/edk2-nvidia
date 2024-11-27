/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _RAS_FW_INTERFACE_LIB_H_
#define _RAS_FW_INTERFACE_LIB_H_

#include <Uefi.h>
#include <Protocol/Apei.h>
#include <Protocol/MmCommunication2.h>
#include <Protocol/RasNsCommPcieDpcDataProtocol.h>
#include <Server/RASNSInterface.h>

typedef
EFI_STATUS
(EFIAPI *GUIDED_COMM)(
  IN UINT32 Socket,
  IN EFI_MM_COMMUNICATE_HEADER  *CommunicateHeader,
  IN RAS_FW_BUFFER              *RasFwBufferInfo
  );

typedef struct {
  RAS_FW_BUFFER    *RasFwBufferInfo;
  UINT32           BertEventId;
  UINT64           EinjDoorbellAddress;
  UINT64           EinjDoorbellValue;
  UINT64           EinjDoorbellMask;
  GUIDED_COMM      GuidedComm;
} RAS_FW_INTERFACE;

EFI_STATUS
RASFwInterfaceInit (
  UINT32            SocketCount,
  RAS_FW_INTERFACE  **RasFwInterfaceOut
  );

#endif /* _RAS_FW_INTERFACE_LIB_H_ */
