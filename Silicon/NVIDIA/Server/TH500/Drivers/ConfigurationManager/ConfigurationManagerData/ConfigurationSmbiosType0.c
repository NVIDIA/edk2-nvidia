/**
  Configuration Manager Data of SMBIOS Type 0 table

  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <libfdt.h>
#include <Library/IpmiBaseLib.h>
#include <Library/PcdLib.h>

#include <IndustryStandard/Ipmi.h>
#include <IndustryStandard/IpmiNetFnApp.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include "ConfigurationSmbiosPrivate.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType0 = {
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType00),
  NULL
};

typedef struct {
  UINT8    Major;
  UINT8    Minor;
} BMC_FW_VERSION;

/**
  This function send an Ipmi command to get the BMC firmware version.

  @param [OUT] Version  Pointer to the BMC firmware version

  @return EFI_SUCCESS       Successful retrieval
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
GetBmcRelease (
  BMC_FW_VERSION  *Version
  )
{
  EFI_STATUS                   Status;
  IPMI_GET_DEVICE_ID_RESPONSE  ResponseData;
  UINT32                       ResponseSize;

  SetMem ((VOID *)&ResponseData, sizeof (ResponseData), 0);
  ResponseSize = sizeof (ResponseData);

  //    Response data:
  //      Byte 1  : Completion code
  //      Byte 2  : Type
  //      Byte 3  : BMC Version major in hex format
  //      Byte 4  : BMC Version minor in BCD format

  Status = IpmiSubmitCommand (
             IPMI_NETFN_APP,
             IPMI_APP_GET_DEVICE_ID,
             NULL,
             0,
             (UINT8 *)&ResponseData,
             &ResponseSize
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %r returned from IpmiSubmitCommand()\n", __FUNCTION__, Status));
    return Status;
  }

  if (ResponseData.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Completion code = 0x%x. Returning\n",
      __FUNCTION__,
      ResponseData.CompletionCode
      ));
    return EFI_PROTOCOL_ERROR;
  }

  Version->Major = ResponseData.FirmwareRev1.Bits.MajorFirmwareRev;
  Version->Minor = BcdToDecimal8 (ResponseData.MinorFirmwareRev);

  return Status;
}

/**
  Field Filling Function. Transform an EFI_EXP_BASE2_DATA to a byte, with '64k'
  as the unit.

  @param  Value              Pointer to Base2_Data

  @retval

**/
UINT8
Base2ToByteWith64KUnit (
  IN  UINTN  Value
  )
{
  UINT8  Size;

  Size = ((Value + (SIZE_64KB - 1)) >> 16);

  return Size;
}

