/** @file

  NctLib library

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NCT_LIB_H_
#define _NCT_LIB_H_

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>

#define MAX_TNSPEC_LEN    (SIZE_2MB)        /* @2MB */
#define TNS_MAGIC_ID      "TNS1"            /* 0x544E5331 */
#define TNS_MAGIC_ID_LEN  4

#define NCT_MAGIC_ID      "nVCt"  /* 0x6E564374 */
#define NCT_MAGIC_ID_LEN  4

#define NCT_FORMAT_VERSION  0x00010000   /* 0xABCDabcd (ABCD.abcd) */

#define NCT_ENTRY_OFFSET       0x4000
#define MAX_NCT_ENTRY          512
#define MAX_NCT_DATA_SIZE      1024
#define NCT_ENTRY_SIZE         1040
#define NCT_ENTRY_DATA_OFFSET  12

#define NCT_NUM_UUID_ENTRIES  1
#define NCT_UUID_ENTRY_SIZE   64
#define UUIDS_PER_NCT_ENTRY   (MAX_NCT_DATA_SIZE / NCT_UUID_ENTRY_SIZE)

#define NCT_MAX_SPEC_LENGTH  64             /* SW spec max length */

/* macro nct tag */
typedef UINT32 NCT_TAG;
#define NCT_TAG_1B_SINGLE   0x10
#define NCT_TAG_2B_SINGLE   0x20
#define NCT_TAG_4B_SINGLE   0x40
#define NCT_TAG_STR_SINGLE  0x80
#define NCT_TAG_1B_ARRAY    0x1A
#define NCT_TAG_2B_ARRAY    0x2A
#define NCT_TAG_4B_ARRAY    0x4A
#define NCT_TAG_STR_ARRAY   0x8A

/* macro nct id */
typedef UINT32 NCT_ID;
#define NCT_ID_START                  0
#define NCT_ID_SERIAL_NUMBER          NCT_ID_START /* ID: 0 */
#define NCT_ID_WIFI_ADDR              1            /* ID: 1 */
#define NCT_ID_BT_ADDR                2            /* ID: 2 */
#define NCT_ID_CM_ID                  3            /* ID: 3 */
#define NCT_ID_LBH_ID                 4            /* ID: 4 */
#define NCT_ID_FACTORY_MODE           5            /* ID: 5 */
#define NCT_ID_RAMDUMP                6            /* ID: 6 */
#define NCT_ID_ID_TEST                7            /* ID: 7 */
#define NCT_ID_BOARD_INFO             8            /* ID: 8 */
#define NCT_ID_GPS_ID                 9            /* ID: 9 */
#define NCT_ID_LCD_ID                 10           /* ID:10 */
#define NCT_ID_ACCELEROMETER_ID       11           /* ID:11 */
#define NCT_ID_COMPASS_ID             12           /* ID:12 */
#define NCT_ID_GYROSCOPE_ID           13           /* ID:13 */
#define NCT_ID_LIGHT_ID               14           /* ID:14 */
#define NCT_ID_CHARGER_ID             15           /* ID:15 */
#define NCT_ID_TOUCH_ID               16           /* ID:16 */
#define NCT_ID_FUELGAUGE_ID           17           /* ID:17 */
#define NCT_ID_WCC                    18           /* ID:18 */
#define NCT_ID_ETH_ADDR               19           /* ID:19 */
#define NCT_ID_UNUSED3                20           /* ID:20 */
#define NCT_ID_UNUSED4                21           /* ID:21 */
#define NCT_ID_UNUSED5                22           /* ID:22 */
#define NCT_ID_UNUSED6                23           /* ID:23 */
#define NCT_ID_UNUSED7                24           /* ID:24 */
#define NCT_ID_UNUSED8                25           /* ID:25 */
#define NCT_ID_UNUSED9                26           /* ID:26 */
#define NCT_ID_UNUSED10               27           /* ID:27 */
#define NCT_ID_UNUSED11               28           /* ID:28 */
#define NCT_ID_UNUSED12               29           /* ID:29 */
#define NCT_ID_UNUSED13               30           /* ID:30 */
#define NCT_ID_UNUSED14               31           /* ID:31 */
#define NCT_ID_UNUSED15               32           /* ID:32 */
#define NCT_ID_UNUSED16               33           /* ID:33 */
#define NCT_ID_UNUSED17               34           /* ID:34 */
#define NCT_ID_UNUSED18               35           /* ID:35 */
#define NCT_ID_UNUSED19               36           /* ID:36 */
#define NCT_ID_UNUSED20               37           /* ID:37 */
#define NCT_ID_BATTERY_MODEL_DATA     38           /* ID:38 */
#define NCT_ID_DEBUG_CONSOLE_PORT_ID  39           /* ID:39 */
#define NCT_ID_BATTERY_MAKE           40           /* ID:40 */
#define NCT_ID_BATTERY_COUNT          41           /* ID:41 */
#define NCT_ID_SPEC                   42           /* ID:42 */
#define NCT_ID_UUID                   43           /* ID:43 */
#define NCT_ID_UUID_END               (NCT_ID_UUID + NCT_NUM_UUID_ENTRIES - 1)
#define NCT_ID_END                    NCT_ID_UUID_END
#define NCT_ID_DISABLED               0xEEEE
#define NCT_ID_MAX                    0xFFFF

typedef struct {
  CHAR8    Sn[30];
} NCT_SERIAL_NUMBER;

