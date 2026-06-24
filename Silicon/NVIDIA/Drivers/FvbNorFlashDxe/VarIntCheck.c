/** @file

  Standalone MM Variable Integrity driver.

  SPDX-FileCopyrightText: Copyright (c) 2024 - 2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/MmServicesTableLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include "Base.h"
#include "FvbPrivate.h"
#include "Library/DebugLib.h"
#include "Library/MemoryAllocationLib.h"
#include <Protocol/SmmVariable.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <IndustryStandard/Tpm20.h>
#include <Library/ArmSvcLib.h>
#include <Library/HashApiLib.h>
#include <Library/PrintLib.h>
#include <Library/OpteeNvLib.h>
#include <Library/NvVarIntLib.h>
#include <Library/NVIDIADebugLib.h>

#define HEADER_SZ_BYTES    (1)
#define MAX_VALID_RECORDS  (2)

typedef struct {
  UINT8     *Measurement;
  UINT64    ByteOffset;
} MEASURE_REC_TYPE;

typedef struct {
  BOOLEAN        Valid;
  CONST CHAR8    *Stage;
  CHAR16         *VariableName;
  EFI_GUID       VendorGuid;
  UINT32         Attributes;
  UINTN          DataSize;
  EFI_STATUS     ComputeStatus;
} VAR_INT_MEASUREMENT_CONTEXT;

typedef enum {
  VarIntRecordVersionV0,
  VarIntRecordVersionV1
} VAR_INT_RECORD_VERSION;

NVIDIA_VAR_INT_PROTOCOL             *VarIntProto = NULL;
STATIC MEASURE_REC_TYPE             *LastMeasurements[MAX_VALID_RECORDS];
STATIC UINT8                        *CurMeas;
STATIC UINT8                        *SpeculativeMeasurement;
STATIC BOOLEAN                      SpeculativeMeasurementValid  = FALSE;
STATIC BOOLEAN                      PreWrittenMeasurementPending = FALSE;
STATIC BOOLEAN                      PrePostComparisonValid       = FALSE;
STATIC BOOLEAN                      PrePostComparisonMatched     = FALSE;
STATIC CONST UINT16                 VarAuthTa                    = 5U;
STATIC UINT16                       OpteeVmId                    = 0;
STATIC UINT16                       MmVmId                       = 0;
STATIC UINT64                       FfaHandle                    = 0;
STATIC VAR_INT_MEASUREMENT_CONTEXT  LastMeasurementContext;

STATIC
UINT32
GetMeasurementPayloadSize (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  );

STATIC
EFI_STATUS
UpdateLiveMeasurementRecordStates (
  IN NVIDIA_VAR_INT_PROTOCOL  *This,
  IN EFI_STATUS               PreviousResult
  );

STATIC
EFI_STATUS
GetLastValidMeasurements (
  IN  NVIDIA_VAR_INT_PROTOCOL  *VarInt,
  OUT MEASURE_REC_TYPE         **Records,
  OUT UINT32                   *NumRecords
  );

STATIC
CONST CHAR8 *
VariableOperationName (
  IN UINT32  Attributes,
  IN UINTN   DataSize
  )
{
  if (DataSize == 0) {
    return "delete";
  }

  if ((Attributes & EFI_VARIABLE_APPEND_WRITE) != 0) {
    return "append";
  }

  return "write";
}

STATIC
VOID
ClearMeasurementContext (
  VOID
  )
{
  ZeroMem (&LastMeasurementContext, sizeof (LastMeasurementContext));
}

STATIC
VOID
SetMeasurementContext (
  IN CONST CHAR8  *Stage,
  IN CHAR16       *VariableName,
  IN EFI_GUID     *VendorGuid,
  IN UINT32       Attributes,
  IN UINTN        DataSize,
  IN EFI_STATUS   ComputeStatus
  )
{
  ClearMeasurementContext ();

  if ((VariableName == NULL) || (VendorGuid == NULL)) {
    return;
  }

  LastMeasurementContext.Valid         = TRUE;
  LastMeasurementContext.Stage         = Stage;
  LastMeasurementContext.VariableName  = VariableName;
  LastMeasurementContext.Attributes    = Attributes;
  LastMeasurementContext.DataSize      = DataSize;
  LastMeasurementContext.ComputeStatus = ComputeStatus;
  CopyGuid (&LastMeasurementContext.VendorGuid, VendorGuid);
}

STATIC
VOID
LogMeasurementContext (
  IN CONST CHAR8  *Reason,
  IN EFI_STATUS   WriteStatus
  )
{
  if (LastMeasurementContext.Valid == FALSE) {
    return;
  }

  DEBUG ((
    DEBUG_ERROR,
    "VarIntMeasContext: reason=%a stage=%a op=%a name=%s guid=%g attr=0x%08x size=%lu compute=%r write=%r\n",
    Reason,
    LastMeasurementContext.Stage,
    VariableOperationName (LastMeasurementContext.Attributes, LastMeasurementContext.DataSize),
    LastMeasurementContext.VariableName,
    &LastMeasurementContext.VendorGuid,
    LastMeasurementContext.Attributes,
    (UINT64)LastMeasurementContext.DataSize,
    LastMeasurementContext.ComputeStatus,
    WriteStatus
    ));
}

STATIC
CONST CHAR8 *
MeasurementHeaderName (
  IN UINT8  Header
  )
{
  switch (Header) {
    case VAR_INT_PENDING:
      return "V0_PENDING";
    case VAR_INT_VALID:
      return "V0_VALID";
    case VAR_INT_INVALID:
      return "V0_INVALID";
    case VAR_INT_V1_PENDING:
      return "V1_PENDING";
    case VAR_INT_V1_VALID:
      return "V1_VALID";
    case VAR_INT_V1_INVALID:
      return "V1_INVALID";
    case FVB_ERASED_BYTE:
      return "ERASED";
    default:
      return "UNKNOWN";
  }
}

STATIC
VOID
LogMeasurementWrite (
  IN CONST CHAR8  *Reason,
  IN UINT64       Offset,
  IN UINT32       Size,
  IN UINT8        OldHeader,
  IN UINT8        NewHeader,
  IN EFI_STATUS   Status
  )
{
  DEBUG ((
    DEBUG_ERROR,
    "VarIntMeasWrite: %a offset=0x%lx size=%u old=0x%x(%a) new=0x%x(%a) status=%r\n",
    Reason,
    Offset,
    Size,
    OldHeader,
    MeasurementHeaderName (OldHeader),
    NewHeader,
    MeasurementHeaderName (NewHeader),
    Status
    ));
  LogMeasurementContext (Reason, Status);
}

STATIC
VOID
PrintMeas (
  IN UINT8  *Meas,
  IN UINTN  Size
  )
{
  DEBUG_CODE_BEGIN ();
  for (int i = 0; i < Size - 1; i++) {
    DEBUG ((DEBUG_INFO, "PrintMeas: Meas[%d] 0x%x  ", i, Meas[i]));
  }

  DEBUG ((DEBUG_INFO, "\n"));
  DEBUG_CODE_END ();
}

STATIC
VOID
LogMeasurementPreview (
  IN CONST CHAR8  *Reason,
  IN UINT8        *Measurement,
  IN UINTN        Size
  )
{
  if ((Reason == NULL) || (Measurement == NULL) || (Size < (HEADER_SZ_BYTES + 8))) {
    return;
  }

  DEBUG ((
    DEBUG_ERROR,
    "VarIntMeasPreview: %a header=0x%x(%a) payload=%02x%02x%02x%02x%02x%02x%02x%02x\n",
    Reason,
    Measurement[0],
    MeasurementHeaderName (Measurement[0]),
    Measurement[HEADER_SZ_BYTES + 0],
    Measurement[HEADER_SZ_BYTES + 1],
    Measurement[HEADER_SZ_BYTES + 2],
    Measurement[HEADER_SZ_BYTES + 3],
    Measurement[HEADER_SZ_BYTES + 4],
    Measurement[HEADER_SZ_BYTES + 5],
    Measurement[HEADER_SZ_BYTES + 6],
    Measurement[HEADER_SZ_BYTES + 7]
    ));
}

STATIC
VOID
ClearSpeculativeMeasurement (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  SpeculativeMeasurementValid = FALSE;

  if ((This != NULL) && (SpeculativeMeasurement != NULL)) {
    ZeroMem (SpeculativeMeasurement, This->MeasurementSize);
  }
}

STATIC
VOID
ClearLastMeasurementBuffers (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  UINTN  Index;

  if (This == NULL) {
    return;
  }

  for (Index = 0; Index < MAX_VALID_RECORDS; Index++) {
    if ((LastMeasurements[Index] != NULL) &&
        (LastMeasurements[Index]->Measurement != NULL))
    {
      ZeroMem (LastMeasurements[Index]->Measurement, This->MeasurementSize);
    }
  }
}

STATIC
VOID
LogPrePostMeasurementCompare (
  IN NVIDIA_VAR_INT_PROTOCOL  *This,
  IN UINT8                    *CommittedMeasurement
  )
{
  UINT32   PayloadSize;
  BOOLEAN  Match;

  PrePostComparisonValid   = FALSE;
  PrePostComparisonMatched = FALSE;

  if ((This == NULL) || (CommittedMeasurement == NULL) ||
      (SpeculativeMeasurement == NULL) || (SpeculativeMeasurementValid == FALSE))
  {
    return;
  }

  PayloadSize = GetMeasurementPayloadSize (This);
  if (PayloadSize < 8) {
    ClearSpeculativeMeasurement (This);
    return;
  }

  Match = (BOOLEAN)(CompareMem (
                      &SpeculativeMeasurement[HEADER_SZ_BYTES],
                      &CommittedMeasurement[HEADER_SZ_BYTES],
                      PayloadSize
                      ) == 0);

  PrePostComparisonValid   = TRUE;
  PrePostComparisonMatched = Match;

  if (Match == TRUE) {
    DEBUG ((
      DEBUG_INFO,
      "VarIntPrePostMatch: op=%a name=%s guid=%g attr=0x%08x size=%lu\n",
      VariableOperationName (LastMeasurementContext.Attributes, LastMeasurementContext.DataSize),
      LastMeasurementContext.VariableName,
      &LastMeasurementContext.VendorGuid,
      LastMeasurementContext.Attributes,
      (UINT64)LastMeasurementContext.DataSize
      ));
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "VarIntPrePostMismatch: op=%a name=%s guid=%g attr=0x%08x size=%lu pre=%02x%02x%02x%02x%02x%02x%02x%02x post=%02x%02x%02x%02x%02x%02x%02x%02x\n",
      VariableOperationName (LastMeasurementContext.Attributes, LastMeasurementContext.DataSize),
      LastMeasurementContext.VariableName,
      &LastMeasurementContext.VendorGuid,
      LastMeasurementContext.Attributes,
      (UINT64)LastMeasurementContext.DataSize,
      SpeculativeMeasurement[HEADER_SZ_BYTES + 0],
      SpeculativeMeasurement[HEADER_SZ_BYTES + 1],
      SpeculativeMeasurement[HEADER_SZ_BYTES + 2],
      SpeculativeMeasurement[HEADER_SZ_BYTES + 3],
      SpeculativeMeasurement[HEADER_SZ_BYTES + 4],
      SpeculativeMeasurement[HEADER_SZ_BYTES + 5],
      SpeculativeMeasurement[HEADER_SZ_BYTES + 6],
      SpeculativeMeasurement[HEADER_SZ_BYTES + 7],
      CommittedMeasurement[HEADER_SZ_BYTES + 0],
      CommittedMeasurement[HEADER_SZ_BYTES + 1],
      CommittedMeasurement[HEADER_SZ_BYTES + 2],
      CommittedMeasurement[HEADER_SZ_BYTES + 3],
      CommittedMeasurement[HEADER_SZ_BYTES + 4],
      CommittedMeasurement[HEADER_SZ_BYTES + 5],
      CommittedMeasurement[HEADER_SZ_BYTES + 6],
      CommittedMeasurement[HEADER_SZ_BYTES + 7]
      ));
    LogMeasurementPreview ("pre-model", SpeculativeMeasurement, This->MeasurementSize);
    LogMeasurementPreview ("post-commit", CommittedMeasurement, This->MeasurementSize);
  }

  ClearSpeculativeMeasurement (This);
}

/*
 * FfaInit
 * Initialize the FFA communication with the Optee VM.
 * Use the FFA_SHARE_MEM_REQ_64/32 to share the memory with the Optee VM.
 *
 * @param[out] OpteeVmId  Optee VM ID.
 *
 */
