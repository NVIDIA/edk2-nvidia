/**
  Configuration Manager Data of SMBIOS Type 16/17/19 tables.

  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.

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

#include "ConfigurationSmbiosPrivate.h"

#define PLATFORM_MAX_SOCKETS      (PcdGet32 (PcdTegraMaxSockets))
#define SMBIOS_TYPE17_MAX_STRLEN  (65)

STATIC CM_OBJECT_TOKEN                  PhysMemArrayToken = CM_NULL_TOKEN;
STATIC CM_SMBIOS_PHYSICAL_MEMORY_ARRAY  *CmMemPhysMemArray;

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

   @param[in, out] Private   Pointer to the private data of SMBIOS creators

   @return EFI_SUCCESS       Successful installation
   @retval !(EFI_SUCCESS)    Other errors

 **/
STATIC
EFI_STATUS
EFIAPI
InstallSmbiosType17Type19Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                             Status;
  VOID                                   *Hob;
  TEGRA_DRAM_DEVICE_INFO                 *DramInfo;
  TEGRA_RESOURCE_INFO                    *ResourceInfo;
  EDKII_PLATFORM_REPOSITORY_INFO         *Repo;
  VOID                                   *DtbBase;
  CM_SMBIOS_MEMORY_DEVICE_INFO           *CmMemDevicesInfo;
  CM_SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS  *CmMemArrayMappedAddress;
  UINTN                                  DramDevicesCount;
  UINTN                                  Index;

  Status  = EFI_SUCCESS;
  Repo    = Private->Repo;
  DtbBase = Private->DtbBase;

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
    goto ExitInstallSmbiosType17Type19Cm;
  }

  CmMemDevicesInfo = AllocateZeroPool (sizeof (CM_SMBIOS_MEMORY_DEVICE_INFO) * DramDevicesCount);
  if (CmMemDevicesInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitInstallSmbiosType17Type19Cm;
  }

  CmMemArrayMappedAddress = AllocateZeroPool (
                              sizeof (CM_SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS)
                              * DramDevicesCount
                              );
  if (CmMemArrayMappedAddress == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitInstallSmbiosType17Type19Cm;
  }

  for (Index = 0; Index < DramDevicesCount; Index++) {
    CmMemDevicesInfo[Index].SerialNum = AllocateZeroPool (SMBIOS_TYPE17_MAX_STRLEN);
    if (CmMemDevicesInfo[Index].SerialNum != NULL) {
      AsciiSPrint (
        CmMemDevicesInfo[Index].SerialNum,
        SMBIOS_TYPE17_MAX_STRLEN,
        "%lu",
        DramInfo[Index].SerialNumber
        );
    }

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

    CmMemDevicesInfo[Index].Size                  = DramInfo[Index].Size;
    CmMemDevicesInfo[Index].DataWidth             = DramInfo[Index].DataWidth;
    CmMemDevicesInfo[Index].TotalWidth            = DramInfo[Index].TotalWidth;
    CmMemDevicesInfo[Index].Rank                  = DramInfo[Index].Rank;
    CmMemDevicesInfo[Index].PhysicalArrayToken    = PhysMemArrayToken;
    CmMemDevicesInfo[Index].DeviceType            = MemoryTypeLpddr5;
    CmMemDevicesInfo[Index].DeviceTechnology      = MemoryTechnologyDram;
    CmMemDevicesInfo[Index].FormFactor            = MemoryFormFactorDie;
    CmMemDevicesInfo[Index].MemoryDeviceInfoToken = (CM_OBJECT_TOKEN)(DramInfo[Index].SerialNumber);

    CmMemArrayMappedAddress[Index].StartingAddress = ResourceInfo->DramRegions[Index].MemoryBaseAddress;
    CmMemArrayMappedAddress[Index].EndingAddress   =
      (ResourceInfo->DramRegions[Index].MemoryBaseAddress + ResourceInfo->DramRegions[Index].MemoryLength);
    CmMemArrayMappedAddress[Index].MemoryArrayMappedAddressToken = (CM_OBJECT_TOKEN)(CmMemArrayMappedAddress[Index].StartingAddress);
    CmMemArrayMappedAddress[Index].PhysMemArrayToken             = PhysMemArrayToken;
  }

  //
  // Install CM object for type 17
  //
  Repo->CmObjectId    = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjMemoryDeviceInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = DramDevicesCount * sizeof (CM_SMBIOS_MEMORY_DEVICE_INFO);
  Repo->CmObjectCount = DramDevicesCount;
  Repo->CmObjectPtr   = CmMemDevicesInfo;
  Repo++;

  //
  // Install CM object for type 19
  //
  Repo->CmObjectId    = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjMemoryArrayMappedAddress);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = DramDevicesCount * sizeof (CM_SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS);
  Repo->CmObjectCount = DramDevicesCount;
  Repo->CmObjectPtr   = CmMemArrayMappedAddress;
  Repo++;

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

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

ExitInstallSmbiosType17Type19Cm:
  return Status;
}

/**
Install CM object for SMBIOS Type 16

 @param[in, out] Private   Pointer to the private data of SMBIOS creators

 @return EFI_SUCCESS       Successful installation
 @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
EFIAPI
InstallSmbiosType16Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                      Status;
  VOID                            *Hob;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  UINTN                           Index;
  UINT64                          DramSize;
  UINT32                          SocketMask;

  Status = EFI_SUCCESS;
  Repo   = Private->Repo;

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
    goto ExitInstallSmbiosType16Cm;
  }

  CmMemPhysMemArray->PhysMemArrayToken  = (CM_OBJECT_TOKEN)CmMemPhysMemArray;
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
  // Add type 16 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType16,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;
  //
  // Install CM object for type 16
  //
  Repo->CmObjectId    = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjPhysicalMemoryArray);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_SMBIOS_PHYSICAL_MEMORY_ARRAY);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = CmMemPhysMemArray;
  Repo++;

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

ExitInstallSmbiosType16Cm:
  return Status;
}

/**
Install CM objects for memory related SMBIOS tables.

 @param[in, out] Private   Pointer to the private data of SMBIOS creators

 @return EFI_SUCCESS       Successful installation
 @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosTypeMemCm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  Status = InstallSmbiosType16Cm (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to install Type 16 %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitInstallSmbiosTypeMem;
  }

  Status = InstallSmbiosType17Type19Cm (Private);
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
  return Status;
}
