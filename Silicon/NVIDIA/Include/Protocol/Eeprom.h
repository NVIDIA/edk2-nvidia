/** @file
  NVIDIA EEPROM Protocol

  Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2019-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019-2022 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __NVIDIA_EEPROM_PROTOCOL_H__
#define __NVIDIA_EEPROM_PROTOCOL_H__

#include <NVIDIABoardConfiguration.h>
#include <Library/NetLib.h>

#define T194_EEPROM_VERSION             1
#define T234_EEPROM_VERSION             2
#define EEPROM_CUSTOMER_BLOCK_SIGNATURE "NVCB"
#define EEPROM_CUSTOMER_TYPE_SIGNATURE  "M1"

#define CAMERA_EEPROM_PART_OFFSET       21
#define CAMERA_EEPROM_PART_NAME         "LPRD"

/**
 * @brief The Product Part Number structure that is embedded into
 * EEPROM layout structure
 *
 * @param Leading - 699 or 600 sticker info
 * @param Separator0 - separator, always '-'
 * @param Class - Board Class, always 8 for mobile
 * @param Id - Board ID (Quill Product 3310)
 * @param Separator1 - separator, always '-'
 * @param Sku - Board SKU
 * @param Separator2 - separator, always '-'
 * @param Fab - FAB value, Eg: 100, 200, 300
 * @param Separator3 - separator, space ' '
 * @param Revision - Manufacturing Major revision
 * @param Separator4 - separator, always '.'
 * @param Ending - Always "0"
 * @param Pad - 0x00
 */
#pragma pack(1)
typedef struct {       /* 20 - 49 */
  UINT8  Leading[3];   /* 20 */
  UINT8  Separator0;   /* 23 */
  UINT8  Class;        /* 24 */
  UINT8  Id[4];        /* 25 */
  UINT8  Separator1;   /* 29 */
  UINT8  Sku[4];       /* 30 */
  UINT8  Separator2;   /* 34 */
  UINT8  Fab[3];       /* 35 */
  UINT8  Separator3;   /* 38 */
  UINT8  Revision;     /* 39 */
  UINT8  Separator4;   /* 40 */
  UINT8  Ending;       /* 41 */
  UINT8  Pad[8];       /* 42 */
} TEGRA_EEPROM_PART_NUMBER;
#pragma pack()

/**
 * @brief The layout of data in T194 EEPROMS
 *
 * @param Version - Version of Board ID contents
 * @param Size - Size of Board ID data that follows this address
 * @param BoardNumber - ID of the board on which EEPROM in mounted
 * @param Sku - Always matches Board SKU on sticker
 * @param Fab - fabrication ID of the Board
 * @param Revision - revision of the Board
 * @param MinorRevision - Minor revision
 * @param MemoryType - Memory type
 * @param PowerConfig - Power cfgs like PM Stuff, DC-DC, VF, Max * Curr Limits
 * @param MiscConfig - Defines spl reworks, mech. Changes. Its a bitwise field
 * @param ModemConfig - Modem, eg: Icera Modem fuse/unfuse, Antenna bands
 * @param TouchConfig - Reworks related to touch
 * @param DisplayConfig - Reflects any spl reworks/changes related to Display
 * @param ReworkLevel - Syseng Rework Level
 * @param Reserved0 - Reserved bytes
 * @param PartNumber - asset_tracker_field_1 - 699 or 600 BOM Number
 * @param WifiMacAddress - MAC address for primary wifi chip
 * @param BtMacAddress - MAC address for bluetooth chip
 * @param SecWifiMacAddress - MAC address for secondary wifi chip
 * @param EthernetMacAddress - MAC address for ethernet port
 * @param SerialNumber - asset_field_tracker_2 - Serial number on sticker
 * @param Reserved1 - Reserved bytes
 * @param CustomerBlockSignature - 'NVCB' - NV Config Block
 * @param CustomerBlockLength - Length from Block Signature to end of EEPROM
 * @param CustomerTypeSignature - 'M1' - MAC Address Struc Type 1
 * @param CustomerVersion - 0x0000
 * @param CustomerWifiMacAddress - Customer usable field
 * @param CustomerBtMacAddress - Customer usable field
 * @param CustomerEthernetMacAddress - Customer usable field
 * @param Reserved2 - Reserved for future use
 * @param Checksum - CRC-8 computed for bytes 0 through 254
 */
#pragma pack(1)
typedef struct {
  UINT16   Version;                       /* 00 */
  UINT16   Size;                          /* 02 */
  UINT16   BoardNumber;                   /* 04 */
  UINT16   Sku;                           /* 06 */
  UINT8    Fab;                           /* 08 */
  UINT8    Revision;                      /* 09 */
  UINT8    MinorRevision;                 /* 10 */
  UINT8    MemoryType;                    /* 11 */
  UINT8    PowerConfig;                   /* 12 */
  UINT8    MiscConfig;                    /* 13 */
  UINT8    ModemConfig;                   /* 14 */
  UINT8    TouchConfig;                   /* 15 */
  UINT8    DisplayConfig;                 /* 16 */
  UINT8    ReworkLevel;                   /* 17 */
  UINT8    Reserved0[2];                  /* 18 */
  TEGRA_EEPROM_PART_NUMBER PartNumber;    /* 20 - 49 */
  UINT8    WifiMacAddress[6];             /* 50 */
  UINT8    BtMacAddress[6];               /* 56 */
  UINT8    SecWifiMacAddress[6];          /* 62 */
  UINT8    EthernetMacAddress[6];         /* 68 */
  UINT8    SerialNumber[15];              /* 74 */
  UINT8    Reserved1[61];                 /* 89 */
  UINT8    CustomerBlockSignature[4];     /* 150 */
  UINT16   CustomerBlockLength;           /* 154 */
  UINT8    CustomerTypeSignature[2];      /* 156 */
  UINT16   CustomerVersion;               /* 158 */
  UINT8    CustomerWifiMacAddress[6];     /* 160 */
  UINT8    CustomerBtMacAddress[6];       /* 166 */
  UINT8    CustomerEthernetMacAddress[6]; /* 172 */
  UINT8    Reserved2[77];                 /* 178 */
  UINT8    Checksum;                      /* 255 */
} T194_EEPROM_DATA;
#pragma pack()

