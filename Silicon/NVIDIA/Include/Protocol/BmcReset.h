/** @file
  NVIDIA BMC Reset Protocol

  This protocol provides an interface to perform BMC factory reset via Redfish.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef BMC_RESET_PROTOCOL_H_
#define BMC_RESET_PROTOCOL_H_

#include <Uefi.h>

//
// BMC Reset Protocol GUID
// {5E8F4B2D-3C7A-4E9F-8D1B-0A6C3E5F7890}
//
#define NVIDIA_BMC_RESET_PROTOCOL_GUID \
  { 0x5e8f4b2d, 0x3c7a, 0x4e9f, { 0x8d, 0x1b, 0x0a, 0x6c, 0x3e, 0x5f, 0x78, 0x90 } }

typedef struct _NVIDIA_BMC_RESET_PROTOCOL NVIDIA_BMC_RESET_PROTOCOL;

/**
  Perform BMC factory reset with provided credentials.

  Credentials are used immediately and zeroed after use. They are never
  stored persistently.

  @param[in]  This      Pointer to NVIDIA_BMC_RESET_PROTOCOL instance.
  @param[in]  Username  BMC username (ASCII, null-terminated). Caller retains ownership.
  @param[in]  Password  BMC password (ASCII, null-terminated). Caller retains ownership.

  @retval EFI_SUCCESS             BMC reset command sent successfully.
  @retval EFI_NOT_READY           Redfish service not available.
  @retval EFI_INVALID_PARAMETER   Username or Password is NULL.
  @retval Others                  Error occurred.
**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_BMC_RESET_FACTORY_RESET)(
  IN NVIDIA_BMC_RESET_PROTOCOL  *This,
  IN CONST CHAR8                *Username,
  IN CONST CHAR8                *Password
  );

/**
  Check if BMC reset service is available.

  @param[in]  This    Pointer to NVIDIA_BMC_RESET_PROTOCOL instance.

  @retval TRUE        Service is available.
  @retval FALSE       Service is not available.
**/
typedef
BOOLEAN
(EFIAPI *NVIDIA_BMC_RESET_IS_AVAILABLE)(
  IN NVIDIA_BMC_RESET_PROTOCOL  *This
  );

struct _NVIDIA_BMC_RESET_PROTOCOL {
  NVIDIA_BMC_RESET_FACTORY_RESET    FactoryReset;
  NVIDIA_BMC_RESET_IS_AVAILABLE     IsAvailable;
};

extern EFI_GUID  gNvidiaBmcResetProtocolGuid;

#endif // BMC_RESET_PROTOCOL_H_
