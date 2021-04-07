/** @file
  Platform To Driver Configuration Protocol

  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <libfdt.h>
#include <PlatformToDriverConfigurationPrivate.h>
#include <PlatformToDriverStructures.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

BOOLEAN IsResponseNeeded;

VOID
EFIAPI
OnExitBootServices (
  IN      EFI_EVENT    Event,
  IN      VOID         *Context
  )
{
  gBS->CloseEvent (Event);

  // Check for Query Response pair
  ASSERT (!IsResponseNeeded);

  return;
}

/**

  SDMMC Driver specific Response function

**/
EFI_STATUS
EFIAPI
ResponseSDMMCParameters (
  IN CONST  VOID                              *ParameterBlock,
  IN CONST  EFI_PLATFORM_CONFIGURATION_ACTION ConfigurationAction
)
{
  if ( ParameterBlock == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  switch (ConfigurationAction) {
  case EfiPlatformConfigurationActionNone:
  case EfiPlatformConfigurationActionUnsupportedGuid:
    FreePool ((VOID*)ParameterBlock);
    return EFI_SUCCESS;

  case EfiPlatformConfigurationActionStopController:
  case EfiPlatformConfigurationActionRestartController:
  case EfiPlatformConfigurationActionRestartPlatform:
  case EfiPlatformConfigurationActionNvramFailed:
    DEBUG ((DEBUG_ERROR, "Handling not supported for ConfigurationAction %a\r\n",ConfigurationAction));
    return EFI_INVALID_PARAMETER;

  default:
    return EFI_SUCCESS;
  }
}

/**

  SDMMC driver specific Query function

  @param ParameterTypeGuid      An EFI_GUID that defines the contents
                                of ParameterBlock.
  @param ParameterBlock         ParameterBlock structure which
                                contains details about the
                                configuration parameters specific to
                                the ParameterTypeGuid.
  @param ParameterBlockSize     Size of ParameterBlock
  @param DtNode                 Device Tree Node used to extract
                                the parameter information
  @retval EFI_SUCCESS           The platform return parameter
                                information for ControllerHandle.

**/
EFI_STATUS
EFIAPI
QuerySDMMCParameters (
  IN OUT   VOID                                   **ParameterBlock,
  IN OUT   UINTN                                  *ParameterBlockSize,
  IN       CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DtNode
)
{
  SDMMC_PARAMETER_INFO      *SdmmcParameterInfo;
  CONST UINT32              *RegulatorPointer;

  if ((ParameterBlock == NULL) ||
      (ParameterBlockSize == NULL)||
      (DtNode == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *ParameterBlockSize = sizeof(SDMMC_PARAMETER_INFO);

  *ParameterBlock = (VOID *)AllocateZeroPool(*ParameterBlockSize);

  if (*ParameterBlock == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  SdmmcParameterInfo = (SDMMC_PARAMETER_INFO*)*ParameterBlock;

  //Obtaining SDMMC parameters from the DT
  RegulatorPointer = (CONST UINT32 *)fdt_getprop (DtNode->DeviceTreeBase,
                                                  DtNode->NodeOffset,
                                                  "vmmc-supply",
                                                  NULL);
  if (NULL != RegulatorPointer) {
    SdmmcParameterInfo->VmmcRegulatorIdPresent = TRUE;
    SdmmcParameterInfo->VmmcRegulatorId = SwapBytes32 (*RegulatorPointer);
  }

  RegulatorPointer = (CONST UINT32 *)fdt_getprop (DtNode->DeviceTreeBase,
                                                  DtNode->NodeOffset,
                                                  "vqmmc-supply",
                                                  NULL);
  if (NULL != RegulatorPointer) {
    SdmmcParameterInfo->VqmmcRegulatorIdPresent = TRUE;
    SdmmcParameterInfo->VqmmcRegulatorId = SwapBytes32 (*RegulatorPointer);
  }

  if (fdt_get_property (DtNode->DeviceTreeBase, DtNode->NodeOffset, "non-removable", NULL) != NULL) {
    SdmmcParameterInfo->NonRemovable = TRUE;
  }

  if (fdt_getprop (DtNode->DeviceTreeBase, DtNode->NodeOffset, "only1v8", NULL) != NULL) {
    SdmmcParameterInfo->Only1v8 = TRUE;
  }

  return EFI_SUCCESS;
}

// Create a mapping between the Driver GUID and the function pointer that extracts the DT info
GUID_DEVICEFUNCPTR_MAPPING GuidDeviceFuncPtrMap[] = {
  { &gEdkiiNonDiscoverableSdhciDeviceGuid, QuerySDMMCParameters, ResponseSDMMCParameters },
  { NULL, NULL, NULL }
};

EFI_STATUS
EFIAPI
Query (
  IN CONST  EFI_PLATFORM_TO_DRIVER_CONFIGURATION_PROTOCOL *This,
  IN CONST  EFI_HANDLE                                    ControllerHandle,
  IN CONST  EFI_HANDLE                                    ChildHandle OPTIONAL,
  IN CONST  UINTN                                         *Instance,
  OUT       EFI_GUID                                      **ParameterTypeGuid,
  OUT       VOID                                          **ParameterBlock,
  OUT       UINTN                                         *ParameterBlockSize
)
{
  EFI_STATUS                        Status;
  NON_DISCOVERABLE_DEVICE           *Device;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DtNode;
  GUID_DEVICEFUNCPTR_MAPPING        *GuidMapper;

  if (IsResponseNeeded) {
    DEBUG ((DEBUG_ERROR, "Cannot call another Query. Previous Query needs Response!\r\n"));
    return EFI_OUT_OF_RESOURCES;
  }
  DtNode = NULL;
  GuidMapper = GuidDeviceFuncPtrMap;

  if((ControllerHandle == NULL) ||
     (Instance == NULL) ||
     (ParameterTypeGuid == NULL) ||
     (ParameterBlock == NULL) ||
     (ParameterBlockSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // Extract the device GUID and DT info from ControllerHandle
  Status = gBS->HandleProtocol (ControllerHandle,
                                &gNVIDIANonDiscoverableDeviceProtocolGuid,
                                (VOID **)&Device
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get non discoverable protocol\r\n"));
    return Status;
  }

  Status = gBS->HandleProtocol (ControllerHandle,
                                &gNVIDIADeviceTreeNodeProtocolGuid,
                                (VOID **)&DtNode
                                );
  if ((EFI_ERROR (Status)) || (DtNode == NULL)) {
    DEBUG ((DEBUG_ERROR, "Failed to get device tree node information\r\n"));
    return Status;
  }

  // Iterate over the list of known clients
  while (GuidMapper->DeviceGuid != NULL) {
    if (CompareGuid (Device->Type, GuidMapper->DeviceGuid)) {
      break;
    }
    GuidMapper++;
  }

  if (GuidMapper->DeviceGuid != NULL) {
    Status = GuidMapper->Query(ParameterBlock, ParameterBlockSize, DtNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Query Function failed to return DT properties\r\n"));
      return Status;
    }
    *ParameterTypeGuid = GuidMapper->DeviceGuid;
    IsResponseNeeded = TRUE;
  }
  else {
    Status = EFI_DEVICE_ERROR;
  }
  return Status;
}

EFI_STATUS
EFIAPI
Response (
  IN CONST  EFI_PLATFORM_TO_DRIVER_CONFIGURATION_PROTOCOL *This,
  IN CONST  EFI_HANDLE                        ControllerHandle,
  IN CONST  EFI_HANDLE                        ChildHandle OPTIONAL,
  IN CONST  UINTN                             *Instance,
  IN CONST  EFI_GUID                          *ParameterTypeGuid,
  IN CONST  VOID                              *ParameterBlock,
  IN CONST  UINTN                             ParameterBlockSize,
  IN CONST  EFI_PLATFORM_CONFIGURATION_ACTION ConfigurationAction
)
{
  EFI_STATUS                        Status;
  GUID_DEVICEFUNCPTR_MAPPING        *GuidMapper;

  if (!IsResponseNeeded) {
    DEBUG ((DEBUG_ERROR, "Response already sent. Cannot send another one!\r\n"));
    return EFI_DEVICE_ERROR;
  }

  GuidMapper = GuidDeviceFuncPtrMap;

  if((ControllerHandle == NULL) ||
     (Instance == NULL) ||
     (ParameterTypeGuid == NULL) ||
     (ParameterBlock == NULL) ||
     (ParameterBlockSize == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  // Iterate over the list of known clients
  while (GuidMapper->DeviceGuid != NULL) {
    if (CompareGuid (ParameterTypeGuid, GuidMapper->DeviceGuid)) {
      break;
    }
    GuidMapper++;
  }

  if (GuidMapper->DeviceGuid != NULL) {
    Status = GuidMapper->Response (ParameterBlock, ConfigurationAction);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Response Function failed\r\n"));
      return Status;
    }
    IsResponseNeeded = FALSE;
  }
  else {
    Status = EFI_DEVICE_ERROR;
  }

  return Status;
}

EFI_PLATFORM_TO_DRIVER_CONFIGURATION_PROTOCOL mPlatformToDriverInterface = {
  Query,
  Response
};

/**
  Entry point for Platform to Drive Configuration Protocol.
  The user code starts with this function.

  @param[in]  ImageHandle   The Driver image handle.
  @param[in]  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS       Install Boot manager menu success.
  @retval Other             Return error status.

**/

EFI_STATUS
EFIAPI
InitializePlatformToDriverConfigurationProtocol (
  IN EFI_HANDLE               ImageHandle,
  IN EFI_SYSTEM_TABLE         *SystemTable
)
{
  EFI_STATUS Status;
  EFI_EVENT  OnExitBootServiceEvent;

  // Check for pending response for query by creating an event
  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                               TPL_CALLBACK,
                               OnExitBootServices,
                               NULL,
                               &gEfiEventExitBootServicesGuid,
                               &OnExitBootServiceEvent
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to create event for query-response check upon exiting boot services \r\n"));
    return Status;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (&ImageHandle,
                                                   &gEfiPlatformToDriverConfigurationProtocolGuid,
                                                   &mPlatformToDriverInterface,
                                                   NULL
                                                   );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to install Platform To Driver Config Protocol (%r)\r\n", __FUNCTION__, Status));
  }

  return Status;
}