/**
 * @brief The layout of data in T234 EEPROMS
 *
 * @param Version - Version of Board ID contents
 * @param Size - Size of Board ID data that follows this address
 * @param BoardNumber - ID of the board on which EEPROM in mounted
 * @param Sku - Always matches Board SKU on sticker
 * @param Fab - fabrication ID of the Board
 * @param Revision - revision of the Board
 * @param MinorRevision - Minor revision
 * @param MemoryType - Memory type
 * @param PowerConfig - Power cfgs like PM Stuff, DC-DC, VF, Max * Curr Limits
 * @param MiscConfig - Defines spl reworks, mech. Changes. Its a bitwise field
 * @param ModemConfig - Modem, eg: Icera Modem fuse/unfuse, Antenna bands
 * @param TouchConfig - Reworks related to touch
 * @param DisplayConfig - Reflects any spl reworks/changes related to Display
 * @param ReworkLevel - Syseng Rework Level
 * @param Reserved0 - Reserved byte
 * @param NumEthernetMacs - Number of ethernet mac addresses
 * @param PartNumber - asset_tracker_field_1 - 699 or 600 BOM Number
 * @param WifiMacAddress - MAC address for primary wifi chip
 * @param BtMacAddress - MAC address for bluetooth chip
 * @param SecWifiMacAddress - MAC address for secondary wifi chip
 * @param EthernetMacAddress - MAC address for ethernet port
 * @param SerialNumber - asset_field_tracker_2 - Serial number on sticker
 * @param Reserved1 - Reserved bytes
 * @param CustomerBlockSignature - 'NVCB' - NV Config Block
 * @param CustomerBlockLength - Length from Block Signature to end of EEPROM
 * @param CustomerTypeSignature - 'M1' - MAC Address Struc Type 1
 * @param CustomerVersion - 0x0000
 * @param CustomerWifiMacAddress - Customer usable field
 * @param CustomerBtMacAddress - Customer usable field
 * @param CustomerEthernetMacAddress - Customer usable field
 * @param CustomerNumEthernetMacs - Customer usable field
 * @param Reserved2 - Reserved for future use
 * @param Checksum - CRC-8 computed for bytes 0 through 254
 */
#pragma pack(1)
typedef struct {
  UINT16   Version;                       /* 00 */
  UINT16   Size;                          /* 02 */
  UINT16   BoardNumber;                   /* 04 */
  UINT16   Sku;                           /* 06 */
  UINT8    Fab;                           /* 08 */
  UINT8    Revision;                      /* 09 */
  UINT8    MinorRevision;                 /* 10 */
  UINT8    MemoryType;                    /* 11 */
  UINT8    PowerConfig;                   /* 12 */
  UINT8    MiscConfig;                    /* 13 */
  UINT8    ModemConfig;                   /* 14 */
  UINT8    TouchConfig;                   /* 15 */
  UINT8    DisplayConfig;                 /* 16 */
  UINT8    ReworkLevel;                   /* 17 */
  UINT8    Reserved0;                     /* 18 */
  UINT8    NumEthernetMacs;               /* 19 */
  TEGRA_EEPROM_PART_NUMBER PartNumber;    /* 20 - 49 */
  UINT8    WifiMacAddress[6];             /* 50 */
  UINT8    BtMacAddress[6];               /* 56 */
  UINT8    SecWifiMacAddress[6];          /* 62 */
  UINT8    EthernetMacAddress[6];         /* 68 */
  UINT8    SerialNumber[15];              /* 74 */
  UINT8    Reserved1[61];                 /* 89 */
  UINT8    CustomerBlockSignature[4];     /* 150 */
  UINT16   CustomerBlockLength;           /* 154 */
  UINT8    CustomerTypeSignature[2];      /* 156 */
  UINT16   CustomerVersion;               /* 158 */
  UINT8    CustomerWifiMacAddress[6];     /* 160 */
  UINT8    CustomerBtMacAddress[6];       /* 166 */
  UINT8    CustomerEthernetMacAddress[6]; /* 172 */
  UINT8    CustomerNumEthernetMacs;       /* 178 */
  UINT8    Reserved2[76];                 /* 179 */
  UINT8    Checksum;                      /* 255 */
} T234_EEPROM_DATA;
#pragma pack()

typedef struct {
  CHAR8    BoardId[BOARD_ID_LEN + 1];
  CHAR8    ProductId[PRODUCT_ID_LEN + 1];
  CHAR8    SerialNumber[SERIAL_NUM_LEN];
  UINT8    MacAddr[NET_ETHER_ADDR_LEN];
  UINT8    NumMacs;
} TEGRA_EEPROM_BOARD_INFO;

#endif
