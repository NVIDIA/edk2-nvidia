/** @file

  DeviceTreeHelperLib stubs for host based tests

  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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

EFI_STATUS
EFIAPI
DeviceTreeGetNodeByPath (
  IN CONST CHAR8  *NodePath,
  OUT INT32       *NodeOffset
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
DeviceTreeGetNodeProperty (
  IN  INT32        NodeOffset,
  IN  CONST CHAR8  *Property,
  OUT CONST VOID   **PropertyData OPTIONAL,
  OUT UINT32       *PropertySize OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
DeviceTreeGetNodePropertyValue32 (
  IN  INT32        NodeOffset,
  IN  CONST CHAR8  *Property,
  OUT UINT32       *PropertyValue
  )
{
  return EFI_UNSUPPORTED;
}
