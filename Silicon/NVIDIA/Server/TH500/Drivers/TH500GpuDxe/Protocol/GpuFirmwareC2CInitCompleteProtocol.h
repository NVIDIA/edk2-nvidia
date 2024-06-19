/** @file

  NVIDIA GPU Firmware C2C Init Complete Protocol interface declaration.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL_H__
#define __GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL_H__

typedef struct _NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL;

/**
  Return a boolean value reflecting the state of the C2C Init Complete firmware boot state.

  @param[in]   This             Instance of GPU Firmware C2C Init complete generation protocol
  @param[out]  C2CInitComplete  Return pointer to the boolean to hold C2C Init complete status (TRUE, FALSE)
  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_READY         PciIo protocol not configured.
  @retval EFI_INVALID_PARAMETER This or Node was NULL.
*/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL_GET_C2CINIT_STATE)(
  IN NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL *This,
  OUT BOOLEAN *C2CInitComplete
  );

// NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL protocol structure.
struct _NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL {
  NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL_GET_C2CINIT_STATE    GetC2CInitCompleteState;
};

#endif // __GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL_H__
