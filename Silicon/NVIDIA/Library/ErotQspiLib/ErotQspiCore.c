/** @file

  Erot Qspi library core routines

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/ErotQspiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Protocol/QspiController.h>
#include "ErotQspiCore.h"

// register addresses
#define EROT_REG_SPI_CONFIG            0x00
#define EROT_REG_SPI_STATUS            0x04
#define EROT_REG_SPI_EROT_STATUS       0x08
#define EROT_REG_SPI_INTERRUPT_ENABLE  0x0C
#define EROT_REG_EROT_MBOX             0x44
#define EROT_REG_HOST_MBOX             0x48

// register values
#define EROT_SPI_STATUS_MEM_WRITE_DONE  0x01
#define EROT_SPI_STATUS_MEM_READ_DONE   0x02

#define EROT_HOST_MBOX_MASK           0x1f0000ff
#define EROT_HOST_MBOX_LENGTH_MASK    0x000000ff
#define EROT_HOST_MBOX_CMD_MASK       0x0f000000
#define EROT_HOST_MBOX_MSG_AVAILABLE  0x10000000
#define EROT_HOST_MBOX_CMD_ACK        0x01000000

#define EROT_MBOX_CMD_MASK           0x0f000000
#define EROT_MBOX_CMD_REQUEST_WRITE  0x02000000
#define EROT_MBOX_CMD_READY_TO_READ  0x03000000
#define EROT_MBOX_CMD_FINISHED_READ  0x04000000
#define EROT_MBOX_CMD_REQUEST_RESET  0x05000000
#define EROT_MBOX_LENGTH_MASK        0x000000ff

#define EROT_POLL_ALL_MEM_WRITE_BUSY     0x0008
#define EROT_POLL_ALL_SREG_BUSY          0x0010
#define EROT_POLL_ALL_TX_FIFO_NOT_EMPTY  0x0400
#define EROT_POLL_ALL_RX_FIFO_EMPTY      0x0100

#define EROT_RX_MEM_START  0x0000
#define EROT_TX_MEM_START  0x8000

#define EROT_MEM_MAX_BYTES_PER_XFER     32
#define EROT_MEM_BYTES_PER_SINGLE_READ  4
#define EROT_MEM_BLOCK_SIZE             4

// commands
#define EROT_CMD_SREG_W8   0x09
#define EROT_CMD_SREG_W32  0x0b
#define EROT_CMD_SREG_R8   0x0d
#define EROT_CMD_SREG_R32  0x0f

#define EROT_CMD_MEM_W8      0x21
#define EROT_CMD_MEM_R8      0x25
#define EROT_CMD_MEM_BLK_W1  0x80
#define EROT_CMD_MEM_BLK_R1  0xA0

#define EROT_CMD_BLK_RD_FIFO_FSR    0xE0
#define EROT_CMD_RD_SNGL_FIFO8_FSR  0x68

#define EROT_CMD_GET_POLL_ALL  0x2F

// message field definitions
#define EROT_QSPI_SET_CFG_MODE_SINGLE  0
#define EROT_QSPI_SET_CFG_MODE_QUAD    1

// timeouts
#define EROT_HOST_MBOX_POLL_MSG_TIMEOUT_MS      0
#define EROT_HOST_MBOX_POLL_DEFAULT_TIMEOUT_MS  100
#define EROT_HOST_MBOX_POLL_LENGTH_TIMEOUT_MS   100
#define EROT_SREG_BUSY_TIMEOUT_MS               100
#define EROT_MEM_BUSY_TIMEOUT_MS                100

#define EROT_WAIT_CYCLES      0
#define EROT_TAR_CYCLES       1
#define EROT_TAR_WAIT_CYCLES  (EROT_WAIT_CYCLES + EROT_TAR_CYCLES)

#pragma pack(1)

typedef struct {
  UINT8    Cmd;
  UINT8    Addr[2];
} EROT_QSPI_SREG_READ8_TX;

typedef struct {
  UINT8    Status[2];
  UINT8    Data;
} EROT_QSPI_SREG_READ8_RX;

typedef struct {
  UINT8    Cmd;
  UINT8    Addr[2];
  UINT8    Data;
} EROT_QSPI_SREG_WRITE8_TX;

typedef struct {
  UINT8    Status[2];
} EROT_QSPI_SREG_WRITE8_RX;

typedef struct {
  UINT8    Cmd;
  UINT8    Addr[2];
} EROT_QSPI_SREG_READ32_TX;

typedef struct {
  UINT8    Status[2];
  UINT8    Data[4];
} EROT_QSPI_SREG_READ32_RX;

typedef struct {
  UINT8    Cmd;
  UINT8    Addr[2];
  UINT8    Data[4];
} EROT_QSPI_SREG_WRITE32_TX;

typedef struct {
  UINT8    Status[2];
} EROT_QSPI_SREG_WRITE32_RX;

typedef struct {
  UINT8    Cmd;
} EROT_QSPI_GET_POLL_ALL_TX;

typedef struct {
  UINT8    Status[4];
} EROT_QSPI_GET_POLL_ALL_RX;

typedef struct {
  UINT8    Cmd;
  UINT8    Addr[2];
} EROT_QSPI_READ_MEM_TX;

typedef struct {
  UINT8    Cmd;
} EROT_QSPI_READ_FIFO_TX;

typedef struct {
  UINT8    Status[2];
  UINT8    Data[EROT_MEM_MAX_BYTES_PER_XFER];
} EROT_QSPI_READ_FIFO_RX;

typedef struct {
  UINT8    Cmd;
  UINT8    Addr[2];
  UINT8    Data[EROT_MEM_MAX_BYTES_PER_XFER];
} EROT_QSPI_WRITE_MEM_TX;

typedef struct {
  UINT8    Type;
  UINT8    Reserved;
  UINT8    WaitCycles;
  UINT8    Mode;
} EROT_QSPI_SET_CFG_MSG;

#pragma pack()

STATIC BOOLEAN  mErotQuadMode = FALSE;

STATIC
BOOLEAN
EFIAPI
ErotQspiGpioIsAsserted (
  IN EROT_QSPI_PRIVATE_DATA  *Private
  )
{
  EMBEDDED_GPIO  *Protocol;
  EFI_STATUS     Status;
  UINTN          GpioState;

  Protocol = Private->Gpio.Protocol;
  Status   = Protocol->Get (Protocol, Private->Gpio.Pin, &GpioState);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: gpio 0x%x get failed: %r\n", __FUNCTION__, Private->Gpio.Pin, Status));
    return FALSE;
  }

  return (GpioState != 1);
}

/**
  Copy and reverse bytes between buffers.  Last byte in src buffer becomes
  first byte in destination buffer.

  @param[out] DstBuffer     Pointer to destination buffer.
  @param[in]  SrcBuffer     Pointer to source buffer.
  @param[in]  Bytes         Number of bytes to copy and reverse.

  @retval None

**/
STATIC
VOID
EFIAPI
ErotQSpiCopyAndReverseBuffer (
  OUT UINT8        *DstBuffer,
  IN  CONST UINT8  *SrcBuffer,
  IN  UINTN        Bytes
  )
{
  UINTN  Index;

  for (Index = 0; Index < Bytes; Index++) {
    DstBuffer[Index] = SrcBuffer[Bytes - Index - 1];
  }
}

