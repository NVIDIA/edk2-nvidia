/** @file

  Platform Resource Lib stubs for host based tests

  Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PLATFORM_RESOURCE_STUB_LIB_H__
#define __PLATFORM_RESOURCE_STUB_LIB_H__

#include <Library/BootChainInfoLib.h>
#include <Library/PlatformResourceLib.h>

/**
  Save mock parameters for GetPartitionInfoStMm() stub

  @param[In]  CpuBlAddress          Address being looked up
  @param[In]  PartitionIndex        Index being looked up
  @param[In]  DeviceInstance        Instance of partition
  @param[In]  PartitionStartByte    Start address of partition
  @param[In]  PartitionSizeBytes    Size of partition
  @param[In]  ReturnStatus          Status to return

  @retval EFI_SUCCESS               Partition info saved/updated for future lookup
  @retval EFI_OUT_OF_RESOURCES      Couldn't allocate space for the new partition info

**/
EFI_STATUS
EFIAPI
MockGetPartitionInfoStMm (
  IN UINTN       CpuBlAddress,
  IN UINT32      PartitionIndex,
  IN UINT16      DeviceInstance,
  IN UINT64      PartitionStartByte,
  IN UINT64      PartitionSizeBytes,
  IN EFI_STATUS  ReturnStatus
  );

/**
  Set up mock parameters for GetActiveBootChain() stub

  @param[In]  ReturnBootChain       Boot chain to return
  @param[In]  ReturnStatus          Status to return

  @retval None

**/
VOID
MockGetActiveBootChain (
  IN  UINT32      ReturnBootChain,
  IN  EFI_STATUS  ReturnStatus
  );

/**
  Set up mock parameters for SetNextBootChain() stub

  @param[In]  ReturnStatus          Status to return

  @retval None

**/
VOID
MockSetNextBootChain (
  IN  EFI_STATUS  ReturnStatus
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
