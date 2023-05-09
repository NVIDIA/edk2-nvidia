/** @file

  Configuration Manager Data Driver private structures for SMBIOS tables

  Copyright (c) 2022 - 2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CONFIGURATION_SMBIOS_PRIVATE_H__
#define __CONFIGURATION_SMBIOS_PRIVATE_H__

#include <Library/FruLib.h>

#define MAX_SMBIOS_TABLE_TYPES_SUPPORTED       64
#define MAX_TYPE2_COUNT                        10
#define MAX_TYPE3_COUNT                        100
#define MAX_TYPE3_CONTAINED_ELEMENT_COUNT      100
#define MAX_TYPE41_COUNT                       100
#define TYPE41_DEVICE_NOT_PRESENT              0xFFFFFFFF
#define TYPE41_ONBOARD_DEVICE_ENABLED          0x80
#define MAX_TPM_VERSION_LEN                    14
#define MAX_FIRMWARE_INVENTORY_FMP_DESC_COUNT  100

//
// CM SMBIOS record population struct
//
typedef struct {
  UINT8              FruDeviceId;
  CM_OBJECT_TOKEN    ChassisCmToken;
} CM_ENCLOSURE_BASEBOARD_INFO;

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

  /// Enclosure and baseboard binding info
  struct {
    CM_ENCLOSURE_BASEBOARD_INFO    *Info;
    UINT8                          Count;
  } EnclosureBaseboardBinding;

  EDKII_PLATFORM_REPOSITORY_INFO    *PlatformRepositoryInfo;
} CM_SMBIOS_PRIVATE_DATA;

typedef EFI_STATUS
(EFIAPI *CM_INSTALL_SMBIOS_RECORD)(
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  );

//
// CM SMBIOS record population struct
//
typedef struct {
  SMBIOS_TYPE                 Type;
  CM_INSTALL_SMBIOS_RECORD    Function;
} CM_SMBIOS_RECORD_POPULATION;

/**
  Install the SMBIOS tables to Configuration Manager Data driver

  @param[in, out] PlatformRepositoryInfo      Pointer to the available Platform Repository
  @param[in]      PlatformRepositoryInfoEnd   End address of the Platform Repository
  @param[in]      PlatformRepositoryInfo      Pointer to the platform repository info

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallCmSmbiosTableList (
  IN OUT  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  IN      UINTN                           PlatformRepositoryInfoEnd,
  IN      EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo
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
  Find and get FRU extra string that has a certain prefix

  @param[in] FruExtra  Pointer to the array of FRU (chassis/board/product) extra
  @param[in] Prefix    FRU extra prefix to search for

  @return A pointer to an allocated string

**/
CHAR8 *
GetFruExtraStr (
  IN CHAR8        **FruExtra,
  IN CONST CHAR8  *Prefix
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
  Install CM object for SMBIOS Type 0

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType0Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
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
  Install CM object for SMBIOS Type 3

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType3Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  );

/**
  Install CM object for SMBIOS Type 8

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType8Cm (
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
  Install CM object for SMBIOS Type 13

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType13Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  );

/**
  Install CM object for SMBIOS Type 32

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType32Cm (
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
  Install CM object for SMBIOS Type 39

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors
**/
EFI_STATUS
EFIAPI
InstallSmbiosType39Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  );

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

/**
  Install CM object for SMBIOS Type Mem

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosTypeMemCm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  );

/**
  Install CM object for SMBIOS Type 45

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType45Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  );

#endif
