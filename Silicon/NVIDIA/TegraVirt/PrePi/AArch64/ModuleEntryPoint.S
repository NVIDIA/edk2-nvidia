//
//  Copyright (c) 2011-2013, ARM Limited. All rights reserved.
//  Copyright (c) 2015-2016, Linaro Limited. All rights reserved.
//  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
//  SPDX-License-Identifier: BSD-2-Clause-Patent
//
//

#include <AsmMacroIoLibV8.h>

ASM_FUNC(_ModuleEntryPoint)

  // x0 will be the beginning of the DTB
  // sp will be something we can use, but might not be sufficiently sized.

  // Share the DTB location
  adr   x8, PcdGet64 (PcdDeviceTreeInitialBaseAddress)
  str   x0, [x8]

  // Discover our MemoryBase and MemorySize from the DTB.
  // This will call a C function, so the sp must be valid.
  bl    ASM_PFX(DiscoverDramFromDt)

  // Load the MemoryBase and MemorySize discovered from the DTB.
  ldr   x1, PcdGet64 (PcdSystemMemoryBase)
  ldr   x9, PcdGet64 (PcdSystemMemorySize)

  // Use the top of memory for the stack.
  // - Leave a page we can guard to detect overflows
  sub x9, x9, #SIZE_4KB
  // - Set the stack pointer.
  add sp, x1, x9

  // Calculate the stack base and leave it in x2.
  MOV32 (x8, FixedPcdGet32(PcdCPUCorePrimaryStackSize))
  sub   x2, sp, x8

  // Zero out the MpId. We don't use this parameter.
  mov x0, xzr

  // Jump to PrePiCore C code
  //    x0 = MpId -> always zero
  //    x1 = UefiMemoryBase
  //    x2 = StacksBase
  bl    ASM_PFX(CEntryPoint)

_NeverReturn:
  b _NeverReturn

// VOID
// DiscoverDramFromDt (
//   VOID   *DeviceTreeBaseAddress   // passed by loader in x0
//   );
// Fills PcdSystemMemoryBase and PcdSystemMemorySize
ASM_PFX(DiscoverDramFromDt):

  mov   x29, x30            // preserve LR

  // Save registers
  mov   x28, x0

  // Retrieve the MemoryBase and MemorySize from the DTB's /memory node.
  adr   x1, PcdGet64 (PcdSystemMemoryBase)
  adr   x2, PcdGet64 (PcdSystemMemorySize)
  bl    FindMemnode

  // Restore registers
  mov   x0, x28

  // Return
  ret    x29
