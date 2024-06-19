/** @file

  NVIDIA GPU Firmware C2C Init Complete Protocol Handler.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "GpuFirmwareC2CInitComplete.h"
#include <Protocol/PciIo.h>
#include "Protocol/GpuFirmwareC2CInitCompleteProtocol.h"
#include "UEFIFspRpc.h"

/* Counter value to limit the output of Get C2C Init Status in polling loop */
/* Sets the value to be used as reset value when decrement counter returns zero */
#define GFC2CIC_LOGGING_TRIGGER_COUNTER  50000
static UINT32  gfc2cic_logging_trigger = 0;

EFI_STATUS
EFIAPI
GpuFirmwareC2CInitCompleteGetC2CInitStatus (
  IN NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL  *This,
  OUT BOOLEAN                                       *C2CInitComplete
  );

NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PRIVATE_DATA  mPrivateDataGfwC2CInitCompleteTemplate = {
  /* .Signature */ NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PRIVATE_DATA_SIGNATURE,
  /* .Handle */ NULL,
  /* .GpuFirmwareC2CInitCompleteProtocol */ {
    GpuFirmwareC2CInitCompleteGetC2CInitStatus
  }
};

/** C2C Init Status check of the GPU Firmware C2CInit Complete Protocol
    @param[in]  This                              NVIDIA GPU Firmware C2C Init Complete Get C2C Init Status Protocol
    @param[out] BootComplete                      C2C Init Complete boolean status.
    @retval EFI_STATUS      EFI_SUCCESS           - Successfully installed protocol on Handle
                            EFI_INVALID_PARAMETER - Protocol memory allocation failed.
**/
EFI_STATUS
EFIAPI
GpuFirmwareC2CInitCompleteGetC2CInitStatus (
  IN NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL  *This,
  OUT BOOLEAN                                       *C2CInitComplete
  )
{
  EFI_STATUS                                         Status;
  NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PRIVATE_DATA  *Private;
  EFI_PCI_IO_PROTOCOL                                *PciIo;
  UINT32                                             ResponsePayload  = 0;
  BOOLEAN                                            bC2CInitComplete = FALSE;
  BOOLEAN                                            bVerboseLog      = (gfc2cic_logging_trigger == 0);

  DEBUG_CODE_BEGIN ();
  if (bVerboseLog) {
    gfc2cic_logging_trigger = GFC2CIC_LOGGING_TRIGGER_COUNTER;
  } else {
    gfc2cic_logging_trigger--;
  }

  DEBUG_CODE_END ();

  Private = NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PRIVATE_DATA_FROM_THIS (This);

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

  DEBUG_CODE_BEGIN ();
  if (bVerboseLog) {
    DEBUG ((DEBUG_INFO, "%a: OpenProtocol on PciIo returned:'%r'\n", __FUNCTION__, Status));
    DEBUG ((DEBUG_INFO, "%a: PciIo ProtocolInstance: '%p' on '%p'\n", __FUNCTION__, PciIo, Private->ControllerHandle));
  }

  DEBUG_CODE_END ();

  if (PciIo != NULL) {
    Status = FspRpcGetC2CInitStatus (PciIo, &ResponsePayload);
    if (EFI_ERROR (Status)) {
      return Status;
    } else {
      bC2CInitComplete = (ResponsePayload == 0x000000ff);
    }

    DEBUG_CODE_BEGIN ();
    if (bVerboseLog) {
      DEBUG ((DEBUG_INFO, "%a: GPU Firmware C2C Init Complete Protocol status:'%r'\n", __FUNCTION__, Status));
      DEBUG ((DEBUG_INFO, "%a: GpuFirmwareC2CInitCompleteProtocol 'CheckGfwC2CInitComplete' for instance:'%p', complete '%u'\n", __FUNCTION__, PciIo, bC2CInitComplete));
    }

    DEBUG_CODE_END ();

    if (NULL != C2CInitComplete) {
      *C2CInitComplete = bC2CInitComplete;
    }
  }

  return Status;
}

///
/// Install/Uninstall protocol
///

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
  )
{
  EFI_STATUS                                         Status = EFI_SUCCESS;
  NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL      *GpuFirmwareC2CInitCompleteProtocol;
  NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PRIVATE_DATA  *Private;

  //
  // Only allow a single GpuFirmwareBootCompleteProtocol instance to be installed per controller.
  //   Does this need to be a TEST_PROTOCOL
  //

  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiNVIDIAGpuFirmwareC2CInitCompleteGuid,
                  (VOID **)&GpuFirmwareC2CInitCompleteProtocol,
                  NULL,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_INFO, "%a: GPU Firmware C2C Init Complete Protocol open status:'%r'\n", __FUNCTION__, Status));
  DEBUG_CODE_END ();

  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: GPU Firmware C2C Init Complete Protocol open status:'%r'\n", __FUNCTION__, Status));
    return EFI_ALREADY_STARTED;
  }

  //
  // Allocate GPU Firmware C2C Init Complete Protocol instance
  //
  Private = AllocateCopyPool (
              sizeof (NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PRIVATE_DATA),
              &mPrivateDataGfwC2CInitCompleteTemplate
              );

  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_INFO, "%a: Handle :[%p]\n", __FUNCTION__, Handle));
  DEBUG ((DEBUG_INFO, "%a: GPU Firmware C2C Init Complete Protocol:fn[GpuFirmwareC2CInitCompleteGetC2CInitStatus:'%p']\n", __FUNCTION__, GpuFirmwareC2CInitCompleteGetC2CInitStatus));
  DEBUG_CODE_END ();

  if (NULL == Private) {
    DEBUG ((DEBUG_ERROR, "ERROR: GPU Firmware C2C Init Complete Protocol instance allocation failed.\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto cleanup;
  }

  Private->ControllerHandle = Handle;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Private->ControllerHandle,
                  &gEfiNVIDIAGpuFirmwareC2CInitCompleteGuid,
                  (VOID **)&Private->GpuFirmwareC2CInitCompleteProtocol,
                  NULL
                  );

  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_INFO, "%a: GPU Firmware C2C Init Complete Protocol status %r\n", __FUNCTION__, Status));
  DEBUG ((DEBUG_INFO, "%a: GPU Firmware C2C Init Complete Protocol Installed Instance [%p] on Handle [%p]\n", __FUNCTION__, Private->GpuFirmwareC2CInitCompleteProtocol, Handle));
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
  )
{
  // Needs to retrieve protocol for CR private structure access and to release pool on private data allocation.
  EFI_STATUS                                         Status;
  NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PROTOCOL      *GpuFirmwareC2CInitComplete;
  NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PRIVATE_DATA  *Private;

  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiNVIDIAGpuFirmwareC2CInitCompleteGuid,
                  (VOID **)&GpuFirmwareC2CInitComplete,
                  NULL,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Protocol open error on Handle [%p]. Status = '%r'.\n", Handle, Status));
    return Status;
  }

  /* Recover Private Data from protocol instance */
  Private = NVIDIA_GPU_FIRMWARE_C2CINIT_COMPLETE_PRIVATE_DATA_FROM_THIS (GpuFirmwareC2CInitComplete);
  if (NULL == Private) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  Private->ControllerHandle,
                  &gEfiNVIDIAGpuFirmwareC2CInitCompleteGuid,
                  (VOID **)&Private->GpuFirmwareC2CInitCompleteProtocol,
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
