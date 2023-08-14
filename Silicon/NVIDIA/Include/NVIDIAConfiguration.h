/** @file
*
*  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __NVIDIA_CONFIGURATION_H__
#define __NVIDIA_CONFIGURATION_H__

#define NVIDIA_SERIAL_PORT_TYPE_16550      0x0
#define NVIDIA_SERIAL_PORT_TYPE_SBSA       0x1
#define NVIDIA_SERIAL_PORT_TYPE_UNDEFINED  0xFF

#define NVIDIA_SERIAL_PORT_SPCR_FULL_16550    0x0
#define NVIDIA_SERIAL_PORT_SPCR_NVIDIA_16550  0x1
#define NVIDIA_SERIAL_PORT_DBG2_NVIDIA_16550  0x2
#define NVIDIA_SERIAL_PORT_SPCR_SBSA          0x3
#define NVIDIA_SERIAL_PORT_DBG2_SBSA          0x4
#define NVIDIA_SERIAL_PORT_DISABLED           0xFF

#define NVIDIA_OS_STATUS_NORMAL      0x0
#define NVIDIA_OS_STATUS_UNBOOTABLE  0xFF

#define NVIDIA_OS_REDUNDANCY_BOOT_ONLY    0x0
#define NVIDIA_OS_REDUNDANCY_BOOT_ROOTFS  0x1

#define L4T_BOOTMODE_VARIABLE_NAME  L"L4TDefaultBootMode"

#define NVIDIA_L4T_BOOTMODE_GRUB      0x0
#define NVIDIA_L4T_BOOTMODE_DIRECT    0x1
#define NVIDIA_L4T_BOOTMODE_BOOTIMG   0x2
#define NVIDIA_L4T_BOOTMODE_RECOVERY  0x3
#define NVIDIA_L4T_BOOTMODE_DEFAULT   0xFF

#define KERNEL_CMD_STR_MIN_SIZE  0
#define KERNEL_CMD_STR_MAX_SIZE  255

#define ASSET_TAG_MAX_LEN   32
#define ASSET_TAG_MAX_SIZE  33

#define ENABLED_PCIE_ALLOW_ALL  0xFF

// Memory test levels
#define MEMORY_TEST_LEVEL_IGNORE     0
#define MEMORY_TEST_LEVEL_QUICK      1
#define MEMORY_TEST_LEVEL_SPARSE     2
#define MEMORY_TEST_LEVEL_EXTENSIVE  3

// Option to expose PCIe in OS
typedef struct {
  UINT8    Enabled;
} NvidiaPcieEnableVariable;

// Volatile option to configure if PCIe option is exposed to Hii
typedef struct {
  UINT8    ConfigEnabled;
} NvidiaPcieResourceConfiguration;
typedef struct {
  UINT8    ConfigEnabled;
} NvidiaPcieEnableInOsConfiguration;

typedef struct {
  UINT8    Enabled;
} NVIDIA_QUICK_BOOT_ENABLED;

typedef struct {
  UINT8    Hierarchy;
} NVIDIA_NEW_DEVICE_HIERARCHY;

typedef struct {
  UINT8    Type;
} NVIDIA_SERIAL_TYPE_CONFIG;
typedef struct {
  UINT8    Configuration;
} NVIDIA_SERIAL_PORT_CONFIG;

typedef struct {
  CHAR16    KernelCommand[KERNEL_CMD_STR_MAX_SIZE];
} NVIDIA_KERNEL_COMMAND_LINE;

typedef struct {
  UINT32    Chain;
} NVIDIA_OS_OVERRIDE;

typedef struct {
  UINT32    Status;
} NVIDIA_OS_STATUS_A;

typedef struct {
  UINT32    Status;
} NVIDIA_OS_STATUS_B;

typedef struct {
  UINT32    Level;
} NVIDIA_OS_REDUNDANCY;

typedef struct {
  UINT32    BootMode;
} NVIDIA_L4T_BOOT_MODE;

typedef struct {
  UINT8    IpMode;
} NVIDIA_IPMI_NETWORK_BOOT_MODE;

typedef struct {
  UINT8    Enabled;
} NVIDIA_ACPI_TIMER_ENABLED;

typedef struct {
  UINT8    Enabled;
} NVIDIA_UEFI_SHELL_ENABLED;

typedef struct {
  UINT8    Enabled;
} NVIDIA_DGPU_DT_EFIFB_SUPPORT;

typedef struct {
  UINT8    Enabled;
} NVIDIA_REDFISH_HOST_INTERFACE_ENABLED;

typedef struct {
  CHAR16    ChassisAssetTag[ASSET_TAG_MAX_SIZE];
  UINT8     AssetTagProtection;
} NVIDIA_PRODUCT_INFO;

typedef struct {
  UINT8    Enabled;
  UINT8    Segment;
  UINT8    Bus;
  UINT8    Device;
  UINT8    Function;
} NVIDIA_ENABLED_PCIE_NIC_TOPOLOGY;

typedef struct {
  UINT8      TestLevel;
  BOOLEAN    NextBoot;
  BOOLEAN    SingleBoot;
  UINT8      TestIterations;
  BOOLEAN    Walking1BitEnabled;
  BOOLEAN    AddressCheckEnabled;
  BOOLEAN    MovingInversions01Enabled;
  BOOLEAN    MovingInversions8BitEnabled;
  BOOLEAN    MovingInversionsRandomEnabled;
  BOOLEAN    BlockMoveEnabled;
  BOOLEAN    MovingInversions64BitEnabled;
  BOOLEAN    RandomNumberSequenceEnabled;
  BOOLEAN    Modulo20RandomEnabled;
  BOOLEAN    BitFadeEnabled;
  UINT64     BitFadePattern;
  UINT64     BitFadeWait;
} NVIDIA_MEMORY_TEST_OPTIONS;

#endif //__NVIDIA_CONFIGURATION_H__
