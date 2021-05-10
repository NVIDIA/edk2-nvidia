/** @file
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __NVIDIA_CONFIGURATION_H__
#define __NVIDIA_CONFIGURATION_H__

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
  UINT8 Type;
} NVIDIA_SERIAL_TYPE_CONFIG;
typedef struct {
  UINT8 Configuration;
} NVIDIA_SERIAL_PORT_CONFIG;

#define NVIDIA_SERIAL_PORT_TYPE_16550        0x0
#define NVIDIA_SERIAL_PORT_TYPE_SBSA         0x1
#define NVIDIA_SERIAL_PORT_TYPE_UNDEFINED    0xFF

#define NVIDIA_SERIAL_PORT_SPCR_FULL_16550   0x0
#define NVIDIA_SERIAL_PORT_SPCR_NVIDIA_16550 0x1
#define NVIDIA_SERIAL_PORT_DBG2_NVIDIA_16550 0x2
#define NVIDIA_SERIAL_PORT_SPCR_SBSA         0x3
#define NVIDIA_SERIAL_PORT_DBG2_SBSA         0x4
#define NVIDIA_SERIAL_PORT_DISABLED          0xFF

#endif //__NVIDIA_CONFIGURATION_H__
