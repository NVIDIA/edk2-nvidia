/** @file

  Copyright (c) 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef USB_FW_PROTOCOL_H__
#define USB_FW_PROTOCOL_H__

#define USB_FW_PROTOCOL_GUID \
  { \
  0xed3b027b, 0x201f, 0x4fe5, { 0xa1, 0x98, 0x42, 0xa0, 0xcf, 0xbf, 0x7f, 0x2e } \
  }

typedef struct UsbFwInfo {
  VOID  *UsbFwBase;
  UINTN UsbFwSize;
} NVIDIA_USBFW_PROTOCOL;

extern EFI_GUID gNVIDIAUsbFwProtocolGuid;

#endif // USB_FW_PROTOCOL_H__
