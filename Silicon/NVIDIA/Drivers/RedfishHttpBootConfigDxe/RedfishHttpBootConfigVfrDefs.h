/** @file
  VFR-compatible definitions for Redfish HTTP Boot Configuration.
  This header contains ONLY definitions safe for VFR compilation.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef REDFISH_HTTP_BOOT_CONFIG_VFR_DEFS_H_
#define REDFISH_HTTP_BOOT_CONFIG_VFR_DEFS_H_

//
// GUID for HTTP Boot Configuration
//
#define NVIDIA_HTTP_BOOT_CONFIG_GUID \
  { 0x8c7d9a1e, 0x5f2b, 0x4a3d, { 0x9e, 0x1f, 0x6c, 0x8b, 0x4a, 0x2d, 0x7e, 0x9f } }

//
// Maximum HTTP URI length
// - RFC 7230 limits URIs to 2048, but we're limited by HII to 255
//
#define HTTP_BOOT_URI_MAX_SIZE  255

//
// HII variable storage structure
//
#pragma pack(1)
typedef struct {
  CHAR16    HttpBootUri[HTTP_BOOT_URI_MAX_SIZE];
} HTTP_BOOT_URI_STORAGE;
#pragma pack()

#endif // REDFISH_HTTP_BOOT_CONFIG_VFR_DEFS_H_
