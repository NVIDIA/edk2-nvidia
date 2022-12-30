/** @file

  NVIDIA GPU Firmware Boot Complete Protocol interface declaration.

  Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL_H__
#define __GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL_H__

/* gEfiNVIDIAGpuFirmwareBootCompleteGuid */

typedef struct _NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL;

/**
  Return a boolean value reflecting the state of the firmware boot.

  @param[in]   This             Instance of GPU DSD AML generation protocol
  @param[out]  BootComplete     Return pointer to the boolean to hold boot complete status (TRUE, FALSE)
  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_READY         There is no Configuration Manager available for the Gpu instance.
  @retval EFI_INVALID_PARAMETER This or Node was NULL.
*/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL_GET_BOOT_STATE)(
  IN NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL *This,
  OUT BOOLEAN *BootComplete
  );

// NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL protocol structure.
struct _NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL {
  NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL_GET_BOOT_STATE    GetBootCompleteState;
};

#endif // __GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL_H__
