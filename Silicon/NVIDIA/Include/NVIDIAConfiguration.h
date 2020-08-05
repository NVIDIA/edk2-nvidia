/** @file
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
#define NVIDIA_PCIE_ENABLE_IN_OS_VARIABLE_NAME L"EnablePcieInOS"
typedef struct {
  UINT8 Enabled;
} NvidiaPcieEnableVariable;

#endif //__NVIDIA_CONFIGURATION_H__
