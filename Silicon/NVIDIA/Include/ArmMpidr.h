/** @file

  ARM MPIDR definitions

  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __ARM_MPIDR_H__
#define __ARM_MPIDR_H__

#define MPIDR_MT_MASK               ((1ULL) << 24)
#define MPIDR_CPU_MASK              MPIDR_AFFLVL_MASK
#define MPIDR_CLUSTER_MASK          (MPIDR_AFFLVL_MASK << MPIDR_AFFINITY_BITS)
#define MPIDR_AFFINITY_BITS         8U
#define MPIDR_AFFLVL_MASK           0xffULL
#define MPIDR_AFF0_SHIFT            0U
#define MPIDR_AFF1_SHIFT            8U
#define MPIDR_AFF2_SHIFT            16U
#define MPIDR_AFF3_SHIFT            32U
#define MPIDR_AFFINITY_MASK         0xff00ffffffULL
#define MPIDR_AFFLVL_SHIFT          3U
#define MPIDR_AFFLVL0               0x0U
#define MPIDR_AFFLVL1               0x1U
#define MPIDR_AFFLVL2               0x2U
#define MPIDR_AFFLVL3               0x3U
#define MPIDR_AFFLVL0_VAL(Mpidr) \
  ((Mpidr >> MPIDR_AFF0_SHIFT) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFFLVL1_VAL(Mpidr) \
  ((Mpidr >> MPIDR_AFF1_SHIFT) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFFLVL2_VAL(Mpidr) \
  ((Mpidr >> MPIDR_AFF2_SHIFT) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFFLVL3_VAL(Mpidr) \
  ((Mpidr >> MPIDR_AFF3_SHIFT) & MPIDR_AFFLVL_MASK)

#endif