STATIC
EFI_STATUS
FfaInit (
  IN NVIDIA_VAR_INT_PROTOCOL  *VarInt
  )
{
  EFI_STATUS  Status;
  UINT64      FfaTxBufferAddr;
  UINT32      FfaTxBufferSize;
  UINT64      FfaRxBufferAddr;
  UINT32      FfaRxBufferSize;
  UINT32      TotalLength;

  Status = FfaGetOpteeVmId (&OpteeVmId);
  NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitFfaInit, "Failed to get Optee VM ID");

  Status = FfaGetMmVmId (&MmVmId);
  NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitFfaInit, "Failed to get MM VM ID");

  Status = FfaGetTxRxBuffer (&FfaTxBufferAddr, &FfaTxBufferSize, &FfaRxBufferAddr, &FfaRxBufferSize);
  NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitFfaInit, "Failed to get Tx/Rx buffer");

  DEBUG ((DEBUG_ERROR, "FfaTxBufferAddr: 0x%lx\n", FfaTxBufferAddr));
  DEBUG ((DEBUG_ERROR, "FfaTxBufferSize: 0x%x\n", FfaTxBufferSize));
  DEBUG ((DEBUG_ERROR, "FfaRxBufferAddr: 0x%lx\n", FfaRxBufferAddr));
  DEBUG ((DEBUG_ERROR, "FfaRxBufferSize: 0x%x\n", FfaRxBufferSize));

  Status = PrepareFfaMemoryDescriptor (
             FfaTxBufferAddr,
             FfaTxBufferSize,
             VarInt->CurMeasurement,
             VarInt->MeasurementSize,
             MmVmId,
             OpteeVmId,
             &TotalLength
             );
  NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitFfaInit, "Failed to prepare FFA memory descriptor");

  Status = FfaSendShareCommand (TotalLength, TotalLength, FfaTxBufferAddr, EFI_SIZE_TO_PAGES (VarInt->MeasurementSize), &FfaHandle);
  NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitFfaInit, "Failed to send FFA share command");

ExitFfaInit:
  return Status;
}

/*
 * SendFfaCmd
 * Send a command to the Optee VM to get the measurement signed.
 * This function is for deployments where StMM is run as Optee TA, the message
 * is sent to the Optee PTA.
 *
 * @param[in,out] Meas  Measurement buffer to be signed.
 * @param[in]     Size  Size of the measurement.
 *
 * @result  EFI_SUCCESS Succesfully signed the measurement
 *          Other       Optee PTA returned failure.
 */
STATIC
EFI_STATUS
SendFfaCmd (
  IN OUT UINT8   *Meas,
  IN     UINT32  Size
  )
{
  EFI_STATUS    Status;
  UINT16        OpteeVmId;
  ARM_SVC_ARGS  SvcArgs;
  UINT16        MmId = 0x8002;

  Status = FfaGetOpteeVmId (&OpteeVmId);
  NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "Failed to get Optee VM ID");

  ZeroMem (&SvcArgs, sizeof (SvcArgs));

  SvcArgs.Arg0 = ARM_FID_FFA_MSG_SEND_DIRECT_REQ_AARCH64;
  SvcArgs.Arg1 = OpteeVmId | MmId << 16;
  SvcArgs.Arg2 = 0;
  SvcArgs.Arg3 = OPTEE_FFA_SERVICE_ID;
  SvcArgs.Arg4 = OPTEE_FFA_SIGN_FID;
  SvcArgs.Arg5 = Size;
  SvcArgs.Arg6 = FfaHandle;

  PrintMeas (Meas, Size);
  DEBUG ((DEBUG_INFO, "SendFfaCmd: Sending COMMAND to OPTEE VM ID 0x%x\n", OpteeVmId));

  ArmCallSvc (&SvcArgs);

  if ((SvcArgs.Arg0 == ARM_FID_FFA_MSG_SEND_DIRECT_RESP) &&
      (SvcArgs.Arg3 == FFA_OK))
  {
    Status = EFI_SUCCESS;
    DEBUG ((DEBUG_INFO, "Command successful\n"));
    DEBUG ((DEBUG_INFO, "SvcArgs.Arg0 0x%lx Arg1 0x%lx Arg2 0x%lx Arg3 0x%lx\n", SvcArgs.Arg0, SvcArgs.Arg1, SvcArgs.Arg2, SvcArgs.Arg3));
    DEBUG ((DEBUG_INFO, "SvcArgs.Arg4 0x%lx Arg5 0x%lx Arg6 0x%lx Arg7 0x%lx\n", SvcArgs.Arg4, SvcArgs.Arg5, SvcArgs.Arg6, SvcArgs.Arg7));

    PrintMeas (Meas, Size);
  } else {
    Status = EFI_UNSUPPORTED;
    DEBUG ((
      DEBUG_ERROR,
      "%a: FFA Command failed 0x%x\n",
      __FUNCTION__,
      SvcArgs.Arg0
      ));
    DEBUG ((DEBUG_ERROR, "SvcArgs.Arg0 0x%lx Arg1 0x%lx Arg2 0x%lx Arg3 0x%lx\n", SvcArgs.Arg0, SvcArgs.Arg1, SvcArgs.Arg2, SvcArgs.Arg3));
    DEBUG ((DEBUG_ERROR, "SvcArgs.Arg4 0x%lx Arg5 0x%lx Arg6 0x%lx Arg7 0x%lx\n", SvcArgs.Arg4, SvcArgs.Arg5, SvcArgs.Arg6, SvcArgs.Arg7));
  }

  return Status;
}

/*
 * SendOpteeFfaCmd
 * Send a command to the Optee VM to get the measurement signed.
 * This function is for deployments where StMM is run as Optee TA, the message
 * is sent to the Optee PTA.
 *
 * @param[in,out] Meas  Measurement buffer to be signed.
 * @param[in]     Size  Size of the measurement.
 *
 * @result  EFI_SUCCESS Succesfully signed the measurement
 *          Other       Optee PTA returned failure.
 */
STATIC
EFI_STATUS
SendOpteeFfaCmd (
  IN OUT UINT8   *Meas,
  IN     UINT32  Size
  )
{
  ARM_SVC_ARGS  SvcArgs;
  EFI_STATUS    Status;

  ZeroMem (&SvcArgs, sizeof (SvcArgs));

  SvcArgs.Arg0 = ARM_FID_FFA_MSG_SEND_DIRECT_REQ;
  SvcArgs.Arg1 = VarAuthTa;
  SvcArgs.Arg2 = Size;
  SvcArgs.Arg3 = (UINT64)Meas;

  ArmCallSvc (&SvcArgs);

  if (SvcArgs.Arg3 == OPTEE_SUCCESS) {
    Status = EFI_SUCCESS;
  } else {
    Status = EFI_UNSUPPORTED;
    DEBUG ((
      DEBUG_ERROR,
      "%a: Optee Command failed %u\n",
      __FUNCTION__,
      SvcArgs.Arg3
      ));
  }

  return Status;
}

/*
 * SendOptee Cmd
 * Send a command to the Jetson User Key PTA to get the measurement signed.
 *
 * @param[in,out] Meas  Measurement buffer to be signed.
 * @param[in]     Size  Size of the measurement.
 *
 * @result  EFI_SUCCESS Succesfully signed the measurement
 *          Other       Optee PTA returned failure.
 */
