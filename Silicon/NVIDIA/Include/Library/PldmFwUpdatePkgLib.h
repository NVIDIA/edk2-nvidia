/** @file

  PLDM FW update package library

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PLDM_FW_UPDATE_PKG_H__
#define __PLDM_FW_UPDATE_PKG_H__

#include <Library/PldmFwUpdateLib.h>

#define PLDM_FW_PKG_FORMAT_REVISION_1  0x1
#define PLDM_FW_PKG_FORMAT_REVISION_2  0x2

#define PLDM_FW_PKG_UUID_V1_0    \
  {{0xF0, 0x18, 0x87, 0x8C, 0xCB, 0x7D, 0x49, 0x43, 0x98, 0x00, 0xA0, 0x2F, 0x05, 0x9A, 0xCA, 0x02}}
#define PLDM_FW_PKG_UUID_V1_1    \
  {{0x12, 0x44, 0xd2, 0x64, 0x8d, 0x7d, 0x47, 0x18, 0xa0, 0x30, 0xfc, 0x8a, 0x56, 0x58, 0x7d, 0x5a}}

#define PLDM_FW_PKG_COMPONENT_OPT_FORCE_UPDATE          BIT0
#define PLDM_FW_PKG_COMPONENT_OPT_USE_COMPARISON_STAMP  BIT1

#pragma pack(1)

typedef struct {
  UINT16    Length;
  UINT8     DescriptorCount;
  UINT32    UpdateOptionFlags;
  UINT8     ImageSetVersionStringType;
  UINT8     ImageSetVersionStringLength;
  UINT16    PackageDataLength;
  UINT8     ApplicableComponents[1];
  //        ComponentImageSetVersionString
  //        RecordDescriptors
  //        FirmwareDevicePackageData
} PLDM_FW_PKG_DEVICE_ID_RECORD;

typedef struct {
  UINT8                           RecordCount;
  PLDM_FW_PKG_DEVICE_ID_RECORD    Records[1];
} PLDM_FW_PKG_FW_DEVICE_ID_AREA;

typedef PLDM_FW_PKG_FW_DEVICE_ID_AREA PLDM_FW_PKG_DOWNSTREAM_DEVICE_ID_AREA;

typedef struct {
  UINT16    Classification;
  UINT16    Id;
  UINT32    ComparisonStamp;
  UINT16    Options;
  UINT16    RequestedActivationMethod;
  UINT32    LocationOffset;
  UINT32    Size;
  UINT8     VersionStringType;
  UINT8     VersionStringLength;
  UINT8     VersionString[1];
} PLDM_FW_PKG_COMPONENT_IMAGE_INFO;

typedef struct {
  UINT16                              ImageCount;
  PLDM_FW_PKG_COMPONENT_IMAGE_INFO    ImageInfo[1];
} PLDM_FW_PKG_COMPONENT_IMAGE_INFO_AREA;

typedef struct {
  PLDM_UUID            Identifier;
  UINT8                FormatRevision;
  UINT16               Size;
  PLDM_TIMESTAMP104    ReleaseDateTime;
  UINT16               ComponentBitmapBitLength;
  UINT8                VersionStringType;
  UINT8                VersionStringLength;
  UINT8                VersionString[1];
  //                   FirmwareDeviceIdArea
  //                   DownstreamDeviceIdArea
  //                   ComponentImageInfoArea
  //                   PackageHeaderChecksum
} PLDM_FW_PKG_HDR;

#pragma pack()

/**
  Get a pointer to the FW device id area.

  @param[in]  Hdr                           Pointer to package header.

  @retval PLDM_FW_PKG_FW_DEVICE_ID_AREA *   Pointer to FW device id area.

**/
CONST PLDM_FW_PKG_FW_DEVICE_ID_AREA *
EFIAPI
PldmFwPkgGetFwDeviceIdArea (
  IN CONST PLDM_FW_PKG_HDR  *Hdr
  );

/**
  Get a pointer to the downstream device id area.

  @param[in]  Hdr                                   Pointer to package header.

  @retval PLDM_FW_PKG_DOWNSTREAM_DEVICE_ID_AREA *   Pointer to downstream device
                                                    id area or NULL if not present.

**/
CONST PLDM_FW_PKG_DOWNSTREAM_DEVICE_ID_AREA *
EFIAPI
PldmFwPkgGetDownstreamDeviceIdArea (
  IN CONST PLDM_FW_PKG_HDR  *Hdr
  );

/**
  Get the device id area size.

  @param[in]  DeviceIdArea                          Pointer to device id area.

  @retval UINTN                                     Size of area in bytes.

**/
UINTN
EFIAPI
PldmFwPkgGetDeviceIdAreaSize (
  IN CONST PLDM_FW_PKG_FW_DEVICE_ID_AREA  *DeviceIdArea
  );

