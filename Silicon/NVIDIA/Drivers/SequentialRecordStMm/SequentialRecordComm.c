/** @file

  MM driver to write Sequential records to Flash. This File handles the
  communications bit.

  Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SequentialRecordPrivate.h"
#include <Protocol/FirmwareVolumeBlock.h>

NVIDIA_SEQ_RECORD_PROTOCOL   *RasSeqProto;
NVIDIA_CMET_RECORD_PROTOCOL  *CmetSeqProto;
NVIDIA_SEQ_RECORD_PROTOCOL   *EarlyVarsProto;

#define EARLY_VARS_RD_SOCKET  (0)
#define UEFI_VARS_SOCKET      (0)

/**
 * GetSeqProto
 *  Get the sequential protocol if any that is installed for the given
 *  partition index.
 *
 * @params[in]   PartitionIndex  Partition Index to look up the sequential
 *                               protocol.
 *
 * @retval       SequentialRecord protocol if found for the given Partition
 *               Index.
 *               NULL if not found.
 **/
STATIC
NVIDIA_SEQ_RECORD_PROTOCOL *
GetSeqProto (
  UINT32  PartitionIndex
  )
{
  EFI_STATUS                  Status;
  UINT32                      Index;
  EFI_HANDLE                  *HandleBuffer;
  UINTN                       NumHandles;
  NVIDIA_SEQ_RECORD_PROTOCOL  *SeqProto;
  NVIDIA_SEQ_RECORD_PROTOCOL  *ReturnProto;

  ReturnProto = NULL;
  Status      = GetProtocolHandleBuffer (
                  &gNVIDIASequentialStorageGuid,
                  &NumHandles,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get NOR Flash protocol (%r)\r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitGetSeqProto;
  }

  for (Index = 0; Index < NumHandles; Index++) {
    Status = gMmst->MmHandleProtocol (
                      HandleBuffer[Index],
                      &gNVIDIASequentialStorageGuid,
                      (VOID **)&SeqProto
                      );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to find SocketId installed on %p %r\n",
        __FUNCTION__,
        HandleBuffer[Index],
        Status
        ));
      continue;
    }

    if (SeqProto->PartitionInfo.PartitionIndex == PartitionIndex) {
      DEBUG ((DEBUG_INFO, "%a: Found SeqProto for %u %p\n", __FUNCTION__, PartitionIndex, SeqProto));
      ReturnProto = SeqProto;
      break;
    }
  }

ExitGetSeqProto:
  return ReturnProto;
}

/**
 * CorruptFvHeader.
 * Utility function to corrupt the UEFI Variable store by corrupting
 * the FV header forcing a re-build of the variable store during the next
 * boot.
 *
 * @params[in]   None.
 *
 * @retval       EFI_SUCCESS      Succesfully corrupted the FV Header.
 *               Other            Failure to get the partition Info or while
 *                                transacting with the device.
 **/
STATIC
EFI_STATUS
CorruptFvHeader (
  VOID
  )
{
  UINT32                      FvHeaderLength;
  UINT64                      FvHeaderOffset;
  NVIDIA_NOR_FLASH_PROTOCOL   *NorFlashProtocol;
  EFI_PHYSICAL_ADDRESS        CpuBlParamsAddr;
  EFI_STATUS                  Status;
  UINT64                      PartitionSize;
  UINT16                      DeviceInstance;
  EFI_FIRMWARE_VOLUME_HEADER  FvHeaderData;

  NorFlashProtocol = GetSocketNorFlashProtocol (UEFI_VARS_SOCKET);
  if (NorFlashProtocol == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get NorFlashProtocol for Socket 0\n",
      __FUNCTION__
      ));
    Status = EFI_UNSUPPORTED;
    goto ExitCorruptFvHeader;
  }

  Status = GetCpuBlParamsAddrStMm (&CpuBlParamsAddr);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get CpuBl Addr %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitCorruptFvHeader;
  }

  Status = GetPartitionInfoStMm (
             (UINTN)CpuBlParamsAddr,
             TEGRABL_VARIABLE_IMAGE_INDEX,
             &DeviceInstance,
             &FvHeaderOffset,
             &PartitionSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to get Variable PartitionInfo %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitCorruptFvHeader;
  }

  FvHeaderLength = sizeof (EFI_FIRMWARE_VOLUME_HEADER);
  Status         = NorFlashProtocol->Read (
                                       NorFlashProtocol,
                                       FvHeaderOffset,
                                       FvHeaderLength,
                                       &FvHeaderData
                                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Read FV header %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitCorruptFvHeader;
  }

  /* Corrupt the signature/revision .*/
  FvHeaderData.Signature = FvHeaderData.Revision = 0;
  Status                 = NorFlashProtocol->Write (
                                               NorFlashProtocol,
                                               FvHeaderOffset,
                                               FvHeaderData.HeaderLength,
                                               &FvHeaderData
                                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to Write Partition header\r\n", __FUNCTION__));
    goto ExitCorruptFvHeader;
  }

