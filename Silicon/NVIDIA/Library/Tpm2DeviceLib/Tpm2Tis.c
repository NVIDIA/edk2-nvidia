/** @file

  TIS (TPM Interface Specification) functions used by TPM2.0 library.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2015 Hewlett Packard Enterprise Development LP<BR>
  Copyright (c) 2022 - 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/TimerLib.h>
#include <Library/Tpm2DeviceLib.h>

#include <IndustryStandard/Tpm20.h>
#include <IndustryStandard/TpmTis.h>

#include "Tpm2DeviceLibInternal.h"

#define TIS_TIMEOUT_MAX  (90000 * 1000)     // 90s
#define TIS_POLL_DELAY   30

#define TIS_INVALID_VALUE  0xFF

/**
  Read one byte from TPM

  @param  Tpm2         pointer to NVIDIA_TPM2_PROTOCOL
  @param  Addr         TPM register address to read from

  @retval Read-back value
**/
UINT8
TisRead8 (
  IN NVIDIA_TPM2_PROTOCOL  *Tpm2,
  IN UINT16                Addr
  )
{
  EFI_STATUS  Status;
  UINT8       Value;

  Status = Tpm2->Transfer (Tpm2, TRUE, Addr, &Value, sizeof (Value));
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    Value = TIS_INVALID_VALUE;
  }

  return Value;
}

/**
  Write one byte to TPM

  @param  Tpm2         pointer to NVIDIA_TPM2_PROTOCOL
  @param  Addr         TPM register address to write to
  @param  Addr         TPM register address to write to
**/
VOID
TisWrite8 (
  IN NVIDIA_TPM2_PROTOCOL  *Tpm2,
  IN UINT16                Addr,
  IN UINT8                 Value
  )
{
  EFI_STATUS  Status;

  Status = Tpm2->Transfer (Tpm2, FALSE, Addr, &Value, sizeof (Value));
  ASSERT_EFI_ERROR (Status);
}

/**
  Check whether TPM chip exist.

  @param  Tpm2      pointer to NVIDIA_TPM2_PROTOCOL

  @retval TRUE      TPM chip exists.
  @retval FALSE     TPM chip is not found.
**/
BOOLEAN
TisPresenceCheck (
  IN NVIDIA_TPM2_PROTOCOL  *Tpm2
  )
{
  UINT8  RegRead;

  RegRead = TisRead8 (Tpm2, TPM_ACCESS_0);
  return (RegRead != TIS_INVALID_VALUE);
}

/**
  Check whether the value of a TPM chip register satisfies the input BIT setting.

  @param[in]  Tpm2         pointer to NVIDIA_TPM2_PROTOCOL
  @param[in]  Addr         Address port of register to be checked.
  @param[in]  BitSet       Check these data bits are set.
  @param[in]  BitClear     Check these data bits are clear.
  @param[in]  TimeOut      The max wait time (unit MicroSecond) when checking register.

  @retval     EFI_SUCCESS  The register satisfies the check bit.
  @retval     EFI_TIMEOUT  The register can't run into the expected status in time.
**/
EFI_STATUS
TisWaitRegisterBits (
  IN NVIDIA_TPM2_PROTOCOL  *Tpm2,
  IN UINT16                Addr,
  IN UINT8                 BitSet,
  IN UINT8                 BitClear,
  IN UINT32                TimeOut
  )
{
  UINT8   RegRead;
  UINT32  WaitTime;

  for (WaitTime = 0; WaitTime < TimeOut; WaitTime += TIS_POLL_DELAY) {
    RegRead = TisRead8 (Tpm2, Addr);
    if (((RegRead & BitSet) == BitSet) && ((RegRead & BitClear) == 0)) {
      return EFI_SUCCESS;
    }

    MicroSecondDelay (TIS_POLL_DELAY);
  }

  return EFI_TIMEOUT;
}

