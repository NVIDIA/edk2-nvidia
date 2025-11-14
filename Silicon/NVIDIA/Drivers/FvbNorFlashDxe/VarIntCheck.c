/** @file

  Standalone MM Variable Integrity driver.

  SPDX-FileCopyrightText: Copyright (c) 2024 - 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/MmServicesTableLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include "Base.h"
#include "FvbPrivate.h"
#include "Library/DebugLib.h"
#include "Library/MemoryAllocationLib.h"
#include <Guid/ImageAuthentication.h>
#include <Protocol/SmmVariable.h>
#include <IndustryStandard/Tpm20.h>
#include <Library/AuthVariableLib.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <Library/ArmSvcLib.h>
#include <Library/PrintLib.h>
#include <Library/OpteeNvLib.h>
#include <Library/NvVarIntLib.h>
#include <Library/NVIDIADebugLib.h>

#define HEADER_SZ_BYTES    (1)
#define MAX_VALID_RECORDS  (2)

typedef struct {
  CHAR16      *VarName;
  EFI_GUID    *VarGuid;
} MEASURE_VAR_TYPE;

typedef struct {
  UINT8     *Measurement;
  UINT64    ByteOffset;
} MEASURE_REC_TYPE;

NVIDIA_VAR_INT_PROTOCOL  *VarIntProto = NULL;
STATIC MEASURE_REC_TYPE  *LastMeasurements[MAX_VALID_RECORDS];
STATIC UINT8             *CurMeas;
STATIC CONST UINT16      VarAuthTa = 5U;
STATIC UINT16            OpteeVmId = 0;
STATIC UINT16            MmVmId    = 0;
STATIC UINT64            FfaHandle = 0;

STATIC MEASURE_VAR_TYPE  SecureVars[] = {
  { EFI_SECURE_BOOT_MODE_NAME,    &gEfiGlobalVariableGuid        },
  { EFI_PLATFORM_KEY_NAME,        &gEfiGlobalVariableGuid        },
  { EFI_KEY_EXCHANGE_KEY_NAME,    &gEfiGlobalVariableGuid        },
  { EFI_IMAGE_SECURITY_DATABASE,  &gEfiImageSecurityDatabaseGuid },
  { EFI_IMAGE_SECURITY_DATABASE1, &gEfiImageSecurityDatabaseGuid }
};

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
BOOLEAN
EFIAPI
IsDigitCharacter (
  IN      CHAR16  Char
  )
{
  return (BOOLEAN)((Char >= L'0' && Char <= L'9'));
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

  if ((SvcArgs.Arg0 == ARM_FID_FFA_MSG_SEND_DIRECT_RESP)) {
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

/*
 * IsSecureDbVar.
 * Is this a SecureDb variable ? We care about SecureBoot
 * and the secure db variables (PK/KEK/db/dbx)
 *
 * @param[in]  VarName  Variable Name.
 * @param[out] VarGuid  Variable Guid.
 *
 * @result TRUE  This is a boot variable.
 *         FALSE not a boot variable.
 */
STATIC
BOOLEAN
IsSecureDbVar (
  IN CHAR16    *VarName,
  IN EFI_GUID  *VarGuid
  )
{
  BOOLEAN  SecureDbVar;
  UINTN    Index;

  SecureDbVar = FALSE;
  for (Index = 0; Index < ARRAY_SIZE (SecureVars); Index++) {
    if ((StrCmp (SecureVars[Index].VarName, VarName) == 0) &&
        (CompareGuid (SecureVars[Index].VarGuid, VarGuid) == TRUE))
    {
      SecureDbVar = TRUE;
      break;
    }
  }

  return SecureDbVar;
}

/*
 * IsBootVar.
 * Is this a boot variable ? We care about BootOrder and Bootxxx
 *
 * @param[in]  VarName  Variable Name.
 * @param[out] VarGuid  Variable Guid.
 *
 * @result TRUE  This is a boot variable.
 *         FALSE not a boot variable.
 */
