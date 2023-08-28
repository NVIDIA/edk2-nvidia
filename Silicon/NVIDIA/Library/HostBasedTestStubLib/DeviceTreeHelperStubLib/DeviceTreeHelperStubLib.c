/** @file

  DeviceTreeHelperLib stubs for host based tests

  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <HostBasedTestStubLib/DeviceTreeHelperStubLib.h>

EFI_STATUS
EFIAPI
GetKernelAddress (
  OUT UINT64  *KernelStart,
  OUT UINT64  *KernelDtbStart
  )
{
  return EFI_UNSUPPORTED;
}