/**
  Get BurstCount by reading the burstCount field of a TIS register
  in the time of default TIS_TIMEOUT_D.

  @param[in]  Tpm2                  pointer to NVIDIA_TPM2_PROTOCOL
  @param[out] BurstCount            Pointer to a buffer to store the got BurstCount.

  @retval     EFI_SUCCESS           Get BurstCount.
  @retval     EFI_TIMEOUT           BurstCount can't be got in time.
**/
EFI_STATUS
TisReadBurstCount (
  IN   NVIDIA_TPM2_PROTOCOL  *Tpm2,
  OUT  UINT16                *BurstCount
  )
{
  EFI_STATUS  Status;
  UINT32      WaitTime;
  UINT8       StsReg[4];

  ASSERT (BurstCount != NULL);

  WaitTime = 0;
  do {
    Status = Tpm2->Transfer (Tpm2, TRUE, TPM_STS_0, StsReg, sizeof (StsReg));
    ASSERT_EFI_ERROR (Status);

    *BurstCount = (UINT16)((StsReg[2] << 8) | StsReg[1]);
    if (*BurstCount != 0) {
      return EFI_SUCCESS;
    }

    MicroSecondDelay (TIS_POLL_DELAY);
    WaitTime += TIS_POLL_DELAY;
  } while (WaitTime < TIS_TIMEOUT_D);

  return EFI_TIMEOUT;
}

/**
  Set TPM chip to ready state by sending ready command TIS_PC_STS_READY
  to Status Register in time.

  @param[in] Tpm2          pointer to NVIDIA_TPM2_PROTOCOL

  @retval    EFI_SUCCESS   TPM chip enters into ready state.
  @retval    EFI_TIMEOUT   TPM chip can't be set to ready state in time.
**/
EFI_STATUS
TisPrepareCommand (
  IN NVIDIA_TPM2_PROTOCOL  *Tpm2
  )
{
  EFI_STATUS  Status;

  TisWrite8 (Tpm2, TPM_STS_0, TIS_PC_STS_READY);
  Status = TisWaitRegisterBits (Tpm2, TPM_STS_0, TIS_PC_STS_READY, 0, TIS_TIMEOUT_B);
  return Status;
}

/**
  Get the control of TPM chip by sending requestUse command TIS_PC_ACC_RQUUSE
  to ACCESS Register in the time of default TIS_TIMEOUT_A.

  @param[in] Tpm2            pointer to NVIDIA_TPM2_PROTOCOL

  @retval    EFI_SUCCESS     Get the control of TPM chip.
  @retval    EFI_NOT_FOUND   TPM chip doesn't exit.
  @retval    EFI_TIMEOUT     Can't get the TPM control in time.
**/
EFI_STATUS
TisRequestUseTpm (
  IN NVIDIA_TPM2_PROTOCOL  *Tpm2
  )
{
  EFI_STATUS  Status;

  ASSERT (Tpm2 != NULL);

  if (!TisPresenceCheck (Tpm2)) {
    return EFI_NOT_FOUND;
  }

  TisWrite8 (Tpm2, TPM_ACCESS_0, TIS_PC_ACC_RQUUSE);
  Status = TisWaitRegisterBits (
             Tpm2,
             TPM_ACCESS_0,
             (UINT8)(TIS_PC_ACC_ACTIVE | TIS_PC_VALID),
             0,
             TIS_TIMEOUT_A
             );
  return Status;
}

