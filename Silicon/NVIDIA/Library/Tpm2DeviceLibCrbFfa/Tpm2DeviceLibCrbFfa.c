/** @file
  TPM 2.0 Device Library using CRB over FF-A to StandaloneMM

  This library provides TPM access via CRB (Command Response Buffer) interface
  through FF-A communication with StandaloneMM TPM service

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HashLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/Tpm2DeviceLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/Tpm2CommandLib.h>
#include <Library/ArmFfaLib.h>

#include <Guid/TpmInstance.h>
#include <Guid/Tpm2ServiceFfa.h>

#include <IndustryStandard/Tpm20.h>
#include <IndustryStandard/TpmPtp.h>
#include <IndustryStandard/ArmFfaPartInfo.h>

#include <NVIDIAStatusCodes.h>
#include <OemStatusCodes.h>

//
// NVIDIA-specific TPM Service Function IDs
//
#define TPM2_FFA_GET_CRB_BUFFER   0x0f001001
#define TPM2_FFA_DISABLE_SERVICE  0x0f001002

#define DEFAULT_LOCALITY  0

STATIC UINT16                      mPartId;
STATIC UINT16                      mStmmPartitionId  = 0;
STATIC BOOLEAN                     mLibraryAvailable = FALSE;
STATIC volatile PTP_CRB_REGISTERS  *mCrbRegisters    = NULL;

/**
  Send request to StandaloneMM to disable TPM CRB FFA service

  @retval EFI_SUCCESS           Successfully got the Standalone MM Partition ID
  @retval Other                 Error.
**/
STATIC
EFI_STATUS
DisableStmmCrbFfaService (
  VOID
  )
{
  EFI_STATUS       Status;
  DIRECT_MSG_ARGS  DirectMsgArgs;

  ASSERT (mStmmPartitionId != 0);

  DEBUG ((DEBUG_INFO, "%a: Send request to disable TPM CRB FFA service\n", __FUNCTION__));

  ZeroMem (&DirectMsgArgs, sizeof (DIRECT_MSG_ARGS));
  DirectMsgArgs.Arg0 = TPM2_FFA_DISABLE_SERVICE;

  Status = ArmFfaLibMsgSendDirectReq2 (
             mStmmPartitionId,
             &gTpm2ServiceFfaGuid,
             &DirectMsgArgs
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send disable service request: %r\n", __FUNCTION__, Status));
    return Status;
  }

  if (DirectMsgArgs.Arg0 != TPM2_FFA_SUCCESS_OK) {
    DEBUG ((DEBUG_ERROR, "%a: Disable service failed: 0x%llx\n", __FUNCTION__, DirectMsgArgs.Arg0));
    return EFI_DEVICE_ERROR;
  }

  PcdSet64S (PcdTpmCrbBase, 0);
  PcdSet64S (PcdTpmCrbSize, 0);
  PcdSet16S (PcdTpmCrbFfaPartitionId, 0);
  return EFI_SUCCESS;
}

