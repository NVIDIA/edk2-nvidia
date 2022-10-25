/** @file

  PCD Patching module for SPI-NOR Data.

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MmServicesTableLib.h>
#include <Library/PcdLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>

#include <Protocol/FirmwareVolumeBlock.h>
#include <Protocol/SmmFirmwareVolumeBlock.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>

#include "FvbPrivate.h"

/**
  GetFvbCountAndBuffer

  Get all the handles that have installed the FVB GUID.

  @param OUT NumberHandles          Number of Handles with the FVB GUID.
  @param OUT Buffer                 Buffer of Handles.
**/
STATIC
EFI_STATUS
GetFvbCountAndBuffer (
  OUT UINTN       *NumberHandles,
  OUT EFI_HANDLE  **Buffer
  )
{
  EFI_STATUS  Status;
  UINTN       BufferSize;

  if ((NumberHandles == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  BufferSize     = 0;
  *NumberHandles = 0;
  *Buffer        = NULL;
  Status         = gMmst->MmLocateHandle (
                            ByProtocol,
                            &gEfiSmmFirmwareVolumeBlockProtocolGuid,
                            NULL,
                            &BufferSize,
                            *Buffer
                            );
  if (EFI_ERROR (Status) && (Status != EFI_BUFFER_TOO_SMALL)) {
    return EFI_NOT_FOUND;
  }

  *Buffer = AllocatePool (BufferSize);
  if (*Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gMmst->MmLocateHandle (
                    ByProtocol,
                    &gEfiSmmFirmwareVolumeBlockProtocolGuid,
                    NULL,
                    &BufferSize,
                    *Buffer
                    );

  *NumberHandles = BufferSize / sizeof (EFI_HANDLE);
  if (EFI_ERROR (Status)) {
    *NumberHandles = 0;
    FreePool (*Buffer);
    *Buffer = NULL;
  }

  return Status;
}

/**
  Fixup the Pcd values for variable storage

  Fixup the PCD values that the Variable driver needs, these buffer addresses
  are dynamically allocated in the FVB driver, currently this is assumed to be
  the SPI-NOR driver.

  @retval EFI_SUCCESS            Protocol was found and PCDs patched up
  @retval EFI_INVALID_PARAMETER  Invalid parameter
  @retval EFI_NOT_FOUND          Protocol interface not found
**/
EFI_STATUS
EFIAPI
StandaloneMmFixupPcdConstructor (
  VOID
  )
{
  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *Fvb;
  EFI_STATUS                          Status;
  EFI_HANDLE                          *HandleBuffer;
  UINTN                               HandleCount;
  NVIDIA_FVB_PRIVATE_DATA             *Private;
  UINTN                               Index;
  TEGRA_PLATFORM_TYPE                 PlatformType;
  TEGRA_BOOT_TYPE                     TegraBootType;
  BOOLEAN                             Fbc;

  /*
   * If we are here and the PcdEmuVariableNvModeEnable is already set, return.
   */
  if (PcdGetBool (PcdEmuVariableNvModeEnable)) {
    return EFI_SUCCESS;
  }

  /* In Jetson deployments, if the QSPI MMIO region isn't found then
   * return, this could be an RPMB platform.
   */
  if (!IsQspiPresent () && IsOpteePresent ()) {
    return EFI_SUCCESS;
  }

  TegraBootType = GetBootType ();
  Fbc           = InFbc ();

  /* Fallback to emulated store in certain boot flavors */
  if ((Fbc == FALSE) || (TegraBootType == TegrablBootRcm)) {
    PatchPcdSetBool (PcdEmuVariableNvModeEnable, TRUE);
    DEBUG ((
      DEBUG_ERROR,
      "Falling back to emulated store Boot Type %d fbc %d\n",
      TegraBootType,
      Fbc
      ));
    return EFI_SUCCESS;
  }

  Status = GetFvbCountAndBuffer (&HandleCount, &HandleBuffer);
  if (EFI_ERROR (Status)) {
    PlatformType = GetPlatformTypeMm ();

    /* If we're doing FD boot on a simulator, allow falling back
       to emulated variables.
     */
    if (PlatformType == TEGRA_PLATFORM_VDK) {
      PatchPcdSetBool (PcdEmuVariableNvModeEnable, TRUE);
      DEBUG ((DEBUG_ERROR, "%a:Fvb not found using Emulated\n", __FUNCTION__));
      return EFI_SUCCESS;
    }
  }

  /* Assert if we are doing a regular boot on a silicon/FPGA platform and
   * we haven't found the variable partitions.
   */
  ASSERT_EFI_ERROR (Status);

  for (Index = 0; Index < HandleCount; Index += 1, Fvb = NULL) {
    Status = gMmst->MmHandleProtocol (
                      HandleBuffer[Index],
                      &gEfiSmmFirmwareVolumeBlockProtocolGuid,
                      (VOID **)&Fvb
                      );
    if (EFI_ERROR (Status)) {
      Status = EFI_NOT_FOUND;
      break;
    }

    Private = BASE_CR (Fvb, NVIDIA_FVB_PRIVATE_DATA, FvbProtocol);
    if (Private != NULL) {
      if (Private->Signature == NVIDIA_FVB_SIGNATURE) {
        PatchPcdSet64 (PcdFlashNvStorageVariableBase64, Private->PartitionAddress);
        PatchPcdSet32 (PcdFlashNvStorageVariableSize, Private->PartitionSize);
        DEBUG ((
          DEBUG_INFO,
          "%a: Fixup PcdFlashNvStorageVariableSize: 0x%x\n",
          __FUNCTION__,
          Private->PartitionSize
          ));
      } else if (Private->Signature == NVIDIA_FWB_SIGNATURE) {
        PatchPcdSet64 (PcdFlashNvStorageFtwWorkingBase64, Private->PartitionAddress);
        PatchPcdSet32 (PcdFlashNvStorageFtwWorkingSize, Private->PartitionSize);
        DEBUG ((
          DEBUG_INFO,
          "%a: Fixup PcdFlashNvStorageFtwWorkingSize: 0x%x\n",
          __FUNCTION__,
          Private->PartitionSize
          ));
      } else if (Private->Signature == NVIDIA_FSB_SIGNATURE) {
        PatchPcdSet64 (PcdFlashNvStorageFtwSpareBase64, Private->PartitionAddress);
        PatchPcdSet32 (PcdFlashNvStorageFtwSpareSize, Private->PartitionSize);
        DEBUG ((
          DEBUG_INFO,
          "%a: Fixup PcdFlashNvStorageFtwSpareSize: 0x%x\n",
          __FUNCTION__,
          Private->PartitionSize
          ));
      } else {
        DEBUG ((DEBUG_ERROR, "Invalid Signature 0x%x\n", Private->Signature));
      }
    }
  }

  FreePool (HandleBuffer);

  DEBUG ((
    DEBUG_INFO,
    "%a: Fixup PcdFlashNvStorageVariableBase64: 0x%lx Size 0%x\n",
    __FUNCTION__,
    PcdGet64 (PcdFlashNvStorageVariableBase64),
    PcdGet32 (PcdFlashNvStorageVariableSize)
    ));
  DEBUG ((
    DEBUG_INFO,
    "%a: Fixup PcdFlashNvStorageFtwWorkingBase64: 0x%lx Size 0x%x\n",
    __FUNCTION__,
    PcdGet64 (PcdFlashNvStorageFtwWorkingBase64),
    PcdGet32 (PcdFlashNvStorageFtwWorkingSize)
    ));
  DEBUG ((
    DEBUG_INFO,
    "%a: Fixup PcdFlashNvStorageFtwSpareBase64: 0x%lx 0x%x \n",
    __FUNCTION__,
    PcdGet64 (PcdFlashNvStorageFtwSpareBase64),
    PcdGet32 (PcdFlashNvStorageFtwSpareSize)
    ));

  return Status;
}
