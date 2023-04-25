/** @file
  Unit tests of the IPMI portion of the PlatformBootOrder library.

  Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformBootOrderLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/HostBasedTestStubLib/IpmiStubLib.h>
#include <Library/UefiBootManagerLib.h>
#include <HostBasedTestStubLib/UefiRuntimeServicesTableStubLib.h>
#include <HostBasedTestStubLib/UefiBootServicesTableStubLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UnitTestLib.h>
#include <Guid/GlobalVariable.h>
#include <Library/IpmiCommandLib.h>
#include <Library/DevicePathLib.h>

#define UNIT_TEST_NAME     "IPMI Boot Order Test"
#define UNIT_TEST_VERSION  "1.0"

#define FREE_NON_NULL(a) \
  if ((a) != NULL) \
  { \
    FreePool ((a)); \
    (a) = NULL; \
  }

#define SAVED_BOOT_ORDER_VARIABLE_NAME  L"SavedBootOrder"

extern UINT8  mIpmiCommandCounter;

typedef enum {
  IBO_DEVICE_NO_CHANGE     = 0b0000, // No Change
  IBO_DEVICE_PXE           = 0b0001, // Pxe
  IBO_DEVICE_HD            = 0b0010, // Nvme
  IBO_DEVICE_HD_SAFE       = 0b0011,
  IBO_DEVICE_DIAG          = 0b0100,
  IBO_DEVICE_CD            = 0b0101, // Cdrom
  IBO_DEVICE_BIOS          = 0b0110, // UEFI Menu
  IBO_DEVICE_REMOTE_FLOPPY = 0b0111, // Sata
  IBO_DEVICE_REMOTE_CD     = 0b1000, // Http
  IBO_DEVICE_REMOTE_MEDIA  = 0b1001, // BMC's USB
  IBO_DEVICE_RESERVED_0    = 0b1010,
  IBO_DEVICE_REMOTE_HD     = 0b1011, // Scsi
  IBO_DEVICE_RESERVED_1    = 0b1100,
  IBO_DEVICE_RESERVED_2    = 0b1101,
  IBO_DEVICE_RESERVED_3    = 0b1110,
  IBO_DEVICE_FLOPPY        = 0b1111  // USB
} IBO_DEVICE;

typedef enum {
  IBO_RESULT_NO_CHANGE,
  IBO_RESULT_BOOT_ORDER_CHANGE,
  IBO_RESULT_BOOT_NEXT_CHANGE,
  IBO_RESULT_OS_INDICATIONS_CHANGE
} IBO_TEST_RESULT;

typedef struct {
  IBO_DEVICE         Device;
  UINT8              Instance;
  IBO_TEST_RESULT    Result;
  BOOLEAN            AlreadyAcked;
  BOOLEAN            Valid;
} IBO_CONTEXT;

// Device 0, Persistent, Unacked, Valid
IBO_CONTEXT  NO_CHANGE_0     = { IBO_DEVICE_NO_CHANGE, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  PXE_0           = { IBO_DEVICE_PXE, 0, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  HD_0            = { IBO_DEVICE_HD, 0, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  HD_SAFE_0       = { IBO_DEVICE_HD_SAFE, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  DIAG_0          = { IBO_DEVICE_DIAG, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  CD_0            = { IBO_DEVICE_CD, 0, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  BIOS_0          = { IBO_DEVICE_BIOS, 0, IBO_RESULT_OS_INDICATIONS_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_FLOPPY_0 = { IBO_DEVICE_REMOTE_FLOPPY, 0, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_CD_0     = { IBO_DEVICE_REMOTE_CD, 0, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_MEDIA_0  = { IBO_DEVICE_REMOTE_MEDIA, 0, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_0_0    = { IBO_DEVICE_RESERVED_0, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_HD_0     = { IBO_DEVICE_REMOTE_HD, 0, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_1_0    = { IBO_DEVICE_RESERVED_1, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_2_0    = { IBO_DEVICE_RESERVED_2, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_3_0    = { IBO_DEVICE_RESERVED_3, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  FLOPPY_0        = { IBO_DEVICE_FLOPPY, 0, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };

// Device 1, Persistent, Unacked, Valid
IBO_CONTEXT  NO_CHANGE_1     = { IBO_DEVICE_NO_CHANGE, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  PXE_1           = { IBO_DEVICE_PXE, 1, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  HD_1            = { IBO_DEVICE_HD, 1, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  HD_SAFE_1       = { IBO_DEVICE_HD_SAFE, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  DIAG_1          = { IBO_DEVICE_DIAG, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  CD_1            = { IBO_DEVICE_CD, 1, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  BIOS_1          = { IBO_DEVICE_BIOS, 1, IBO_RESULT_OS_INDICATIONS_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_FLOPPY_1 = { IBO_DEVICE_REMOTE_FLOPPY, 1, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_CD_1     = { IBO_DEVICE_REMOTE_CD, 1, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_MEDIA_1  = { IBO_DEVICE_REMOTE_MEDIA, 1, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_0_1    = { IBO_DEVICE_RESERVED_0, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_HD_1     = { IBO_DEVICE_REMOTE_HD, 1, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_1_1    = { IBO_DEVICE_RESERVED_1, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_2_1    = { IBO_DEVICE_RESERVED_2, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_3_1    = { IBO_DEVICE_RESERVED_3, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  FLOPPY_1        = { IBO_DEVICE_FLOPPY, 1, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };

// Device 2, Persistent, Unacked, Valid
IBO_CONTEXT  NO_CHANGE_2     = { IBO_DEVICE_NO_CHANGE, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  PXE_2           = { IBO_DEVICE_PXE, 2, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  HD_2            = { IBO_DEVICE_HD, 2, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  HD_SAFE_2       = { IBO_DEVICE_HD_SAFE, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  DIAG_2          = { IBO_DEVICE_DIAG, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  CD_2            = { IBO_DEVICE_CD, 2, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  BIOS_2          = { IBO_DEVICE_BIOS, 2, IBO_RESULT_OS_INDICATIONS_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_FLOPPY_2 = { IBO_DEVICE_REMOTE_FLOPPY, 2, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_CD_2     = { IBO_DEVICE_REMOTE_CD, 2, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_MEDIA_2  = { IBO_DEVICE_REMOTE_MEDIA, 2, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_0_2    = { IBO_DEVICE_RESERVED_0, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_HD_2     = { IBO_DEVICE_REMOTE_HD, 2, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_1_2    = { IBO_DEVICE_RESERVED_1, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_2_2    = { IBO_DEVICE_RESERVED_2, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_3_2    = { IBO_DEVICE_RESERVED_3, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  FLOPPY_2        = { IBO_DEVICE_FLOPPY, 2, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };

// Device 3, Persistent, Unacked, Valid
IBO_CONTEXT  NO_CHANGE_3     = { IBO_DEVICE_NO_CHANGE, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  PXE_3           = { IBO_DEVICE_PXE, 3, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  HD_3            = { IBO_DEVICE_HD, 3, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  HD_SAFE_3       = { IBO_DEVICE_HD_SAFE, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  DIAG_3          = { IBO_DEVICE_DIAG, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  CD_3            = { IBO_DEVICE_CD, 3, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  BIOS_3          = { IBO_DEVICE_BIOS, 3, IBO_RESULT_OS_INDICATIONS_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_FLOPPY_3 = { IBO_DEVICE_REMOTE_FLOPPY, 3, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_CD_3     = { IBO_DEVICE_REMOTE_CD, 3, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_MEDIA_3  = { IBO_DEVICE_REMOTE_MEDIA, 3, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_0_3    = { IBO_DEVICE_RESERVED_0, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  REMOTE_HD_3     = { IBO_DEVICE_REMOTE_HD, 3, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_1_3    = { IBO_DEVICE_RESERVED_1, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_2_3    = { IBO_DEVICE_RESERVED_2, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  RESERVED_3_3    = { IBO_DEVICE_RESERVED_3, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  FLOPPY_3        = { IBO_DEVICE_FLOPPY, 3, IBO_RESULT_BOOT_ORDER_CHANGE, FALSE, TRUE };

// Acked
IBO_CONTEXT  ACKED_PXE_0 = { IBO_DEVICE_PXE, 0, IBO_RESULT_NO_CHANGE, TRUE, TRUE };

IBO_CONTEXT  ACKED_PXE_2           = { IBO_DEVICE_PXE, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_HD_2            = { IBO_DEVICE_HD, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_HD_SAFE_2       = { IBO_DEVICE_HD_SAFE, 2, IBO_RESULT_NO_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_DIAG_2          = { IBO_DEVICE_DIAG, 2, IBO_RESULT_NO_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_CD_2            = { IBO_DEVICE_CD, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_BIOS_2          = { IBO_DEVICE_BIOS, 2, IBO_RESULT_OS_INDICATIONS_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_REMOTE_FLOPPY_2 = { IBO_DEVICE_REMOTE_FLOPPY, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_REMOTE_CD_2     = { IBO_DEVICE_REMOTE_CD, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_REMOTE_MEDIA_2  = { IBO_DEVICE_REMOTE_MEDIA, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_RESERVED_0_2    = { IBO_DEVICE_RESERVED_0, 2, IBO_RESULT_NO_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_REMOTE_HD_2     = { IBO_DEVICE_REMOTE_HD, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_RESERVED_1_2    = { IBO_DEVICE_RESERVED_1, 2, IBO_RESULT_NO_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_RESERVED_2_2    = { IBO_DEVICE_RESERVED_2, 2, IBO_RESULT_NO_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_RESERVED_3_2    = { IBO_DEVICE_RESERVED_3, 2, IBO_RESULT_NO_CHANGE, TRUE, TRUE };
IBO_CONTEXT  ACKED_FLOPPY_2        = { IBO_DEVICE_FLOPPY, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, TRUE };

// Invalid
IBO_CONTEXT  INVALID_PXE_0 = { IBO_DEVICE_PXE, 0, IBO_RESULT_NO_CHANGE, TRUE, FALSE };

IBO_CONTEXT  INVALID_PXE_2           = { IBO_DEVICE_PXE, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_HD_2            = { IBO_DEVICE_HD, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_HD_SAFE_2       = { IBO_DEVICE_HD_SAFE, 2, IBO_RESULT_NO_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_DIAG_2          = { IBO_DEVICE_DIAG, 2, IBO_RESULT_NO_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_CD_2            = { IBO_DEVICE_CD, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_BIOS_2          = { IBO_DEVICE_BIOS, 2, IBO_RESULT_OS_INDICATIONS_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_REMOTE_FLOPPY_2 = { IBO_DEVICE_REMOTE_FLOPPY, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_REMOTE_CD_2     = { IBO_DEVICE_REMOTE_CD, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_REMOTE_MEDIA_2  = { IBO_DEVICE_REMOTE_MEDIA, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_RESERVED_0_2    = { IBO_DEVICE_RESERVED_0, 2, IBO_RESULT_NO_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_REMOTE_HD_2     = { IBO_DEVICE_REMOTE_HD, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_RESERVED_1_2    = { IBO_DEVICE_RESERVED_1, 2, IBO_RESULT_NO_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_RESERVED_2_2    = { IBO_DEVICE_RESERVED_2, 2, IBO_RESULT_NO_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_RESERVED_3_2    = { IBO_DEVICE_RESERVED_3, 2, IBO_RESULT_NO_CHANGE, TRUE, FALSE };
IBO_CONTEXT  INVALID_FLOPPY_2        = { IBO_DEVICE_FLOPPY, 2, IBO_RESULT_BOOT_ORDER_CHANGE, TRUE, FALSE };

// Device 0, Next, Unacked, Valid
IBO_CONTEXT  NEXT_NO_CHANGE_0     = { IBO_DEVICE_NO_CHANGE, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_PXE_0           = { IBO_DEVICE_PXE, 0, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_HD_0            = { IBO_DEVICE_HD, 0, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_HD_SAFE_0       = { IBO_DEVICE_HD_SAFE, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_DIAG_0          = { IBO_DEVICE_DIAG, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_CD_0            = { IBO_DEVICE_CD, 0, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_BIOS_0          = { IBO_DEVICE_BIOS, 0, IBO_RESULT_OS_INDICATIONS_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_FLOPPY_0 = { IBO_DEVICE_REMOTE_FLOPPY, 0, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_CD_0     = { IBO_DEVICE_REMOTE_CD, 0, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_MEDIA_0  = { IBO_DEVICE_REMOTE_MEDIA, 0, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_0_0    = { IBO_DEVICE_RESERVED_0, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_HD_0     = { IBO_DEVICE_REMOTE_HD, 0, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_1_0    = { IBO_DEVICE_RESERVED_1, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_2_0    = { IBO_DEVICE_RESERVED_2, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_3_0    = { IBO_DEVICE_RESERVED_3, 0, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_FLOPPY_0        = { IBO_DEVICE_FLOPPY, 0, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };

// Device 1, Next, Unacked, Valid
IBO_CONTEXT  NEXT_NO_CHANGE_1     = { IBO_DEVICE_NO_CHANGE, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_PXE_1           = { IBO_DEVICE_PXE, 1, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_HD_1            = { IBO_DEVICE_HD, 1, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_HD_SAFE_1       = { IBO_DEVICE_HD_SAFE, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_DIAG_1          = { IBO_DEVICE_DIAG, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_CD_1            = { IBO_DEVICE_CD, 1, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_BIOS_1          = { IBO_DEVICE_BIOS, 1, IBO_RESULT_OS_INDICATIONS_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_FLOPPY_1 = { IBO_DEVICE_REMOTE_FLOPPY, 1, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_CD_1     = { IBO_DEVICE_REMOTE_CD, 1, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_MEDIA_1  = { IBO_DEVICE_REMOTE_MEDIA, 1, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_0_1    = { IBO_DEVICE_RESERVED_0, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_HD_1     = { IBO_DEVICE_REMOTE_HD, 1, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_1_1    = { IBO_DEVICE_RESERVED_1, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_2_1    = { IBO_DEVICE_RESERVED_2, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_3_1    = { IBO_DEVICE_RESERVED_3, 1, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_FLOPPY_1        = { IBO_DEVICE_FLOPPY, 1, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };

// Device 2, Next, Unacked, Valid
IBO_CONTEXT  NEXT_NO_CHANGE_2     = { IBO_DEVICE_NO_CHANGE, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_PXE_2           = { IBO_DEVICE_PXE, 2, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_HD_2            = { IBO_DEVICE_HD, 2, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_HD_SAFE_2       = { IBO_DEVICE_HD_SAFE, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_DIAG_2          = { IBO_DEVICE_DIAG, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_CD_2            = { IBO_DEVICE_CD, 2, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_BIOS_2          = { IBO_DEVICE_BIOS, 2, IBO_RESULT_OS_INDICATIONS_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_FLOPPY_2 = { IBO_DEVICE_REMOTE_FLOPPY, 2, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_CD_2     = { IBO_DEVICE_REMOTE_CD, 2, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_MEDIA_2  = { IBO_DEVICE_REMOTE_MEDIA, 2, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_0_2    = { IBO_DEVICE_RESERVED_0, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_HD_2     = { IBO_DEVICE_REMOTE_HD, 2, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_1_2    = { IBO_DEVICE_RESERVED_1, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_2_2    = { IBO_DEVICE_RESERVED_2, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_3_2    = { IBO_DEVICE_RESERVED_3, 2, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_FLOPPY_2        = { IBO_DEVICE_FLOPPY, 2, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };

// Device 3, Next, Unacked, Valid
IBO_CONTEXT  NEXT_NO_CHANGE_3     = { IBO_DEVICE_NO_CHANGE, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_PXE_3           = { IBO_DEVICE_PXE, 3, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_HD_3            = { IBO_DEVICE_HD, 3, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_HD_SAFE_3       = { IBO_DEVICE_HD_SAFE, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_DIAG_3          = { IBO_DEVICE_DIAG, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_CD_3            = { IBO_DEVICE_CD, 3, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_BIOS_3          = { IBO_DEVICE_BIOS, 3, IBO_RESULT_OS_INDICATIONS_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_FLOPPY_3 = { IBO_DEVICE_REMOTE_FLOPPY, 3, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_CD_3     = { IBO_DEVICE_REMOTE_CD, 3, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_MEDIA_3  = { IBO_DEVICE_REMOTE_MEDIA, 3, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_0_3    = { IBO_DEVICE_RESERVED_0, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_REMOTE_HD_3     = { IBO_DEVICE_REMOTE_HD, 3, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_1_3    = { IBO_DEVICE_RESERVED_1, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_2_3    = { IBO_DEVICE_RESERVED_2, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_RESERVED_3_3    = { IBO_DEVICE_RESERVED_3, 3, IBO_RESULT_NO_CHANGE, FALSE, TRUE };
IBO_CONTEXT  NEXT_FLOPPY_3        = { IBO_DEVICE_FLOPPY, 3, IBO_RESULT_BOOT_NEXT_CHANGE, FALSE, TRUE };

UINTN      ExpectedBootOrderSize;
UINTN      ExpectedSavedBootOrderSize;
UINT16     *ExpectedBootOrder;
UINT16     *ExpectedSavedBootOrder;
UINT64     ExpectedOsIndications;
UINT32     NextOptionNumber;
EFI_EVENT  EventSavePtr;

// To set up BootOrder, SavedBootOrder, and OsIndications
UNIT_TEST_STATUS
EFIAPI
SetupUefiVariables (
  IN  UINTN   BootOrderSize,
  IN  VOID    *BootOrderData,
  IN  VOID    *SavedBootOrderData,
  IN  UINT64  OsIndications
  )
{
  EFI_STATUS  Status;

  NextOptionNumber = 0;

  Status = gRT->SetVariable (
                  EFI_BOOT_ORDER_VARIABLE_NAME,
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  BootOrderSize,
                  BootOrderData
                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  if (SavedBootOrderData) {
    Status = gRT->SetVariable (
                    SAVED_BOOT_ORDER_VARIABLE_NAME,
                    &gNVIDIATokenSpaceGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    BootOrderSize,
                    SavedBootOrderData
                    );
    UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  }

  Status = gRT->SetVariable (
                  EFI_OS_INDICATIONS_VARIABLE_NAME,
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  sizeof (OsIndications),
                  &OsIndications
                  );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  EventSavePtr = NULL;

  return UNIT_TEST_PASSED;
}

/**
  Check Results of test vs expected

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
IBO_CheckResults (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINTN       BootOrderSize;
  VOID        *BootOrderData;
  VOID        *SavedBootOrderData;
  UINT64      OsIndications;
  UINT32      Attributes;
  UINTN       VariableSize;
  UINTN       Index;

  BootOrderSize = ExpectedBootOrderSize;
  if (ExpectedBootOrder == NULL) {
    BootOrderData = NULL;
  } else {
    BootOrderData = AllocatePool (BootOrderSize);
    UT_ASSERT_NOT_NULL (BootOrderData);
  }

  if (ExpectedSavedBootOrder == NULL) {
    SavedBootOrderData = NULL;
  } else {
    SavedBootOrderData = AllocatePool (BootOrderSize);
    UT_ASSERT_NOT_NULL (SavedBootOrderData);
  }

  Status = gRT->GetVariable (
                  EFI_BOOT_ORDER_VARIABLE_NAME,
                  &gEfiGlobalVariableGuid,
                  &Attributes,
                  &BootOrderSize,
                  BootOrderData
                  );
  if (ExpectedBootOrderSize == 0) {
    UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);
  } else {
    UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
    UT_ASSERT_EQUAL (BootOrderSize, ExpectedBootOrderSize);
    for (Index = 0; Index < BootOrderSize/sizeof (UINT16); Index++) {
      DEBUG ((DEBUG_INFO, "BO[%u]=0x%x, EBO=0x%x\n", Index, ((UINT16 *)BootOrderData)[Index], ExpectedBootOrder[Index]));
    }

    UT_ASSERT_MEM_EQUAL (BootOrderData, ExpectedBootOrder, ExpectedBootOrderSize);
  }

  Status = gRT->GetVariable (
                  SAVED_BOOT_ORDER_VARIABLE_NAME,
                  &gNVIDIATokenSpaceGuid,
                  &Attributes,
                  &BootOrderSize,
                  SavedBootOrderData
                  );
  if (ExpectedSavedBootOrderSize == 0) {
    UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);
  } else {
    UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
    UT_ASSERT_EQUAL (BootOrderSize, ExpectedSavedBootOrderSize);
    for (Index = 0; Index < BootOrderSize/sizeof (UINT16); Index++) {
      DEBUG ((DEBUG_INFO, "SBO[%u]=0x%x, ESBO=0x%x\n", Index, ((UINT16 *)SavedBootOrderData)[Index], ExpectedSavedBootOrder[Index]));
    }

    UT_ASSERT_MEM_EQUAL (SavedBootOrderData, ExpectedSavedBootOrder, ExpectedSavedBootOrderSize);
  }

  VariableSize = sizeof (OsIndications);
  Status       = gRT->GetVariable (
                        EFI_OS_INDICATIONS_VARIABLE_NAME,
                        &gEfiGlobalVariableGuid,
                        &Attributes,
                        &VariableSize,
                        &OsIndications
                        );
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (VariableSize, sizeof (OsIndications));
  UT_ASSERT_EQUAL (OsIndications, ExpectedOsIndications);

  // check that BootOrder was properly restored
  if (EventSavePtr != NULL) {
    Status = gRT->SetVariable (
                    L"BootCurrent",
                    &gEfiGlobalVariableGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                    sizeof (UINT16),
                    &ExpectedBootOrder[0]
                    );
    UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

    gBS->SignalEvent (EventSavePtr);

    // Check that boot order was restored properly
    Status = gRT->GetVariable (
                    EFI_BOOT_ORDER_VARIABLE_NAME,
                    &gEfiGlobalVariableGuid,
                    &Attributes,
                    &BootOrderSize,
                    BootOrderData
                    );
    if (ExpectedBootOrderSize == 0) {
      UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);
    } else {
      UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
      UT_ASSERT_EQUAL (BootOrderSize, ExpectedBootOrderSize);
      for (Index = 0; Index < BootOrderSize/sizeof (UINT16); Index++) {
        DEBUG ((DEBUG_INFO, "RBO[%u]=0x%x, ESBO=0x%x\n", Index, ((UINT16 *)BootOrderData)[Index], ExpectedSavedBootOrder[Index]));
      }

      UT_ASSERT_MEM_EQUAL (BootOrderData, ExpectedSavedBootOrder, ExpectedBootOrderSize);
    }

    Status = gRT->GetVariable (
                    SAVED_BOOT_ORDER_VARIABLE_NAME,
                    &gNVIDIATokenSpaceGuid,
                    &Attributes,
                    &BootOrderSize,
                    SavedBootOrderData
                    );
    UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);
  }

  FREE_NON_NULL (BootOrderData);

  UT_ASSERT_EQUAL (mIpmiCommandCounter, 0);
  return UNIT_TEST_PASSED;
}

/**
  Empty UEFI variables Setup

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
IBO_EmptyBootOrderSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  ExpectedBootOrderSize      = 0;
  ExpectedSavedBootOrderSize = 0;
  ExpectedBootOrder          = NULL;
  ExpectedOsIndications      = 0;

  return SetupUefiVariables (0, NULL, NULL, 0);
}

/*
  UINT8    Type;    ///< 0x01 Hardware Device Path.
                    ///< 0x02 ACPI Device Path.
                    ///< 0x03 Messaging Device Path.
                    ///< 0x04 Media Device Path.
                    ///< 0x05 BIOS Boot Specification Device Path.
                    ///< 0x7F End of Hardware Device Path.

  UINT8    SubType; ///< Varies by Type
                    ///< 0xFF End Entire Device Path, or
                    ///< 0x01 End This Instance of a Device Path and start a new
                    ///< Device Path.

// Boot order seen in TH500 sim

//Media MEDIA_PIWG_FW_VOL_DP
    OptionNumber=0x0, OptionType=0x2
    Type=0x4, Subtype=0x7, length[0]=0x14, length[1]=0x0
    OptionalDataSize=0x0

//Media MEDIA_RAM_DISK_DP
    OptionNumber=0x1, OptionType=0x2
    Type=0x4, Subtype=0x9, length[0]=0x26, length[1]=0x0
    OptionalDataSize=0x10

//Hardware HW_VENDOR_DP
    OptionNumber=0x2, OptionType=0x2
    Type=0x1, Subtype=0x4, length[0]=0x14, length[1]=0x0
    OptionalDataSize=0x10

//Hardware HW_VENDOR_DP
    OptionNumber=0x3, OptionType=0x2
    Type=0x1, Subtype=0x4, length[0]=0x14, length[1]=0x0
    OptionalDataSize=0x10

//Media MEDIA_PIWG_FW_VOL_DP
    OptionNumber=0x4, OptionType=0x2
    Type=0x4, Subtype=0x7, length[0]=0x14, length[1]=0x0
    OptionalDataSize=0x0

//Media MEDIA_PIWG_FW_VOL_DP
    OptionNumber=0x5, OptionType=0x2
    Type=0x4, Subtype=0x7, length[0]=0x14, length[1]=0x0
    OptionalDataSize=0x0
*/

