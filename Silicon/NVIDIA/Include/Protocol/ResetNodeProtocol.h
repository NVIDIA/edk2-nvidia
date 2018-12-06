/** @file
  Reset node protocol Protocol

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __RESET_NODE_PROTOCOL_H__
#define __RESET_NODE_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_RESET_NODE_PROTOCOL_GUID \
  { \
  0xc1bc3373, 0xeaa5, 0x418f, { 0x91, 0xb2, 0xa3, 0x71, 0xd6, 0xc3, 0x9b, 0xab } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_RESET_NODE_PROTOCOL NVIDIA_RESET_NODE_PROTOCOL;


/**
  This function allows for deassert of all reset nodes.

  @param[in]     This                The instance of the NVIDIA_RESET_NODE_PROTOCOL.

  @return EFI_SUCCESS                All resets deasserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to deassert all resets
**/
typedef
EFI_STATUS
(EFIAPI *RESET_NODE_DEASSERT_ALL) (
  IN  NVIDIA_RESET_NODE_PROTOCOL   *This
  );

/**
  This function allows for assert of all reset nodes.

  @param[in]     This                The instance of the NVIDIA_RESET_NODE_PROTOCOL.

  @return EFI_SUCCESS                All resets deasserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to de-assert all resets
**/
typedef
EFI_STATUS
(EFIAPI *RESET_NODE_ASSERT_ALL) (
  IN  NVIDIA_RESET_NODE_PROTOCOL   *This
  );

/**
  This function allows for deassert of specified reset nodes.

  @param[in]     This                The instance of the NVIDIA_RESET_NODE_PROTOCOL.
  @param[in]     ResetId             Id to de-assert

  @return EFI_SUCCESS                Resets deasserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to deassert resets
**/
typedef
EFI_STATUS
(EFIAPI *RESET_NODE_DEASSERT) (
  IN  NVIDIA_RESET_NODE_PROTOCOL   *This,
  IN  UINT32                       ResetId
  );

/**
  This function allows for assert of specified reset nodes.

  @param[in]     This                The instance of the NVIDIA_RESET_NODE_PROTOCOL.
  @param[in]     ResetId             Id to assert

  @return EFI_SUCCESS                Resets asserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to assert resets
**/
typedef
EFI_STATUS
(EFIAPI *RESET_NODE_ASSERT) (
  IN  NVIDIA_RESET_NODE_PROTOCOL   *This,
  IN  UINT32                       ResetId
  );

typedef struct {
  UINT32                  ResetId;
  CONST CHAR8             *ResetName;
} NVIDIA_RESET_NODE_ENTRY;

/// NVIDIA_RESET_NODE_PROTOCOL protocol structure.
struct _NVIDIA_RESET_NODE_PROTOCOL {

  RESET_NODE_DEASSERT_ALL DeassertAll;
  RESET_NODE_ASSERT_ALL   AssertAll;
  RESET_NODE_DEASSERT     Deassert;
  RESET_NODE_ASSERT       Assert;
  UINTN                   Resets;
  NVIDIA_RESET_NODE_ENTRY ResetEntries[0];
};

extern EFI_GUID gNVIDIAResetNodeProtocolGuid;

#endif
