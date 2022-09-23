/** @file
  A simple stub implementation of IpmiBaseLib.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

typedef struct {
  UINT8         *ResponseData;
  UINT32        ResponseDataSize;
  EFI_STATUS    ForcedStatus;
} IPMI_COMMAND;

/**
 Init Ipmi stub support

  @retval None

**/
VOID
IpmiStubInit (
  VOID
  );

/**
 Cleanup Ipmi stub support

  @retval None

**/
VOID
IpmiStubDeInit (
  VOID
  );

EFI_STATUS
MockIpmiSubmitCommand (
  IN UINT8       *ResponseData,
  IN UINT32      ResponseDataSize,
  IN EFI_STATUS  ReturnStatus
  );

/**
  Routine to send commands to BMC.

  @param NetFunction       - Net function of the command
  @param Command           - IPMI Command
  @param CommandData       - Command Data
  @param CommandDataSize   - Size of CommandData
  @param ResponseData      - Response Data
  @param ResponseDataSize  - Response Data Size

  @retval EFI_NOT_AVAILABLE_YET - IpmiTransport Protocol is not installed yet

**/
EFI_STATUS
IpmiSubmitCommand (
  IN UINT8    NetFunction,
  IN UINT8    Command,
  IN UINT8    *CommandData,
  IN UINT32   CommandDataSize,
  OUT UINT8   *ResponseData,
  OUT UINT32  *ResponseDataSize
  );

EFI_STATUS
InitializeIpmiBase (
  VOID
  );