STATIC
EFI_STATUS
SendOpteeCmd (
  IN OUT UINT8   *Meas,
  IN     UINT32  Size
  )
{
  EFI_STATUS  Status;

  if (IsOpteePresent ()) {
    Status = SendOpteeFfaCmd (Meas, Size);
  } else {
    Status = SendFfaCmd (Meas, Size);
  }

  return Status;
}

/**
 * GetMeasurementSizes
 * Util Fn to get the size of the hash measnurement.
 *
 * @param MeasSize    Output containing the size.
 *
 * @retval EFI_SUCCESS      Returned measurement size.
 *         EFI_UNSUPPOERTED The Hash scheme isn't supported.
 */
STATIC
EFI_STATUS
GetMeasurementSize (
  UINT32  *MeasSize
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  switch (PcdGet32 (PcdHashApiLibPolicy)) {
    case HASH_ALG_SHA256:
    case HASH_ALG_SM3_256:
      *MeasSize = 32;
      break;
    case HASH_ALG_SHA384:
      *MeasSize = 48;
      break;
    case HASH_ALG_SHA512:
      *MeasSize = 64;
      break;
    default:
      *MeasSize = 0;
      Status    = EFI_UNSUPPORTED;
  }

  return Status;
}

/*
 * PartitionRead
 * Util Function to read out of the Reserved Partition.
 * The caller ensures that the read offset isn't stradling erase blocks.
 *
 * @param[in]     This    Variable Integrity Protocol Pointer.
 * @param[in]     Offset  Partition Offset to read from.
 * @param[in]     Size    Number of Bytes to read.
 * @param[out]    Buffer  Output buffer to read to.
 *
 * @result  EFI_SUCCESS             Succesful read.
 *          EFI_INVALID_PARAMETER   Invalid Partition Offset.
 */
STATIC
EFI_STATUS
PartitionRead (
  IN  NVIDIA_VAR_INT_PROTOCOL  *This,
  IN  UINT32                   Offset,
  IN  UINT32                   Size,
  OUT UINT8                    *Buffer
  )
{
  EFI_STATUS  Status;
  UINT32      PartitionStart;
  UINT32      PartitionEnd;
  UINT32      BufferOffset;

  PartitionStart = This->PartitionByteOffset;
  PartitionEnd   = PartitionStart + This->PartitionSize;

  if ((Offset < PartitionStart) && (Offset > PartitionEnd)) {
    Status = EFI_INVALID_PARAMETER;
    goto ExitPartitionRead;
  }

  if ((Offset + Size) > PartitionEnd) {
    Status = EFI_INVALID_PARAMETER;
    goto ExitPartitionRead;
  }

  BufferOffset = Offset - PartitionStart;
  CopyMem (Buffer, (This->PartitionData + BufferOffset), Size);
  Status = EFI_SUCCESS;
ExitPartitionRead:
  return Status;
}

/*
 * PartitionWrite
 * Util Function to write to the Reserved Partition.
 * The caller of the function makes sure that the write isn't stradling erase
 * blocks, so the checks here are minimal.
 *
 * @param[in]     This    Variable Integrity Protocol Pointer.
 * @param[in]     Offset  Partition Offset to write to.
 * @param[in]     Size    Number of Bytes to write.
 * @param[in]     Buffer  Input buffer to copy from.
 *
 * @result  EFI_SUCCESS            Succesful write
 *          EFI_INVALID_PARAMETER  Invalid Flash offset.
 *          Other                  Failure to write to Flash.
 */
STATIC
EFI_STATUS
PartitionWrite (
  IN  NVIDIA_VAR_INT_PROTOCOL  *This,
  IN  UINT32                   Offset,
  IN  UINT32                   Size,
  IN  UINT8                    *Buffer
  )
{
  EFI_STATUS  Status;
  UINT32      PartitionStart;
  UINT32      PartitionEnd;
  UINT32      BufferOffset;

  PartitionStart = This->PartitionByteOffset;
  PartitionEnd   = PartitionStart + This->PartitionSize;

  if ((Offset < PartitionStart) && (Offset > PartitionEnd)) {
    Status = EFI_INVALID_PARAMETER;
    goto ExitPartitionWrite;
  }

  if ((Offset + Size) > PartitionEnd) {
    Status = EFI_INVALID_PARAMETER;
    goto ExitPartitionWrite;
  }

  BufferOffset = Offset - PartitionStart;

  /* Update the Partition.*/
  Status = This->NorFlashProtocol->Write (
                                     This->NorFlashProtocol,
                                     Offset,
                                     Size,
                                     Buffer
                                     );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Write Failed(%r) at %lx Size %u\n",
      __FUNCTION__,
      Status,
      Offset,
      Size
      ));
    goto ExitPartitionWrite;
  }

  DEBUG ((DEBUG_INFO, "PartitionWrite: Buffer 0x%lx Size %u\n", Buffer, Size));
  PrintMeas (Buffer, Size);
  /* Update the Partition Data*/
  CopyMem ((This->PartitionData + BufferOffset), Buffer, Size);

ExitPartitionWrite:
  return Status;
}

/*
 * PartitionErase
 * Util Function to Erase LBAs on the Reserved Partition.
 *
 * @param[in]     This      Variable Integrity Protocol Pointer.
 * @param[in]     Lba       Starting LBA to start erasing.
 * @param[in]     NumBlocks Number of LBAs to erase.

 *
 * @result  EFI_SUCCESS     Succesful write
 *          Other           Failure to erase LBAs on Flash.
 */
STATIC
EFI_STATUS
PartitionErase (
  IN  NVIDIA_VAR_INT_PROTOCOL  *This,
  IN  UINT32                   Lba,
  IN  UINT32                   NumBlocks
  )
{
  EFI_STATUS  Status;
  UINT32      PartitionStart;
  UINT32      PartitionEnd;
  UINT32      BufferOffset;
  UINT32      Offset;
  UINT32      Size;

  PartitionStart = This->PartitionByteOffset;
  PartitionEnd   = PartitionStart + This->PartitionSize;
  Offset         = Lba * This->BlockSize;
  Size           = NumBlocks * This->BlockSize;

  if ((Offset < PartitionStart) && (Offset > PartitionEnd)) {
    Status = EFI_INVALID_PARAMETER;
    goto ExitPartitionErase;
  }

  if ((Offset + Size) > PartitionEnd) {
    Status = EFI_INVALID_PARAMETER;
    goto ExitPartitionErase;
  }

  BufferOffset = Offset - PartitionStart;

  /* Erase Partition Data*/
  Status = This->NorFlashProtocol->Erase (
                                     This->NorFlashProtocol,
                                     Lba,
                                     NumBlocks
                                     );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Erase Failed(%r) at Block %u NumBlocks %u\n",
      __FUNCTION__,
      Status,
      Lba,
      NumBlocks
      ));
    goto ExitPartitionErase;
  }

  /* Update the Partition Data*/
  SetMem ((This->PartitionData + BufferOffset), Size, FVB_ERASED_BYTE);

ExitPartitionErase:
  return Status;
}

STATIC
UINT32
GetMeasurementPayloadSize (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  return This->MeasurementSize - HEADER_SZ_BYTES;
}

STATIC
BOOLEAN
RecordWindowFitsInBlock (
  IN NVIDIA_VAR_INT_PROTOCOL  *This,
  IN UINT64                   Offset,
  IN UINT32                   NumRecords
  )
{
  UINT64  BlockStart;
  UINT64  BlockEnd;
  UINT64  WriteSize;

  BlockStart = (Offset / This->BlockSize) * This->BlockSize;
  BlockEnd   = BlockStart + This->BlockSize;
  WriteSize  = This->MeasurementSize * NumRecords;

  return (BOOLEAN)((Offset + WriteSize) <= BlockEnd);
}

STATIC
BOOLEAN
IsRecordPending (
  IN UINT8  Header
  )
{
  return (BOOLEAN)((Header == VAR_INT_PENDING) || (Header == VAR_INT_V1_PENDING));
}

STATIC
BOOLEAN
IsRecordValid (
  IN UINT8  Header
  )
{
  return (BOOLEAN)((Header == VAR_INT_VALID) || (Header == VAR_INT_V1_VALID));
}

STATIC
BOOLEAN
IsRecordLive (
  IN UINT8  Header
  )
{
  return (BOOLEAN)(IsRecordPending (Header) || IsRecordValid (Header));
}

STATIC
BOOLEAN
IsRecordV0 (
  IN UINT8  Header
  )
{
  return (BOOLEAN)(
                   (Header == VAR_INT_PENDING) ||
                   (Header == VAR_INT_VALID) ||
                   (Header == VAR_INT_INVALID)
                   );
}

STATIC
BOOLEAN
IsRecordV1 (
  IN UINT8  Header
  )
{
  return (BOOLEAN)(
                   (Header == VAR_INT_V1_PENDING) ||
                   (Header == VAR_INT_V1_VALID) ||
                   (Header == VAR_INT_V1_INVALID)
                   );
}

STATIC
UINT8
MakeRecordHeader (
  IN VAR_INT_RECORD_VERSION  Version,
  IN UINT8                   V0State
  )
{
  if (Version == VarIntRecordVersionV1) {
    switch (V0State) {
      case VAR_INT_PENDING:
        return VAR_INT_V1_PENDING;
      case VAR_INT_VALID:
        return VAR_INT_V1_VALID;
      case VAR_INT_INVALID:
        return VAR_INT_V1_INVALID;
      default:
        return V0State;
    }
  }

  return V0State;
}

STATIC
UINT8
SetRecordStatePreserveVersion (
  IN UINT8  Header,
  IN UINT8  V0State
  )
{
  if (IsRecordV1 (Header)) {
    return MakeRecordHeader (VarIntRecordVersionV1, V0State);
  }

  return MakeRecordHeader (VarIntRecordVersionV0, V0State);
}

