/** @file

  Configuration Manager Data Driver private structures for SMBIOS tables

  Copyright (c) 2022 - 2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CONFIGURATION_SMBIOS_PRIVATE_H__
#define __CONFIGURATION_SMBIOS_PRIVATE_H__

#include <Library/FruLib.h>

#define MAX_SMBIOS_TABLE_TYPES_SUPPORTED  64

/** This structure contains data used by SMBIOS CM object creators */
typedef struct CmSmbiosPrivateData {
  /// List of SMBIOS Tables that will be installed (EStdObjSmbiosTableList)
  CM_STD_OBJ_SMBIOS_TABLE_INFO      CmSmbiosTableList[MAX_SMBIOS_TABLE_TYPES_SUPPORTED];

  /// Number of SMBIOS Tables that will be installed
  UINTN                             CmSmbiosTableCount;

  /// Pointer to the available Platform Repository
  EDKII_PLATFORM_REPOSITORY_INFO    *Repo;

  /// End address of the Platform Repository
  UINTN                             RepoEnd;

  /// Pointer to device tree
  VOID                              *DtbBase;

  /// Device tree size
  UINTN                             DtbSize;

  /// Offset to '/firmware/smbios' node
  INTN                              DtbSmbiosOffset;

  /// Pointer to FRU info array
  FRU_DEVICE_INFO                   **FruInfo;

  /// Number of FRUs in FRU info array
  UINT8                             FruCount;
} CM_SMBIOS_PRIVATE_DATA;

/**
  Install the SMBIOS tables to Configuration Manager Data driver

  @param[in, out] PlatformRepositoryInfo      Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd   End address of the Platform Repository

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallCmSmbiosTableList (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd
  );

/**
  Find FRU by FRU description

  @param[in] Private        Pointer to the private data of SMBIOS creators
  @param[in] FruDescPattern FRU description pattern to look for

  @return A pointer to the FRU record if found, or NULL if not found.

**/
FRU_DEVICE_INFO *
FindFruByDescription (
  IN  CM_SMBIOS_PRIVATE_DATA  *Private,
  IN  CHAR8                   *FruDescPattern
  );

/**
  Allocate and copy string

  @param[in] String     String to be copied

  @return A pointer to the copied string if succeeds, or NULL if fails.

**/
CHAR8 *
AllocateCopyString (
  IN  CHAR8  *String
  );

/**
  Install CM object for SMBIOS Type 1

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType1Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  );

/**
  Install CM object for SMBIOS Type 2

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType2Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  );

/**
  Install CM object for SMBIOS Type 9

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType9Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  );

/**
  Install CM object for SMBIOS Type 11

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType11Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  );

/**
  Install CM object for SMBIOS Type 38

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
EFIAPI
InstallSmbiosType38Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  );

/**
  Install CM object for SMBIOS Type 43

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType43Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  );

#endif
