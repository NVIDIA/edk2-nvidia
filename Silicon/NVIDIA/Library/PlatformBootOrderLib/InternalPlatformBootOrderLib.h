/** @file
*
*  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _INTERNAL_PLATFORM_BOOT_ORDER_LIB_H_
#define _INTERNAL_PLATFORM_BOOT_ORDER_LIB_H_

#ifdef EDKII_UNIT_TEST_FRAMEWORK_ENABLED
  #undef DEBUG_ERROR
#define DEBUG_ERROR  DEBUG_INFO
  #undef DEBUG_WARN
#define DEBUG_WARN  DEBUG_INFO
#endif

// Frees a pointer if non-NULL, and sets it to NULL
#define FREE_NON_NULL(a) \
  if ((a) != NULL) { \
    FreePool ((a));  \
    (a) = NULL;      \
  }

#define NVIDIA_BOOT_TYPE_HTTP     0
#define NVIDIA_BOOT_TYPE_BOOTIMG  1
#define NVIDIA_BOOT_TYPE_VIRTUAL  2

typedef struct {
  CHAR8    *OrderName;
  INT32    PriorityOrder;
  UINT8    Type;
  UINT8    SubType;
  UINT8    ExtraSpecifier;
  UINTN    SegmentNum;
  UINTN    BusNum;
  UINTN    DevNum;
  UINTN    FuncNum;
} NVIDIA_BOOT_ORDER_PRIORITY;

#define DEFAULT_BOOT_ORDER_STRING   "boot.img,nvme,usb,sd,emmc,ufs"
#define BOOT_ORDER_CLASS_SEPARATOR  ','
#define BOOT_ORDER_SBDF_SEPARATOR   ':'
#define BOOT_ORDER_SBDF_STARTER     '|'
#define BOOT_ORDER_TERMINATOR       '\0'

#define BOOT_ORDER_TEMPLATE_CLASS_COUNT  15
extern NVIDIA_BOOT_ORDER_PRIORITY  mBootPriorityTemplate[BOOT_ORDER_TEMPLATE_CLASS_COUNT];

VOID
PrintBootOrder (
  IN CONST UINTN   DebugPrintLevel,
  IN CONST CHAR16  *HeaderMessage,
  IN UINT16        *BootOrder,
  IN UINTN         BootOrderSize
  );

NVIDIA_BOOT_ORDER_PRIORITY *
EFIAPI
GetBootClassOfOption (
  IN EFI_BOOT_MANAGER_LOAD_OPTION  *Option,
  IN NVIDIA_BOOT_ORDER_PRIORITY    *Table,
  IN UINTN                         Count
  );

EFI_STATUS
EFIAPI
GetBootClassOfOptionNum (
  IN UINT16                          OptionNum,
  IN OUT NVIDIA_BOOT_ORDER_PRIORITY  **Class,
  IN NVIDIA_BOOT_ORDER_PRIORITY      *Table,
  IN UINTN                           Count
  );

NVIDIA_BOOT_ORDER_PRIORITY *
EFIAPI
GetBootClassOfName (
  CHAR8                          *ClassName,
  UINTN                          ClassNameLen,
  IN NVIDIA_BOOT_ORDER_PRIORITY  *Table,
  IN UINTN                       Count
  );

#endif
