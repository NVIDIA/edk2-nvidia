/** @file
  ProtocolHelperLib stubs for host based tests.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/ProtocolHelperLib.h>

#include <Protocol/PciRootBridgeConfigurationIo.h>

extern EFI_GUID  gNVIDIAPciRootBridgeConfigurationIoProtocolGuid;

STATIC EFI_STATUS                                        mGetParentFromHandleStatus = EFI_NOT_FOUND;
STATIC BOOLEAN                                           mRootBridgeC2cInitDone     = TRUE;
STATIC NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  mRootBridgeConfig;

VOID
EFIAPI
ProtocolHelperStubReset (
  VOID
  )
{
  mGetParentFromHandleStatus = EFI_SUCCESS;
  mRootBridgeC2cInitDone     = TRUE;
}

VOID
EFIAPI
ProtocolHelperStubSetGetParentProtocolFromHandleStatus (
  IN EFI_STATUS  Status
  )
{
  mGetParentFromHandleStatus = Status;
}

VOID
EFIAPI
ProtocolHelperStubSetRootBridgeC2cInitDone (
  IN BOOLEAN  C2cInitDone
  )
{
  mRootBridgeC2cInitDone = C2cInitDone;
}

EFI_STATUS
EFIAPI
ProtocolHelperGetParentProtocolFromDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL  *DevicePath,
  IN EFI_GUID                  *ProtocolGuid,
  OUT VOID                     **Protocol
  )
{
  (VOID)DevicePath;
  (VOID)ProtocolGuid;
  (VOID)Protocol;
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
ProtocolHelperGetParentProtocolFromHandle (
  IN EFI_HANDLE  Handle,
  IN EFI_GUID    *ProtocolGuid,
  OUT VOID       **Protocol
  )
{
  (VOID)Handle;

  if ((ProtocolGuid == NULL) || (Protocol == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!CompareGuid (ProtocolGuid, &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid)) {
    return EFI_UNSUPPORTED;
  }

  if (EFI_ERROR (mGetParentFromHandleStatus)) {
    *Protocol = NULL;
    return mGetParentFromHandleStatus;
  }

  ZeroMem (&mRootBridgeConfig, sizeof (mRootBridgeConfig));
  mRootBridgeConfig.C2cInitDone = mRootBridgeC2cInitDone;
  *Protocol                     = &mRootBridgeConfig;
  return EFI_SUCCESS;
}
