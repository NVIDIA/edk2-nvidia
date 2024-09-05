/** @file
*
*  Kernel Args Protocol definition.
*
*  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __KERNEL_ARGS_PROTOCOL_H__
#define __KERNEL_ARGS_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>

//
// Define for forward reference.
//
typedef struct _NVIDIA_KERNEL_ARGS_PROTOCOL NVIDIA_KERNEL_ARGS_PROTOCOL;

/**
  Copy the new KernelArgs into the KernelArgsProtocol, freeing up the old string if necessary

  @param[in]  This       The KernelArgsProtocol
  @param[in]  NewArgs    The new KernelArgs string (may be NULL)

  @retval EFI_SUCCESS            The operation completed successfully.
  @retval EFI_INVALID_PARAMETER  "This" is NULL.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate space for the new args.

**/
typedef
  EFI_STATUS(*NVIDIA_KERNEL_ARGS_UPDATE)(
  NVIDIA_KERNEL_ARGS_PROTOCOL  *This,
  CONST CHAR16                 *NewArgs
  );

// NVIDIA_KERNEL_ARGS_PROTOCOL protocol structure.
struct _NVIDIA_KERNEL_ARGS_PROTOCOL {
  CHAR16                       *KernelArgs;
  NVIDIA_KERNEL_ARGS_UPDATE    UpdateKernelArgs;
};

#endif /* __KERNEL_ARGS_PROTOCOL_H__ */