/**
  Get a pointer to the FW device image set version string.

  @param[in]  Hdr                       Pointer to package header.
  @param[in]  Record                    Pointer to device id record.

  @retval VOID *                        Pointer to image set version string.

**/
CONST VOID *
EFIAPI
PldmFwPkgGetDeviceIdRecordImageSetVersionString (
  IN CONST PLDM_FW_PKG_HDR               *Hdr,
  IN CONST PLDM_FW_PKG_DEVICE_ID_RECORD  *Record
  );

/**
  Get a pointer to the FW device descriptors

  @param[in]  Hdr                       Pointer to package header.
  @param[in]  Record                    Pointer to device id record.

  @retval PLDM_FW_DESCRIPTOR *          Pointer to descriptors.

**/
CONST PLDM_FW_DESCRIPTOR *
EFIAPI
PldmFwPkgGetFwDeviceIdRecordDescriptors (
  IN CONST PLDM_FW_PKG_HDR               *Hdr,
  IN CONST PLDM_FW_PKG_DEVICE_ID_RECORD  *Record
  );

/**
  Get a pointer to the next FW device id record.

  @param[in]  Record                        Pointer to device id record.

  @retval PLDM_FW_PKG_DEVICE_ID_RECORD *    Pointer to next descriptor.

**/
CONST PLDM_FW_PKG_DEVICE_ID_RECORD *
EFIAPI
PldmFwPkgGetNextDeviceIdRecord (
  CONST PLDM_FW_PKG_DEVICE_ID_RECORD  *Record
  );

/**
  Get a pointer to the component image info area.

  @param[in]  Hdr                                   Pointer to package header.

  @retval PLDM_FW_PKG_COMPONENT_IMAGE_INFO_AREA *   Pointer to area.

**/
CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO_AREA *
EFIAPI
PldmFwPkgGetComponentImageInfoArea (
  IN CONST PLDM_FW_PKG_HDR  *Hdr
  );

/**
  Get size of component image info area.

  @param[in]  ImageInfoArea             Pointer to component image info area.

  @retval UINTN                         Size of area in bytes.

**/
UINTN
EFIAPI
PldmFwPkgGetComponentImageInfoAreaSize (
  IN CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO_AREA  *ImageInfoArea
  );

/**
  Get size of component image info structure.

  @param[in]  ImageInfo                     Pointer to component image.

  @retval UINTN                             Size of component image structure.

**/
UINTN
EFIAPI
PldmFwPkgGetComponentImageInfoSize (
  IN CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO  *ImageInfo
  );

/**
  Get pointer to next component image.

  @param[in]  ImageInfo                         Pointer to component image.

  @retval PLDM_FW_PKG_COMPONENT_IMAGE_INFO *    Pointer to next component image.

**/
CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO *
EFIAPI
PldmFwPkgGetNextComponentImage (
  IN CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO  *ImageInfo
  );

/**
  Check if package matches any firmware device descriptor in list.

  @param[in]  Hdr                   Pointer to package header.
  @param[in]  DescriptorCount       Number of descriptors in list.
  @param[in]  FwDescriptors         Pointer to descriptor list.
  @param[out] FwDeviceRecord        Pointer to save matching FW device record.

  @retval BOOLEAN                   TRUE if match found and record returned.

**/
BOOLEAN
EFIAPI
PldmFwPkgMatchesFD (
  IN CONST PLDM_FW_PKG_HDR                *Hdr,
  IN UINTN                                DescriptorCount,
  IN CONST PLDM_FW_DESCRIPTOR             *FwDescriptors,
  OUT CONST PLDM_FW_PKG_DEVICE_ID_RECORD  **FwDeviceRecord
  );

/**
  Check if package is applicable to component.

  @param[in]  ComponentIndex            Index of component.
  @param[in]  Hdr                       Pointer to package header.
  @param[in]  Record                    Pointer to device id record.

  @retval BOOLEAN                       TRUE if component is applicable.

**/
BOOLEAN
EFIAPI
PldmFwPkgComponentIsApplicable (
  IN UINTN                               ComponentIndex,
  IN CONST PLDM_FW_PKG_HDR               *Hdr,
  IN CONST PLDM_FW_PKG_DEVICE_ID_RECORD  *Record
  );

/**
  Validate package header.

  @param[in]  Hdr                   Pointer to package header.
  @param[in]  Length                Length of package in bytes.

  @retval EFI_SUCCESS               No errors found.
  @retval Others                    Error detected.

**/
EFI_STATUS
EFIAPI
PldmFwPkgHdrValidate (
  IN CONST PLDM_FW_PKG_HDR  *Hdr,
  IN UINTN                  Length
  );

#endif