STATIC
EFI_STATUS
ComputedMeasurementMatchesValidRecord (
  IN  NVIDIA_VAR_INT_PROTOCOL  *This,
  IN  UINT8                    *Measurement,
  OUT BOOLEAN                  *Matched
  )
{
  EFI_STATUS        Status;
  UINT32            NumValidRecords;
  UINT32            PayloadSize;
  UINTN             Index;
  MEASURE_REC_TYPE  *Record;

  if ((This == NULL) || (Measurement == NULL) || (Matched == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Matched    = FALSE;
  PayloadSize = GetMeasurementPayloadSize (This);
  Status      = GetLastValidMeasurements (
                  This,
                  LastMeasurements,
                  &NumValidRecords
                  );
  if (EFI_ERROR (Status)) {
    ClearLastMeasurementBuffers (This);
    return Status;
  }

  for (Index = 0; Index < NumValidRecords; Index++) {
    Record = LastMeasurements[Index];
    if ((IsRecordV1 (Record->Measurement[0]) == TRUE) &&
        (IsRecordValid (Record->Measurement[0]) == TRUE) &&
        (CompareMem (
           &Measurement[HEADER_SZ_BYTES],
           &Record->Measurement[HEADER_SZ_BYTES],
           PayloadSize
           ) == 0))
    {
      *Matched = TRUE;
      break;
    }
  }

  ClearLastMeasurementBuffers (This);
  return EFI_SUCCESS;
}

STATIC
BOOLEAN
IsV0MigrationAllowed (
  VOID
  )
{
  /*
   * TODO: Replace this stub with the MM-visible MB2 ratchet update status.
   * This should return TRUE only for the skipped-transition RUS state.
   */
  return TRUE;
}

STATIC
EFI_STATUS
ComputeCurrentMeasurement (
  IN NVIDIA_VAR_INT_PROTOCOL  *This,
  IN CHAR16                   *VariableName,
  IN EFI_GUID                 *VendorGuid,
  IN UINT32                   Attributes,
  IN VOID                     *Data,
  IN UINTN                    Size,
  IN VAR_INT_RECORD_VERSION   Version,
  OUT UINT8                   *Measurement
  )
{
  EFI_STATUS  Status;
  UINT32      PayloadSize;
  UINT8       *Meas;

  PayloadSize = GetMeasurementPayloadSize (This);
  Meas        = &Measurement[HEADER_SZ_BYTES];

  ZeroMem (Measurement, This->MeasurementSize);

  if (Version == VarIntRecordVersionV1) {
    Status = ComputeVarMeasurementV1 (VariableName, VendorGuid, Attributes, Data, Size, Meas);
  } else {
    Status = ComputeVarMeasurementV0 (VariableName, VendorGuid, Attributes, Data, Size, Meas);
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to compute measurement %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitComputeCurrentMeasurement;
  }

  Status = SendOpteeCmd (Meas, PayloadSize);
  if (EFI_ERROR (Status)) {
    NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "Failed to get signed measurement - %r", Status);
    goto ExitComputeCurrentMeasurement;
  }

  Measurement[0] = FVB_ERASED_BYTE;

ExitComputeCurrentMeasurement:
  return Status;
}

/*
 * GetWriteOffset
 * Get the next byte offset in flash to write the next record to.
 * If there isn't an erased section of the flash to write to erase
 * a block and return a new offset.
 *
 * @param[in]   This   Variable Integrity Protocol.
 * @param[out]  Offset Byte Offset to write to.
 *
 * @retval   EFI_SUCCESS Found the next offset to write to
 *           Other       Failed to find the next offset.
 */
STATIC
EFI_STATUS
GetWriteOffset (
  IN  NVIDIA_VAR_INT_PROTOCOL  *This,
  OUT UINT64                   *Offset
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT64      StartOffset;
  UINT64      EndOffset;
  UINT64      CurOffset;
  UINT64      BlockOffset;
  UINT64      BlockEnd;
  UINT64      ValidRecord;
  UINT8       *ReadBuf;
  BOOLEAN     FoundOffset;
  UINT32      CurBlock;
  UINT32      StartBlock;
  UINT32      EndBlock;
  UINT32      NumPartitionBlocks;

  Status      = EFI_NOT_FOUND;
  FoundOffset = FALSE;
  StartOffset = This->PartitionByteOffset;
  EndOffset   = StartOffset + This->PartitionSize;

  ReadBuf            = CurMeas;
  CurOffset          = StartOffset;
  BlockEnd           = CurOffset + This->BlockSize;
  BlockOffset        = CurOffset;
  ValidRecord        = 0;
  NumPartitionBlocks = (This->PartitionSize / This->BlockSize);
  StartBlock         = (StartOffset / This->BlockSize);
  EndBlock           = (StartBlock + NumPartitionBlocks - 1);

  /* Iterate over the partition (block at a time) */
  while ((CurOffset < EndOffset) && (FoundOffset == FALSE)) {
    while (BlockOffset < BlockEnd) {
      if (RecordWindowFitsInBlock (This, BlockOffset, 1) == TRUE) {
        Status = PartitionRead (
                   This,
                   BlockOffset,
                   This->MeasurementSize,
                   ReadBuf
                   );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Failed to read the working area\r\n", __FUNCTION__));
          break;
        }

        if ((ReadBuf[0] == FVB_ERASED_BYTE)) {
          DEBUG ((
            DEBUG_INFO,
            "%a: Found a Valid Write Offset %lx\n",
            __FUNCTION__,
            BlockOffset
            ));
          FoundOffset = TRUE;
          *Offset     = BlockOffset;
          break;
        } else if (IsRecordValid (ReadBuf[0])) {
          ValidRecord = BlockOffset;
        }
      }

      BlockOffset += This->MeasurementSize;
    }

    CurOffset  += This->BlockSize;
    BlockEnd   += This->BlockSize;
    BlockOffset = CurOffset;
  }

  /* Couldn't find an erased region to write to.
   * If there are no valid records, pick the start offset of the partition.
   * else if there is a valid record, pick the next block.
   */
  if (FoundOffset == FALSE) {
    if ((ValidRecord == 0) || (NumPartitionBlocks == 1)) {
      *Offset = This->PartitionByteOffset;
    } else {
      CurBlock = (ValidRecord / This->BlockSize);
      if (CurBlock == EndBlock) {
        *Offset = This->PartitionByteOffset;
      } else {
        *Offset = (CurBlock + 1) * This->BlockSize;
      }
    }
  }

  if ((*Offset % This->BlockSize) == 0) {
    DEBUG ((DEBUG_INFO, "Erasing Block %lu\n", *Offset));
    Status = PartitionErase (
               This,
               (*Offset / This->BlockSize),
               1
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to Erase block at %lu\n", __FUNCTION__, *Offset));
    }
  }

  return Status;
}

/**
  VarIntComputeMeasurement
  Arm measurement tracking for an incoming variable update, or compute the
  committed measurement when called without a variable.

  @param This                  Pointer to Variable Integrity Protocol.
  @param VariableName          Name of the Variable being updated.
  @param VendorGuid            Guid of the Variable being updated.
  @param Data                  Pointer to the Variable Data.
  @param Size                  Size of the Variable Data.

  @retval EFI_SUCCESS          Computed a new measurement for the variables
                               being monitored (or if ignored).
          other                failed to compute new measurement.

**/
STATIC
EFI_STATUS
EFIAPI
VarIntComputeMeasurement (
  IN NVIDIA_VAR_INT_PROTOCOL  *This,
  IN CHAR16                   *VariableName,
  IN EFI_GUID                 *VendorGuid,
  IN UINT32                   Attributes,
  IN VOID                     *Data,
  IN UINTN                    Size
  )
{
  EFI_STATUS  Status;
  BOOLEAN     HasVariableUpdate;

  HasVariableUpdate = (BOOLEAN)((VariableName != NULL) || (VendorGuid != NULL));

  if ((HasVariableUpdate == TRUE) && ((VariableName == NULL) || (VendorGuid == NULL))) {
    return EFI_INVALID_PARAMETER;
  }

  if (HasVariableUpdate == TRUE) {
    This->MeasurementDirty       = FALSE;
    PrePostComparisonValid       = FALSE;
    PrePostComparisonMatched     = FALSE;
    PreWrittenMeasurementPending = FALSE;
    ClearSpeculativeMeasurement (This);
  }

  if ((HasVariableUpdate == TRUE) &&
      (NvVarIntCanUpdateMeasurement (VariableName, VendorGuid, Attributes, Size) == FALSE))
  {
    ClearMeasurementContext ();
    ZeroMem (This->CurMeasurement, This->MeasurementSize);
    Status = EFI_SUCCESS;
    goto ExitComputeVarMeasurement;
  }

  if ((HasVariableUpdate == TRUE) &&
      (NvVarIntIsNoOpUpdate (VariableName, VendorGuid, Attributes, Data, Size) == TRUE))
  {
    ClearMeasurementContext ();
    ZeroMem (This->CurMeasurement, This->MeasurementSize);
    Status = EFI_SUCCESS;
    goto ExitComputeVarMeasurement;
  }

  if (HasVariableUpdate == TRUE) {
    Status = ComputeCurrentMeasurement (
               This,
               VariableName,
               VendorGuid,
               Attributes,
               Data,
               Size,
               VarIntRecordVersionV1,
               This->CurMeasurement
               );
    SetMeasurementContext (
      "pre",
      VariableName,
      VendorGuid,
      Attributes,
      Size,
      Status
      );

    if (EFI_ERROR (Status)) {
      ClearSpeculativeMeasurement (This);
      goto ExitComputeVarMeasurement;
    }

    if (SpeculativeMeasurement != NULL) {
      CopyMem (SpeculativeMeasurement, This->CurMeasurement, This->MeasurementSize);
      SpeculativeMeasurementValid = TRUE;
    }

    This->MeasurementDirty = TRUE;
    goto ExitComputeVarMeasurement;
  }

  Status = ComputeCurrentMeasurement (
             This,
             VariableName,
             VendorGuid,
             Attributes,
             Data,
             Size,
             VarIntRecordVersionV1,
             This->CurMeasurement
             );
  if (!EFI_ERROR (Status)) {
    LogPrePostMeasurementCompare (This, This->CurMeasurement);
  }

  if (EFI_ERROR (Status)) {
    ClearMeasurementContext ();
    ClearSpeculativeMeasurement (This);
  }

ExitComputeVarMeasurement:
  return Status;
}

