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

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PlatformToDriverConfiguration.h>

EFI_STATUS
EFIAPI
Query(
  IN CONST  EFI_PLATFORM_TO_DRIVER_CONFIGURATION_PROTOCOL *This,
  IN CONST  EFI_HANDLE                                    ControllerHandle,
  IN CONST  EFI_HANDLE                                    ChildHandle OPTIONAL,
  IN CONST  UINTN                                         *Instance,
  OUT       EFI_GUID                                      **ParameterTypeGuid,
  OUT       VOID                                          **ParameterBlock,
  OUT       UINTN                                         *ParameterBlockSize
)
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
Response(
  IN CONST  EFI_PLATFORM_TO_DRIVER_CONFIGURATION_PROTOCOL *This,
  IN CONST  EFI_HANDLE                        ControllerHandle,
  IN CONST  EFI_HANDLE                        ChildHandle OPTIONAL,
  IN CONST  UINTN                             *Instance,
  IN CONST  EFI_GUID                          *ParameterTypeGuid,
  IN CONST  VOID                              *ParameterBlock,
  IN CONST  UINTN                             ParameterBlockSize ,
  IN CONST  EFI_PLATFORM_CONFIGURATION_ACTION ConfigurationAction
)
{
  return EFI_SUCCESS;
}

EFI_PLATFORM_TO_DRIVER_CONFIGURATION_PROTOCOL mPlatformToDriverInterface = {
  Query,
  Response
};

/**
  Entry point for Platform to Drive Configuration Protocol.
  The user code starts with this function.

  @param  ImageHandle    The Driver image handle.
  @param[in]  SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS    Install Boot manager menu success.
  @retval Other          Return error status.

**/

EFI_STATUS
EFIAPI
InitializePlatformToDriverConfigurationProtocol(
    IN EFI_HANDLE               ImageHandle,
    IN EFI_SYSTEM_TABLE         *SystemTable
 )
{
  EFI_STATUS Status;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiPlatformToDriverConfigurationProtocolGuid,
                  &mPlatformToDriverInterface,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to install Platform To Driver Config Protocol (%r)\r\n", __FUNCTION__, Status));
  }

  return Status;
}
