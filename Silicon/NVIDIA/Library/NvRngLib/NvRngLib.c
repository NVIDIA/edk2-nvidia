/** @file
 Rng Lib that gets the RNG from a secure RNG driver in StMM or from a Non-Safe
 Rng Source if MM isn't present.

 SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "NvRngProto.h"

STATIC NVIDIA_NVRNG_PROTOCOL  *RngOps = NULL;

/**
  Generates a 16-bit random number.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 16-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber16 (
  OUT     UINT16  *Rand
  )
{
  return RngOps->NvGetRng16 (Rand);
}

/**
  Generates a 32-bit random number.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 32-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber32 (
  OUT     UINT32  *Rand
  )
{
  return RngOps->NvGetRng32 (Rand);
}

/**
  Generates a 64-bit random number.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 64-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber64 (
  OUT     UINT64  *Rand
  )
{
  return RngOps->NvGetRng64 (Rand);
}

/**
  Generates a 128-bit random number.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 128-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber128 (
  OUT     UINT64  *Rand
  )
{
  return RngOps->NvGetRng128 (Rand);
}

/**
  Get a GUID identifying the RNG algorithm implementation.

  @param [out] RngGuid  If success, contains the GUID identifying
                        the RNG algorithm implementation.

  @retval EFI_SUCCESS             Success.
  @retval EFI_UNSUPPORTED         Not supported.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
GetRngGuid (
  GUID  *RngGuid
  )
{
  return RngOps->NvGetRngGuid (RngGuid);
}

EFI_STATUS
EFIAPI
NvRngLibConstructor (
  VOID
  )
{
  RngOps = HwRngGetOps ();
  if (RngOps == NULL) {
    RngOps = NonHwRngGetOps ();
    DEBUG ((DEBUG_ERROR, "%a: No StMM Using NonHW RngLib\n", __FUNCTION__));
  } else {
    DEBUG ((DEBUG_INFO, "%a: Using HW RngLib\n", __FUNCTION__));
  }

  return EFI_SUCCESS;
}