/*
 * VarIntWriteMeasurement
 * Write the measurement to Flash.
 *
 * @param[in] This    Variable Integrity Measurement Protocol.
 *
 * @return  EFI_SUCCESS Wrote a measurement to the Flash.
 *          Other       Failed to write a measurement.
 */
STATIC
EFI_STATUS
EFIAPI
VarIntWriteMeasurement (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  EFI_STATUS  Status;
  UINT64      CurOffset;
  UINT8       *CorrectedMeasurement;
  BOOLEAN     PreMeasurementWrite;
  BOOLEAN     MeasurementUnchanged;

  Status               = EFI_SUCCESS;
  CorrectedMeasurement = NULL;
  PreMeasurementWrite  = SpeculativeMeasurementValid;
  MeasurementUnchanged = FALSE;

  if (PreWrittenMeasurementPending == TRUE) {
    if (PrePostComparisonValid == FALSE) {
      DEBUG ((DEBUG_ERROR, "%a: Missing pre/post comparison for pending measurement\n", __FUNCTION__));
      Status = EFI_NOT_READY;
      goto ExitVarIntWriteMeasurement;
    }

    if (PrePostComparisonMatched == TRUE) {
      This->CurMeasurement[0] = VAR_INT_V1_PENDING;
      goto ExitVarIntWriteMeasurement;
    }

    CorrectedMeasurement = AllocateCopyPool (This->MeasurementSize, This->CurMeasurement);
    if (CorrectedMeasurement == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ExitVarIntWriteMeasurement;
    }

    Status = UpdateLiveMeasurementRecordStates (This, EFI_ABORTED);
    if (EFI_ERROR (Status)) {
      goto ExitVarIntWriteMeasurement;
    }

    PreWrittenMeasurementPending = FALSE;
    CopyMem (This->CurMeasurement, CorrectedMeasurement, This->MeasurementSize);
  }

  Status = ComputedMeasurementMatchesValidRecord (
             This,
             This->CurMeasurement,
             &MeasurementUnchanged
             );
  if (EFI_ERROR (Status)) {
    goto ExitVarIntWriteMeasurement;
  }

  if (MeasurementUnchanged == TRUE) {
    DEBUG ((
      DEBUG_ERROR,
      "VarIntMeasSkip: committed measurement unchanged\n"
      ));
    ClearMeasurementContext ();
    ClearSpeculativeMeasurement (This);
    PrePostComparisonValid       = FALSE;
    PrePostComparisonMatched     = FALSE;
    PreWrittenMeasurementPending = FALSE;
    This->MeasurementDirty       = FALSE;
    ZeroMem (This->CurMeasurement, This->MeasurementSize);
    Status = EFI_SUCCESS;
    goto ExitVarIntWriteMeasurement;
  }

  Status = GetWriteOffset (This, &CurOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error Getting Offset\n",
      __FUNCTION__
      ));
    goto ExitVarIntWriteMeasurement;
  }

  DEBUG ((DEBUG_INFO, "%a: Write Offset %lu\n", __FUNCTION__, CurOffset));
  This->CurMeasurement[0] = VAR_INT_V1_PENDING;
  Status                  = PartitionWrite (
                              This,
                              CurOffset,
                              This->MeasurementSize,
                              This->CurMeasurement
                              );
  LogMeasurementWrite (
    "new-pending",
    CurOffset,
    This->MeasurementSize,
    FVB_ERASED_BYTE,
    This->CurMeasurement[0],
    Status
    );
  LogMeasurementPreview ("new-pending", This->CurMeasurement, This->MeasurementSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Write measurement to %lu\n", CurOffset));
    goto ExitVarIntWriteMeasurement;
  }

  if (PreMeasurementWrite == TRUE) {
    PreWrittenMeasurementPending = TRUE;
  }

ExitVarIntWriteMeasurement:
  if (EFI_ERROR (Status) && (PreMeasurementWrite == TRUE)) {
    PreWrittenMeasurementPending = FALSE;
    ClearSpeculativeMeasurement (This);
    This->MeasurementDirty = FALSE;
    ZeroMem (This->CurMeasurement, This->MeasurementSize);
  }

  if (CorrectedMeasurement != NULL) {
    FreePool (CorrectedMeasurement);
  }

  return Status;
}

STATIC
BOOLEAN
EFIAPI
VarIntIsDeferred (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  if (This == NULL) {
    return FALSE;
  }

  return This->BootstrapDeferred;
}

STATIC
EFI_STATUS
EFIAPI
VarIntMarkDirty (
  IN NVIDIA_VAR_INT_PROTOCOL  *This,
  IN CHAR16                   *VariableName,
  IN EFI_GUID                 *VendorGuid,
  IN EFI_STATUS               PreviousResult
  )
{
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->BootstrapDeferred == FALSE) {
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (PreviousResult) || (NvVarIntIsExcludedVar (VariableName, VendorGuid) == TRUE)) {
    return EFI_SUCCESS;
  }

  This->MeasurementDirty = TRUE;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
VarIntFlushDeferredMeasurement (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  EFI_STATUS  Status;
  UINT64      CurOffset;
  UINT8       OldHeader;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->BootstrapDeferred == FALSE) {
    return EFI_SUCCESS;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: Flushing deferred bootstrap measurement Dirty=%u\n",
    __FUNCTION__,
    This->MeasurementDirty
    ));

  Status = ComputeCurrentMeasurement (
             This,
             NULL,
             NULL,
             0,
             NULL,
             0,
             VarIntRecordVersionV1,
             This->CurMeasurement
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to compute deferred measurement %r\n", __FUNCTION__, Status));
    goto ExitVarIntFlushDeferredMeasurement;
  }

  Status = GetWriteOffset (This, &CurOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error Getting Offset %r\n", __FUNCTION__, Status));
    goto ExitVarIntFlushDeferredMeasurement;
  }

  This->CurMeasurement[0] = VAR_INT_V1_PENDING;
  Status                  = PartitionWrite (
                              This,
                              CurOffset,
                              This->MeasurementSize,
                              This->CurMeasurement
                              );
  LogMeasurementWrite (
    "deferred-pending",
    CurOffset,
    This->MeasurementSize,
    FVB_ERASED_BYTE,
    This->CurMeasurement[0],
    Status
    );
  LogMeasurementPreview ("deferred-pending", This->CurMeasurement, This->MeasurementSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to write deferred measurement to %lu %r\n", __FUNCTION__, CurOffset, Status));
    goto ExitVarIntFlushDeferredMeasurement;
  }

  OldHeader               = This->CurMeasurement[0];
  This->CurMeasurement[0] = VAR_INT_V1_VALID;
  Status                  = PartitionWrite (
                              This,
                              CurOffset,
                              1,
                              &This->CurMeasurement[0]
                              );
  LogMeasurementWrite (
    "deferred-valid",
    CurOffset,
    1,
    OldHeader,
    This->CurMeasurement[0],
    Status
    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to validate deferred measurement at %lu %r\n", __FUNCTION__, CurOffset, Status));
    goto ExitVarIntFlushDeferredMeasurement;
  }

  ClearMeasurementContext ();
  This->BootstrapDeferred = FALSE;
  This->MeasurementDirty  = FALSE;

ExitVarIntFlushDeferredMeasurement:
  ZeroMem (This->CurMeasurement, This->MeasurementSize);
  return Status;
}

