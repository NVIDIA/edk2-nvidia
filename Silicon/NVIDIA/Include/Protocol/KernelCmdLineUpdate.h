/** @file
  Kernel Command Line Update Protocol

  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL_H__
#define __NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL_H__


#define NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL_GUID \
  { \
  0xc61a1a9a, 0x8f92, 0x4e2e, { 0x97, 0x8d, 0x04, 0x8d, 0x81, 0xed, 0xdc, 0x8b } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL;


/// NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL protocol structure.
struct _NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL {
  CHAR16 *ExistingCommandLineArgument;
  CHAR16 *NewCommandLineArgument;
};

extern EFI_GUID gNVIDIAKernelCmdLineUpdateGuid;

#endif
