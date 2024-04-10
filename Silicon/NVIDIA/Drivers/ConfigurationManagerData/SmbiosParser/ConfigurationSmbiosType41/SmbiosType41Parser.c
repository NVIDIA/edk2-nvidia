/** @file
  Configuration Manager Data of SMBIOS Type 41 table.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PciIo.h>
#include "SmbiosParserPrivate.h"
#include "SmbiosType41Parser.h"

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
  @param [in]  ParserHandle A handle to the parser instance.
  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType41Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  VOID                                    *DtbBase = Private->DtbBase;
  CM_SMBIOS_ONBOARD_DEVICE_EXTENDED_INFO  *OnboardDeviceExInfo;
  EFI_STATUS                              Status;
  CONST VOID                              *Property;
  CONST CHAR8                             *PropertyStr;
  INT32                                   Length;
  UINTN                                   Index;
  UINTN                                   Index2;
  UINTN                                   NumOnboardDevices;
  INT32                                   NodeOffset;
  UINT8                                   DeviceType;
  CHAR8                                   Type41NodeStr[] = "/firmware/smbios/type41@xx";
  DEVICE_TYPE_INSTANCE                    *DeviceTypeInstances;
  UINTN                                   DeviceTypeInstanceCount;
  UINT32                                  VendorDeviceId;
  UINT32                                  SegmentNum;
  UINT32                                  BusNum;
  UINT32                                  DevFuncNum;
  UINTN                                   Segment;
  UINTN                                   Bus;
  UINTN                                   Device;
  UINTN                                   Function;
  EFI_PCI_IO_PROTOCOL                     *PciIo;
  UINTN                                   HandleCount;
  EFI_HANDLE                              *HandleBuf;
  CM_OBJ_DESCRIPTOR                       Desc;
  CM_OBJECT_TOKEN                         *TokenMap;

  TokenMap                = NULL;
  NumOnboardDevices       = 0;
  OnboardDeviceExInfo     = NULL;
  DeviceTypeInstances     = NULL;
  DeviceTypeInstanceCount = 0;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuf
                  );
  if (EFI_ERROR (Status)) {
    HandleCount = 0;
  }

  for (Index = 0; Index < MAX_TYPE41_COUNT; Index++) {
    ZeroMem (Type41NodeStr, sizeof (Type41NodeStr));
    AsciiSPrint (Type41NodeStr, sizeof (Type41NodeStr), "/firmware/smbios/type41@%u", Index);
    NodeOffset = fdt_path_offset (DtbBase, Type41NodeStr);
    if (NodeOffset < 0) {
      break;
    }

    OnboardDeviceExInfo = ReallocatePool (
                            sizeof (CM_SMBIOS_ONBOARD_DEVICE_EXTENDED_INFO) * (NumOnboardDevices),
                            sizeof (CM_SMBIOS_ONBOARD_DEVICE_EXTENDED_INFO) * (NumOnboardDevices + 1),
                            OnboardDeviceExInfo
                            );
    if (OnboardDeviceExInfo == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto CleanupAndReturn;
    }

    SegmentNum                                                  = 0;
    BusNum                                                      = 0;
    DevFuncNum                                                  = 0;
    DeviceType                                                  = 0;
    VendorDeviceId                                              = MAX_UINT32;
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
      if (OnboardDeviceExInfo[NumOnboardDevices].ReferenceDesignation != NULL) {
        AsciiSPrint (OnboardDeviceExInfo[NumOnboardDevices].ReferenceDesignation, Length + 1, PropertyStr);
      }
    }

    OnboardDeviceExInfo[NumOnboardDevices].DeviceTypeInstance = GetOnboardDeviceInstance (
                                                                  DeviceType,
                                                                  &DeviceTypeInstances,
                                                                  &DeviceTypeInstanceCount
                                                                  );
    OnboardDeviceExInfo[NumOnboardDevices].SegmentGroupNum = SegmentNum;
    OnboardDeviceExInfo[NumOnboardDevices].BusNum          = BusNum;
    OnboardDeviceExInfo[NumOnboardDevices].DevFuncNum      = DevFuncNum;

    OnboardDeviceExInfo[NumOnboardDevices].DeviceType = DeviceType & ~TYPE41_ONBOARD_DEVICE_ENABLED;

    for (Index2 = 0; Index2 < HandleCount; Index2++) {
      Status = gBS->HandleProtocol (
                      HandleBuf[Index2],
                      &gEfiPciIoProtocolGuid,
                      (VOID **)&PciIo
                      );
      if (!EFI_ERROR (Status)) {
        Segment  = 0;
        Bus      = 0;
        Device   = 0;
        Function = 0;
        Status   = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
        if (!EFI_ERROR (Status) &&
            (OnboardDeviceExInfo[NumOnboardDevices].SegmentGroupNum == Segment) &&
            (OnboardDeviceExInfo[NumOnboardDevices].BusNum == Bus) &&
            (OnboardDeviceExInfo[NumOnboardDevices].DevFuncNum == (UINT8)((Device << 3) | Function)))
        {
          Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, 0x00, 1, &VendorDeviceId);
          if (!EFI_ERROR (Status) && (VendorDeviceId != 0xFFFF)) {
            OnboardDeviceExInfo[NumOnboardDevices].DeviceType = DeviceType | TYPE41_ONBOARD_DEVICE_ENABLED;
          }
        }
      }
    }

    NumOnboardDevices++;
  }

  FREE_NON_NULL (DeviceTypeInstances);

  DEBUG ((DEBUG_INFO, "%a: Number of onboard devices = %u\n", __FUNCTION__, NumOnboardDevices));
  if (NumOnboardDevices == 0) {
    Status = EFI_SUCCESS;
    goto CleanupAndReturn;
  }

  // Allocate Token Map
  Status = NvAllocateCmTokens (ParserHandle, NumOnboardDevices, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 41: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < NumOnboardDevices; Index++) {
    OnboardDeviceExInfo[Index].CmObjectToken = TokenMap[Index];
  }

  //
  // Install CM object for type 41
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjOnboardDeviceExInfo);
  Desc.Size     = sizeof (CM_SMBIOS_ONBOARD_DEVICE_EXTENDED_INFO) * NumOnboardDevices;
  Desc.Count    = NumOnboardDevices;
  Desc.Data     = OnboardDeviceExInfo;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 41 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
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

CleanupAndReturn:
  FREE_NON_NULL (TokenMap);
  FREE_NON_NULL (OnboardDeviceExInfo);
  return Status;
}