/* actual system devices on TH500

//Media MEDIA_HARDDRIVE_DP
EFI Boot Option
    OptionNumber=0x3, OptionType=0x2
    Type=0x4, Subtype=0x1, length[0]=0x2A, length[1]=0x0
    Path=HD(1,GPT,860E734B-5D2B-4736-821F-EA0BE3B36BCF,0x800,0x100000)/\EFI\ubuntu\shimaa64.efi
    OptionalDataSize=0x0

//Media MEDIA_PIWG_FW_VOL_DP
EFI Boot Option
    OptionNumber=0x5, OptionType=0x2
    Type=0x4, Subtype=0x7, length[0]=0x14, length[1]=0x0
    Path=Fv(9AEF2E52-DEAD-4F63-B895-3A504A3E63C4)/FvFile(462CAA21-7614-4503-836E-8AB6F4662331)
    OptionalDataSize=0x0

//Media MEDIA_PIWG_FW_VOL_DP
EFI Boot Option
    OptionNumber=0x7, OptionType=0x2
    Type=0x4, Subtype=0x7, length[0]=0x14, length[1]=0x0
    Path=Fv(9AEF2E52-DEAD-4F63-B895-3A504A3E63C4)/FvFile(EEC25BDC-67F2-4D95-B1D5-F81B2039D11D)
    OptionalDataSize=0x0

//Media MEDIA_PIWG_FW_VOL_DP
EFI Boot Option
    OptionNumber=0x8, OptionType=0x2
    Type=0x4, Subtype=0x7, length[0]=0x14, length[1]=0x0
    Path=Fv(9AEF2E52-DEAD-4F63-B895-3A504A3E63C4)/FvFile(7C04A583-9E3E-4F1C-AD65-E05268D0B4D1)
    OptionalDataSize=0x0

//Hardware HW_VENDOR_DP - nvme [HD 0100]
EFI Boot Option
    OptionNumber=0x0, OptionType=0x2
    Type=0x1, Subtype=0x4, length[0]=0x14, length[1]=0x0
    Path=VenHw(1E5A432C-0466-4D31-B009-D4D9239271D3)/MemoryMapped(0xB,0x14180000,0x14181FFF)/PciRoot(0x0)/Pci(0x0,0x0)/Pci(0x0,0x0)/Pci(0x0,0x0)/Pci(0x0,0x0)/NVMe(0x1,F1-90-1A-3A-01-75-A0-00)
    OptionalDataSize=0x10

//Hardware HW_VENDOR_DP - pxev4 [PXE 0001]
EFI Boot Option
    OptionNumber=0x1, OptionType=0x2
    Type=0x1, Subtype=0x4, length[0]=0x14, length[1]=0x0
    Path=VenHw(1E5A432C-0466-4D31-B009-D4D9239271D3)/MemoryMapped(0xB,0x14180000,0x14181FFF)/PciRoot(0x0)/Pci(0x0,0x0)/Pci(0x0,0x0)/Pci(0x5,0x0)/Pci(0x0,0x0)/MAC(48B02DB14D21,0x0)/IPv4(0.0.0.0)
    OptionalDataSize=0x10

//Hardware HW_VENDOR_DP - pxev6 [PXE 0001]
EFI Boot Option
    OptionNumber=0x2, OptionType=0x2
    Type=0x1, Subtype=0x4, length[0]=0x14, length[1]=0x0
    Path=VenHw(1E5A432C-0466-4D31-B009-D4D9239271D3)/MemoryMapped(0xB,0x14180000,0x14181FFF)/PciRoot(0x0)/Pci(0x0,0x0)/Pci(0x0,0x0)/Pci(0x5,0x0)/Pci(0x0,0x0)/MAC(48B02DB14D21,0x0)/IPv6(0000:0000:0000:0000:0000:0000:0000:0000)
    OptionalDataSize=0x10

//Hardware HW_VENDOR_DP - httpv4 [Remote CD 1000]
EFI Boot Option
    OptionNumber=0x4, OptionType=0x2
    Type=0x1, Subtype=0x4, length[0]=0x14, length[1]=0x0
    Path=VenHw(1E5A432C-0466-4D31-B009-D4D9239271D3)/MemoryMapped(0xB,0x14180000,0x14181FFF)/PciRoot(0x0)/Pci(0x0,0x0)/Pci(0x0,0x0)/Pci(0x5,0x0)/Pci(0x0,0x0)/MAC(48B02DB14D21,0x0)/IPv4(0.0.0.0)/Uri()
    OptionalDataSize=0x10

//Hardware HW_VENDOR_DP - httpv6 [Remote CD 1000]
EFI Boot Option
    OptionNumber=0x6, OptionType=0x2
    Type=0x1, Subtype=0x4, length[0]=0x14, length[1]=0x0
    Path=VenHw(1E5A432C-0466-4D31-B009-D4D9239271D3)/MemoryMapped(0xB,0x14180000,0x14181FFF)/PciRoot(0x0)/Pci(0x0,0x0)/Pci(0x0,0x0)/Pci(0x5,0x0)/Pci(0x0,0x0)/MAC(48B02DB14D21,0x0)/IPv6(0000:0000:0000:0000:0000:0000:0000:0000)/Uri()
    OptionalDataSize=0x10
    */
