/** @file
  Host-based unit tests for QspiControllerLib.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <Library/QspiControllerLib.h>
#include <Library/TimerLib.h>
#include <Library/UnitTestLib.h>

#define UNIT_TEST_NAME     "QspiControllerLib Unit Tests"
#define UNIT_TEST_VERSION  "1.0"
#define TEST_MMIO_SIZE     0x200

UINT32
EFIAPI
UnitTestMmioRead32 (
  IN UINTN  Address
  );

UINT32
EFIAPI
UnitTestMmioWrite32 (
  IN UINTN   Address,
  IN UINT32  Value
  );

UINT32
EFIAPI
UnitTestMmioBitFieldRead32 (
  IN UINTN  Address,
  IN UINTN  StartBit,
  IN UINTN  EndBit
  );

UINT32
EFIAPI
UnitTestMmioBitFieldWrite32 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  Value
  );

VOID
EFIAPI
UnitTestMicroSecondDelay (
  IN UINTN  MicroSeconds
  );

#define MmioRead32(Address)          UnitTestMmioRead32 (Address)
#define MmioWrite32(Address, Value)  UnitTestMmioWrite32 (Address, Value)
#define MmioBitFieldRead32(Address, StartBit, EndBit) \
        UnitTestMmioBitFieldRead32 (Address, StartBit, EndBit)
#define MmioBitFieldWrite32(Address, StartBit, EndBit, Value) \
        UnitTestMmioBitFieldWrite32 (Address, StartBit, EndBit, Value)
#define MicroSecondDelay(MicroSeconds)  UnitTestMicroSecondDelay (MicroSeconds)

#undef STATIC
#define STATIC
#include "../QspiControllerLib.c"
#undef STATIC
#define STATIC  static

#undef MicroSecondDelay
#undef MmioBitFieldWrite32
#undef MmioBitFieldRead32
#undef MmioWrite32
#undef MmioRead32

STATIC UINT32   mRegisters[TEST_MMIO_SIZE / sizeof (UINT32)];
STATIC BOOLEAN  mSetFifoErrorOnPio;
STATIC BOOLEAN  mFailTxFlush;
STATIC BOOLEAN  mFailRxFlush;
STATIC UINTN    mDelay1UsCount;
STATIC UINTN    mDelay100UsCount;
STATIC UINTN    mTxFlushCount;
STATIC UINTN    mRxFlushCount;
STATIC UINTN    mDmaDisableCount;
STATIC UINTN    mFifoErrorClearCount;
STATIC UINTN    mCommand2WriteCount;

#define TEST_QSPI_BASE  ((EFI_PHYSICAL_ADDRESS)(UINTN)mRegisters)
#define REG_INDEX(Offset)  ((Offset) / sizeof (UINT32))

STATIC
UINT32
BitMask (
  IN UINTN  StartBit,
  IN UINTN  EndBit
  )
{
  UINT64  Mask;
  UINTN   Width;

  Width = EndBit - StartBit + 1;
  Mask  = (Width == 32) ? 0xFFFFFFFFULL : ((1ULL << Width) - 1);

  return (UINT32)(Mask << StartBit);
}

STATIC
UINT32 *
RegisterAt (
  IN UINTN  Address
  )
{
  UINTN  Offset;

  Offset = Address - (UINTN)mRegisters;
  ASSERT (Offset < sizeof (mRegisters));
  ASSERT ((Offset % sizeof (UINT32)) == 0);

  return &mRegisters[REG_INDEX (Offset)];
}

UINT32
EFIAPI
UnitTestMmioRead32 (
  IN UINTN  Address
  )
{
  return *RegisterAt (Address);
}

UINT32
EFIAPI
UnitTestMmioWrite32 (
  IN UINTN   Address,
  IN UINT32  Value
  )
{
  UINTN   Offset;
  UINT32  *Register;

  Offset   = Address - (UINTN)mRegisters;
  Register = RegisterAt (Address);

  if (Offset == QSPI_TRANSFER_STATUS_0) {
    if ((Value & (1U << QSPI_TRANSFER_STATUS_0_RDY_BIT)) != 0) {
      *Register &= ~(1U << QSPI_TRANSFER_STATUS_0_RDY_BIT);
    }

    return Value;
  }

  if (Offset == QSPI_COMMAND2_0) {
    mCommand2WriteCount++;
  }

  if (Offset == QSPI_FIFO_STATUS_0) {
    if ((Value & QSPI_FIFO_STATUS_0_ERROR_MASK) != 0) {
      mFifoErrorClearCount++;
      *Register &= ~QSPI_FIFO_STATUS_0_ERROR_MASK;
    }

    if ((Value & (1U << QSPI_FIFO_STATUS_0_TX_FIFO_FLUSH_BIT)) != 0) {
      mTxFlushCount++;
      if (mFailTxFlush) {
        *Register &= ~(1U << QSPI_FIFO_STATUS_0_TX_FIFO_EMPTY_BIT);
      } else {
        *Register |= (1U << QSPI_FIFO_STATUS_0_TX_FIFO_EMPTY_BIT);
      }

      *Register &= ~(1U << QSPI_FIFO_STATUS_0_TX_FIFO_FULL_BIT);
      *Register &= ~(1U << QSPI_FIFO_STATUS_0_TX_FIFO_FLUSH_BIT);
    }

    if ((Value & (1U << QSPI_FIFO_STATUS_0_RX_FIFO_FLUSH_BIT)) != 0) {
      mRxFlushCount++;
      if (mFailRxFlush) {
        *Register &= ~(1U << QSPI_FIFO_STATUS_0_RX_FIFO_EMPTY_BIT);
      } else {
        *Register |= (1U << QSPI_FIFO_STATUS_0_RX_FIFO_EMPTY_BIT);
      }

      *Register &= ~(1U << QSPI_FIFO_STATUS_0_RX_FIFO_FLUSH_BIT);
    }

    return Value;
  }

  *Register = Value;

  if (Offset == QSPI_COMMAND_0) {
    if ((Value & (1U << QSPI_COMMAND_0_PIO_BIT)) != 0) {
      mRegisters[REG_INDEX (QSPI_TRANSFER_STATUS_0)] |=
        (1U << QSPI_TRANSFER_STATUS_0_RDY_BIT);
      if (mSetFifoErrorOnPio) {
        mRegisters[REG_INDEX (QSPI_FIFO_STATUS_0)] |=
          (1U << QSPI_FIFO_STATUS_0_RX_FIFO_UNF_BIT);
      }
    }
  } else if (Offset == QSPI_DMA_CTL_0) {
    if ((Value & (1U << QSPI_DMA_CTL_0_DMA_EN_BIT)) == 0) {
      mDmaDisableCount++;
    }
  }

  return Value;
}

UINT32
EFIAPI
UnitTestMmioBitFieldRead32 (
  IN UINTN  Address,
  IN UINTN  StartBit,
  IN UINTN  EndBit
  )
{
  return (UnitTestMmioRead32 (Address) & BitMask (StartBit, EndBit)) >>
         StartBit;
}

UINT32
EFIAPI
UnitTestMmioBitFieldWrite32 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  Value
  )
{
  UINT32  Register;
  UINT32  Mask;

  Register = UnitTestMmioRead32 (Address);
  Mask     = BitMask (StartBit, EndBit);
  Register = (Register & ~Mask) | ((Value << StartBit) & Mask);

  return UnitTestMmioWrite32 (Address, Register);
}

VOID
EFIAPI
UnitTestMicroSecondDelay (
  IN UINTN  MicroSeconds
  )
{
  if (MicroSeconds == QSPI_CONFIG_SETTLE_DELAY) {
    mDelay1UsCount++;
  } else if (MicroSeconds == QSPI_CLEAR_STATE_DELAY) {
    mDelay100UsCount++;
  }
}

STATIC
VOID
ResetTestState (
  VOID
  )
{
  ZeroMem (mRegisters, sizeof (mRegisters));
  mRegisters[REG_INDEX (QSPI_FIFO_STATUS_0)] =
    (1U << QSPI_FIFO_STATUS_0_TX_FIFO_EMPTY_BIT) |
    (1U << QSPI_FIFO_STATUS_0_RX_FIFO_EMPTY_BIT);
  mRegisters[REG_INDEX (QSPI_TRANSFER_STATUS_0)] =
    (1U << QSPI_TRANSFER_STATUS_0_RDY_BIT);

  TimeOutMessage       = FALSE;
  mSetFifoErrorOnPio   = FALSE;
  mFailTxFlush         = FALSE;
  mFailRxFlush         = FALSE;
  mDelay1UsCount       = 0;
  mDelay100UsCount     = 0;
  mTxFlushCount        = 0;
  mRxFlushCount        = 0;
  mDmaDisableCount     = 0;
  mFifoErrorClearCount = 0;
  mCommand2WriteCount  = 0;
}

STATIC
UNIT_TEST_STATUS
AssertTransferDisabled (
  VOID
  )
{
  UINT32  Command;
  UINT32  DmaCtl;

  Command = mRegisters[REG_INDEX (QSPI_COMMAND_0)];
  DmaCtl  = mRegisters[REG_INDEX (QSPI_DMA_CTL_0)];

  UT_ASSERT_EQUAL ((Command >> QSPI_COMMAND_0_PIO_BIT) & 1U, 0);
  UT_ASSERT_EQUAL ((Command >> QSPI_COMMAND_0_TX_EN_BIT) & 1U, 0);
  UT_ASSERT_EQUAL ((Command >> QSPI_COMMAND_0_RX_EN_BIT) & 1U, 0);
  UT_ASSERT_EQUAL ((DmaCtl >> QSPI_DMA_CTL_0_DMA_EN_BIT) & 1U, 0);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
AssertChipSelectDeasserted (
  VOID
  )
{
  UINT32  Command;

  Command = mRegisters[REG_INDEX (QSPI_COMMAND_0)];

  UT_ASSERT_EQUAL ((Command >> QSPI_COMMAND_0_CS_SW_VAL_BIT) & 1U, 1);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestNullPacketRejected (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ResetTestState ();

  UT_ASSERT_EQUAL (
    QspiPerformTransaction (TEST_QSPI_BASE, NULL),
    EFI_INVALID_PARAMETER
    );

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestTransmitUsesSettleDelayAndCleansUp (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS               Status;
  UINT8                    TxData[3];
  QSPI_TRANSACTION_PACKET  Packet;
  UNIT_TEST_STATUS         TestStatus;

  ResetTestState ();
  TxData[0] = 0xA5;
  TxData[1] = 0x5A;
  TxData[2] = 0xC3;
  ZeroMem (&Packet, sizeof (Packet));
  Packet.TxBuf      = TxData;
  Packet.TxLen      = sizeof (TxData);
  Packet.ChipSelect = 1;
  Packet.Control    = QSPI_CONTROLLER_CONTROL_FAST_MODE;

  Status = QspiPerformTransaction (TEST_QSPI_BASE, &Packet);

  UT_ASSERT_NOT_EFI_ERROR (Status);
  TestStatus = AssertTransferDisabled ();
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  TestStatus = AssertChipSelectDeasserted ();
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  UT_ASSERT_TRUE (mDelay1UsCount >= 1);
  UT_ASSERT_EQUAL (mDelay100UsCount, 0);
  UT_ASSERT_TRUE (mDmaDisableCount >= 1);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestRxEmptyErrorRecoversController (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS               Status;
  UINT8                    RxData;
  QSPI_TRANSACTION_PACKET  Packet;
  UNIT_TEST_STATUS         TestStatus;

  ResetTestState ();
  ZeroMem (&Packet, sizeof (Packet));
  Packet.RxBuf      = &RxData;
  Packet.RxLen      = sizeof (RxData);
  Packet.ChipSelect = 2;
  Packet.Control    = QSPI_CONTROLLER_CONTROL_FAST_MODE;

  Status = QspiPerformTransaction (TEST_QSPI_BASE, &Packet);

  UT_ASSERT_EQUAL (Status, EFI_DEVICE_ERROR);
  TestStatus = AssertTransferDisabled ();
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  TestStatus = AssertChipSelectDeasserted ();
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  UT_ASSERT_TRUE (mTxFlushCount >= 1);
  UT_ASSERT_TRUE (mRxFlushCount >= 1);
  UT_ASSERT_TRUE (mDelay100UsCount >= 1);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestFifoErrorRecoversController (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS               Status;
  UINT8                    RxData;
  QSPI_TRANSACTION_PACKET  Packet;
  UNIT_TEST_STATUS         TestStatus;

  ResetTestState ();
  mRegisters[REG_INDEX (QSPI_FIFO_STATUS_0)] &=
    ~(1U << QSPI_FIFO_STATUS_0_RX_FIFO_EMPTY_BIT);
  mSetFifoErrorOnPio = TRUE;
  ZeroMem (&Packet, sizeof (Packet));
  Packet.RxBuf      = &RxData;
  Packet.RxLen      = sizeof (RxData);
  Packet.ChipSelect = 0;
  Packet.Control    = QSPI_CONTROLLER_CONTROL_FAST_MODE;

  Status = QspiPerformTransaction (TEST_QSPI_BASE, &Packet);

  UT_ASSERT_EQUAL (Status, EFI_DEVICE_ERROR);
  TestStatus = AssertTransferDisabled ();
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  TestStatus = AssertChipSelectDeasserted ();
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  UT_ASSERT_TRUE (mFifoErrorClearCount >= 1);
  UT_ASSERT_TRUE (mTxFlushCount >= 1);
  UT_ASSERT_TRUE (mRxFlushCount >= 1);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestInitializeRestoresCommand2PerCall (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ResetTestState ();
  mRegisters[REG_INDEX (QSPI_COMMAND2_0)] = 0x11111111;

  UT_ASSERT_NOT_EFI_ERROR (QspiInitialize (TEST_QSPI_BASE, 1));
  UT_ASSERT_EQUAL (mRegisters[REG_INDEX (QSPI_COMMAND2_0)], 0x11111111);

  mRegisters[REG_INDEX (QSPI_COMMAND2_0)] = 0x22222222;

  UT_ASSERT_NOT_EFI_ERROR (QspiInitialize (TEST_QSPI_BASE, 1));
  UT_ASSERT_EQUAL (mRegisters[REG_INDEX (QSPI_COMMAND2_0)], 0x22222222);

  return UNIT_TEST_PASSED;
}

STATIC
UNIT_TEST_STATUS
EFIAPI
TestInitializeRestoresCommand2OnFlushFailure (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ResetTestState ();
  mRegisters[REG_INDEX (QSPI_COMMAND2_0)] = 0x33333333;
  mFailTxFlush                            = TRUE;

  UT_ASSERT_EQUAL (QspiInitialize (TEST_QSPI_BASE, 1), EFI_DEVICE_ERROR);
  UT_ASSERT_EQUAL (mRegisters[REG_INDEX (QSPI_COMMAND2_0)], 0x33333333);
  UT_ASSERT_EQUAL (mCommand2WriteCount, 1);

  return UNIT_TEST_PASSED;
}

EFI_STATUS
EFIAPI
UefiTestMain (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      Suite;

  Framework = NULL;
  Status    = InitUnitTestFramework (
                &Framework,
                UNIT_TEST_NAME,
                gEfiCallerBaseName,
                UNIT_TEST_VERSION
                );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = CreateUnitTestSuite (
             &Suite,
             Framework,
             "Controller",
             "QspiControllerLib.Controller",
             NULL,
             NULL
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  AddTestCase (
    Suite,
    "Reject NULL packet",
    "NullPacket",
    TestNullPacketRejected,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    Suite,
    "Transmit settles and cleans up",
    "TransmitCleanup",
    TestTransmitUsesSettleDelayAndCleansUp,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    Suite,
    "RX empty error recovers",
    "RxEmptyRecover",
    TestRxEmptyErrorRecoversController,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    Suite,
    "FIFO error recovers",
    "FifoErrorRecover",
    TestFifoErrorRecoversController,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    Suite,
    "Initialize restores COMMAND2 per call",
    "Command2Restore",
    TestInitializeRestoresCommand2PerCall,
    NULL,
    NULL,
    NULL
    );
  AddTestCase (
    Suite,
    "Initialize restores COMMAND2 on flush failure",
    "Command2RestoreFailure",
    TestInitializeRestoresCommand2OnFlushFailure,
    NULL,
    NULL,
    NULL
    );

  Status = RunAllTestSuites (Framework);

Done:
  if (Framework != NULL) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  return UefiTestMain ();
}
