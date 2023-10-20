/** @file

  Android Boot Config Driver

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ANDROID_BOOTCONFIG_H__
#define __ANDROID_BOOTCONFIG_H__

#include <Uefi.h>

#define BOOTCONFIG_MAGIC          "#BOOTCONFIG\n"
#define BOOTCONFIG_MAGIC_SIZE     12
#define BOOTCONFIG_SIZE_SIZE      4
#define BOOTCONFIG_CHECKSUM_SIZE  4
#define BOOTCONFIG_TRAILER_SIZE   BOOTCONFIG_MAGIC_SIZE +\
                                BOOTCONFIG_SIZE_SIZE + \
                                BOOTCONFIG_CHECKSUM_SIZE

/*
 * Add a string of boot config parameters to memory appended by the trailer.
 * This memory needs to be immediately following the end of the ramdisks.
 * The new boot config trailer will be written to the end of the entire
 * parameter section(previous + new). The trailer contains a 4 byte size of the
 * parameters, followed by a 4 byte checksum of the parameters, followed by a 12
 * byte magic string.
 *
 * @param Params pointer to string of boot config parameters
 * @param ParamsSize size of params string in bytes
 * @param BootConfigStartAddr address that the boot config section is starting
 *        at in memory.
 * @param BootConfigSize size of the current bootconfig section in bytes.
 * @return number of bytes added to the boot config section. -1 for error.
 */
EFI_STATUS
AddBootConfigParameters (
  CHAR8   *Params,
  UINT32  ParamsSize,
  UINT64  BootConfigStartAddr,
  UINT32  BootConfigSize
  );

/*
 * Add the boot config trailer to the end of the boot config parameter section.
 * This can be used after the vendor bootconfig section has been placed into
 * memory if there are no additional parameters that need to be added.
 * The new boot config trailer will be written to the end of the entire
 * parameter section at (bootconfig_start_addr + bootconfig_size).
 * The trailer contains a 4 byte size of the parameters, followed by a 4 byte
 * checksum of the parameters, followed by a 12 byte magic string.
 *
 * @param BootConfigStartAddr address that the boot config section is starting
 *        at in memory.
 * @param BootConfigSize size of the current bootconfig section in bytes.
 * @return number of bytes added to the boot config section. -1 for error.
 */
EFI_STATUS
AddBootConfigTrailer (
  UINT64  BootConfigStartAddr,
  UINT32  BootConfigSize
  );

#endif /* __ANDROID_BOOTCONFIG_H__ */
