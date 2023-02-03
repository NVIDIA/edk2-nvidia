/** @file
#include "BpmpIpcDxePrivate.h"
  HspDoorbell protocol implementation for BPMP IPC driver.

  Copyright (c) 2018-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BpmpIpcDxePrivate.h"
#include "HspDoorbellPrivate.h"
#include <Library/IoLib.h>

HSP_MASTER_ID  DoorbellToMaster[HspDoorbellMax] = {
  HSP_MASTER_DPMU,
  HSP_MASTER_CCPLEX,
  HSP_MASTER_SECURE_CCPLEX,
  HSP_MASTER_BPMP,
  HSP_MASTER_SPE,
  HSP_MASTER_SCE,
  HSP_MASTER_APE,
};

/**
  This function allows for a remote IPC to the BPMP firmware to be executed.

  @param[in]     This                The instance of the NVIDIA_HSP_DOORBELL_PROTOCOL.
  @param[in]     Doorbell            Doorbell to ring

  @return EFI_SUCCESS               The doorbell has been rung.
  @return EFI_UNSUPPORTED           The doorbell is not supported.
  @return EFI_DEVICE_ERROR          Failed to ring the doorbell.
  @return EFI_NOT_READY             Doorbell is not ready to receive from CCPLEX
**/
EFI_STATUS
HspDoorbellRingDoorbell (
  IN  NVIDIA_HSP_DOORBELL_PROTOCOL  *This,
  IN  HSP_DOORBELL_ID               Doorbell
  )
{
  NVIDIA_HSP_DOORBELL_PRIVATE_DATA  *PrivateData = NULL;

  PrivateData = HSP_DOORBELL_PRIVATE_DATA_FROM_THIS (This);

  if (Doorbell >= HspDoorbellMax) {
    return EFI_UNSUPPORTED;
  }

  if (0 == MmioBitFieldRead32 (
             PrivateData->DoorbellLocation[Doorbell] + HSP_DB_REG_ENABLE,
             HSP_MASTER_CCPLEX,
             HSP_MASTER_CCPLEX
             ))
  {
    return EFI_NOT_READY;
  }

  // Ring doorbell
  MmioWrite32 (PrivateData->DoorbellLocation[Doorbell] + HSP_DB_REG_TRIGGER, 1);

  return EFI_SUCCESS;
}

/**
  This function enables the channel for communication with the CCPLEX.

  @param[in]     This                The instance of the NVIDIA_HSP_DOORBELL_PROTOCOL.
  @param[in]     Doorbell            Doorbell of the channel to enable

  @return EFI_SUCCESS               The channel is enabled.
  @return EFI_UNSUPPORTED           The channel is not supported.
  @return EFI_DEVICE_ERROR          Failed to enable channel.
**/
EFI_STATUS
HspDoorbellEnableChannel (
  IN  NVIDIA_HSP_DOORBELL_PROTOCOL  *This,
  IN  HSP_DOORBELL_ID               Doorbell
  )
{
  NVIDIA_HSP_DOORBELL_PRIVATE_DATA  *PrivateData = NULL;
  HSP_MASTER_ID                     Master;
  UINT32                            Timeout = PcdGet32 (PcdHspDoorbellTimeout) / TIMEOUT_STALL_US;

  PrivateData = HSP_DOORBELL_PRIVATE_DATA_FROM_THIS (This);

  if (Doorbell >= HspDoorbellMax) {
    return EFI_UNSUPPORTED;
  }

  Master = DoorbellToMaster[Doorbell];

  MmioBitFieldWrite32 (
    PrivateData->DoorbellLocation[HspDoorbellCcplex] + HSP_DB_REG_ENABLE,
    Master,
    Master,
    1
    );

  DEBUG ((DEBUG_ERROR, "%a: Waiting for HSP Doorbell Channel Enabled.\r\n", __FUNCTION__));
  while (0 == MmioBitFieldRead32 (
                PrivateData->DoorbellLocation[Doorbell] + HSP_DB_REG_ENABLE,
                HSP_MASTER_CCPLEX,
                HSP_MASTER_CCPLEX
                ))
  {
    gBS->Stall (TIMEOUT_STALL_US);
    if (Timeout != 0) {
      Timeout--;
      if (Timeout == 0) {
        return EFI_NOT_READY;
      }
    }
  }

  DEBUG ((DEBUG_ERROR, "%a: HSP Doorbell Channel Enabled.\r\n", __FUNCTION__));

  return EFI_SUCCESS;
}

/**
  This routine starts the HspDoorbell protocol on the device.

  @param NonDiscoverableProtocol  A pointer to the NonDiscoverableProtocol.
  @param Controller               Handle of device to bind driver to..

  @retval EFI_SUCCESS             This driver is added to this device.
  @retval EFI_ALREADY_STARTED     This driver is already running on this device.
  @retval other                   Some error occurs when binding this driver to this device.

**/
EFI_STATUS
EFIAPI
HspDoorbellProtocolInit (
  IN EFI_HANDLE               *Controller,
  IN NON_DISCOVERABLE_DEVICE  *NonDiscoverableProtocol
  )
{
  EFI_STATUS                        Status;
  NVIDIA_HSP_DOORBELL_PRIVATE_DATA  *PrivateData = NULL;
  EFI_PHYSICAL_ADDRESS              HspBase;
  UINTN                             Index;
  HSP_DIMENSIONING_DATA             HspDimensioningData;

  PrivateData = AllocateZeroPool (sizeof (NVIDIA_HSP_DOORBELL_PRIVATE_DATA));
  if (NULL == PrivateData) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  PrivateData->Signature                      = HSP_DOORBELL_SIGNATURE;
  PrivateData->DoorbellProtocol.RingDoorbell  = HspDoorbellRingDoorbell;
  PrivateData->DoorbellProtocol.EnableChannel = HspDoorbellEnableChannel;

  // First resource must be MMIO
  if ((NonDiscoverableProtocol->Resources == NULL) ||
      (NonDiscoverableProtocol->Resources->Desc != ACPI_ADDRESS_SPACE_DESCRIPTOR) ||
      (NonDiscoverableProtocol->Resources->ResType != ACPI_ADDRESS_SPACE_TYPE_MEM))
  {
    DEBUG ((DEBUG_ERROR, "%a: Invalid node resources.\r\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto ErrorExit;
  }

  HspBase = NonDiscoverableProtocol->Resources->AddrRangeMin;

  HspDimensioningData.RawValue = MmioRead32 (HspBase + HSP_DIMENSIONING);

  HspBase += HSP_COMMON_REGION_SIZE;                                               /* skip common regs */
  HspBase += HspDimensioningData.SharedMailboxes << HSP_MAILBOX_SHIFT_SIZE;        /* skip shared mailboxes */
  HspBase += HspDimensioningData.SharedSemaphores << HSP_SEMAPHORE_SHIFT_SIZE;     /* skip shared semaphores */
  HspBase += HspDimensioningData.ArbitratedSemaphores << HSP_SEMAPHORE_SHIFT_SIZE; /* skip arbitrated semaphores */

  for (Index = HspDoorbellDpmu; Index < HspDoorbellMax; Index++) {
    PrivateData->DoorbellLocation[Index] = HspBase;
    HspBase                             += HSP_DOORBELL_REGION_SIZE;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  Controller,
                  &gNVIDIAHspDoorbellProtocolGuid,
                  &PrivateData->DoorbellProtocol,
                  NULL
                  );
ErrorExit:
  if (EFI_ERROR (Status)) {
    if (NULL != PrivateData) {
      FreePool (PrivateData);
    }
  }

  return Status;
}
