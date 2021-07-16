/** @file
  Regulator Control Protocol

  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2018 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2018 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __REGULATOR_PROTOCOL_H__
#define __REGULATOR_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_REGULATOR_PROTOCOL_GUID \
  { \
  0x48e74d70, 0x8dd4, 0x43af, { 0xad, 0x0d, 0x8a, 0x52, 0x05, 0x59, 0x81, 0x6b } \
  }

typedef struct _NVIDIA_REGULATOR_PROTOCOL NVIDIA_REGULATOR_PROTOCOL;

typedef struct {
  BOOLEAN     IsEnabled;
  BOOLEAN     AlwaysEnabled;
  BOOLEAN     IsAvailable;
  UINTN       CurrentMicrovolts;
  UINTN       MinMicrovolts;
  UINTN       MaxMicrovolts;
  UINTN       MicrovoltStep;
  CONST CHAR8 *Name;
} REGULATOR_INFO;

/**
  This function gets information about the specified regulator.

  @param[in]     This                The instance of the NVIDIA_REGULATOR_PROTOCOL.
  @param[in]     RegulatorId         Id of the regulator.
  @param[out]    RegulatorInfo       Pointer that will contain the regulator info

  @return EFI_SUCCESS                Regulator info returned.
  @return EFI_NOT_FOUND              Regulator is not supported on target.
  @return EFI_DEVICE_ERROR           Other error occured.
**/
typedef
EFI_STATUS
(EFIAPI *REGULATOR_GET_INFO) (
  IN  NVIDIA_REGULATOR_PROTOCOL  *This,
  IN  UINT32                     RegulatorId,
  OUT REGULATOR_INFO             *RegulatorInfo
  );

/**
  This function gets the regulator id from the name specified

  @param[in]     This                The instance of the NVIDIA_REGULATOR_PROTOCOL.
  @param[in]     Name                Name of the regulator
  @param[out]    RegulatorId         Pointer to the id of the regulator.

  @return EFI_SUCCESS                Regulator id returned.
  @return EFI_NOT_FOUND         Pointer to the i     Regulator is not supported on target.
  @return EFI_DEVICE_ERROR           Other error occured.
**/
typedef
EFI_STATUS
(EFIAPI *REGULATOR_GET_ID_FROM_NAME) (
  IN  NVIDIA_REGULATOR_PROTOCOL  *This,
  IN  CONST CHAR8                *Name,
  OUT UINT32                     *RegulatorId
  );

/**
 * This function retrieves the ids for all the regulators on the system
 *
  @param[in]      This               The instance of the NVIDIA_REGULATOR_PROTOCOL.
 * @param[in,out] BufferSize         On input size of Regulator Id buffer in bytes, on output size required.
 * @param[out]    RegulatorIds       Pointer to array of regulator ids to fill out
 *
 * @return EFI_SUCCESS               Regulators returned
 * @return EFI_INVALID_PARAMETER     BufferSize is not 0 but RegulatorIds is NULL
 * @return EFI_BUFFER_TOO_SMALL      BufferSize is to small on input for supported regulators.
 */
typedef
EFI_STATUS
(EFIAPI *REGULATOR_GET_REGULATORS) (
  IN     NVIDIA_REGULATOR_PROTOCOL  *This,
  IN OUT UINTN                      *BufferSize,
  OUT    UINT32                     *RegulatorIds OPTIONAL
  );

/**
 * This function notifies the caller of that the state of the regulator has changed.
 * This can be for regulator being made available or change in enable status or voltage.
 *
 * @param[in] This                   The instance of the NVIDIA_REGULATOR_PROTOCOL.
 * @param[in] RegulatorId            Id of the regulator to be notified on.
 * @param[in] Event                  Event that will be signaled when regulator state changes.
 *
 * @return EFI_SUCCESS               Registration was successful.
 * @return EFI_NOT_FOUND             Regulator Id not supported.
 * @return EFI_DEVICE_ERROR          Notification registration failed.
 */
typedef
EFI_STATUS
(EFIAPI *REGULATOR_NOTIFY_STATE_CHANGE) (
  IN NVIDIA_REGULATOR_PROTOCOL  *This,
  IN UINT32                     RegulatorId,
  IN EFI_EVENT                  Event
  );

/**
 * This function enables or disables the specified regulator.
 *
 * @param[in] This                   The instance of the NVIDIA_REGULATOR_PROTOCOL.
 * @param[in] RegulatorId            Id of the regulator to be enable/disable.
 * @param[in] Enable                 TRUE to enable, FALSE to disable.
 *
 * @return EFI_SUCCESS               Regulator state change occurred.
 * @return EFI_NOT_FOUND             Regulator Id not supported.
 * @return EFI_UNSUPPORTED           Regulator does not support being enabled or disabled
 * @return EFI_DEVICE_ERROR          Other error occurred.
 */
typedef
EFI_STATUS
(EFIAPI *REGULATOR_ENABLE) (
  IN NVIDIA_REGULATOR_PROTOCOL  *This,
  IN UINT32                     RegulatorId,
  IN BOOLEAN                    Enable
  );

/**
 * This function sets the voltage the specified regulator.
 *
 * @param[in] This                   The instance of the NVIDIA_REGULATOR_PROTOCOL.
 * @param[in] RegulatorId            Id of the regulator to set voltage on.
 * @param[in] Microvolts             Voltage in microvolts.
 *
 * @return EFI_SUCCESS               Regulator state change occurred.
 * @return EFI_NOT_FOUND             Regulator Id not supported.
 * @return EFI_UNSUPPORTED           Regulator does not support voltage change.
 * @return EFI_INVALID_PARAMETER     Voltage specified is not supported.
 * @return EFI_DEVICE_ERROR          Other error occurred.
 */
typedef
EFI_STATUS
(EFIAPI *REGULATOR_SET_VOLTAGE) (
  IN NVIDIA_REGULATOR_PROTOCOL  *This,
  IN UINT32                     RegulatorId,
  IN UINTN                      Microvolts
  );

/// NVIDIA_REGULATOR_PROTOCOL protocol structure.
struct _NVIDIA_REGULATOR_PROTOCOL {

  REGULATOR_GET_INFO            GetInfo;
  REGULATOR_GET_ID_FROM_NAME    GetIdFromName;
  REGULATOR_GET_REGULATORS      GetRegulators;
  REGULATOR_NOTIFY_STATE_CHANGE NotifyStateChange;
  REGULATOR_ENABLE              Enable;
  REGULATOR_SET_VOLTAGE         SetVoltage;
};

extern EFI_GUID gNVIDIARegulatorProtocolGuid;

#endif
