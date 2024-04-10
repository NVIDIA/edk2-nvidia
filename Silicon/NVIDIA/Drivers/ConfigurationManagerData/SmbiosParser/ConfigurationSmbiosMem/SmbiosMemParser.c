/** @file
  Configuration Manager Data of SMBIOS Type 16/17/19 tables.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "SmbiosParserPrivate.h"
#include "SmbiosMemParser.h"

#define PLATFORM_MAX_SOCKETS      (PcdGet32 (PcdTegraMaxSockets))
#define SMBIOS_TYPE17_MAX_STRLEN  (65)

STATIC CM_OBJECT_TOKEN                  PhysMemArrayToken  = CM_NULL_TOKEN;
STATIC CM_SMBIOS_PHYSICAL_MEMORY_ARRAY  *CmMemPhysMemArray = NULL;

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType16 = {
  SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType16),
  NULL
};

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType17 = {
  SMBIOS_TYPE_MEMORY_DEVICE,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType17),
  NULL
};

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType19 = {
  SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType19),
  NULL
};

/**
  Install CM object for SMBIOS Type 17 and Type 19
   @param [in]  ParserHandle A handle to the parser instance.
   @param[in, out] Private   Pointer to the private data of SMBIOS creators

   @return EFI_SUCCESS       Successful installation
   @retval !(EFI_SUCCESS)    Other errors

 **/