/**
  Install CM object for SMBIOS Type 0

   @param[in, out] Private   Pointer to the private data of SMBIOS creators

   @return EFI_SUCCESS       Successful installation
   @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
EFIAPI
InstallSmbiosType0Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo    = Private->Repo;
  VOID                            *DtbBase = Private->DtbBase;
  INTN                            DtbOffset;
  CM_STD_BIOS_INFO                *BiosInfo;
  CHAR16                          *Vendor;
  CHAR16                          *Version;
  CHAR16                          *ReleaseDate;
  CHAR8                           *VendorAsciiStr;
  CHAR8                           *VersionAsciiStr;
  CHAR8                           *ReleaseDateAsciiStr;
  UINT64                          BiosPhysicalSize;
  INT32                           Length;
  MISC_BIOS_CHARACTERISTICS       *BiosChar;
  CONST VOID                      *Property;
  BMC_FW_VERSION                  BmcRelease;

  //
  // Allocate and zero out Bios Info
  //
  BiosInfo = (CM_STD_BIOS_INFO *)AllocateZeroPool (sizeof (CM_STD_BIOS_INFO));
  if (BiosInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate bios info.\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  // Fill Type00 data
  // Fill SytemBiosMajorRelease and SytemBiosMinorRelease info from DTB
  DtbOffset = fdt_subnode_offset (DtbBase, Private->DtbSmbiosOffset, "type0");
  if (DtbOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Device tree node for SMBIOS Type 0 not found.\n", __FUNCTION__));
    BiosPhysicalSize = 0x00;
  } else {
    Property = fdt_getprop (DtbBase, DtbOffset, "rom_size", &Length);
    if ((Property == NULL) || (Length == 0)) {
      DEBUG ((DEBUG_ERROR, "%a: Device tree property 'rom_size' not found.\n", __FUNCTION__));
      BiosPhysicalSize = 0x00;
    } else {
      BiosPhysicalSize = (UINT64)fdt32_to_cpu (*(UINT32 *)Property);
    }
  }

  Vendor = (CHAR16 *)PcdGetPtr (PcdFirmwareVendor);
  if (StrLen (Vendor) > 0) {
    VendorAsciiStr =  (CHAR8 *)AllocateZeroPool (sizeof (CHAR8) * (StrLen (Vendor) + 1));
    if (VendorAsciiStr != NULL) {
      UnicodeStrToAsciiStrS (Vendor, VendorAsciiStr, StrLen (Vendor) + 1);
    }

    BiosInfo->BiosVendor = VendorAsciiStr;
  }

  Version = (CHAR16 *)PcdGetPtr (PcdFirmwareVersionString);
  if (StrLen (Version) > 0) {
    VersionAsciiStr = (CHAR8 *)AllocateZeroPool (sizeof (CHAR8) * (StrLen (Version) +1));
    if (VersionAsciiStr != NULL) {
      UnicodeStrToAsciiStrS (Version, VersionAsciiStr, StrLen (Version) +1);
    }

    BiosInfo->BiosVersion = VersionAsciiStr;
  }

  BiosInfo->BiosSegment = (UINT16)(FixedPcdGet32 (PcdFdBaseAddress) / SIZE_64KB);

  ReleaseDate = (CHAR16 *)PcdGetPtr (PcdFirmwareReleaseDateString);
  if (StrLen (ReleaseDate) > 0) {
    ReleaseDateAsciiStr = (CHAR8 *)AllocateZeroPool (sizeof (CHAR8) * (StrLen (ReleaseDate) + 1));
    if (ReleaseDateAsciiStr != NULL) {
      UnicodeStrToAsciiStrS (ReleaseDate, ReleaseDateAsciiStr, StrLen (ReleaseDate) + 1);
    }

    BiosInfo->BiosReleaseDate = ReleaseDateAsciiStr;
  }

  if (BiosPhysicalSize < SIZE_16MB) {
    BiosInfo->BiosSize = Base2ToByteWith64KUnit (BiosPhysicalSize) - 1;
  } else {
    BiosInfo->BiosSize = 0xFF;
    if (BiosPhysicalSize < SIZE_16GB) {
      BiosInfo->ExtendedBiosSize.Size = BiosPhysicalSize / SIZE_1MB;
      BiosInfo->ExtendedBiosSize.Unit = 0; // Size is in MB
    } else {
      BiosInfo->ExtendedBiosSize.Size = BiosPhysicalSize / SIZE_1GB;
      BiosInfo->ExtendedBiosSize.Unit = 1; // Size is in GB
    }
  }

  BiosChar                                       = (MISC_BIOS_CHARACTERISTICS *)(FixedPcdGetPtr (PcdBiosCharacteristics));
  BiosInfo->BiosCharacteristics                  = *(BiosChar);
  BiosInfo->BIOSCharacteristicsExtensionBytes[0] = (UINT8)(PcdGet16 (PcdBiosCharacteristicsExtension) & 0xFF);
  BiosInfo->BIOSCharacteristicsExtensionBytes[1] = (UINT8)(PcdGet16 (PcdBiosCharacteristicsExtension) >> 8);

  BiosInfo->SystemBiosMajorRelease = 0xFF;
  BiosInfo->SystemBiosMinorRelease = 0xFF;

  Status = GetBmcRelease (&BmcRelease);
  if (Status == EFI_SUCCESS) {
    BiosInfo->ECFirmwareMajorRelease = BmcRelease.Major;
    BiosInfo->ECFirmwareMinorRelease = BmcRelease.Minor;
  } else {
    BiosInfo->ECFirmwareMajorRelease = 0xFF;
    BiosInfo->ECFirmwareMinorRelease = 0xFF;
  }

  BiosInfo->BiosInfoToken = REFERENCE_TOKEN (BiosInfo[0]);
  //
  // Add type 0 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType0,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

  //
  // Install CM object for type 0
  //
  Repo->CmObjectId    = CREATE_CM_STD_OBJECT_ID (EStdObjBiosInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_STD_BIOS_INFO);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = BiosInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

  return EFI_SUCCESS;
}
