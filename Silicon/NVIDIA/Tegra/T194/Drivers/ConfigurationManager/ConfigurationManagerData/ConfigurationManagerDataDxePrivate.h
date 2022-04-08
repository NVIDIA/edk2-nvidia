/** @file
  Configuration Manager Data Dxe Private Definitions

  Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CONFIGURATION_MANAGER_DXE_PRIVATE_H__
#define __CONFIGURATION_MANAGER_DXE_PRIVATE_H__

#include <ConfigurationManagerObject.h>

#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/ConfigurationManagerLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/NvgLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <libfdt.h>

#include <IndustryStandard/DebugPort2Table.h>
#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>

#include <Protocol/AmlPatchProtocol.h>
#include <Protocol/AmlGenerationProtocol.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <Protocol/PciRootBridgeIo.h>

#include <NVIDIAConfiguration.h>

#include "Platform.h"
#include <T194/T194Definitions.h>

#include "Dsdt.hex"
#include "Dsdt.offset.h"
#include "SsdtAhci.hex"
#include "SsdtAhci.offset.h"
#include "SdcTemplate.hex"
#include "SdcTemplate.offset.h"
#include "I2cTemplate.hex"
#include "I2cTemplate.offset.h"

#define ACPI_PATCH_MAX_PATH  255
#define ACPI_DEVICE_MAX      9
#define ACPI_SDCT_REG0       "SDCT.REG0"
#define ACPI_SDCT_UID        "SDCT._UID"
#define ACPI_SDCT_INT0       "SDCT.INT0"
#define ACPI_SDCT_RMV        "SDCT._RMV"
#define ACPI_I2CT_REG0       "I2CT.REG0"
#define ACPI_I2CT_UID        "I2CT._UID"
#define ACPI_I2CT_INT0       "I2CT.INT0"
#define ACPI_FAN_FANR        "_SB_.FAN_.FANR"
#define ACPI_FAN_STA         "_SB_.FAN_._STA"

#define AHCI_PCIE_SEGMENT  1

#endif // __CONFIGURATION_MANAGER_DXE_PRIVATE_H__
