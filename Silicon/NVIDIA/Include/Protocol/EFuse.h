/** @file
  EFuse Register Access Protocol

  SPDX-FileCopyrightText: Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EFUSE_PROTOCOL_H__
#define __EFUSE_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

#define NVIDIA_EFUSE_PROTOCOL_GUID \
  { \
  0xb5938c29, 0xe1c0, 0x4969, { 0x8b, 0x87, 0xe1, 0x5b, 0xdf, 0xf3, 0x78, 0x89 } \
  }

typedef struct _NVIDIA_EFUSE_PROTOCOL NVIDIA_EFUSE_PROTOCOL;

/**
  This function reads and returns the value of a specified Fuse Register

  @param[in]     This                The instance of NVIDIA_EFUSE_PROTOCOL.
  @param[in]     RegisterOffset      Offset from the EFUSE Base address to read.
  @param[out]    RegisterValue       Value of the Read Fuse Register.

  @return EFI_SUCCESS                Fuse Register Value successfully returned.
  @return EFI_INVALID_PARAMETER      Register Offset param not in EFUSE Region
  @return EFI_DEVICE_ERROR           Other error occured in reading FUSE Registers.
**/
typedef
EFI_STATUS
(EFIAPI *EFUSE_READ_REGISTER)(
  IN  NVIDIA_EFUSE_PROTOCOL  *This,
  IN  UINT32    RegisterOffset,
  OUT UINT32    *RegisterValue
  );

/**
  This function writes and returns the value of a specified Fuse Register

  @param[in]        This                The instance of NVIDIA_EFUSE_PROTOCOL.
  @param[in]        RegisterOffset      Offset from the EFUSE Base address to write.
  @param[in]        RegisterValue       Value of the Write Fuse Register.

  @return EFI_SUCCESS                Fuse Register Value successfully returned.
  @return EFI_INVALID_PARAMETER      Register Offset param not in EFUSE Region
  @return EFI_DEVICE_ERROR           Other error occured in reading FUSE Registers.
**/
typedef
EFI_STATUS
(EFIAPI *EFUSE_WRITE_REGISTER)(
  IN     NVIDIA_EFUSE_PROTOCOL  *This,
  IN     UINT32    RegisterOffset,
  IN     UINT32    *RegisterValue
  );

struct _NVIDIA_EFUSE_PROTOCOL {
  EFUSE_READ_REGISTER     ReadReg;
  EFUSE_WRITE_REGISTER    WriteReg;
};

extern EFI_GUID  gNVIDIAEFuseProtocolGuid;

#endif
