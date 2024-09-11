/** @file
  Random number generator that gets the RNG via the ARM TrngLib.

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "Base.h"
#include "NvRngProto.h"
#include "ProcessorBind.h"
#include "Uefi/UefiBaseType.h"
#include <Library/ArmTrngLib.h>

STATIC NVIDIA_NVRNG_PROTOCOL  HwRngOps;

/**
  Generate high-quality entropy source using a TRNG or through RDRAND.

  @param[in]   Length        Size of the buffer, in bytes, to fill with.
  @param[out]  Entropy       Pointer to the buffer to store the entropy data.

  @retval  RETURN_SUCCESS            The function completed successfully.
  @retval  RETURN_INVALID_PARAMETER  Invalid parameter.
  @retval  RETURN_UNSUPPORTED        Function not implemented.
  @retval  RETURN_BAD_BUFFER_SIZE    Buffer size is too small.
  @retval  RETURN_NOT_READY          No Entropy available.
**/
STATIC
EFI_STATUS
EFIAPI
GenerateEntropy (
  IN  UINTN  Length,
  OUT UINT8  *Entropy
  )
{
  EFI_STATUS  Status;
  UINTN       CollectedEntropyBits;
  UINTN       RequiredEntropyBits;
  UINTN       EntropyBits;
  UINTN       Index;
  UINTN       MaxBits;

  ZeroMem (Entropy, Length);

  RequiredEntropyBits  = (Length << 3);
  Index                = 0;
  CollectedEntropyBits = 0;
  MaxBits              = GetArmTrngMaxSupportedEntropyBits ();
  Status               = RETURN_NOT_READY;
  while (CollectedEntropyBits < RequiredEntropyBits) {
    EntropyBits = MIN ((RequiredEntropyBits - CollectedEntropyBits), MaxBits);
    Status      = GetArmTrngEntropy (
                    EntropyBits,
                    (Length - Index),
                    &Entropy[Index]
                    );
    if (EFI_ERROR (Status)) {
      // Discard the collected bits.
      ZeroMem (Entropy, Length);
      return Status;
    }

    CollectedEntropyBits += EntropyBits;
    Index                += (EntropyBits >> 3);
  } // while

  return Status;
}

/**
  Get a 16-bit random number from the RNG driver in StMM.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 16-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
STATIC
BOOLEAN
HwGetRandomNumber16 (
  OUT     UINT16  *Rand
  )
{
  EFI_STATUS  Status;
  UINT8       *RndPtr;
  BOOLEAN     Ret;

  ASSERT (Rand != NULL);
  RndPtr = (UINT8 *)Rand;

  Ret    = TRUE;
  Status = GenerateEntropy (sizeof (UINT16), RndPtr);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to get Entropy %r\n", __FUNCTION__, Status));
    Ret = FALSE;
  }

  return Ret;
}

/**
  Get a 32-bit random number from RNG Driver in StMM.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 32-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
STATIC
BOOLEAN
HwGetRandomNumber32 (
  OUT     UINT32  *Rand
  )
{
  EFI_STATUS  Status;
  UINT8       *RndPtr;
  BOOLEAN     Ret;

  ASSERT (Rand != NULL);
  RndPtr = (UINT8 *)Rand;

  Ret    = TRUE;
  Status = GenerateEntropy (sizeof (UINT32), RndPtr);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to get Entropy %r\n", __FUNCTION__, Status));
    Ret = FALSE;
  }

  return Ret;
}

/**
  Get a 64-bit random number from the RNG driver in StMM.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 64-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
STATIC
BOOLEAN
HwGetRandomNumber64 (
  OUT     UINT64  *Rand
  )
{
  EFI_STATUS  Status;
  UINT8       *RndPtr;
  BOOLEAN     Ret;

  ASSERT (Rand != NULL);
  RndPtr = (UINT8 *)Rand;

  Ret    = TRUE;
  Status = GenerateEntropy (sizeof (UINT64), RndPtr);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to get Entropy %r\n", __FUNCTION__, Status));
    Ret = FALSE;
  }

  return Ret;
}

/**
  Get 128-bit random number from RNG driver in StMM.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 128-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
STATIC
BOOLEAN
HwGetRandomNumber128 (
  OUT     UINT64  *Rand
  )
{
  ASSERT (Rand != NULL);

  if (!HwGetRandomNumber64 (Rand)) {
    return FALSE;
  }

  return HwGetRandomNumber64 (++Rand);
}

/**
  Get a GUID identifying the RNG algorithm implementated by NVRNG IP.

  @param [out] RngGuid    RNG algorithm implementation.

  @retval EFI_SUCCESS             Success.
          EFI_INVALID_PARAMETER   Null Parameter value.
**/
STATIC
EFI_STATUS
HwGetRngGuid (
  GUID  *RngGuid
  )
{
  if (RngGuid == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CopyGuid (RngGuid, &gEfiRngAlgorithmRaw);
  return EFI_SUCCESS;
}

/**
 * Get the Rng Ops if using the StMM based RNG Driver.
 *
 * @param[in]      None.
 *
 * @retval  Pointer to RNG Ops on success.
 *          NULL on failure.
 **/
NVIDIA_NVRNG_PROTOCOL *
HwRngGetOps (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT16      MajorRev;
  UINT16      MinorRev;

  Status = GetArmTrngVersion (&MajorRev, &MinorRev);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a %d Failed to get Trng Version %r\n", __FUNCTION__, __LINE__, Status));
    return NULL;
  }

  HwRngOps.NvGetRng16   = HwGetRandomNumber16;
  HwRngOps.NvGetRng32   = HwGetRandomNumber32;
  HwRngOps.NvGetRng64   = HwGetRandomNumber64;
  HwRngOps.NvGetRng128  = HwGetRandomNumber128;
  HwRngOps.NvGetRngGuid = HwGetRngGuid;

  return &HwRngOps;
}
