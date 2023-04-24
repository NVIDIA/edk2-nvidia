/** @file

  NVIDIA GPU Firmware Boot Complete Protocol Handler.

  Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "GpuFirmwareBootComplete.h"
#include <Protocol/PciIo.h>
#include "Protocol/GpuFirmwareBootCompleteProtocol.h"
#include "core/GPUMemoryInfo.h" // check required
#include "core/GPUSupport.h"    //

EFI_STATUS
EFIAPI
GpuFirmwareBootCompletGetBootStatus (
  IN NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL  *This,
  OUT BOOLEAN                                    *BootComplete
  );

NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PRIVATE_DATA  mPrivateDataGfwBootCompleteTemplate = {
  /* .Signature */ NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PRIVATE_DATA_SIGNATURE,
  /* .Handle */ NULL,
  /* .GpuFirmwareBootCompleteProtocol */ {
    GpuFirmwareBootCompletGetBootStatus
  }
};

/** Boot Status check of the GPU Firmware Boot Complete Protocol
    @param[in]  This                              NVIDIA GPU Firmware Boot Complete Get Boot Status Protocol
    @param[out] BootComplete                      Boot Complete boolean status.
    @retval EFI_STATUS      EFI_SUCCESS           - Successfully installed protocol on Handle
                            EFI_INVALID_PARAMETER - Protocol memory allocation failed.
**/
EFI_STATUS
EFIAPI
GpuFirmwareBootCompletGetBootStatus (
  IN NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL  *This,
  OUT BOOLEAN                                    *BootComplete
  )
{
  EFI_STATUS                                      Status;
  NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PRIVATE_DATA  *Private;
  EFI_PCI_IO_PROTOCOL                             *PciIo;
  BOOLEAN                                         bFirmwareComplete = FALSE;

  Private = NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PRIVATE_DATA_FROM_THIS (This);

  if (NULL == Private) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  // Protocol is installed and managed already, just get instance
  Status = gBS->OpenProtocol (
                  Private->ControllerHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  ASSERT_EFI_ERROR (Status);
  DEBUG ((DEBUG_INFO, "%a: OpenProtocol on PciIo returned:'%r'\n", __FUNCTION__, Status));
  DEBUG ((DEBUG_INFO, "%a: PciIo ProtocolInstance: '%p' on '%p'\n", __FUNCTION__, PciIo, Private->ControllerHandle));

  if (PciIo != NULL) {
    Status = CheckGfwInitComplete (PciIo, &bFirmwareComplete);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    DEBUG_CODE_BEGIN ();
    DEBUG ((DEBUG_INFO, "%a: GPU Firmware Boot Complete Protocol status:'%r'\n", __FUNCTION__, Status));
    DEBUG ((DEBUG_INFO, "%a: GpuFirmwareBootCompleteProtocol 'CheckGfwInitComplete' for instance:'%p', complete '%u'\n", __FUNCTION__, PciIo, bFirmwareComplete));
    DEBUG_CODE_END ();

    if (NULL != BootComplete) {
      *BootComplete = bFirmwareComplete;
    }
  }

  return Status;
}

///
/// Install/Uninstall protocol
///

/** Install the GPU Firmware Boot Complete Protocol from the Controller Handle
    @param[in] Handle       Controller Handle to install  the protocol on
    @retval EFI_STATUS      EFI_SUCCESS          - Successfully installed protocol on Handle
                            EFI_OUT_OF_RESOURCES - Protocol memory allocation failed.
                            (pass through OpenProtocol)
                            (pass through InstallMultipleProtocolInterfaces)
**/
EFI_STATUS
EFIAPI
InstallGpuFirmwareBootCompleteProtocolInstance (
  IN EFI_HANDLE  Handle
  )
{
  EFI_STATUS                                      Status = EFI_SUCCESS;
  NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL      *GpuFirmwareBootCompleteProtocol;
  NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PRIVATE_DATA  *Private;

  //
  // Only allow a single GpuFirmwareBootCompleteProtocol instance to be installed.
  //

  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiNVIDIAGpuFirmwareBootCompleteGuid,
                  (VOID **)&GpuFirmwareBootCompleteProtocol,
                  NULL,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_INFO, "%a: GPU Firmware Boot Complete Protocol open status:'%r'\n", __FUNCTION__, Status));
  DEBUG_CODE_END ();

  if (!EFI_ERROR (Status)) {
    return EFI_ALREADY_STARTED;
  }

  //
  // Allocate GPU Firmware Boot Complete Protocol instance
  //
  Private = AllocateCopyPool (
              sizeof (NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PRIVATE_DATA),
              &mPrivateDataGfwBootCompleteTemplate
              );

  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_INFO, "%a: Handle :[%p]\n", __FUNCTION__, Handle));
  DEBUG ((DEBUG_INFO, "%a: GPU Firmware Boot Complete Protocol:fn[GpuFirmwareBootCompletGetBootStatus:'%p']\n", __FUNCTION__, GpuFirmwareBootCompletGetBootStatus));
  DEBUG_CODE_END ();

  if (NULL == Private) {
    DEBUG ((DEBUG_ERROR, "ERROR: GPU Firmware Boot Complete Protocol instance allocation failed.\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto cleanup;
  }

  Private->ControllerHandle = Handle;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Private->ControllerHandle,
                  &gEfiNVIDIAGpuFirmwareBootCompleteGuid,
                  (VOID **)&Private->GpuFirmwareBootCompleteProtocol,
                  NULL
                  );

  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_INFO, "%a: GPU Firmware Boot Complete Protocol status %r\n", __FUNCTION__, Status));
  DEBUG_CODE_END ();

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Protocol install error on Handle [%p]. Status = %r.\n", Handle, Status));
  }

cleanup:
  if (EFI_ERROR (Status)) {
    if (NULL == Private) {
      FreePool (Private);
    }
  }

  return Status;
}

/** Uninstall the GPU Firmware Boot Complete Protocol from the Controller Handle
    @param[in] Handle       Controller Handle to uninstall the protocol on
    @retval EFI_STATUS      EFI_SUCCESS
                            (pass through OpenProtocol)
                            (pass through UninstallMultipleProtocolInterfaces)
**/
EFI_STATUS
EFIAPI
UninstallGpuFirmwareBootCompleteProtocolInstance (
  IN EFI_HANDLE  Handle
  )
{
  // Needs to retrieve protocol for CR private structure access and to release pool on private data allocation.
  EFI_STATUS                                      Status;
  NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PROTOCOL      *GpuFirmwareBootComplete;
  NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PRIVATE_DATA  *Private;

  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiNVIDIAGpuFirmwareBootCompleteGuid,
                  (VOID **)&GpuFirmwareBootComplete,
                  NULL,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Protocol open error on Handle [%p]. Status = '%r'.\n", Handle, Status));
    return Status;
  }

  /* Recover Private Data from protocol instance */
  Private = NVIDIA_GPU_FIRMWARE_BOOT_COMPLETE_PRIVATE_DATA_FROM_THIS (GpuFirmwareBootComplete);
  if (NULL == Private) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  Private->ControllerHandle,
                  &gEfiNVIDIAGpuFirmwareBootCompleteGuid,
                  (VOID **)&Private->GpuFirmwareBootCompleteProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Protocol Uninstall error on Handle[%p]. Status = '%r'.\n", Handle, Status));
    return Status;
  }

  // Free allocation
  if (NULL == Private) {
    FreePool (Private);
  }

  return EFI_SUCCESS;
}
