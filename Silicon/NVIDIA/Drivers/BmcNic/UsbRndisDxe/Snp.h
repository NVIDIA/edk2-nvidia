/** @file
  Definition of Simple Network functions.

  Copyright (c) 2011, Intel Corporation
  Copyright (c) 2020, ARM Limited. All rights reserved.
  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

 **/

#ifndef SNP_H_
#define SNP_H_

#include "UsbRndisDxe.h"

#include <Library/NetLib.h>

/**
  Ethernet header layout

  IEEE 802.3-2002 Part 3 specification, section 3.1.1.
**/
#pragma pack(1)
typedef struct {
  UINT8     DstAddr[NET_ETHER_ADDR_LEN];
  UINT8     SrcAddr[NET_ETHER_ADDR_LEN];
  UINT16    Type;
} ETHERNET_HEADER;
#pragma pack()

/**
  Initial RNDIS SNP service

  @param[in]      Private       Poniter to private data

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
UsbRndisInitialSnpService (
  IN  USB_RNDIS_PRIVATE_DATA  *Private
  );

#endif
