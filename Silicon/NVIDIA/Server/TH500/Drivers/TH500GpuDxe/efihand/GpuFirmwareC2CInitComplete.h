/** @file

    NVIDIA GPU Firmware C2C Init Complete Protocol Handler private data,
    containing record macro and install/uninstall prototypes.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __GPU_FIRMWARE_C2CINIT_COMPLETE_H__
#define __GPU_FIRMWARE_C2CINIT_COMPLETE_H__

// #include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/GpuFirmwareC2CInitCompleteProtocol.h>
#include <Protocol/PciIo.h>

// Strings and Signature data
// Private data signature
#define NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PRIVATE_DATA_SIGNATURE  SIGNATURE_32 ('N','G','F','C')

#pragma pack(1)

typedef struct {
  UINT32                                           Signature;

  EFI_HANDLE                                       ControllerHandle;

  NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL    GpuFirmwareC2CInitCompleteProtocol;
} NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PRIVATE_DATA;

#pragma pack()

// Private Data Containing Record macro
#define NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PRIVATE_DATA_FROM_THIS(a) \
    CR (a, NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PRIVATE_DATA, GpuFirmwareC2CInitCompleteProtocol, NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PRIVATE_DATA_SIGNATURE);

/** Install the GPU Firmware C2C Init Complete Protocol from the Controller Handle
    @param[in] Handle       Controller Handle to install the protocol on
    @retval EFI_STATUS      EFI_SUCCESS          - Successfully installed protocol on Handle
                            EFI_OUT_OF_RESOURCES - Protocol memory allocation failed.
                            (pass through OpenProtocol)
                            (pass through InstallMultipleProtocolInterfaces)
**/
EFI_STATUS
EFIAPI
InstallGpuFirmwareC2CInitCompleteProtocolInstance (
  IN EFI_HANDLE  Handle
  );

/** Uninstall the GPU Firmware C2C Init Complete Protocol from the Controller Handle
    @param[in] Handle       Controller Handle to uninstall the protocol on
    @retval EFI_STATUS      EFI_SUCCESS
                            (pass through OpenProtocol)
                            (pass through UninstallMultipleProtocolInterfaces)
**/
EFI_STATUS
EFIAPI
UninstallGpuFirmwareC2CInitCompleteProtocolInstance (
  IN EFI_HANDLE  Handle
  );

#endif // __GPU_FIRMWARE_C2CINIT_COMPLETE_H__