/**
  Get the Standalone MM Partition ID for TPM service.

  @retval EFI_SUCCESS           Successfully got the Standalone MM Partition ID
  @retval Other                 Error.
**/
STATIC
EFI_STATUS
GetTpmServicePartitionId (
  VOID
  )
{
  EFI_STATUS              Status;
  VOID                    *TxBuffer;
  UINT64                  TxBufferSize;
  VOID                    *RxBuffer;
  UINT64                  RxBufferSize;
  EFI_FFA_PART_INFO_DESC  *StmmPartInfo;
  UINT32                  Count;
  UINT32                  Size;

  Status = ArmFfaLibPartitionIdGet (&mPartId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get partition id: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = ArmFfaLibGetRxTxBuffers (
             &TxBuffer,
             &TxBufferSize,
             &RxBuffer,
             &RxBufferSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get Rx/Tx buffer: %r\n", __FUNCTION__, Status));
    goto ErrorHandler;
  }

  Status = ArmFfaLibPartitionInfoGet (
             &gTpm2ServiceFfaGuid,
             FFA_PART_INFO_FLAG_TYPE_DESC,
             &Count,
             &Size
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get service partition details: %r\n", __FUNCTION__, Status));
    goto ErrorHandler;
  }

  if ((Count != 1) || (Size < sizeof (EFI_FFA_PART_INFO_DESC))) {
    Status = EFI_INVALID_PARAMETER;
    DEBUG ((DEBUG_ERROR, "%a: Invalid Count: %u, Size: %u\n", __FUNCTION__, Count, Size));
    goto ErrorHandler;
  }

  StmmPartInfo     = (EFI_FFA_PART_INFO_DESC *)RxBuffer;
  mStmmPartitionId = StmmPartInfo->PartitionId;

  DEBUG ((DEBUG_INFO, "%a: TPM Service Partition ID: 0x%x\n", __FUNCTION__, mStmmPartitionId));
  Status = EFI_SUCCESS;

ErrorHandler:
  ArmFfaLibRxRelease (mPartId);
  return Status;
}

/**
  Get the CRB buffer address from StandaloneMM

  @retval EFI_SUCCESS           Successfully got the CRB buffer address.
  @retval Other                 Error.
**/
STATIC
EFI_STATUS
GetCrbBufferAddress (
  VOID
  )
{
  EFI_STATUS       Status;
  DIRECT_MSG_ARGS  DirectMsgArgs;
  UINT64           BufferBase;
  UINT64           BufferSize;

  if (mCrbRegisters != NULL) {
    return EFI_SUCCESS;
  }

  ASSERT (mStmmPartitionId != 0);

  //
  // Get CRB buffer address via FF-A
  // Using custom FID to get the shared CRB buffer address
  //
  ZeroMem (&DirectMsgArgs, sizeof (DIRECT_MSG_ARGS));
  DirectMsgArgs.Arg0 = TPM2_FFA_GET_CRB_BUFFER;

  Status = ArmFfaLibMsgSendDirectReq2 (
             mStmmPartitionId,
             &gTpm2ServiceFfaGuid,
             &DirectMsgArgs
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send get CRB buffer request: %r\n", __FUNCTION__, Status));
    return Status;
  }

  if ((DirectMsgArgs.Arg0 != TPM2_FFA_SUCCESS_OK) && (DirectMsgArgs.Arg0 != TPM2_FFA_SUCCESS_OK_RESULTS_RETURNED)) {
    DEBUG ((DEBUG_ERROR, "%a: Get CRB buffer failed: %llx\n", __FUNCTION__, DirectMsgArgs.Arg0));
    return EFI_NOT_READY;
  }

  //
  // CRB buffer address and size are returned in Arg1 and Arg2
  //
  BufferBase = DirectMsgArgs.Arg1;
  BufferSize = DirectMsgArgs.Arg2;

  if ((BufferBase == 0) || (BufferSize < sizeof (PTP_CRB_REGISTERS))) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid CRB buffer: base=0x%llx, size=%llx\n", __FUNCTION__, BufferBase, BufferSize));
    return EFI_DEVICE_ERROR;
  }

  mCrbRegisters = (PTP_CRB_REGISTERS *)BufferBase;

  DEBUG ((DEBUG_INFO, "%a: CRB buffer at 0x%llx, size 0x%llx\n", __FUNCTION__, BufferBase, BufferSize));

  return EFI_SUCCESS;
}

/**
  Performs byte-wise copy operations to/from the CRB buffer.
  Standard CopyMem() cannot be used because it may optimize transfers to word or DWORD sizes,
  which would trigger alignment faults when accessing the CRB buffer.

  @param  DestinationBuffer   The pointer to the destination buffer of the memory copy.
  @param  SourceBuffer        The pointer to the source buffer of the memory copy.
  @param  Length              The number of bytes to copy from SourceBuffer to DestinationBuffer.

  @return DestinationBuffer.
**/
STATIC
VOID *
CrbCopyMem (
  OUT VOID       *DestinationBuffer,
  IN CONST VOID  *SourceBuffer,
  IN UINTN       Length
  )
{
  volatile UINT8  *Des = (volatile UINT8 *)DestinationBuffer;
  volatile UINT8  *Src = (volatile UINT8 *)SourceBuffer;

  if (DestinationBuffer == SourceBuffer) {
    return DestinationBuffer;
  }

  for ( ; Length > 0; Length--, Des++, Src++) {
    *Des = *Src;
  }

  return DestinationBuffer;
}

