/** @file
  Unit test definitions for the AndroidBoot driver.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _ANDROID_BOOT_DXE_UNIT_TEST_PRIVATE_H_
#define _ANDROID_BOOT_DXE_UNIT_TEST_PRIVATE_H_

#include "../AndroidBootDxe.h"

#include <Library/UnitTestLib.h>
#include <HostBasedTestStubLib/PcdStubLib.h>
#include <HostBasedTestStubLib/BlockIoStubProtocol.h>
#include <HostBasedTestStubLib/DiskIoStubProtocol.h>
#include <HostBasedTestStubLib/MemoryAllocationStubLib.h>

#define ADD_TEST_CASE(fn, ctx) \
  AddTestCase (Suite, #fn " with " #ctx, #fn, fn, fn##_Prepare, fn##_Cleanup, &ctx)

/**
  Test plan structure for AndroidBootRead
 */
typedef struct {
  BOOLEAN       WithDiskIo;
  EFI_STATUS    ReadReturn;
  VOID          *ReadBuffer;
  EFI_STATUS    ExpectedOffset;
} TEST_PLAN_ANDROID_BOOT_READ;

/**
  Test plan structure for AndroidBootGetVerify
 */
typedef struct {
  BOOLEAN                        WithBlockIo;
  BOOLEAN                        WithImgData;
  BOOLEAN                        WithKernelArgs;
  BOOLEAN                        FailAllocation;
  UINT64                         PcdRcmKernelSize;
  EFI_BLOCK_IO_MEDIA             *Media;
  EFI_BLOCK_IO_PROTOCOL          *BlockIo;
  EFI_DISK_IO_PROTOCOL           *DiskIo;
  TEST_PLAN_ANDROID_BOOT_READ    *AndroidBootReads[4];
  ANDROID_BOOT_DATA              *ExpectedImgData;
  CHAR16                         *ExpectedKernelArgs;
  EFI_STATUS                     ExpectedReturn;
} TEST_PLAN_ANDROID_BOOT_GET_VERIFY;

/**
  Test plan structure for UpdateKernelArgs
 */
typedef struct {
  BOOLEAN         FailAllocation;
  BOOLEAN         InvalidProtocol;
  CONST CHAR16    *InitialKernelArgs;
  CONST CHAR16    *NewKernelArgs;
  EFI_STATUS      ExpectedReturn;
} TEST_PLAN_UPDATE_KERNEL_ARGS;

/**
  Populate the BootImgHeader test suite.
 */
VOID
BootImgHeader_PopulateSuite (
  UNIT_TEST_SUITE_HANDLE  Suite
  );

/**
  Set up the UpdateKernelArgs test suite.
 */
VOID
EFIAPI
Suite_UpdateKernelArgs_Setup (
  VOID
  );

/**
  Populate the UpdateKernelArgs test suite.
 */
VOID
UpdateKernelArgs_PopulateSuite (
  UNIT_TEST_SUITE_HANDLE  Suite
  );

#endif
