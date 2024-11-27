/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2021-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/
#include "RASFwHspInterface.h"
#include <T410/T410Definitions.h>

RAS_FW_INTERFACE  gRasFwInterface;

EFI_STATUS
RasHspApiCall (
  IN UINT32             Socket,
  RAS_HSP_API_CMD_RESP  *CmdResp,
  UINT32                TimeoutMs
  )
{
  UINT64  HspBase = T410_HSP_SHRD_MBOX_RAS_UEFI_SOCKET_0 + ((UINT64)Socket << T410_SOCKET_SHFT);

  if (MmioRead32 (HspBase + RAS_HSP_UEFI_MAILBOX_TAG_OFFSET) != 0) {
    DEBUG ((DEBUG_ERROR, "HSP not ready\n"));
    return EFI_NOT_READY;
  }

  MmioWrite32 (HspBase + RAS_HSP_UEFI_MAILBOX_MB0_OFFSET, CmdResp->Request);
  MmioWrite32 (HspBase + RAS_HSP_UEFI_MAILBOX_MB1_OFFSET, CmdResp->Arg0);
  MmioWrite32 (HspBase + RAS_HSP_UEFI_MAILBOX_MB2_OFFSET, CmdResp->Arg1);
  MmioWrite32 (HspBase + RAS_HSP_UEFI_MAILBOX_MB3_OFFSET, CmdResp->Arg2);
  MmioWrite32 (HspBase + RAS_HSP_UEFI_MAILBOX_TAG_OFFSET, BIT31);

  while ((TimeoutMs > 0) && (MmioRead32 (HspBase + RAS_HSP_UEFI_MAILBOX_TAG_OFFSET) != 0)) {
    MicroSecondDelay (1000U);
    TimeoutMs--;
  }

  if (MmioRead32 (HspBase + RAS_HSP_UEFI_MAILBOX_TAG_OFFSET)) {
    DEBUG ((DEBUG_ERROR, "Timeout waiting for HSP tag\n"));
    return EFI_TIMEOUT;
  }

  CmdResp->Result = MmioRead32 (HspBase + RAS_HSP_UEFI_MAILBOX_MB0_OFFSET);
  CmdResp->Arg0   = MmioRead32 (HspBase + RAS_HSP_UEFI_MAILBOX_MB1_OFFSET);
  CmdResp->Arg1   = MmioRead32 (HspBase + RAS_HSP_UEFI_MAILBOX_MB2_OFFSET);
  CmdResp->Arg2   = MmioRead32 (HspBase + RAS_HSP_UEFI_MAILBOX_MB3_OFFSET);

  return EFI_SUCCESS;
}

/**
  Function map region into GCD and MMU

  @param[in]  BaseAddress           Base address of region
  @param[in]  Size                  Size of region

  @return EFI_SUCCESS               GCD/MMU Updated.

**/
EFI_STATUS
AddMemoryRegion (
  IN  UINT64  BaseAddress,
  IN  UINT64  Size
  )
{
  EFI_STATUS  Status;
  UINT64      AlignedBaseAddress = BaseAddress & ~(SIZE_4KB-1);
  UINT64      AlignedSize        = Size + (BaseAddress - AlignedBaseAddress);
  UINT64      AlignedEnd;
  UINT64      ScanLocation;

  AlignedSize = ALIGN_VALUE (Size, SIZE_4KB);
  AlignedEnd  = AlignedBaseAddress + AlignedSize;

  ScanLocation = AlignedBaseAddress;
  while (ScanLocation < AlignedEnd) {
    EFI_GCD_MEMORY_SPACE_DESCRIPTOR  MemorySpace;
    UINT64                           OverlapSize;

    Status = gDS->GetMemorySpaceDescriptor (ScanLocation, &MemorySpace);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to GetMemorySpaceDescriptor (0x%llx): %r.\r\n", __FUNCTION__, ScanLocation, Status));
      return Status;
    }

    OverlapSize = MIN (MemorySpace.BaseAddress + MemorySpace.Length, AlignedEnd) - ScanLocation;
    if (MemorySpace.GcdMemoryType == EfiGcdMemoryTypeNonExistent) {
      Status = gDS->AddMemorySpace (
                      EfiGcdMemoryTypeMemoryMappedIo,
                      ScanLocation,
                      OverlapSize,
                      EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to AddMemorySpace: (0x%llx, 0x%llx) %r.\r\n", __FUNCTION__, ScanLocation, OverlapSize, Status));
        return Status;
      }

      Status = gDS->SetMemorySpaceAttributes (
                      ScanLocation,
                      OverlapSize,
                      EFI_MEMORY_UC
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to SetMemorySpaceAttributes: (0x%llx, 0x%llx) %r.\r\n", __FUNCTION__, ScanLocation, OverlapSize, Status));
        return Status;
      }
    }

    ScanLocation += OverlapSize;
  }

  return EFI_SUCCESS;
}

