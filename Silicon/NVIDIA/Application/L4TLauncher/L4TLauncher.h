/** @file

  SPDX-FileCopyrightText: Copyright (c) 2022-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __L4T_LAUNCHER_H_
#define __L4T_LAUNCHER_H_
#include "L4TOpteeDecrypt.h"

#include <Protocol/L4TLauncherSupportProtocol.h>

#define GRUB_PATH                       L"EFI\\BOOT\\grubaa64.efi"
#define GRUB_BOOTCONFIG_FILE            L"EFI\\BOOT\\boot.cfg"
#define MAX_BOOTCONFIG_CONTENT_SIZE     512
#define MAX_CBOOTARG_SIZE               256
#define GRUB_BOOTCONFIG_CONTENT_FORMAT  "set cbootargs=\"%s\"\r\nset root_partition_number=%u\r\nset bootimg_present=%u\r\nset recovery_present=%u\r\n"
#define DETACHED_SIG_FILE_EXTENSION     L".sig"

#define EXTLINUX_CONF_PATH  L"boot\\extlinux\\extlinux.conf"

#define BOOTMODE_DIRECT_STRING    L"bootmode=direct"
#define BOOTMODE_GRUB_STRING      L"bootmode=grub"
#define BOOTMODE_BOOTIMG_STRING   L"bootmode=bootimg"
#define BOOTMODE_RECOVERY_STRING  L"bootmode=recovery"

#define BOOTCHAIN_OVERRIDE_STRING  L"bootchain="

#define MAX_PARTITION_NAME_SIZE  36       // From the UEFI spec for GPT partitions

#define BOOT_FW_VARIABLE_NAME  L"BootChainFwCurrent"
#define BOOT_OS_VARIABLE_NAME  L"BootChainOsCurrent"

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
#define EXTLINUX_KEY_OVERLAYS    L"OVERLAYS"

#define EXTLINUX_CBOOT_ARG  L"${cbootargs}"

#define MAX_EXTLINUX_OPTIONS  10

typedef struct {
  CHAR16    *Label;
  CHAR16    *MenuLabel;
  CHAR16    *LinuxPath;
  CHAR16    *DtbPath;
  CHAR16    *InitrdPath;
  CHAR16    *BootArgs;
  CHAR16    *Overlays;
} EXTLINUX_BOOT_OPTION;

typedef struct {
  UINT32                  DefaultBootEntry;
  CHAR16                  *MenuTitle;
  EXTLINUX_BOOT_OPTION    BootOptions[MAX_EXTLINUX_OPTIONS];
  UINT32                  NumberOfBootOptions;
  UINT32                  Timeout;
} EXTLINUX_BOOT_CONFIG;

extern L4T_LAUNCHER_SUPPORT_PROTOCOL  *gL4TSupportProtocol;

#endif /* __L4T_LAUNCHER_H_ */
