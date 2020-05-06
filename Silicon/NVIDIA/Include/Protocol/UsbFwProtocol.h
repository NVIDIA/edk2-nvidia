/** @file

  Copyright (c) 2020, NVIDIA Corporation. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
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
