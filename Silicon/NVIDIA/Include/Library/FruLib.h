/** @file

  FRU Library

  Copyright (c) 2022 - 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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

#define MAX_FRU_STR_LENGTH  16

#define MAX_FRU_MULTI_RECORDS  8

#define FRU_MULTI_RECORD_VERSION  0x02

#define FRU_MULTI_RECORD_TYPE_POWER_SUPPLY_INFO       0x00
#define FRU_MULTI_RECORD_TYPE_DC_OUTPUT               0x01
#define FRU_MULTI_RECORD_TYPE_DC_LOAD                 0x02
#define FRU_MULTI_RECORD_TYPE_MANAGEMENT_ACCESS       0x03
#define FRU_MULTI_RECORD_TYPE_BASE_COMPATIBILITY      0x04
#define FRU_MULTI_RECORD_TYPE_EXTENDED_COMPATIBILITY  0x05
#define FRU_MULTI_RECORD_TYPE_EXTENDED_DC_OUTPUT      0x09
#define FRU_MULTI_RECORD_TYPE_EXTENDED_DC_LOAD        0x0A

#pragma pack(1)

typedef struct {
  UINT8    Type;
  UINT8    Version   : 4;
  UINT8    Reserved  : 3;
  UINT8    EndOfList : 1;
  UINT8    Length;
  UINT8    RecordChecksum;
  UINT8    HeaderChecksum;
} FRU_MULTI_RECORD_HEADER;

typedef struct {
  UINT16    Capacity;
  UINT16    PeakVA;
  UINT8     InrushCurrent;
  UINT8     InrushInterval;
  UINT16    LowendInput1;
  UINT16    HighendInput1;
  UINT16    LowendInput2;
  UINT16    HighendInput2;
  UINT8     LowendFreq;
  UINT8     HighendFreq;
  UINT8     DropoutTolerance;
  UINT8     PredictiveFail   : 1;
  UINT8     PwrFactorCorr    : 1;
  UINT8     AutoSwitch       : 1;
  UINT8     HotSwap          : 1;
  UINT8     Tach             : 1;
  UINT8     Reserved         : 3;
  UINT16    PeakCapacity     : 12;
  UINT16    HoldupTime       : 4;
  UINT8     CombinedVoltage2 : 4;
  UINT8     CombinedVoltage1 : 4;
  UINT16    CombinedCapacity;
  UINT8     RpsThreshold;
} MULTI_RECORD_POWER_SUPPLY_INFO;

typedef struct {
  UINT8     OutputNumber : 4;
  UINT8     Reserved     : 3;
  UINT8     Standby      : 1;
  INT16     NominalVoltage;
  INT16     MaxNegDev;
  INT16     MaxPosDev;
  UINT16    RippleAndNoise;
  UINT16    MinCurrent;
  UINT16    MaxCurrent;
} MULTI_RECORD_DC_OUTPUT;

typedef struct {
  FRU_MULTI_RECORD_HEADER    Header;
  union {
    MULTI_RECORD_POWER_SUPPLY_INFO    PsuInfo;
    MULTI_RECORD_DC_OUTPUT            DcOutput;
    UINT8                             Data[1];
  };
} FRU_MULTI_RECORD;

#pragma pack()

/** A structure that defines the FRU Device Info for various planes/boards
 *
 *  The Board specific information for various boards is described by this structure
 *
**/
typedef struct FruDeviceInfo {
  UINT8               FruDeviceId;
  CHAR8               FruDeviceDescription[MAX_FRU_STR_LENGTH + 1];
  /// The enumeration value of the chassis type, refer SMBIOS Spec, Table 16 - System Enclosure or Chassis Types
  UINT8               ChassisType;
  CHAR8               *ChassisPartNum;
  CHAR8               *ChassisSerial;
  CHAR8               *ChassisExtra[MAX_EXTRA_FRU_AREA_ENTRIES];
  // Number of minutes from 0:00hrs 1/1/96.
  UINT32              ManufacturingDate;
  CHAR8               *BoardManufacturer;
  CHAR8               *BoardProduct;
  CHAR8               *BoardSerial;
  CHAR8               *BoardPartNum;
  CHAR8               *BoardExtra[MAX_EXTRA_FRU_AREA_ENTRIES];
  CHAR8               *ProductManufacturer;
  CHAR8               *ProductName;
  CHAR8               *ProductPartNum;
  CHAR8               *ProductVersion;
  CHAR8               *ProductSerial;
  CHAR8               *ProductAssetTag;
  CHAR8               *ProductExtra[MAX_EXTRA_FRU_AREA_ENTRIES];
  FRU_MULTI_RECORD    *MultiRecords[MAX_FRU_MULTI_RECORDS];
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