EFI_STATUS
EFIAPI
VarIntFlushDeferredMeasurementAtReadyToBoot (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  EFI_STATUS  Status;

  if ((This == NULL) || (This->FlushDeferredMeasurement == NULL) || (This->IsDeferred == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = This->FlushDeferredMeasurement (This);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to flush deferred measurement %r\n", __FUNCTION__, Status));
    return Status;
  }

  if (This->IsDeferred (This) == TRUE) {
    DEBUG ((DEBUG_ERROR, "%a: Deferred bootstrap measurement still pending\n", __FUNCTION__));
    return EFI_NOT_READY;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
VarIntNotifyExitBootServicesPreserveOnly (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  NvVarIntNotifyExitBootServices ();

  if ((This == NULL) || (This->IsDeferred == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->IsDeferred (This) == TRUE) {
    DEBUG ((DEBUG_ERROR, "%a: Deferred bootstrap measurement still pending at ExitBootServices\n", __FUNCTION__));
    return EFI_NOT_READY;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
VarIntFatalBootstrapFailure (
  IN EFI_STATUS   FailureStatus,
  IN CONST CHAR8  *FailureReason
  )
{
  if (!EFI_ERROR (FailureStatus)) {
    FailureStatus = EFI_DEVICE_ERROR;
  }

  if (FailureReason == NULL) {
    FailureReason = "VarInt bootstrap";
  }

  NV_ASSERT_RETURN (!EFI_ERROR (FailureStatus), CpuDeadLoop (), "%a failed - %r!\r\n", FailureReason, FailureStatus);

  return FailureStatus;
}

/**
  Get the Last Valid Measurement from the partition.

  @param VarInt      - A pointer to the partition data
  @param Records      - Offset of the variable partition
  @param NumRecords        - Size of the partition
  @param RecordOffset   - TRUE if the variable data should be checked
  @param NorFlashProtocol     - Pointer to nor flash protocol
  @param FlashAttributes      - Pointer to flash attributes for the nor flash partition is on

**/
STATIC
EFI_STATUS
GetLastValidMeasurements (
  IN  NVIDIA_VAR_INT_PROTOCOL  *VarInt,
  OUT MEASURE_REC_TYPE         **Records,
  OUT UINT32                   *NumRecords
  )
{
  EFI_STATUS                 Status;
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlash;
  UINT64                     StartOffset;
  UINT64                     EndOffset;
  UINT64                     CurOffset;
  UINT64                     BlockOffset;
  UINT64                     BlockEnd;
  UINT64                     NumValidRecords;
  UINT8                      *ReadBuf;

  ReadBuf  = NULL;
  Status   = EFI_SUCCESS;
  NorFlash = VarInt->NorFlashProtocol;
  if (NorFlash == NULL) {
    Status = EFI_DEVICE_ERROR;
    goto ExitGetLastValidMeasuremets;
  }

  ReadBuf     = CurMeas;
  StartOffset = VarInt->PartitionByteOffset;
  EndOffset   = StartOffset + VarInt->PartitionSize;

  CurOffset       = StartOffset;
  BlockOffset     = CurOffset;
  BlockEnd        = BlockOffset + VarInt->BlockSize;
  NumValidRecords = 0;
  *NumRecords     = 0;

  while (CurOffset < EndOffset) {
    while (BlockOffset < BlockEnd) {
      if (RecordWindowFitsInBlock (VarInt, BlockOffset, 1) == TRUE) {
        Status = PartitionRead (
                   VarInt,
                   BlockOffset,
                   VarInt->MeasurementSize,
                   ReadBuf
                   );
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: NorFlash Read Failed at %lu offset %r\n",
            __FUNCTION__,
            BlockOffset,
            Status
            ));
          goto ExitGetLastValidMeasuremets;
        }

        if (IsRecordLive (ReadBuf[0]) == TRUE) {
          NumValidRecords++;
          if (NumValidRecords > MAX_VALID_RECORDS) {
            DEBUG ((
              DEBUG_ERROR,
              "%a: More than %d Valid measurements found %x\n",
              __FUNCTION__,
              MAX_VALID_RECORDS,
              ReadBuf[0]
              ));
            Status = EFI_DEVICE_ERROR;
            goto ExitGetLastValidMeasuremets;
          } else {
            DEBUG ((DEBUG_INFO, "Found Record at %lu Header %x\n", BlockOffset, ReadBuf[0]));
            CopyMem (Records[(NumValidRecords - 1)]->Measurement, ReadBuf, VarInt->MeasurementSize);
            *NumRecords                               += 1;
            Records[(NumValidRecords - 1)]->ByteOffset = BlockOffset;
          }
        }
      }

      BlockOffset += VarInt->MeasurementSize;
    }

    CurOffset  += VarInt->BlockSize;
    BlockEnd   += VarInt->BlockSize;
    BlockOffset = CurOffset;
  }

ExitGetLastValidMeasuremets:
  return Status;
}

/*
 * CommitMeasurements
 * Commit the Pending measurements to the NorFlash.
 *
 * @param  NumValidRecords Number of Records to Process
 * @param  Measurements    Array of Records to process
 * @param  NorFlashProto   Pointer to the Nor Flash Protocol.
 * @param  PreviousResult  Status of the Update Variable Operation.
 *
 * @retval EFI_SUCCESS Success
 *         Other       Failed to update the records on Flash.
 */
STATIC
EFI_STATUS
CommitMeasurements (
  IN  UINT32                   NumValidRecords,
  IN  MEASURE_REC_TYPE         **Measurements,
  IN  NVIDIA_VAR_INT_PROTOCOL  *VarIntProto,
  IN  EFI_STATUS               PreviousResult
  )
{
  UINTN             Index;
  MEASURE_REC_TYPE  *CurRec;
  EFI_STATUS        Status;
  EFI_STATUS        WriteStatus;
  UINT8             OldHeader;

  Status = EFI_SUCCESS;
  for (Index = 0; Index < NumValidRecords; Index++) {
    CurRec    = Measurements[Index];
    OldHeader = CurRec->Measurement[0];
    if (IsRecordPending (CurRec->Measurement[0]) == TRUE) {
      /* If the Var Update failed, then declare the pending measurement
       * as invalid.
       */
      if (EFI_ERROR (PreviousResult)) {
        CurRec->Measurement[0] = SetRecordStatePreserveVersion (
                                   CurRec->Measurement[0],
                                   VAR_INT_INVALID
                                   );
      } else {
        CurRec->Measurement[0] = SetRecordStatePreserveVersion (
                                   CurRec->Measurement[0],
                                   VAR_INT_VALID
                                   );
      }
    } else {
      /* If the Var Update failed, then don't invalidate the previous
       *  Valid measurement.
       */
      if (EFI_ERROR (PreviousResult)) {
        CurRec->Measurement[0] = SetRecordStatePreserveVersion (
                                   CurRec->Measurement[0],
                                   VAR_INT_VALID
                                   );
      } else {
        CurRec->Measurement[0] = SetRecordStatePreserveVersion (
                                   CurRec->Measurement[0],
                                   VAR_INT_INVALID
                                   );
      }
    }

    DEBUG ((
      DEBUG_INFO,
      "%a: Writing 0x%x to %lu Prev %r\n",
      __FUNCTION__,
      CurRec->Measurement[0],
      CurRec->ByteOffset,
      PreviousResult
      ));
    WriteStatus = PartitionWrite (
                    VarIntProto,
                    CurRec->ByteOffset,
                    1,
                    &CurRec->Measurement[0]
                    );
    LogMeasurementWrite (
      "commit-state",
      CurRec->ByteOffset,
      1,
      OldHeader,
      CurRec->Measurement[0],
      WriteStatus
      );
    if (EFI_ERROR (WriteStatus)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to Write measurement to %lu %r\n",
        __FUNCTION__,
        CurRec->ByteOffset,
        WriteStatus
        ));
      if (!EFI_ERROR (Status)) {
        Status = WriteStatus;
      }
    }
  }

  return Status;
}

STATIC
EFI_STATUS
UpdateLiveMeasurementRecordStates (
  IN NVIDIA_VAR_INT_PROTOCOL  *This,
  IN EFI_STATUS               PreviousResult
  )
{
  EFI_STATUS  Status;
  UINT32      NumValidRecords;

  Status = GetLastValidMeasurements (
             This,
             LastMeasurements,
             &NumValidRecords
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Get Valid Measurements %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitUpdateLiveMeasurementRecordStates;
  }

  if (NumValidRecords == 0) {
    DEBUG ((DEBUG_ERROR, "%a: No Valid Records are found\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto ExitUpdateLiveMeasurementRecordStates;
  }

  Status = CommitMeasurements (
             NumValidRecords,
             LastMeasurements,
             This,
             PreviousResult
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Commit Measurements %r\n",
      __FUNCTION__,
      Status
      ));
  }

ExitUpdateLiveMeasurementRecordStates:
  ClearLastMeasurementBuffers (This);
  return Status;
}

STATIC
EFI_STATUS
FinalizeValidatedRecords (
  IN  NVIDIA_VAR_INT_PROTOCOL  *VarInt,
  IN  UINT32                   NumValidRecords,
  IN  MEASURE_REC_TYPE         **Measurements,
  IN  MEASURE_REC_TYPE         *KeepRecord
  )
{
  UINTN             Index;
  MEASURE_REC_TYPE  *CurRec;
  EFI_STATUS        Status;
  EFI_STATUS        WriteStatus;
  UINT8             OldState;
  UINT8             NewState;

  Status = EFI_SUCCESS;
  for (Index = 0; Index < NumValidRecords; Index++) {
    CurRec   = Measurements[Index];
    OldState = CurRec->Measurement[0];
    NewState = OldState;

    if (CurRec == KeepRecord) {
      if (IsRecordPending (CurRec->Measurement[0]) == TRUE) {
        NewState = SetRecordStatePreserveVersion (
                     CurRec->Measurement[0],
                     VAR_INT_VALID
                     );
      }
    } else if (IsRecordLive (CurRec->Measurement[0]) == TRUE) {
      NewState = SetRecordStatePreserveVersion (
                   CurRec->Measurement[0],
                   VAR_INT_INVALID
                   );
    }

    if (NewState == CurRec->Measurement[0]) {
      continue;
    }

    CurRec->Measurement[0] = NewState;
    WriteStatus            = PartitionWrite (
                               VarInt,
                               CurRec->ByteOffset,
                               1,
                               &CurRec->Measurement[0]
                               );
    LogMeasurementWrite (
      "validate-state",
      CurRec->ByteOffset,
      1,
      OldState,
      CurRec->Measurement[0],
      WriteStatus
      );
    if (EFI_ERROR (WriteStatus)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to update measurement state at %lu %r\n",
        __FUNCTION__,
        CurRec->ByteOffset,
        WriteStatus
        ));
      Status = WriteStatus;
    }
  }

  return Status;
}

STATIC
EFI_STATUS
MigrateV0RecordToV1 (
  IN NVIDIA_VAR_INT_PROTOCOL  *This,
  IN MEASURE_REC_TYPE         *V0Record
  )
{
  EFI_STATUS  Status;
  UINT64      CurOffset;
  UINT8       OldHeader;

  if ((V0Record == NULL) || (IsRecordV0 (V0Record->Measurement[0]) == FALSE)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetWriteOffset (This, &CurOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error Getting Offset\n", __FUNCTION__));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: Migrating V0 measurement to V1 at %lu\n", __FUNCTION__, CurOffset));
  This->CurMeasurement[0] = VAR_INT_V1_PENDING;
  Status                  = PartitionWrite (
                              This,
                              CurOffset,
                              This->MeasurementSize,
                              This->CurMeasurement
                              );
  LogMeasurementWrite (
    "migrate-v1-pending",
    CurOffset,
    This->MeasurementSize,
    FVB_ERASED_BYTE,
    This->CurMeasurement[0],
    Status
    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to write V1 measurement to %lu %r\n", __FUNCTION__, CurOffset, Status));
    return Status;
  }

  OldHeader               = This->CurMeasurement[0];
  This->CurMeasurement[0] = VAR_INT_V1_VALID;
  Status                  = PartitionWrite (
                              This,
                              CurOffset,
                              1,
                              &This->CurMeasurement[0]
                              );
  LogMeasurementWrite (
    "migrate-v1-valid",
    CurOffset,
    1,
    OldHeader,
    This->CurMeasurement[0],
    Status
    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to validate V1 measurement at %lu %r\n", __FUNCTION__, CurOffset, Status));
    return Status;
  }

  OldHeader                = V0Record->Measurement[0];
  V0Record->Measurement[0] = SetRecordStatePreserveVersion (
                               V0Record->Measurement[0],
                               VAR_INT_INVALID
                               );
  Status = PartitionWrite (
             This,
             V0Record->ByteOffset,
             1,
             &V0Record->Measurement[0]
             );
  LogMeasurementWrite (
    "migrate-v0-invalid",
    V0Record->ByteOffset,
    1,
    OldHeader,
    V0Record->Measurement[0],
    Status
    );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to invalidate V0 measurement at %lu %r\n",
      __FUNCTION__,
      V0Record->ByteOffset,
      Status
      ));
  }

  return Status;
}

