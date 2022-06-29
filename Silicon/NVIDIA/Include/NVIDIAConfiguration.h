/** @file
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __NVIDIA_CONFIGURATION_H__
#define __NVIDIA_CONFIGURATION_H__

#define NVIDIA_SERIAL_PORT_TYPE_16550        0x0
#define NVIDIA_SERIAL_PORT_TYPE_SBSA         0x1
#define NVIDIA_SERIAL_PORT_TYPE_UNDEFINED    0xFF

#define NVIDIA_SERIAL_PORT_SPCR_FULL_16550   0x0
#define NVIDIA_SERIAL_PORT_SPCR_NVIDIA_16550 0x1
#define NVIDIA_SERIAL_PORT_DBG2_NVIDIA_16550 0x2
#define NVIDIA_SERIAL_PORT_SPCR_SBSA         0x3
#define NVIDIA_SERIAL_PORT_DBG2_SBSA         0x4
#define NVIDIA_SERIAL_PORT_DISABLED          0xFF

#define NVIDIA_OS_OVERRIDE_A                 0x0
#define NVIDIA_OS_OVERRIDE_B                 0x1
#define NVIDIA_OS_OVERRIDE_DEFAULT           0xFF

#define L4T_BOOTMODE_VARIABLE_NAME           L"L4TDefaultBootMode"

#define NVIDIA_L4T_BOOTMODE_GRUB             0x0
#define NVIDIA_L4T_BOOTMODE_DIRECT           0x1
#define NVIDIA_L4T_BOOTMODE_BOOTIMG          0x2
#define NVIDIA_L4T_BOOTMODE_RECOVERY         0x3
#define NVIDIA_L4T_BOOTMODE_DEFAULT          0xFF

#define KERNEL_CMD_STR_MIN_SIZE              0
#define KERNEL_CMD_STR_MAX_SIZE              255

//Option to expose PCIe in OS
typedef struct {
  UINT8 Enabled;
} NvidiaPcieEnableVariable;

//Volatile option to configure if PCIe option is exposed to Hii
typedef struct {
  UINT8 ConfigEnabled;
} NvidiaPcieResourceConfiguration;
typedef struct {
  UINT8 ConfigEnabled;
} NvidiaPcieEnableInOsConfiguration;

typedef struct {
  UINT8 Enabled;
} NVIDIA_QUICK_BOOT_ENABLED;

typedef struct {
  UINT8 Hierarchy;
} NVIDIA_NEW_DEVICE_HIERARCHY;

typedef struct {
  UINT8 Type;
} NVIDIA_SERIAL_TYPE_CONFIG;
typedef struct {
  UINT8 Configuration;
} NVIDIA_SERIAL_PORT_CONFIG;

typedef struct {
  CHAR16 KernelCommand[KERNEL_CMD_STR_MAX_SIZE];
} NVIDIA_KERNEL_COMMAND_LINE;

typedef struct {
  UINT32 Chain;
} NVIDIA_OS_OVERRIDE;

typedef struct {
  UINT32 BootMode;
} NVIDIA_L4T_BOOT_MODE;

#endif //__NVIDIA_CONFIGURATION_H__
