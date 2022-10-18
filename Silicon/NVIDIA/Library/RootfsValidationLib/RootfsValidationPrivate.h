/** @file
  Rootfs Validation Private Structures.

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ROOTFSVALIDATIONPRIVATE_H__
#define __ROOTFSVALIDATIONPRIVATE_H__

/*
 * Rootfs Scratch register
 *
 * 00:15 magic 'FACE'
 * 16:17 Current rootfs slot
 * 18:19 Retry count of rootfs slot B
 * 20:21 Retry count of rootfs slot A
 * 22:31 reserved
 */
#define SR_RF_MAGIC_MASK  0x0000FFFFU
#define SR_RF_MAGIC       0xFACEU   /* 'FACE' */

#define RF_CURRENT_SLOT_SHIFT   16U
#define RF_CURRENT_SLOT_MASK    (0x03U << RF_CURRENT_SLOT_SHIFT)
#define RF_RETRY_COUNT_B_SHIFT  18U
#define RF_RETRY_COUNT_B_MASK   (0x03U << RF_RETRY_COUNT_B_SHIFT)
#define RF_RETRY_COUNT_A_SHIFT  20U
#define RF_RETRY_COUNT_A_MASK   (0x03U << RF_RETRY_COUNT_A_SHIFT)

#define SR_RF_MAGIC_GET(reg)  ((reg) & SR_RF_MAGIC_MASK)
#define SR_RF_MAGIC_SET(reg)  (((reg) & ~SR_RF_MAGIC_MASK) | SR_RF_MAGIC)

#define SR_RF_CURRENT_SLOT_GET(reg)        (((reg) & RF_CURRENT_SLOT_MASK) >> RF_CURRENT_SLOT_SHIFT)
#define SR_RF_CURRENT_SLOT_SET(slot, reg)  (((reg) & ~RF_CURRENT_SLOT_MASK) |   \
                                             (((slot) & 0x03U) << RF_CURRENT_SLOT_SHIFT))

#define SR_RF_RETRY_COUNT_B_GET(reg)         (((reg) & RF_RETRY_COUNT_B_MASK) >> RF_RETRY_COUNT_B_SHIFT)
#define SR_RF_RETRY_COUNT_B_SET(count, reg)  (((reg) & ~RF_RETRY_COUNT_B_MASK) | \
                                             (((count) & 0x03U) << RF_RETRY_COUNT_B_SHIFT))

#define SR_RF_RETRY_COUNT_A_GET(reg)         (((reg) & RF_RETRY_COUNT_A_MASK) >> RF_RETRY_COUNT_A_SHIFT)
#define SR_RF_RETRY_COUNT_A_SET(count, reg)  (((reg) & ~RF_RETRY_COUNT_A_MASK) | \
                                             (((count) & 0x03U) << RF_RETRY_COUNT_A_SHIFT))

#define ROOTFS_SLOT_A  0
#define ROOTFS_SLOT_B  1

#define FROM_REG_TO_VAR  0
#define FROM_VAR_TO_REG  1

#define DELAY_SECOND  1000000

typedef enum {
  RF_STATUS_A,
  RF_STATUS_B,

  RF_REDUNDANCY,
  RF_RETRY_MAX,

  RF_FW_NEXT,

  RF_VARIABLE_INDEX_MAX
} RF_VARIABLE_INDEX;

typedef struct {
  UINT32    Value;
  UINT32    UpdateFlag; // 1 - update, 0 - not update
} RF_VARIABLE;

typedef struct {
  RF_VARIABLE    RootfsVar[RF_VARIABLE_INDEX_MAX];
  UINT32         RetryCountSlotA;
  UINT32         RetryCountSlotB;
  UINT32         CurrentSlot;
} L4T_RF_AB_PARAM;

typedef struct {
  CHAR16      *Name;
  UINT32      Attributes;
  UINT8       Bytes;
  EFI_GUID    *Guid;
} RF_AB_VARIABLE;

#endif
