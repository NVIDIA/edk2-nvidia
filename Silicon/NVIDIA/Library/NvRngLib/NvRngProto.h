/** @file
 Header file defining the RNG ops that both RNG methods can use.

 SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVIDIA_NVRNG_PROTOCOL_H__
#define __NVIDIA_NVRNG_PROTOCOL_H__

#include "Base.h"
#include "ProcessorBind.h"
#include "Uefi/UefiBaseType.h"
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>

typedef struct _NVIDIA_NVRNG_PROTOCOL NVIDIA_NVRNG_PROTOCOL;

typedef
BOOLEAN
(EFIAPI *GETRNG16)(
  IN UINT16 *Rng
  );

typedef
BOOLEAN
(EFIAPI *GETRNG32)(
  IN UINT32 *Rng
  );

typedef
BOOLEAN
(EFIAPI *GETRNG64)(
  IN UINT64 *Rng
  );

typedef
BOOLEAN
(EFIAPI *GETRNG128)(
  IN UINT64 *Rng
  );

typedef
EFI_STATUS
(EFIAPI *GETRNGGUID)(
  OUT GUID *Guid
  );

struct _NVIDIA_NVRNG_PROTOCOL {
  GETRNG16      NvGetRng16;
  GETRNG32      NvGetRng32;
  GETRNG64      NvGetRng64;
  GETRNG128     NvGetRng128;
  GETRNGGUID    NvGetRngGuid;
};

NVIDIA_NVRNG_PROTOCOL *
HwRngGetOps (
  VOID
  );

NVIDIA_NVRNG_PROTOCOL *
NonHwRngGetOps (
  VOID
  );

#endif
