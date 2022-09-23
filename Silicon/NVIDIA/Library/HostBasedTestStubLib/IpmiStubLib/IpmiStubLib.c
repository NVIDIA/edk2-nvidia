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

#include <Library/HostBasedTestStubLib/IpmiStubLib.h>

IPMI_COMMAND  *mStubIpmiCommand;
UINT8         mIpmiCommandCounter = 0;

#define MAX_IPMI_COMMAND_SUPPORTED  10

/**
 Init Ipmi stub support

  @retval None

**/
VOID
IpmiStubInit (
  VOID
  )
{
  mStubIpmiCommand = (IPMI_COMMAND *)AllocateZeroPool (sizeof (IPMI_COMMAND) * MAX_IPMI_COMMAND_SUPPORTED);
}

/**
 Cleanup Ipmi stub support

  @retval None

**/
VOID
IpmiStubDeInit (
  VOID
  )
{
  FreePool (mStubIpmiCommand);
}

EFI_STATUS
MockIpmiSubmitCommand (
  IN UINT8       *ResponseData,
  IN UINT32      ResponseDataSize,
  IN EFI_STATUS  ReturnStatus
  )
{
  if (mIpmiCommandCounter == MAX_IPMI_COMMAND_SUPPORTED) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (mIpmiCommandCounter == 0) {
    IpmiStubInit ();
  }

  mStubIpmiCommand[mIpmiCommandCounter].ResponseData     = ResponseData;
  mStubIpmiCommand[mIpmiCommandCounter].ResponseDataSize = ResponseDataSize;
  mStubIpmiCommand[mIpmiCommandCounter].ForcedStatus     = ReturnStatus;
  mIpmiCommandCounter++;
  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;
  mIpmiCommandCounter--;
  CopyMem (ResponseData, mStubIpmiCommand[mIpmiCommandCounter].ResponseData, mStubIpmiCommand[mIpmiCommandCounter].ResponseDataSize);
  *ResponseDataSize = mStubIpmiCommand[mIpmiCommandCounter].ResponseDataSize;
  Status            = mStubIpmiCommand[mIpmiCommandCounter].ForcedStatus;
  if (mIpmiCommandCounter == 0) {
    IpmiStubDeInit ();
  }

  return Status;
}

/**
  Initialize the global varible with the pointer of IpmiTransport Protocol.

  @retval EFI_SUCCESS  Always return success.

**/
EFI_STATUS
InitializeIpmiBase (
  VOID
  )
{
  return EFI_SUCCESS;
}