/**
  Perform a qspi transfer of Tx and Rx data with erot.

  @param[in]  Private       Pointer to private structure for erot.
  @param[in]  TxLength      Number of bytes to send from TxBuffer.
  @param[in]  TxBuffer      Pointer to transmit buffer.
  @param[in]  RxLength      Number of bytes to receive into RxBuffer.
  @param[in]  RxBuffer      Pointer to receive buffer.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiXfer (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN UINT32                  TxLength,
  IN CONST VOID              *TxBuffer,
  IN UINT32                  RxLength,
  IN VOID                    *RxBuffer
  )
{
  EFI_STATUS               Status;
  QSPI_TRANSACTION_PACKET  Packet;

  DEBUG ((DEBUG_VERBOSE, "%a: %s TxLength=%u, RxLength=%u\n", __FUNCTION__, Private->Name, TxLength, RxLength));

  ErotQspiPrintBuffer ("QspiTx", TxBuffer, TxLength);

  Packet.TxBuf      = (VOID *)TxBuffer;
  Packet.TxLen      = TxLength;
  Packet.RxBuf      = RxBuffer;
  Packet.RxLen      = RxLength;
  Packet.WaitCycles = (RxBuffer == NULL) ? 0 : EROT_TAR_WAIT_CYCLES * 8;
  Packet.ChipSelect = Private->ChipSelect;
  Packet.Control    = QSPI_CONTROLLER_CONTROL_FAST_MODE;

  Status = Private->Qspi->PerformTransaction (Private->Qspi, &Packet);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s Failed TxLen=%u, RxLen=%u: %r\n", __FUNCTION__, Private->Name, TxLength, RxLength, Status));
    return Status;
  }

  ErotQspiPrintBuffer ("QspiRx", RxBuffer, RxLength);

  return Status;
}

/**
  Get the PollAll register from the erot.

  @param[in]  Private       Pointer to private structure for erot.

  @retval UINT32            PollAll register value.

**/
STATIC
UINT32
EFIAPI
ErotQspiGetPollAll (
  IN EROT_QSPI_PRIVATE_DATA  *Private
  )
{
  EROT_QSPI_GET_POLL_ALL_TX  Tx;
  EROT_QSPI_GET_POLL_ALL_RX  Rx;
  EFI_STATUS                 Status;

  Tx.Cmd = EROT_CMD_GET_POLL_ALL;

  Status = ErotQspiXfer (Private, sizeof (Tx), &Tx, sizeof (Rx), &Rx);
  if (EFI_ERROR (Status)) {
    return 0;
  }

  return MctpBEBufferToUint32 (Rx.Status);
}