UNIT_TEST_STATUS
EFIAPI
IBO_AddDP (
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath,
  UINT16                    *BootNum
  )
{
  EFI_STATUS                    Status;
  EFI_BOOT_MANAGER_LOAD_OPTION  Option;

  ZeroMem (&Option, sizeof (Option));
  Option.FilePath     = DevicePath;
  Option.OptionType   = LoadOptionTypeBoot;
  Option.OptionNumber = NextOptionNumber++;
  *BootNum            = Option.OptionNumber;

  Status = EfiBootManagerLoadOptionToVariable (&Option);
  if (EFI_ERROR (Status)) {
    return UNIT_TEST_ERROR_TEST_FAILED;
  }

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
IBO_AddUsbDP (
  UINT8   Port,
  UINT8   Interface,
  UINT16  *BootNum
  )
{
  USB_DEVICE_PATH  DevicePath[2];

  DevicePath[0].Header.Type      = MESSAGING_DEVICE_PATH;
  DevicePath[0].Header.SubType   = MSG_USB_DP;
  DevicePath[0].Header.Length[0] = sizeof (DevicePath[0]);
  DevicePath[0].ParentPortNumber = Port;
  DevicePath[0].InterfaceNumber  = Interface;
  DevicePath[1].Header.Type      = END_DEVICE_PATH_TYPE;
  DevicePath[1].Header.SubType   = END_ENTIRE_DEVICE_PATH_SUBTYPE;
  DevicePath[1].Header.Length[0] = END_DEVICE_PATH_LENGTH;

  if (!IsDevicePathValid (&DevicePath[0].Header, sizeof (DevicePath))) {
    DEBUG ((DEBUG_ERROR, "DevicePath isn't valid!\n"));
    return UNIT_TEST_ERROR_TEST_FAILED;
  }

  return IBO_AddDP (&DevicePath[0].Header, BootNum);
}

// Note: Currently this only puts USB devices into the boot order
UNIT_TEST_STATUS
EFIAPI
IBO_BootOrderSetup (
  IN UNIT_TEST_CONTEXT  Context,
  IN UINTN              Count
  )
{
  UNIT_TEST_STATUS  Status;
  UINT16            BootNum;
  IBO_CONTEXT       *IboContext = (IBO_CONTEXT *)Context;
  UINTN             Index;
  BOOLEAN           WillModifyBootOrder;

  // Set up Initial Boot Order

  ExpectedBootOrderSize = Count*sizeof (UINT16);
  ExpectedBootOrder     = AllocatePool (ExpectedBootOrderSize);
  if (ExpectedBootOrder == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for ExpectedBootOrder\n", __FUNCTION__));
    return UNIT_TEST_ERROR_TEST_FAILED;
  }

  for (Index = 0; Index < Count; Index++) {
    Status = IBO_AddUsbDP (0, 0, &BootNum);
    if (Status != UNIT_TEST_PASSED) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to add instance %u of Usb DP\n", __FUNCTION__, Index));
      return Status;
    }

    ExpectedBootOrder[Index] = BootNum;
  }

  // Create initial state, before expectations take effect
  Status = SetupUefiVariables (ExpectedBootOrderSize, ExpectedBootOrder, NULL, 0);
  if (Status != UNIT_TEST_PASSED) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to setup Uefi variables\n", __FUNCTION__));
    return Status;
  }

  // Determine Expected Boot Order

  ExpectedSavedBootOrderSize = 0;
  // JDS TODO - this assumes only USB(Floppy) in boot order!
  if ((IboContext->Instance > 1) && (IboContext->Device == IBO_DEVICE_FLOPPY) && (IboContext->Instance <= Count) && !IboContext->AlreadyAcked && IboContext->Valid) {
    WillModifyBootOrder = TRUE;
  } else {
    WillModifyBootOrder = FALSE;
  }

  switch (IboContext->Result) {
    case IBO_RESULT_NO_CHANGE:
      ExpectedOsIndications = 0;
      break;

    case IBO_RESULT_BOOT_NEXT_CHANGE:
      if (WillModifyBootOrder) {
        ExpectedSavedBootOrder = AllocatePool (ExpectedBootOrderSize);
        if (ExpectedSavedBootOrder == NULL) {
          return UNIT_TEST_ERROR_TEST_FAILED;
        }

        CopyMem (&ExpectedSavedBootOrder[0], &ExpectedBootOrder[0], ExpectedBootOrderSize);
        ExpectedSavedBootOrderSize = ExpectedBootOrderSize;
        // Note: MockUefiCreateEventEx must be called in the test, not the setup, due to how mock checking works
      }

    // fall through now that we've saved boot order

    case IBO_RESULT_BOOT_ORDER_CHANGE:
      ExpectedOsIndications = 0;
      if (WillModifyBootOrder) {
        BootNum = ExpectedBootOrder[IboContext->Instance-1];
        CopyMem (&ExpectedBootOrder[1], &ExpectedBootOrder[0], sizeof (ExpectedBootOrder[0])*(IboContext->Instance - 1));
        ExpectedBootOrder[0] = BootNum;
      }

      break;

    case IBO_RESULT_OS_INDICATIONS_CHANGE:
      if ((IboContext->Device == IBO_DEVICE_BIOS) && !IboContext->AlreadyAcked && IboContext->Valid) {
        ExpectedOsIndications = EFI_OS_INDICATIONS_BOOT_TO_FW_UI;
      } else {
        ExpectedOsIndications = 0;
      }

      break;
  }

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
IBO_SingleBootOrderSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  return IBO_BootOrderSetup (Context, 1);
}

