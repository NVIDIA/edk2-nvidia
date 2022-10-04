/** @file
  Usb Pad Control Protocol

  Copyright (c) 2019, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __USBPADCTL_PROTOCOL_H__
#define __USBPADCTL_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_USBPADCTL_PROTOCOL_GUID \
  { \
  0xce280679, 0xb75a, 0x45c7, { 0xa8, 0x3f, 0xd5, 0xa6, 0xa6, 0x77, 0xd8, 0x36 } \
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

struct _NVIDIA_USBPADCTL_PROTOCOL {
  USBPADCTL_INIT_HW      InitHw;
  USBPADCTL_DEINIT_HW    DeInitHw;
};

extern EFI_GUID  gNVIDIAUsbPadCtlProtocolGuid;

#endif
