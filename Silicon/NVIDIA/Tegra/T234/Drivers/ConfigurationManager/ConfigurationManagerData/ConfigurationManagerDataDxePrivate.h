/** @file
  Configuration Manager Data Dxe Private Definitions

  Copyright (c) 2023, NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CONFIGURATION_MANAGER_DXE_PRIVATE_H__
#define __CONFIGURATION_MANAGER_DXE_PRIVATE_H__

#include <ConfigurationManagerObject.h>

#include <Library/ArmLib.h>
#include <Library/ArmGicLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/ConfigurationManagerLib.h>
#include <Library/DebugLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <libfdt.h>

#include <IndustryStandard/DebugPort2Table.h>

#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>

#include <Protocol/AmlPatchProtocol.h>
#include <Protocol/AmlGenerationProtocol.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include <NVIDIAConfiguration.h>

#include "Platform.h"
#include "ConfigurationManagerDataPrivate.h"
#include <T234/T234Definitions.h>
#include "ConfigurationIortPrivate.h"

#include "Dsdt.hex"
#include "Dsdt.offset.h"
#include "SdcTemplate.hex"
#include "SdcTemplate.offset.h"

#define ACPI_PATCH_MAX_PATH  255
#define ACPI_SDCT_REG0       "SDCT.REG0"
#define ACPI_SDCT_UID        "SDCT._UID"
#define ACPI_SDCT_INT0       "SDCT.INT0"
#define ACPI_SDCT_RMV        "SDCT._RMV"

#define IORT_TABLE_GEN  L"IortTableGen"

#endif // __CONFIGURATION_MANAGER_DXE_PRIVATE_H__
