/** @file

  FRU Library

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FRU_LIB__
#define __FRU_LIB__

#define MAX_NUMBER_OF_FRU_DEVICE_IDS  100
// There may or may not be some extra entries for each of the FRU areas
// Chassis Area, Board Area, Product Area. This is defined under the
// assumption that this value is sufficient to hold all the extra fru area
// entries.
#define MAX_EXTRA_FRU_AREA_ENTRIES  10

/** A structure that defines the FRU Device Info for various planes/boards
 *
 *  The Board specific information for various boards is described by this structure
 *
**/
typedef struct FruDeviceInfo {
  UINT8     FruDeviceId;
  CHAR8     FruDeviceDescription[16];
  /// The enumeration value of the chassis type, refer SMBIOS Spec, Table 16 - System Enclosure or Chassis Types
  UINT8     ChassisType;
  CHAR8     *ChassisPartNum;
  CHAR8     *ChassisSerial;
  CHAR8     *ChassisExtra[MAX_EXTRA_FRU_AREA_ENTRIES];
  // Number of minutes from 0:00hrs 1/1/96.
  UINT32    ManufacturingDate;
  CHAR8     *BoardManufacturer;
  CHAR8     *BoardProduct;
  CHAR8     *BoardSerial;
  CHAR8     *BoardPartNum;
  CHAR8     *BoardExtra[MAX_EXTRA_FRU_AREA_ENTRIES];
  CHAR8     *ProductManufacturer;
  CHAR8     *ProductName;
  CHAR8     *ProductPartNum;
  CHAR8     *ProductVersion;
  CHAR8     *ProductSerial;
  CHAR8     *ProductAssetTag;
  CHAR8     *ProductExtra[MAX_EXTRA_FRU_AREA_ENTRIES];
} FRU_DEVICE_INFO;

/**
  ReadAllFrus
  The Functions Calls the FRU reader functions to get the
  Platform information.

  @param  FruInfo     The pointer to the list of FRU records
  @param  FruCount    The pointer to the param that stores the total Fru Records read

  @retval EFI_SUCCESS          Returns if all the Frus are parsed and the FruDeviceInfo[] is updated.
          EFI_OUT_OF_RESOURCES Returns if dynamic memory allocation fails for any buffer.
          EFI_PROTOCOL_ERROR   Returns if Ipmi protocol error occurs.
**/
EFI_STATUS
ReadAllFrus (
  OUT FRU_DEVICE_INFO  ***FruInfo,
  OUT UINT8            *FruCount
  );

/**
 FreeAllFruRecords frees the memory for all the Fru Record buffers.

 @retval EFI_SUCCESS        always Returns EFI_SUCCESS
**/
EFI_STATUS
FreeAllFruRecords (
  VOID
  );

#endif
