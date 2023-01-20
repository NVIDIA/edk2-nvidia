/** @file
  Configuration Manager Data of SMBIOS Type 41 table

  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <libfdt.h>
#include <Library/PrintLib.h>
#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include <Library/PciSegmentLib.h>
#include "ConfigurationSmbiosPrivate.h"

typedef struct {
  MISC_ONBOARD_DEVICE_TYPE    DeviceType;
  UINT8                       Instance;
} DEVICE_TYPE_INSTANCE;

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType41 = {
  SMBIOS_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType41),
  NULL
};

UINT8
EFIAPI
GetOnboardDeviceInstance (
  UINT8                 DeviceType,
  DEVICE_TYPE_INSTANCE  **DeviceTypeInstances,
  UINTN                 *DeviceTypeInstanceCount
  )
{
  EFI_STATUS            Status;
  UINTN                 DeviceTypeIndex;
  UINT8                 DeviceTypeInstance;
  DEVICE_TYPE_INSTANCE  *TempDeviceTypeInstances;

  Status                  = EFI_NOT_FOUND;
  DeviceTypeInstance      = 0;
  TempDeviceTypeInstances = *DeviceTypeInstances;

  for (DeviceTypeIndex = 0; DeviceTypeIndex < *DeviceTypeInstanceCount; DeviceTypeIndex++) {
    if (DeviceType == (*DeviceTypeInstances)[DeviceTypeIndex].DeviceType) {
      DeviceTypeInstance = ++(*DeviceTypeInstances)[DeviceTypeIndex].Instance;
      Status             = EFI_SUCCESS;
      break;
    }
  }

  if (Status == EFI_NOT_FOUND) {
    TempDeviceTypeInstances = ReallocatePool (
                                sizeof (DEVICE_TYPE_INSTANCE) * (*DeviceTypeInstanceCount),
                                sizeof (DEVICE_TYPE_INSTANCE) * (*DeviceTypeInstanceCount + 1),
                                *DeviceTypeInstances
                                );
    if (TempDeviceTypeInstances != NULL) {
      //
      // Appending new device type to pool.
      //
      *DeviceTypeInstances                                        = TempDeviceTypeInstances;
      (*DeviceTypeInstances)[*DeviceTypeInstanceCount].DeviceType = DeviceType;
      (*DeviceTypeInstances)[*DeviceTypeInstanceCount].Instance   = 1;

      DeviceTypeInstance = (*DeviceTypeInstances)[*DeviceTypeInstanceCount].Instance;
      (*DeviceTypeInstanceCount)++;
    }
  }

  return DeviceTypeInstance;
}

/**
  Install CM object for SMBIOS Type 41

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType41Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO       *Repo    = Private->Repo;
  VOID                                 *DtbBase = Private->DtbBase;
  CM_STD_ONBOARD_DEVICE_EXTENDED_INFO  *OnboardDeviceExInfo;
  EFI_STATUS                           Status;
  CONST VOID                           *Property;
  CONST CHAR8                          *PropertyStr;
  INT32                                Length;
  UINTN                                Index;
  UINTN                                NumOnboardDevices;
  INT32                                NodeOffset;
  UINT8                                DeviceType;
  CHAR8                                Type41NodeStr[] = "/firmware/smbios/type41@xx";
  DEVICE_TYPE_INSTANCE                 *DeviceTypeInstances;
  UINTN                                DeviceTypeInstanceCount;
  UINT32                               VendorDeviceId;
  UINT32                               SegmentNum;
  UINT32                               BusNum;
  UINT32                               DevFuncNum;

  NumOnboardDevices       = 0;
  OnboardDeviceExInfo     = NULL;
  DeviceTypeInstances     = NULL;
  DeviceTypeInstanceCount = 0;

  for (Index = 0; Index < MAX_TYPE41_COUNT; Index++) {
    ZeroMem (Type41NodeStr, sizeof (Type41NodeStr));
    AsciiSPrint (Type41NodeStr, sizeof (Type41NodeStr), "/firmware/smbios/type41@%u", Index);
    NodeOffset = fdt_path_offset (DtbBase, Type41NodeStr);
    if (NodeOffset < 0) {
      Status = EFI_NOT_FOUND;
      break;
    }

    OnboardDeviceExInfo = ReallocatePool (
                            sizeof (CM_STD_ONBOARD_DEVICE_EXTENDED_INFO) * (NumOnboardDevices),
                            sizeof (CM_STD_ONBOARD_DEVICE_EXTENDED_INFO) * (NumOnboardDevices + 1),
                            OnboardDeviceExInfo
                            );
    if (OnboardDeviceExInfo == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    SegmentNum                                                  = 0;
    BusNum                                                      = 0;
    DevFuncNum                                                  = 0;
    DeviceType                                                  = 0;
    VendorDeviceId                                              = (UINT32)-1;
    OnboardDeviceExInfo[NumOnboardDevices].ReferenceDesignation = NULL;

    Property = fdt_getprop (DtbBase, NodeOffset, "device-type", &Length);
    if (Property != NULL) {
      DeviceType = (UINT8)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "segment-group-number", &Length);
    if (Property != NULL) {
      SegmentNum = (UINT16)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "bus-number", &Length);
    if (Property != NULL) {
      BusNum = (UINT8)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "device-function-number", &Length);
    if (Property != NULL) {
      DevFuncNum = (UINT8)fdt32_to_cpu (*(UINT32 *)Property);
    }

    PropertyStr = fdt_getprop (DtbBase, NodeOffset, "reference-designation", &Length);
    if (PropertyStr != NULL) {
      OnboardDeviceExInfo[NumOnboardDevices].ReferenceDesignation = AllocateZeroPool (Length + 1);
      AsciiSPrint (OnboardDeviceExInfo[NumOnboardDevices].ReferenceDesignation, Length + 1, PropertyStr);
    }

    VendorDeviceId = PciSegmentRead32 (
                       PCI_SEGMENT_LIB_ADDRESS (
                         SegmentNum,
                         BusNum,
                         (DevFuncNum >> 3) & 0x1F,
                         DevFuncNum & 0x7,
                         0
                         )
                       );
    if (VendorDeviceId != TYPE41_DEVICE_NOT_PRESENT) {
      OnboardDeviceExInfo[NumOnboardDevices].DeviceType = DeviceType | TYPE41_ONBOARD_DEVICE_ENABLED;
    } else {
      OnboardDeviceExInfo[NumOnboardDevices].DeviceType = DeviceType & ~TYPE41_ONBOARD_DEVICE_ENABLED;
    }

    OnboardDeviceExInfo[NumOnboardDevices].DeviceTypeInstance = GetOnboardDeviceInstance (
                                                                  DeviceType,
                                                                  &DeviceTypeInstances,
                                                                  &DeviceTypeInstanceCount
                                                                  );
    OnboardDeviceExInfo[NumOnboardDevices].SegmentGroupNum = SegmentNum;
    OnboardDeviceExInfo[NumOnboardDevices].BusNum          = BusNum;
    OnboardDeviceExInfo[NumOnboardDevices].DevFuncNum      = DevFuncNum;
    NumOnboardDevices++;
  }

  if (DeviceTypeInstances != NULL) {
    FreePool (DeviceTypeInstances);
  }

  DEBUG ((DEBUG_INFO, "%a: Number of onboard devices = %u\n", __FUNCTION__, NumOnboardDevices));
  if (NumOnboardDevices == 0) {
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < NumOnboardDevices; Index++) {
    OnboardDeviceExInfo[Index].CmObjectToken = REFERENCE_TOKEN (OnboardDeviceExInfo[Index]);
  }

  //
  // Add type 41 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType41,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;
  //
  // Install CM object for type 41
  //
  Repo->CmObjectId    = CREATE_CM_STD_OBJECT_ID (EStdObjOnboardDeviceExInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_STD_ONBOARD_DEVICE_EXTENDED_INFO) * NumOnboardDevices;
  Repo->CmObjectCount = NumOnboardDevices;
  Repo->CmObjectPtr   = OnboardDeviceExInfo;
  if ((UINTN)Repo < Private->RepoEnd) {
    Repo++;
  }

  ASSERT ((UINTN)Repo <= Private->RepoEnd);
  Private->Repo = Repo;

  return EFI_SUCCESS;
}
