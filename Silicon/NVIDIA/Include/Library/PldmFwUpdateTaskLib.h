/** @file

  PLDM FW update task lib

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PLDM_FW_UPDATE_TASK_LIB_H__
#define __PLDM_FW_UPDATE_TASK_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Protocol/FirmwareManagement.h>
#include <Protocol/MctpProtocol.h>

typedef enum {
  PLDM_FW_UPDATE_TASK_ERROR_NONE,
  PLDM_FW_UPDATE_TASK_ERROR_QUERY_DEVICE_IDS_FAILED,
  PLDM_FW_UPDATE_TASK_ERROR_NO_FD_MATCH_IN_PKG,
  PLDM_FW_UPDATE_TASK_ERROR_GET_FW_PARAMS_FAILED,
  PLDM_FW_UPDATE_TASK_ERROR_GET_FW_PARAMS_BUFFER_TOO_SMALL,
  PLDM_FW_UPDATE_TASK_ERROR_NO_UPDATE_COMPONENTS,
  PLDM_FW_UPDATE_TASK_ERROR_REQUEST_UPDATE_FAILED,
  PLDM_FW_UPDATE_TASK_ERROR_REQUEST_UPDATE_UNSUPPORTED,
  PLDM_FW_UPDATE_TASK_ERROR_PASS_COMPONENT_TABLE_FAILED,
  PLDM_FW_UPDATE_TASK_ERROR_PASS_COMPONENT_TABLE_BAD_RSP,
  PLDM_FW_UPDATE_TASK_ERROR_UPDATE_COMPONENT_FAILED,
  PLDM_FW_UPDATE_TASK_ERROR_UPDATE_COMPONENT_WILL_NOT_UPDATE,
  PLDM_FW_UPDATE_TASK_ERROR_TRANSFER_COMPLETE_BAD_LEN,
  PLDM_FW_UPDATE_TASK_ERROR_TRANSFER_COMPLETE_RESULT_ERR,
  PLDM_FW_UPDATE_TASK_ERROR_VERIFY_COMPLETE_BAD_LEN,
  PLDM_FW_UPDATE_TASK_ERROR_VERIFY_COMPLETE_FAILED,
  PLDM_FW_UPDATE_TASK_ERROR_APPLY_COMPLETE_BAD_LEN,
  PLDM_FW_UPDATE_TASK_ERROR_APPLY_COMPLETE_FAILED,
  PLDM_FW_UPDATE_TASK_ERROR_ACTIVATE_FW_FAILED,
  PLDM_FW_UPDATE_TASK_ERROR_REQUEST_FW_DATA_TIMEOUT,
  PLDM_FW_UPDATE_TASK_ERROR_RECEIVE_FAILED,
  PLDM_FW_UPDATE_TASK_ERROR_RECEIVE_BAD_LEN,
  PLDM_FW_UPDATE_TASK_ERROR_RECEIVE_BAD_TYPE,
  PLDM_FW_UPDATE_TASK_ERROR_UNSUPPORTED_CMD,
  PLDM_FW_UPDATE_TASK_ERROR_SEND_REQ_FAILED,
  PLDM_FW_UPDATE_TASK_ERROR_REQ_RETRIES_EXHAUSTED,

  PLDM_FW_UPDATE_TASK_ERROR_MAX
} PLDM_FW_UPDATE_TASK_ERROR;

typedef
EFI_STATUS
(EFIAPI *PLDM_FW_UPDATE_TASK_PROGRESS)(
  IN UINTN Completion
  );

/**
  Create FW update task for Firmware Device.

  @param[in]  FD                Firmware Device to update.
  @param[in]  Package           Pointer to PLDM package.
  @param[in]  Length            Length of package in bytes.

  @retval EFI_SUCCESS           Operation completed normally.
  @retval Others                Failure occurred.

**/
EFI_STATUS
EFIAPI
PldmFwUpdateTaskCreate (
  IN  NVIDIA_MCTP_PROTOCOL  *FD,
  IN  CONST VOID            *Package,
  IN  UINTN                 Length
  );

/**
  Execute all FW update tasks previously created.

  @param[out]  Error                Pointer to save error code.
  @param[out]  ActivationMethod     Pointer to save activation method.

  @retval EFI_SUCCESS               Operation completed normally.
  @retval Others                    Failure occurred.

**/
EFI_STATUS
EFIAPI
PldmFwUpdateTaskExecuteAll (
  OUT PLDM_FW_UPDATE_TASK_ERROR  *Error,
  OUT UINT16                     *ActivationMethod
  );

/**
  Initialize FW update task library.

  @param[in]  NumDevices            Number of firmware devices.
  @param[in]  ProgressFunction      Progress update function OPTIONAL

  @retval EFI_SUCCESS               Operation completed normally.
  @retval Others                    Failure occurred.

**/
EFI_STATUS
EFIAPI
PldmFwUpdateTaskLibInit (
  IN  UINTN                         NumDevices,
  IN  PLDM_FW_UPDATE_TASK_PROGRESS  ProgressFunction OPTIONAL
  );

#endif
