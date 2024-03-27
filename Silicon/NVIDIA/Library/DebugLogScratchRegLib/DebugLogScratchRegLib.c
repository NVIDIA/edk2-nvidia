/** @file
  Library to log data to scratch registers, this used by the DebugLib or the
  Exception Handler Library.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/DebugLib.h>
#include <Library/DebugLogScratchRegLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/PcdLib.h>
#include <Library/IoLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugPrintErrorLevelLib.h>

/**
 * Get the FileName from the full-path.
 *
 * @param[in] FileName Full Filename including path.
 *
 * @result   FileName only.
 **/
STATIC
CHAR8 *
GetFileNameFromPath (
  IN CONST CHAR8  *FileName
  )
{
  CHAR8  *Src;
  CHAR8  *Res;

  Src = (CHAR8 *)FileName;
  Res = AsciiStrStr (Src, "/");

  while (Res != NULL) {
    Src = Res + 1;
    Res = AsciiStrStr (Src, "/");
  }

  return Src;
}

VOID
DumpRegisters (
  UINT64  ScratchBase,
  UINT32  NumRegs
  )
{
  DEBUG_CODE_BEGIN ();
  UINTN   Index;
  UINT32  RegValue;

  for (Index = 0; Index < NumRegs; Index++) {
    RegValue = MmioRead32 (ScratchBase + (Index * 4));
    DEBUG ((DEBUG_ERROR, "Reg[%u] 0x%x %u \n", Index, RegValue, RegValue));
  }

  DEBUG_CODE_END ();
}

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
  )
{
  UINTN   Index;
  UINTN   RegIndex;
  UINT32  Val;

  for (Index = 0, RegIndex = 0; ((Index < AsciiStrLen (Name)) && (RegIndex < NumRegs));
       RegIndex++, Index += sizeof (UINT32))
  {
    CopyMem (&Val, &Name[Index], sizeof (UINT32));
    MmioWrite32 (
      (ScratchBase + (RegIndex * 4)),
      Val
      );
  }
}

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
  )
{
  UINT32  FwNameValue;
  CHAR8   *FileNameToLog;
  UINT32  Index;

  for (Index = 0; Index < NumRegs; Index++) {
    MmioWrite32 (
      (ScratchBase + (Index * 4)),
      0
      );
  }

  CopyMem (&FwNameValue, PcdGetPtr (PcdNvFirmwareStr), sizeof (UINT32));
  MmioWrite32 (
    (ScratchBase),
    FwNameValue
    );

  if (FileName != NULL) {
    FileNameToLog = GetFileNameFromPath (FileName);
  } else {
    FileNameToLog = "NULL";
  }

  LogStringToScratchRegisters (FileNameToLog, ScratchBase, NumRegs);
}

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
  )
{
  UINT64  Num;
  UINT32  LineVal;
  UINTN   Index;

  for (Index = 0; Index < NumRegs; Index++) {
    MmioWrite32 (
      (ScratchBase + (Index * 4)),
      0
      );
  }

  Num   = LineNumber;
  Index = 0;
  while ((Num != 0) && (Index < NumRegs)) {
    LineVal = Num % 10000;
    MmioWrite32 (
      (ScratchBase + ((NumRegs - 1 - Index) * 4)),
      LineVal
      );
    Num = Num / 10000;
    Index++;
  }
}

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
  )
{
  MmioWrite32 (ScratchReg, Val);
}

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
  )
{
  UINT32  RegVal;

  RegVal = (UINT32)(Val >> 32);
  LogUint32ToScratchRegisters (RegVal, ScratchBase);
  RegVal = (UINT32)Val;
  LogUint32ToScratchRegisters (RegVal, (ScratchBase + 4));
}
