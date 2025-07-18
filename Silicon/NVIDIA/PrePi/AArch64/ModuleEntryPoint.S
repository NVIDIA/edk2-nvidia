//
//  SPDX-FileCopyrightText: Copyright (c) 2018-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//  Copyright (c) 2011-2015, ARM Limited. All rights reserved.
//
//  SPDX-License-Identifier: BSD-2-Clause-Patent
//

#include <AsmMacroLib.h>

ASM_FUNC(_ModuleEntryPoint)
  //
  // We are built as a ET_DYN PIE executable, so we need to process all
  // relative relocations regardless of whether or not we are executing from
  // the same offset we were linked at. This is only possible if we are
  // running from RAM.
  //
  adr   x8, __reloc_base
  adr   x9, __reloc_start
  adr   x10, __reloc_end

.Lreloc_loop:
  cmp   x9, x10
  bhs   .Lreloc_done

  //
  // AArch64 uses the ELF64 RELA format, which means each entry in the
  // relocation table consists of
  //
  //   UINT64 offset          : the relative offset of the value that needs to
  //                            be relocated
  //   UINT64 info            : relocation type and symbol index (the latter is
  //                            not used for R_AARCH64_RELATIVE relocations)
  //   UINT64 addend          : value to be added to the value being relocated
  //
  ldp   x11, x12, [x9], #24   // read offset into x11 and info into x12
  cmp   x12, #0x403           // check info == R_AARCH64_RELATIVE?
  bne   .Lreloc_loop          // not a relative relocation? then skip

  ldr   x12, [x9, #-8]        // read addend into x12
  add   x12, x12, x8          // add reloc base to addend to get relocated value
  str   x12, [x11, x8]        // write relocated value at offset
  b     .Lreloc_loop
.Lreloc_done:

  bl ASM_PFX(TegraGetChipID)
  mov w28, w0

  EL1_OR_EL2(x1)

1:
2:
  //Get memory locations
  bl ASM_PFX(TegraGetBLCarveoutInfoLocationAddress)
  mov x5, x0
  ldp x0, x1, [x5], #8
  sub x1, x1, #SIZE_4KB
  add sp, x0, x1

  // x2 = The top of the Stack
  // Stack for the primary core = PrimaryCoreStack
  MOV32 (x3, FixedPcdGet32(PcdCPUCorePrimaryStackSize))
  sub   x2, sp, x3

  // Jump to PrePi C code
  //    x0 = MemoryBase
  //    x1 = MemorySize
  //    x2 = StackBase
  //    x3 = StackSize
  bl    ASM_PFX(CEntryPoint)

_NeverReturn:
  b _NeverReturn
