/** @file
  Provides platform policy services used during a capsule update that uses the
  services of the NVIDIA_CAPSULE_UPDATE_POLICY_PROTOCOL.

  Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "CapsuleUpdatePolicyDxe.h"

///
/// Handle for the Capsule Update Policy Protocol
///
EFI_HANDLE  mHandle = NULL;

///
/// Capsule Update Policy Protocol instance
///
NVIDIA_CAPSULE_UPDATE_POLICY_PROTOCOL  mCapsuleUpdatePolicy = {
  CapsuleUpdatePolicyCheckSystemPower,
  CapsuleUpdatePolicyCheckSystemThermal,
  CapsuleUpdatePolicyCheckSystemEnvironment,
  CapsuleUpdatePolicyIsLowestSupportedVersionCheckRequired,
  CapsuleUpdatePolicyIsLockFmpDeviceAtLockEventGuidRequired
};

EFI_STATUS
EFIAPI
CapsuleUpdatePolicyCheckSystemPower (
  IN  NVIDIA_CAPSULE_UPDATE_POLICY_PROTOCOL *This,
  OUT BOOLEAN                               *Good
  )
{
  *Good = TRUE;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CapsuleUpdatePolicyCheckSystemThermal (
  IN  NVIDIA_CAPSULE_UPDATE_POLICY_PROTOCOL *This,
  OUT BOOLEAN                               *Good
  )
{
  *Good = TRUE;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CapsuleUpdatePolicyCheckSystemEnvironment (
  IN  NVIDIA_CAPSULE_UPDATE_POLICY_PROTOCOL *This,
  OUT BOOLEAN                               *Good
  )
{
  *Good = TRUE;
  return EFI_SUCCESS;
}

BOOLEAN
EFIAPI
CapsuleUpdatePolicyIsLowestSupportedVersionCheckRequired (
  IN  NVIDIA_CAPSULE_UPDATE_POLICY_PROTOCOL *This
  )
{
  return TRUE;
}

BOOLEAN
EFIAPI
CapsuleUpdatePolicyIsLockFmpDeviceAtLockEventGuidRequired (
  IN  NVIDIA_CAPSULE_UPDATE_POLICY_PROTOCOL *This
  )
{
  // Don't use FmpDxe flash locking, FmpDeviceLib controls flash access
  return FALSE;
}

EFI_STATUS
EFIAPI
CapsuleUpdatePolicyInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;

  ASSERT_PROTOCOL_ALREADY_INSTALLED (NULL, &gNVIDIACapsuleUpdatePolicyProtocolGuid);
  Status = gBS->InstallMultipleProtocolInterfaces (&mHandle,
                                                   &gNVIDIACapsuleUpdatePolicyProtocolGuid,
                                                   &mCapsuleUpdatePolicy,
                                                   NULL);
  ASSERT_EFI_ERROR (Status);

  return Status;
}
