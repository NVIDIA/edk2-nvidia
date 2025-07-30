/** @file
  Configuration Manager Data of SMBIOS Type 16/17/19 tables.

  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <Library/FruLib.h>
#include <TH500/TH500Definitions.h>
#include <libfdt.h>

#include <ConfigurationManagerObject.h>

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
  Install CM object for SMBIOS Type 17
   @param [in]  ParserHandle A handle to the parser instance.
   @param[in, out] Private   Pointer to the private data of SMBIOS creators

   @return EFI_SUCCESS       Successful installation
   @retval !(EFI_SUCCESS)    Other errors

 **/
STATIC
EFI_STATUS
EFIAPI
InstallSmbiosType17Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  EFI_STATUS                    Status;
  VOID                          *Hob;
  TEGRA_DRAM_DEVICE_INFO        *DramInfo;
  VOID                          *DtbBase;
  CM_SMBIOS_MEMORY_DEVICE_INFO  *CmMemDevicesInfo;
  UINTN                         DramDevicesCount;
  UINTN                         Index;
  CM_OBJ_DESCRIPTOR             Desc;
  CM_OBJECT_TOKEN               *TokenMapType17;
  FRU_DEVICE_INFO               *FruInfo;
  CHAR8                         *FruDesc;
  INT32                         NodeOffset;
  CONST VOID                    *Property;
  CHAR8                         Type4NodeStr[] = "/firmware/smbios/type4@xx";
  UINT32                        SocketMask;
  UINT32                        SocketIndex;
  UINT32                        DimmIndex;
  UINT32                        Count;
  UINT32                        DevCount;

  TokenMapType17   = NULL;
  CmMemDevicesInfo = NULL;
  Status           = EFI_SUCCESS;
  DtbBase          = Private->DtbBase;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    DramInfo   = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->DramDeviceInfo;
    SocketMask = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->SocketMask;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Platform Resource Info for Type 17 table\n",
      __FUNCTION__
      ));
    Status = EFI_NOT_FOUND;
    goto ExitInstallSmbiosType17Cm;
  }

  DramDevicesCount = CmMemPhysMemArray->NumMemDevices;

  if (DramDevicesCount == 0) {
    Status = EFI_UNSUPPORTED;
    DEBUG ((DEBUG_ERROR, "%a: DramDeviceCount is 0 - skipping Type 17 table\n", __FUNCTION__));
    goto ExitInstallSmbiosType17Cm;
  }

  CmMemDevicesInfo = AllocateZeroPool (sizeof (CM_SMBIOS_MEMORY_DEVICE_INFO) * DramDevicesCount);
  if (CmMemDevicesInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate 0x%llx bytes for DramDevices\n", __FUNCTION__, sizeof (CM_SMBIOS_MEMORY_DEVICE_INFO) * DramDevicesCount));
    goto ExitInstallSmbiosType17Cm;
  }

  for (SocketIndex = 0, DevCount = 0; SocketIndex < PLATFORM_MAX_SOCKETS; SocketIndex++) {
    if (!(SocketMask & (1UL << SocketIndex))) {
      continue;
    }

    DimmIndex = SocketIndex * MAX_DIMMS_PER_SOCKET;
    for (Count = 0; Count < ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->NumModules[SocketIndex]; Count++) {
      CmMemDevicesInfo[DevCount].ModuleManufacturerId = (UINT16)DramInfo[DimmIndex + Count].ManufacturerId;
      CmMemDevicesInfo[DevCount].SerialNum            = AllocateZeroPool (SMBIOS_TYPE17_MAX_STRLEN);
      if (CmMemDevicesInfo[DevCount].SerialNum != NULL) {
        AsciiSPrint (
          CmMemDevicesInfo[DevCount].SerialNum,
          SMBIOS_TYPE17_MAX_STRLEN,
          "%lu",
          DramInfo[DimmIndex + Count].SerialNumber
          );
      }

      if (AsciiStrLen ((CHAR8 *)DramInfo[DimmIndex + Count].PartNumber) != 0) {
        CmMemDevicesInfo[DevCount].PartNum = AllocateCopyString ((CHAR8 *)DramInfo[DimmIndex + Count].PartNumber);
      } else {
        //
        // For solder-down DRAMs design, now using processor board info
        // for DRAM part number reporting.
        //
        AsciiSPrint (
          Type4NodeStr,
          sizeof (Type4NodeStr),
          "/firmware/smbios/type4@%u",
          SocketIndex
          );

        NodeOffset = 0;
        NodeOffset = fdt_path_offset (DtbBase, Type4NodeStr);
        if (NodeOffset > 0) {
          Property = NULL;
          Property = fdt_getprop (DtbBase, NodeOffset, "fru-desc", NULL);
          if (Property != NULL) {
            FruInfo = NULL;
            FruDesc = (CHAR8 *)Property;
            FruInfo = FindFruByDescription (Private, FruDesc);
            if ((FruInfo != NULL) && (FruInfo->BoardPartNum != NULL)) {
              CmMemDevicesInfo[DevCount].PartNum = AllocateCopyString (FruInfo->BoardPartNum);
            }
          }
        }
      }

      //
      // Set the memory min/max/config voltage.
      //
      CmMemDevicesInfo[DevCount].MinVolt  = 1100;
      CmMemDevicesInfo[DevCount].MaxVolt  = 1100;
      CmMemDevicesInfo[DevCount].ConfVolt = 1100;

      CmMemDevicesInfo[DevCount].DeviceLocator = AllocateZeroPool (SMBIOS_TYPE17_MAX_STRLEN);
      if (CmMemDevicesInfo[DevCount].DeviceLocator != NULL) {
        AsciiSPrint (
          CmMemDevicesInfo[DevCount].DeviceLocator,
          SMBIOS_TYPE17_MAX_STRLEN,
          "LP5x_%u",
          DimmIndex + Count
          );
      }

      CmMemDevicesInfo[DevCount].BankLocator = AllocateZeroPool (SMBIOS_TYPE17_MAX_STRLEN);
      if (CmMemDevicesInfo[DevCount].BankLocator != NULL) {
        AsciiSPrint (
          CmMemDevicesInfo[DevCount].BankLocator,
          SMBIOS_TYPE17_MAX_STRLEN,
          "LP5x_%u",
          DimmIndex + Count
          );
      }

      CmMemDevicesInfo[DevCount].Size       = DramInfo[DimmIndex + Count].Size;
      CmMemDevicesInfo[DevCount].DataWidth  = DramInfo[DimmIndex + Count].DataWidth;
      CmMemDevicesInfo[DevCount].TotalWidth = DramInfo[DimmIndex + Count].TotalWidth;
      CmMemDevicesInfo[DevCount].Rank       = DramInfo[DimmIndex + Count].Rank;
      // Per spec the speed is to be reported in MT/s (Mega Transfers / second)
      CmMemDevicesInfo[DevCount].Speed                                             = ((DramInfo[DimmIndex + Count].SpeedKhz / 1000) * 2);
      CmMemDevicesInfo[DevCount].PhysicalArrayToken                                = PhysMemArrayToken;
      CmMemDevicesInfo[DevCount].DeviceType                                        = MemoryTypeLpddr5;
      CmMemDevicesInfo[DevCount].TypeDetail.Synchronous                            = 1;
      CmMemDevicesInfo[DevCount].TypeDetail.Unbuffered                             = 1;
      CmMemDevicesInfo[DevCount].DeviceTechnology                                  = MemoryTechnologyDram;
      CmMemDevicesInfo[DevCount].MemoryErrorInformationHandle                      = 0xFFFE;
      CmMemDevicesInfo[DevCount].ConfiguredMemorySpeed                             = CmMemDevicesInfo[DevCount].Speed;
      CmMemDevicesInfo[DevCount].MemoryOperatingModeCapability.Bits.VolatileMemory = 1;

      if (DramInfo[DimmIndex + Count].FormFactor == 0) {
        CmMemDevicesInfo[DevCount].FormFactor = MemoryFormFactorDie;
      } else if (DramInfo[DimmIndex + Count].FormFactor == 1) {
        CmMemDevicesInfo[DevCount].FormFactor = MemoryFormFactorCamm;
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Unsupported form factor for SMBIOS Type 17\n", __FUNCTION__));
      }

      DevCount++;
    }
  }

  // Allocate Token Map for Type 17
  Status = NvAllocateCmTokens (ParserHandle, DramDevicesCount, &TokenMapType17);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 17: %r\n", __FUNCTION__, Status));
    goto ExitInstallSmbiosType17Cm;
  }

  for (Index = 0; Index < DramDevicesCount; Index++) {
    CmMemDevicesInfo[Index].MemoryDeviceInfoToken = TokenMapType17[Index];
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
    goto ExitInstallSmbiosType17Cm;
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

ExitInstallSmbiosType17Cm:
  FREE_NON_NULL (TokenMapType17);
  FREE_NON_NULL (CmMemDevicesInfo); // Note: Don't free the subfield allocations, because CM may be pointing to them
  return Status;
}

