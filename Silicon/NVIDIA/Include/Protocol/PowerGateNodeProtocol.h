/** @file
  Power Gate node protocol Protocol

  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __POWER_GATE_NODE_PROTOCOL_H__
#define __POWER_GATE_NODE_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_POWER_GATE_NODE_PROTOCOL_GUID \
  { \
  0xdc05db20, 0x5dde, 0x4e97, { 0xb3, 0xc7, 0x7b, 0x37, 0x4c, 0x40, 0x73, 0xbb } \
  }

typedef enum {
  CmdPgStateOff = 0,
  CmdPgStateOn = 1,
  CmdPgStateMax,
} MRQ_PG_STATES;

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
  IN  UINT32                            PowerGateId
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
  IN  UINT32                            PowerGateId
  );

/**
  This function allows for getting state of specified power gate nodes.

  @param[in]     This                The instance of the NVIDIA_POWER_GATE_NODE_PROTOCOL.
  @param[in]     PgId                Id to get state of
  @param[out]    PowerGateState      State of PgId

  @return EFI_SUCCESS                Pg asserted.
  @return EFI_NOT_READY              BPMP-IPC protocol is not installed.
  @return EFI_DEVICE_ERROR           Failed to get Pg state
**/
typedef
EFI_STATUS
(EFIAPI *POWER_GATE_NODE_GET_STATE) (
  IN  NVIDIA_POWER_GATE_NODE_PROTOCOL   *This,
  IN  UINT32                            PowerGateId,
  OUT UINT32                            *PowerGateState
  );

/// NVIDIA_RESET_NODE_PROTOCOL protocol structure.
struct _NVIDIA_POWER_GATE_NODE_PROTOCOL {

  POWER_GATE_NODE_DEASSERT     Deassert;
  POWER_GATE_NODE_ASSERT       Assert;
  POWER_GATE_NODE_GET_STATE    GetState;
  UINT32                       NumberOfPowerGates;
  UINT32                       PowerGateId[1];
};

extern EFI_GUID gNVIDIAPowerGateNodeProtocolGuid;

#endif
