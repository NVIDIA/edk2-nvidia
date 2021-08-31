/** @file
*  Resource Configuration Dxe
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
*  Copyright (c) 2017, Linaro Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
*  Portions provided under the following terms:
*  Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#ifndef __RESOURCE_CONFIG_HII_H__
#define __RESOURCE_CONFIG_HII_H__

#include <Guid/NVIDIATokenSpaceGuid.h>
#include <NVIDIAConfiguration.h>

#define RESOURCE_CONFIG_FORMSET_GUID  { 0x685c0b6e, 0x11af, 0x47cf, { 0xa9, 0xef, 0x95, 0xac, 0x18, 0x68, 0x73, 0xc3 } }

#define RESOURCE_CONFIG_FORM_ID         0x0001
#define PCIE_CONFIGURATION_FORM_ID      0x0002

#define KEY_ENABLE_PCIE_CONFIG          0x0100
#define KEY_ENABLE_PCIE_IN_OS_CONFIG    0x0101
#define KEY_ENABLE_QUICK_BOOT           0x0102
#define KEY_SERIAL_PORT_CONFIG          0x0103
#define KEY_KERNEL_CMDLINE              0x0104
#define KEY_OS_CHAIN_OVERRIDE           0x0105

#define PCIE_IN_OS_DISABLE              0x0
#define PCIE_IN_OS_ENABLE               0x1

#define QUICK_BOOT_DISABLE              0x0
#define QUICK_BOOT_ENABLE               0x1

#endif // __RESOURCE_CONFIG_HII_H__
