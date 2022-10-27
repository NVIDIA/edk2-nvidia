/** @file

  Configuration Manager Data Driver private structures

  Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.

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

  @param[in, out] PlatformRepositoryInfo      Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd   End address of the Platform Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
InstallIoRemappingTable (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  );

/**
  Checks if MPAM nodes are enabled in the device tree

  @retval   TRUE or FALSE
 */
BOOLEAN
EFIAPI
IsMpamEnabled (
  VOID
  );

/**
  Install the populated MPAM Table and MSC nodes to Configuration Manager Data driver

  @param[in, out] PlatformRepositoryInfo      Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd   End address of the Platform Repository
  @param[in, out] PlatformRepositoryInfo      Pointer to the ACPI Table Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
InstallMpamTable (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  );

/**
  Install the HMAT nodes to Configuration Manager Data driver

  @param[in, out] PlatformRepositoryInfo      Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd   End address of the Platform Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallHeterogeneousMemoryAttributeTable (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  );

/**
  Install the SRAT nodes to Configuration Manager Data driver

  @param[in, out] PlatformRepositoryInfo      Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd   End address of the Platform Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallStaticResourceAffinityTable (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  );

/**
  Initialize the cache resources and proc hierarchy entries in the platform configuration repository.

  @param Repo Pointer to a repo structure that will be added to and updated with the data updated

  @retval EFI_SUCCESS   Success
**/
EFI_STATUS
EFIAPI
UpdateCpuInfo (
  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo
  );

/**
  Generate the CacheID using the pHandle for Cache Struct
**/
UINT32
EFIAPI
GetCacheIdFrompHandle (
  UINT32  pHandle
  );

/**
  Install the SLIT nodes to Configuration Manager Data driver

  @param[in, out] PlatformRepositoryInfo      Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd   End address of the Platform Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallStaticLocalityInformationTable (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  );

#endif
