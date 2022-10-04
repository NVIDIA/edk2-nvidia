/** @file
  EFuse Register Access Protocol

  Copyright (c) 2019, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EFUSE_PROTOCOL_H__
#define __EFUSE_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_EFUSE_PROTOCOL_GUID \
  { \
  0x961c9250, 0x1f49, 0x44a2, { 0x97, 0x41, 0x66, 0x27, 0x4d, 0xbe, 0x65, 0xd0 } \
  }

typedef struct _NVIDIA_EFUSE_PROTOCOL NVIDIA_EFUSE_PROTOCOL;

/**
  This function reads and returns the value of a specified Fuse Register

  @param[in]     This                The instance of NVIDIA_EFUSE_PROTOCOL.
  @param[in]     RegisterOffset      Offset from the EFUSE Base address to read.
  @param[out]    RegisterValue       Value of the Read Fuse Register.

  @return EFI_SUCCESS                Fuse Register Value successfully returned.
  @return EFI_DEVICE_ERROR           Other error occured in reading FUSE Registers.
**/
typedef
EFI_STATUS
(EFIAPI *EFUSE_READ_REGISTER)(
  IN  NVIDIA_EFUSE_PROTOCOL  *This,
  IN  UINT32    RegisterOffset,
  OUT UINT32    *RegisterValue
  );

struct _NVIDIA_EFUSE_PROTOCOL {
  EFUSE_READ_REGISTER    ReadReg;
};

extern EFI_GUID  gNVIDIAEFuseProtocolGuid;

#endif
