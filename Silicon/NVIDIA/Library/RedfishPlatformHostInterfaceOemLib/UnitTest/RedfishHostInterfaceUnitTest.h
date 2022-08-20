/** @file

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <Uefi.h>
#include <IndustryStandard/IpmiNetFnTransport.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/HostBasedTestStubLib/IpmiStubLib.h>
#include <HostBasedTestStubLib/UefiRuntimeServicesTableStubLib.h>
#include <Library/UnitTestLib.h>
#include "../RedfishPlatformHostInterfaceIpmi.h"
