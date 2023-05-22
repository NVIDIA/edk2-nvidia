/** @file
  NVIDIA Async driver status

  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ASYNC_DRIVER_STATUS_H__
#define ASYNC_DRIVER_STATUS_H__

#include <PiDxe.h>
#include <Uefi/UefiSpec.h>

#define NVIDIA_ASYNC_DRIVER_STATUS_GUID \
  { \
    0xdf28c8db, 0x28f0, 0x4325, { 0x88, 0x6a, 0x93, 0x6e, 0x92, 0x17, 0xc5, 0xc1 } \
  }

//
// Define for forward reference.
//
typedef struct _NVIDIA_ASYNC_DRIVER_STATUS_PROTOCOL NVIDIA_ASYNC_DRIVER_STATUS_PROTOCOL;

/**
  Gets info on if an async driver is still running.

  @param[in]     This                The instance of the NVIDIA_SE_RNG_PROTOCOL.
  @param[out]    StillPending        This driver is still running setup

  @retval EFI_SUCCESS                Status was returned.
  @retval EFI_INVALID_PARAMETER      StillPending is NULL.
  @retval others                     Error processing status
**/
typedef
EFI_STATUS
(EFIAPI *ASYNC_DRIVER_GETSTATUS)(
  IN  NVIDIA_ASYNC_DRIVER_STATUS_PROTOCOL   *This,
  IN  BOOLEAN                               *StillPending
  );

/// NVIDIA_ASYNC_DRIVER_STATUS_PROTOCOL protocol structure.
struct _NVIDIA_ASYNC_DRIVER_STATUS_PROTOCOL {
  ASYNC_DRIVER_GETSTATUS    GetStatus;
};

extern EFI_GUID  gNVIDIAAsyncDriverStatusProtocol;

#endif
