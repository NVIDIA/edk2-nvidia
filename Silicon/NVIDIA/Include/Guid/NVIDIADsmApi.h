/** @file
*
*  This file defines Nvidia specific DSM methods for ACPI
*  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef NVIDIA_DSM_API_H__
#define NVIDIA_DSM_API_H__

#define NVIDIA_GPU_STATUS_DSM_GUID_STR  "47fdec2f-901b-4f92-aab4-2a34a6c37286"

#define NVIDIA_GPU_STATUS_DSM_REV  0

/*
  - Function 1 - Get GPU Containment status
    Arguments:
      None
    Return Value:
      0 - No containment
      1 - Hardware containment
      2 - Software containment
*/

#define NVIDIA_GPU_SMMU_BYPASS_DSM_GUID_STR  "6c8f20d6-1567-493d-979e-7abf18907624"
#define NVIDIA_GPU_SMMU_BYPASS_DSM_REV       0

/*
  - Function 1 - Set SMMU Bypass
    Arguments:
      Enable flag (1=enable, 0=disable)
    Return Value:
      0 - Success
      1 - UEFI support option not enabled
      2 - Error detecting socket
*/

#endif
