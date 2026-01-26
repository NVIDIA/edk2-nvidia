/** @file
  Formset definitions for Redfish HTTP Boot Configuration VFR.

  This header contains only the definitions required by the VFR compiler.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef REDFISH_HTTP_BOOT_CONFIG_FORMSET_H_
#define REDFISH_HTTP_BOOT_CONFIG_FORMSET_H_

#include "RedfishHttpBootConfigVfrDefs.h"

//
// Formset GUID
//
#define REDFISH_HTTP_BOOT_CONFIG_FORMSET_GUID \
  { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90 } }

//
// Variable store ID
//
#define HTTP_BOOT_URI_VARSTORE_ID  1

//
// Form IDs
//
#define FORM_ID_HTTP_BOOT_CONFIG  0x0001

//
// Question IDs
//
#define KEY_HTTP_BOOT_URI  0x1000

#endif // REDFISH_HTTP_BOOT_CONFIG_FORMSET_H_
