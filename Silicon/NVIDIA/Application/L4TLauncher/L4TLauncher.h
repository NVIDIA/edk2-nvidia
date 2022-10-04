/** @file

  Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __L4T_LAUNCHER_H_
#define __L4T_LAUNCHER_H_

#define GRUB_PATH                       L"EFI\\BOOT\\grubaa64.efi"
#define GRUB_BOOTCONFIG_FILE            L"EFI\\BOOT\\boot.cfg"
#define MAX_BOOTCONFIG_CONTENT_SIZE     512
#define MAX_CBOOTARG_SIZE               256
#define GRUB_BOOTCONFIG_CONTENT_FORMAT  "set cbootargs=\"%s\"\r\nset root_partition_number=%d\r\nset bootimg_present=%d\r\nset recovery_present=%d\r\n"
#define DETACHED_SIG_FILE_EXTENSION     L".sig"

#define EXTLINUX_CONF_PATH  L"boot\\extlinux\\extlinux.conf"

#define BOOTMODE_DIRECT_STRING    L"bootmode=direct"
#define BOOTMODE_GRUB_STRING      L"bootmode=grub"
#define BOOTMODE_BOOTIMG_STRING   L"bootmode=bootimg"
#define BOOTMODE_RECOVERY_STRING  L"bootmode=recovery"

#define BOOTCHAIN_OVERRIDE_STRING  L"bootchain="

#define MAX_PARTITION_NAME_SIZE  36       // From the UEFI spec for GPT partitions

#define BOOT_FW_VARIABLE_NAME           L"BootChainFwCurrent"
#define BOOT_OS_VARIABLE_NAME           L"BootChainOsCurrent"
#define BOOT_OS_OVERRIDE_VARIABLE_NAME  L"BootChainOsOverride"
#define ROOTFS_INFO_VARIABLE_NAME       L"RootfsInfo"

#define ROOTFS_BASE_NAME        L"APP"
#define BOOTIMG_BASE_NAME       L"kernel"
#define BOOTIMG_DTB_BASE_NAME   L"kernel-dtb"
#define RECOVERY_BASE_NAME      L"recovery"
#define RECOVERY_DTB_BASE_NAME  L"recovery-dtb"

#define EXTLINUX_KEY_TIMEOUT     L"TIMEOUT"
#define EXTLINUX_KEY_DEFAULT     L"DEFAULT"
#define EXTLINUX_KEY_MENU_TITLE  L"MENU TITLE"
#define EXTLINUX_KEY_LABEL       L"LABEL"
#define EXTLINUX_KEY_MENU_LABEL  L"MENU LABEL"
#define EXTLINUX_KEY_LINUX       L"LINUX"
#define EXTLINUX_KEY_INITRD      L"INITRD"
#define EXTLINUX_KEY_FDT         L"FDT"
#define EXTLINUX_KEY_APPEND      L"APPEND"

#define EXTLINUX_CBOOT_ARG  L"${cbootargs}"

#define MAX_EXTLINUX_OPTIONS  10

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
#define SR_RF_MAGIC       0xFACEUL   /* 'FACE' */

#define RF_CURRENT_SLOT_SHIFT   16
#define RF_CURRENT_SLOT_MASK    (0x03UL << RF_CURRENT_SLOT_SHIFT)
#define RF_RETRY_COUNT_B_SHIFT  18
#define RF_RETRY_COUNT_B_MASK   (0x03UL << RF_RETRY_COUNT_B_SHIFT)
#define RF_RETRY_COUNT_A_SHIFT  20
#define RF_RETRY_COUNT_A_MASK   (0x03UL << RF_RETRY_COUNT_A_SHIFT)

#define SR_RF_MAGIC_GET(reg)  ((reg) & SR_RF_MAGIC_MASK)
#define SR_RF_MAGIC_SET(reg)  (((reg) & ~SR_RF_MAGIC_MASK) | SR_RF_MAGIC)

#define SR_RF_CURRENT_SLOT_GET(reg)        (((reg) & RF_CURRENT_SLOT_MASK) >> RF_CURRENT_SLOT_SHIFT)
#define SR_RF_CURRENT_SLOT_SET(slot, reg)  (((reg) & ~RF_CURRENT_SLOT_MASK) |   \
                                             (((slot) & 0x03UL) << RF_CURRENT_SLOT_SHIFT))

