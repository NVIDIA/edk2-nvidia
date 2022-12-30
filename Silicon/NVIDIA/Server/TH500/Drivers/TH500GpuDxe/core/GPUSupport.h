/** @file

  NVIDIA GPU support structures and prototypes.

  Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

///
/// GPUSupport.h: GPU Support routines.
///

#ifndef __GPU_SUPPORT_H__
#define __GPU_SUPPORT_H__

#include <Uefi.h>
#include <Protocol/PciIo.h>

typedef enum {
  GPU_MODE_EH  = 0,
  GPU_MODE_SHH = 1,
  GPU_MODE_EHH = 2
} GPU_MODE;

/** Returns the Mode of the GPU

  @param[in]  PciIo   PciIo protocol of controller handle to check status on
  @param[out] GpuMode Mode of controller associated with the PciIo protocol passed as a parameter
  @retval status
            EFI_SUCCESS
            EFI_INVALID_PARAMETER
**/
EFI_STATUS
EFIAPI
CheckGpuMode (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  OUT GPU_MODE            *GpuMode
  );

/** Returns the State of the firmware initialization for the GPU

  @param[in]    PciIo         PciIo protocol of controller handle to check status on
  @param[out]   bInitComplete boolean of firmware initialization completion check
  @retval status
            EFI_SUCCESS
            EFI_INVALID_PARAMETER
**/
EFI_STATUS
EFIAPI
CheckGfwInitComplete (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  OUT BOOLEAN             *bInitComplete
  );

/** Returns the Memory Size for the GPU

  @param[in] ControllerHandle - Controller Handle to obtain the GPU Memory Information from

  @retval UINT64 containing the GPU MemSize
*/
UINT64
EFIAPI
GetGPUMemSize (
  IN EFI_HANDLE  ControllerHandle
  );

#endif // __GPU_SUPPORT_H__
