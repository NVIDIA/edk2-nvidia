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

#define PLATFORM_MAX_SOCKETS  (PcdGet32 (PcdTegraMaxSockets))

STATIC CM_OBJECT_TOKEN  PhysMemArrayToken = CM_NULL_TOKEN;

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType16 = {
  SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType16),
  NULL
};

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
  EFI_STATUS                       Status;
  VOID                             *Hob;
  EDKII_PLATFORM_REPOSITORY_INFO   *Repo;
  CM_SMBIOS_PHYSICAL_MEMORY_ARRAY  *CmMemPhysMemArray;
  UINTN                            Index;
  UINT64                           DramSize;
  UINT32                           SocketMask;

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

ExitInstallSmbiosTypeMem:
  return Status;
}