/**
  Send a command to TPM for execution and return response data.

  @param[in]      Tpm2          pointer to NVIDIA_TPM2_PROTOCOL
  @param[in]      BufferIn      Buffer for command data.
  @param[in]      SizeIn        Size of command data.
  @param[in, out] BufferOut     Buffer for response data.
  @param[in, out] SizeOut       Size of response data.

  @retval EFI_SUCCESS           Operation completed successfully.
  @retval EFI_BUFFER_TOO_SMALL  Response data buffer is too small.
  @retval EFI_DEVICE_ERROR      Unexpected device behavior.
  @retval EFI_UNSUPPORTED       Unsupported TPM version
**/
EFI_STATUS
TisTpmCommand (
  IN     NVIDIA_TPM2_PROTOCOL  *Tpm2,
  IN     UINT8                 *BufferIn,
  IN     UINT32                SizeIn,
  IN OUT UINT8                 *BufferOut,
  IN OUT UINT32                *SizeOut
  )
{
  EFI_STATUS  Status;
  UINT16      BurstCount;
  UINT32      Index;
  UINT32      TpmOutSize = 0;
  UINT16      Data16;
  UINT32      Data32;
  UINT16      TransferSize;

  ASSERT (Tpm2  != NULL);
  ASSERT (BufferIn  != NULL);
  ASSERT (BufferOut != NULL);
  ASSERT (SizeIn   >= sizeof (TPM2_COMMAND_HEADER));
  ASSERT (*SizeOut >= sizeof (TPM2_RESPONSE_HEADER));

  DEBUG_CODE_BEGIN ();
  UINTN  DebugSize;

  DEBUG ((DEBUG_VERBOSE, "Tpm2TisTpmCommand Send - "));
  if (SizeIn > 0x100) {
    DebugSize = 0x40;
  } else {
    DebugSize = SizeIn;
  }

  for (Index = 0; Index < DebugSize; Index++) {
    DEBUG ((DEBUG_VERBOSE, "%02x ", BufferIn[Index]));
  }

  if (DebugSize != SizeIn) {
    DEBUG ((DEBUG_VERBOSE, "...... "));
    for (Index = SizeIn - 0x20; Index < SizeIn; Index++) {
      DEBUG ((DEBUG_VERBOSE, "%02x ", BufferIn[Index]));
    }
  }

  DEBUG ((DEBUG_VERBOSE, "\n"));
  DEBUG_CODE_END ();

  Status = TisPrepareCommand (Tpm2);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Tpm2 is not ready for command!\n"));
    return EFI_DEVICE_ERROR;
  }

  //
  // Send the command data to Tpm
  //
  Index = 0;
  while (Index < SizeIn) {
    Status = TisReadBurstCount (Tpm2, &BurstCount);
    if (EFI_ERROR (Status)) {
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }

    TransferSize = MIN (TPM_MAX_TRANSFER_SIZE, MIN (BurstCount, (SizeIn - Index)));
    Status       = Tpm2->Transfer (
                           Tpm2,
                           FALSE,
                           TPM_DATA_FIFO_0,
                           BufferIn + Index,
                           TransferSize
                           );
    if (EFI_ERROR (Status)) {
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }

    Index += TransferSize;
  }

  //
  // Check the Tpm status STS_EXPECT change from 1 to 0
  //
  Status = TisWaitRegisterBits (
             Tpm2,
             TPM_STS_0,
             (UINT8)TIS_PC_VALID,
             TIS_PC_STS_EXPECT,
             TIS_TIMEOUT_C
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Tpm2 STS_EXPECT timeout. TPM failed to receive command.\n"));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  //
  // Executed the TPM command and waiting for the response data ready
  //
  TisWrite8 (Tpm2, TPM_STS_0, TIS_PC_STS_GO);

  //
  // NOTE: That may take many seconds to minutes for certain commands, such as key generation.
  //
  Status = TisWaitRegisterBits (
             Tpm2,
             TPM_STS_0,
             (UINT8)(TIS_PC_VALID | TIS_PC_STS_DATA),
             0,
             TIS_TIMEOUT_MAX
             );
  if (EFI_ERROR (Status)) {
    //
    // dataAvail check timeout. Cancel the currently executing command by writing commandCancel,
    // Expect TPM_RC_CANCELLED or successfully completed response.
    //
    DEBUG ((DEBUG_ERROR, "Wait for Tpm2 response data time out. Trying to cancel the command!!\n"));

    TisWrite8 (Tpm2, TPM_STS_MISC_0, TIS_PC_STS_MISC_CANCEL);
    Status = TisWaitRegisterBits (
               Tpm2,
               TPM_STS_0,
               (UINT8)(TIS_PC_VALID | TIS_PC_STS_DATA),
               0,
               TIS_TIMEOUT_B
               );
    //
    // Do not clear CANCEL bit here because Writes of 0 to this bit are ignored
    //
    if (EFI_ERROR (Status)) {
      //
      // Cancel executing command fail to get any response
      // Try to abort the command with write of a 1 to commandReady in Command Execution state
      //
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }
  }

  //
  // Get response data header
  //
  Index      = 0;
  BurstCount = 0;
  while (Index < sizeof (TPM2_RESPONSE_HEADER)) {
    Status = TisReadBurstCount (Tpm2, &BurstCount);
    if (EFI_ERROR (Status)) {
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }

    if (*SizeOut < (Index + BurstCount)) {
      Status = EFI_BUFFER_TOO_SMALL;
      goto Exit;
    }

    TransferSize = MIN (TPM_MAX_TRANSFER_SIZE, BurstCount);
    Status       = Tpm2->Transfer (
                           Tpm2,
                           TRUE,
                           TPM_DATA_FIFO_0,
                           BufferOut + Index,
                           TransferSize
                           );
    if (EFI_ERROR (Status)) {
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }

    Index += TransferSize;
  }

  //
  // Check the response data header (tag,parasize and returncode )
  //
  CopyMem (&Data16, BufferOut, sizeof (UINT16));
  // TPM2 should not use this RSP_COMMAND
  if (SwapBytes16 (Data16) == TPM_ST_RSP_COMMAND) {
    DEBUG ((DEBUG_ERROR, "TPM2: TPM_ST_RSP error - %x\n", TPM_ST_RSP_COMMAND));
    Status = EFI_UNSUPPORTED;
    goto Exit;
  }

  CopyMem (&Data32, (BufferOut + 2), sizeof (UINT32));
  TpmOutSize = SwapBytes32 (Data32);
  if (*SizeOut < TpmOutSize) {
    Status = EFI_BUFFER_TOO_SMALL;
    goto Exit;
  }

  *SizeOut = TpmOutSize;
  //
  // Continue reading the remaining data
  //
  while (Index < TpmOutSize) {
    Status = TisReadBurstCount (Tpm2, &BurstCount);
    if (EFI_ERROR (Status)) {
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }

    ASSERT (*SizeOut >= (Index + BurstCount));

    TransferSize = MIN (TPM_MAX_TRANSFER_SIZE, BurstCount);
    Status       = Tpm2->Transfer (
                           Tpm2,
                           TRUE,
                           TPM_DATA_FIFO_0,
                           BufferOut + Index,
                           TransferSize
                           );
    if (EFI_ERROR (Status)) {
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }

    Index += TransferSize;
  }

Exit:
  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_VERBOSE, "Tpm2TisTpmCommand Receive - "));
  for (Index = 0; Index < TpmOutSize; Index++) {
    DEBUG ((DEBUG_VERBOSE, "%02x ", BufferOut[Index]));
  }

  DEBUG ((DEBUG_VERBOSE, "\n"));
  DEBUG_CODE_END ();

  TisWrite8 (Tpm2, TPM_STS_0, TIS_PC_STS_READY);
  return Status;
}

/**
  Release the control of TPM chip

  @param[in] Tpm2             Pointer to NVIDIA_TPM2_PROTOCOL
**/
VOID
TisReleaseTpm (
  IN NVIDIA_TPM2_PROTOCOL  *Tpm2
  )
{
  ASSERT (Tpm2 != NULL);

  //
  // According to TIS spec, software relinquishes the TPM’s locality by
  // writing a “1” to the TPM_ACCESS_x.activeLocality field.
  //
  TisWrite8 (Tpm2, TPM_ACCESS_0, TIS_PC_ACC_ACTIVE);
}
