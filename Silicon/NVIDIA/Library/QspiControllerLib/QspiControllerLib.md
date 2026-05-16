# QspiControllerLib Design Document

## Overview

`QspiControllerLib` programs NVIDIA QSPI controllers for PIO flash
transactions. It provides controller initialization, chip-select control,
combined sequence control, wait-cycle programming, TX/RX FIFO access, and
wait-state configuration.

The library is a BASE library and does not publish protocols. Callers provide
the controller register base address and transaction packet.

## Functional Requirements

| ID | Requirement |
|----|-------------|
| FR-01 | The library shall initialize the controller for master mode, mode 0, software chip-select, inactive-high chip-select, and big-endian byte order. |
| FR-02 | Initialization shall preserve the existing `COMMAND2` register value. |
| FR-03 | A transaction shall reject NULL packets and invalid buffer/length combinations before touching controller state. |
| FR-04 | Transfer enables, DMA enable, and stale FIFO status shall be cleared before programming a PIO transaction. |
| FR-05 | The library shall wait for controller configuration to settle before starting PIO and after FIFO flush operations. |
| FR-06 | TX and RX paths shall detect FIFO overflow, underflow, and generic FIFO error status. |
| FR-07 | Error paths shall recover the controller by disabling transfer state, clearing transaction status, clearing FIFO status, and flushing both FIFOs. |
| FR-08 | The transaction exit path shall disable combined sequence mode and deassert chip-select. |

## Transaction Flow

```
QspiPerformTransaction()
    |
    +-- Validate packet and transfer buffers
    |
    +-- Assert chip-select
    |
    +-- Configure combined sequence mode when requested
    |
    +-- Program transfer parameters for each TX/RX chunk
    |
    +-- Clear stale FIFO status and wait for configuration settle
    |
    +-- Start PIO transfer and wait for ready status
    |
    +-- Read or write FIFO data
    |
    +-- Disable combined sequence mode and deassert chip-select
```

The cleanup path runs for both success and failure. This keeps later
transactions from inheriting stale chip-select, combined sequence, PIO, TX/RX,
or DMA state.

## Error Recovery

PIO transactions treat FIFO error bits as media-facing controller failures.
When a FIFO error, timeout, or invalid controller state is detected, the
library disables active transfer state, clears transaction and FIFO status, and
flushes both TX and RX FIFOs. Recovery is local to the controller instance and
the original transaction status is returned to the caller.

## Health And Robustness

This module does not publish Driver Health because it is a BASE library. Health
is represented by the `EFI_STATUS` returned to callers. Robustness depends on
bounded register polling, explicit cleanup on every transaction exit, and
controller recovery before returning from error paths.

## Testing

`QspiControllerLibUnitTestsHost` covers invalid packet handling, TX cleanup,
RX empty recovery, FIFO error recovery, `COMMAND2` preservation during
initialization, and transaction exit cleanup.

## Related Files

- `Silicon/NVIDIA/Library/QspiControllerLib/QspiControllerLib.c`
- `Silicon/NVIDIA/Library/QspiControllerLib/QspiControllerLibPrivate.h`
- `Silicon/NVIDIA/Library/QspiControllerLib/QspiControllerLib.inf`
- `Silicon/NVIDIA/Library/QspiControllerLib/UnitTest/QspiControllerLibUnitTests.c`
- `Silicon/NVIDIA/Library/QspiControllerLib/UnitTest/QspiControllerLibUnitTestsHost.inf`

## Copyright

SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent
