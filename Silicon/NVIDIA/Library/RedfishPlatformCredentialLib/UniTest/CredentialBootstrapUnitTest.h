/** @file
  Unit test definitions for the Redfish bootstrap credential library.

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Protocol/EdkIIRedfishCredential.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/HostBasedTestStubLib/IpmiStubLib.h>
#include <Library/UnitTestLib.h>
#include <Library/RedfishCredentialLib.h>

#include "../RedfishPlatformCredentialLib.h"

#define FREE_NON_NULL(a) \
  if ((a) != NULL) \
  { \
    FreePool ((a)); \
    (a) = NULL; \
  }
