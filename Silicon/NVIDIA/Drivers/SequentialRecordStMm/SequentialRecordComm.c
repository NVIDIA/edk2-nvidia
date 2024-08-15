/** @file

  MM driver to write Sequential records to Flash. This File handles the
  communications bit.

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SequentialRecordPrivate.h"
#include <Protocol/FirmwareVolumeBlock.h>
#include <Protocol/SmmVariable.h>
#include <Library/BaseLib.h>
#include <NVIDIAConfiguration.h>
#include <IndustryStandard/Acpi64.h>

STATIC NVIDIA_SEQ_RECORD_PROTOCOL   *RasSeqProto;
STATIC NVIDIA_CMET_RECORD_PROTOCOL  *CmetSeqProto;
STATIC NVIDIA_SEQ_RECORD_PROTOCOL   *EarlyVarsProto;
STATIC EFI_SMM_VARIABLE_PROTOCOL    *SmmVar;

#define EARLY_VARS_RD_SOCKET  (0)
#define MAX_VAR_NAME          (256 * sizeof (CHAR16))

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
  UINTN                       CmetPayloadSize;

  if ((CommBuffer == NULL) || (CommBufferSize == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Communication buffer : %r\n", __FUNCTION__, EFI_INVALID_PARAMETER));
    return EFI_SUCCESS;
  }

  if (*CommBufferSize < sizeof (RAS_MM_COMMUNICATE_PAYLOAD)) {
    DEBUG ((DEBUG_ERROR, "%a: Communication buffer : %r\n", __FUNCTION__, EFI_BUFFER_TOO_SMALL));
    return EFI_SUCCESS;
  }

  CmetHeader  = (RAS_MM_COMMUNICATE_PAYLOAD *)CommBuffer;
  CmetPayload = CmetHeader->Data;

  CmetPayloadSize = *CommBufferSize - sizeof (RAS_MM_COMMUNICATE_PAYLOAD);

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
                               CmetPayloadSize,
                               CmetHeader->Flag
                               );
      break;
    case WRITE_NEXT_RECORD:
      Status = CmetSeqProto->WriteRecord (
                               CmetSeqProto,
                               CmetHeader->Socket,
                               (VOID *)CmetPayload,
                               CmetPayloadSize,
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
 * @brief Given a RAS log from RAS Firmware, this function can be used to change where any given CPER is sent and
 * therefore override the defaults from RAS Firmware.
 *
 * @param RasPayload  RAS Log from RAS Firmware that can be cast to RAS_LOG_MM_ENTRY
 * @param Target      Bit field with destination of the CPER. Only the PUBLISH_HEST and PUBLISH_BMC bits can be added or
 *                    removed.
 * @return UINTN      Updated target where the PUBLISH_HEST and/or PUBLISH_BMC bits may have been changed.
 */
