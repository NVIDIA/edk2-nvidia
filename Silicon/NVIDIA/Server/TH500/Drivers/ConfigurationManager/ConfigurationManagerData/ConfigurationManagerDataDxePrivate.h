/** @file
  Configuration Manager Data Dxe Private Definitions

  Copyright (c) 2022, NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CONFIGURATION_MANAGER_DXE_PRIVATE_H__
#define __CONFIGURATION_MANAGER_DXE_PRIVATE_H__

#include <ConfigurationManagerObject.h>

#include <Library/ArmLib.h>
#include <Library/ArmGicLib.h>
#include <Library/ConfigurationManagerLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <libfdt.h>

#include <IndustryStandard/DebugPort2Table.h>
#include <IndustryStandard/Slit.h>
#include <IndustryStandard/IoRemappingTable.h>
#include <IndustryStandard/Mpam.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>

#include <Protocol/AmlPatchProtocol.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include <NVIDIAConfiguration.h>

#include "Platform.h"
#include "ConfigurationManagerDataPrivate.h"
#include <TH500/TH500Definitions.h>

#include <Dsdt.hex>
#include <Dsdt.offset.h>
#include <SsdtEth.hex>

#define ACPI_PATCH_MAX_PATH    255
#define ACPI_PCI_STA_TEMPLATE  "_SB_.PCI%d._STA"

#endif // __CONFIGURATION_MANAGER_DXE_PRIVATE_H__
