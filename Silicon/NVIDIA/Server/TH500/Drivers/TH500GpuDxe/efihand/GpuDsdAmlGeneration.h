/** @file

  NVIDIA GPU DSD AML Generation Protocol Handler private data,
    containing record macro and install/uninstall prototypes.

  SPDX-FileCopyrightText: Copyright (c) 2022-2024, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __GPU_DSD_AML_GENERATION_H__
#define __GPU_DSD_AML_GENERATION_H__

// #include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/GpuDsdAmlGenerationProtocol.h>
#include <Protocol/PciIo.h>

// Strings and Signature data
// Private data signature
#define NVIDIA_GPU_DSD_AML_GENERATION_PRIVATE_DATA_SIGNATURE  SIGNATURE_32 ('N','G','D','A')

#pragma pack(1)

typedef struct {
  UINT32                                    Signature;

  EFI_HANDLE                                Handle;

  NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL    GpuDsdAmlGenerationProtocol;
} NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL_PRIVATE_DATA;

#pragma pack()

// Private Data Containing Record macro
#define NVIDIA_GPU_DSD_AML_GENERATION_PRIVATE_DATA_FROM_THIS(a) \
    CR (a, NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL_PRIVATE_DATA, GpuDsdAmlGenerationProtocol, NVIDIA_GPU_DSD_AML_GENERATION_PRIVATE_DATA_SIGNATURE);

/** Install the GPU DSD AML Generation Protocol on the Controller Handle
    @param[in] Handle       Controller Handle to install the protocol on
    @param[in] GpuFamily    Gpu family detected
    @retval EFI_STATUS      EFI_SUCCESS          - Successfully installed protocol on Handle
                            EFI_OUT_OF_RESOURCES - Protocol memory allocation failed.
                            (pass through OpenProtocol)
                            (pass through InstallMultipleProtocolInterfaces)
**/
EFI_STATUS
EFIAPI
InstallGpuDsdAmlGenerationProtocolInstance (
  IN EFI_HANDLE         Handle,
  IN NVIDIA_GPU_FAMILY  GpuFamily
  );

/** Uninstall the GPU DSD AML Generation Protocol from the Controller Handle
    @param[in] Handle       Controller Handle to uninstall the protocol on
    @retval EFI_STATUS      EFI_SUCCESS
                            (pass through OpenProtocol)
                            (pass through UninstallMultipleProtocolInterfaces)
**/
EFI_STATUS
EFIAPI
UninstallGpuDsdAmlGenerationProtocolInstance (
  IN EFI_HANDLE  Handle
  );

#endif // __GPU_DSD_AML_GENERATION_H__