ExitCorruptFvHeader:
  return Status;
}

/**
 * MMI handler for CMET Log service.
 *
 * @params[in]   DispatchHandle   Handle of the registered MMI..
 * @params[in]   RegisterContext  Context Info from MMI root handler.
 * @params[out]  CommBuffer       Comm Buffer sent by the client.
 * @params[out]  CommBufferSize   Comm Buffer Size sent from the client.
 *
 * @retval       EFI_SUCCESS     Always return Success to the MMI root handler
 *                               The error from the service will be in-band of
 *                               the service call.
 **/
STATIC
EFI_STATUS
CmetMsgHandler (
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  )
{
  EFI_STATUS                  Status;
  RAS_MM_COMMUNICATE_PAYLOAD  *CmetHeader;
  UINT8                       *CmetPayload;

  CmetHeader  = (RAS_MM_COMMUNICATE_PAYLOAD *)CommBuffer;
  CmetPayload = CmetHeader->Data;

  if (CmetSeqProto == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: No Storage support for Cmet Vars\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ExitCmetMsgHandler;
  }

  if (IsBufInSecSpMbox ((UINTN)CommBuffer, RASFW_VMID) == FALSE) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ComBuffer %lu is not in the RAS FW Mbox\n",
      __FUNCTION__,
      (UINT64)(CommBuffer)
      ));
    Status = EFI_INVALID_PARAMETER;
    goto ExitCmetMsgHandler;
  }

  switch (CmetHeader->Function) {
    case READ_LAST_RECORD:
      Status = CmetSeqProto->ReadRecord (
                               CmetSeqProto,
                               CmetHeader->Socket,
                               (VOID *)CmetPayload,
                               *CommBufferSize,
                               CmetHeader->Flag
                               );
      break;
    case WRITE_NEXT_RECORD:
      Status = CmetSeqProto->WriteRecord (
                               CmetSeqProto,
                               CmetHeader->Socket,
                               (VOID *)CmetPayload,
                               *CommBufferSize,
                               CmetHeader->Flag
                               );
      break;
    default:
      DEBUG ((
        DEBUG_ERROR,
        "%a: Unknown Function %u\n",
        __FUNCTION__,
        CmetHeader->Function
        ));
      Status = EFI_INVALID_PARAMETER;
      break;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a:%d Fn %u Size %u Return %r\n",
    __FUNCTION__,
    __LINE__,
    CmetHeader->Function,
    *CommBufferSize,
    Status
    ));
ExitCmetMsgHandler:
  CmetHeader->ReturnStatus = Status;
  return EFI_SUCCESS;
}

/**
 * MMI handler for RAS Log service.
 *
 * @params[in]   DispatchHandle   Handle of the registered MMI..
 * @params[in]   RegisterContext  Context Info from MMI root handler.
 * @params[out]  CommBuffer       Comm Buffer sent by the client.
 * @params[out]  CommBufferSize   Comm Buffer Size sent from the client.
 *
 * @retval       EFI_SUCCESS     Always return Success to the MMI root handler
 *                               The error from the service will be in-band of
 *                               the service call.
 **/
