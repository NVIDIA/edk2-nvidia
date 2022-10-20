/** @file

  Macronix ASP (Advanced Sector Protection) implementation

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Protocol/QspiController.h>
#include <MacronixAsp.h>

#define CMD_RDSCUR  0x2B
#define CMD_WPSEL   0x68
#define CMD_GBULK   0x98
#define CMD_RDSPB   0xE2
#define CMD_WRSPB   0xE3
#define WPSEL       (1 << 7)

#define QSPIPERFORMTRANSACTION(x)  QspiPerformTransaction((EFI_PHYSICAL_ADDRESS)QspiBaseAddress, x);

STATIC UINT64   QspiBaseAddress = 0;
STATIC BOOLEAN  AspInitialized  = FALSE;
STATIC UINT8    ChipSelect      = 0;

STATIC EFI_STATUS
MxReadRegister (
  IN  UINT8   *Cmd,
  IN  UINT32  CmdSize,
  OUT UINT8   *Resp
  )
{
  EFI_STATUS               Status;
  QSPI_TRANSACTION_PACKET  Packet;

  if ((Cmd == NULL) || (Resp == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (QspiBaseAddress == 0) {
    return EFI_NOT_READY;
  }

  Packet.TxBuf      = Cmd;
  Packet.RxBuf      = Resp;
  Packet.TxLen      = CmdSize;
  Packet.RxLen      = sizeof (UINT8);
  Packet.WaitCycles = 0;
  Packet.ChipSelect = ChipSelect;

  Status = QSPIPERFORMTRANSACTION (&Packet);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Could not read register: 0x%x (%r).\n",
      __FUNCTION__,
      *Cmd,
      Status
      ));
  }

  return Status;
}

STATIC EFI_STATUS
MxPollingBit (
  IN UINT8   *Cmd,
  IN UINT32  CmdSize,
  IN UINTN   BitMask,
  IN UINT8   Expect,
  IN UINTN   RetryCount
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT8       Reply;
  UINT32      Count;

  if (Cmd == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (QspiBaseAddress == 0) {
    return EFI_NOT_READY;
  }

  Reply = 0;
  Count = 0;
  do {
    Status = MxReadRegister (Cmd, CmdSize, &Reply);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Could not read NOR flash status 1 register.\n", __FUNCTION__));
      break;
    }

    Count++;
    if (Count == RetryCount) {
      DEBUG ((DEBUG_ERROR, "%a: NOR flash polling bit slower than usual.\n", __FUNCTION__));
      Status = EFI_TIMEOUT;
      break;
    }

    MicroSecondDelay (TIMEOUT);
  } while ((Reply & BitMask) != Expect);

  return Status;
}

STATIC EFI_STATUS
MxWriteRegister (
  IN  UINT8   *Cmd,
  IN  UINT32  CmdSize
  )
{
  EFI_STATUS               Status;
  UINT8                    Command;
  QSPI_TRANSACTION_PACKET  Packet;

  if (Cmd == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (QspiBaseAddress == 0) {
    return EFI_NOT_READY;
  }

  Command           = NOR_WREN_ENABLE;
  Packet.TxBuf      = &Command;
  Packet.RxBuf      = NULL;
  Packet.TxLen      = sizeof (Command);
  Packet.RxLen      = 0;
  Packet.WaitCycles = 0;
  Packet.ChipSelect = ChipSelect;
  Status            = QSPIPERFORMTRANSACTION (&Packet);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Could not write WREN (%r).\n", __FUNCTION__, Status));
    goto exit;
  }

  Command = NOR_READ_SR1;
  Status  = MxPollingBit (&Command, sizeof (Command), NOR_SR1_WEL_BMSK, NOR_SR1_WEL_BMSK, NOR_SR1_WEL_RETRY_CNT);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Polling write enable latch failed (%r).\n", __FUNCTION__, Status));
    goto exit;
  }

  Packet.TxBuf      = Cmd;
  Packet.RxBuf      = NULL;
  Packet.TxLen      = CmdSize;
  Packet.RxLen      = 0;
  Packet.WaitCycles = 0;
  Packet.ChipSelect = ChipSelect;
  Status            = QSPIPERFORMTRANSACTION (&Packet);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Could not write register 0x%x (%r).\n",
      __FUNCTION__,
      *Cmd,
      Status
      ));
    goto exit;
  }

  Command = NOR_READ_SR1;
  Status  = MxPollingBit (&Command, sizeof (Command), NOR_SR1_WIP_BMSK, 0, NOR_SR1_WIP_RETRY_CNT);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Polling write in progress failed (%r).\n", __FUNCTION__, Status));
    goto exit;
  }

exit:
  return Status;
}

EFI_STATUS
MxAspInitialize (
  UINT64  QspiBase,
  UINT8   FlashCS
  )
{
  QspiBaseAddress = QspiBase;
  AspInitialized  = TRUE;
  ChipSelect      = FlashCS;
  return EFI_SUCCESS;
}

EFI_STATUS
MxAspIsInitialized (
  BOOLEAN  *Initialized
  )
{
  if (Initialized == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *Initialized = AspInitialized;
  return EFI_SUCCESS;
}

EFI_STATUS
MxAspIsEnabled (
  BOOLEAN  *Enabled
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT8       Cmd;
  UINT8       Reply;

  if (Enabled == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Cmd    = CMD_RDSCUR;
  Reply  = 0;
  Status = MxReadRegister (&Cmd, sizeof (Cmd), &Reply);
  if (EFI_ERROR (Status)) {
    goto exit;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Got security register value: 0x%x\n",
    __FUNCTION__,
    Reply
    ));
  *Enabled = (Reply & WPSEL) ? TRUE : FALSE;

exit:
  return Status;
}

EFI_STATUS
MxAspEnable (
  VOID
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT8       Cmd;
  UINT8       Reply;

  Cmd    = CMD_RDSCUR;
  Reply  = 0;
  Status = MxReadRegister (&Cmd, sizeof (Cmd), &Reply);
  if (EFI_ERROR (Status)) {
    goto exit;
  }

  DEBUG ((DEBUG_INFO, "%a: security register value: 0x%x\n", __FUNCTION__, Reply));
  if ((Reply & WPSEL) == 0) {
    // ASP is not enabled yet, will burn WPSEL (OTP bit)
    DEBUG ((DEBUG_INFO, "Start burning Macronix WPSEL OTP bit.\n"));
    Cmd    = CMD_WPSEL;
    Status = MxWriteRegister (&Cmd, sizeof (Cmd));
    if (EFI_ERROR (Status)) {
      goto exit;
    }

    Cmd    = CMD_RDSCUR;
    Reply  = 0;
    Status = MxReadRegister (&Cmd, sizeof (Cmd), &Reply);
    if (EFI_ERROR (Status)) {
      goto exit;
    }

    if ((Reply & WPSEL) == 0) {
      DEBUG ((DEBUG_ERROR, "WPSEL burning failed.\n"));
      goto exit;
    }

    DEBUG ((DEBUG_INFO, "Macronix WPSEL OTP bit has been burned. ASP is enabled.\n"));
  }

  // Clear all DPBs
  Cmd    = CMD_GBULK;
  Status = MxWriteRegister (&Cmd, sizeof (Cmd));
  if (EFI_ERROR (Status)) {
    goto exit;
  }

  DEBUG ((DEBUG_INFO, "Macronix all DPBs have been cleared.\n"));

exit:
  return Status;
}

EFI_STATUS
MxAspLock (
  UINT32  Address
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT8       LockCmd[5];

  LockCmd[0] = CMD_WRSPB;
  LockCmd[1] = (Address & 0xFF000000) >> 24;
  LockCmd[2] = (Address & 0xFF0000) >> 16;
  LockCmd[3] = (Address & 0xFF00) >> 8;
  LockCmd[4] = Address & 0xFF;
  Status     = MxWriteRegister (LockCmd, sizeof (LockCmd));
  if (EFI_ERROR (Status)) {
    goto exit;
  }

exit:
  return Status;
}

EFI_STATUS
MxAspIsLocked (
  UINT32   Address,
  BOOLEAN  *Locked
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT8       Cmd[5];
  UINT8       Reply;

  if (Locked == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Cmd[0] = CMD_RDSPB;
  Cmd[1] = (Address & 0xFF000000) >> 24;
  Cmd[2] = (Address & 0xFF0000) >> 16;
  Cmd[3] = (Address & 0xFF00) >> 8;
  Cmd[4] = Address & 0xFF;
  Reply  = 0;
  Status = MxReadRegister (Cmd, sizeof (Cmd), &Reply);
  if (EFI_ERROR (Status)) {
    goto exit;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Check address 0x%x lock status: 0x%x\n",
    __FUNCTION__,
    Address,
    Reply
    ));
  *Locked = (Reply == 0xFF) ? TRUE : FALSE;

exit:
  return Status;
}