#define SR_RF_RETRY_COUNT_B_GET(reg)         (((reg) & RF_RETRY_COUNT_B_MASK) >> RF_RETRY_COUNT_B_SHIFT)
#define SR_RF_RETRY_COUNT_B_SET(count, reg)  (((reg) & ~RF_RETRY_COUNT_B_MASK) | \
                                             (((count) & 0x03UL) << RF_RETRY_COUNT_B_SHIFT))

#define SR_RF_RETRY_COUNT_A_GET(reg)         (((reg) & RF_RETRY_COUNT_A_MASK) >> RF_RETRY_COUNT_A_SHIFT)
#define SR_RF_RETRY_COUNT_A_SET(count, reg)  (((reg) & ~RF_RETRY_COUNT_A_MASK) | \
                                             (((count) & 0x03UL) << RF_RETRY_COUNT_A_SHIFT))

/*
 * Variable RootfsInfo
 *
 *
 * 00:00 Link with firmware or not
 * 01:01 Current rootfs slot
 * 02:03 Retry count of rootfs slot B
 * 04:05 Retry count of rootfs slot A
 * 06:07 Status of rootfs slot B
 * 08:09 Status of rootfs slot A
 * 10:11 Rootfs update mode B
 * 12:13 Rootfs update mode A
 * 14:15 Maximum rootfs retry count
 * 16:17 A/B redundancy level configuration
 * 18:31 reserved
 */

#define RF_INFO_SLOT_LINK_FW_SHIFT   0
#define RF_INFO_SLOT_LINK_FW_MASK    (0x01UL << RF_INFO_SLOT_LINK_FW_SHIFT)
#define RF_INFO_CURRENT_SLOT_SHIFT   1
#define RF_INFO_CURRENT_SLOT_MASK    (0x01UL << RF_INFO_CURRENT_SLOT_SHIFT)
#define RF_INFO_RETRY_CNT_B_SHIFT    2
#define RF_INFO_RETRY_CNT_B_MASK     (0x03UL << RF_INFO_RETRY_CNT_B_SHIFT)
#define RF_INFO_RETRY_CNT_A_SHIFT    4
#define RF_INFO_RETRY_CNT_A_MASK     (0x03UL << RF_INFO_RETRY_CNT_A_SHIFT)
#define RF_INFO_STATUS_B_SHIFT       6
#define RF_INFO_STATUS_B_MASK        (0x03UL << RF_INFO_STATUS_B_SHIFT)
#define RF_INFO_STATUS_A_SHIFT       8
#define RF_INFO_STATUS_A_MASK        (0x03UL << RF_INFO_STATUS_A_SHIFT)
#define RF_INFO_UPD_MODE_B_SHIFT     10
#define RF_INFO_UPD_MODE_B_MASK      (0x03UL << RF_INFO_UPD_MODE_B_SHIFT)
#define RF_INFO_UPD_MODE_A_SHIFT     12
#define RF_INFO_UPD_MODE_A_MASK      (0x03UL << RF_INFO_UPD_MODE_A_SHIFT)
#define RF_INFO_MAX_RETRY_CNT_SHIFT  14
#define RF_INFO_MAX_RETRY_CNT_MASK   (0x03UL << RF_INFO_MAX_RETRY_CNT_SHIFT)
#define RF_INFO_REDUNDANCY_SHIFT     16
#define RF_INFO_REDUNDANCY_MASK      (0x03UL << RF_INFO_REDUNDANCY_SHIFT)

#define RF_INFO_SLOT_LINK_FW_GET(var)   (((var) & RF_INFO_SLOT_LINK_FW_MASK) >> RF_INFO_SLOT_LINK_FW_SHIFT)
#define RF_INFO_CURRENT_SLOT_GET(var)   (((var) & RF_INFO_CURRENT_SLOT_MASK) >> RF_INFO_CURRENT_SLOT_SHIFT)
#define RF_INFO_RETRY_CNT_B_GET(var)    (((var) & RF_INFO_RETRY_CNT_B_MASK) >> RF_INFO_RETRY_CNT_B_SHIFT)
#define RF_INFO_RETRY_CNT_A_GET(var)    (((var) & RF_INFO_RETRY_CNT_A_MASK) >> RF_INFO_RETRY_CNT_A_SHIFT)
#define RF_INFO_STATUS_B_GET(var)       (((var) & RF_INFO_STATUS_B_MASK) >> RF_INFO_STATUS_B_SHIFT)
#define RF_INFO_STATUS_A_GET(var)       (((var) & RF_INFO_STATUS_A_MASK) >> RF_INFO_STATUS_A_SHIFT)
#define RF_INFO_UPD_MODE_B_GET(var)     (((var) & RF_INFO_UPD_MODE_B_MASK) >> RF_INFO_UPD_MODE_B_SHIFT)
#define RF_INFO_UPD_MODE_A_GET(var)     (((var) & RF_INFO_UPD_MODE_A_MASK) >> RF_INFO_UPD_MODE_A_SHIFT)
#define RF_INFO_MAX_RETRY_CNT_GET(var)  (((var) & RF_INFO_MAX_RETRY_CNT_MASK) >> RF_INFO_MAX_RETRY_CNT_SHIFT)
#define RF_INFO_REDUNDANCY_GET(var)     (((var) & RF_INFO_REDUNDANCY_MASK) >> RF_INFO_REDUNDANCY_SHIFT)

