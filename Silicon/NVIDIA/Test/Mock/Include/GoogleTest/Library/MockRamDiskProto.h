/** @file
  Google Test mocks for RamDisk protocol

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_RAM_DISK_PROTO_H_
#define MOCK_RAM_DISK_PROTO_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>

extern "C" {
  #include <Uefi.h>
  #include <Protocol/RamDisk.h>
}

struct MockRamDiskProto {
  MOCK_INTERFACE_DECLARATION (MockRamDiskProto);

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    Register,
    (IN  UINT64                    RamDiskBase,
     IN  UINT64                    RamDiskSize,
     IN  EFI_GUID                  *RamDiskType,
     IN  EFI_DEVICE_PATH           *ParentDevicePath OPTIONAL,
     OUT EFI_DEVICE_PATH_PROTOCOL  **DevicePath)
    );

  // Unregister currently not implemented
};

extern "C" {
  extern EFI_RAM_DISK_PROTOCOL  *gMockRamDiskProtocol;
}

#endif // MOCK_RAM_DISK_PROTO_H_
