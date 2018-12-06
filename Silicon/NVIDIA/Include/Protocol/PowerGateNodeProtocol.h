/** @file
  Power Gate node protocol Protocol

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __POWER_GATE_NODE_PROTOCOL_H__
#define __POWER_GATE_NODE_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_POWER_GATE_NODE_PROTOCOL_GUID \
  { \
  0x945ea433, 0x3ce4, 0x4298, { 0x81, 0x42, 0x7c, 0xcc, 0x94, 0x0d, 0x96, 0x5c } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_POWER_GATE_NODE_PROTOCOL NVIDIA_POWER_GATE_NODE_PROTOCOL;


/**
  This function allows for deassert of specified power gate node.

  @param[in]     This                The instance of the NVIDIA_POWER_GATE_NODE_PROTOCOL.
  @param[in]     ResetId             Id to de-assert

  @return EFI_SUCCESS                Powergate deasserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to deassert powergate
**/
typedef
EFI_STATUS
(EFIAPI *POWER_GATE_NODE_DEASSERT) (
  IN  NVIDIA_POWER_GATE_NODE_PROTOCOL   *This,
  IN  UINT32                       PowerGateId
  );

/**
  This function allows for assert of specified power gate nodes.

  @param[in]     This                The instance of the NVIDIA_POWER_GATE_NODE_PROTOCOL.
  @param[in]     ResetId             Id to assert

  @return EFI_SUCCESS                PowerGate asserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to assert PowerGate
**/
typedef
EFI_STATUS
(EFIAPI *POWER_GATE_NODE_ASSERT) (
  IN  NVIDIA_POWER_GATE_NODE_PROTOCOL   *This,
  IN  UINT32                       PowerGateId
  );

/// NVIDIA_RESET_NODE_PROTOCOL protocol structure.
struct _NVIDIA_POWER_GATE_NODE_PROTOCOL {

  POWER_GATE_NODE_DEASSERT     Deassert;
  POWER_GATE_NODE_ASSERT       Assert;
  UINT32                  PowerGateId;
};

extern EFI_GUID gNVIDIAPowerGateNodeProtocolGuid;

#endif
