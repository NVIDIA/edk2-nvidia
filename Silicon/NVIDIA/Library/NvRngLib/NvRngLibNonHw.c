/** @file
  BaseRng Library that uses the TimerLib to provide reasonably random numbers.
  Do not use this on a production system.
  This is a copy of the Library in BaseRngLibTimerLib

  Copyright (c) 2023, Arm Limited. All rights reserved.
  Copyright (c) Microsoft Corporation.
  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvRngProto.h"
#include <Library/BaseLib.h>
#include <Library/TimerLib.h>
#include <Guid/RngAlgorithm.h>

STATIC NVIDIA_NVRNG_PROTOCOL  NonHwRngOps;
#define DEFAULT_DELAY_TIME_IN_MICROSECONDS  10

/**
 Using the TimerLib GetPerformanceCounterProperties() we delay
 for enough time for the PerformanceCounter to increment.

 If the return value from GetPerformanceCounterProperties (TimerLib)
 is zero, this function will return 10 and attempt to assert.
 **/
STATIC
UINT32
CalculateMinimumDecentDelayInMicroseconds (
  VOID
  )
{
  UINT64  CounterHz;

  // Get the counter properties
  CounterHz = GetPerformanceCounterProperties (NULL, NULL);
  // Make sure we won't divide by zero
  if (CounterHz == 0) {
    ASSERT (CounterHz != 0); // Assert so the developer knows something is wrong
    return DEFAULT_DELAY_TIME_IN_MICROSECONDS;
  }

  // Calculate the minimum delay based on 1.5 microseconds divided by the hertz.
  // We calculate the length of a cycle (1/CounterHz) and multiply it by 1.5 microseconds
  // This ensures that the performance counter has increased by at least one
  return (UINT32)(MAX (DivU64x64Remainder (1500000, CounterHz, NULL), 1));
}

/**
  Generates a 16-bit random number from a non HW RNG generator.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 16-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
STATIC
BOOLEAN
NonHwGetRandomNumber16 (
  OUT     UINT16  *Rand
  )
{
  UINT32  Index;
  UINT8   *RandPtr;
  UINT32  DelayInMicroSeconds;

  ASSERT (Rand != NULL);

  if (Rand == NULL) {
    return FALSE;
  }

  DelayInMicroSeconds = CalculateMinimumDecentDelayInMicroseconds ();
  RandPtr             = (UINT8 *)Rand;
  // Get 2 bytes of random ish data
  for (Index = 0; Index < sizeof (UINT16); Index++) {
    *RandPtr = (UINT8)(GetPerformanceCounter () & 0xFF);
    // Delay to give the performance counter a chance to change
    MicroSecondDelay (DelayInMicroSeconds);
    RandPtr++;
  }

  return TRUE;
}

/**
  Generates a 32-bit random number from a non HW RNG generator.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 32-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
STATIC
BOOLEAN
NonHwGetRandomNumber32 (
  OUT     UINT32  *Rand
  )
{
  UINT32  Index;
  UINT8   *RandPtr;
  UINT32  DelayInMicroSeconds;

  ASSERT (Rand != NULL);

  if (NULL == Rand) {
    return FALSE;
  }

  RandPtr             = (UINT8 *)Rand;
  DelayInMicroSeconds = CalculateMinimumDecentDelayInMicroseconds ();
  // Get 4 bytes of random ish data
  for (Index = 0; Index < sizeof (UINT32); Index++) {
    *RandPtr = (UINT8)(GetPerformanceCounter () & 0xFF);
    // Delay to give the performance counter a chance to change
    MicroSecondDelay (DelayInMicroSeconds);
    RandPtr++;
  }

  return TRUE;
}

/**
  Generates a 64-bit random number from a non HW RNG generator.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 64-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
STATIC
BOOLEAN
NonHwGetRandomNumber64 (
  OUT     UINT64  *Rand
  )
{
  UINT32  Index;
  UINT8   *RandPtr;
  UINT32  DelayInMicroSeconds;

  ASSERT (Rand != NULL);

  if (NULL == Rand) {
    return FALSE;
  }

  RandPtr             = (UINT8 *)Rand;
  DelayInMicroSeconds = CalculateMinimumDecentDelayInMicroseconds ();
  // Get 8 bytes of random ish data
  for (Index = 0; Index < sizeof (UINT64); Index++) {
    *RandPtr = (UINT8)(GetPerformanceCounter () & 0xFF);
    // Delay to give the performance counter a chance to change
    MicroSecondDelay (DelayInMicroSeconds);
    RandPtr++;
  }

  return TRUE;
}

/**
  Generates a 128-bit random number from a non HW RNG generator.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 128-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
STATIC
BOOLEAN
NonHwGetRandomNumber128 (
  OUT     UINT64  *Rand
  )
{
  ASSERT (Rand != NULL);
  // This should take around 80ms

  // Read first 64 bits
  if (!NonHwGetRandomNumber64 (Rand)) {
    return FALSE;
  }

  // Read second 64 bits
  return NonHwGetRandomNumber64 (++Rand);
}

/**
  Get a GUID identifying the RNG algorithm implementation.

  @param [out] RngGuid  If success, contains the GUID identifying
                        the RNG algorithm implementation.

  @retval EFI_SUCCESS             Success.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
NonHwGetRngGuid (
  GUID  *RngGuid
  )
{
  if (RngGuid == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (RngGuid, &gEdkiiRngAlgorithmUnSafe, sizeof (*RngGuid));
  return EFI_SUCCESS;
}

/**
 * Get the Rng Ops if using the Unsafe NonHW Rng Generator.
 *
 * @param[in]      None.
 *
 * @retval  Pointer to RNG Ops on success.
 *          NULL on failure.
 **/
NVIDIA_NVRNG_PROTOCOL *
NonHwRngGetOps (
  VOID
  )
{
  NonHwRngOps.NvGetRng16   = NonHwGetRandomNumber16;
  NonHwRngOps.NvGetRng32   = NonHwGetRandomNumber32;
  NonHwRngOps.NvGetRng64   = NonHwGetRandomNumber64;
  NonHwRngOps.NvGetRng128  = NonHwGetRandomNumber128;
  NonHwRngOps.NvGetRngGuid = NonHwGetRngGuid;

  return &NonHwRngOps;
}
