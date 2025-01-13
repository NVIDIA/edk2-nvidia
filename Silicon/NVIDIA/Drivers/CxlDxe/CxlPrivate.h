/** @file

  CXL Driver Private Data

  SPDX-FileCopyrightText: Copyright (c) 2025-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CXL_PRIVATE_H__
#define __CXL_PRIVATE_H__

#include <PiDxe.h>

#include <IndustryStandard/Pci.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/DevicePath.h>
#include <Protocol/PciIo.h>
#include <Protocol/DriverSupportedEfiVersion.h>

// TODO: These class codes should be upstreamed to Pci22.h in edk2 repository
#define PCI_CXL_PI                       0x10 // CXL PROGRAMMING INTERFACE
#define PCI_CLASS_MEMORY_CONTROLLER_CXL  0x02 // CXL MEMORY DEVICE

#endif
