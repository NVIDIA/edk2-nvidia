/** @file
  Stub implementations for CreateHttpBootOption tests.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Protocol/DevicePath.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

//
// Global variables to control stub behavior
//
static EFI_STATUS                gStubFindNicByMacStatus     = EFI_SUCCESS;
static EFI_HANDLE                gStubFindNicByMacHandle     = NULL;
static EFI_STATUS                gStubBuildDevicePathStatus  = EFI_SUCCESS;
static EFI_DEVICE_PATH_PROTOCOL  *gStubBuildDevicePathResult = NULL;

/**
  Set the result that FindNicByMac should return.

  @param[in]  Status     Status to return from FindNicByMac
  @param[in]  NicHandle  Handle to return (if Status is EFI_SUCCESS)

**/
VOID
SetStubFindNicByMac (
  EFI_STATUS  Status,
  EFI_HANDLE  NicHandle
  )
{
  gStubFindNicByMacStatus = Status;
  gStubFindNicByMacHandle = NicHandle;
}

/**
  Clear the configured result for FindNicByMac.

**/
VOID
ClearStubFindNicByMac (
  VOID
  )
{
  gStubFindNicByMacStatus = EFI_SUCCESS;
  gStubFindNicByMacHandle = NULL;
}

/**
  Set the result that BuildHttpBootDevicePath should return.

  @param[in]  Status      Status to return from BuildHttpBootDevicePath
  @param[in]  DevicePath  Device path to return (if Status is EFI_SUCCESS)

**/
VOID
SetStubBuildHttpBootDevicePath (
  EFI_STATUS                Status,
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  gStubBuildDevicePathStatus = Status;
  gStubBuildDevicePathResult = DevicePath;
}

/**
  Clear the configured result for BuildHttpBootDevicePath.

**/
VOID
ClearStubBuildHttpBootDevicePath (
  VOID
  )
{
  gStubBuildDevicePathStatus = EFI_SUCCESS;
  gStubBuildDevicePathResult = NULL;
}

/**
  Stub implementation of FindNicByMac.

  Returns the status and handle set by SetStubFindNicByMac().

  @param[in]   MacAddr    MAC address to find
  @param[out]  NicHandle  Handle of the NIC (if found)

  @retval Status set by SetStubFindNicByMac()

**/
EFI_STATUS
FindNicByMac (
  IN  CONST EFI_MAC_ADDRESS  *MacAddr,
  OUT EFI_HANDLE             *NicHandle
  )
{
  if (EFI_ERROR (gStubFindNicByMacStatus)) {
    return gStubFindNicByMacStatus;
  }

  *NicHandle = gStubFindNicByMacHandle;
  return EFI_SUCCESS;
}

/**
  Stub implementation of BuildHttpBootDevicePath.

  Returns the status and device path set by SetStubBuildHttpBootDevicePath().

  @param[in]   NicHandle   Handle of the NIC
  @param[in]   Uri         URI string
  @param[out]  DevicePath  Device path (allocated)

  @retval Status set by SetStubBuildHttpBootDevicePath()

**/
EFI_STATUS
BuildHttpBootDevicePath (
  IN  EFI_HANDLE                NicHandle,
  IN  CONST CHAR16              *Uri,
  OUT EFI_DEVICE_PATH_PROTOCOL  **DevicePath
  )
{
  if (EFI_ERROR (gStubBuildDevicePathStatus)) {
    return gStubBuildDevicePathStatus;
  }

  // Allocate a copy of the device path for the caller to free
  UINTN  Size = sizeof (EFI_DEVICE_PATH_PROTOCOL);

  *DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)AllocateZeroPool (Size);
  if (*DevicePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (*DevicePath, gStubBuildDevicePathResult, Size);
  return EFI_SUCCESS;
}

/**
  Stub implementation of IsMacAllZeros.

  Checks if MAC address is all zeros.

  @param[in]  MacAddr  MAC address to check

  @retval TRUE   MAC address is all zeros
  @retval FALSE  MAC address is not all zeros

**/
BOOLEAN
IsMacAllZeros (
  IN CONST EFI_MAC_ADDRESS  *MacAddr
  )
{
  UINTN  Index;

  for (Index = 0; Index < 6; Index++) {
    if (MacAddr->Addr[Index] != 0) {
      return FALSE;
    }
  }

  return TRUE;
}
