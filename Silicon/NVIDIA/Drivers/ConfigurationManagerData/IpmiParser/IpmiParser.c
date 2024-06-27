/** @file
  IPMI Device Parser

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "IpmiParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MemoryAllocationLib.h>

BOOLEAN  mIpmiDevCmInstalled = FALSE;

STATIC
CONST CHAR8  *SsifCompatibility[] = {
  "ssif-bmc",
  NULL
};

/**
  Install CM object for IPMI device information

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
EFIAPI
IpmiParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                  Status;
  UINT32                      Count;
  CM_SMBIOS_IPMI_DEVICE_INFO  *IpmiDeviceInfo;
  CM_OBJECT_TOKEN             *TokenMap;
  CM_OBJ_DESCRIPTOR           Desc;
  UINT32                      Index;
  INT32                       NodeOffset;
  UINT32                      SlaveAddress;

  if (mIpmiDevCmInstalled) {
    return EFI_SUCCESS;
  }

  TokenMap = NULL;

  Status = DeviceTreeGetCompatibleNodeCount (SsifCompatibility, &Count);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get SSIF count\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  IpmiDeviceInfo = (CM_SMBIOS_IPMI_DEVICE_INFO *)AllocateZeroPool (sizeof (CM_SMBIOS_IPMI_DEVICE_INFO) * Count);
  if (IpmiDeviceInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate Ipmi device info.\n", __FUNCTION__));
    goto CleanupAndReturn;
  }

  Status = NvAllocateCmTokens (ParserHandle, Count, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate %u tokens for IPMI: %r\n", __FUNCTION__, Count, Status));
  }

  NodeOffset = -1;
  Index      = 0;
  while (EFI_SUCCESS == DeviceTreeGetNextCompatibleNode (SsifCompatibility, &NodeOffset)) {
    Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "reg", &SlaveAddress);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to get device slave address - %r\r\n", __func__, Status));
      goto CleanupAndReturn;
    }

    IpmiDeviceInfo[Index].IpmiIntfType = IPMIDeviceInfoInterfaceTypeSSIF;

    IpmiDeviceInfo[Index].IpmiSpecRevision = 0x20;

    IpmiDeviceInfo[Index].IpmiI2CSlaveAddress = SlaveAddress;

    IpmiDeviceInfo[Index].IpmiNVStorageDevAddress = 0x00;

    // Per IPMI spec, if the BMC uses SSIF, this field is equal to SlaveAddress
    IpmiDeviceInfo[Index].IpmiBaseAddress = SlaveAddress;

    // This field is unused and set to 0x00 for SSIF, per IPMI Spec
    IpmiDeviceInfo[Index].IpmiBaseAddModIntInfo = 0x00;

    // Per IPMI spec, this field is set to 0x00
    IpmiDeviceInfo[Index].IpmiInterruptNum = 0x00;

    IpmiDeviceInfo[Index].IpmiUid = Index;

    IpmiDeviceInfo[Index].IpmiDeviceInfoToken = TokenMap[Index];

    Index++;
  }

  // Add the CmObj to the Configuration Manager.
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjIpmiDeviceInfo);
  Desc.Size     = sizeof (CM_SMBIOS_IPMI_DEVICE_INFO) * Index;
  Desc.Count    = Index;
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

REGISTER_PARSER_FUNCTION (IpmiParser, "skip-ipmi-table")
