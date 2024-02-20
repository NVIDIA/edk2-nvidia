/** @file
  Static Locality Information Table Parser

  SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef SLIT_PARSER_H_
#define SLIT_PARSER_H_

#include "HbmParser.h"

// Normalized Distances
#define NORMALIZED_LOCAL_DISTANCE              PcdGet32 (PcdLocalDistance)
#define NORMALIZED_UNREACHABLE_DISTANCE        PcdGet32 (PcdUnreachableDistance)
#define NORMALIZED_CPU_TO_REMOTE_CPU_DISTANCE  PcdGet32 (PcdCpuToRemoteCpuDistance)
#define NORMALIZED_GPU_TO_REMOTE_GPU_DISTANCE  PcdGet32 (PcdGpuToRemoteGpuDistance)
#define NORMALIZED_CPU_TO_LOCAL_HBM_DISTANCE   PcdGet32 (PcdCpuToLocalHbmDistance)
#define NORMALIZED_CPU_TO_REMOTE_HBM_DISTANCE  PcdGet32 (PcdCpuToRemoteHbmDistance)
#define NORMALIZED_HBM_TO_LOCAL_CPU_DISTANCE   PcdGet32 (PcdHbmToLocalCpuDistance)
#define NORMALIZED_HBM_TO_REMOTE_CPU_DISTANCE  PcdGet32 (PcdHbmToRemoteCpuDistance)
#define NORMALIZED_GPU_TO_LOCAL_HBM_DISTANCE   PcdGet32 (PcdGpuToLocalHbmDistance)
#define NORMALIZED_GPU_TO_REMOTE_HBM_DISTANCE  PcdGet32 (PcdGpuToRemoteHbmDistance)
#define NORMALIZED_HBM_TO_LOCAL_GPU_DISTANCE   PcdGet32 (PcdHbmToLocalGpuDistance)
#define NORMALIZED_HBM_TO_REMOTE_GPU_DISTANCE  PcdGet32 (PcdHbmToRemoteGpuDistance)

/** SLIT parser function.

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.
  @param [in]  ParserHandle A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.
  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
SlitParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

#endif