STATIC
EFI_STATUS
RasLogMsgHandler (
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  )
{
  EFI_STATUS                  Status;
  RAS_MM_COMMUNICATE_PAYLOAD  *RasHeader;
  UINT8                       *RasPayload;

  RasHeader  = (RAS_MM_COMMUNICATE_PAYLOAD *)CommBuffer;
  RasPayload = RasHeader->Data;

  if (RasSeqProto == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: No Storage support for RASLog\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ExitRasMsgHandler;
  }

  if (IsBufInSecSpMbox ((UINTN)CommBuffer, RASFW_VMID) == FALSE) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ComBuffer %lu is not in the RAS FW Mbox\n",
      __FUNCTION__,
      (UINT64)(CommBuffer)
      ));
    Status = EFI_INVALID_PARAMETER;
    goto ExitRasMsgHandler;
  }

  switch (RasHeader->Function) {
    case READ_LAST_RECORD:
      Status = RasSeqProto->ReadLast (
                              RasSeqProto,
                              RasHeader->Socket,
                              (VOID *)RasPayload,
                              *CommBufferSize
                              );
      break;
    case WRITE_NEXT_RECORD:
      Status = RasSeqProto->WriteNext (
                              RasSeqProto,
                              RasHeader->Socket,
                              (VOID *)RasPayload,
                              *CommBufferSize
                              );
      break;
    default:
      DEBUG ((
        DEBUG_ERROR,
        "%a: Unknown Function %u\n",
        __FUNCTION__,
        RasHeader->Function
        ));
      Status = EFI_INVALID_PARAMETER;
      break;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a:%d Fn %u Size %u  Return %r\n",
    __FUNCTION__,
    __LINE__,
    RasHeader->Function,
    *CommBufferSize,
    Status
    ));
ExitRasMsgHandler:
  RasHeader->ReturnStatus = Status;
  return EFI_SUCCESS;
}

/**
 * Utility Function to delete the Early Vars Partition.
 *
 * @params[in/out]   None.
 *
 * @retval       EFI_SUCCESS     Succesfully erased the EarlyVars Partition.
 *                Other          Failed to erase partition.
 **/
STATIC
EFI_STATUS
EraseEarlyVarsPartition (
  VOID
  )
{
  UINTN                 SocketIdx;
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  CpuBlParams;

  if (EarlyVarsProto == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: No Storage support for Early Vars\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ExitEraseEarlyVarsPartition;
  }

  Status = GetCpuBlParamsAddrStMm (&CpuBlParams);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get CPU BL Addr %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitEraseEarlyVarsPartition;
  }

  for (SocketIdx = 0; SocketIdx < MAX_SOCKETS; SocketIdx++) {
    if (IsSocketEnabledStMm (CpuBlParams, SocketIdx) == TRUE) {
      Status = EarlyVarsProto->ErasePartition (
                                 EarlyVarsProto,
                                 SocketIdx
                                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Erase Failed Socket %u\n",
          __FUNCTION__,
          SocketIdx
          ));
      }
    }
  }

ExitEraseEarlyVarsPartition:
  return Status;
}

/**
 * MMI handler for Early Variable service.
 *
 * @params[in]   DispatchHandle   Handle of the registered MMI..
 * @params[in]   RegisterContext  Context Info from MMI root handler.
 * @params[out]  CommBuffer       Comm Buffer sent by the client.
 * @params[out]  CommBufferSize   Comm Buffer Size sent from the client.
 *
 * @retval       EFI_SUCCESS     Always return Success to the MMI root handler
 *                               The error from the service will be in-band of
 *                               the service call.
 **/
STATIC
EFI_STATUS
EarlyVarsMsgHandler (
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  )
{
  EFI_STATUS                    Status;
  NVIDIA_MM_MB1_RECORD_PAYLOAD  *EarlyVars;
  UINT8                         *Record;
  UINTN                         SocketIdx;
  EFI_PHYSICAL_ADDRESS          CpuBlAddr;
  UINT32                        RecSize;

  EarlyVars = (NVIDIA_MM_MB1_RECORD_PAYLOAD *)CommBuffer;
  RecSize   = TEGRABL_EARLY_BOOT_VARS_MAX_SIZE - sizeof (TEGRABL_EARLY_BOOT_VARS_DATA_HEADER);
  if (EarlyVarsProto == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: No Storage support for Early Vars\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ExitEarlyVarsMsgHandler;
  }

  Status = GetCpuBlParamsAddrStMm (&CpuBlAddr);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get CPU BL Addr %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitEarlyVarsMsgHandler;
  }

  Record = EarlyVars->Data;
  DEBUG ((
    DEBUG_ERROR,
    "%a: Fn %u Size %u\n ",
    __FUNCTION__,
    EarlyVars->Command,
    *CommBufferSize
    ));
  switch (EarlyVars->Command) {
    case READ_LAST_RECORD:
      Status = EarlyVarsProto->ReadLast (
                                 EarlyVarsProto,
                                 EARLY_VARS_RD_SOCKET,
                                 (VOID *)Record,
                                 RecSize
                                 );
      break;
    case WRITE_NEXT_RECORD:
      for (SocketIdx = 0; SocketIdx < MAX_SOCKETS; SocketIdx++) {
        if (IsSocketEnabledStMm (CpuBlAddr, SocketIdx) == TRUE) {
          Status = EarlyVarsProto->WriteNext (
                                     EarlyVarsProto,
                                     SocketIdx,
                                     (VOID *)Record,
                                     RecSize
                                     );
          if (EFI_ERROR (Status)) {
            DEBUG ((
              DEBUG_ERROR,
              "%a: Write Failed Socket %u %r\n",
              __FUNCTION__,
              SocketIdx,
              Status
              ));
            break;
          }
        }
      }

      break;
    case ERASE_PARTITION:
      Status = EraseEarlyVarsPartition ();
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Erase Failed Earlyvars Partition %r\n",
          __FUNCTION__,
          Status
          ));
      }

      break;
    default:
      DEBUG ((
        DEBUG_ERROR,
        "%a: Invalid Function %u\n",
        __FUNCTION__,
        EarlyVars->Command
        ));
      Status = EFI_INVALID_PARAMETER;
      break;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Got Function %u Return %r\n",
    __FUNCTION__,
    EarlyVars->Command,
    Status
    ));
