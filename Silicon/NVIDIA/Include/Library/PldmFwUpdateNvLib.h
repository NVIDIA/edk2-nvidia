/** @file

  PLDM FW update NVIDIA vendor-defined definitions and helper functions

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PLDM_FW_UPDATE_NV_LIB_H__
#define __PLDM_FW_UPDATE_NV_LIB_H__

#include <Library/PldmBaseLib.h>

// NVIDIA vendor-defined result codes
typedef enum {
  PLDM_FW_NV_TRANSFER_RESULT_SPI_REQUEST_FAILED  = 0x70,
  PLDM_FW_NV_TRANSFER_RESULT_SPI_WRITE_PROTECTED = 0x71,
  PLDM_FW_NV_TRANSFER_RESULT_INTERNAL_ERROR      = 0x72,
} PLDM_FW_NV_TRANSFER_RESULT;

typedef enum {
  PLDM_FW_NV_VERIFY_RESULT_IMAGE_SAME_AS_SPI              = 0x90,
  PLDM_FW_NV_VERIFY_RESULT_METADATA_AUTHENTICATION_FAILED = 0x91,
  PLDM_FW_NV_VERIFY_RESULT_ROLLBACK_CHECK_FAILED          = 0x93,
  PLDM_FW_NV_VERIFY_RESULT_KEY_REVOKE_CHECK_FAILED        = 0x94,
  PLDM_FW_NV_VERIFY_RESULT_AUTHENTICATION_FAILED          = 0x95,
  PLDM_FW_NV_VERIFY_RESULT_TRANSFER_INCOMPLETE            = 0x96,
  PLDM_FW_NV_VERIFY_RESULT_AP_SKU_MISMATCH                = 0x97,
  PLDM_FW_NV_VERIFY_RESULT_PACKAGE_SIZE_CHECK_FAILED      = 0x98,
} PLDM_FW_NV_VERIFY_RESULT;

typedef enum {
  PLDM_FW_NV_APPLY_RESULT_AP_AUTHENTICATION_FAILED = 0xb0,
} PLDM_FW_NV_APPLY_RESULT;

#endif