UNIT_TEST_STATUS
EFIAPI
IBO_DualBootOrderSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  return IBO_BootOrderSetup (Context, 2);
}

UNIT_TEST_STATUS
EFIAPI
IBO_TripleBootOrderSetup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  return IBO_BootOrderSetup (Context, 3);
}

VOID
EFIAPI
IBO_Cleanup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  FREE_NON_NULL (ExpectedBootOrder);
  FREE_NON_NULL (ExpectedSavedBootOrder);
}

/**
  A simple unit test to test the normal IPMI code path

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
IBO_IpmiRequest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                              Status;
  IPMI_SET_BOOT_OPTIONS_RESPONSE          SetResponse;
  IPMI_GET_BOOT_OPTIONS_RESPONSE          *GetP5Response;
  IPMI_GET_BOOT_OPTIONS_RESPONSE          *GetP4Response;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5  *GetP5Data;
  IBO_CONTEXT                             *IboContext    = (IBO_CONTEXT *)Context;
  UINTN                                   P4ResponseSize = sizeof (IPMI_GET_BOOT_OPTIONS_RESPONSE) + sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4);
  UINTN                                   P5ResponseSize = sizeof (IPMI_GET_BOOT_OPTIONS_RESPONSE) + sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5);

  SetResponse.CompletionCode = IPMI_COMP_CODE_NORMAL;

  GetP4Response = AllocateZeroPool (P4ResponseSize);
  GetP5Response = AllocateZeroPool (P5ResponseSize);

  GetP4Response->CompletionCode                         = IPMI_COMP_CODE_NORMAL;
  GetP4Response->ParameterVersion.Bits.ParameterVersion = 1;
  GetP4Response->ParameterValid.Bits.ParameterValid     = 0; // 0 == Not invalid
  GetP4Response->ParameterValid.Bits.ParameterSelector  = 4;
  GetP4Response->ParameterData[0]                       = 0;
  GetP4Response->ParameterData[1]                       = IboContext->AlreadyAcked ? 0 : BOOT_OPTION_HANDLED_BY_BIOS;

  GetP5Response->CompletionCode                         = IPMI_COMP_CODE_NORMAL;
  GetP5Response->ParameterVersion.Bits.ParameterVersion = 1;
  GetP5Response->ParameterValid.Bits.ParameterValid     = 0; // Note: 0 means valid!
  GetP5Response->ParameterValid.Bits.ParameterSelector  = 5;
  GetP5Data                                             = (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5 *)&GetP5Response->ParameterData[0];
  GetP5Data->Data1.Bits.BootFlagValid                   = IboContext->Valid ? 1 : 0;
  GetP5Data->Data2.Bits.BootDeviceSelector              = IboContext->Device;
  GetP5Data->Data5.Bits.DeviceInstanceSelector          = IboContext->Instance;
  GetP5Data->Data1.Bits.PersistentOptions               = (IboContext->Result == IBO_RESULT_BOOT_ORDER_CHANGE) ? 1 : 0;

  // Test will do:
  //   Get P4 Ack handled by bios
  //   Get P5 Flags D1 valid, D2 Device, D5 Instance, D1 Persistence
  //   Set Ack
  //   Set Flags
  // So need to Mock those in reverse

  if (!IboContext->AlreadyAcked) {
    Status = MockIpmiSubmitCommand ((UINT8 *)&SetResponse, sizeof (SetResponse), EFI_SUCCESS); // Set Flags
    UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
    Status = MockIpmiSubmitCommand ((UINT8 *)&SetResponse, sizeof (SetResponse), EFI_SUCCESS); // Set Ack
    UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
    if (IboContext->Valid) {
      Status = MockIpmiSubmitCommand ((UINT8 *)GetP5Response, P5ResponseSize, EFI_SUCCESS); // Get P5
      UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);
    }
  }

  Status = MockIpmiSubmitCommand ((UINT8 *)GetP4Response, P4ResponseSize, EFI_SUCCESS); // Get P4
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  if (ExpectedSavedBootOrder != NULL) {
    MockUefiCreateEventEx (&gEfiEventReadyToBootGuid, &EventSavePtr, EFI_SUCCESS);
  }

  CheckIPMIForBootOrderUpdates ();
  ProcessIPMIBootOrderUpdates ();

  return IBO_CheckResults (Context);
}

#define ADD_IPMI_TEST(TEST_SUITE, TEST_SETUP, TEST_CONTEXT) \
  Status = AddTestCase (TEST_SUITE, #TEST_SETUP " with " #TEST_CONTEXT, #TEST_SETUP "_" #TEST_CONTEXT, IBO_IpmiRequest, TEST_SETUP, IBO_Cleanup, &TEST_CONTEXT);

/**
  Initialize the unit test framework, suite, and unit tests for the
  sample unit tests and run the unit tests.

  @retval  EFI_SUCCESS           All test cases were dispatched.
  @retval  EFI_OUT_OF_RESOURCES  There are not enough resources available to
                                 initialize the unit tests.
**/
EFI_STATUS
EFIAPI
SetupAndRunUnitTests (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      EmptyBootOrder;
  UNIT_TEST_SUITE_HANDLE      SingleBootOrder;
  UNIT_TEST_SUITE_HANDLE      DualBootOrder;
  UNIT_TEST_SUITE_HANDLE      TripleBootOrder;
  BOOLEAN                     RuntimePreserveVariables = FALSE;

  Framework = NULL;

  DEBUG ((DEBUG_INFO, "%a: v%a\n", UNIT_TEST_NAME, UNIT_TEST_VERSION));

  UefiBootServicesTableInit ();
  UefiRuntimeServicesTableInit (RuntimePreserveVariables);
  Status = InitUnitTestFramework (&Framework, UNIT_TEST_NAME, gEfiCallerBaseName, UNIT_TEST_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to setup Test Framework. Exiting with status = %r\n", Status));
    ASSERT (FALSE);
    return Status;
  }

  //
  // Populate the Empty Boot Order Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&EmptyBootOrder, Framework, "Empty Boot Order Tests", "UnitTest.EmptyBootOrder", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for Empty Boot Order Tests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  // Normal change requests
  ADD_IPMI_TEST (EmptyBootOrder, IBO_EmptyBootOrderSetup, NO_CHANGE_0);

  // Already-acknowledged change requests
  ADD_IPMI_TEST (EmptyBootOrder, IBO_EmptyBootOrderSetup, ACKED_PXE_0);

  // Invalid change requests
  ADD_IPMI_TEST (EmptyBootOrder, IBO_EmptyBootOrderSetup, INVALID_PXE_0);

  //
  // Populate the Single Boot Order Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&SingleBootOrder, Framework, "Single Boot Order Tests", "UnitTest.SingleBootOrder", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for Single Boot Order Tests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, NO_CHANGE_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, PXE_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, HD_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, HD_SAFE_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, DIAG_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, CD_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, BIOS_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, REMOTE_FLOPPY_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, REMOTE_CD_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, REMOTE_MEDIA_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, RESERVED_0_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, REMOTE_HD_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, RESERVED_1_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, RESERVED_2_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, RESERVED_3_0);
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, FLOPPY_0);

  // Already-acknowledged change requests
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, ACKED_PXE_0);

  // Invalid change requests
  ADD_IPMI_TEST (SingleBootOrder, IBO_SingleBootOrderSetup, INVALID_PXE_0);

  //
  // Populate the Dual Boot Order Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&DualBootOrder, Framework, "Dual Boot Order Tests", "UnitTest.DualBootOrder", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for Dual Boot Order Tests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, NO_CHANGE_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, PXE_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, HD_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, HD_SAFE_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, DIAG_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, CD_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, BIOS_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_FLOPPY_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_CD_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_MEDIA_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_0_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_HD_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_1_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_2_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_3_0);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, FLOPPY_0);

  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, NO_CHANGE_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, PXE_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, HD_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, HD_SAFE_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, DIAG_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, CD_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, BIOS_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_FLOPPY_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_CD_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_MEDIA_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_0_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_HD_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_1_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_2_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_3_1);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, FLOPPY_1);

  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, NO_CHANGE_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, PXE_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, HD_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, HD_SAFE_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, DIAG_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, CD_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, BIOS_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_FLOPPY_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_CD_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_MEDIA_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_0_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_HD_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_1_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_2_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_3_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, FLOPPY_2);

  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, NO_CHANGE_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, PXE_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, HD_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, HD_SAFE_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, DIAG_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, CD_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, BIOS_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_FLOPPY_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_CD_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_MEDIA_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_0_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, REMOTE_HD_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_1_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_2_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, RESERVED_3_3);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, FLOPPY_3);

  // Already-acknowledged change requests
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_PXE_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_HD_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_HD_SAFE_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_DIAG_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_CD_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_BIOS_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_REMOTE_FLOPPY_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_REMOTE_CD_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_REMOTE_MEDIA_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_RESERVED_0_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_REMOTE_HD_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_RESERVED_1_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_RESERVED_2_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_RESERVED_3_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, ACKED_FLOPPY_2);

  // Invalid change requests
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_PXE_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_HD_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_HD_SAFE_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_DIAG_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_CD_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_BIOS_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_REMOTE_FLOPPY_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_REMOTE_CD_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_REMOTE_MEDIA_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_RESERVED_0_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_REMOTE_HD_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_RESERVED_1_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_RESERVED_2_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_RESERVED_3_2);
  ADD_IPMI_TEST (DualBootOrder, IBO_DualBootOrderSetup, INVALID_FLOPPY_2);

  //
  // Populate the Triple Boot Order Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&TripleBootOrder, Framework, "Triple Boot Order Tests", "UnitTest.TripleBootOrder", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for Triple Boot Order Tests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NO_CHANGE_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, PXE_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, HD_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, HD_SAFE_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, DIAG_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, CD_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, BIOS_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_FLOPPY_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_CD_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_MEDIA_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_0_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_HD_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_1_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_2_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_3_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, FLOPPY_0);

  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NO_CHANGE_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, PXE_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, HD_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, HD_SAFE_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, DIAG_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, CD_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, BIOS_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_FLOPPY_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_CD_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_MEDIA_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_0_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_HD_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_1_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_2_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_3_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, FLOPPY_1);

  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NO_CHANGE_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, PXE_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, HD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, HD_SAFE_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, DIAG_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, CD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, BIOS_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_FLOPPY_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_CD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_MEDIA_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_0_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_HD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_1_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_2_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_3_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, FLOPPY_2);

  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NO_CHANGE_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, PXE_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, HD_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, HD_SAFE_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, DIAG_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, CD_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, BIOS_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_FLOPPY_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_CD_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_MEDIA_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_0_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, REMOTE_HD_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_1_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_2_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, RESERVED_3_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, FLOPPY_3);

  // Already-acknowledged change requests
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_PXE_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_HD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_HD_SAFE_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_DIAG_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_CD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_BIOS_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_REMOTE_FLOPPY_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_REMOTE_CD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_REMOTE_MEDIA_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_RESERVED_0_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_REMOTE_HD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_RESERVED_1_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_RESERVED_2_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_RESERVED_3_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, ACKED_FLOPPY_2);

  // Invalid change requests
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_PXE_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_HD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_HD_SAFE_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_DIAG_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_CD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_BIOS_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_REMOTE_FLOPPY_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_REMOTE_CD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_REMOTE_MEDIA_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_RESERVED_0_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_REMOTE_HD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_RESERVED_1_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_RESERVED_2_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_RESERVED_3_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, INVALID_FLOPPY_2);

  // Boot Next change

  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_NO_CHANGE_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_PXE_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_HD_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_HD_SAFE_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_DIAG_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_CD_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_BIOS_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_FLOPPY_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_CD_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_MEDIA_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_0_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_HD_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_1_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_2_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_3_0);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_FLOPPY_0);

  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_NO_CHANGE_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_PXE_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_HD_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_HD_SAFE_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_DIAG_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_CD_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_BIOS_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_FLOPPY_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_CD_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_MEDIA_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_0_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_HD_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_1_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_2_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_3_1);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_FLOPPY_1);

  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_NO_CHANGE_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_PXE_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_HD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_HD_SAFE_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_DIAG_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_CD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_BIOS_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_FLOPPY_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_CD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_MEDIA_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_0_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_HD_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_1_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_2_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_3_2);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_FLOPPY_2);

  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_NO_CHANGE_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_PXE_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_HD_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_HD_SAFE_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_DIAG_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_CD_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_BIOS_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_FLOPPY_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_CD_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_MEDIA_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_0_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_REMOTE_HD_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_1_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_2_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_RESERVED_3_3);
  ADD_IPMI_TEST (TripleBootOrder, IBO_TripleBootOrderSetup, NEXT_FLOPPY_3);

  //
  // Execute the tests.
  //
  Status = RunAllTestSuites (Framework);

  UefiBootServicesTableDeinit ();
  UefiRuntimeServicesTableDeinit (RuntimePreserveVariables);

  return Status;
}

/**
  Standard UEFI entry point for target based
  unit test execution from UEFI Shell.
**/
EFI_STATUS
EFIAPI
BaseLibUnitTestAppEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return SetupAndRunUnitTests ();
}

/**
  Standard POSIX C entry point for host based unit test execution.
**/
int
main (
  int   argc,
  char  *argv[]
  )
{
  return SetupAndRunUnitTests ();
}
