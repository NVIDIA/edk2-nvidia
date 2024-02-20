/** @file
  IPMI Device Parser

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "NvCmObjectDescUtility.h"
#include "IpmiParser.h"

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

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
EFIAPI
IpmiParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle
  )
{
  EFI_STATUS               Status;
  UINT32                   Count;
  CM_STD_IPMI_DEVICE_INFO  *IpmiDeviceInfo;
  UINT32                   Handles;
  VOID                     *DtbBase;
  UINTN                    DtbSize;
  CONST VOID               *Property;
  INT32                    PropertyLen;
  UINT32                   I2cAddress;
  CM_OBJECT_TOKEN          *TokenMap;
  CM_OBJ_DESCRIPTOR        Desc;

  if (mIpmiDevCmInstalled) {
    return EFI_SUCCESS;
  }

  TokenMap = NULL;

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
    goto CleanupAndReturn;
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

  Status = NvAllocateCmTokens (ParserHandle, 1, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for IPMI: %r\n", __FUNCTION__, Status));
  }

  IpmiDeviceInfo->IpmiDeviceInfoToken = TokenMap[0];

  // Add the CmObj to the Configuration Manager.
  Desc.ObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjIpmiDeviceInfo);
  Desc.Size     = sizeof (CM_STD_IPMI_DEVICE_INFO);
  Desc.Count    = 1;
  Desc.Data     = IpmiDeviceInfo;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add IPMI to ConfigManager: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  mIpmiDevCmInstalled = TRUE;

CleanupAndReturn:
  FREE_NON_NULL (TokenMap);
  return Status;
}