/*
 * VarIntInvalidateLast
 * Invalidate the Last written measurement. This could be to declare
 * a pending measurement as valid and invalidating the last valid
 * measurement OR
 * vice-versa if the UpdateVariable had failed.
 *
 * @param  This        Pointer to the Variable Integrity Protocol.
 * @param  PrevResult  The status of the Previous Variable Update.
 *
 * @retval EFI_SUCCESS Success
 *         Other       Failed to update the records on Flash.
 */
EFI_STATUS
EFIAPI
VarIntInvalidateLast (
  IN  NVIDIA_VAR_INT_PROTOCOL  *This,
  IN  CHAR16                   *VariableName,
  IN  EFI_GUID                 *VendorGuid,
  IN  EFI_STATUS               PrevResult
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if (NvVarIntIsExcludedVar (VariableName, VendorGuid) == TRUE) {
    ClearMeasurementContext ();
    Status = EFI_SUCCESS;
    goto ExitVarIntInvalidateLast;
  }

  if ((IsRecordPending (This->CurMeasurement[0]) == FALSE) &&
      (PreWrittenMeasurementPending == FALSE))
  {
    Status = EFI_SUCCESS;
    goto ExitVarIntInvalidateLast;
  }

  This->CurMeasurement[0] = SetRecordStatePreserveVersion (
                              This->CurMeasurement[0],
                              VAR_INT_VALID
                              );
  Status = UpdateLiveMeasurementRecordStates (This, PrevResult);

ExitVarIntInvalidateLast:
  ClearMeasurementContext ();
  ClearSpeculativeMeasurement (This);
  PreWrittenMeasurementPending = FALSE;
  PrePostComparisonValid       = FALSE;
  PrePostComparisonMatched     = FALSE;
  This->MeasurementDirty       = FALSE;
  ZeroMem (This->CurMeasurement, This->MeasurementSize);
  ClearLastMeasurementBuffers (This);

  return Status;
}

/**
  InitPartition
  If the Partition is erased, initialize the partition with a
  computed measurement in the partition.

  @param This    Pointer to Variable Integrity Protocol.

  @retval EFI_SUCCESS Partition is initialized.
          other       Failed to initialize Partition.

**/
STATIC
EFI_STATUS
InitPartition (
  IN NVIDIA_VAR_INT_PROTOCOL  *VarInt
  )
{
  EFI_STATUS  Status;
  UINT64      WriteOffset;

  if (VarInt->CurMeasurement[0] == FVB_ERASED_BYTE) {
    DEBUG ((DEBUG_ERROR, "Initializing Partition\n"));
    VarInt->CurMeasurement[0] = VAR_INT_V1_VALID;
    Status                    = GetWriteOffset (VarInt, &WriteOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Error Getting Offset\n",
        __FUNCTION__
        ));
      goto ExitInitPartition;
    }

    DEBUG ((DEBUG_INFO, "%a: Write Offset %lu\n", __FUNCTION__, WriteOffset));
    Status = PartitionWrite (
               VarInt,
               WriteOffset,
               VarInt->MeasurementSize,
               VarInt->CurMeasurement
               );
    LogMeasurementWrite (
      "init-valid",
      WriteOffset,
      VarInt->MeasurementSize,
      FVB_ERASED_BYTE,
      VarInt->CurMeasurement[0],
      Status
      );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Write measurement to %lu\n", WriteOffset));
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to init partition %r\n",
        __FUNCTION__,
        Status
        ));
      goto ExitInitPartition;
    }
  } else {
    Status = EFI_SUCCESS;
  }

ExitInitPartition:
  return Status;
}

/**
 * Check if every buffer byte is erased or zero.
 *
 * @param[in]  Buf             Buffer to check.
 * @param[in]  Size            Size of the buffer.
 * @param[out] FirstDataOffset  Offset of first byte that is neither erased nor zero.
 * @param[out] FirstDataValue   Value of first byte that is neither erased nor zero.
 *
 * @return TRUE  Contents of buffer are all FVB_ERASED_BYTE or 0.
 *         FALSE Otherwise.
 */
STATIC
BOOLEAN
IsBufErasedOrZero (
  IN  UINT8  *Buf,
  IN  UINTN  Size,
  OUT UINTN  *FirstDataOffset,
  OUT UINT8  *FirstDataValue
  )
{
  UINTN  Index;

  for (Index = 0; Index < Size; Index++) {
    if ((Buf[Index] != FVB_ERASED_BYTE) && (Buf[Index] != 0)) {
      if (FirstDataOffset != NULL) {
        *FirstDataOffset = Index;
      }

      if (FirstDataValue != NULL) {
        *FirstDataValue = Buf[Index];
      }

      return FALSE;
    }
  }

  return TRUE;
}

/**
  IsMeasurementPartitionErasedOrZero
  Check if the variable integrity storage region is blank.

  @param This    Pointer to Variable Integrity Protocol.

  @retval TRUE   Partition is blank, containing only erased or zero bytes.
          other  Partition contains other data.

**/
BOOLEAN
IsMeasurementPartitionErasedOrZero (
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProto,
  UINT64                     PartitionStartOffset,
  UINT64                     PartitionSize
  )
{
  UINT8       *Buf;
  BOOLEAN     IsErasedOrZero;
  EFI_STATUS  Status;
  UINT64      EndOffset;
  UINT64      PartitionOffset;
  UINTN       FirstDataOffset;
  UINT8       FirstDataValue;

  Buf = AllocateRuntimeZeroPool (SIZE_1KB * sizeof (UINT8));
  if (Buf == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create read buf\n", __FUNCTION__));
    ASSERT (FALSE);
    Status         = EFI_OUT_OF_RESOURCES;
    IsErasedOrZero = FALSE;
    goto ExitIsMeasurementPartitionErased;
  }

  EndOffset      = PartitionStartOffset + PartitionSize;
  IsErasedOrZero = TRUE;

  for (PartitionOffset = PartitionStartOffset; PartitionOffset < EndOffset; PartitionOffset += SIZE_1KB) {
    Status = NorFlashProto->Read (
                              NorFlashProto,
                              PartitionOffset,
                              SIZE_1KB,
                              Buf
                              );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: NorFlash Read Failed at 0x%lx offset %r, partition start=0x%lx size=0x%lx\n",
        __FUNCTION__,
        PartitionOffset,
        Status,
        PartitionStartOffset,
        PartitionSize
        ));
      IsErasedOrZero = FALSE;
      goto ExitIsMeasurementPartitionErased;
    }

    FirstDataOffset = 0;
    FirstDataValue  = 0;
    if (IsBufErasedOrZero (Buf, SIZE_1KB, &FirstDataOffset, &FirstDataValue) != TRUE) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Measurement partition is not blank: "
        "start=0x%lx size=0x%lx first_data_abs=0x%lx first_data_rel=0x%lx value=0x%x\n",
        __FUNCTION__,
        PartitionStartOffset,
        PartitionSize,
        PartitionOffset + FirstDataOffset,
        (PartitionOffset - PartitionStartOffset) + FirstDataOffset,
        FirstDataValue
        ));
      IsErasedOrZero = FALSE;
      goto ExitIsMeasurementPartitionErased;
    }
  }

ExitIsMeasurementPartitionErased:
  if (Buf != NULL) {
    FreePool (Buf);
  }

  return IsErasedOrZero;
}