STATIC
EFI_STATUS
EFIAPI
InstallSmbiosType17Type19Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  EFI_STATUS                             Status;
  VOID                                   *Hob;
  TEGRA_DRAM_DEVICE_INFO                 *DramInfo;
  TEGRA_RESOURCE_INFO                    *ResourceInfo;
  VOID                                   *DtbBase;
  CM_SMBIOS_MEMORY_DEVICE_INFO           *CmMemDevicesInfo;
  CM_SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS  *CmMemArrayMappedAddress;
  UINTN                                  DramDevicesCount;
  UINTN                                  Index;
  CM_OBJ_DESCRIPTOR                      Desc;
  CM_OBJECT_TOKEN                        *TokenMapType17;
  CM_OBJECT_TOKEN                        *TokenMapType19;

  TokenMapType17          = NULL;
  TokenMapType19          = NULL;
  CmMemDevicesInfo        = NULL;
  CmMemArrayMappedAddress = NULL;
  Status                  = EFI_SUCCESS;
  DtbBase                 = Private->DtbBase;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    DramInfo     = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->DramDeviceInfo;
    ResourceInfo = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ResourceInfo;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Platform Resource Info\n",
      __FUNCTION__
      ));
    Status = EFI_NOT_FOUND;
    goto ExitInstallSmbiosType17Type19Cm;
  }

  DramDevicesCount = CmMemPhysMemArray->NumMemDevices;

  if (DramDevicesCount == 0) {
    Status = EFI_UNSUPPORTED;
    DEBUG ((DEBUG_ERROR, "%a: DramDeviceCount is 0 - skipping Type 17 and 19 tables\n", __FUNCTION__));
    goto ExitInstallSmbiosType17Type19Cm;
  }

  CmMemDevicesInfo = AllocateZeroPool (sizeof (CM_SMBIOS_MEMORY_DEVICE_INFO) * DramDevicesCount);
  if (CmMemDevicesInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate 0x%llx bytes for DramDevices\n", __FUNCTION__, sizeof (CM_SMBIOS_MEMORY_DEVICE_INFO) * DramDevicesCount));
    goto ExitInstallSmbiosType17Type19Cm;
  }

  CmMemArrayMappedAddress = AllocateZeroPool (
                              sizeof (CM_SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS)
                              * DramDevicesCount
                              );
  if (CmMemArrayMappedAddress == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate 0x%llx bytes for CmMemArrayMappedAddress\n", __FUNCTION__, sizeof (CM_SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS) * DramDevicesCount));
    goto ExitInstallSmbiosType17Type19Cm;
  }

  for (Index = 0; Index < DramDevicesCount; Index++) {
    CmMemDevicesInfo[Index].ModuleManufacturerId = (UINT16)DramInfo[Index].ManufacturerId;
    CmMemDevicesInfo[Index].SerialNum            = AllocateZeroPool (SMBIOS_TYPE17_MAX_STRLEN);
    if (CmMemDevicesInfo[Index].SerialNum != NULL) {
      AsciiSPrint (
        CmMemDevicesInfo[Index].SerialNum,
        SMBIOS_TYPE17_MAX_STRLEN,
        "%lu",
        DramInfo[Index].SerialNumber
        );
    }

    //
    // Set the memory module manufacturer as NVIDIA.
    //
    CmMemDevicesInfo[Index].ModuleManufacturerId = 0x6B03;

    CmMemDevicesInfo[Index].DeviceLocator = AllocateZeroPool (SMBIOS_TYPE17_MAX_STRLEN);
    if (CmMemDevicesInfo[Index].DeviceLocator != NULL) {
      AsciiSPrint (
        CmMemDevicesInfo[Index].DeviceLocator,
        SMBIOS_TYPE17_MAX_STRLEN,
        "LP5x_%u",
        Index
        );
    }

    CmMemDevicesInfo[Index].BankLocator = AllocateZeroPool (SMBIOS_TYPE17_MAX_STRLEN);
    if (CmMemDevicesInfo[Index].BankLocator != NULL) {
      AsciiSPrint (
        CmMemDevicesInfo[Index].BankLocator,
        SMBIOS_TYPE17_MAX_STRLEN,
        "LP5x_%u",
        Index
        );
    }

    CmMemDevicesInfo[Index].Size       = DramInfo[Index].Size;
    CmMemDevicesInfo[Index].DataWidth  = DramInfo[Index].DataWidth;
    CmMemDevicesInfo[Index].TotalWidth = DramInfo[Index].TotalWidth;
    CmMemDevicesInfo[Index].Rank       = DramInfo[Index].Rank;
    // Per spec the speed is to be reported in MT/s (Mega Transfers / second)
    CmMemDevicesInfo[Index].Speed              = ((DramInfo[Index].SpeedKhz / 1000) * 2);
    CmMemDevicesInfo[Index].PhysicalArrayToken = PhysMemArrayToken;
    CmMemDevicesInfo[Index].DeviceType         = MemoryTypeLpddr5;
    CmMemDevicesInfo[Index].DeviceTechnology   = MemoryTechnologyDram;
    CmMemDevicesInfo[Index].FormFactor         = MemoryFormFactorDie;

    CmMemArrayMappedAddress[Index].StartingAddress = ResourceInfo->DramRegions[Index].MemoryBaseAddress;
    CmMemArrayMappedAddress[Index].EndingAddress   =
      (ResourceInfo->DramRegions[Index].MemoryBaseAddress + ResourceInfo->DramRegions[Index].MemoryLength);
    CmMemArrayMappedAddress[Index].PhysMemArrayToken = PhysMemArrayToken;
  }

  // Allocate Token Map for Type 17
  Status = NvAllocateCmTokens (ParserHandle, DramDevicesCount, &TokenMapType17);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 17: %r\n", __FUNCTION__, Status));
    goto ExitInstallSmbiosType17Type19Cm;
  }

  // Allocate Token Map for Type 19
  Status = NvAllocateCmTokens (ParserHandle, DramDevicesCount, &TokenMapType19);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 19: %r\n", __FUNCTION__, Status));
    goto ExitInstallSmbiosType17Type19Cm;
  }

  for (Index = 0; Index < DramDevicesCount; Index++) {
    CmMemDevicesInfo[Index].MemoryDeviceInfoToken                = TokenMapType17[Index];
    CmMemArrayMappedAddress[Index].MemoryArrayMappedAddressToken = TokenMapType19[Index];
  }

  //
  // Install CM object for type 17
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjMemoryDeviceInfo);
  Desc.Size     = DramDevicesCount * sizeof (CM_SMBIOS_MEMORY_DEVICE_INFO);
  Desc.Count    = DramDevicesCount;
  Desc.Data     = CmMemDevicesInfo;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMapType17, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 17 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto ExitInstallSmbiosType17Type19Cm;
  }

  //
  // Install CM object for type 19
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjMemoryArrayMappedAddress);
  Desc.Size     = DramDevicesCount * sizeof (CM_SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS);
  Desc.Count    = DramDevicesCount;
  Desc.Data     = CmMemArrayMappedAddress;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMapType19, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 19 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto ExitInstallSmbiosType17Type19Cm;
  }

  //
  // Add type 17 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType17,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

  //
  // Add type 19 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType19,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

