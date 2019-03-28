/** @file
  Random number generator services that uses SE AES operations to provide
  to provide high-quality random numbers.

Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Protocol/SeRngProtocol.h>
#include <Library/UefiBootServicesTableLib.h>


STATIC NVIDIA_SE_RNG_PROTOCOL *mRngProtocol = NULL;

/**
  The constructor function checks for existence of SE RNG protocol.

  @retval EFI_SUCCESS   The constructor returns EFI_SUCCESS if protocol was found.
  @retval others        Locate protocol failure

**/
EFI_STATUS
EFIAPI
SeRngLibConstructor (
  VOID
  )
{
  EFI_STATUS Status;

  Status = gBS->LocateProtocol (&gNVIDIASeRngProtocolGuid, NULL, (VOID **)&mRngProtocol);

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to locate RNG protocol (%r)\r\n",__FUNCTION__,Status));
  }
  return Status;
}

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
  OUT     UINT16                    *Rand
  )
{
  EFI_STATUS Status;
  UINT64 Random[2];

  ASSERT (Rand != NULL);

  Status = mRngProtocol->GetRandom128 (mRngProtocol, Random);

  if (!EFI_ERROR(Status)) {
    CopyMem (Rand, Random, sizeof (*Rand));
  }
  return !EFI_ERROR (Status);
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
  OUT     UINT32                    *Rand
  )
{
  EFI_STATUS Status;
  UINT64 Random[2];

  ASSERT (Rand != NULL);

  Status = mRngProtocol->GetRandom128 (mRngProtocol, Random);

  if (!EFI_ERROR(Status)) {
    CopyMem (Rand, Random, sizeof (*Rand));
  }
  return !EFI_ERROR (Status);
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
  OUT     UINT64                    *Rand
  )
{
  EFI_STATUS Status;
  UINT64 Random[2];

  ASSERT (Rand != NULL);

  Status = mRngProtocol->GetRandom128 (mRngProtocol, Random);

  if (!EFI_ERROR(Status)) {
    CopyMem (Rand, Random, sizeof (*Rand));
  }
  return !EFI_ERROR (Status);
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
  OUT     UINT64                    *Rand
  )
{
  EFI_STATUS Status;

  ASSERT (Rand != NULL);

  Status = mRngProtocol->GetRandom128 (mRngProtocol, Rand);

  return !EFI_ERROR (Status);
}
