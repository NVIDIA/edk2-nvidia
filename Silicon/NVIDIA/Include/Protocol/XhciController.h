/** @file
  Xhci Controller Protocol

  Copyright (c) 2019, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

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
(EFIAPI *XHCICONTROLLER_GET_BASEADDRESS)(
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
(EFIAPI *XHCICONTROLLER_GET_CFGADDRESS)(
  IN  NVIDIA_XHCICONTROLLER_PROTOCOL  *This,
  OUT EFI_PHYSICAL_ADDRESS *CfgAdress
  );

struct _NVIDIA_XHCICONTROLLER_PROTOCOL {
  XHCICONTROLLER_GET_BASEADDRESS    GetBaseAddr;
  XHCICONTROLLER_GET_CFGADDRESS     GetCfgAddr;
};

extern EFI_GUID  gNVIDIAXhciControllerProtocolGuid;

#endif
