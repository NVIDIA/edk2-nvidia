/** @file
  Host-build wrapper for Dbg2NetworkParser.c.

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

extern EFI_GUID  gNVIDIAPublicVariableGuid;
extern EFI_GUID  gEfiPciIoProtocolGuid;

#include "../Dbg2NetworkParser.c"
