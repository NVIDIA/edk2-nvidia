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

#define HEADER_SZ_BYTES              (1)
#define MAX_VALID_RECORDS            (2)
#define T234_V0_EXTRA_PAYLOAD_BYTES  (1)

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

typedef enum {
  VarIntRecordFilterAny,
  VarIntRecordFilterV0,
  VarIntRecordFilterV1
} VAR_INT_RECORD_FILTER;

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
STATIC BOOLEAN                      OpteePresent                 = FALSE;
STATIC VAR_INT_MEASUREMENT_CONTEXT  LastMeasurementContext;

STATIC
UINT32
GetMeasurementPayloadSize (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  );

STATIC
UINT32
GetV0MeasurementPayloadSize (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  );

STATIC
UINT32
GetV0MeasurementRecordSize (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  );

STATIC
UINT32
GetMaxMeasurementRecordSize (
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
EFI_STATUS
GetLastValidMeasurementsByLayout (
  IN  NVIDIA_VAR_INT_PROTOCOL  *VarInt,
  OUT MEASURE_REC_TYPE         **Records,
  OUT UINT32                   *NumRecords,
  IN  UINT32                   RecordSize,
  IN  VAR_INT_RECORD_FILTER    RecordFilter,
  IN  CONST UINT8              *ExpectedPayload OPTIONAL,
  OUT BOOLEAN                  *RecordLimitExceeded OPTIONAL
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
    DEBUG_INFO,
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
    DEBUG_INFO,
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
    DEBUG_INFO,
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
    ZeroMem (SpeculativeMeasurement, GetMaxMeasurementRecordSize (This));
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
      ZeroMem (LastMeasurements[Index]->Measurement, GetMaxMeasurementRecordSize (This));
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

  Status = FfaSendShareCommand (
             TotalLength,
             TotalLength,
             FfaTxBufferAddr,
             EFI_SIZE_TO_PAGES (GetMaxMeasurementRecordSize (VarInt)),
             &FfaHandle
             );
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
  UINT8         *SharedMeas;

  //
  // The FFA handle is established once for CurMeasurement.  Callers that
  // use a scratch measurement buffer must marshal their payload through
  // that shared page; the direct request carries only the handle, not Meas.
  //
  if ((Meas == NULL) || (VarIntProto == NULL) ||
      (VarIntProto->CurMeasurement == NULL) ||
      (VarIntProto->MeasurementSize <= HEADER_SZ_BYTES) ||
      (Size != GetMeasurementPayloadSize (VarIntProto)))
  {
    return EFI_INVALID_PARAMETER;
  }

  SharedMeas = &VarIntProto->CurMeasurement[HEADER_SZ_BYTES];
  if (Meas != SharedMeas) {
    CopyMem (SharedMeas, Meas, Size);
  }

  Status = FfaGetOpteeVmId (&OpteeVmId);
  if (EFI_ERROR (Status)) {
    ZeroMem (
      VarIntProto->CurMeasurement,
      VarIntProto->MeasurementSize
      );
    return Status;
  }

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

    PrintMeas (SharedMeas, Size);
    if (Meas != SharedMeas) {
      CopyMem (Meas, SharedMeas, Size);
    }
  } else {
    ZeroMem (
      VarIntProto->CurMeasurement,
      VarIntProto->MeasurementSize
      );
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

  if (OpteePresent) {
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
  IN  UINT64                   Offset,
  IN  UINT32                   Size,
  OUT UINT8                    *Buffer
  )
{
  EFI_STATUS  Status;
  UINT64      PartitionStart;
  UINT64      PartitionEnd;
  UINTN       BufferOffset;

  PartitionStart = This->PartitionByteOffset;
  PartitionEnd   = PartitionStart + This->PartitionSize;

  if ((Offset < PartitionStart) || (Offset > PartitionEnd) ||
      (Size > (PartitionEnd - Offset)))
  {
    Status = EFI_INVALID_PARAMETER;
    goto ExitPartitionRead;
  }

  BufferOffset = (UINTN)(Offset - PartitionStart);
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
  IN  UINT64                   Offset,
  IN  UINT32                   Size,
  IN  UINT8                    *Buffer
  )
{
  EFI_STATUS  Status;
  UINT64      PartitionStart;
  UINT64      PartitionEnd;
  UINTN       BufferOffset;

  PartitionStart = This->PartitionByteOffset;
  PartitionEnd   = PartitionStart + This->PartitionSize;

  if ((Offset < PartitionStart) || (Offset > PartitionEnd) ||
      (Size > (PartitionEnd - Offset)))
  {
    Status = EFI_INVALID_PARAMETER;
    goto ExitPartitionWrite;
  }

  BufferOffset = (UINTN)(Offset - PartitionStart);

  /* Update the Partition.*/
  Status = This->NorFlashProtocol->Write (
                                     This->NorFlashProtocol,
                                     (UINT32)Offset,
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
  UINT64      PartitionStart;
  UINT64      PartitionEnd;
  UINTN       BufferOffset;
  UINT64      Offset;
  UINT64      Size;

  PartitionStart = This->PartitionByteOffset;
  PartitionEnd   = PartitionStart + This->PartitionSize;
  Offset         = (UINT64)Lba * This->BlockSize;
  Size           = (UINT64)NumBlocks * This->BlockSize;

  if ((Offset < PartitionStart) || (Offset > PartitionEnd) ||
      (Size > (PartitionEnd - Offset)))
  {
    Status = EFI_INVALID_PARAMETER;
    goto ExitPartitionErase;
  }

  BufferOffset = (UINTN)(Offset - PartitionStart);

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
  SetMem ((This->PartitionData + BufferOffset), (UINTN)Size, FVB_ERASED_BYTE);

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
UINT32
GetV0MeasurementPayloadSize (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  UINT32  PayloadSize;

  PayloadSize = GetMeasurementPayloadSize (This);
  if (OpteePresent) {
    PayloadSize += T234_V0_EXTRA_PAYLOAD_BYTES;
  }

  return PayloadSize;
}

STATIC
UINT32
GetV0MeasurementRecordSize (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  return HEADER_SZ_BYTES + GetV0MeasurementPayloadSize (This);
}

STATIC
UINT32
GetMaxMeasurementRecordSize (
  IN NVIDIA_VAR_INT_PROTOCOL  *This
  )
{
  return MAX (This->MeasurementSize, GetV0MeasurementRecordSize (This));
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
RecordWindowOfSizeFitsInBlock (
  IN NVIDIA_VAR_INT_PROTOCOL  *This,
  IN UINT64                   Offset,
  IN UINT32                   RecordSize
  )
{
  UINT64  BlockStart;
  UINT64  BlockEnd;

  BlockStart = (Offset / This->BlockSize) * This->BlockSize;
  BlockEnd   = BlockStart + This->BlockSize;

  return (BOOLEAN)((Offset + RecordSize) <= BlockEnd);
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
  BOOLEAN  MigrationAllowed;

  MigrationAllowed = FeaturePcdGet (PcdAllowVarIntV0Migration);

  DEBUG ((
    DEBUG_INFO,
    "VarIntMigrationPolicy: pcd_allow_v0_migration=%u\n",
    MigrationAllowed
    ));

  return MigrationAllowed;
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
  UINT32      RecordSize;
  UINT8       *Meas;

  if (Version == VarIntRecordVersionV0) {
    PayloadSize = GetV0MeasurementPayloadSize (This);
    RecordSize  = GetV0MeasurementRecordSize (This);
  } else {
    PayloadSize = GetMeasurementPayloadSize (This);
    RecordSize  = This->MeasurementSize;
  }

  Meas = &Measurement[HEADER_SZ_BYTES];

  ZeroMem (Measurement, RecordSize);

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

  DEBUG ((
    DEBUG_INFO,
    "VarIntSignLayout: version=%u optee_present=%u record_size=%u "
    "payload_size=%u send_size=%u measurement=%p payload=%p\n",
    Version,
    OpteePresent,
    RecordSize,
    PayloadSize,
    PayloadSize,
    Measurement,
    Meas
    ));

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
BOOLEAN
IsBufferErased (
  IN CONST UINT8  *Buffer,
  IN UINTN        Size
  )
{
  UINTN  Index;

  if (Buffer == NULL) {
    return FALSE;
  }

  for (Index = 0; Index < Size; Index++) {
    if (Buffer[Index] != FVB_ERASED_BYTE) {
      return FALSE;
    }
  }

  return TRUE;
}

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

        if (IsBufferErased (ReadBuf, This->MeasurementSize)) {
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
  return GetLastValidMeasurementsByLayout (
           VarInt,
           Records,
           NumRecords,
           VarInt->MeasurementSize,
           VarIntRecordFilterV1,
           NULL,
           NULL
           );
}

STATIC
EFI_STATUS
GetLastValidMeasurementsByLayout (
  IN  NVIDIA_VAR_INT_PROTOCOL  *VarInt,
  OUT MEASURE_REC_TYPE         **Records,
  OUT UINT32                   *NumRecords,
  IN  UINT32                   RecordSize,
  IN  VAR_INT_RECORD_FILTER    RecordFilter,
  IN  CONST UINT8              *ExpectedPayload OPTIONAL,
  OUT BOOLEAN                  *RecordLimitExceeded OPTIONAL
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
  BOOLEAN                    LimitExceeded;

  ReadBuf       = NULL;
  Status        = EFI_SUCCESS;
  LimitExceeded = FALSE;
  NorFlash      = VarInt->NorFlashProtocol;
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
  if (RecordLimitExceeded != NULL) {
    *RecordLimitExceeded = FALSE;
  }

  while (CurOffset < EndOffset) {
    while (BlockOffset < BlockEnd) {
      if (RecordWindowOfSizeFitsInBlock (VarInt, BlockOffset, RecordSize) == TRUE) {
        Status = PartitionRead (
                   VarInt,
                   BlockOffset,
                   RecordSize,
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

        if ((IsRecordLive (ReadBuf[0]) == TRUE) &&
            ((RecordFilter == VarIntRecordFilterAny) ||
             ((RecordFilter == VarIntRecordFilterV0) && IsRecordV0 (ReadBuf[0])) ||
             ((RecordFilter == VarIntRecordFilterV1) && IsRecordV1 (ReadBuf[0]))))
        {
          NumValidRecords++;
          if (NumValidRecords > MAX_VALID_RECORDS) {
            if (LimitExceeded == FALSE) {
              DEBUG ((
                DEBUG_ERROR,
                "%a: More than %d live-record candidates found for stride %u\n",
                __FUNCTION__,
                MAX_VALID_RECORDS,
                RecordSize
                ));
            }

            LimitExceeded = TRUE;
            if (RecordLimitExceeded != NULL) {
              *RecordLimitExceeded = TRUE;
            }

            if ((ExpectedPayload != NULL) &&
                (CompareMem (
                   ExpectedPayload,
                   &ReadBuf[HEADER_SZ_BYTES],
                   RecordSize - HEADER_SZ_BYTES
                   ) == 0))
            {
              CopyMem (Records[MAX_VALID_RECORDS - 1]->Measurement, ReadBuf, RecordSize);
              Records[MAX_VALID_RECORDS - 1]->ByteOffset = BlockOffset;
            }
          } else {
            DEBUG ((
              DEBUG_INFO,
              "VarIntRecordScan: index=%u offset_abs=0x%lx offset_rel=0x%lx "
              "stride=%u block_size=0x%x header=0x%x(%a)\n",
              (UINT32)(NumValidRecords - 1),
              BlockOffset,
              BlockOffset - StartOffset,
              RecordSize,
              VarInt->BlockSize,
              ReadBuf[0],
              MeasurementHeaderName (ReadBuf[0])
              ));
            CopyMem (Records[(NumValidRecords - 1)]->Measurement, ReadBuf, RecordSize);
            *NumRecords                               += 1;
            Records[(NumValidRecords - 1)]->ByteOffset = BlockOffset;
          }
        }
      }

      BlockOffset += RecordSize;
    }

    CurOffset  += VarInt->BlockSize;
    BlockEnd   += VarInt->BlockSize;
    BlockOffset = CurOffset;
  }

  if (LimitExceeded == TRUE) {
    Status = EFI_DEVICE_ERROR;
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
HasLiveV0MeasurementByLayout (
  IN  NVIDIA_VAR_INT_PROTOCOL  *This,
  OUT BOOLEAN                  *Found
  )
{
  UINT32  RecordSize;
  UINT64  BlockOffset;
  UINT64  BlockEnd;
  UINT64  EndOffset;
  UINTN   PartitionOffset;
  UINT8   *PartitionData;

  if ((This == NULL) || (Found == NULL) || (This->BlockSize == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  *Found        = FALSE;
  RecordSize    = GetV0MeasurementRecordSize (This);
  EndOffset     = This->PartitionByteOffset + This->PartitionSize;
  PartitionData = (UINT8 *)This->PartitionData;

  for (BlockOffset = This->PartitionByteOffset;
       BlockOffset < EndOffset;
       BlockOffset += This->BlockSize)
  {
    BlockEnd = BlockOffset + This->BlockSize;
    while (RecordWindowOfSizeFitsInBlock (This, BlockOffset, RecordSize) &&
           (BlockOffset < BlockEnd))
    {
      PartitionOffset = BlockOffset - This->PartitionByteOffset;
      if ((IsRecordV0 (PartitionData[PartitionOffset]) == TRUE) &&
          (IsRecordValid (PartitionData[PartitionOffset]) == TRUE))
      {
        *Found = TRUE;
        return EFI_SUCCESS;
      }

      BlockOffset += RecordSize;
    }

    BlockOffset = BlockEnd - This->BlockSize;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
HasMatchingV0MeasurementByLayout (
  IN  NVIDIA_VAR_INT_PROTOCOL  *This,
  IN  CONST UINT8              *V0Measurement,
  OUT BOOLEAN                  *Found,
  OUT UINT64                   *MatchOffset
  )
{
  UINT32  RecordSize;
  UINT64  BlockOffset;
  UINT64  BlockEnd;
  UINT64  EndOffset;
  UINTN   PartitionOffset;
  UINT8   *PartitionData;

  if ((This == NULL) || (V0Measurement == NULL) || (Found == NULL) ||
      (MatchOffset == NULL) ||
      (This->BlockSize == 0))
  {
    return EFI_INVALID_PARAMETER;
  }

  *Found        = FALSE;
  *MatchOffset  = MAX_UINT64;
  RecordSize    = GetV0MeasurementRecordSize (This);
  EndOffset     = This->PartitionByteOffset + This->PartitionSize;
  PartitionData = (UINT8 *)This->PartitionData;

  for (BlockOffset = This->PartitionByteOffset;
       BlockOffset < EndOffset;
       BlockOffset += This->BlockSize)
  {
    BlockEnd = BlockOffset + This->BlockSize;
    while (RecordWindowOfSizeFitsInBlock (This, BlockOffset, RecordSize) &&
           (BlockOffset < BlockEnd))
    {
      PartitionOffset = BlockOffset - This->PartitionByteOffset;
      if ((IsRecordV0 (PartitionData[PartitionOffset]) == TRUE) &&
          (IsRecordValid (PartitionData[PartitionOffset]) == TRUE) &&
          (CompareMem (
             &PartitionData[PartitionOffset + HEADER_SZ_BYTES],
             &V0Measurement[HEADER_SZ_BYTES],
             GetV0MeasurementPayloadSize (This)
             ) == 0))
      {
        *Found       = TRUE;
        *MatchOffset = BlockOffset;
        return EFI_SUCCESS;
      }

      BlockOffset += RecordSize;
    }

    BlockOffset = BlockEnd - This->BlockSize;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetMigrationWriteOffset (
  IN  NVIDIA_VAR_INT_PROTOCOL  *This,
  IN  MEASURE_REC_TYPE         *SourceRecord,
  IN  UINT8                    *V1Measurement,
  IN  UINT64                   ProtectedV0Offset,
  OUT UINT64                   *Offset
  )
{
  EFI_STATUS  Status;
  UINT32      NumBlocks;
  UINT32      MatchedBlock;
  UINT32      CandidateBlock;
  UINT32      BlocksChecked;
  UINT32      ByteIndex;
  UINT32      RecordOffset;
  UINT32      V0RecordOffset;
  UINT32      V0RecordSize;
  UINT32      ProtectedV0Block;
  UINT8       *BlockData;
  BOOLEAN     BlockErased;

  if ((This == NULL) || (SourceRecord == NULL) || (V1Measurement == NULL) ||
      (Offset == NULL) || (This->BlockSize == 0) ||
      ((This->PartitionSize / This->BlockSize) < 2))
  {
    return EFI_INVALID_PARAMETER;
  }

  if ((SourceRecord->ByteOffset < This->PartitionByteOffset) ||
      (SourceRecord->ByteOffset >= (This->PartitionByteOffset + This->PartitionSize)) ||
      ((ProtectedV0Offset != MAX_UINT64) &&
       ((ProtectedV0Offset < This->PartitionByteOffset) ||
        (ProtectedV0Offset >= (This->PartitionByteOffset + This->PartitionSize)))))
  {
    return EFI_INVALID_PARAMETER;
  }

  NumBlocks        = This->PartitionSize / This->BlockSize;
  MatchedBlock     = (SourceRecord->ByteOffset - This->PartitionByteOffset) / This->BlockSize;
  ProtectedV0Block = MAX_UINT32;
  V0RecordSize     = GetV0MeasurementRecordSize (This);
  if (ProtectedV0Offset != MAX_UINT64) {
    ProtectedV0Block = (ProtectedV0Offset - This->PartitionByteOffset) / This->BlockSize;
  }

  if (MatchedBlock >= NumBlocks) {
    return EFI_INVALID_PARAMETER;
  }

  for (BlocksChecked = 1; BlocksChecked < NumBlocks; BlocksChecked++) {
    CandidateBlock = (MatchedBlock + BlocksChecked) % NumBlocks;
    if (CandidateBlock == ProtectedV0Block) {
      continue;
    }

    BlockData   = This->PartitionData + (CandidateBlock * This->BlockSize);
    BlockErased = TRUE;

    for (ByteIndex = 0; ByteIndex < This->BlockSize; ByteIndex++) {
      if (BlockData[ByteIndex] != FVB_ERASED_BYTE) {
        BlockErased = FALSE;
        break;
      }
    }

    if (BlockErased) {
      *Offset = This->PartitionByteOffset + (CandidateBlock * This->BlockSize);
      return EFI_SUCCESS;
    }
  }

  //
  // Preserve the matched V0 block until its V1 replacement is durable.  If
  // the circular log has touched every other block, find a block without a
  // live V1 record and erase it to create a safe migration destination.
  //
  for (BlocksChecked = 1; BlocksChecked < NumBlocks; BlocksChecked++) {
    CandidateBlock = (MatchedBlock + BlocksChecked) % NumBlocks;
    if (CandidateBlock == ProtectedV0Block) {
      continue;
    }

    BlockData = This->PartitionData + (CandidateBlock * This->BlockSize);
    for (RecordOffset = 0;
         (RecordOffset + This->MeasurementSize) <= This->BlockSize;
         RecordOffset += This->MeasurementSize)
    {
      if ((IsRecordLive (BlockData[RecordOffset]) == TRUE) &&
          (IsRecordV1 (BlockData[RecordOffset]) == TRUE))
      {
        //
        // A V1-stride boundary can fall inside a legacy V0 payload.  Do not
        // treat that payload byte as a V1 header when a live, correctly
        // aligned V0 record covers it.
        //
        V0RecordOffset = (RecordOffset / V0RecordSize) * V0RecordSize;
        if ((RecordOffset != V0RecordOffset) &&
            ((V0RecordOffset + V0RecordSize) <= This->BlockSize) &&
            (IsRecordLive (BlockData[V0RecordOffset]) == TRUE) &&
            (IsRecordV0 (BlockData[V0RecordOffset]) == TRUE))
        {
          continue;
        }

        DEBUG ((
          DEBUG_INFO,
          "%a: Skipping block %u containing a live V1 record\n",
          __FUNCTION__,
          CandidateBlock
          ));
        break;
      }
    }

    if ((RecordOffset + This->MeasurementSize) <= This->BlockSize) {
      continue;
    }

    *Offset = This->PartitionByteOffset + (CandidateBlock * This->BlockSize);
    DEBUG ((
      DEBUG_INFO,
      "%a: Erasing reusable block at %lu for V0 migration\n",
      __FUNCTION__,
      *Offset
      ));
    Status = PartitionErase (
               This,
               (*Offset / This->BlockSize),
               1
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to erase migration block at %lu: %r\n",
        __FUNCTION__,
        *Offset,
        Status
        ));
    }

    return Status;
  }

  DEBUG ((DEBUG_ERROR, "%a: No reusable migration block without a live V1 record\n", __FUNCTION__));
  return EFI_OUT_OF_RESOURCES;
}

STATIC
EFI_STATUS
WriteV1ToMigrationBlock (
  IN  NVIDIA_VAR_INT_PROTOCOL  *This,
  IN  MEASURE_REC_TYPE         *SourceRecord,
  IN  UINT8                    *V1Measurement,
  IN  UINT64                   ProtectedV0Offset,
  OUT UINT64                   *V1RecordOffset
  )
{
  EFI_STATUS  Status;
  UINT64      CurOffset;
  UINT8       OldHeader;

  if ((This == NULL) || (SourceRecord == NULL) || (V1Measurement == NULL) ||
      (V1RecordOffset == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetMigrationWriteOffset (
             This,
             SourceRecord,
             V1Measurement,
             ProtectedV0Offset,
             &CurOffset
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error getting migration offset: %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: Writing current V1 measurement at %lu\n", __FUNCTION__, CurOffset));
  OldHeader        = V1Measurement[0];
  V1Measurement[0] = VAR_INT_V1_PENDING;
  Status           = PartitionWrite (
                       This,
                       CurOffset,
                       This->MeasurementSize,
                       V1Measurement
                       );
  LogMeasurementWrite (
    "migrate-v1-pending",
    CurOffset,
    This->MeasurementSize,
    FVB_ERASED_BYTE,
    V1Measurement[0],
    Status
    );
  if (EFI_ERROR (Status)) {
    V1Measurement[0] = OldHeader;
    DEBUG ((DEBUG_ERROR, "%a: Failed to write V1 measurement at %lu: %r\n", __FUNCTION__, CurOffset, Status));
    return Status;
  }

  OldHeader        = V1Measurement[0];
  V1Measurement[0] = VAR_INT_V1_VALID;
  Status           = PartitionWrite (
                       This,
                       CurOffset,
                       1,
                       &V1Measurement[0]
                       );
  LogMeasurementWrite (
    "migrate-v1-valid",
    CurOffset,
    1,
    OldHeader,
    V1Measurement[0],
    Status
    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to validate V1 measurement at %lu: %r\n", __FUNCTION__, CurOffset, Status));
    return Status;
  }

  *V1RecordOffset = CurOffset;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EraseLegacyMigrationBlocks (
  IN NVIDIA_VAR_INT_PROTOCOL  *This,
  IN UINT64                   V1RecordOffset,
  IN UINT64                   V0RecordOffset
  )
{
  EFI_STATUS  Status;
  UINT32      FirstLba;
  UINT32      DestinationLba;
  UINT32      SourceLba;
  UINT32      NumBlocks;
  UINT32      BlockIndex;
  UINT32      Lba;

  if ((This == NULL) || (This->BlockSize == 0) ||
      (V1RecordOffset < This->PartitionByteOffset) ||
      (V1RecordOffset >= (This->PartitionByteOffset + This->PartitionSize)) ||
      ((V0RecordOffset != MAX_UINT64) &&
       ((V0RecordOffset < This->PartitionByteOffset) ||
        (V0RecordOffset >= (This->PartitionByteOffset + This->PartitionSize)))))
  {
    return EFI_INVALID_PARAMETER;
  }

  FirstLba       = This->PartitionByteOffset / This->BlockSize;
  DestinationLba = V1RecordOffset / This->BlockSize;
  if (V0RecordOffset == MAX_UINT64) {
    SourceLba = MAX_UINT32;
  } else {
    SourceLba = (UINT32)(V0RecordOffset / This->BlockSize);
  }

  NumBlocks = This->PartitionSize / This->BlockSize;

  DEBUG ((
    DEBUG_INFO,
    "VarIntMigrationCleanup: v0_offset=0x%lx v1_offset=0x%lx first_lba=%u source_lba=%u destination_lba=%u blocks=%u\n",
    V0RecordOffset,
    V1RecordOffset,
    FirstLba,
    SourceLba,
    DestinationLba,
    NumBlocks
    ));

  //
  // Keep the matching V0 source intact until every other legacy block has
  // been erased.  The source remains the last known-good legacy record if an
  // earlier erase fails, while the replacement V1 is already durable.
  //
  for (BlockIndex = 0; BlockIndex < NumBlocks; BlockIndex++) {
    Lba = FirstLba + BlockIndex;
    if ((Lba == DestinationLba) || (Lba == SourceLba)) {
      continue;
    }

    Status = PartitionErase (This, Lba, 1);
    DEBUG ((
      DEBUG_INFO,
      "VarIntMigrationErase: lba=%u offset=0x%lx source_last=0 status=%r\n",
      Lba,
      (UINT64)Lba * This->BlockSize,
      Status
      ));
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to erase legacy migration block %u: %r\n",
        __FUNCTION__,
        Lba,
        Status
        ));
      return Status;
    }
  }

  if ((SourceLba != MAX_UINT32) && (SourceLba != DestinationLba)) {
    Status = PartitionErase (This, SourceLba, 1);
    DEBUG ((
      DEBUG_INFO,
      "VarIntMigrationErase: lba=%u offset=0x%lx source_last=1 status=%r\n",
      SourceLba,
      (UINT64)SourceLba * This->BlockSize,
      Status
      ));
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to erase legacy migration source block %u: %r\n",
        __FUNCTION__,
        SourceLba,
        Status
        ));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

STATIC
BOOLEAN
IsOnlyLiveV1RecordInBlock (
  IN NVIDIA_VAR_INT_PROTOCOL  *This,
  IN MEASURE_REC_TYPE         *Record
  )
{
  UINT64  BlockIndex;
  UINT64  BlockOffset;
  UINT64  RecordOffset;
  UINT64  SelectedRecordOffset;
  UINT32  V0RecordSize;
  UINT8   *BlockData;

  if ((This == NULL) || (Record == NULL) || (This->BlockSize == 0) ||
      (This->MeasurementSize == 0) ||
      (Record->ByteOffset < This->PartitionByteOffset) ||
      (Record->ByteOffset >= (This->PartitionByteOffset + This->PartitionSize)))
  {
    return FALSE;
  }

  BlockIndex           = (Record->ByteOffset - This->PartitionByteOffset) / This->BlockSize;
  BlockOffset          = This->PartitionByteOffset + (BlockIndex * This->BlockSize);
  BlockData            = This->PartitionData + (BlockIndex * This->BlockSize);
  SelectedRecordOffset = Record->ByteOffset - BlockOffset;

  for (RecordOffset = 0;
       (RecordOffset + This->MeasurementSize) <= This->BlockSize;
       RecordOffset += This->MeasurementSize)
  {
    if ((IsRecordLive (BlockData[RecordOffset]) == TRUE) &&
        (IsRecordV1 (BlockData[RecordOffset]) == TRUE) &&
        ((BlockOffset + RecordOffset) != Record->ByteOffset))
    {
      return FALSE;
    }
  }

  //
  // A matching V1 replacement can share its block with a retained V0 record
  // after an interrupted migration.  Such a block is not clean: preserving
  // it would retain the V0 indefinitely.  Force relocation of the V1 so the
  // mixed source block can be erased.
  //
  V0RecordSize = GetV0MeasurementRecordSize (This);
  for (RecordOffset = 0;
       (RecordOffset + V0RecordSize) <= This->BlockSize;
       RecordOffset += V0RecordSize)
  {
    //
    // V0 and V1 records use different strides.  A V0-stride probe can land
    // inside the selected V1 record, where an authenticated payload byte may
    // resemble a live V0 header.  Bytes owned by the selected V1 record are
    // payload, not independent V0 record headers.
    //
    if ((RecordOffset >= SelectedRecordOffset) &&
        ((RecordOffset - SelectedRecordOffset) < This->MeasurementSize))
    {
      continue;
    }

    if ((IsRecordLive (BlockData[RecordOffset]) == TRUE) &&
        (IsRecordV0 (BlockData[RecordOffset]) == TRUE))
    {
      return FALSE;
    }
  }

  return TRUE;
}

STATIC
EFI_STATUS
MigrateV0RecordToV1 (
  IN  NVIDIA_VAR_INT_PROTOCOL  *This,
  IN  MEASURE_REC_TYPE         *V0Record,
  IN  UINT8                    *V1Measurement,
  OUT UINT64                   *V1RecordOffset
  )
{
  EFI_STATUS  Status;

  if ((This == NULL) || (V0Record == NULL) || (V1Measurement == NULL) ||
      (V1RecordOffset == NULL) ||
      (IsRecordV0 (V0Record->Measurement[0]) == FALSE))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = WriteV1ToMigrationBlock (
             This,
             V0Record,
             V1Measurement,
             V0Record->ByteOffset,
             V1RecordOffset
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // The replacement is durable.  Keep the source V0 live until legacy block
  // cleanup erases that source block last.
  //
  return EFI_SUCCESS;
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
  UINT8             *V0ComputedMeasurement;
  UINT8             *SavedV1Measurement;
  MEASURE_REC_TYPE  *ReadMeas;
  MEASURE_REC_TYPE  *MatchedRecord;
  BOOLEAN           Matched;
  BOOLEAN           MatchedRecordBlockClean;
  BOOLEAN           MigrationBlocked;
  BOOLEAN           V1RecordLimitExceeded;
  BOOLEAN           V0RecordPresent;
  BOOLEAN           V0MigrationPending;
  UINT32            PayloadSize;
  UINT32            V0PayloadSize;
  UINT32            V0RecordSize;
  UINT64            MigrationOffset;
  UINT64            MatchingV0Offset;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((This->CurMeasurement == NULL) || (CurMeas == NULL)) {
    return EFI_NOT_READY;
  }

  Matched                 = FALSE;
  MatchedRecord           = NULL;
  MatchedRecordBlockClean = FALSE;
  MigrationBlocked        = FALSE;
  V1RecordLimitExceeded   = FALSE;
  V0RecordPresent         = FALSE;
  V0MigrationPending      = FALSE;
  PayloadSize             = GetMeasurementPayloadSize (This);
  V0PayloadSize           = GetV0MeasurementPayloadSize (This);
  V0RecordSize            = GetV0MeasurementRecordSize (This);
  MigrationOffset         = 0;
  MatchingV0Offset        = MAX_UINT64;

  Status                = EFI_SUCCESS;
  V1Meas                = &This->CurMeasurement[HEADER_SZ_BYTES];
  V0Meas                = NULL;
  V0ComputedMeasurement = NULL;
  SavedV1Measurement    = NULL;

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
  Status = GetLastValidMeasurementsByLayout (
             This,
             LastMeasurements,
             &NumValidRecords,
             This->MeasurementSize,
             VarIntRecordFilterV1,
             V1Meas,
             &V1RecordLimitExceeded
             );
  if (EFI_ERROR (Status)) {
    if ((V1RecordLimitExceeded == TRUE) &&
        (IsV0MigrationAllowed () == TRUE))
    {
      DEBUG ((
        DEBUG_INFO,
        "%a: V1 scan exceeded the live-record limit; checking retained candidates and legacy V0 layout\n",
        __FUNCTION__
        ));
      Status = EFI_SUCCESS;
    } else {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to Get Valid Measurements for Var Store %r\n",
        __FUNCTION__,
        Status
        ));
      goto ExitVarIntValidate;
    }
  }

  if (NumValidRecords == 0) {
    DEBUG ((DEBUG_INFO, "%a: No V1 records found; checking legacy V0 layout\n", __FUNCTION__));
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
      Matched = TRUE;
      if ((MatchedRecord == NULL) ||
          (IsOnlyLiveV1RecordInBlock (This, ReadMeas) == TRUE))
      {
        MatchedRecord           = ReadMeas;
        MatchedRecordBlockClean = IsOnlyLiveV1RecordInBlock (This, ReadMeas);
      }

      DEBUG ((
        DEBUG_INFO,
        "%a: %u Found V1 MATCH, Measurement Valid\n",
        __FUNCTION__,
        Index
        ));
      Status = EFI_SUCCESS;
      if (MatchedRecordBlockClean == TRUE) {
        break;
      }
    }
  }

  if (Matched == TRUE) {
    if (IsV0MigrationAllowed () == TRUE) {
      Status = HasLiveV0MeasurementByLayout (This, &V0RecordPresent);
      if (EFI_ERROR (Status)) {
        goto ExitVarIntValidate;
      }

      if (V0RecordPresent == TRUE) {
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

        Status = HasMatchingV0MeasurementByLayout (
                   This,
                   CurMeas,
                   &V0MigrationPending,
                   &MatchingV0Offset
                   );
        if (EFI_ERROR (Status)) {
          goto ExitVarIntValidate;
        }
      }
    }

    if ((V1RecordLimitExceeded == TRUE) || (V0MigrationPending == TRUE)) {
      if (MatchedRecordBlockClean == TRUE) {
        //
        // A previous relocation may already have made the replacement
        // durable before reset.  Finalize that clean copy and resume erasing
        // the contaminated legacy blocks instead of relocating it again.
        //
        Status = FinalizeValidatedRecords (
                   This,
                   1,
                   &MatchedRecord,
                   MatchedRecord
                   );
        if (!EFI_ERROR (Status)) {
          Status = EraseLegacyMigrationBlocks (
                     This,
                     MatchedRecord->ByteOffset,
                     MatchingV0Offset
                     );
        }
      } else {
        //
        // Header-only candidates from a legacy layout share the matched
        // record's erase block.  Make a durable copy in a different block
        // before erasing the source and all legacy data.
        //
        Status = WriteV1ToMigrationBlock (
                   This,
                   MatchedRecord,
                   MatchedRecord->Measurement,
                   MatchingV0Offset,
                   &MigrationOffset
                   );
        if (!EFI_ERROR (Status)) {
          Status = EraseLegacyMigrationBlocks (
                     This,
                     MigrationOffset,
                     MatchingV0Offset
                     );
        }
      }
    } else {
      Status = FinalizeValidatedRecords (
                 This,
                 NumValidRecords,
                 LastMeasurements,
                 MatchedRecord
                 );
    }

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

  if (IsV0MigrationAllowed () == FALSE) {
    DEBUG ((
      DEBUG_ERROR,
      "VarIntMigrationPolicy: V1 did not match and V0 migration is disabled by PcdAllowVarIntV0Migration\r\n"
      ));
    MigrationBlocked = TRUE;
    Status           = EFI_SECURITY_VIOLATION;
    goto ExitVarIntValidate;
  }

  //
  // The FF-A transport signs scratch measurements through CurMeasurement's
  // shared page.  Preserve V1 before computing V0 so migration cannot persist
  // a V0 payload under a V1 header on non-OP-TEE deployments.
  //
  SavedV1Measurement = AllocateRuntimeCopyPool (
                         This->MeasurementSize,
                         This->CurMeasurement
                         );
  if (SavedV1Measurement == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitVarIntValidate;
  }

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

  LogMeasurementPreview ("validate-computed-v0", CurMeas, V0RecordSize);

  //
  // CurMeas is also the flash-read scratch buffer used by the record scan.
  // Preserve the computed V0 value before the scan overwrites it.
  //
  V0ComputedMeasurement = AllocateRuntimeCopyPool (V0RecordSize, CurMeas);
  if (V0ComputedMeasurement == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitVarIntValidate;
  }

  V0Meas = &V0ComputedMeasurement[HEADER_SZ_BYTES];

  ClearLastMeasurementBuffers (This);
  Status = GetLastValidMeasurementsByLayout (
             This,
             LastMeasurements,
             &NumValidRecords,
             V0RecordSize,
             VarIntRecordFilterV0,
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to scan legacy V0 measurements %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitVarIntValidate;
  }

  for (Index = 0; Index < NumValidRecords; Index++) {
    ReadMeas = LastMeasurements[Index];

    if (IsRecordV0 (ReadMeas->Measurement[0]) == FALSE) {
      continue;
    }

    if (CompareMem (V0Meas, &ReadMeas->Measurement[HEADER_SZ_BYTES], V0PayloadSize) == 0) {
      Matched       = TRUE;
      MatchedRecord = ReadMeas;
      DEBUG ((
        DEBUG_INFO,
        "%a: %u Found V0 MATCH, migrating to V1\n",
        __FUNCTION__,
        Index
        ));
      Status = MigrateV0RecordToV1 (
                 This,
                 MatchedRecord,
                 SavedV1Measurement,
                 &MigrationOffset
                 );
      if (!EFI_ERROR (Status)) {
        Status = EraseLegacyMigrationBlocks (
                   This,
                   MigrationOffset,
                   MatchedRecord->ByteOffset
                   );
      }

      goto ExitVarIntValidate;
    }
  }

ExitVarIntValidate:
  if ((Matched != TRUE) &&
      (MigrationBlocked != TRUE) &&
      !EFI_ERROR (Status) &&
      (This->BootstrapDeferred == FALSE))
  {
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

  ZeroMem (
    This->CurMeasurement,
    GetMaxMeasurementRecordSize (This)
    );
  if (CurMeas != NULL) {
    ZeroMem (CurMeas, GetMaxMeasurementRecordSize (This));
  }

  if (V0ComputedMeasurement != NULL) {
    ZeroMem (V0ComputedMeasurement, V0RecordSize);
    FreePool (V0ComputedMeasurement);
  }

  if (SavedV1Measurement != NULL) {
    ZeroMem (SavedV1Measurement, This->MeasurementSize);
    FreePool (SavedV1Measurement);
  }

  ClearLastMeasurementBuffers (This);

  DEBUG ((
    DEBUG_INFO,
    "VarIntValidationResult: status=%r matched=%u migration_blocked=%u bootstrap_deferred=%u v0_record_size=%u v1_record_size=%u\n",
    Status,
    Matched,
    MigrationBlocked,
    This->BootstrapDeferred,
    V0RecordSize,
    This->MeasurementSize
    ));

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
  UINT32      MaxRecordSize;
  UINTN       Index;
  BOOLEAN     CurMeasAllocated;

  CurMeasAllocated = FALSE;

  if ((NorFlashProto == NULL) || (NorFlashAttributes == NULL) ||
      (NorFlashAttributes->BlockSize == 0) || (PartitionSize == 0) ||
      (PartitionStartOffset > MAX_UINT32) || (PartitionSize > MAX_UINT32) ||
      (PartitionSize > (((UINT64)MAX_UINT32 + 1) - PartitionStartOffset)) ||
      ((PartitionStartOffset % NorFlashAttributes->BlockSize) != 0) ||
      ((PartitionSize % NorFlashAttributes->BlockSize) != 0))
  {
    return EFI_INVALID_PARAMETER;
  }

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

  OpteePresent = IsOpteePresent ();

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
  MaxRecordSize                         = GetMaxMeasurementRecordSize (VarIntProto);
  DEBUG ((
    DEBUG_INFO,
    "VarIntLayout: optee_present=%u hash_size=%u header_size=%u "
    "record_size=%u payload_size=%u partition_start=0x%lx "
    "partition_size=0x%lx block_size=0x%x\n",
    OpteePresent,
    MeasSize,
    HEADER_SZ_BYTES,
    VarIntProto->MeasurementSize,
    VarIntProto->MeasurementSize - HEADER_SZ_BYTES,
    (UINT64)PartitionStartOffset,
    (UINT64)PartitionSize,
    VarIntProto->BlockSize
    ));
  VarIntProto->CurMeasurement = AllocateAlignedPages (EFI_SIZE_TO_PAGES (MaxRecordSize), EFI_PAGE_SIZE);
  if (VarIntProto->CurMeasurement == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "%a: Not enough resources to allocate Measurement Buffer - %r", __FUNCTION__, Status);
  }

  ZeroMem (
    VarIntProto->CurMeasurement,
    EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (MaxRecordSize))
    );

  VarIntProto->PartitionData = AllocateRuntimeZeroPool (VarIntProto->PartitionSize);
  if (VarIntProto->PartitionData == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitVarIntInit, "%a: Not enough resources to allocate Partition Data - %r", __FUNCTION__, Status);
  }

  Status = NorFlashProto->Read (
                            NorFlashProto,
                            (UINT32)VarIntProto->PartitionByteOffset,
                            (UINT32)VarIntProto->PartitionSize,
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
                                             MaxRecordSize * sizeof (UINT8)
                                             );
    if (LastMeasurements[Index]->Measurement == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitVarIntInit, "%a: Failed to Allocate Measurements Buffer - %r", __FUNCTION__, Status);
    }

    LastMeasurements[Index]->ByteOffset = 0;
  }

  //
  // Historical T234 V0 firmware signed from an aligned page-backed buffer.
  // Preserve that property for the legacy compatibility path because OP-TEE
  // consumes the raw StMM address supplied in the direct FF-A request.
  //
  CurMeas = AllocateAlignedPages (EFI_SIZE_TO_PAGES (MaxRecordSize), EFI_PAGE_SIZE);
  if (CurMeas == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "%a: Not Enough Resources to allocate Buffer - %r", __FUNCTION__, Status);
  }

  CurMeasAllocated = TRUE;
  ZeroMem (
    CurMeas,
    EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (MaxRecordSize))
    );

  SpeculativeMeasurement = AllocateRuntimeZeroPool (MaxRecordSize);
  if (SpeculativeMeasurement == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "%a: Not Enough Resources to allocate Speculative Buffer - %r", __FUNCTION__, Status);
  }

  if (!OpteePresent) {
    Status = FfaInit (VarIntProto);
    NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "Failed to Initialize FFA - %r", Status);
  }

ExitVarIntInit:
  if (EFI_ERROR (Status) && (CurMeasAllocated == TRUE) && (CurMeas != NULL)) {
    FreeAlignedPages (CurMeas, EFI_SIZE_TO_PAGES (MaxRecordSize));
    CurMeas = NULL;
  }

  return Status;
}
