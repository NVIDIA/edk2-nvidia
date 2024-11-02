/** @file
  This file exposes the internal interfaces which may be unit tested
  for the RamDiskOS driver.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef RAM_DISK_OS_GOOGLE_TEST_H_
#define RAM_DISK_OS_GOOGLE_TEST_H_

//
// Minimal includes needed to compile
//
#include <Uefi.h>

/**
  The entry point for RamDiskDxe driver.

  @param[in] ImageHandle     The image handle of the driver.
  @param[in] SystemTable     The system table.

  @retval EFI_ALREADY_STARTED     The driver already exists in system.
  @retval EFI_OUT_OF_RESOURCES    Fail to execute entry point due to lack of
                                  resources.
  @retval EFI_SUCCES              All the related protocols are installed on
                                  the driver.
**/
EFI_STATUS
RamDiskOSEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

#endif // RAM_DISK_OS_GOOGLE_TEST_H_
