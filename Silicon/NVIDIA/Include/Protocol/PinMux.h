/** @file
  PinMux Register Access Protocol

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __PINMUX_PROTOCOL_H__
#define __PINMUX_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_PINMUX_PROTOCOL_GUID \
  { \
  0x284ebb8b, 0x58eb, 0x4dff, { 0xbf, 0x97, 0x2c, 0x95, 0x89, 0xb0, 0x01, 0x0f } \
  }

typedef struct _NVIDIA_PINMUX_PROTOCOL NVIDIA_PINMUX_PROTOCOL;

/**
  This function reads and returns the value of a specified PinMux Register

  @param[in]     This                The instance of NVIDIA_PINMUX_PROTOCOL.
  @param[in]     RegisterOffset      Offset from the PINMUX Base address to read.
  @param[out]    RegisterValue       Value of the Read PinMux Register.

  @return EFI_SUCCESS                PinMux Register Value successfully returned.
  @return EFI_DEVICE_ERROR           Other error occured in reading PINMUX Register.
**/
typedef
EFI_STATUS
(EFIAPI *PINMUX_READ_REGISTER) (
  IN  NVIDIA_PINMUX_PROTOCOL  *This,
  IN  UINT32    RegisterOffset,
  OUT UINT32    *RegisterValue
  );

/**
  This function writes provided value to specified PinMux Register

  @param[in]     This                The instance of NVIDIA_PINMUX_PROTOCOL.
  @param[in]     RegisterOffset      Offset from the PINMUX Base address to Write.
  @param[in]     Value               Value for writing to PinMux Register.

  @return EFI_SUCCESS                PinMux Register Value successfully Written.
  @return EFI_DEVICE_ERROR           Other error occured in writing to PINMUX Register.
**/
typedef
EFI_STATUS
(EFIAPI *PINMUX_WRITE_REGISTER) (
  IN  NVIDIA_PINMUX_PROTOCOL  *This,
  IN  UINT32    RegisterOffset,
  IN  UINT32    Value
  );

struct _NVIDIA_PINMUX_PROTOCOL {
  PINMUX_READ_REGISTER       ReadReg;
  PINMUX_WRITE_REGISTER      WriteReg;
};

extern EFI_GUID gNVIDIAPinMuxProtocolGuid;

#endif