ExitEarlyVarsMsgHandler:
  EarlyVars->Status = Status;
  return EFI_SUCCESS;
}

/**
 * MMI handler for SatMc service.
 *
 * @params[in]   DispatchHandle   Handle of the registered MMI..
 * @params[in]   RegisterContext  Context Info from MMI root handler.
 * @params[out]  CommBuffer       Comm Buffer sent by the client.
 * @params[out]  CommBufferSize   Comm Buffer Size sent from the client.
 *
 * @retval       EFI_SUCCESS     Always return Success to the MMI root handler
 *                               The error from the service will be in-band of
 *                               the service call.
 **/
STATIC
EFI_STATUS
SatMcMsgHandler (
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  )
{
  EFI_STATUS                    Status;
  SATMC_MM_COMMUNICATE_PAYLOAD  *SatMcMmMsg;

  SatMcMmMsg = (SATMC_MM_COMMUNICATE_PAYLOAD *)CommBuffer;

  if (IsBufInSecSpMbox ((UINTN)CommBuffer, SATMC_VMID) == FALSE) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ComBuffer %lu is not in the SATMC Mbox\n",
      __FUNCTION__,
      (UINT64)(CommBuffer)
      ));
    Status = EFI_INVALID_PARAMETER;
    goto ExitSatMcMsgHandler;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: Fn %u Size %u\n ",
    __FUNCTION__,
    SatMcMmMsg->Command,
    *CommBufferSize
    ));

  switch (SatMcMmMsg->Command) {
    case CLEAR_EFI_VARIABLES:
      Status = CorruptFvHeader ();
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to Corrupt FV Header %r",
          __FUNCTION__,
          Status
          ));
        goto ExitSatMcMsgHandler;
      }

      Status = EraseEarlyVarsPartition ();
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to Erase Early Vars Partition %r",
          __FUNCTION__,
          Status
          ));
        goto ExitSatMcMsgHandler;
      }

      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a: Unknown command %u\n", __FUNCTION__, SatMcMmMsg->Command));
      Status = EFI_INVALID_PARAMETER;
      break;
  }

  DEBUG ((DEBUG_INFO, "%a: Returning %a \n", __FUNCTION__, Status));

ExitSatMcMsgHandler:
  SatMcMmMsg->ReturnStatus = Status;
  return EFI_SUCCESS;
}

/**
 * Register handler for RAS Log record writing..
 *
 * @params[]     None.
 *
 * @retval       EFI_SUCCESS     Successfully registered the Early Vars MMI
 *                               handler. Note that even if we failed to look
 *                               up the partition we return success.
 *               OTHER           When trying to register an MMI handler, return
 *                               Status code and stop the rest of the driver
 *                               from progressing.
 **/
STATIC
EFI_STATUS
RegisterRasLogHandler (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;

  Handle = NULL;
  Status = EFI_SUCCESS;
  Status = gMmst->MmiHandlerRegister (
                    RasLogMsgHandler,
                    &gNVIDIARasLogMmGuid,
                    &Handle
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Register MMI handler failed (%r)\n",
      __FUNCTION__,
      Status
      ));
    goto ExitRasLogHandler;
  }

  RasSeqProto = GetSeqProto (TEGRABL_RAS_ERROR_LOGS);
  if (RasSeqProto == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Get Sequential Proto for RAS\n",
      __FUNCTION__
      ));

    /* Log the failure to get partition info, but return
     * success, so we can receive the MMI but not honor the storage portion.
     */
    Status = EFI_SUCCESS;
  }

