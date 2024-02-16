/** @file

  Configuration Manager Data Driver private structures

  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CONFIGURATION_MANAGER_DATA_PRIVATE_H__
#define __CONFIGURATION_MANAGER_DATA_PRIVATE_H__

/**
  Initialize the data structure based on the data populated from the device tree
  and install the IORT nodes in the module private structure

  @return EFI_SUCCESS       Successful initialization of the module private structure
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
InitializeIoRemappingNodes (
  VOID
  );

/**
  Install the populated IORT nodes to Configuration Manager Data driver

  @param[in, out] PlatformRepositoryInfo       Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd    End address of the Platform Repository
  @param[in]      NVIDIAPlatformRepositoryInfo Available Nvidia Platform Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
InstallIoRemappingTable (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  );

#endif
