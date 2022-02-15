/** @file

  Platform Resource Lib stubs for host based tests

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef __PLATFORM_RESOURCE_STUB_LIB_H__
#define __PLATFORM_RESOURCE_STUB_LIB_H__

#include <Library/BootChainInfoLib.h>
#include <Library/PlatformResourceLib.h>

/**
  Set up mock parameters for GetActiveBootChain() stub

  @param[In]  ReturnBootChain       Boot chain to return
  @param[In]  ReturnStatus          Status to return

  @retval None

**/
VOID
MockGetActiveBootChain (
  IN  UINT32        ReturnBootChain,
  IN  EFI_STATUS    ReturnStatus
  );

/**
  Set up mock parameters for SetNextBootChain() stub

  @param[In]  ReturnStatus          Status to return

  @retval None

**/
VOID
MockSetNextBootChain (
  IN  EFI_STATUS        ReturnStatus
  );

/**
  Initialize Platform Resource stub lib

  @retval None

**/
VOID
PlatformResourcesStubLibInit (
  VOID
  );

/**
  De-initialize Platform Resource stub lib

  @retval None

**/
VOID
PlatformResourcesStubLibDeinit (
  VOID
  );

#endif
