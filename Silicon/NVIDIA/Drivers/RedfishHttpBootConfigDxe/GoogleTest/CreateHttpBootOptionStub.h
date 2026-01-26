/** @file
  Stub implementations for CreateHttpBootOption tests.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CREATE_HTTP_BOOT_OPTION_STUB_H_
#define CREATE_HTTP_BOOT_OPTION_STUB_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
  Set the result that FindNicByMac should return.

  @param[in]  Status     Status to return from FindNicByMac
  @param[in]  NicHandle  Handle to return (if Status is EFI_SUCCESS)

**/
VOID
SetStubFindNicByMac (
  EFI_STATUS  Status,
  EFI_HANDLE  NicHandle
  );

/**
  Clear the configured result for FindNicByMac.

**/
VOID
ClearStubFindNicByMac (
  VOID
  );

/**
  Set the result that BuildHttpBootDevicePath should return.

  @param[in]  Status      Status to return from BuildHttpBootDevicePath
  @param[in]  DevicePath  Device path to return (if Status is EFI_SUCCESS)

**/
VOID
SetStubBuildHttpBootDevicePath (
  EFI_STATUS                Status,
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  );

/**
  Clear the configured result for BuildHttpBootDevicePath.

**/
VOID
ClearStubBuildHttpBootDevicePath (
  VOID
  );

#ifdef __cplusplus
}
#endif

#endif // CREATE_HTTP_BOOT_OPTION_STUB_H_
