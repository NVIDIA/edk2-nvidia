/** @file
*
*  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _INTERNAL_PLATFORM_BOOT_ORDER_IPMI_LIB_H_
#define _INTERNAL_PLATFORM_BOOT_ORDER_IPMI_LIB_H_

#include "InternalPlatformBootOrderLib.h"

// Moves the element at INDEX to the start of the ARRAY
#define MOVE_INDEX_TO_START(ARRAY, INDEX)                            \
do {                                                                 \
  __typeof__((ARRAY)[0]) Value;                                      \
  if ((INDEX) > 0) {                                                 \
    Value = (ARRAY)[(INDEX)];                                        \
    CopyMem (&((ARRAY)[1]), &((ARRAY)[0]), (INDEX)*sizeof(Value));   \
    (ARRAY)[0] = Value;                                              \
  }                                                                  \
} while (FALSE);

#define IPMI_GET_BOOT_OPTIONS_PARAMETER_INVALID  1
#define IPMI_PARAMETER_VERSION                   1

#define SAVED_BOOT_ORDER_VARIABLE_NAME        L"SavedBootOrder"
#define SAVED_BOOT_ORDER_FLAGS_VARIABLE_NAME  L"SavedBootOrderFlags"

#define SAVED_BOOT_ORDER_ALL_INSTANCES_FLAG  0x1
#define SAVED_BOOT_ORDER_VIRTUAL_FLAG        0x2

#endif
