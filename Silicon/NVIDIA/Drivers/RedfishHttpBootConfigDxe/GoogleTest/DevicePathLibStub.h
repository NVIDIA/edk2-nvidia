/** @file
  Stub implementation of DevicePathLib functions for unit tests.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef DEVICE_PATH_FROM_HANDLE_STUB_H_
#define DEVICE_PATH_FROM_HANDLE_STUB_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
  Set the device path that DevicePathFromHandle should return.

  @param[in]  DevicePath  Device path to return, or NULL to return NULL

**/
VOID
SetStubDevicePath (
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  );

/**
  Set the result that AppendDevicePathNode should return.

  @param[in]  DevicePath  Device path to return, or NULL to simulate allocation failure

**/
VOID
SetStubAppendDevicePathNodeResult (
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  );

/**
  Clear the configured result for AppendDevicePathNode.
  After this, AppendDevicePathNode will use its default behavior.

**/
VOID
ClearStubAppendDevicePathNodeResult (
  VOID
  );

/**
  Configure AppendDevicePathNode to fail on a specific call number.

  @param[in]  CallNumber  Call number to fail on (1-based), or 0 to disable

**/
VOID
SetStubAppendDevicePathNodeFailOnCall (
  UINTN  CallNumber
  );

#ifdef __cplusplus
}
#endif

#endif // DEVICE_PATH_FROM_HANDLE_STUB_H_
