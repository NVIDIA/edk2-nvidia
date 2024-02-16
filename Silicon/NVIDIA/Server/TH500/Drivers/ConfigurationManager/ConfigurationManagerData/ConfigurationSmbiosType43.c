/** @file
  Configuration Manager Data of SMBIOS Type 43 table

  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IpmiBaseLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/Tpm2CommandLib.h>
#include <libfdt.h>

#include <IndustryStandard/Ipmi.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "ConfigurationSmbiosPrivate.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType43 = {
  SMBIOS_TYPE_TPM_DEVICE,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType43),
  NULL
};

/**
  Install CM object for SMBIOS Type 43

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType43Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo    = Private->Repo;
  VOID                            *DtbBase = Private->DtbBase;
  CM_SMBIOS_TPM_DEVICE_INFO       *TpmInfo;
  EFI_STATUS                      Status;
  INTN                            DtbOffset;
  CONST VOID                      *Property;
  INT32                           Length;
  CHAR8                           *DescStr = "TPM";
  UINT32                          ManufacturerID;
  UINT32                          FirmwareVersion1;
  UINT32                          FirmwareVersion2;

  if (!PcdGetBool (PcdTpmEnable)) {
    return EFI_NOT_FOUND;
  }

  //
  // Read Vendor ID from TPM device
  //
  Status = Tpm2GetCapabilityManufactureID (&ManufacturerID);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read TPM manufacturer ID - %r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  //
  // Read firmware version from TPM device
  //
  Status = Tpm2GetCapabilityFirmwareVersion (&FirmwareVersion1, &FirmwareVersion2);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read TPM firmware version - %r\n", __FUNCTION__, Status));
    return EFI_DEVICE_ERROR;
  }

  //
  // Get TPM description from DTB
  //
  DtbOffset = fdt_subnode_offset (DtbBase, Private->DtbSmbiosOffset, "type43");
  if (DtbOffset < 0) {
    DEBUG ((DEBUG_INFO, "%a: Device tree node for SMBIOS Type 43 not found.\n", __FUNCTION__));
  } else {
    Property = fdt_getprop (DtbBase, DtbOffset, "description", &Length);
    if ((Property == NULL) || (Length == 0)) {
      DEBUG ((DEBUG_INFO, "%a: Device tree property 'type43/description' not found.\n", __FUNCTION__));
    } else {
      DescStr = (CHAR8 *)Property;
    }
  }

  //
  // Allocate and zero out TPM Info. The strings that are NULL will be set as "Unknown"
  //
  TpmInfo = (CM_SMBIOS_TPM_DEVICE_INFO *)AllocateZeroPool (sizeof (CM_SMBIOS_TPM_DEVICE_INFO));
  if (TpmInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for TPM info\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (&TpmInfo->VendorID, &ManufacturerID, sizeof (TpmInfo->VendorID));
  TpmInfo->MajorSpecVersion   = 0x02; // TPM 2.0
  TpmInfo->MinorSpecVersion   = 0x00;
  TpmInfo->FirmwareVersion1   = FirmwareVersion1;
  TpmInfo->FirmwareVersion2   = FirmwareVersion2;
  TpmInfo->Description        = AllocateCopyString (DescStr);
  TpmInfo->Characteristics    = 0;
  TpmInfo->OemDefined         = 0;
  TpmInfo->TpmDeviceInfoToken = REFERENCE_TOKEN (TpmInfo[0]);

  //
  // Add type 43 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType43,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

  //
  // Install CM object for type 43
  //
  Repo->CmObjectId    = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjTpmDeviceInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_SMBIOS_TPM_DEVICE_INFO);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = TpmInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

  return EFI_SUCCESS;
}
