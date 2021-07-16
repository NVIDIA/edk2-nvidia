/** @file
This module installs smm unavailable protocol.

Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

  Portions provided under the following terms:
  Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

/**
  The destructor function installs smm unavailable protocol

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the Management mode System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
StandaloneMmUnavailableLibDestructor (
  IN EFI_HANDLE                           ImageHandle,
  IN EFI_SYSTEM_TABLE                     *SystemTable
  )
{
  EFI_STATUS            Status;
  EFI_HANDLE            Handle;

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gNVIDIAStandaloneMmUnavailableGuid,
                  NULL,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);
  return EFI_SUCCESS;
}
