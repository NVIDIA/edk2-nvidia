/**
 *  StandaloneMm specific ArmLib.h definitions.
 *
 *  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#ifndef STANDALONE_MM_ARM_LIB_H_
#define STANDALONE_MM_ARM_LIB_H_

/*
 * Extended version of ARM_MEMORY_REGION_ATTRIBUTES.
 *
 * This StandaloneMm specific version of the enum adds values for NONSECURE regions.
 * These values were originally part of ARM_MEMORY_REGION_ATTRIBUTES, but
 * removed because ArmMmuLib didn't actually distinguish between secure and
 * non-secure. However, in our implementation of StandaloneMm, which contains a
 * fork of ArmMmuLib, we need to make the distinction.
 */
typedef enum {
  // Values from ARM_MEMORY_REGION_ATTRIBUTES
  STMM_ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED = 0,
  STMM_ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK,
  STMM_ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK_NONSHAREABLE,
  STMM_ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK_RO,
  STMM_ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK_XP,
  STMM_ARM_MEMORY_REGION_ATTRIBUTE_WRITE_THROUGH,
  STMM_ARM_MEMORY_REGION_ATTRIBUTE_DEVICE,

  // Add values for NONSECURE support
  STMM_ARM_MEMORY_REGION_ATTRIBUTE_NONSECURE_UNCACHED_UNBUFFERED,
  STMM_ARM_MEMORY_REGION_ATTRIBUTE_NONSECURE_WRITE_BACK,
  STMM_ARM_MEMORY_REGION_ATTRIBUTE_NONSECURE_WRITE_BACK_NONSHAREABLE,
  STMM_ARM_MEMORY_REGION_ATTRIBUTE_NONSECURE_WRITE_THROUGH,
  STMM_ARM_MEMORY_REGION_ATTRIBUTE_NONSECURE_DEVICE
} STMM_ARM_MEMORY_REGION_ATTRIBUTES;

/*
 * Memory region descriptor that uses STMM_ARM_MEMORY_REGION_ATTRIBUTES.
 */
typedef struct {
  EFI_PHYSICAL_ADDRESS                 PhysicalBase;
  EFI_VIRTUAL_ADDRESS                  VirtualBase;
  UINT64                               Length;
  STMM_ARM_MEMORY_REGION_ATTRIBUTES    Attributes;
} STMM_ARM_MEMORY_REGION_DESCRIPTOR;

#endif // STANDALONE_MM_ARM_LIB_H_
