/** @file
  FRU spec definitions from the Platform Management FRU Information Storage
  definition V 1.0, Revision 1.2.

  Copyright (c) 2022 - 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2003 - 2022 Sun Microsystems, Inc.  All Rights Reserved.
  This file defines the various areas in the FRU and their common format.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#define SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR  0x11
#define FRU_END_OF_FIELDS                   0xC1
#define MAXIMUM_BYTES_TO_STRING_SIZE        0x64
#define MAX_VALUE_CHASSIS_TYPE              0x24
#define MAX_FRU_DESC_SIZE                   0x10
#define END_OF_SDR_RECORDS                  0xFFFF
#define PREDEFINED_FIELD                    0x01
#define CUSTOM_FIELD                        0x02

#define MAX_FRU_SIZE  0x1000

#define IPMI_MULTI_RECORD_HEADER_RESPONSE_SIZE   \
  (sizeof (IPMI_READ_FRU_DATA_RESPONSE) + sizeof (FRU_MULTI_RECORD_HEADER))

/* IPMI FRU Information Storage Definition v1.0 rev 1.3, Table 11-1 */

typedef union {
  UINT32    Bits;
  CHAR8     Chars[4];
} SIX_BIT_ASCII_DATA;

typedef struct {
  UINT8    Version;
  union {
    struct {
      UINT8    Internal;
      UINT8    Chassis;
      UINT8    Board;
      UINT8    Product;
      UINT8    Multi;
    } Offset;
    UINT8    Offsets[5];
  };

  UINT8    Pad;
  UINT8    Checksum;
} FRU_HEADER;

typedef struct {
  UINT8     AreaVersion;
  UINT8     Type;
  UINT16    AreaLength;
  CHAR8     *PartNum;
  CHAR8     *SerialNum;
} FRU_CHASSIS_AREA;

typedef struct {
  UINT8     AreaVersion;
  UINT8     LanguageCode;
  UINT16    AreaLength;
  UINT32    ManufactureDateTime;
  CHAR8     *Manufacturer;
  CHAR8     *ProductName;
  CHAR8     *SerialNum;
  CHAR8     *PartNum;
  CHAR8     *FruId;
} FRU_BOARD_AREA;

typedef struct {
  UINT8     AreaVersion;
  UINT8     LanguageCode;
  UINT16    AreaLength;
  CHAR8     *Manufacturer;
  CHAR8     *ProductName;
  CHAR8     *PartNum;
  CHAR8     *Version;
  CHAR8     *SerialNum;
  CHAR8     *AssetTag;
  CHAR8     *FruId;
} FRU_PRODUCT_AREA;

typedef enum {
  CHASSIS_AREA,
  BOARD_AREA,
  PRODUCT_AREA
} AREA_TYPE;
