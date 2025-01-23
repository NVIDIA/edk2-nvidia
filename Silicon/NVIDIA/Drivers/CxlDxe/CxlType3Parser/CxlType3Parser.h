/** @file
  CXL Type 3 parser header.

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef CXL_TYPE3_PARSER_H_
#define CXL_TYPE3_PARSER_H_

#include <Protocol/PciIo.h>

/**
  Function to parse a Type 3 CXL device.

  @param  [in]  Controller        The Controller handle of the device.
  @param  [in]  PciIo             The PciIo protocol of the device.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
CxlType3 (
  IN  EFI_HANDLE           Controller,
  IN  EFI_PCI_IO_PROTOCOL  *PciIo
  );

#endif /* CXL_TYPE3_PARSER_H_ */
