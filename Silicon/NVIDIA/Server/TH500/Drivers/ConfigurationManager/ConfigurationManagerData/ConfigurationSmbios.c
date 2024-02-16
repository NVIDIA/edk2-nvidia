/** @file
  Configuration Manager Data of SMBIOS tables

  Copyright (c) 2022 - 2023, NVIDIA CORPORATION. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/FruLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <libfdt.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "ConfigurationSmbiosPrivate.h"

/**
  Find FRU by FRU description

  @param[in] Private          Pointer to the private data of SMBIOS creators
  @param[in] FruDescPattern   FRU description pattern to look for.
                              Wildcard '?' can be used to match any characters.

  @return A pointer to the FRU record if found, or NULL if not found.

**/
FRU_DEVICE_INFO *
FindFruByDescription (
  IN  CM_SMBIOS_PRIVATE_DATA  *Private,
  IN  CHAR8                   *FruDescPattern
  )
{
  FRU_DEVICE_INFO  **FruInfo = Private->FruInfo;
  UINT8            FruCount  = Private->FruCount;
  UINT8            Index;
  CHAR8            *FruDesc;
  CHAR8            *Pattern;

  for (Index = 0; Index < FruCount; Index++) {
    FruDesc = FruInfo[Index]->FruDeviceDescription;
    Pattern = FruDescPattern;

    while ((*FruDesc != '\0') &&
           ((*Pattern == '?') || (*FruDesc == *Pattern)))
    {
      FruDesc++;
      Pattern++;
    }

    if (*FruDesc == *Pattern) {
      return FruInfo[Index];
    }
  }

  return NULL;
}

/**
  Find and get FRU extra string that has a certain prefix

  @param[in] FruExtra  Pointer to the array of FRU (chassis/board/product) extra
  @param[in] Prefix    FRU extra prefix to search for

  @return A pointer to an allocated string

**/
CHAR8 *
GetFruExtraStr (
  IN CHAR8        **FruExtra,
  IN CONST CHAR8  *Prefix
  )
{
  UINT32  Index;
  UINTN   PrefixLen;

  ASSERT (FruExtra != NULL);
  ASSERT (Prefix != NULL);

  PrefixLen = AsciiStrLen (Prefix);

  for (Index = 0; Index < MAX_EXTRA_FRU_AREA_ENTRIES; Index++) {
    if (FruExtra[Index] == NULL) {
      break;
    }

    if (AsciiStrnCmp (FruExtra[Index], Prefix, PrefixLen) == 0) {
      return AllocateCopyString (FruExtra[Index] + PrefixLen);
    }
  }

  return NULL;
}

/**
  Allocate and copy string

  @param[in] String     String to be copied

  @return A pointer to the copied string if succeeds, or NULL if fails.

**/
CHAR8 *
AllocateCopyString (
  IN  CHAR8  *String
  )
{
  if (String == NULL) {
    return NULL;
  }

  return (CHAR8 *)AllocateCopyPool (AsciiStrLen (String) + 1, String);
}

