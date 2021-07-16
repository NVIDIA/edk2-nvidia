/** @file
  Xhci Controller Protocol

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2019 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __XHCICONTROLLER_PROTOCOL_H__
#define __XHCICONTROLLER_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_XHCICONTROLLER_PROTOCOL_GUID \
  { \
  0xe516021f, 0x012a, 0x4b1f, { 0x91, 0x5b, 0x2a, 0x6e, 0x05, 0xb2, 0xe9, 0xe2 } \
  }

typedef struct _NVIDIA_XHCICONTROLLER_PROTOCOL NVIDIA_XHCICONTROLLER_PROTOCOL;

/**
  Function returns Base Address of XHCI Registers

  @param[in]     This                Instance of NVIDIA_XHCICONTROLLER_PROTOCOL
  @param[out]    BaseAdress          Base Address of XHCI Registers

  @return EFI_SUCCESS                Address returned Successfully.
**/
typedef
EFI_STATUS
(EFIAPI *XHCICONTROLLER_GET_BASEADDRESS) (
  IN  NVIDIA_XHCICONTROLLER_PROTOCOL  *This,
  OUT EFI_PHYSICAL_ADDRESS *BaseAdress
  );


/**
  Function returns Address of XHCI Configuration Registers

  @param[in]     This                Instance of NVIDIA_XHCICONTROLLER_PROTOCOL
  @param[out]    CfgAdress           Address of XHCI Configuration Registers

  @return EFI_SUCCESS                Address returned Successfully.
**/
typedef
EFI_STATUS
(EFIAPI *XHCICONTROLLER_GET_CFGADDRESS) (
  IN  NVIDIA_XHCICONTROLLER_PROTOCOL  *This,
  OUT EFI_PHYSICAL_ADDRESS *CfgAdress
  );

struct _NVIDIA_XHCICONTROLLER_PROTOCOL {
  XHCICONTROLLER_GET_BASEADDRESS  GetBaseAddr;
  XHCICONTROLLER_GET_CFGADDRESS   GetCfgAddr;
};

extern EFI_GUID gNVIDIAXhciControllerProtocolGuid;

#endif
