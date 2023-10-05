/** @file

  Status register library

  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>

#define STATUS_REG_RESERVED         0x80000000
#define STATUS_REG_VERSION_MASK     0x70000000
#define STATUS_REG_VERSION_CURRENT  0x00000000
#define STATUS_REG_PHASE_MASK       0x0c000000
#define STATUS_REG_PHASE_PREPI      0x00000000
#define STATUS_REG_PHASE_DXE        0x08000000
#define STATUS_REG_PHASE_BDS        0x04000000
#define STATUS_REG_STATUS_MASK      0x03ffffff

// PREPI phase bits
#define STATUS_REG_PREPI_STARTED  0x00000001

/**
  Update status register with new phase.

  @param[in]  Phase                 New Phase.
  @param[in]  Bits                  Initial status bits.

  @retval None

**/
VOID
EFIAPI
StatusRegSetPhase (
  UINT32  Phase,
  UINT32  Bits
  );

/**
  Set status bits in status register.

  @param[in]  Bits                  Status bits to set.

  @retval None

**/
VOID
EFIAPI
StatusRegSetBits (
  UINT32  Bits
  );

/**
  Clear bits in status register.

  @param[in]  Bits                  Status bits to clear.

  @retval None

**/
VOID
EFIAPI
StatusRegClearBits (
  UINT32  Bits
  );

/**
  Get status register.

  @retval UINT32                    Current status register value.

**/
UINT32
EFIAPI
StatusRegGet (
  VOID
  );

/**
  Reset status register to 0.

  @retval None

**/
VOID
EFIAPI
StatusRegReset (
  VOID
  );