STATIC EFI_STATUS
HspGetRasFwBuffer (
  IN UINT32          Socket,
  OUT RAS_FW_BUFFER  *RasFwBufferInfo
  )
{
  RAS_HSP_API_CMD_RESP  RasClusterArgs;
  UINT64                BufferBase, BufferSize;

  RasClusterArgs.Request = UEFI_CMD_GET_APEI_BUFFER;
  EFI_STATUS  Status = RasHspApiCall (Socket, &RasClusterArgs, 10);

  if (Status != EFI_SUCCESS) {
    return Status;
  }

  DEBUG ((
    DEBUG_INFO,
    "HSP Call results=0x%x 0x%x 0x%x 0x%x\n",
    RasClusterArgs.Result,
    RasClusterArgs.Res0,
    RasClusterArgs.Res1,
    RasClusterArgs.Res2
    ));

  BufferBase = (((UINT64)RasClusterArgs.Res1) << 32U) + RasClusterArgs.Res2;
  BufferSize = RasClusterArgs.Res0;

  AddMemoryRegion (BufferBase, BufferSize);

  CopyMem (RasFwBufferInfo, (UINT64 *)BufferBase, sizeof (RAS_FW_BUFFER));
  ASSERT (BufferBase == RasFwBufferInfo->Base);
  ASSERT (BufferSize == RasFwBufferInfo->Size);
  ASSERT ((RasFwBufferInfo->CommBase + RasFwBufferInfo->CommSize) <= RasFwBufferInfo->EinjBase);
  ASSERT ((RasFwBufferInfo->EinjBase + RasFwBufferInfo->EinjSize) <= RasFwBufferInfo->PcieBase);
  ASSERT ((RasFwBufferInfo->PcieBase + RasFwBufferInfo->PcieSize) <= RasFwBufferInfo->CperBase);
  ASSERT ((RasFwBufferInfo->CperBase + RasFwBufferInfo->CperSize) <= (BufferBase + BufferSize));

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
HspGuidedCommunication (
  IN UINT32                     Socket,
  IN EFI_MM_COMMUNICATE_HEADER  *CommunicateHeader,
  IN RAS_FW_BUFFER              *RasFwBufferInfo
  )
{
  RAS_HSP_API_CMD_RESP  RasClusterArgs;
  EFI_STATUS            Status;
  UINTN                 BufferSize;

  if (RasFwBufferInfo->CommBase == 0) {
    return EFI_INVALID_PARAMETER;
  }

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

  RasClusterArgs.Request = UEFI_CMD_MM_COMMUNICATE;
  Status                 = RasHspApiCall (Socket, &RasClusterArgs, 10);
  if (Status != EFI_SUCCESS) {
    return Status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
RASFwInterfaceInit (
  UINT32            SocketCount,
  RAS_FW_INTERFACE  **RasFwInterfaceOut
  )
{
  EFI_STATUS  Status;

  ASSERT (RasFwInterfaceOut != NULL);
  gRasFwInterface.RasFwBufferInfo = AllocateZeroPool (sizeof (RAS_FW_BUFFER) * SocketCount);
  ASSERT (gRasFwInterface.RasFwBufferInfo != NULL);

  for (UINT32 Socket = 0; Socket < SocketCount; Socket++) {
    Status = HspGetRasFwBuffer (Socket, &(gRasFwInterface.RasFwBufferInfo[Socket]));
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get RAS_FW NS shared mem (S%u): %r\n",
        __FUNCTION__,
        Socket,
        Status
        ));
      return EFI_PROTOCOL_ERROR;
    }
  }

  gRasFwInterface.GuidedComm          = HspGuidedCommunication;
  gRasFwInterface.BertEventId         = HSP_BERT_EVENT_ID;
  gRasFwInterface.EinjDoorbellAddress = T410_HSP_SHRD_MBOX_RAS_EINJ_SOCKET_0 + RAS_HSP_UEFI_MAILBOX_TAG_OFFSET;
  gRasFwInterface.EinjDoorbellValue   = BIT31;
  gRasFwInterface.EinjDoorbellMask    = BIT31;
  *RasFwInterfaceOut                  = &gRasFwInterface;

  return EFI_SUCCESS;
}
