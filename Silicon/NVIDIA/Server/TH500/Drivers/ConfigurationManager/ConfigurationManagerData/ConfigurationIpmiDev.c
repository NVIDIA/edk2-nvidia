/** @file
  Configuration Manager Data of IPMI Device Information

  Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/MemoryAllocationLib.h>
#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <libfdt.h>

STATIC BOOLEAN  mIpmiDevCmInstalled = FALSE;

/**
  Install CM object for IPMI device information

  @param[in, out] PlatformRepositoryInfo      Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd   End address of the Platform Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
EFIAPI
InstallIpmiDeviceInformationCm (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd
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

  Repo = *PlatformRepositoryInfo;

  if (mIpmiDevCmInstalled) {
    return EFI_SUCCESS;
  }

  //
  // Allocate and zero out Ipmi Device Info
  //
  IpmiDeviceInfo = AllocateZeroPool (sizeof (CM_STD_IPMI_DEVICE_INFO));
  if (IpmiDeviceInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate Ipmi device info.\n", __FUNCTION__));
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
    DEBUG ((DEBUG_ERROR, "%a: Error: %u SSIF interfaces found in DT\n", __FUNCTION__, Count));
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

  IpmiDeviceInfo->IpmiUid = 0x00;

  IpmiDeviceInfo->IpmiDeviceInfoToken = REFERENCE_TOKEN (IpmiDeviceInfo[0]);

  //
  // Install CM object for Ipmi device info
  //
  Repo->CmObjectId    = CREATE_CM_STD_OBJECT_ID (EStdObjIpmiDeviceInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_STD_IPMI_DEVICE_INFO);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = IpmiDeviceInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= PlatformRepositoryInfoEnd);

  *PlatformRepositoryInfo = Repo;

  mIpmiDevCmInstalled = TRUE;

  return EFI_SUCCESS;
}
