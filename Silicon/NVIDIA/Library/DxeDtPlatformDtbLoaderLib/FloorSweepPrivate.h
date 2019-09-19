/** @file
*
*  Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
*  Copyright (c) 2017, Linaro, Ltd. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#ifndef __FLOOR_SWEEP_PRIVATE_H__
#define __FLOOR_SWEEP_PRIVATE_H__

#define TEGRA_NVG_CHANNEL_NUM_CORES        20
#define TEGRA_NVG_CHANNEL_LOGICAL_TO_MPIDR 23


#define AA64_MRS(reg, var)  do { \
  asm volatile ("mrs %0, "#reg : "=r"(var) : : "memory", "cc"); \
} while (FALSE)

static inline void WriteNvgChannelIdx(UINT32 Channel)
{
  asm volatile ("msr s3_0_c15_c1_2, %0" : : "r"(Channel) : "memory", "cc");
}

static inline void WriteNvgChannelData(UINT64 Data)
{
  asm volatile ("msr s3_0_c15_c1_3, %0" : : "r"(Data) : "memory", "cc");
}

static inline UINT64 ReadNvgChannelData(void)
{
  UINT64 Reg;
  AA64_MRS(s3_0_c15_c1_3, Reg);
  return Reg;
}

EFI_STATUS
UpdateCpuFloorsweepingConfig (
  IN VOID *Dtb
  );

#endif //__FLOOR_SWEEP_PRIVATE_H__

