/** @file

  DXE Services Table Lib stub for host based tests. Only provides the gDS
  pointer symbol (initialised to NULL); host tests that link a module which
  references DXE Services but does not call them under test will link cleanly.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <PiDxe.h>
#include <Library/DxeServicesTableLib.h>

EFI_DXE_SERVICES  *gDS = NULL;