/**
  Install CM object for SMBIOS Type 19
   @param [in]  ParserHandle A handle to the parser instance.
   @param[in, out] Private   Pointer to the private data of SMBIOS creators

   @return EFI_SUCCESS       Successful installation
   @retval !(EFI_SUCCESS)    Other errors

 **/
STATIC
EFI_STATUS
EFIAPI
InstallSmbiosType19Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  EFI_STATUS                             Status;
  VOID                                   *Hob;
  TEGRA_RESOURCE_INFO                    *ResourceInfo;
  VOID                                   *DtbBase;
  CM_SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS  *CmMemArrayMappedAddress;
  UINTN                                  SocketCount;
  UINTN                                  Index;
  UINT32                                 SocketMask;
  CM_OBJ_DESCRIPTOR                      Desc;
  CM_OBJECT_TOKEN                        *TokenMapType19;

  TokenMapType19          = NULL;
  CmMemArrayMappedAddress = NULL;
  Status                  = EFI_SUCCESS;
  DtbBase                 = Private->DtbBase;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    SocketMask   = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->SocketMask;
    ResourceInfo = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->ResourceInfo;
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get Platform Resource Info\n",
      __FUNCTION__
      ));
    Status = EFI_NOT_FOUND;
    goto ExitInstallSmbiosType19Cm;
  }

  for (Index = 0, SocketCount = 0; Index < PLATFORM_MAX_SOCKETS; Index++) {
    if (!(SocketMask & (1UL << Index))) {
      continue;
    }

    SocketCount++;
  }

  if (SocketCount == 0) {
    Status = EFI_UNSUPPORTED;
    DEBUG ((DEBUG_ERROR, "%a: DramDeviceCount is 0 - skipping Type 19 tables\n", __FUNCTION__));
    goto ExitInstallSmbiosType19Cm;
  }

  CmMemArrayMappedAddress = AllocateZeroPool (
                              sizeof (CM_SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS)
                              * SocketCount
                              );
  if (CmMemArrayMappedAddress == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate 0x%llx bytes for CmMemArrayMappedAddress\n", __FUNCTION__, sizeof (CM_SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS) * SocketCount));
    goto ExitInstallSmbiosType19Cm;
  }

  for (Index = 0; Index < SocketCount; Index++) {
    CmMemArrayMappedAddress[Index].StartingAddress = ResourceInfo->DramRegions[Index].MemoryBaseAddress;
    CmMemArrayMappedAddress[Index].EndingAddress   =
      (ResourceInfo->DramRegions[Index].MemoryBaseAddress + ResourceInfo->DramRegions[Index].MemoryLength - 1);
    CmMemArrayMappedAddress[Index].PhysMemArrayToken = PhysMemArrayToken;
  }

  // Allocate Token Map for Type 19
  Status = NvAllocateCmTokens (ParserHandle, SocketCount, &TokenMapType19);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 19: %r\n", __FUNCTION__, Status));
    goto ExitInstallSmbiosType19Cm;
  }

  for (Index = 0; Index < SocketCount; Index++) {
    CmMemArrayMappedAddress[Index].MemoryArrayMappedAddressToken = TokenMapType19[Index];
  }

  //
  // Install CM object for type 19
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjMemoryArrayMappedAddress);
  Desc.Size     = SocketCount * sizeof (CM_SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS);
  Desc.Count    = SocketCount;
  Desc.Data     = CmMemArrayMappedAddress;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMapType19, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 19 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto ExitInstallSmbiosType19Cm;
  }

  //
  // Add type 19 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType19,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

ExitInstallSmbiosType19Cm:
  FREE_NON_NULL (TokenMapType19);
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

    CmMemPhysMemArray->NumMemDevices += ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->NumModules[Index];
  }

  CmMemPhysMemArray->Location                     = MemoryArrayLocationSystemBoard;
  CmMemPhysMemArray->MemoryErrorCorrectionType    = MemoryErrorCorrectionMultiBitEcc;
  CmMemPhysMemArray->Use                          = MemoryArrayUseSystemMemory;
  CmMemPhysMemArray->Size                         = DramSize;
  CmMemPhysMemArray->MemoryErrorInformationHandle = 0xFFFE;

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

  Status = InstallSmbiosType17Cm (ParserHandle, Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to install Type 17 %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitInstallSmbiosTypeMem;
  }

  Status = InstallSmbiosType19Cm (ParserHandle, Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to install Type 19 %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitInstallSmbiosTypeMem;
  }

ExitInstallSmbiosTypeMem:
  FREE_NON_NULL (CmMemPhysMemArray);
  return Status;
}
