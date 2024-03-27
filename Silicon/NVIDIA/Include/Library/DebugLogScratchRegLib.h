/** @file

  Library to log data to scratch registers, this used by the DebugLib or the
  Exception Handler Library.

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DEBUG_LOG_SCRATCH_REG_LIB_H_
#define __DEBUG_LOG_SCRATCH_REG_LIB_H_

/**
 * LogStringToScratchRegisters
 * Log a string to a group of Scratch registers.
 *
 * @param[in] Name         String to log.
 * @param[in] SratchBase   Base Address of Scratch Registers.
 * @param[in] NumRegs      Number of Registers to Log String.
 **/
VOID
LogStringToScratchRegisters (
  IN CONST CHAR8  *Name,
  IN UINT64       ScratchBase,
  IN UINT32       NumRegs
  );

/*
 * LogFileNameToScratchRegisters
 * Log the FileName to scratch Registers.

 * @param  FileName        Name of the source file that generated the assert condition.
 * @param[in] SratchBase   Base Address of Scratch Registers.
 * @param[in] NumRegs      Number of Registers to Log String.
 */
VOID
LogFileNameToScratchRegisters (
  IN CONST CHAR8  *FileName,
  IN UINT64       ScratchBase,
  IN UINT32       NumRegs
  );

/**
 * LogLineNumToScratchRegisters
 * Log a Line Number to a group of Scratch registers.
 *
 * @param[in] LineNumber   LineNumber to log.
 * @param[in] SratchBase   Base Address of Scratch Registers.
 * @param[in] NumRegs      Number of Registers to Log String.
 **/
VOID
LogLineNumToScratchRegisters (
  IN UINTN   LineNumber,
  IN UINT64  ScratchBase,
  IN UINT32  NumRegs
  );

/**
 * LogUint32ToScratchRegisters
 * Log a UINT32 to Scratch registers.
 *
 * @param[in] Val          UINT64 to log.
 * @param[in] SratchBase   Base Address of Scratch Registers.
 **/
VOID
LogUint32ToScratchRegisters (
  IN UINT32  Val,
  IN UINT64  ScratchReg
  );

/**
 * LogUint64ToScratchRegisters
 * Log a UINT64 to Scratch registers.
 *
 * @param[in] Val          UINT64 to log.
 * @param[in] SratchBase   Base Address of Scratch Registers.
 **/
VOID
LogUint64ToScratchRegisters (
  IN UINT64  Val,
  IN UINT64  ScratchBase
  );

VOID
DumpRegisters (
  UINT64  ScratchBase,
  UINT32  NumRegs
  );

#endif // __DEBUG_LOG_SCRATCH_REG_LIB_H_