STATIC
BOOLEAN
IsBootVar (
  IN CHAR16    *VarName,
  IN EFI_GUID  *VarGuid
  )
{
  BOOLEAN  BootVar;
  CHAR16   *BootStr;
  UINTN    BootStrLen;

  BootVar    = FALSE;
  BootStr    = StrStr (VarName, L"Boot");
  BootStrLen = StrLen (L"Boot");

  /* If there is a Boot at the beginning */
  if ((BootStr != NULL) && (BootStr == VarName) &&
      (StrLen (VarName) > BootStrLen) &&
      (CompareGuid (&gEfiGlobalVariableGuid, VarGuid) == TRUE))
  {
    if (StrCmp (VarName, EFI_BOOT_ORDER_VARIABLE_NAME) == 0) {
      DEBUG ((DEBUG_INFO, "%d: Callback received for BootVar %s\n", __LINE__, VarName));
      BootVar = TRUE;
    } else if (IsDigitCharacter (VarName[BootStrLen]) == TRUE) {
      DEBUG ((DEBUG_INFO, "Callback received for BootVar %s\n", VarName));
      BootVar = TRUE;
    }
  }

  return BootVar;
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
  EFI_STATUS  Status;

  Status    = EFI_SUCCESS;
  *MeasSize = 0;
  switch (PcdGet32 (PcdHashApiLibPolicy)) {
    case HASH_ALG_SHA256:
    case HASH_ALG_SM3_256:
      *MeasSize += 32;
      break;
    case HASH_ALG_SHA384:
      *MeasSize += 48;
      break;
    case HASH_ALG_SHA512:
      *MeasSize += 64;
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
      if ((BlockOffset + This->MeasurementSize) < BlockEnd) {
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
        } else if (ReadBuf[0] == VAR_INT_VALID) {
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
  Compute the New measurement for the variables we're monitoring.
  if this is for a variable we're not monitoring , then ignore.

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
  UINT8       *Meas;

  if ((IsSecureDbVar (VariableName, VendorGuid) == FALSE) &&
      (IsBootVar (VariableName, VendorGuid) == FALSE))
  {
    Status = EFI_SUCCESS;
    goto ExitComputeVarMeasurement;
  }

  ZeroMem (This->CurMeasurement, This->MeasurementSize);
  Meas   = &This->CurMeasurement[1];
  Status = ComputeVarMeasurement (VariableName, VendorGuid, Attributes, Data, Size, Meas);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to compute measurement %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitComputeVarMeasurement;
  }

  Status = SendOpteeCmd (&This->CurMeasurement[1], (This->MeasurementSize - 1));

  /*
   * Failed to get measurement, for now treat this by not marking the measurement
   * as ready to be written to the Flash.
   * Unsure if we should assert at this point.
   */
  if (EFI_ERROR (Status)) {
    NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "Failed to get signed measurement - %r", Status);
  } else {
    This->CurMeasurement[0] = FVB_ERASED_BYTE;
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
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT64      CurOffset;

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
  This->CurMeasurement[0] = VAR_INT_PENDING;
  Status                  = PartitionWrite (
                              This,
                              CurOffset,
                              This->MeasurementSize,
                              This->CurMeasurement
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Write measurement to %lu\n", CurOffset));
  }

ExitVarIntWriteMeasurement:
  return Status;
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
      if ((BlockOffset + VarInt->MeasurementSize) < BlockEnd) {
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

        if ((ReadBuf[0] == VAR_INT_VALID) ||
            (ReadBuf[0] == VAR_INT_PENDING))
        {
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

  Status = EFI_SUCCESS;
  for (Index = 0; Index < NumValidRecords; Index++) {
    CurRec = Measurements[Index];
    if (CurRec->Measurement[0] == VAR_INT_PENDING) {
      /* If the Var Update failed, then declare the pending measurement
       * as invalid.
       */
      if (EFI_ERROR (PreviousResult)) {
        CurRec->Measurement[0] = VAR_INT_INVALID;
      } else {
        CurRec->Measurement[0] = VAR_INT_VALID;
      }
    } else {
      /* If the Var Update failed, then don't invalidate the previous
       *  Valid measurement.
       */
      if (EFI_ERROR (PreviousResult)) {
        CurRec->Measurement[0] = VAR_INT_VALID;
      } else {
        CurRec->Measurement[0] = VAR_INT_INVALID;
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
    Status = PartitionWrite (
               VarIntProto,
               CurRec->ByteOffset,
               1,
               &CurRec->Measurement[0]
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to Write measurement to %lu %r\n",
        __FUNCTION__,
        CurRec->ByteOffset,
        Status
        ));
    }
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
  UINT32      NumValidRecords;
  UINTN       Index;

  if ((IsSecureDbVar (VariableName, VendorGuid) == FALSE) &&
      (IsBootVar (VariableName, VendorGuid) == FALSE))
  {
    Status = EFI_SUCCESS;
    goto ExitVarIntInvalidateLast;
  }

  if (This->CurMeasurement[0] != VAR_INT_PENDING) {
    Status = EFI_SUCCESS;
    goto ExitVarIntInvalidateLast;
  }

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
    goto ExitVarIntInvalidateLast;
  }

  if (NumValidRecords == 0) {
    DEBUG ((DEBUG_ERROR, "%a: No Valid Records are found\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto ExitVarIntInvalidateLast;
  }

  This->CurMeasurement[0] = VAR_INT_VALID;
  Status                  = CommitMeasurements (
                              NumValidRecords,
                              LastMeasurements,
                              This,
                              PrevResult
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Commit Measurements %r\n",
      __FUNCTION__,
      Status
      ));
  }

ExitVarIntInvalidateLast:
  ZeroMem (This->CurMeasurement, This->MeasurementSize);
  for (Index = 0; Index < MAX_VALID_RECORDS; Index++) {
    ZeroMem (LastMeasurements[Index]->Measurement, This->MeasurementSize);
  }

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
  IN NVIDIA_VAR_INT_PROTOCOL  *VarInt,
  IN UINT8                    *Meas
  )
{
  EFI_STATUS  Status;
  UINT64      WriteOffset;

  if (VarInt->CurMeasurement[0] == FVB_ERASED_BYTE) {
    DEBUG ((DEBUG_ERROR, "Initializing Partition\n"));
    VarInt->CurMeasurement[0] = VAR_INT_VALID;
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
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Write measurement to %lu\n", WriteOffset));
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to init partition %r\n",
        __FUNCTION__,
        Status
        ));
    }
  } else {
    Status = EFI_SUCCESS;
  }

ExitInitPartition:
  return Status;
}

/**
 * Check if all the buffer is filled with the Value.
 *
 * @param[in] Buf    Buffer to check.
 * @param[in] Size   Size of the buffer.
 * @param[in] Value  Value to check against.
 *
 * @return TRUE  Contents of buffer filled with Value.
 *         FALSE Otherwise.
 */
STATIC
BOOLEAN
CheckBuf (
  UINT8  *Buf,
  UINTN  Size,
  UINT8  Value
  )
{
  UINTN  Index;

  for (Index = 0; Index < Size; Index++) {
    if (Buf[Index] != Value) {
      return FALSE;
    }
  }

  return TRUE;
}

/**
  IsMeasurementPartitionErasedOrZero
  Check if the variable integrity storage region is erased

  @param This    Pointer to Variable Integrity Protocol.

  @retval TRUE   Partition is erased.
          other  Partition isn't erased..

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
        "%a: NorFlash Read Failed at %lu offset %r\n",
        __FUNCTION__,
        PartitionOffset,
        Status
        ));
      IsErasedOrZero = FALSE;
      goto ExitIsMeasurementPartitionErased;
    }

    if ((CheckBuf (Buf, SIZE_1KB, FVB_ERASED_BYTE) != TRUE) &&
        (CheckBuf (Buf, SIZE_1KB, 0) != TRUE))
    {
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
  UINT8             *Meas;
  MEASURE_REC_TYPE  *ReadMeas;
  BOOLEAN           Matched;
  BOOLEAN           RecommitRec;

  Matched     = FALSE;
  RecommitRec = FALSE;

  Status = EFI_SUCCESS;

  /* Compute the hash over the variables we're monitoring */
  Meas   = &This->CurMeasurement[1];
  Status = ComputeVarMeasurement (NULL, NULL, 0, NULL, 0, Meas);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a Failed to Compute %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitVarIntValidate;
  }

  Status = SendOpteeCmd (Meas, (This->MeasurementSize - 1));
  NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "Failed to get signed device measurement - %r", Status);

  This->CurMeasurement[0] = FVB_ERASED_BYTE;

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
    if (CompareMem (Meas, &ReadMeas->Measurement[1], (This->MeasurementSize - 1)) == 0) {
      Matched = TRUE;
      DEBUG ((
        DEBUG_INFO,
        "%a: %u Found MATCH, Measurement Valid\n",
        __FUNCTION__,
        Index
        ));
      if (ReadMeas->Measurement[0] == VAR_INT_PENDING) {
        RecommitRec = TRUE;
      }

      Status = EFI_SUCCESS;
    } else {
      DEBUG ((
        DEBUG_INFO,
        " %a:ERROR Failed to match Stored Measurement with computed.\n",
        __FUNCTION__
        ));
      if (ReadMeas->Measurement[0] == VAR_INT_PENDING) {
        ReadMeas->Measurement[0] = VAR_INT_VALID;
        RecommitRec              = TRUE;
      }
    }
  }

  /* We've discovered more than one valid record We may need to re-commit the
   * last records.
   */
  if ((NumValidRecords > 1) || (RecommitRec == TRUE)) {
    DEBUG ((DEBUG_INFO, "Found more than one Valid Record, commiting\n"));
    Status = CommitMeasurements (
               NumValidRecords,
               LastMeasurements,
               This,
               EFI_SUCCESS
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to Commit Measurements %r\n",
        __FUNCTION__,
        Status
        ));
    }
  }

