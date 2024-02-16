/** @file
  Nvidia's Configuration manager Object Descriptor Utility.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2021, ARM Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef NV_CM_OBJECT_DESC_UTILITY_PRIVATE_H_
#define NV_CM_OBJECT_DESC_UTILITY_PRIVATE_H_

#include <Library/HwInfoParserLib.h>

/** A structure describing the instance of the FdtHwInfoParser.
*/
typedef struct FdtHwInfoParser {
  /// Pointer to the HwDataSource i.e. the
  /// Flattened Device Tree (Fdt).
  VOID                  *Fdt;

  /// Pointer to the caller's context.
  VOID                  *Context;

  /// Function pointer called by the
  /// parser when adding information.
  HW_INFO_ADD_OBJECT    HwInfoAdd;
} FDT_HW_INFO_PARSER;

/** A pointer type for FDT_HW_INFO_PARSER.
*/
typedef FDT_HW_INFO_PARSER *FDT_HW_INFO_PARSER_HANDLE;

#endif // NV_CM_OBJECT_DESC_UTILITY_PRIVATE_H_
