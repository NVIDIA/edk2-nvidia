/** @file
  Usb Pad Control Protocol

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

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
(EFIAPI *USBPADCTL_INIT_HW) (
  IN  NVIDIA_USBPADCTL_PROTOCOL  *This
  );

struct _NVIDIA_USBPADCTL_PROTOCOL {
  USBPADCTL_INIT_HW             InitHw;
};

extern EFI_GUID gNVIDIAUsbPadCtlProtocolGuid;

#endif