#define RF_INFO_SLOT_LINK_FW_SET(link, var)  (((var) & ~RF_INFO_SLOT_LINK_FW_MASK) |   \
                                               (((link) & 0x01UL) << RF_INFO_SLOT_LINK_FW_SHIFT))
#define RF_INFO_CURRENT_SLOT_SET(slot, var)  (((var) & ~RF_INFO_CURRENT_SLOT_MASK) |   \
                                               (((slot) & 0x01UL) << RF_INFO_CURRENT_SLOT_SHIFT))
#define RF_INFO_RETRY_CNT_B_SET(count, var)  (((var) & ~RF_INFO_RETRY_CNT_B_MASK) |   \
                                               (((count) & 0x03UL) << RF_INFO_RETRY_CNT_B_SHIFT))
#define RF_INFO_RETRY_CNT_A_SET(count, var)  (((var) & ~RF_INFO_RETRY_CNT_A_MASK) |   \
                                               (((count) & 0x03UL) << RF_INFO_RETRY_CNT_A_SHIFT))
#define RF_INFO_STATUS_B_SET(status, var)    (((var) & ~RF_INFO_STATUS_B_MASK) |   \
                                               (((status) & 0x03UL) << RF_INFO_STATUS_B_SHIFT))
#define RF_INFO_STATUS_A_SET(status, var)    (((var) & ~RF_INFO_STATUS_A_MASK) |   \
                                               (((status) & 0x03UL) << RF_INFO_STATUS_A_SHIFT))

#define REDUNDANCY_BOOT_ONLY    0
#define REDUNDANCY_BOOT_ROOTFS  1

#define ROOTFS_SLOT_A  0
#define ROOTFS_SLOT_B  1

#define RF_INFO_SLOT_LINK_FW      0
#define RF_INFO_SLOT_NOT_LINK_FW  1

#define ROOTFS_NORMAL          0
#define ROOTFS_UPD_IN_PROCESS  1
#define ROOTFS_UPD_DONE        2
#define ROOTFS_UNBOOTABLE      3

#define FROM_REG_TO_VAR  0
#define FROM_VAR_TO_REG  1

typedef struct {
  UINT32    BootMode;
  UINT32    BootChain;
} L4T_BOOT_PARAMS;

typedef struct {
  CHAR16    *Label;
  CHAR16    *MenuLabel;
  CHAR16    *LinuxPath;
  CHAR16    *DtbPath;
  CHAR16    *InitrdPath;
  CHAR16    *BootArgs;
} EXTLINUX_BOOT_OPTION;

typedef struct {
  UINT32                  DefaultBootEntry;
  CHAR16                  *MenuTitle;
  EXTLINUX_BOOT_OPTION    BootOptions[MAX_EXTLINUX_OPTIONS];
  UINT32                  NumberOfBootOptions;
  UINT32                  Timeout;
} EXTLINUX_BOOT_CONFIG;

STATIC VOID                *mRamdiskData = NULL;
STATIC UINTN               mRamdiskSize  = 0;
STATIC EFI_SIGNATURE_LIST  **AllowedDB   = NULL;
STATIC EFI_SIGNATURE_LIST  **RevokedDB   = NULL;

typedef struct {
  VENDOR_DEVICE_PATH          VendorMediaNode;
  EFI_DEVICE_PATH_PROTOCOL    EndNode;
} RAMDISK_DEVICE_PATH;

STATIC CONST RAMDISK_DEVICE_PATH  mRamdiskDevicePath =
{
  {
    {
      MEDIA_DEVICE_PATH,
      MEDIA_VENDOR_DP,
      { sizeof (VENDOR_DEVICE_PATH),       0 }
    },
    LINUX_EFI_INITRD_MEDIA_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 }
  }
};

#endif /* __L4T_LAUNCHER_H_ */
