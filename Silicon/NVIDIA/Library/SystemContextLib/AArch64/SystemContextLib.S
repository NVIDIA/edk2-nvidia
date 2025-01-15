//
// Copyright (c) 2011 - 2021, Arm Limited. All rights reserved.<BR>
// Copyright (c) 2016 HP Development Company, L.P.
// SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
// Fill an EFI_SYSTEM_CONTEXT_AARCH64
// Add function to swap system context
//
// This code borrows from ArmExceptionLib/AArch64/ExceptionSupport.S, except
// here, we want to simply capture the system context and not handle
// exceptions.
//------------------------------------------------------------------------------


#include <AsmMacroLib.h>

#define GP_CONTEXT_SIZE    (32 *  8)
#define FP_CONTEXT_SIZE    (32 * 16)
#define SYS_CONTEXT_SIZE   ( 6 *  8) // 5 SYS regs + Alignment requirement (ie: the stack must be aligned on 0x10)

  .macro ReadSystemContext

  // Store the GP registers
  stp      x0, x1, [x0]
  stp      x2, x3, [x0, #0x10]
  stp      x4, x5, [x0, #0x20]
  stp      x6, x7, [x0, #0x30]
  stp      x8,  x9,  [x0, #0x40]
  stp      x10, x11, [x0, #0x50]
  stp      x12, x13, [x0, #0x60]
  stp      x14, x15, [x0, #0x70]
  stp      x16, x17, [x0, #0x80]
  stp      x18, x19, [x0, #0x90]
  stp      x20, x21, [x0, #0xa0]
  stp      x22, x23, [x0, #0xb0]
  stp      x24, x25, [x0, #0xc0]
  stp      x26, x27, [x0, #0xd0]
  stp      x28, x29, [x0, #0xe0]

  // The stack pointer cannot be stored directly.  Move it into a temporary
  // register first.
  mov      x15, sp
  stp      x30, x15,  [x0, #0xf0]

  // Store the FP registers
  stp      q0,  q1,  [x0, #0x100]
  stp      q2,  q3,  [x0, #0x120]
  stp      q4,  q5,  [x0, #0x140]
  stp      q6,  q7,  [x0, #0x160]
  stp      q8,  q9,  [x0, #0x180]
  stp      q10, q11, [x0, #0x1a0]
  stp      q12, q13, [x0, #0x1c0]
  stp      q14, q15, [x0, #0x1e0]
  stp      q16, q17, [x0, #0x200]
  stp      q18, q19, [x0, #0x220]
  stp      q20, q21, [x0, #0x240]
  stp      q22, q23, [x0, #0x260]
  stp      q24, q25, [x0, #0x280]
  stp      q26, q27, [x0, #0x2a0]
  stp      q28, q29, [x0, #0x2c0]
  stp      q30, q31, [x0, #0x2e0]

  // Store the SYS registers
  EL1_OR_EL2(x15)
1:mrs      x10, elr_el1   // Exception Link Register
  mrs      x11, spsr_el1  // Saved Processor Status Register 32bit
  mrs      x13, esr_el1   // EL1 Exception syndrome register 32bit
  mrs      x14, far_el1   // EL1 Fault Address Register
  b        4f

2:mrs      x10, elr_el2   // Exception Link Register
  mrs      x11, spsr_el2  // Saved Processor Status Register 32bit
  mrs      x13, esr_el2   // EL2 Exception syndrome register 32bit
  mrs      x14, far_el2   // EL2 Fault Address Register
  b        4f

4:mrs      x12, fpsr      // Floating point Status Register  32bit

  // Save the SYS regs
  str      x10,  [x0, #0x300] // Exception Link Register
  str      x11,  [x0, #0x308] // Saved Processor Status Register 32bit
  str      x12,  [x0, #0x310] // Floating point Status Register  32bit
  str      x13,  [x0, #0x318] // EL3 Exception syndrome register 32bit
  str      x14,  [x0, #0x320] // EL3 Fault Address Register

  // Restore the registers we used
  ldp      x10, x11, [x0, #0x50]
  ldp      x12, x13, [x0, #0x60]
  ldp      x14, x15, [x0, #0x70]

  .endm

/**
  Gets the current system context

  @param[in,out] SystemContext  SystemContext structure to fill out

VOID
EFIAPI
GetSystemContext (
  IN OUT EFI_SYSTEM_CONTEXT  SystemContext
  );
**/

GCC_ASM_EXPORT(GetSystemContext)
ASM_PFX(GetSystemContext):
  ReadSystemContext
  ret

/**
  Swaps the system context

  In addition to the system registers this changes the LR so it returns to the
  previous context caller, however the other context should swap back so this
  will overall function like a regular function call.

  @param[in,out] CurrentSystemContext SystemContext structure to update with
                                      current context
  @param[in]     NewSystemContext     SystemContext to be restored, everything
                                      will be restored except for PC

EFI_STATUS
EFIAPI
SwapSystemContext (
  IN OUT EFI_SYSTEM_CONTEXT  CurrentSystemContext,
  IN     EFI_SYSTEM_CONTEXT  NewSystemContext
  );
**/
GCC_ASM_EXPORT(SwapSystemContext)
ASM_PFX(SwapSystemContext):
  ReadSystemContext

  // Read the SYS regs we need
  ldr      x10,  [x1, #0x300] // Exception Link Register
  ldr      x11,  [x1, #0x308] // Saved Processor Status Register 32bit
  ldr      x12,  [x1, #0x310] // Floating point Status Register  32bit
  ldr      x13,  [x1, #0x318] // EL3 Exception syndrome register 32bit
  ldr      x14,  [x1, #0x320] // EL3 Fault Address Register

  msr      fpsr, x12

  EL1_OR_EL2(x28)
1:msr      elr_el1,  x10   // Exception Link Register
  msr      spsr_el1, x11  // Saved Processor Status Register 32bit
  msr      esr_el1,  x13   // EL2 Exception syndrome register 32bit
  msr      far_el1,  x14   // EL2 Fault Address Register
  b        4f
2:msr      elr_el2,  x10   // Exception Link Register
  msr      spsr_el2, x11  // Saved Processor Status Register 32bit
  msr      esr_el2,  x13   // EL2 Exception syndrome register 32bit
  msr      far_el2,  x14   // EL2 Fault Address Register
  b        4f
4:

  // Restore FP regs.
  ldp      q0,  q1,  [x1, #0x100]
  ldp      q2,  q3,  [x1, #0x120]
  ldp      q4,  q5,  [x1, #0x140]
  ldp      q6,  q7,  [x1, #0x160]
  ldp      q8,  q9,  [x1, #0x180]
  ldp      q10, q11, [x1, #0x1a0]
  ldp      q12, q13, [x1, #0x1c0]
  ldp      q14, q15, [x1, #0x1e0]
  ldp      q16, q17, [x1, #0x200]
  ldp      q18, q19, [x1, #0x220]
  ldp      q20, q21, [x1, #0x240]
  ldp      q22, q23, [x1, #0x260]
  ldp      q24, q25, [x1, #0x280]
  ldp      q26, q27, [x1, #0x2a0]
  ldp      q28, q29, [x1, #0x2c0]
  ldp      q30, q31, [x1, #0x2e0]

  // Restore GP regs.
  ldp      x30, x15,  [x1, #0xf0]
  mov      sp, x15

  ldp      x2,  x3,  [x1, #0x10]
  ldp      x4,  x5,  [x1, #0x20]
  ldp      x6,  x7,  [x1, #0x30]
  ldp      x8,  x9,  [x1, #0x40]
  ldp      x10, x11, [x1, #0x50]
  ldp      x12, x13, [x1, #0x60]
  ldp      x14, x15, [x1, #0x70]
  ldp      x16, x17, [x1, #0x80]
  ldp      x18, x19, [x1, #0x90]
  ldp      x20, x21, [x1, #0xa0]
  ldp      x22, x23, [x1, #0xb0]
  ldp      x24, x25, [x1, #0xc0]
  ldp      x26, x27, [x1, #0xd0]
  ldp      x28, x29, [x1, #0xe0]
  ldp      x0,  x1,  [x1]

  ret
