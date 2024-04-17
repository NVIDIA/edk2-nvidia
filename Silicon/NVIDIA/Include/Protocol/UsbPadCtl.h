/** @file
  Usb Pad Control Protocol

  SPDX-FileCopyrightText: Copyright (c) 2019-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __USBPADCTL_PROTOCOL_H__
#define __USBPADCTL_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_USBPADCTL_PROTOCOL_GUID \
  { \
  0xadf12597, 0xf8bd, 0x4f06, { 0xb6, 0x6a, 0xe1, 0x7a, 0xc9, 0xe3, 0x96, 0x4c } \
  }

typedef struct _NVIDIA_USBPADCTL_PROTOCOL NVIDIA_USBPADCTL_PROTOCOL;

/**
  This function Initializes the USB Pads for specific Chip.

  @param[in]     This                Instance of NVIDIA_USBPADCTL_PROTOCOL

  @return EFI_SUCCESS                PADS Successfully Enabled.
**/
typedef
EFI_STATUS
(EFIAPI *USBPADCTL_INIT_HW)(
  IN  NVIDIA_USBPADCTL_PROTOCOL  *This
  );

/**
  This function DeInitializes the USB Pads for specific Chip.

  @param[in]     This                Instance of NVIDIA_USBPADCTL_PROTOCOL

  @return EFI_SUCCESS                PADS Successfully Disabled.
**/
typedef
VOID
(EFIAPI *USBPADCTL_DEINIT_HW)(
  IN  NVIDIA_USBPADCTL_PROTOCOL  *This
  );

/**
  This function Initializes the USB Pads for specific Chip.

  @param[in]     This                Instance of NVIDIA_USBPADCTL_PROTOCOL

  @return EFI_SUCCESS                PADS Successfully Enabled.
**/
typedef
EFI_STATUS
(EFIAPI *USBPADCTL_INIT_DEV_HW)(
  IN  NVIDIA_USBPADCTL_PROTOCOL  *This
  );

/**
  This function DeInitializes the USB Pads for specific Chip.

  @param[in]     This                Instance of NVIDIA_USBPADCTL_PROTOCOL

  @return EFI_SUCCESS                PADS Successfully Disabled.
**/
typedef
VOID
(EFIAPI *USBPADCTL_DEINIT_DEV_HW)(
  IN  NVIDIA_USBPADCTL_PROTOCOL  *This
  );

struct _NVIDIA_USBPADCTL_PROTOCOL {
  USBPADCTL_INIT_HW          InitHw;
  USBPADCTL_DEINIT_HW        DeInitHw;
  USBPADCTL_INIT_DEV_HW      InitDevHw;
  USBPADCTL_DEINIT_DEV_HW    DeInitDevHw;
};

extern EFI_GUID  gNVIDIAUsbPadCtlProtocolGuid;

#endif
