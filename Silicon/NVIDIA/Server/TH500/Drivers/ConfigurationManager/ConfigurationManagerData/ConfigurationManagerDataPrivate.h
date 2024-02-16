/** @file

  Configuration Manager Data Driver private structures

  Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.

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

  @param[in] PlatformRepositoryInfo      Pointer to the full Platform Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallHeterogeneousMemoryAttributeTable (
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepositoryInfo
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
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepositoryInfo
  );

/**
  Install the APMT table to Configuration Manager Data driver

  @param[in] PlatformRepositoryInfo      Pointer to the full Platform Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallArmPerformanceMonitoringUnitTable (
  IN EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepositoryInfo
  );

/**
  Check if GPU is enabled on given Socket ID

  @param[in] UINT32      SocketId

  @return    BOOLEAN     (TRUE if enabled
                          FALSE if disabled)

**/
BOOLEAN
EFIAPI
IsGpuEnabledOnSocket (
  UINTN  SocketId
  );

/**
  Check if a given HBM domain is enabled or not

  @param[in] UINT32      DmnIdx

  @return    BOOLEAN     (TRUE if enabled
                          FALSE if disabled)

**/
BOOLEAN
EFIAPI
IsHbmDmnEnabled (
  UINT32  DmnIdx
  );

/**
  Generate a bit map to indicate enabled HBM memory segments

  @param[in] VOID

**/
EFI_STATUS
EFIAPI
GenerateHbmMemPxmDmnMap (
  VOID
  );

/**
  Obtain the total number of proximity domains
  This includes, CPU, GPU and GPU HBM domains

  @param[in] VOID

**/
UINT32
EFIAPI
GetMaxPxmDomains (
  VOID
  );

/**
  Obtain the max number of HBM memory proximity domains

  @param[in] VOID

**/
UINT32
EFIAPI
GetMaxHbmPxmDomains (
  VOID
  );

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
  );

/**
  Install the SPMI table to Configuration Manager Data driver

  @param[in, out] PlatformRepositoryInfo      Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd   End address of the Platform Repository
  @param[in, out] PlatformRepositoryInfo      Pointer to the ACPI Table Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallServiceProcessorManagementInterfaceTable (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  );

/**
  Install the TPM2 table to Configuration Manager Data driver

  @param[in, out] PlatformRepositoryInfo      Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd   End address of the Platform Repository
  @param[in, out] PlatformRepositoryInfo      Pointer to the ACPI Table Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallTrustedComputingPlatform2Table (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
  );

#endif