typedef struct {
  UINT8    Addr[6];
} NCT_WIFI_ADDR;

typedef struct {
  UINT8    Addr[6];
} NCT_BT_ADDR;

typedef struct {
  UINT8    Addr[6];
} NCT_ETH_ADDR;

typedef struct {
  UINT16    Id;
} NCT_CM_ID;

typedef struct {
  UINT16    Id;
} NCT_LBH_ID;

typedef struct {
  UINT32    Flag;
} NCT_FACTORY_MODE;

typedef struct {
  UINT32    Flag;
} NCT_RAMDUMP;

typedef struct {
  UINT32    Flag;
} NCT_WCC;

typedef struct {
  UINT32    ProcBoardId;
  UINT32    ProcSku;
  UINT32    ProcFab;
  UINT32    PmuBoardId;
  UINT32    PmuSku;
  UINT32    PmuFab;
  UINT32    DisplayBoardId;
  UINT32    DisplaySku;
  UINT32    DisplayFab;
} NCT_BOARD_INFO;

typedef struct {
  UINT32    PortId;
} NCT_DEBUG_PORT_ID;

typedef struct {
  UINT8    Data[MAX_NCT_DATA_SIZE];
} NCT_SPEC;

typedef struct {
  CHAR8    Id[NCT_UUID_ENTRY_SIZE];
} NCT_UUID_CONTAINER;

#pragma pack(1)
typedef union {
  NCT_SERIAL_NUMBER     SerialNumber;
  NCT_WIFI_ADDR         WifiAddr;
  NCT_BT_ADDR           BtAddr;
  NCT_CM_ID             CmId;
  NCT_LBH_ID            LbhId;
  NCT_FACTORY_MODE      FactoryMode;
  NCT_RAMDUMP           Ramdump;
  NCT_WCC               Wcc;
  NCT_ETH_ADDR          EthAddr;
  NCT_BOARD_INFO        BoardInfo;
  NCT_LBH_ID            GpsId;
  NCT_LBH_ID            LcdId;
  NCT_LBH_ID            AccelerometerId;
  NCT_LBH_ID            CompassId;
  NCT_LBH_ID            GyroscopeId;
  NCT_LBH_ID            LightId;
  NCT_LBH_ID            ChargerId;
  NCT_LBH_ID            TouchId;
  NCT_LBH_ID            FuelgaugeId;
  NCT_DEBUG_PORT_ID     DebugPort;
  NCT_SPEC              Spec;
  NCT_UUID_CONTAINER    Uuids[UUIDS_PER_NCT_ENTRY];
  UINT8                 U8Data;
  UINT16                U16Data;
  UINT32                U32Data;
  UINT8                 U8Array[MAX_NCT_DATA_SIZE];
  UINT16                U16Array[MAX_NCT_DATA_SIZE/sizeof (UINT16)];
  UINT32                U32Array[MAX_NCT_DATA_SIZE/sizeof (UINT32)];
} NCT_ITEM;
#pragma pack()

typedef struct {
  UINT32      Index;
  UINT32      Reserved[2];
  NCT_ITEM    Data;
  UINT32      CheckSum;
} NCT_ENTRY;

/*
 * tnspec in NCT lies in space between NCT header and first NCT entry
 * (at 0x4000).
 * TnsOff: offset where tnspec lies from the start of NCT partition.
 * TnsLen: length of tnspec
 */
typedef struct {
  UINT32    MagicId;
  UINT32    VendorId;
  UINT32    ProductId;
  UINT32    Version;
  UINT32    Revision;
  UINT32    TnsId;
  UINT32    TnsOff;
  UINT32    TnsLen;
  UINT32    TnsCrc32;
} NCT_PART_HEAD;

typedef struct {
  NCT_BOARD_INFO    BoardInfo;
} NCT_CUST_INFO;

/**
 * Read an Nct Item with a given ID
 *
 * @param[in]  Id       Nct item Id to read
 * @param[out] Buf      Output buffer to store Nct Item
 *
 * @retval EFI_SUCCESS            All process is successful
 * @retval EFI_NOT_READY          Nct is not initialized
 * @retval EFI_INVALID_PARAMETER  "Id" exceeds limit or output "Buf" is NULL, or integrity broken
 */
EFI_STATUS
EFIAPI
NctReadItem (
  IN  NCT_ID    Id,
  OUT NCT_ITEM  *Buf
  );

/**
 * Get readable spec id/config from NCT
 *
 * @param[out] Id       Buffer to store spec/id
 * @param[out] Config   Buffer to store spec/config
 *
 * @retval EFI_SUCCESS            The operation completed successfully.
 * @retval EFI_INVALID_PARAMETER  "Id" or "Config" is NULL
 * @retval EFI_NOT_FOUND          Cfg or id is not found in spec
 */
EFI_STATUS
EFIAPI
NctGetSpec (
  OUT CHAR8  *Id,
  OUT CHAR8  *Config
  );

/**
 * Get a serial number from Nvidia Configrature Table.
 *
 * @param[out] SerialNumber Output buffer to store SN
 *
 * @retval EFI_SUCCESS            The serial number was gotten successfully.
 * @retval EFI_INVALID_PARAMETER  "SerialNumber" buffer is NULL.
 */
EFI_STATUS
EFIAPI
NctGetSerialNumber (
  OUT CHAR8  *SerialNumber
  );

#endif /* _NCT_LIB_H_ */
