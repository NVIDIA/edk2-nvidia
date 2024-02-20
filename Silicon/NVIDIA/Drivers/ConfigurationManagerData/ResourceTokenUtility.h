/** @file
  Resource token utility functions.

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef RESOURCE_TOKEN_UTILITY_H_
#define RESOURCE_TOKEN_UTILITY_H_

#include <ArmNameSpaceObjects.h>
#include <Library/HwInfoParserLib.h>

/** Creates Memory Range CM Object.

  Creates and registers a memory region CM object for a device.

  @param [in]  ParserHandle       A handle to the parser instance.
  @param [in]  NodeOffset         Offset of the node in device tree.
  @param [in]  ResourceMax        Maximum number of resources to add to CM object.
                                  0 for unlimited.
  @param [out] MemoryRanges       Optional pointer to return the rangess in the device.
  @param [out] MemoryRangeCount   Optional pointer to return number of ranges
  @param [out] Token              Optional pointer to return the token of the CM object

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
CreateMemoryRangesObject (
  IN  CONST HW_INFO_PARSER_HANDLE           ParserHandle,
  IN        INT32                           NodeOffset,
  IN        INT32                           ResourceMax,
  OUT       CM_ARM_MEMORY_RANGE_DESCRIPTOR  **MemoryRanges OPTIONAL,
  OUT       UINT32                          *MemoryRangeCount OPTIONAL,
  OUT       CM_OBJECT_TOKEN                 *Token OPTIONAL
  );

/** Creates Interrupts CM Object.

  Creates and registers a interrupts CM object for a device.

  @param [in]  ParserHandle       A handle to the parser instance.
  @param [in]  NodeOffset         Offset of the node in device tree.
  @param [in]  ResourceMax        Maximum number of resources to add to CM object.
                                  0 for unlimited.
  @param [out] Interrupts         Optional pointer to return the interrupts in the device.
  @param [out] InterruptCount     Optional pointer to return number of interrupts
  @param [out] Token              Optional pointer to return the token of the CM object

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
CreateInterruptsObject (
  IN  CONST HW_INFO_PARSER_HANDLE     ParserHandle,
  IN        INT32                     NodeOffset,
  IN        INT32                     ResourceMax,
  OUT       CM_ARM_GENERIC_INTERRUPT  *Interrupts OPTIONAL,
  OUT       UINT32                    *InterruptCount OPTIONAL,
  OUT       CM_OBJECT_TOKEN           *Token OPTIONAL
  );

#endif // RESOURCE_TOKEN_UTILITY_H_
