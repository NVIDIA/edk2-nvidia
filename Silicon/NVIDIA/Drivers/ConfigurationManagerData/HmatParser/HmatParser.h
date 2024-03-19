/** @file
  Heterogeneous Memory Attribute Table (HMAT) Parser.

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef HBM_PARSER_H_
#define HBM_PARSER_H_

#include <Library/NvCmObjectDescUtility.h>

#define NORMALIZED_UNREACHABLE_LATENCY    0xFFFF
#define NORMALIZED_UNREACHABLE_BANDWIDTH  0x0

#define ENTRY_BASE_UNIT_NANO_SEC_TO_PICO_SEC  0x3E8
#define ENTRY_BASE_UNIT_GBPS_TO_MBPS          0x3E8

#define  READ_LATENCY_DATATYPE      1
#define  WRITE_LATENCY_DATATYPE     2
#define  ACCESS_BANDWIDTH_DATATYPE  3

UINT64
EFIAPI
GetSizeOfLatencyAndBandwidthInfoStruct (
  UINT32  NumInitProxDmns,
  UINT32  NumTarProxDmns
  );

VOID
EFIAPI
ObtainLatencyBandwidthInfo (
  UINT16  *ReadLatencyList,
  UINT16  *WriteLatencyList,
  UINT16  *AccessBandwidthList,
  UINT32  NumInitProxDmns,
  UINT32  NumTarProxDmns
  );

/** HMAT parser function.

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
HmatParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

#endif
