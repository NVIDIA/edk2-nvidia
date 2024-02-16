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

  @param[in]     DoorbellLocation    A pointer to HSP Doorbell address.
  @param[in]     Doorbell            Doorbell to ring

  @return EFI_SUCCESS               The doorbell has been rung.
  @return EFI_UNSUPPORTED           The doorbell is not supported.
  @return EFI_DEVICE_ERROR          Failed to ring the doorbell.
  @return EFI_NOT_READY             Doorbell is not ready to receive from CCPLEX
**/
EFI_STATUS
HspDoorbellRingDoorbell (
  IN  EFI_PHYSICAL_ADDRESS  *DoorbellLocation,
  IN  HSP_DOORBELL_ID       Doorbell
  )
{
  if (Doorbell >= HspDoorbellMax) {
    return EFI_UNSUPPORTED;
  }

  if (0 == MmioBitFieldRead32 (
             DoorbellLocation[Doorbell] + HSP_DB_REG_ENABLE,
             HSP_MASTER_CCPLEX,
             HSP_MASTER_CCPLEX
             ))
  {
    return EFI_NOT_READY;
  }

  // Ring doorbell
  MmioWrite32 (DoorbellLocation[Doorbell] + HSP_DB_REG_TRIGGER, 1);

  return EFI_SUCCESS;
}

/**
  This function enables the channel for communication with the CCPLEX.

  @param[in]     DoorbellLocation    A pointer to HSP Doorbell address.
  @param[in]     Doorbell            Doorbell of the channel to enable

  @return EFI_SUCCESS               The channel is enabled.
  @return EFI_UNSUPPORTED           The channel is not supported.
  @return EFI_DEVICE_ERROR          Failed to enable channel.
**/
EFI_STATUS
HspDoorbellEnableChannel (
  IN  EFI_PHYSICAL_ADDRESS  *DoorbellLocation,
  IN  HSP_DOORBELL_ID       Doorbell
  )
{
  HSP_MASTER_ID  Master;
  UINT32         Timeout = PcdGet32 (PcdHspDoorbellTimeout) / TIMEOUT_STALL_US;

  if (Doorbell >= HspDoorbellMax) {
    return EFI_UNSUPPORTED;
  }

  Master = DoorbellToMaster[Doorbell];

  MmioBitFieldWrite32 (
    DoorbellLocation[HspDoorbellCcplex] + HSP_DB_REG_ENABLE,
    Master,
    Master,
    1
    );

  DEBUG ((DEBUG_ERROR, "%a: Waiting for HSP Doorbell Channel Enabled.\r\n", __FUNCTION__));
  while (0 == MmioBitFieldRead32 (
                DoorbellLocation[Doorbell] + HSP_DB_REG_ENABLE,
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
  This routine initializes HSP Doorbell on the device..

  @param DTNodeInfo          Info regarding HSP device tree base address,node, offset,
                              device type and init function.
  @param HspDevice           A pointer to the HspDevice.
  @param DoorbellLocation    A Pointer to HSP Doorbell address.

  @retval EFI_SUCCESS             This driver is added to this device.
  @retval EFI_ALREADY_STARTED     This driver is already running on this device.
  @retval other                   Some error occurs when binding this driver to this device.

**/
EFI_STATUS
HspDoorbellInit (
  IN      NVIDIA_DT_NODE_INFO      *DTNodeInfo,
  IN      NON_DISCOVERABLE_DEVICE  *HspDevice,
  IN OUT  EFI_PHYSICAL_ADDRESS     *DoorbellLocation
  )
{
  EFI_PHYSICAL_ADDRESS   HspBase;
  UINTN                  Index;
  HSP_DIMENSIONING_DATA  HspDimensioningData;

  // First resource must be MMIO
  if ((HspDevice->Resources == NULL) ||
      (HspDevice->Resources->Desc != ACPI_ADDRESS_SPACE_DESCRIPTOR) ||
      (HspDevice->Resources->ResType != ACPI_ADDRESS_SPACE_TYPE_MEM))
  {
    DEBUG ((DEBUG_ERROR, "%a: Invalid node resources.\r\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  HspBase = HspDevice->Resources->AddrRangeMin;

  HspDimensioningData.RawValue = MmioRead32 (HspBase + HSP_DIMENSIONING);

  HspBase += HSP_COMMON_REGION_SIZE;                                               /* skip common regs */
  HspBase += HspDimensioningData.SharedMailboxes << HSP_MAILBOX_SHIFT_SIZE;        /* skip shared mailboxes */
  HspBase += HspDimensioningData.SharedSemaphores << HSP_SEMAPHORE_SHIFT_SIZE;     /* skip shared semaphores */
  HspBase += HspDimensioningData.ArbitratedSemaphores << HSP_SEMAPHORE_SHIFT_SIZE; /* skip arbitrated semaphores */

  for (Index = HspDoorbellDpmu; Index < HspDoorbellMax; Index++) {
    DoorbellLocation[Index] = HspBase;
    HspBase                += HSP_DOORBELL_REGION_SIZE;
  }

  return EFI_SUCCESS;
}
