/** @file
  EDK2 API for AvbLib

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __AVB_VERIFIED_BOOT__
#define __AVB_VERIFIED_BOOT__

#include <Uefi/UefiBaseType.h>
#include <Library/OpteeNvLib.h>

typedef enum {
  VERIFIED_BOOT_GREEN_STATE,
  VERIFIED_BOOT_YELLOW_STATE,
  VERIFIED_BOOT_ORANGE_STATE,
  VERIFIED_BOOT_RED_STATE,
  VERIFIED_BOOT_RED_STATE_EIO,
  VERIFIED_BOOT_UNKNOWN_STATE,
} AVB_BOOT_STATE;

#define TA_AVB_MAX_ROLLBACK_LOCATIONS  256

// ROT params definition
#define ROT_VERIFIEDBOOT_KEY_NAME    "avb.managed_verity_mode.verified_boot_key"
#define ROT_SERIALNO_NAME            "avb.managed_verity_mode.serial"
#define ROT_VBMETA_DIGEST_NAME       "avb.managed_verity_mode.vbmeta_digest"
#define ROT_DEVICE_BOOT_LOCKED_NAME  "avb.managed_verity_mode.device_boot_locked"
#define ROT_VERIFIEDBOOT_STATE_NAME  "avb.managed_verity_mode.verified_boot_state"
#define ROT_BOOT_PATCHLEVEL_NAME     "avb.managed_verity_mode.boot_patchlevel"

#define PROP_BOOT_PATCHLEVEL_NAME  "com.android.build.boot.security_patch"

/*
 * Gets the rollback index corresponding to the given rollback index slot.
 *
 * in   Params[0].Union.Value.A:      rollback index slot
 * out  Params[1].Union.Value.A:      upper 32 bits of rollback index
 * out  Params[1].Union.Value.B:      lower 32 bits of rollback index
 */
#define TA_AVB_CMD_READ_ROLLBACK_INDEX  0

/*
 * Updates the rollback index corresponding to the given rollback index slot.
 *
 * Will refuse to update a slot with a lower value.
 *
 * in   Params[0].Union.Value.A:      rollback index slot
 * in   Params[1].Union.Value.A:      upper 32 bits of rollback index
 * in   Params[1].Union.Value.B:      lower 32 bits of rollback index
 */
#define TA_AVB_CMD_WRITE_ROLLBACK_INDEX  1

/*
 * Gets the lock state of the device.
 *
 * out  Params[0].Union.Value.A:      lock state
 */
#define TA_AVB_CMD_READ_LOCK_STATE  2

/*
 * Sets the lock state of the device.
 *
 * If the lock state is changed all rollback slots will be reset to 0
 *
 * in   Params[0].Union.Value.A:      lock state
 */
#define TA_AVB_CMD_WRITE_LOCK_STATE  3

/*
 * Reads a persistent value corresponding to the given name.
 *
 * in    Params[0].Union.Memory:       persistent value name
 * inout Params[1].Union.Memory:       read persistent value buffer
 */
#define TA_AVB_CMD_READ_PERSIST_VALUE  4

/*
 * Writes a persistent value corresponding to the given name.
 *
 * in   Params[0].Union.Memory:       persistent value name
 * in   Params[1].Union.Memory:       persistent value buffer to write
 */
#define TA_AVB_CMD_WRITE_PERSIST_VALUE  5

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

/**
  Init the optee interface for AVB

  @retval EFI_SUCCESS  Init Optee interface successfully.
**/
EFI_STATUS
AvbOpteeInterfaceInit (
  VOID
  );

/**
  Invoke an AVB TA cmd request

  @param[inout] AvbTaArg  OPTEE_INVOKE_FUNCTION_ARG for AVB TA cmd

  @retval EFI_SUCCESS     The operation completed successfully.

**/
EFI_STATUS
AvbOpteeInvoke (
  IN OUT OPTEE_INVOKE_FUNCTION_ARG  *InvokeFunctionArg
  );

#endif
