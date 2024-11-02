/** @file
  Google Test mocks for RamDisk Protocol

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <GoogleTest/Library/MockRamDiskProto.h>

MOCK_INTERFACE_DEFINITION(MockRamDiskProto);

MOCK_FUNCTION_DEFINITION(MockRamDiskProto, Register,   5, EFIAPI);

static EFI_RAM_DISK_PROTOCOL localRamDiskProtocol = {
  Register,    // mock version of EFI_RAM_DISK_PROTOCOL.Register
  NULL,        // Unregister currently not implemented
};

extern "C" {
  EFI_RAM_DISK_PROTOCOL* gMockRamDiskProtocol = &localRamDiskProtocol;
}