/**
  Poll until PollAll register has desired value or time out.

  @param[in]  Private       Pointer to private structure for erot.
  @param[in]  StatusBitMask Bitmask to apply to register.
  @param[in]  PollWhile     Value of masked register that causes poll to continue.
  @param[in]  TimeoutMs     Number of ms to poll before time out.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiPollForStatus (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN UINT32                  StatusBitMask,
  IN UINT32                  PollWhile,
  IN UINTN                   TimeoutMs
  )
{
  UINT32  Reg;
  UINT64  EndNs;

  EndNs = ErotQspiNsCounter () + EROT_QSPI_MS_TO_NS (TimeoutMs);

  while (((Reg = ErotQspiGetPollAll (Private)) & StatusBitMask) == PollWhile) {
    if (ErotQspiNsCounter () >= EndNs) {
      DEBUG ((DEBUG_ERROR, "%a: Timeout Reg=0x%x mask=0x%x while=0x%x\n", __FUNCTION__, Reg, StatusBitMask, PollWhile));
      return EFI_TIMEOUT;
    }
  }

  return EFI_SUCCESS;
}

/**
  Check Sreg status value.

  @param[in]  SregStatus    Status value to check.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiSregCheckStatus (
  IN  UINT16  SregStatus
  )
{
  DEBUG ((DEBUG_VERBOSE, "%a: SregStatus=%u\n", __FUNCTION__, SregStatus));

  return EFI_SUCCESS;
}

/**
  Read 8-bit Sreg.

  @param[in]  Private       Pointer to private structure for erot.
  @param[in]  Addr          Sreg address.
  @param[out] Value         Pointer to return register value.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
EFI_STATUS
EFIAPI
ErotQspiSregRead8 (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN UINT16                  Addr,
  OUT UINT8                  *Value
  )
{
  EROT_QSPI_SREG_READ8_TX  Tx;
  EROT_QSPI_SREG_READ8_RX  Rx;
  EFI_STATUS               Status;

  Status = ErotQspiPollForStatus (
             Private,
             EROT_POLL_ALL_SREG_BUSY,
             EROT_POLL_ALL_SREG_BUSY,
             EROT_SREG_BUSY_TIMEOUT_MS
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Tx.Cmd = EROT_CMD_SREG_R8;
  MctpUint16ToBEBuffer (Tx.Addr, Addr);

  Status = ErotQspiXfer (Private, sizeof (Tx), &Tx, sizeof (Rx), &Rx);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *Value = Rx.Data;

  return ErotQspiSregCheckStatus (MctpBEBufferToUint16 (Rx.Status));
}

/**
  Write 8-bit Sreg.

  @param[in]  Private       Pointer to private structure for erot.
  @param[in]  Addr          Sreg address.
  @param[in ] Value         Value to write.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiSregWrite8 (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN UINT16                  Addr,
  IN UINT8                   Value
  )
{
  EROT_QSPI_SREG_WRITE8_TX  Tx;
  EROT_QSPI_SREG_WRITE8_RX  Rx;
  EFI_STATUS                Status;

  Status = ErotQspiPollForStatus (
             Private,
             EROT_POLL_ALL_SREG_BUSY,
             EROT_POLL_ALL_SREG_BUSY,
             EROT_SREG_BUSY_TIMEOUT_MS
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Tx.Cmd = EROT_CMD_SREG_W8;
  MctpUint16ToBEBuffer (Tx.Addr, Addr);
  Tx.Data = Value;

  Status = ErotQspiXfer (Private, sizeof (Tx), &Tx, sizeof (Rx), &Rx);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return ErotQspiSregCheckStatus (MctpBEBufferToUint16 (Rx.Status));
}

/**
  Read 32-bit Sreg.

  @param[in]  Private       Pointer to private structure for erot.
  @param[in]  Addr          Sreg address.
  @param[out] Value         Pointer to return register value.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiSregRead32 (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN UINT16                  Addr,
  OUT UINT32                 *Value
  )
{
  EROT_QSPI_SREG_READ32_TX  Tx;
  EROT_QSPI_SREG_READ32_RX  Rx;
  EFI_STATUS                Status;

  Status = ErotQspiPollForStatus (
             Private,
             EROT_POLL_ALL_SREG_BUSY,
             EROT_POLL_ALL_SREG_BUSY,
             EROT_SREG_BUSY_TIMEOUT_MS
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Tx.Cmd = EROT_CMD_SREG_R32;
  MctpUint16ToBEBuffer (Tx.Addr, Addr);

  Status = ErotQspiXfer (Private, sizeof (Tx), &Tx, sizeof (Rx), &Rx);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *Value = MctpBEBufferToUint32 (Rx.Data);

  return ErotQspiSregCheckStatus (MctpBEBufferToUint16 (Rx.Status));
}

/**
  Write 32-bit Sreg.

  @param[in]  Private       Pointer to private structure for erot.
  @param[in]  Addr          Sreg address.
  @param[out] Value         Value to write.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiSregWrite32 (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN UINT16                  Addr,
  IN UINT32                  Value
  )
{
  EROT_QSPI_SREG_WRITE32_TX  Tx;
  EROT_QSPI_SREG_WRITE32_RX  Rx;
  EFI_STATUS                 Status;

  Status = ErotQspiPollForStatus (
             Private,
             EROT_POLL_ALL_SREG_BUSY,
             EROT_POLL_ALL_SREG_BUSY,
             EROT_SREG_BUSY_TIMEOUT_MS
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Tx.Cmd = EROT_CMD_SREG_W32;
  MctpUint16ToBEBuffer (Tx.Addr, Addr);
  MctpUint32ToBEBuffer (Tx.Data, Value);

  Status = ErotQspiXfer (Private, sizeof (Tx), &Tx, sizeof (Rx), &Rx);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return ErotQspiSregCheckStatus (MctpBEBufferToUint16 (Rx.Status));
}

/**
  Decode host mailbox value.

  @param[in] Value          Value to decode.

  @retval CHAR8 *           String with decoded value.

**/
CONST CHAR8 *
EFIAPI
ErotQspiDecodeHostMbox (
  IN UINT32  Value
  )
{
  STATIC CHAR8  Str[64];
  CONST CHAR8   *Cmd;

  Str[0] = '\0';
  switch (Value & EROT_HOST_MBOX_CMD_MASK) {
    case EROT_HOST_MBOX_CMD_ACK:  Cmd = "ACK ";
      break;
    default:      Cmd = "";
      break;
  }

  AsciiStrCatS (Str, sizeof (Str), Cmd);

  if (Value & EROT_HOST_MBOX_MSG_AVAILABLE) {
    AsciiStrCatS (Str, sizeof (Str), "MSG_AVAILABLE ");
  }

  return Str;
}

