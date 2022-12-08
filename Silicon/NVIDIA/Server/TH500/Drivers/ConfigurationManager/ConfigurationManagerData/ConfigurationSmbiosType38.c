/**
  Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/MemoryAllocationLib.h>
#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <libfdt.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include "ConfigurationManagerDataPrivate.h"

#include "ConfigurationSmbiosPrivate.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType38 = {
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType38),
  NULL
};

/**
  Install CM object for SMBIOS Type 38

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
EFIAPI
InstallSmbiosType38Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                      Status;
  UINT32                          Count;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  CM_STD_IPMI_DEVICE_INFO         *IpmiDeviceInfo;
  UINT32                          Handles;
  VOID                            *DtbBase;
  UINTN                           DtbSize;
  CONST VOID                      *Property;
  INT32                           PropertyLen;
  UINT32                          I2cAddress;

  Repo = Private->Repo;

  //
  // Allocate and zero out Ipmi Device Info
  //
  IpmiDeviceInfo = AllocateZeroPool (sizeof (CM_STD_IPMI_DEVICE_INFO));
  if (IpmiDeviceInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate Ipmi device info.\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to load device tree.\n", __FUNCTION__));
    return Status;
  }

  Count  = 1; // Only one SSIF interface is expected
  Status = GetMatchingEnabledDeviceTreeNodes ("ssif-bmc", &Handles, &Count);

  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "%a: No SSIF support on this system.\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  if (Status == EFI_BUFFER_TOO_SMALL) {
    DEBUG ((DEBUG_ERROR, "%a: Error: %d SSIF interfaces found in DT\n", __FUNCTION__, Count));
    return EFI_UNSUPPORTED;
  }

  Property = fdt_getprop (DtbBase, Handles, "reg", &PropertyLen);
  if ((Property != NULL) && (PropertyLen == sizeof (UINT32))) {
    I2cAddress = fdt32_to_cpu (*(UINT32 *)Property);
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Unable to get SSIF information from DT. Returning\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  IpmiDeviceInfo->IpmiIntfType = IPMIDeviceInfoInterfaceTypeSSIF;

  IpmiDeviceInfo->IpmiSpecRevision = 0x20;

  IpmiDeviceInfo->IpmiI2CSlaveAddress = I2cAddress;

  IpmiDeviceInfo->IpmiNVStorageDevAddress = 0x00;

  // Per IPMI spec, if the BMC uses SSIF, this field is equal to SlaveAddress
  IpmiDeviceInfo->IpmiBaseAddress = I2cAddress;

  // This field is unused and set to 0x00 for SSIF, per IPMI Spec
  IpmiDeviceInfo->IpmiBaseAddModIntInfo = 0x00;

  // Per IPMI spec, this field is set to 0x00
  IpmiDeviceInfo->IpmiInterruptNum = 0x00;

  IpmiDeviceInfo->IpmiDeviceInfoToken = REFERENCE_TOKEN (IpmiDeviceInfo[0]);

  //
  // Add type 38 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType38,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

  //
  // Install CM object for type 38
  //
  Repo->CmObjectId    = CREATE_CM_STD_OBJECT_ID (EStdObjIpmiDeviceInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_STD_IPMI_DEVICE_INFO);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = IpmiDeviceInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

  return EFI_SUCCESS;
}