/**
  Send a TPM command to StandaloneMM via FF-A CRB interface.

  @param[in]      InputParameterBlockSize  Size of the TPM2 input parameter block.
  @param[in]      InputParameterBlock      Pointer to the TPM2 input parameter block.
  @param[in,out]  OutputParameterBlockSize Size of the TPM2 output parameter block.
  @param[in]      OutputParameterBlock     Pointer to the TPM2 output parameter block.

  @retval EFI_SUCCESS            The command byte stream was successfully sent to the device and a response was successfully received.
  @retval EFI_DEVICE_ERROR       The command was not successfully sent to the device or a response was not successfully received from the device.
  @retval EFI_BUFFER_TOO_SMALL   The output parameter block is too small.
**/
EFI_STATUS
EFIAPI
Tpm2SubmitCommand (
  IN UINT32      InputParameterBlockSize,
  IN UINT8       *InputParameterBlock,
  IN OUT UINT32  *OutputParameterBlockSize,
  IN UINT8       *OutputParameterBlock
  )
{
  EFI_STATUS       Status;
  DIRECT_MSG_ARGS  DirectMsgArgs;
  UINT32           ResponseSize;
  UINT32           Data32;

  if ((InputParameterBlock == NULL) || (OutputParameterBlock == NULL) || (OutputParameterBlockSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!mLibraryAvailable) {
    return EFI_UNSUPPORTED;
  }

  ASSERT (mStmmPartitionId != 0);
  ASSERT (mCrbRegisters != NULL);

  if ((InputParameterBlockSize == 0) || (InputParameterBlockSize > sizeof (mCrbRegisters->CrbDataBuffer))) {
    DEBUG ((DEBUG_ERROR, "%a: Command size too large: %u\n", __FUNCTION__, InputParameterBlockSize));
    return EFI_BAD_BUFFER_SIZE;
  }

  //
  // Copy command data into the CRB buffer
  //
  CrbCopyMem ((VOID *)mCrbRegisters->CrbDataBuffer, InputParameterBlock, InputParameterBlockSize);
  ArmDataMemoryBarrier ();

  //
  // Write control registers to signal command is ready
  //
  mCrbRegisters->CrbControlRequest = PTP_CRB_CONTROL_AREA_REQUEST_COMMAND_READY;
  mCrbRegisters->CrbControlStart   = PTP_CRB_CONTROL_START;
  ArmDataMemoryBarrier ();

  //
  // Send START command to process TPM command
  //
  ZeroMem (&DirectMsgArgs, sizeof (DIRECT_MSG_ARGS));
  DirectMsgArgs.Arg0 = TPM2_FFA_START;
  DirectMsgArgs.Arg1 = TPM2_FFA_START_FUNC_QUALIFIER_COMMAND;
  DirectMsgArgs.Arg2 = DEFAULT_LOCALITY;

  Status = ArmFfaLibMsgSendDirectReq2 (
             mStmmPartitionId,
             &gTpm2ServiceFfaGuid,
             &DirectMsgArgs
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to start command: %r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  if ((DirectMsgArgs.Arg0 != TPM2_FFA_SUCCESS_OK) && (DirectMsgArgs.Arg0 != TPM2_FFA_SUCCESS_OK_RESULTS_RETURNED)) {
    DEBUG ((DEBUG_ERROR, "%a: Command execution failed: 0x%llx\n", __FUNCTION__, DirectMsgArgs.Arg0));
    return EFI_DEVICE_ERROR;
  }

  ArmDataMemoryBarrier ();
  CrbCopyMem (&Data32, (VOID *)(mCrbRegisters->CrbDataBuffer + OFFSET_OF (TPM2_COMMAND_HEADER, paramSize)), sizeof (Data32));
  ResponseSize = SwapBytes32 (Data32);

  if (ResponseSize < sizeof (TPM2_RESPONSE_HEADER)) {
    DEBUG ((DEBUG_ERROR, "%a: Response size too small: %u\n", __FUNCTION__, ResponseSize));
    return EFI_DEVICE_ERROR;
  }

  if (ResponseSize > sizeof (mCrbRegisters->CrbDataBuffer)) {
    DEBUG ((DEBUG_ERROR, "%a: Response size too large: %u\n", __FUNCTION__, ResponseSize));
    return EFI_DEVICE_ERROR;
  }

  if (ResponseSize > *OutputParameterBlockSize) {
    *OutputParameterBlockSize = ResponseSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  //
  // Copy response data
  //
  CrbCopyMem (OutputParameterBlock, (VOID *)mCrbRegisters->CrbDataBuffer, ResponseSize);
  *OutputParameterBlockSize = ResponseSize;

  DEBUG ((DEBUG_VERBOSE, "%a: Command completed, response size: %u\n", __FUNCTION__, ResponseSize));

  return EFI_SUCCESS;
}

/**
  Request use TPM2 via FF-A.

  @retval EFI_SUCCESS      Get the control of TPM2 chip.
  @retval EFI_NOT_FOUND    TPM2 not found.
  @retval EFI_DEVICE_ERROR Unexpected device behavior.
**/
EFI_STATUS
EFIAPI
Tpm2RequestUseTpm (
  VOID
  )
{
  EFI_STATUS       Status;
  DIRECT_MSG_ARGS  DirectMsgArgs;

  if (!mLibraryAvailable) {
    return EFI_UNSUPPORTED;
  }

  ASSERT (mStmmPartitionId != 0);
  ASSERT (mCrbRegisters != NULL);

  mCrbRegisters->LocalityControl = PTP_CRB_LOCALITY_CONTROL_REQUEST_ACCESS;
  ArmDataMemoryBarrier ();

  ZeroMem (&DirectMsgArgs, sizeof (DIRECT_MSG_ARGS));
  DirectMsgArgs.Arg0 = TPM2_FFA_START;
  DirectMsgArgs.Arg1 = TPM2_FFA_START_FUNC_QUALIFIER_LOCALITY;
  DirectMsgArgs.Arg2 = DEFAULT_LOCALITY;

  Status = ArmFfaLibMsgSendDirectReq2 (
             mStmmPartitionId,
             &gTpm2ServiceFfaGuid,
             &DirectMsgArgs
             );
  if (EFI_ERROR (Status) || (DirectMsgArgs.Arg0 != TPM2_FFA_SUCCESS_OK)) {
    DEBUG ((DEBUG_ERROR, "%a: Locality request failed: 0x%llx\n", __FUNCTION__, DirectMsgArgs.Arg0));
    return EFI_DEVICE_ERROR;
  }

  ArmDataMemoryBarrier ();
  if ((mCrbRegisters->LocalityStatus & PTP_CRB_LOCALITY_STATUS_GRANTED) == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Locality request denied: 0x%llx\n", __FUNCTION__, DirectMsgArgs.Arg0));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Initialize TPM via FF-A CRB interface.

  @retval EFI_SUCCESS      TPM initialized successfully.
  @retval EFI_DEVICE_ERROR Unexpected device behavior.
**/
STATIC
EFI_STATUS
Tpm2Initialize (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = Tpm2RequestUseTpm ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to request to use TPM.\n", __FUNCTION__));
    REPORT_STATUS_CODE_WITH_EXTENDED_DATA (
      EFI_ERROR_CODE | EFI_ERROR_MAJOR,
      EFI_CLASS_NV_FIRMWARE | EFI_NV_FW_UEFI_EC_TPM_INACCESSIBLE,
      OEM_EC_DESC_TPM_INACCESSIBLE,
      sizeof (OEM_EC_DESC_TPM_INACCESSIBLE)
      );

    return EFI_DEVICE_ERROR;
  }

  if (PcdGet8 (PcdTpm2InitializationPolicy) == 1) {
    DEBUG ((DEBUG_INFO, "%a: TPM Startup CLEAR\n", __FUNCTION__));
    Status = Tpm2Startup (TPM_SU_CLEAR);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: TPM Startup CLEAR failed - %r\n", __FUNCTION__, Status));
      return EFI_DEVICE_ERROR;
    }
  }

  //
  // Run self-test to check if TPM is working, if not, disable TPM
  //
  if (PcdGet8 (PcdTpm2SelfTestPolicy) == 1) {
    Status = Tpm2SelfTest (NO);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: TPM self-test failed - %r\n", __FUNCTION__, Status));
      REPORT_STATUS_CODE_WITH_EXTENDED_DATA (
        EFI_ERROR_CODE | EFI_ERROR_MAJOR,
        EFI_CLASS_NV_FIRMWARE | EFI_NV_FW_UEFI_EC_TPM_SELF_TEST_FAILED,
        OEM_EC_DESC_TPM_SELF_TEST_FAILED,
        sizeof (OEM_EC_DESC_TPM_SELF_TEST_FAILED)
        );
      return Status;
    }
  }

  DEBUG ((DEBUG_INFO, "%a: TPM initialized successfully via FF-A CRB\n", __FUNCTION__));

  return EFI_SUCCESS;
}

/**
  Constructor for TPM2 device library using FF-A CRB interface.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The operation completed successfully.
**/
EFI_STATUS
EFIAPI
Tpm2DeviceLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "%a: Initializing TPM2 Device Library with CRB over FF-A\n", __FUNCTION__));

  //
  // Check if FF-A is supported
  //
  if (!IsFfaSupported ()) {
    DEBUG ((DEBUG_ERROR, "%a: FF-A not supported\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  //
  // Try to get the TPM service partition ID
  //
  Status = GetTpmServicePartitionId ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: TPM service not available - %r\n", __FUNCTION__, Status));
    return EFI_SUCCESS;
  }

  //
  // Try to get the CRB buffer address from StandaloneMM
  //
  Status = GetCrbBufferAddress ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get CRB buffer: %r\n", __FUNCTION__, Status));
    return EFI_SUCCESS;
  }

  mLibraryAvailable = TRUE;

  //
  // Initialize TPM
  //
  Status = Tpm2Initialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: TPM initialization failed: %r\n", __FUNCTION__, Status));
    Status = DisableStmmCrbFfaService ();
    ASSERT_EFI_ERROR (Status);
    mLibraryAvailable = FALSE;
    // This error is non-fatal. Return success to prevent TcgDxe Autogen ASSERT.
    return EFI_SUCCESS;
  }

  //
  // Set interface type to CRB
  //
  PcdSet8S (PcdActiveTpmInterfaceType, Tpm2PtpInterfaceCrb);

  return EFI_SUCCESS;
}
