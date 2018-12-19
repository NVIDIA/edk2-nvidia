//
//  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
//  Copyright (c) 2011-2015, ARM Limited. All rights reserved.
//
//  This program and the accompanying materials
//  are licensed and made available under the terms and conditions of the BSD License
//  which accompanies this distribution.  The full text of the license may be found at
//  http://opensource.org/licenses/bsd-license.php
//
//  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
//  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
//
//

#include <AsmMacroIoLibV8.h>

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

  EL1_OR_EL2_OR_EL3(x1)

3: MOV32 (w0, FixedPcdGet32(PcdArmNonSecModeTransition))
   msr spsr_el3, x0

   MOV32 (w0, FixedPcdGet32(PcdArmScr))
   bl ASM_PFX(ArmWriteScr)

   MOV32 (w0, FixedPcdGet32(PcdArmArchTimerFreqInHz))
   bl ASM_PFX(ArmWriteCntFrq)

   bl ASM_PFX(ArmGicV3GetControlSystemRegisterEnable)
   orr x0, x0, #1
   bl ASM_PFX(ArmGicV3SetControlSystemRegisterEnable)

   adr x0, #12
   msr elr_el3, x0
   eret

1:
2:
  // Do early platform specific actions
  bl    ASM_PFX(ArmPlatformPeiBootAction)

  MOV64 (x5, FixedPcdGet64(PcdBootloaderInfoLocationAddress))
  MOV64 (x6, FixedPcdGet64(PcdBootloaderCarveoutOffset))
  MOV64 (x7, FixedPcdGet64(PcdSystemMemoryBase))
  ldr   w5, [x5]
  cmp   x5, x7
  b.ge _BootloaderAddressGood
  lsl   x5, x5, #16
_BootloaderAddressGood:
  add   x5, x5, x6

  ldp   x0, x1, [x5], #8
  add sp, x0, x1

  // x2 = The top of the Stack
  // Stack for the primary core = PrimaryCoreStack
  MOV32 (x3, FixedPcdGet32(PcdCPUCorePrimaryStackSize))
  sub   x2, sp, x3

  // Jump to PrePi C code
  //    x0 = MemoryBase
  //    x1 = MemorySize
  //    x2 = StackBase
  //    x2 = StackSize
  bl    ASM_PFX(CEntryPoint)

_NeverReturn:
  b _NeverReturn