/**
  Install the SMBIOS tables to Configuration Manager Data driver

  @param[in, out] PlatformRepositoryInfo      Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd   End address of the Platform Repository
  @param[in]      PlatformRepositoryInfo      Pointer to the platform repository info

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallCmSmbiosTableList (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  )
{
  CM_SMBIOS_PRIVATE_DATA          *Private;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  EFI_STATUS                      Status;
  UINTN                           Index;
  CM_SMBIOS_RECORD_POPULATION     CmInstallSmbiosRecords[] = {
    { EFI_SMBIOS_TYPE_BIOS_INFORMATION,                     InstallSmbiosType0Cm   },
    { EFI_SMBIOS_TYPE_SYSTEM_INFORMATION,                   InstallSmbiosType1Cm   },
    { EFI_SMBIOS_TYPE_SYSTEM_ENCLOSURE,                     InstallSmbiosType3Cm   },
    { EFI_SMBIOS_TYPE_PROCESSOR_INFORMATION,                InstallSmbiosProcSubCm },
    { EFI_SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION,           InstallSmbiosType8Cm   },
    { EFI_SMBIOS_TYPE_SYSTEM_SLOTS,                         InstallSmbiosType9Cm   },
    { EFI_SMBIOS_TYPE_OEM_STRINGS,                          InstallSmbiosType11Cm  },
    { EFI_SMBIOS_TYPE_BIOS_LANGUAGE_INFORMATION,            InstallSmbiosType13Cm  },
    { EFI_SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY,                InstallSmbiosTypeMemCm },
    { EFI_SMBIOS_TYPE_BASEBOARD_INFORMATION,                InstallSmbiosType2Cm   },
    { EFI_SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION,              InstallSmbiosType32Cm  },
    { EFI_SMBIOS_TYPE_IPMI_DEVICE_INFORMATION,              InstallSmbiosType38Cm  },
    { EFI_SMBIOS_TYPE_SYSTEM_POWER_SUPPLY,                  InstallSmbiosType39Cm  },
    { EFI_SMBIOS_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION, InstallSmbiosType41Cm  },
    { SMBIOS_TYPE_TPM_DEVICE,                               InstallSmbiosType43Cm  },
    { SMBIOS_TYPE_FIRMWARE_INVENTORY_INFORMATION,           InstallSmbiosType45Cm  }
  };

  Private = AllocatePool (sizeof (CM_SMBIOS_PRIVATE_DATA));
  if (Private == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Private->CmSmbiosTableCount              = 0;
  Private->Repo                            = *PlatformRepositoryInfo;
  Private->RepoEnd                         = PlatformRepositoryInfoEnd;
  Private->EnclosureBaseboardBinding.Count = 0;
  Private->EnclosureBaseboardBinding.Info  = NULL;
  Private->PlatformRepositoryInfo          = NVIDIAPlatformRepositoryInfo;
  //
  // Load device tree SMBIOS node
  //
  Status = DtPlatformLoadDtb (&Private->DtbBase, &Private->DtbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to load device tree.\n", __FUNCTION__));
    FreePool (Private);
    return Status;
  }

  Private->DtbSmbiosOffset = fdt_path_offset (Private->DtbBase, "/firmware/smbios");
  if (Private->DtbSmbiosOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Device tree node for SMBIOS not found.\n", __FUNCTION__));
    // Continue anyway to install SMBIOS tables that do not need DTB
  }

  //
  // Read all FRUs
  //
  Status = ReadAllFrus (&Private->FruInfo, &Private->FruCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to read FRUs.\n", __FUNCTION__));
    // Continue anyway to install SMBIOS tables that do not need FRU info
    Private->FruCount = 0;
  }

  //
  // Install CM object for each SMBIOS table
  //
  for (Index = 0; Index < ARRAY_SIZE (CmInstallSmbiosRecords); Index++) {
    Status = CmInstallSmbiosRecords[Index].Function (Private);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Install CM object of SMBIOS Type %d, Status = %r.\n", __FUNCTION__, CmInstallSmbiosRecords[Index].Type, Status));
    }
  }

  //
  // Free all FRUs
  //
  Status = FreeAllFruRecords ();
  ASSERT_EFI_ERROR (Status);

  //
  // Install CM object for SMBIOS table list
  //
  if (Private->CmSmbiosTableCount > 0) {
    Repo                = Private->Repo;
    Repo->CmObjectId    = CREATE_CM_STD_OBJECT_ID (EStdObjSmbiosTableList);
    Repo->CmObjectToken = CM_NULL_TOKEN;
    Repo->CmObjectSize  = sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO) * Private->CmSmbiosTableCount;
    Repo->CmObjectCount = Private->CmSmbiosTableCount;
    Repo->CmObjectPtr   = AllocateCopyPool (Repo->CmObjectSize, &Private->CmSmbiosTableList);
    Repo++;

    ASSERT ((UINTN)Repo <= PlatformRepositoryInfoEnd);

    *PlatformRepositoryInfo = Repo;
  }

  FreePool (Private);
  return EFI_SUCCESS;
}
