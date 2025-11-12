/** @file
  EDK2 API for OpteeTpmDeviceLib

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __OPTEE_TPM_DEVICE_LIB_H__
#define __OPTEE_TPM_DEVICE_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define FTPM_SUBMIT_COMMAND  (0)

VOID EFIAPI
ExitBootServicesCallBack (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  );

#endif // __OPTEE_TPM_DEVICE_LIB_H__
