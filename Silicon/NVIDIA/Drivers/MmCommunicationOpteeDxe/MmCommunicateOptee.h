/** @file

  SPDX-FileCopyrightText: Copyright (c) 2022-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef MM_COMMUNICATE_OPTEE_H_
#define MM_COMMUNICATE_OPTEE_H_

#define MM_MAJOR_VER_MASK   0xEFFF0000
#define MM_MINOR_VER_MASK   0x0000FFFF
#define MM_MAJOR_VER_SHIFT  16

#define MM_MAJOR_VER(x)  (((x) & MM_MAJOR_VER_MASK) >> MM_MAJOR_VER_SHIFT)
#define MM_MINOR_VER(x)  ((x) & MM_MINOR_VER_MASK)

#define MM_CALLER_MAJOR_VER  0x1U
#define MM_CALLER_MINOR_VER  0x0U

#endif /* MM_COMMUNICATE_OPTEE_H_ */