/**
  Decode erot mailbox value.

  @param[in] Value          Value to decode.

  @retval CHAR8 *           String with decoded value.

**/
CONST CHAR8 *
EFIAPI
ErotQspiDecodeErotMbox (
  IN UINT32  Value
  )
{
  CONST CHAR8  *Cmd;

  switch (Value & EROT_MBOX_CMD_MASK) {
    case EROT_MBOX_CMD_REQUEST_WRITE: Cmd = "REQUEST_WRITE";
      break;
    case EROT_MBOX_CMD_READY_TO_READ: Cmd = "READY_TO_READ";
      break;
    case EROT_MBOX_CMD_FINISHED_READ: Cmd = "FINISHED_READ";
      break;
    case EROT_MBOX_CMD_REQUEST_RESET: Cmd = "REQUEST_RESET";
      break;
    default:                          Cmd = "<unknown>";
      break;
  }

  return Cmd;
}

/**
  Write erot mailbox.

  @param[in]  Private       Pointer to private structure for erot.
  @param[in]  Value         Value to write.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiWriteErotMbox (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN UINT32                  Value
  )
{
  EFI_STATUS  Status;

  Status = ErotQspiSregWrite32 (
             Private,
             EROT_REG_EROT_MBOX,
             Value
             );

  DEBUG ((DEBUG_VERBOSE, "%a: Mbox=0x%08x %a Status=%r\n", __FUNCTION__, Value, ErotQspiDecodeErotMbox (Value), Status));

  return Status;
}

/**
  Poll host mailbox.

  @param[in]  Private          Pointer to private structure for erot.
  @param[in]  PollLengthField  If TRUE, poll the host mailbox length byte.
                               If FALSE, poll for mailbox set to Cmd.
  @param[in]  Cmd              Mailbox command to poll for.
  @param[out] Length           Pointer to return length if PollLengthField TRUE. OPTIONAL

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiPollHostMbox (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN BOOLEAN                 PollLengthField,
  IN UINT32                  Cmd,
  OUT UINT8                  *Length     OPTIONAL
  )
{
  UINT64      EndNs;
  UINT32      Mbox;
  UINT32      Mask;
  UINT32      MaskedMbox;
  EFI_STATUS  Status;
  EFI_STATUS  MboxStatus;
  UINTN       PollMs;

  if (PollLengthField && (Length == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (PollLengthField) {
    Mask   = EROT_HOST_MBOX_LENGTH_MASK;
    PollMs = EROT_HOST_MBOX_POLL_LENGTH_TIMEOUT_MS;
  } else {
    Mask   = EROT_HOST_MBOX_CMD_MASK;
    PollMs = (Cmd == EROT_HOST_MBOX_MSG_AVAILABLE) ?
             EROT_HOST_MBOX_POLL_MSG_TIMEOUT_MS :
             EROT_HOST_MBOX_POLL_DEFAULT_TIMEOUT_MS;
  }

  EndNs  = ErotQspiNsCounter () + EROT_QSPI_MS_TO_NS (PollMs);
  Status = EFI_TIMEOUT;
  do {
    if (!ErotQspiGpioIsAsserted (Private)) {
      continue;
    }

    MboxStatus = ErotQspiSregRead32 (Private, EROT_REG_HOST_MBOX, &Mbox);
    if (EFI_ERROR (MboxStatus)) {
      DEBUG ((DEBUG_ERROR, "%a: read failed: %r\n", __FUNCTION__, Status));
      continue;
    }

    DEBUG ((DEBUG_VERBOSE, "%a: Mbox=0x%08x %a\n", __FUNCTION__, Mbox, ErotQspiDecodeHostMbox (Mbox)));

    if (Mbox & EROT_HOST_MBOX_MSG_AVAILABLE) {
      if (Cmd == EROT_HOST_MBOX_MSG_AVAILABLE) {
        return EFI_SUCCESS;
      }

      DEBUG ((DEBUG_VERBOSE, "%a: msg avail, Mbox=0x%x Cmd=0x%x\n", __FUNCTION__, Mbox, Cmd));

      Private->HasMessageAvailable = TRUE;
      // allow MSG_AVAILABLE to serve as ACK
      if (Cmd == EROT_HOST_MBOX_CMD_ACK) {
        return EFI_SUCCESS;
      }
    }

    MaskedMbox = Mbox & Mask;

    if (PollLengthField) {
      if (MaskedMbox != 0) {
        *Length = (UINT8)MaskedMbox;
        DEBUG ((DEBUG_VERBOSE, "%a: got Length Mbox=0x%x Mask=0x%x Len=0x%x\n", __FUNCTION__, Mbox, Mask, *Length));
        return EFI_SUCCESS;
      }

      continue;
    }

    if (MaskedMbox == Cmd) {
      DEBUG ((DEBUG_VERBOSE, "%a: got Cmd Mbox=0x%x Mask=0x%x Cmd=0x%x\n", __FUNCTION__, Mbox, Mask, Cmd));
      return EFI_SUCCESS;
    }
  } while (EFI_ERROR (Status) && (ErotQspiNsCounter () < EndNs));

  if (EFI_ERROR (Status) && (Cmd != EROT_HOST_MBOX_MSG_AVAILABLE)) {
    DEBUG ((DEBUG_ERROR, "%a: failed Mbox=0x%x, Cmd=0x%x PollLen=%u: %r\n", __FUNCTION__, Mbox, Cmd, PollLengthField, Status));
  }

  return Status;
}

/**
  Reset erot spb interface.

  @param[in]  Private       Pointer to private structure for erot.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiSpbReset (
  IN EROT_QSPI_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  Private->HasMessageAvailable = FALSE;

  Status = ErotQspiWriteErotMbox (Private, EROT_MBOX_CMD_REQUEST_RESET);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: reset write failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = ErotQspiPollHostMbox (
             Private,
             FALSE,
             EROT_HOST_MBOX_CMD_ACK,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ACK after reset failed: %r\n", __FUNCTION__, Status));
  }

  return Status;
}

/**
  Do erot write memory command.

  @param[in]  Private       Pointer to private structure for erot.
  @param[in]  Bytes         Number of bytes to send to erot.
  @param[in]  Buffer        Pointer to write memory command and data.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiDoWriteMemCommand (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN UINT32                  Bytes,
  IN CONST VOID              *Buffer
  )
{
  EFI_STATUS  Status;

  Status = ErotQspiPollForStatus (
             Private,
             EROT_POLL_ALL_MEM_WRITE_BUSY | EROT_POLL_ALL_RX_FIFO_EMPTY,
             0,
             EROT_MEM_BUSY_TIMEOUT_MS
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiXfer (Private, Bytes, Buffer, 0, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiSregWrite8 (Private, EROT_REG_SPI_STATUS, EROT_SPI_STATUS_MEM_WRITE_DONE);

  return Status;
}

/**
  Write erot memory.

  @param[in]  Private       Pointer to private structure for erot.
  @param[in]  Offset        Offset in erot Rx buffer to write.
  @param[in]  Bytes         Number of bytes to write.
  @param[in]  Data          Pointer to data to write.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiWriteMem (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN UINT32                  Offset,
  IN UINT32                  Bytes,
  IN CONST VOID              *Data
  )
{
  EROT_QSPI_WRITE_MEM_TX  Tx;
  CONST UINT8             *Payload;
  UINT32                  XferBytes;
  UINT32                  XferOffset;
  UINTN                   Index;

  ErotQspiPrintBuffer (__FUNCTION__, Data, Bytes);

  Payload    = (CONST UINT8 *)Data;
  XferOffset = 0;
  while (Bytes > 0) {
    XferBytes = (Bytes > EROT_MEM_MAX_BYTES_PER_XFER) ?
                EROT_MEM_MAX_BYTES_PER_XFER : Bytes;
    if (XferBytes >= EROT_MEM_BLOCK_SIZE) {
      XferBytes &= ~(EROT_MEM_BLOCK_SIZE - 1);
      Tx.Cmd     = EROT_CMD_MEM_BLK_W1 + (XferBytes / EROT_MEM_BLOCK_SIZE - 1);
      MctpUint16ToBEBuffer (Tx.Addr, Offset + XferOffset);
      for (Index = 0; Index < XferBytes; Index += EROT_MEM_BLOCK_SIZE) {
        ErotQSpiCopyAndReverseBuffer (&Tx.Data[Index], &Payload[XferOffset + Index], EROT_MEM_BLOCK_SIZE);
      }

      DEBUG ((DEBUG_VERBOSE, "%a: writing %u bytes\n", __FUNCTION__, XferBytes));
      ErotQspiDoWriteMemCommand (
        Private,
        OFFSET_OF (EROT_QSPI_WRITE_MEM_TX, Data) + XferBytes,
        &Tx
        );
    } else {
      for (Index = 0; Index < XferBytes; Index++) {
        DEBUG ((DEBUG_VERBOSE, "%a: writing single byte\n", __FUNCTION__));
        Tx.Cmd = EROT_CMD_MEM_W8;
        MctpUint16ToBEBuffer (Tx.Addr, Offset + XferOffset + Index);
        Tx.Data[0] = Payload[XferOffset + Index];
        ErotQspiDoWriteMemCommand (
          Private,
          OFFSET_OF (EROT_QSPI_WRITE_MEM_TX, Data) + 1,
          &Tx
          );
      }
    }

    XferOffset += XferBytes;
    Bytes      -= XferBytes;
  }

  return EFI_SUCCESS;
}

/**
  Do erot read memory command.

  @param[in]  Private       Pointer to private structure for erot.
  @param[in]  Bytes         Number of bytes to read from erot.
  @param[in]  Buffer        Pointer to store data from erot.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiDoReadMemCommand (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN UINT8                   Cmd1,
  IN UINT8                   Cmd2,
  IN UINT16                  Addr,
  IN UINT32                  Bytes,
  OUT VOID                   *Buffer
  )
{
  EROT_QSPI_READ_MEM_TX   ReadMemTx;
  EROT_QSPI_READ_FIFO_TX  ReadFifoTx;
  EROT_QSPI_READ_FIFO_RX  ReadFifoRx;
  EFI_STATUS              Status;

  ReadMemTx.Cmd = Cmd1;
  MctpUint16ToBEBuffer (ReadMemTx.Addr, Addr);
  Status = ErotQspiXfer (Private, sizeof (ReadMemTx), &ReadMemTx, 0, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiPollForStatus (
             Private,
             EROT_POLL_ALL_TX_FIFO_NOT_EMPTY,
             EROT_POLL_ALL_TX_FIFO_NOT_EMPTY,
             EROT_MEM_BUSY_TIMEOUT_MS
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Initiate FIFO READ
  ReadFifoTx.Cmd = Cmd2;
  Status         = ErotQspiXfer (
                     Private,
                     sizeof (ReadFifoTx),
                     &ReadFifoTx,
                     OFFSET_OF (EROT_QSPI_READ_FIFO_RX, Data) + Bytes,
                     &ReadFifoRx
                     );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((MctpBEBufferToUint16 (ReadFifoRx.Status) & EROT_SPI_STATUS_MEM_READ_DONE) == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Got bad FIFO read status: 0x%x\n", __FUNCTION__, MctpBEBufferToUint16 (ReadFifoRx.Status)));
  }

  CopyMem (Buffer, ReadFifoRx.Data, Bytes);

  // write memory read done bit to clear it
  return ErotQspiSregWrite32 (Private, EROT_REG_SPI_STATUS, EROT_SPI_STATUS_MEM_READ_DONE);
}

/**
  Read erot memory.

  @param[in]  Private       Pointer to private structure for erot.
  @param[in]  Offset        Offset in erot Tx buffer to read.
  @param[in]  Bytes         Number of bytes to read.
  @param[in]  Data          Pointer to buffer to store data from erot.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiReadMem (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN UINT32                  Offset,
  IN UINT32                  Bytes,
  OUT VOID                   *Data
  )
{
  UINT8       Buffer[EROT_MEM_MAX_BYTES_PER_XFER];
  UINT32      XferOffset;
  UINT32      XferBytes;
  UINTN       Index;
  EFI_STATUS  Status;
  UINT8       *Payload;
  UINT32      BytesRequested;

  BytesRequested = Bytes;
  Payload        = (UINT8 *)Data;
  XferOffset     = 0;
  while (Bytes > 0) {
    XferBytes = (Bytes > EROT_MEM_MAX_BYTES_PER_XFER) ?
                EROT_MEM_MAX_BYTES_PER_XFER : Bytes;
    if (XferBytes >= EROT_MEM_BLOCK_SIZE) {
      XferBytes &= ~(EROT_MEM_BLOCK_SIZE - 1);

      DEBUG ((DEBUG_VERBOSE, "%a: Reading %u bytes\n", __FUNCTION__, XferBytes));
      Status = ErotQspiDoReadMemCommand (
                 Private,
                 EROT_CMD_MEM_BLK_R1 + ((XferBytes / EROT_MEM_BLOCK_SIZE) - 1),
                 EROT_CMD_BLK_RD_FIFO_FSR + ((XferBytes / EROT_MEM_BLOCK_SIZE) - 1),
                 Offset + XferOffset,
                 XferBytes,
                 Buffer
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      for (Index = 0; Index < XferBytes; Index += EROT_MEM_BLOCK_SIZE) {
        ErotQSpiCopyAndReverseBuffer (&Payload[XferOffset + Index], &Buffer[Index], EROT_MEM_BLOCK_SIZE);
      }
    } else {
      for (Index = 0; Index < XferBytes; Index++) {
        DEBUG ((DEBUG_VERBOSE, "%a: Reading single byte\n", __FUNCTION__));
        Status = ErotQspiDoReadMemCommand (
                   Private,
                   EROT_CMD_MEM_R8,
                   EROT_CMD_RD_SNGL_FIFO8_FSR,
                   Offset + XferOffset + Index,
                   EROT_MEM_BYTES_PER_SINGLE_READ,
                   Buffer
                   );
        if (EFI_ERROR (Status)) {
          return Status;
        }

        Payload[XferOffset + Index] = Buffer[EROT_MEM_BYTES_PER_SINGLE_READ - 1];
      }
    }

    XferOffset += XferBytes;
    Bytes      -= XferBytes;
  }

  ErotQspiPrintBuffer (__FUNCTION__, Data, BytesRequested);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ErotQspiSendPacket (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN UINTN                   Length
  )
{
  CONST EROT_QSPI_PACKET  *Packet;
  EFI_STATUS              Status;

  Packet = &Private->Packet;

  ErotQspiPrintBuffer ("SendPacket", Packet, Length);

  Status = ErotQspiWriteErotMbox (Private, EROT_MBOX_CMD_REQUEST_WRITE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiPollHostMbox (
             Private,
             FALSE,
             EROT_HOST_MBOX_CMD_ACK,
             NULL
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiWriteMem (
             Private,
             EROT_RX_MEM_START,
             Length,
             Packet
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiWriteErotMbox (Private, Length);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiPollHostMbox (
             Private,
             FALSE,
             EROT_HOST_MBOX_CMD_ACK,
             NULL
             );

  return Status;
}

EFI_STATUS
EFIAPI
ErotQspiSendSetCfg (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  BOOLEAN                    QuadMode
  )
{
  EROT_QSPI_SET_CFG_MSG  Packet;
  EFI_STATUS             Status;

  Packet.Type       = EROT_QSPI_MSG_TYPE_SET_CFG;
  Packet.Reserved   = 0;
  Packet.WaitCycles = 0;
  Packet.Mode       = (QuadMode) ? EROT_QSPI_SET_CFG_MODE_QUAD : EROT_QSPI_SET_CFG_MODE_SINGLE;

  ErotQspiPrintBuffer ("SendSetCfg", &Packet, sizeof (Packet));

  Status = ErotQspiWriteErotMbox (Private, EROT_MBOX_CMD_REQUEST_WRITE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiPollHostMbox (
             Private,
             FALSE,
             EROT_HOST_MBOX_CMD_ACK,
             NULL
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiWriteMem (
             Private,
             EROT_RX_MEM_START,
             sizeof (Packet),
             &Packet
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiWriteErotMbox (Private, sizeof (Packet));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: setting qspi quad mode %u\n", __FUNCTION__, QuadMode));
  mErotQuadMode = QuadMode;

  ErotQspiPollHostMbox (
    Private,
    FALSE,
    EROT_HOST_MBOX_CMD_ACK,
    NULL
    );

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ErotQspiSpbInit (
  IN EROT_QSPI_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  Status = ErotQspiSpbReset (Private);

  return Status;
}

EFI_STATUS
EFIAPI
ErotQspiSpbDeinit (
  IN EROT_QSPI_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  Status = ErotQspiSpbReset (Private);

  return Status;
}

BOOLEAN
EFIAPI
ErotQspiHasInterruptReq (
  IN  EROT_QSPI_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  if (Private->HasMessageAvailable) {
    DEBUG ((DEBUG_VERBOSE, "%a: HasMsgAvailable\n", __FUNCTION__));
    Private->HasMessageAvailable = FALSE;
    return TRUE;
  }

  if (!ErotQspiGpioIsAsserted (Private)) {
    return FALSE;
  }

  Status = ErotQspiPollHostMbox (
             Private,
             FALSE,
             EROT_HOST_MBOX_MSG_AVAILABLE,
             NULL
             );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  return TRUE;
}

UINT64
EFIAPI
ErotQspiNsCounter (
  VOID
  )
{
  return GetTimeInNanoSecond (GetPerformanceCounter ());
}

EFI_STATUS
EFIAPI
ErotQspiRecvPacket (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN OUT UINTN               *Length
  )
{
  EFI_STATUS        Status;
  UINT8             PacketLength;
  EROT_QSPI_PACKET  *Packet;

  Packet = &Private->Packet;

  Status = ErotQspiWriteErotMbox (Private, EROT_MBOX_CMD_READY_TO_READ);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiPollHostMbox (
             Private,
             TRUE,
             0,
             &PacketLength
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (PacketLength > sizeof (*Packet)) {
    DEBUG ((DEBUG_ERROR, "%a: packet length %u too big\n", __FUNCTION__, PacketLength));
    return EFI_UNSUPPORTED;
  }

  Status = ErotQspiReadMem (Private, EROT_TX_MEM_START, PacketLength, Packet);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiWriteErotMbox (Private, EROT_MBOX_CMD_FINISHED_READ);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiPollHostMbox (
             Private,
             FALSE,
             EROT_HOST_MBOX_CMD_ACK,
             NULL
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ack failed\n", __FUNCTION__));
  }

  *Length = PacketLength;

  ErotQspiPrintBuffer ("Resp", Packet, PacketLength);

  return EFI_SUCCESS;
}

VOID
EFIAPI
ErotQspiPrintBuffer (
  IN CONST CHAR8  *String,
  IN CONST VOID   *Buffer,
  IN UINTN        Length
  )
{
  UINTN  LineSize;
  UINTN  LineIndex;
  UINTN  BufferIndex;
  UINT8  *Data;
  CHAR8  Line[100];
  CHAR8  Byte[4];

  Data = (UINT8 *)Buffer;
  for (BufferIndex = 0; BufferIndex < Length; BufferIndex += 16) {
    Line[0]  = '\0';
    LineSize = MIN (Length - BufferIndex, 16);
    for (LineIndex = 0; LineIndex < LineSize; LineIndex++) {
      AsciiSPrint (Byte, sizeof (Byte), " %02x", Data[BufferIndex + LineIndex]);
      AsciiStrCatS (Line, sizeof (Line), Byte);
      if ((LineIndex % 4 == 3) && (LineIndex + 1 < LineSize)) {
        AsciiStrCatS (Line, sizeof (Line), "  ");
      }
    }

    DEBUG ((DEBUG_VERBOSE, "%a 0x%04x:%a\n", String, BufferIndex, Line));
  }
}
