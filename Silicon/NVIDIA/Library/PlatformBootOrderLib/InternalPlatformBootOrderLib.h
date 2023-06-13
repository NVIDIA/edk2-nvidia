/** @file
*
*  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _INTERNAL_PLATFORM_BOOT_ORDER_LIB_H_
#define _INTERNAL_PLATFORM_BOOT_ORDER_LIB_H_

#ifdef EDKII_UNIT_TEST_FRAMEWORK_ENABLED
  #undef DEBUG_ERROR
#define DEBUG_ERROR  DEBUG_INFO
  #undef DEBUG_WARN
#define DEBUG_WARN  DEBUG_INFO
#endif

#define FREE_NON_NULL(a) \
  if ((a) != NULL) { \
    FreePool ((a));  \
    (a) = NULL;      \
  }

#define NVIDIA_BOOT_TYPE_HTTP                    0
#define NVIDIA_BOOT_TYPE_BOOTIMG                 1
#define NVIDIA_BOOT_TYPE_VIRTUAL                 2
#define IPMI_GET_BOOT_OPTIONS_PARAMETER_INVALID  1
#define IPMI_PARAMETER_VERSION                   1

#define SAVED_BOOT_ORDER_VARIABLE_NAME  L"SavedBootOrder"

typedef struct {
  CHAR8    *OrderName;
  INT32    PriorityOrder;
  UINT8    Type;
  UINT8    SubType;
  UINT8    ExtraSpecifier;
  UINTN    SegmentNum;
  UINTN    BusNum;
  UINTN    DevNum;
  UINTN    FuncNum;
} NVIDIA_BOOT_ORDER_PRIORITY;

#define DEFAULT_BOOT_ORDER_STRING   "boot.img,usb,sd,emmc,ufs"
#define BOOT_ORDER_CLASS_SEPARATOR  ','
#define BOOT_ORDER_SBDF_SEPARATOR   ':'
#define BOOT_ORDER_SBDF_STARTER     '|'
#define BOOT_ORDER_TERMINATOR       '\0'

#endif
