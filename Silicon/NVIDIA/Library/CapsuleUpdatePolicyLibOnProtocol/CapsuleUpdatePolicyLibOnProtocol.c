/** @file
  Provides platform policy services used during a capsule update that uses the
  services of the NVIDIA_CAPSULE_UPDATE_POLICY_PROTOCOL. If the
  NVIDIA_CAPSULE_UPDATE_POLICY_PROTOCOL is not available, then assume the
  platform is in a state that supports a firmware update.

  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
  Copyright (c) 2016, Microsoft Corporation. All rights reserved.<BR>
  Copyright (c) 2018-2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Portions provided under the following terms:
  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#include <PiDxe.h>
#include <Library/CapsuleUpdatePolicyLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/CapsuleUpdatePolicy.h>

///
/// Pointer to the Capsule Update Policy Protocol instance that is
/// optionally installed by a platform.
///
NVIDIA_CAPSULE_UPDATE_POLICY_PROTOCOL  *mCapsuleUpdatePolicy = NULL;

/**
  Check if the Capsule Update Policy Protocol pointer is valid
**/
STATIC
BOOLEAN
EFIAPI
CapsuleUpdatePolicyProtocolIsValid (
  VOID
  )
{
  return (mCapsuleUpdatePolicy != NULL);
}

EFI_STATUS
EFIAPI
CheckSystemPower (
  OUT BOOLEAN  *Good
  )
{
  if (CapsuleUpdatePolicyProtocolIsValid ()) {
    return mCapsuleUpdatePolicy->CheckSystemPower (mCapsuleUpdatePolicy, Good);
  }
  *Good = TRUE;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CheckSystemThermal (
  OUT BOOLEAN   *Good
  )
{
  if (CapsuleUpdatePolicyProtocolIsValid ()) {
    return mCapsuleUpdatePolicy->CheckSystemThermal (mCapsuleUpdatePolicy, Good);
  }
  *Good = TRUE;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CheckSystemEnvironment (
  OUT BOOLEAN   *Good
  )
{
  if (CapsuleUpdatePolicyProtocolIsValid ()) {
    return mCapsuleUpdatePolicy->CheckSystemEnvironment (mCapsuleUpdatePolicy, Good);
  }
  *Good = TRUE;
  return EFI_SUCCESS;
}

BOOLEAN
EFIAPI
IsLowestSupportedVersionCheckRequired (
  VOID
  )
{
  if (CapsuleUpdatePolicyProtocolIsValid ()) {
    return mCapsuleUpdatePolicy->IsLowestSupportedVersionCheckRequired (mCapsuleUpdatePolicy);
  }
  return TRUE;
}

BOOLEAN
EFIAPI
IsLockFmpDeviceAtLockEventGuidRequired (
  VOID
  )
{
  if (CapsuleUpdatePolicyProtocolIsValid ()) {
    return mCapsuleUpdatePolicy->IsLockFmpDeviceAtLockEventGuidRequired (mCapsuleUpdatePolicy);
  }
  // Don't use FmpDxe flash locking, FmpDeviceLib controls flash access
  return FALSE;
}

/**
  Library initialization

  @param[in] ImageHandle  Image handle of this driver.
  @param[in] SystemTable  Pointer to SystemTable.

**/
EFI_STATUS
EFIAPI
CapsuleUpdatePolicyLibInit (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  // find and save the CapsuleUpdatePolicy Protocol pointer
  Status = gBS->LocateProtocol (&gNVIDIACapsuleUpdatePolicyProtocolGuid,
                                NULL,
                                (VOID **)&mCapsuleUpdatePolicy);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "CapsuleUpdatePolicy Protocol Guid=%g not found: %r\n",
            &gNVIDIACapsuleUpdatePolicyProtocolGuid, Status));
    // use default policies
    mCapsuleUpdatePolicy = NULL;
  }

  return Status;
}
