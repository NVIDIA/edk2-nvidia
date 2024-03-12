/** @file
  EDK2 API for AvbLib

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __AVB_VERIFIED_BOOT__
#define __AVB_VERIFIED_BOOT__

#include <Uefi/UefiBaseType.h>

typedef enum {
  VERIFIED_BOOT_UNKNOWN_STATE,
  VERIFIED_BOOT_RED_STATE,
  VERIFIED_BOOT_YELLOW_STATE,
  VERIFIED_BOOT_GREEN_STATE,
  VERIFIED_BOOT_ORANGE_STATE,
  VERIFIED_BOOT_RED_STATE_EIO,
} AVB_BOOT_STATE;

/**
 * Process all verified boot related issues aka verify boot.img
 * signature, pass params to tlk, show verified boot UI
 *
 * @param[in]  IsRecovery       If boot type is recovery boot
 * @param[in]  ControllerHandle Handle of controller that will be used
 *                              to access partitions in AvbLib.
 * @param[out] AvbCmdline       avb cmdline passed out by main libavb call
 *
 * @return EFI_SUCCESS if all process is successfully, else apt error
 */
EFI_STATUS
AvbVerifyBoot (
  IN BOOLEAN     IsRecovery,
  IN EFI_HANDLE  ControllerHandle,
  OUT CHAR8      **AvbCmdline
  );

#endif
