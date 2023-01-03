/** @file
  Provides the Simple Network functions.

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UsbRndisDxe.h"

/**
  Get MAC address of USB NIC.

  @param[in]  This                 Instance of protocol for device.
  @param[out] MacAddress           Pointer to return MAC address.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
EFI_STATUS
RndisGetMacAddress (
  IN  NVIDIA_USB_NIC_INFO_PROTOCOL  *This,
  OUT EFI_MAC_ADDRESS               *MacAddress
  )
{
  USB_RNDIS_PRIVATE_DATA  *Private;

  if ((This == NULL) || (MacAddress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = USB_RNDIS_PRIVATE_DATA_FROM_USB_NIC_INFO_THIS (This);
  ZeroMem (MacAddress, sizeof (EFI_MAC_ADDRESS));
  CopyMem (MacAddress, &Private->UsbData.CurrentAddress, NET_ETHER_ADDR_LEN);

  return EFI_SUCCESS;
}

/**
  Initial RNDIS USB NIC Information protocol.

  @param[in]      Private       Poniter to private data

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
UsbRndisInitialUsbNicInfo (
  IN  USB_RNDIS_PRIVATE_DATA  *Private
  )
{
  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private->UsbNicInfoProtocol.GetMacAddress = RndisGetMacAddress;

  return EFI_SUCCESS;
}
