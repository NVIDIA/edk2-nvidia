/** @file
* ModuleEntryPoint.S
* Module Entry point for StMM in Hafnium based configurations.
*
* SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
* SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <AsmMacroLib.h>
#define STACK_SIZE            0x2000
#define sizeof_sp_pkg_header  24
#define DTB_MAGIC             0xEDFE0DD0

ASM_FUNC(_ModuleEntryPoint)
  /*
   * The DTB is located before code, search for it by scanning memory
   * and searching for the device tree header magic.
   */

  adr x29, .
  MOV32 (x27, DTB_MAGIC)
_SearchDTB:
  sub x29, x29, #4
  ldr w2, [x29]
  cmp x2, x27
  bne _SearchDTB
  mov x1, x29


  /* Prepare the stack */
  adr x29, Stack
  add x29, x29, STACK_SIZE
  sub x29, x29, #4
  and x29, x29, #-0x10
  mov sp, x29

  /* Disable trapping floating point access in EL1. */
  mov x30, #(0x3 << 20)
  msr cpacr_el1, x30
  isb


  /*
   * Jump to C entry point
   * X0: Allocated memory size for partition
   * X1: Pointer to DTB
   */
  bl ASM_PFX(_ModuleEntryPointC)

_NeverReturn:
  b _NeverReturn

.align 12
Stack:
.skip STACK_SIZE, 0
StackEnd:

_ExceptionHandler:
  /* Find the pointer before VBAR and jump to it */
  mrs x28, vbar_el1
  sub x28, x28, 8
  ldr x28, [x28]
  blr x28

.align 12
vectors:
.skip 0x1000, 0