/**
  VarIntValidate
  Validate the Variable Integrity measurements.
  Check if there are valid integrity measurements stored in the
  region of NOR-Flash set aside for these measurements.

  @param This   NVIDIA Var Int Protocol.

  @retval EFI_SUCCESS   Var Integrity partition is valid.
          other         Var Integrity measurements isn't valid.
**/
EFI_STATUS
EFIAPI
VarIntValidate (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  EFI_STATUS        Status;
  UINT32            NumValidRecords;
  UINTN             Index;
  UINT8             *V1Meas;
  UINT8             *V0Meas;
  MEASURE_REC_TYPE  *ReadMeas;
  MEASURE_REC_TYPE  *MatchedRecord;
  BOOLEAN           Matched;
  UINT32            PayloadSize;

  Matched       = FALSE;
  MatchedRecord = NULL;
  PayloadSize   = GetMeasurementPayloadSize (This);

  Status = EFI_SUCCESS;
  V1Meas = &This->CurMeasurement[HEADER_SZ_BYTES];
  V0Meas = &CurMeas[HEADER_SZ_BYTES];

  This->BootstrapDeferred = FALSE;
  This->MeasurementDirty  = FALSE;

  if (IsMeasurementPartitionErasedOrZero (
        This->NorFlashProtocol,
        This->PartitionByteOffset,
        This->PartitionSize
        ) == TRUE)
  {
    DEBUG ((DEBUG_ERROR, "%a: Measurement partition is empty, deferring bootstrap measurement\n", __FUNCTION__));
    This->BootstrapDeferred = TRUE;
    Status                  = EFI_SUCCESS;
    goto ExitVarIntValidate;
  }

  /* Compute the hash over the variables we're monitoring */
  Status = ComputeCurrentMeasurement (
             This,
             NULL,
             NULL,
             0,
             NULL,
             0,
             VarIntRecordVersionV1,
             This->CurMeasurement
             );
  if (EFI_ERROR (Status)) {
    goto ExitVarIntValidate;
  }

  LogMeasurementPreview ("validate-computed-v1", This->CurMeasurement, This->MeasurementSize);

  /* Get the valid measurements from the NOR-FLash */
  Status = GetLastValidMeasurements (
             This,
             LastMeasurements,
             &NumValidRecords
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Get Valid Measurements for Var Store %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitVarIntValidate;
  }

  if (NumValidRecords == 0) {
    DEBUG ((DEBUG_ERROR, "%a: No Valid Records are found\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto ExitVarIntValidate;
  }

  for (Index = 0; Index < NumValidRecords; Index++) {
    ReadMeas = LastMeasurements[Index];

    DEBUG ((DEBUG_INFO, "ReadMeas: 0x%lx\n", ReadMeas));
    PrintMeas (ReadMeas->Measurement, This->MeasurementSize);
    LogMeasurementPreview ("validate-record", ReadMeas->Measurement, This->MeasurementSize);

    /*
      Accept a matching V1 pending record during boot validation as recovery
      for reset after the variable-store FVB write but before the post-set
      callback committed the measurement state. FinalizeValidatedRecords()
      promotes only the matching record and invalidates the other live records.
    */
    if ((IsRecordV1 (ReadMeas->Measurement[0]) == TRUE) &&
        (CompareMem (V1Meas, &ReadMeas->Measurement[HEADER_SZ_BYTES], PayloadSize) == 0))
    {
      Matched       = TRUE;
      MatchedRecord = ReadMeas;
      DEBUG ((
        DEBUG_INFO,
        "%a: %u Found V1 MATCH, Measurement Valid\n",
        __FUNCTION__,
        Index
        ));
      Status = EFI_SUCCESS;
      break;
    }
  }

  if (Matched == TRUE) {
    Status = FinalizeValidatedRecords (
               This,
               NumValidRecords,
               LastMeasurements,
               MatchedRecord
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to finalize measurements %r\n",
        __FUNCTION__,
        Status
        ));
    }

    goto ExitVarIntValidate;
  }

  if (IsV0MigrationAllowed () == TRUE) {
    Status = ComputeCurrentMeasurement (
               This,
               NULL,
               NULL,
               0,
               NULL,
               0,
               VarIntRecordVersionV0,
               CurMeas
               );
    if (EFI_ERROR (Status)) {
      goto ExitVarIntValidate;
    }

    for (Index = 0; Index < NumValidRecords; Index++) {
      ReadMeas = LastMeasurements[Index];

      if ((IsRecordV0 (ReadMeas->Measurement[0]) == TRUE) &&
          (CompareMem (V0Meas, &ReadMeas->Measurement[HEADER_SZ_BYTES], PayloadSize) == 0))
      {
        Matched       = TRUE;
        MatchedRecord = ReadMeas;
        DEBUG ((
          DEBUG_INFO,
          "%a: %u Found V0 MATCH, migrating to V1\n",
          __FUNCTION__,
          Index
          ));
        Status = MigrateV0RecordToV1 (This, MatchedRecord);
        if (!EFI_ERROR (Status)) {
          Status = FinalizeValidatedRecords (
                     This,
                     NumValidRecords,
                     LastMeasurements,
                     NULL
                     );
        }

        goto ExitVarIntValidate;
      }
    }
  }

ExitVarIntValidate:
  if ((Matched != TRUE) && (This->BootstrapDeferred == FALSE)) {
    if (IsMeasurementPartitionErasedOrZero (
          This->NorFlashProtocol,
          This->PartitionByteOffset,
          This->PartitionSize
          ) == TRUE)
    {
      DEBUG ((DEBUG_ERROR, "The Variable Integrity Partition is erased\n"));
      Status = InitPartition (This);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Init Partition Failed %r\n", Status));
      }
    } else {
      /* If we're here then we couldn't find a matching measurement for
       * the Var store, flag this as a possible tamper detect.
       */
      DEBUG ((
        DEBUG_ERROR,
        "%a: FAILED TO VALIDATE: no matching measurement found in "
        "non-empty measurement partition\n",
        __FUNCTION__
        ));
      Status = EFI_DEVICE_ERROR;
    }
  }

  ZeroMem (This->CurMeasurement, This->MeasurementSize);
  if (CurMeas != NULL) {
    ZeroMem (CurMeas, This->MeasurementSize);
  }

  return Status;
}

/**
  VarIntInit
  Initialize the VarInt Protocol and register a callback for the
  Smm Variable protocol.

  @param PartitionStartOffset  Starting offset for measurements on the device.
  @param PartitionSize         Size set aside for the measurements.
  @param NorFlashProto         NorFlash Protocol.
  @param NorFlashAttributes    Attributes for the NorFlash device.

  @retval EFI_SUCCESS          VarInt protocol installed.
          other                failed to install the protocol.

**/
EFI_STATUS
EFIAPI
VarIntInit (
  IN UINTN                      PartitionStartOffset,
  IN UINTN                      PartitionSize,
  IN NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProto,
  IN NOR_FLASH_ATTRIBUTES       *NorFlashAttributes
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  VarIntHandle;
  UINT32      MeasSize;
  UINTN       Index;

  VarIntProto = AllocateRuntimeZeroPool (sizeof (NVIDIA_VAR_INT_PROTOCOL));
  if (VarIntProto == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a %d:Not enough resources to alloc VarIntProto\n",
      __FUNCTION__,
      __LINE__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitVarIntInit;
  }

  Status = GetMeasurementSize (&MeasSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get MeasurementSize %r\n", __FUNCTION__, Status));
    goto ExitVarIntInit;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: Partition Start 0x%lx %lu Size %u\n",
    __FUNCTION__,
    PartitionStartOffset,
    PartitionStartOffset,
    PartitionSize
    ));
  VarIntProto->PartitionByteOffset      = PartitionStartOffset;
  VarIntProto->PartitionSize            = PartitionSize;
  VarIntProto->BlockSize                = NorFlashAttributes->BlockSize;
  VarIntProto->WriteNewMeasurement      = VarIntWriteMeasurement;
  VarIntProto->InvalidateLast           = VarIntInvalidateLast;
  VarIntProto->ComputeNewMeasurement    = VarIntComputeMeasurement;
  VarIntProto->Validate                 = VarIntValidate;
  VarIntProto->MarkDirty                = VarIntMarkDirty;
  VarIntProto->FlushDeferredMeasurement = VarIntFlushDeferredMeasurement;
  VarIntProto->IsDeferred               = VarIntIsDeferred;
  VarIntProto->NorFlashProtocol         = NorFlashProto;
  VarIntProto->MeasurementSize          = MeasSize + HEADER_SZ_BYTES;
  VarIntProto->CurMeasurement           = AllocateAlignedPages (EFI_SIZE_TO_PAGES (VarIntProto->MeasurementSize), EFI_PAGE_SIZE);
  if (VarIntProto->CurMeasurement == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "%a: Not enough resources to allocate Measurement Buffer - %r", __FUNCTION__, Status);
  }

  ZeroMem (VarIntProto->CurMeasurement, VarIntProto->MeasurementSize);

  VarIntProto->PartitionData = AllocateRuntimeZeroPool (VarIntProto->PartitionSize);
  if (VarIntProto->PartitionData == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitVarIntInit, "%a: Not enough resources to allocate Partition Data - %r", __FUNCTION__, Status);
  }

  Status = NorFlashProto->Read (
                            NorFlashProto,
                            VarIntProto->PartitionByteOffset,
                            VarIntProto->PartitionSize,
                            VarIntProto->PartitionData
                            );
  if (EFI_ERROR (Status)) {
    NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitVarIntInit, "Failed to Read from Reserved Partition - %r", Status);
  }

  VarIntHandle = NULL;
  Status       = gMmst->MmInstallProtocolInterface (
                          &VarIntHandle,
                          &gNVIDIAVarIntGuid,
                          EFI_NATIVE_INTERFACE,
                          VarIntProto
                          );
  NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitVarIntInit, "Failed to install VarInt Protocol - %r", Status);

  /* Allocate these resources one-time */
  for (Index = 0; Index < MAX_VALID_RECORDS; Index++) {
    LastMeasurements[Index] = AllocateRuntimeZeroPool (sizeof (MEASURE_REC_TYPE));
    if (LastMeasurements[Index] == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitVarIntInit, "%a: Failed to Allocate Memory - %r", __FUNCTION__, Status);
    }

    LastMeasurements[Index]->Measurement = AllocateRuntimeZeroPool (
                                             VarIntProto->MeasurementSize * sizeof (UINT8)
                                             );
    if (LastMeasurements[Index]->Measurement == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitVarIntInit, "%a: Failed to Allocate Measurements Buffer - %r", __FUNCTION__, Status);
    }

    LastMeasurements[Index]->ByteOffset = 0;
  }

  CurMeas = AllocateRuntimeZeroPool (VarIntProto->MeasurementSize);
  if (CurMeas == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "%a: Not Enough Resources to allocate Buffer - %r", __FUNCTION__, Status);
  }

  SpeculativeMeasurement = AllocateRuntimeZeroPool (VarIntProto->MeasurementSize);
  if (SpeculativeMeasurement == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "%a: Not Enough Resources to allocate Speculative Buffer - %r", __FUNCTION__, Status);
  }

  if (!IsOpteePresent ()) {
    Status = FfaInit (VarIntProto);
    NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "Failed to Initialize FFA - %r", Status);
  }

ExitVarIntInit:
  return Status;
}
