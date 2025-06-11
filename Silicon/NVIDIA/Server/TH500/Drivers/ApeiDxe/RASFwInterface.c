/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/
#include "Apei.h"

static UINT16  RasFwVmId = 0;

/**
 * Allocate RX/TX buffers for FFA communication.
 * Note: the RX/TX buffers are shared for the entire NS world, so they must be
 * freed after use.
 *
 * @param[in]  pages           Number of pages for both buffers
 * @param[out] rx              Address of the RX buffer
 * @param[out] tx              Address of the TX buffer
 *
 * @return EFI_SUCCESS         Buffers allocated and mapped with Hafnium
 *
**/
STATIC
EFI_STATUS
FfaAllocateAndMapRxTxBuffers (
  IN  UINTN             pages,
  OUT PHYSICAL_ADDRESS  *rx,
  OUT PHYSICAL_ADDRESS  *tx
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;
  EFI_STATUS    Status = EFI_SUCCESS;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  Status = gBS->AllocatePages (AllocateAnyPages, EfiBootServicesData, pages, rx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: RX buffer allocation failed\n", __FUNCTION__));
    goto out;
  }

  Status = gBS->AllocatePages (AllocateAnyPages, EfiBootServicesData, pages, tx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: TX buffer allocation failed\n", __FUNCTION__));
    goto out_rx;
  }

  ArmSmcArgs.Arg0 = ARM_SVC_ID_FFA_RXTX_MAP;
  ArmSmcArgs.Arg1 = (UINTN)*tx;
  ArmSmcArgs.Arg2 = (UINTN)*rx;
  ArmSmcArgs.Arg3 = pages;

  CallFfaSmc (&ArmSmcArgs);

  if (ArmSmcArgs.Arg2 != ARM_FFA_RET_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SVC_ID_FFA_RXTX_MAP failed: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg2
      ));
    Status = RETURN_OUT_OF_RESOURCES;
    goto out_tx;
  }

  goto out;

out_tx:
  gBS->FreePages (*tx, pages);
out_rx:
  gBS->FreePages (*rx, pages);
out:
  return Status;
}

/**
 * Free the RX buffer after reading data from it.
 *
 * @return EFI_SUCCESS         RX buffer released.
 *
**/
STATIC
EFI_STATUS
FfaReleaseRxBuffer (
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;
  EFI_STATUS    Status = EFI_SUCCESS;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  ArmSmcArgs.Arg0 = ARM_SVC_ID_FFA_RX_RELEASE;
  ArmSmcArgs.Arg1 = 0; /* NS World */

  CallFfaSmc (&ArmSmcArgs);

  if (ArmSmcArgs.Arg2 != ARM_FFA_RET_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SVC_ID_FFA_RX_RELEASE failed: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg2
      ));
    Status = RETURN_OUT_OF_RESOURCES;
  }

  return Status;
}

/**
 * Unmap RX/TX buffers and free them.
 *
 * @param[in]  pages           Number of pages for both buffers
 * @param[in]  rx              Address of the RX buffer
 * @param[in]  tx              Address of the TX buffer
 *
 * @return EFI_SUCCESS         Buffers unmapped and freed.
 *
**/
STATIC
EFI_STATUS
FfaFreeRxTxBuffers (
  IN  UINTN             pages,
  IN  PHYSICAL_ADDRESS  rx,
  IN  PHYSICAL_ADDRESS  tx
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;
  EFI_STATUS    Status = EFI_SUCCESS;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  ArmSmcArgs.Arg0 = ARM_SVC_ID_FFA_RXTX_UNMAP;
  ArmSmcArgs.Arg1 = 0; /* NS World */

  CallFfaSmc (&ArmSmcArgs);

  if (ArmSmcArgs.Arg2 != ARM_FFA_RET_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SVC_ID_FFA_RXTX_UNMAP failed: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg2
      ));
    Status = RETURN_OUT_OF_RESOURCES;
  }

  gBS->FreePages (tx, pages);
  gBS->FreePages (rx, pages);

  return Status;
}

/**
 * Get the RAS_FW VM Id by using its UUID and querying Hafnium via FFA.
 *
 * @return VmId            Return the VmId or 0 if not found
 *
**/
STATIC
UINT16
FfaGetRasFwPartitionId (
  )
{
  PHYSICAL_ADDRESS  rx, tx;
  UINT16            VmId;
  UINTN             pages  = 1;
  EFI_STATUS        Status = EFI_SUCCESS;

  Status = FfaAllocateAndMapRxTxBuffers (pages, &rx, &tx);
  if (EFI_ERROR (Status)) {
    return 0;
  }

  ARM_SMC_ARGS  ArmSmcArgs;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  ArmSmcArgs.Arg0 = ARM_SVC_ID_FFA_PARTITION_INFO_GET;
  ArmSmcArgs.Arg1 = RAS_FW_UUID_0;
  ArmSmcArgs.Arg2 = RAS_FW_UUID_1;
  ArmSmcArgs.Arg3 = RAS_FW_UUID_2;
  ArmSmcArgs.Arg4 = RAS_FW_UUID_3;

  CallFfaSmc (&ArmSmcArgs);

  /* One SP should have been found */
  if (ArmSmcArgs.Arg2 != 1) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ARM_SVC_ID_FFA_PARTITION_INFO_GET failed: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg2
      ));
    return 0;
  }

  VmId = *((UINT16 *)rx);
  DEBUG ((DEBUG_INFO, "%a: RAS_FW VmId=0x%x\n", __FUNCTION__, VmId));

  FfaReleaseRxBuffer ();
  Status = FfaFreeRxTxBuffers (pages, rx, tx);

  return VmId;
}

