/** @file

  BMC USB NIC information protocol

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __USB_NIC_INFO_PROTOCOL_H__
#define __USB_NIC_INFO_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>

typedef struct _NVIDIA_USB_NIC_INFO_PROTOCOL NVIDIA_USB_NIC_INFO_PROTOCOL;

/**
  Get MAC address of USB NIC.

  @param[in]  This                 Instance of protocol for device.
  @param[out] MacAddress           Pointer to return MAC address.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
typedef
EFI_STATUS
(EFIAPI *USB_NIC_INFO_GET_MAC_ADDRESS)(
  IN  NVIDIA_USB_NIC_INFO_PROTOCOL    *This,
  OUT EFI_MAC_ADDRESS                 *MacAddress
  );

//
// protocol interface
//
struct _NVIDIA_USB_NIC_INFO_PROTOCOL {
  USB_NIC_INFO_GET_MAC_ADDRESS    GetMacAddress;
};

extern EFI_GUID  gNVIDIAUsbNicInfoProtocolGuid;

#endif
