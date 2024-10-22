/** @file

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2015, Linaro Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

SECTIONS
{
  PROVIDE(__reloc_base = .);

  . = PECOFF_HEADER_SIZE;
  .text : ALIGN(CONSTANT(MAXPAGESIZE)) {
    *(.text .text*)
    *(.got .got*)
    *(.rodata .rodata*)
  }

  .data ALIGN(ALIGNOF(.text)) : ALIGN(CONSTANT(MAXPAGESIZE)) {
    *(.data .data*)
    *(.bss .bss*)
  }

  . = ALIGN(0x20);
  PROVIDE(__reloc_start = .);
  .rela : {
    *(.rel .rel.*)
    *(.rela .rela.*)
  }
  PROVIDE(__reloc_end = .);

  .note (INFO) : { *(.note.gnu.build-id) }

  /DISCARD/ : {
    *(.note.GNU-stack)
    *(.gnu.hash)
    *(.gnu_debuglink)
    *(.interp)
    *(.dynamic)
    *(.dynsym)
    *(.dynstr)
    *(.hash)
    *(.comment)
  }
}

