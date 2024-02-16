/** @file
*  Provides private header functions for MpCoreLib
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef MPCOREINFO_LIB_PRIVATE_H_
#define MPCOREINFO_LIB_PRIVATE_H_

#include <PiDxe.h>

// Utility function to reset library globals
EFIAPI
VOID
MpCoreInfoLibResetModule (
  VOID
  );

#endif