ExitInstallSmbiosType17Type19Cm:
  FREE_NON_NULL (TokenMapType17);
  FREE_NON_NULL (TokenMapType19);
  FREE_NON_NULL (CmMemDevicesInfo); // Note: Don't free the subfield allocations, because CM may be pointing to them
  FREE_NON_NULL (CmMemArrayMappedAddress);
  return Status;
}

/**
Install CM object for SMBIOS Type 16
 @param [in]  ParserHandle A handle to the parser instance.
 @param[in, out] Private   Pointer to the private data of SMBIOS creators

 @return EFI_SUCCESS       Successful installation
 @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
EFIAPI
InstallSmbiosType16Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  EFI_STATUS         Status;
  VOID               *Hob;
  UINTN              Index;
  UINT64             DramSize;
  UINT32             SocketMask;
  CM_OBJECT_TOKEN    *TokenMap;
  CM_OBJ_DESCRIPTOR  Desc;

  TokenMap = NULL;
  Status   = EFI_SUCCESS;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    DramSize   = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->PhysicalDramSize;
    SocketMask = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->SocketMask;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Platform Resource Info\n",
      __FUNCTION__
      ));
    Status = EFI_NOT_FOUND;
    goto ExitInstallSmbiosType16Cm;
  }

  CmMemPhysMemArray = AllocateZeroPool (sizeof (CM_SMBIOS_PHYSICAL_MEMORY_ARRAY));
  if (CmMemPhysMemArray == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate 0x%llx bytes for CmMemPhysMemArray\n", __FUNCTION__, sizeof (CM_SMBIOS_PHYSICAL_MEMORY_ARRAY)));
    goto ExitInstallSmbiosType16Cm;
  }

  // Allocate Token Map
  Status = NvAllocateCmTokens (ParserHandle, 1, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 16: %r\n", __FUNCTION__, Status));
    goto ExitInstallSmbiosType16Cm;
  }

  CmMemPhysMemArray->PhysMemArrayToken  = TokenMap[0];
  PhysMemArrayToken                     = CmMemPhysMemArray->PhysMemArrayToken;
  CmMemPhysMemArray->MemoryErrInfoToken = CM_NULL_TOKEN;
  for (Index = 0, CmMemPhysMemArray->NumMemDevices = 0; Index < PLATFORM_MAX_SOCKETS; Index++) {
    if (!(SocketMask & (1UL << Index))) {
      continue;
    }

    CmMemPhysMemArray->NumMemDevices++;
  }

  CmMemPhysMemArray->Location                  = MemoryArrayLocationSystemBoard;
  CmMemPhysMemArray->MemoryErrorCorrectionType = MemoryErrorCorrectionSingleBitEcc;
  CmMemPhysMemArray->Use                       = MemoryArrayUseSystemMemory;
  CmMemPhysMemArray->Size                      = DramSize;

  //
  // Install CM object for type 16
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjPhysicalMemoryArray);
  Desc.Size     = sizeof (CM_SMBIOS_PHYSICAL_MEMORY_ARRAY);
  Desc.Count    = 1;
  Desc.Data     = CmMemPhysMemArray;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 16 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto ExitInstallSmbiosType16Cm;
  }

  //
  // Add type 16 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType16,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

ExitInstallSmbiosType16Cm:
  FREE_NON_NULL (TokenMap);
  return Status;
}

/**
Install CM objects for memory related SMBIOS tables.
 @param [in]  ParserHandle A handle to the parser instance.
 @param[in, out] Private   Pointer to the private data of SMBIOS creators

 @return EFI_SUCCESS       Successful installation
 @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosTypeMemCm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  EFI_STATUS  Status;

  Status = InstallSmbiosType16Cm (ParserHandle, Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to install Type 16 %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitInstallSmbiosTypeMem;
  }

  Status = InstallSmbiosType17Type19Cm (ParserHandle, Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to install Type 17/19 %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitInstallSmbiosTypeMem;
  }

ExitInstallSmbiosTypeMem:
  FREE_NON_NULL (CmMemPhysMemArray);
  return Status;
}