ExitVarIntValidate:
  if (Matched != TRUE) {
    if (IsMeasurementPartitionErasedOrZero (
          This->NorFlashProtocol,
          This->PartitionByteOffset,
          This->PartitionSize
          ) == TRUE)
    {
      DEBUG ((DEBUG_ERROR, "The Variable Integrity Partition is erased\n"));
      Status = InitPartition (This, Meas);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Init Partition Failed %r\n", Status));
      }
    } else {
      /* If we're here then we couldn't find a matching measurement for
       * the Var store, flag this as a possible tamper detect.
       */
      DEBUG ((DEBUG_ERROR, "%a: FAILED TO VALIDATE\n", __FUNCTION__));
      Status = EFI_DEVICE_ERROR;
    }
  }

  ZeroMem (This->CurMeasurement, This->MeasurementSize);
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
  VarIntProto->PartitionByteOffset   = PartitionStartOffset;
  VarIntProto->PartitionSize         = PartitionSize;
  VarIntProto->BlockSize             = NorFlashAttributes->BlockSize;
  VarIntProto->WriteNewMeasurement   = VarIntWriteMeasurement;
  VarIntProto->InvalidateLast        = VarIntInvalidateLast;
  VarIntProto->ComputeNewMeasurement = VarIntComputeMeasurement;
  VarIntProto->Validate              = VarIntValidate;
  VarIntProto->NorFlashProtocol      = NorFlashProto;
  VarIntProto->MeasurementSize       = MeasSize + HEADER_SZ_BYTES;
  VarIntProto->CurMeasurement        = AllocateAlignedPages (EFI_SIZE_TO_PAGES (MeasSize), EFI_PAGE_SIZE);
  if (VarIntProto->CurMeasurement == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "%a: Not enough resources to allocate Measurement Buffer - %r", __FUNCTION__, Status);
  }

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

  if (!IsOpteePresent ()) {
    Status = FfaInit (VarIntProto);
    NV_ASSERT_RETURN (!EFI_ERROR (Status), CpuDeadLoop (), "Failed to Initialize FFA - %r", Status);
  }

ExitVarIntInit:
  return Status;
}
