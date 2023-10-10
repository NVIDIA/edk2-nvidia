/** @file
  Configuration Manager Data Dxe Private Definitions

  Copyright (c) 2022 - 2023, NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CONFIGURATION_MANAGER_DXE_PRIVATE_H__
#define __CONFIGURATION_MANAGER_DXE_PRIVATE_H__

#include <ConfigurationManagerObject.h>

#include <Library/AmlLib/AmlLib.h>
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
#include <Library/Tpm2CommandLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <libfdt.h>

#include <IndustryStandard/DebugPort2Table.h>
#include <IndustryStandard/IoRemappingTable.h>
#include <IndustryStandard/Mpam.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>

#include <Protocol/AmlPatchProtocol.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <Protocol/RasNsCommPcieDpcDataProtocol.h>
#include <Protocol/BpmpIpc.h>

#include <NVIDIAConfiguration.h>

#include "Platform.h"
#include "ConfigurationManagerDataPrivate.h"
#include "ConfigurationSmbiosPrivate.h"
#include <TH500/TH500Definitions.h>

#include <Dsdt.hex>
#include <Dsdt.offset.h>
#include <SsdtEth.hex>
#include <SsdtSocket1.hex>
#include <SsdtSocket1.offset.h>
#include <SsdtSocket2.hex>
#include <SsdtSocket2.offset.h>
#include <SsdtSocket3.hex>
#include <SsdtSocket3.offset.h>
#include <BpmpSsdtSocket0.hex>
#include <BpmpSsdtSocket0.offset.h>
#include <BpmpSsdtSocket1.hex>
#include <BpmpSsdtSocket1.offset.h>
#include <BpmpSsdtSocket2.hex>
#include <BpmpSsdtSocket2.offset.h>
#include <BpmpSsdtSocket3.hex>
#include <BpmpSsdtSocket3.offset.h>
#include <SsdtTpm.hex>

#define ACPI_PATCH_MAX_PATH  255
#define ACPI_PLAT_INFO       "_SB_.PLAT"
#define ACPI_GED1_SMR1       "_SB_.GED1.SMR1"
#define ACPI_QSPI1_STA       "_SB_.QSP1._STA"
#define ACPI_TPM1_STA        "_SB_.TPM1._STA"
#define ACPI_I2C3_STA        "_SB_.I2C3._STA"
#define ACPI_SSIF_STA        "_SB_.I2C3.SSIF._STA"

#define MAX_DEVICES_PER_THERMAL_ZONE  10
#define MAX_UNICODE_STRING_LEN        128
typedef struct {
  UINT32          ZoneId;
  BOOLEAN         PassiveSupported;
  BOOLEAN         CriticalSupported;
  UINTN           *PassiveCpus;
  CONST CHAR16    *SocketFormatString;
} THERMAL_ZONE_DATA;

#endif // __CONFIGURATION_MANAGER_DXE_PRIVATE_H__