EFI_STATUS
FfaGetRasFwBuffer (
  OUT RAS_FW_BUFFER  *RasFwBufferInfo
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;

  RasFwVmId = FfaGetRasFwPartitionId ();
  if (RasFwVmId == 0) {
    return EFI_UNSUPPORTED;
  }

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  ArmSmcArgs.Arg0 = ARM_FID_FFA_MSG_SEND_DIRECT_REQ;
  ArmSmcArgs.Arg1 = RasFwVmId;
  ArmSmcArgs.Arg3 = RAS_FW_NS_BUFFER_REQ;

  CallFfaSmc (&ArmSmcArgs);

  if (ArmSmcArgs.Arg0 != ARM_FID_FFA_MSG_SEND_DIRECT_RESP) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Invalid FFA response: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg0
      ));
    return EFI_INVALID_PARAMETER;
  }

  RasFwBufferInfo->Base = ArmSmcArgs.Arg4;
  RasFwBufferInfo->Size = ArmSmcArgs.Arg5;

  ASSERT ((RAS_FW_COMM_SIZE+RAS_FW_EINJ_SIZE+RAS_FW_PCIE_SIZE) < RasFwBufferInfo->Size);

  RasFwBufferInfo->CommBase = RasFwBufferInfo->Base;
  RasFwBufferInfo->CommSize = RAS_FW_COMM_SIZE;
  RasFwBufferInfo->EinjBase = RasFwBufferInfo->CommBase +
                              RasFwBufferInfo->CommSize;
  RasFwBufferInfo->EinjSize = RAS_FW_EINJ_SIZE;
  RasFwBufferInfo->PcieBase = RasFwBufferInfo->EinjBase +
                              RasFwBufferInfo->EinjSize;
  RasFwBufferInfo->PcieSize = RAS_FW_PCIE_SIZE;
  RasFwBufferInfo->CperBase = RasFwBufferInfo->PcieBase +
                              RasFwBufferInfo->PcieSize;
  RasFwBufferInfo->CperSize = RasFwBufferInfo->Size -
                              (RasFwBufferInfo->CommSize +
                               RasFwBufferInfo->EinjSize +
                               RasFwBufferInfo->PcieSize);

  DEBUG ((
    DEBUG_INFO,
    "%a: CommBase: 0x%llx\tCommSize: 0x%x\r\n",
    __FUNCTION__,
    RasFwBufferInfo->CommBase,
    RasFwBufferInfo->CommSize
    ));
  DEBUG ((
    DEBUG_INFO,
    "%a: EinjBase: 0x%llx\tEinjSize: 0x%x\r\n",
    __FUNCTION__,
    RasFwBufferInfo->EinjBase,
    RasFwBufferInfo->EinjSize
    ));
  DEBUG ((
    DEBUG_INFO,
    "%a: PcieBase: 0x%llx\tPcieSize: 0x%x\r\n",
    __FUNCTION__,
    RasFwBufferInfo->PcieBase,
    RasFwBufferInfo->PcieSize
    ));
  DEBUG ((
    DEBUG_INFO,
    "%a: CperBase: 0x%llx\tCperSize: 0x%x\r\n",
    __FUNCTION__,
    RasFwBufferInfo->CperBase,
    RasFwBufferInfo->CperSize
    ));
  return EFI_SUCCESS;
}

EFI_STATUS
FfaGuidedCommunication (
  IN EFI_MM_COMMUNICATE_HEADER  *CommunicateHeader,
  IN RAS_FW_BUFFER              *RasFwBufferInfo
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;
  UINTN         BufferSize;

  if (RasFwBufferInfo->CommBase == 0) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));

  ArmSmcArgs.Arg0 = ARM_FID_FFA_MSG_SEND_DIRECT_REQ;
  ArmSmcArgs.Arg1 = RasFwVmId;
  ArmSmcArgs.Arg3 = RAS_FW_GUID_COMMUNICATION;

  BufferSize = sizeof (CommunicateHeader->HeaderGuid) +
               sizeof (CommunicateHeader->MessageLength);

  if (BufferSize > RasFwBufferInfo->CommSize) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: buffer size too small: %u\n",
      __FUNCTION__,
      BufferSize
      ));
    return EFI_OUT_OF_RESOURCES;
  }

  MmioWriteBuffer64 (RasFwBufferInfo->CommBase, BufferSize, (UINT64 *)CommunicateHeader);

  CallFfaSmc (&ArmSmcArgs);

  if (ArmSmcArgs.Arg0 != ARM_FID_FFA_MSG_SEND_DIRECT_RESP) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Invalid FFA response: 0x%x\n",
      __FUNCTION__,
      ArmSmcArgs.Arg0
      ));
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}