STATIC
UINTN
RasLogOverrideTargets (
  IN UINT8  *RasPayload,
  IN UINTN  Target
  )
{
  RAS_LOG_MM_ENTRY                                 *LogEntry;
  EFI_ACPI_6_4_GENERIC_ERROR_DATA_ENTRY_STRUCTURE  *Gedes;

  if (FeaturePcdGet (PcdNoCorrectedErrorsInHest)) {
    LogEntry = (RAS_LOG_MM_ENTRY *)RasPayload;
    Gedes    = (EFI_ACPI_6_4_GENERIC_ERROR_DATA_ENTRY_STRUCTURE *)LogEntry->Log;
    DEBUG ((DEBUG_INFO, "%a: Target=0x%llx Severity=0x%lx\n", __FUNCTION__, Target, Gedes->ErrorSeverity));

    /* Don't publish corrected/informational errors to HEST/OS */
    if ((Gedes->ErrorSeverity == EFI_ACPI_6_4_ERROR_SEVERITY_CORRECTED) ||
        (Gedes->ErrorSeverity == EFI_ACPI_6_4_ERROR_SEVERITY_NONE))
    {
      Target &= ~(PUBLISH_HEST);
      Target |= PUBLISH_BMC;
      DEBUG ((DEBUG_INFO, "%a: Corrected/Informational error. New Target=0x%llx\n", __FUNCTION__, Target));
    }
  }

  return Target;
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
  UINTN                       RasPayloadSize;

  if ((CommBuffer == NULL) || (CommBufferSize == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Communication buffer : %r\n", __FUNCTION__, EFI_INVALID_PARAMETER));
    return EFI_SUCCESS;
  }

  if (*CommBufferSize < sizeof (RAS_MM_COMMUNICATE_PAYLOAD)) {
    DEBUG ((DEBUG_ERROR, "%a: Communication buffer : %r\n", __FUNCTION__, EFI_BUFFER_TOO_SMALL));
    return EFI_SUCCESS;
  }

  RasHeader  = (RAS_MM_COMMUNICATE_PAYLOAD *)CommBuffer;
  RasPayload = RasHeader->Data;

  RasPayloadSize = *CommBufferSize - sizeof (RasHeader);

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
                              RasPayloadSize
                              );
      break;
    case WRITE_NEXT_RECORD:
      Status = RasSeqProto->WriteNext (
                              RasSeqProto,
                              RasHeader->Socket,
                              (VOID *)RasPayload,
                              RasPayloadSize
                              );
      RasHeader->Flag = RasLogOverrideTargets (RasPayload, RasHeader->Flag);
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

  if ((CommBuffer == NULL) || (CommBufferSize == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Communication buffer : %r\n", __FUNCTION__, EFI_INVALID_PARAMETER));
    return EFI_SUCCESS;
  }

  if (*CommBufferSize < sizeof (NVIDIA_MM_MB1_RECORD_PAYLOAD)) {
    DEBUG ((DEBUG_ERROR, "%a: Communication buffer : %r\n", __FUNCTION__, EFI_BUFFER_TOO_SMALL));
    return EFI_SUCCESS;
  }

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
  Is variable protected

  @retval  True   Variable is protected.
  @retval  False  Variable is not protected.

**/
STATIC
BOOLEAN
EFIAPI
IsVariableProtectedStmm (
  EFI_GUID  VariableGuid,
  CHAR16    *VariableName
  )
{
  EFI_STATUS           Status;
  UINTN                ProductInfoSize;
  NVIDIA_PRODUCT_INFO  ProductInfo;
  CHAR16               ProductInfoVariableName[] = L"ProductInfo";

  ProductInfoSize = sizeof (ProductInfo);

  //
  // Avoid deleting user password variables
  //
  if (CompareGuid (&VariableGuid, &gUserAuthenticationGuid)) {
    return TRUE;
  }

  //
  // Check if we have to protect product asset tag info.
  //
  Status = SmmVar->SmmGetVariable (
                     ProductInfoVariableName,
                     &gNVIDIAPublicVariableGuid,
                     NULL,
                     &ProductInfoSize,
                     &ProductInfo
                     );
  if (Status == EFI_SUCCESS) {
    if ((ProductInfo.AssetTagProtection != 0) &&
        (StrnCmp (VariableName, ProductInfoVariableName, StrLen (ProductInfoVariableName)) == 0) &&
        CompareGuid (&VariableGuid, &gNVIDIAPublicVariableGuid))
    {
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * DeleteNsVars
 * Function to delete all the non-secure variables and locked variables.
 * This function is usually called from SatMc SP.
 *
 *
 * @retval EFI_SUCCESS Deleted all the NS and locked variables.
 *         Other       Fail to delete.
 **/
STATIC
EFI_STATUS
DeleteNsVars (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_STATUS  GetVarStatus;
  EFI_STATUS  ClearVarStatus;
  CHAR16      *CurVarName;
  CHAR16      *NextVarName;
  EFI_GUID    CurVarGuid;
  EFI_GUID    NextVarGuid;
  UINTN       NameSize;

  CurVarName  = NULL;
  NextVarName = NULL;
  Status      = EFI_SUCCESS;

  if (SmmVar == NULL) {
    Status = EFI_UNSUPPORTED;
    goto ExitDeleteNsVars;
  }

  CurVarName = AllocateZeroPool (MAX_VAR_NAME);
  if (CurVarName == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitDeleteNsVars;
  }

  NextVarName = AllocateZeroPool (MAX_VAR_NAME);
  if (NextVarName == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitDeleteNsVars;
  }

  NameSize = MAX_VAR_NAME;

  GetVarStatus = SmmVar->SmmGetNextVariableName (
                           &NameSize,
                           NextVarName,
                           &NextVarGuid
                           );
  while (!EFI_ERROR (GetVarStatus)) {
    CopyMem (CurVarName, NextVarName, NameSize);
    CopyGuid (&CurVarGuid, &NextVarGuid);
    NameSize = MAX_VAR_NAME;

    GetVarStatus = SmmVar->SmmGetNextVariableName (
                             &NameSize,
                             NextVarName,
                             &NextVarGuid
                             );

    if (IsVariableProtectedStmm (CurVarGuid, CurVarName)) {
      DEBUG ((
        DEBUG_ERROR,
        "Delete Variable %g:%s Write Protected\r\n",
        &CurVarGuid,
        CurVarName
        ));
      continue;
    }

    ClearVarStatus = SmmVar->SmmSetVariable (
                               CurVarName,
                               &CurVarGuid,
                               0,
                               0,
                               NULL
                               );
    DEBUG ((
      DEBUG_ERROR,
      "Delete Variable %g:%s %r\r\n",
      &CurVarGuid,
      CurVarName,
      ClearVarStatus
      ));
  }

ExitDeleteNsVars:
  if (CurVarName) {
    FreePool (CurVarName);
  }

  if (NextVarName) {
    FreePool (NextVarName);
  }

  return Status;
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
  UINT64                        FvHeaderOffset;
  UINT64                        PartitionSize;
  EFI_PHYSICAL_ADDRESS          CpuBlParamsAddr;
  UINT16                        DeviceInstance;

  if ((CommBuffer == NULL) || (CommBufferSize == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Communication buffer : %r\n", __FUNCTION__, EFI_INVALID_PARAMETER));
    return EFI_SUCCESS;
  }

  if (*CommBufferSize < sizeof (SATMC_MM_COMMUNICATE_PAYLOAD)) {
    DEBUG ((DEBUG_ERROR, "%a: Communication buffer : %r\n", __FUNCTION__, EFI_BUFFER_TOO_SMALL));
    return EFI_SUCCESS;
  }

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
      Status = GetCpuBlParamsAddrStMm (&CpuBlParamsAddr);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to get CpuBl Addr %r\n",
          __FUNCTION__,
          Status
          ));
        goto ExitSatMcMsgHandler;
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
        goto ExitSatMcMsgHandler;
      }

      Status = CorruptFvHeader (FvHeaderOffset, PartitionSize);
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
    case CLEAR_EFI_NSVARS:
      Status = DeleteNsVars ();
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to Erase Early Vars Partition %r, Cmd %u",
          __FUNCTION__,
          Status,
          SatMcMmMsg->Command
          ));
        goto ExitSatMcMsgHandler;
      }

      Status = EraseEarlyVarsPartition ();
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to Erase Early Vars Partition %r, Cmd %u",
          __FUNCTION__,
          Status,
          SatMcMmMsg->Command
          ));
        goto ExitSatMcMsgHandler;
      }

      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a: Unknown command %u\n", __FUNCTION__, SatMcMmMsg->Command));
      Status = EFI_INVALID_PARAMETER;
      break;
  }

  DEBUG ((DEBUG_INFO, "%a: Returning %r \n", __FUNCTION__, Status));

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
  if (EFI_ERROR (Status)) {
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

  Status = gMmst->MmLocateProtocol (
                    &gEfiSmmVariableProtocolGuid,
                    NULL,
                    (VOID **)&SmmVar
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: gEfiSmmVariableProtocolGuid: NOT LOCATED!\n",
      __FUNCTION__
      ));
    SmmVar = NULL;
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
      "%a: failed to Register CMET log handler %r\n",
      __FUNCTION__,
      Status
      ));
  }

  Status = RegisterSatMcHandler ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to Register SatMc log handler %r\n",
      __FUNCTION__,
      Status
      ));
  }

  return EFI_SUCCESS;
}
