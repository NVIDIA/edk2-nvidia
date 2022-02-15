/** @file
  Reset node protocol Protocol

  Copyright (c) 2018-2019, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __RESET_NODE_PROTOCOL_H__
#define __RESET_NODE_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_RESET_NODE_PROTOCOL_GUID \
  { \
  0xf027ceae, 0xa96d, 0x490d, { 0xbe, 0x82, 0x12, 0x35, 0x81, 0xef, 0x11, 0x88 } \
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
  This function allows for module reset of all reset nodes.

  @param[in]     This                The instance of the NVIDIA_RESET_NODE_PROTOCOL.

  @return EFI_SUCCESS                All resets deasserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to reset all modules
**/
typedef
EFI_STATUS
(EFIAPI *RESET_NODE_MODULE_RESET_ALL) (
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

/**
  This function allows for module reset of specified reset nodes.

  @param[in]     This                The instance of the NVIDIA_RESET_NODE_PROTOCOL.
  @param[in]     ResetId             Id to reset

  @return EFI_SUCCESS                Resets asserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to reset module
**/
typedef
EFI_STATUS
(EFIAPI *RESET_NODE_MODULE_RESET) (
  IN  NVIDIA_RESET_NODE_PROTOCOL   *This,
  IN  UINT32                       ResetId
  );

typedef struct {
  UINT32                  ResetId;
  CONST CHAR8             *ResetName;
} NVIDIA_RESET_NODE_ENTRY;

/// NVIDIA_RESET_NODE_PROTOCOL protocol structure.
struct _NVIDIA_RESET_NODE_PROTOCOL {

  RESET_NODE_DEASSERT_ALL     DeassertAll;
  RESET_NODE_ASSERT_ALL       AssertAll;
  RESET_NODE_MODULE_RESET_ALL ModuleResetAll;
  RESET_NODE_DEASSERT         Deassert;
  RESET_NODE_ASSERT           Assert;
  RESET_NODE_MODULE_RESET     ModuleReset;
  UINTN                       Resets;
  NVIDIA_RESET_NODE_ENTRY     ResetEntries[0];
};

extern EFI_GUID gNVIDIAResetNodeProtocolGuid;

#endif