ExitRasLogHandler:
  return Status;
}

/**
 * Register handler for Early Variables service.
 *
 * @params[]     None.
 *
 * @retval       EFI_SUCCESS     Successfully registered the Early Vars MMI
 *                               handler. Note that even if we failed to look
 *                               up the partition we return success.
 *               OTHER           When trying to register an MMI handler, return
 *                               Status code and stop the rest of the driver
 *                               from progressing.
 **/
STATIC
EFI_STATUS
RegisterEarlyVarsHandler (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;

  Handle = NULL;
  Status = EFI_SUCCESS;
  Status = gMmst->MmiHandlerRegister (
                    EarlyVarsMsgHandler,
                    &gNVIDIAMmMb1RecordGuid,
                    &Handle
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Register MMI handler failed (%r)\n",
      __FUNCTION__,
      Status
      ));
    goto ExitEarlyVarsHandler;
  }

  EarlyVarsProto = GetSeqProto (TEGRABL_EARLY_BOOT_VARS);
  if (EarlyVarsProto == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Get Sequential Proto for EarlyVars\n",
      __FUNCTION__
      ));
    EarlyVarsProto = NULL;

    /* Log the failure to get partition info, but return
     * success, so we can receive the MMI but not honor the storage portion.
     */
    Status = EFI_SUCCESS;
  }

ExitEarlyVarsHandler:
  return Status;
}

/**
 * Register handler for RAS CMET service.
 *
 * @params[]     None.
 *
 * @retval       EFI_SUCCESS     Successfully registered the Early Vars MMI
 *                               handler. Note that even if we failed to look
 *                               up the partition we return success.
 *               OTHER           When trying to register an MMI handler, return
 *                               Status code and stop the rest of the driver
 *                               from progressing.
 **/
STATIC
EFI_STATUS
RegisterCmetHandler (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;

  Handle = NULL;
  Status = gMmst->MmiHandlerRegister (
                    CmetMsgHandler,
                    &gNVIDIARasCmetMmGuid,
                    &Handle
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Register MMI handler failed (%r)\n",
      __FUNCTION__,
      Status
      ));
    goto ExitCmetRegister;
  }

  Status = gMmst->MmLocateProtocol (
                    &gNVIDIACmetStorageGuid,
                    NULL,
                    (VOID **)&CmetSeqProto
                    );
  if (CmetSeqProto == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Get Sequential Proto for Cmet\n",
      __FUNCTION__
      ));
    CmetSeqProto = NULL;

    /* Log the failure to get partition info, but return
     * success, so we can receive the MMI but not honor the storage portion.
     */
    Status = EFI_SUCCESS;
  }

ExitCmetRegister:
  return Status;
}

/**
 * Register handler for SatMc service.
 *
 * @params[]     None.
 *
 * @retval       EFI_SUCCESS     Successfully registered the Early Vars MMI
 *                               handler. Note that even if we failed to look
 *                               up the partition we return success.
 *               OTHER           When trying to register an MMI handler, return
 *                               Status code and stop the rest of the driver
 *                               from progressing.
 **/
STATIC
EFI_STATUS
RegisterSatMcHandler (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;

  Handle = NULL;
  Status = gMmst->MmiHandlerRegister (
                    SatMcMsgHandler,
                    &gNVIDIASatMcMmGuid,
                    &Handle
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Register MMI handler failed (%r)\n",
      __FUNCTION__,
      Status
      ));
    goto ExitSatMcRegister;
  }

ExitSatMcRegister:
  return Status;
}

/**
  Initialize the Sequential Record Communications Driver

  @param[in]  ImageHandle   of the loaded driver
  @param[in]  SystemTable   Pointer to the System Table

**/
EFI_STATUS
EFIAPI
SequentialRecordCommInitialize (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *MmSystemTable
  )
{
  EFI_STATUS  Status;

  Status = RegisterEarlyVarsHandler ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to Register Early Variable handler%r\n",
      __FUNCTION__,
      Status
      ));
  }

  Status = RegisterRasLogHandler ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to Register RAS log handler%r\n",
      __FUNCTION__,
      Status
      ));
  }

  Status = RegisterCmetHandler ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to Register CMET log handler%r\n",
      __FUNCTION__,
      Status
      ));
  }

  Status = RegisterSatMcHandler ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to Register CMET log handler%r\n",
      __FUNCTION__,
      Status
      ));
  }

  return EFI_SUCCESS;
}
