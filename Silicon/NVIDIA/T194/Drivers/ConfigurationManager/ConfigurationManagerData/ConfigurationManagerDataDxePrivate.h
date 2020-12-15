/** @file
  Configuration Manager Data Dxe Private Definitions

  Copyright (c) 2021, NVIDIA Corporation. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/
#ifndef __CONFIGURATION_MANAGER_DXE_PRIVATE_H__
#define __CONFIGURATION_MANAGER_DXE_PRIVATE_H__

#include <ConfigurationManagerObject.h>

#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <libfdt.h>

#include <IndustryStandard/DebugPort2Table.h>
#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>

#include <Protocol/AmlPatchProtocol.h>
#include <Protocol/AmlGenerationProtocol.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include <NVIDIAConfiguration.h>

#include "Platform.h"
#include <T194/T194Definitions.h>

#include "Dsdt.hex"
#include "Dsdt.offset.h"
#include "SsdtPci.hex"
#include "SsdtPci.offset.h"
#include "SdcTemplate.hex"
#include "SdcTemplate.offset.h"


#define ACPI_PATCH_MAX_PATH   255
#define ACPI_DEVICE_MAX       9
#define ACPI_PCI_STA_TEMPLATE "_SB_.PCI%d._STA"
#define ACPI_SDCT_REG0        "SDCT.REG0"
#define ACPI_SDCT_INT0        "SDCT.INT0"
#define ACPI_FAN_FANR         "_SB_.FAN_.FANR"
#define ACPI_FAN_STA          "_SB_.FAN_._STA"

#endif // __CONFIGURATION_MANAGER_DXE_PRIVATE_H__
