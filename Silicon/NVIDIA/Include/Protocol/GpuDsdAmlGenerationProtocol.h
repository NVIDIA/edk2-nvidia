/** @file

  NVIDIA GPU DSD AML Generation Protocol interface declaration.

  SPDX-FileCopyrightText: Copyright (c) 2022-2024, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __GPU_DSD_AML_GENERATION_PROTOCOL_H__
#define __GPU_DSD_AML_GENERATION_PROTOCOL_H__

#include <Library/AcpiHelperLib.h>
#include <Library/AmlLib/AmlLib.h>

typedef struct _NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL;

typedef enum {
  NVIDIA_GPU_HOPPER,
  NVIDIA_GPU_BLACKWELL,
  NVIDIA_GPU_UNKNOWN
} NVIDIA_GPU_FAMILY;

/**
  Return a pointer to the DSD AML node being generated for the Gpu node

  @param[in]   This             Instance of GPU DSD AML generation protocol
  @param[out]  Node             Return pointer to the AML DSD Node generated for the Gpu

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory to generate the AML DSD Node.
  @retval EFI_NOT_READY         There is no Configuration Manager available for the Gpu instance.
  @retval EFI_INVALID_PARAMETER This or Node was NULL.
*/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_GPU_DSD_AML_GENERATION_GET_DSD_NODE)(
  IN NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL *This,
  OUT AML_NODE_HANDLE *Node
  );

/**
  Return a pointer to the memory size of the GPU

  @param[in]   This             Instance of GPU DSD AML generation protocol
  @param[out]  MemorySize       Return pointer to the Memory Size
  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_READY         There is no Configuration Manager available for the Gpu instance.
  @retval EFI_INVALID_PARAMETER This or Node was NULL.
*/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_GPU_DSD_AML_GENERATION_GET_MEMORY_SIZE)(
  IN NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL *This,
  OUT UINT64 *MemorySize
  );

/**
  Return a pointer to the base physical address of the EGM carveout for the socket.

  @param[in]   This             Instance of GPU DSD AML generation protocol
  @param[out]  EgmBasePa        Return pointer to the EGM Bask PA
  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_READY         There is no Configuration Manager available for the Gpu instance.
  @retval EFI_INVALID_PARAMETER This or Node was NULL.
*/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_GPU_DSD_AML_GENERATION_GET_EGM_BASE_PA)(
  IN NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL *This,
  OUT UINT64 *EgmBasePa
  );

/**
  Return a pointer to the size of the EGM carveout for the socket.

  @param[in]   This             Instance of GPU DSD AML generation protocol
  @param[out]  EgmSize          Return pointer to the EGM Size
  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_READY         There is no Configuration Manager available for the Gpu instance.
  @retval EFI_INVALID_PARAMETER This or Node was NULL.
*/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_GPU_DSD_AML_GENERATION_GET_EGM_SIZE)(
  IN NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL *This,
  OUT UINT64 *EgmSize
  );

// NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL protocol structure.
struct _NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL {
  NVIDIA_GPU_FAMILY                                GpuFamily;
  NVIDIA_GPU_DSD_AML_GENERATION_GET_DSD_NODE       GetDsdNode;
  NVIDIA_GPU_DSD_AML_GENERATION_GET_MEMORY_SIZE    GetMemorySize;
  NVIDIA_GPU_DSD_AML_GENERATION_GET_EGM_BASE_PA    GetEgmBasePa;
  NVIDIA_GPU_DSD_AML_GENERATION_GET_EGM_SIZE       GetEgmSize;
};

#endif // __GPU_DSD_AML_GENERATION_PROTOCOL_H__
