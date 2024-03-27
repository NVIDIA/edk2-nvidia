/** @file
  Default Exception Callback Library.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/DefaultExceptionCallbackLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLogScratchRegLib.h>

extern
CHAR8 *
GetImageName (
  IN  UINTN  FaultAddress,
  OUT UINTN  *ImageBase,
  OUT UINTN  *PeCoffSizeOfHeaders
  );

/**
  This is the callback made as part of the DefaultException Handler.

  Since this is exception context don't do anything crazy like try to allocate memory.

  @param  ExceptionType    Type of the exception
  @param  SystemContext    Register state at the time of the Exception

**/
VOID
DefaultExceptionCallback (
  IN     EFI_EXCEPTION_TYPE  ExceptionType,
  IN OUT EFI_SYSTEM_CONTEXT  SystemContext
  )
{
  UINT64  ExceptionTypeBase;
  UINT64  FarBase;
  UINT64  FpBase;
  CHAR8   *Pdb, *PrevPdb;
  UINTN   ImageBase;
  UINTN   PeCoffSizeOfHeader;
  UINT64  *Fp;
  UINT64  RootFp[2];
  UINTN   Idx;
  UINTN   FpIdx;

  if (PcdGetBool (PcdNvLogToScratchRegs) == TRUE) {
    ExceptionTypeBase = PcdGet64 (PcdNvScratchRegBase) + (PcdGet32 (PcdExceptionTypeStartReg) * 4);
    LogUint32ToScratchRegisters (ExceptionType, ExceptionTypeBase);

    FarBase = PcdGet64 (PcdNvScratchRegBase) + (PcdGet32 (PcdFARStartReg) * 4);
    LogUint64ToScratchRegisters (SystemContext.SystemContextAArch64->FAR, FarBase);

    FpBase = PcdGet64 (PcdNvScratchRegBase) + (PcdGet32 (PcdFPStartReg) * 4);
    if ((UINT64 *)SystemContext.SystemContextAArch64->FP != 0) {
      Idx       = 0;
      FpIdx     = 0;
      RootFp[0] = ((UINT64 *)SystemContext.SystemContextAArch64->FP)[0];
      RootFp[1] = ((UINT64 *)SystemContext.SystemContextAArch64->FP)[1];

      if (RootFp[1] != SystemContext.SystemContextAArch64->LR) {
        RootFp[0] = SystemContext.SystemContextAArch64->FP;
        RootFp[1] = SystemContext.SystemContextAArch64->LR;
      }

      for (Fp = RootFp; Fp[0] != 0; Fp = (UINT64 *)Fp[0]) {
        Pdb = GetImageName (Fp[1], &ImageBase, &PeCoffSizeOfHeader);
        if ((Pdb != NULL) && (Pdb != PrevPdb)) {
          if ((FpIdx + PcdGet32 (PcdPerFpLimit)) < PcdGet32 (PcdFPRegLimit)) {
            LogFileNameToScratchRegisters (Pdb, (FpBase + (FpIdx * 4)), PcdGet32 (PcdPerFpLimit));
            FpIdx += PcdGet32 (PcdPerFpLimit);
          }

          PrevPdb = Pdb;
        }
      }
    }
  }
}
