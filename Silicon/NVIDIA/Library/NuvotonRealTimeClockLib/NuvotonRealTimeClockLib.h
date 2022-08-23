/** @file

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NUVOTON_REAL_TIME_CLOCK_LIB_H__
#define __NUVOTON_REAL_TIME_CLOCK_LIB_H__

#include <PiDxe.h>
#include <Pi/PiI2c.h>

#define NUVOTON_RTC_TIME_ADDRESS            0x00
#define NUVOTON_RTC_DAY_OF_WEEK_ADDRESS     0x06
#define NUVOTON_RTC_CONTROL_ADDRESS         0x0A
#define NUVOTON_RTC_PRIMARY_ACCESS_ADDRESS  0x23

#define NUVOTON_RTC_CONTROL_TWO_SECONDARY  0
#define NUVOTON_RTC_CONTROL_TWO_PRIMARY    1
#define NUVOTON_RTC_CONTROL_CIE_DISABLE    0
#define NUVOTON_RTC_CONTROL_CIE_ENABLE     1
#define NUVOTON_RTC_CONTROL_OFIE_DISABLE   0
#define NUVOTON_RTC_CONTROL_OFIE_ENABLE    1
#define NUVOTON_RTC_CONTROL_AIE_DISABLE    0
#define NUVOTON_RTC_CONTROL_AIE_ENABLE     1
#define NUVOTON_RTC_CONTROL_DSM_DST_OFF    0
#define NUVOTON_RTC_CONTROL_DSM_DST_ON     1
#define NUVOTON_RTC_CONTROL_HF_12H         0
#define NUVOTON_RTC_CONTROL_HF_24H         1
#define NUVOTON_RTC_CONTROL_DM_BCD         0
#define NUVOTON_RTC_CONTROL_DM_BIN         1
#define NUVOTON_RTC_CONTROL_ST_RUN         0
#define NUVOTON_RTC_CONTROL_ST_STOP        1

#define NUVOTON_RTC_SECOND_MASK  0x7F
#define NUVOTON_RTC_MINUTE_MASK  0x7F
#define NUVOTON_RTC_HOUR_MASK    0x3F
#define NUVOTON_RTC_PM_MASK      0x80
#define NUVOTON_RTC_DAY_MASK     0x3F
#define NUVOTON_RTC_MONTH_MASK   0x1F
#define NUVOTON_RTC_YEAR_MASK    0xFF

#define NUVOTON_RTC_BASE_YEAR    2000
#define NUVOTON_RTC_WDAY_OFFSET  3

#pragma pack(1)
typedef struct {
  UINT8    TWO  : 1;               // Time Register Write Ownership
  UINT8    CIE  : 1;               // RTC Clear Interrupt Enable
  UINT8    OFIE : 1;               // Oscillator Fail Interrupt Enable
  UINT8    AIE  : 1;               // Alarm Interrupt Enable
  UINT8    DSM  : 1;               // Day Light Saving Mode
  UINT8    HF   : 1;               // Hour Format
  UINT8    DM   : 1;               // Data Mode
  UINT8    ST   : 1;               // Stop
} NUVOTON_RTC_CONTROL;

typedef struct {
  UINT8    BVL      : 3;           // Battery Voltage Level
  UINT8    Reserved : 1;           //
  UINT8    CIF      : 1;           // RTC_CLR# asserted status bit
  UINT8    RTCF     : 1;           // RTC Fail Bit
  UINT8    OF       : 1;           // Oscillator Fail Bit
  UINT8    AF       : 1;           // Alarm Flag
} NUVOTON_RTC_STATUS;

typedef struct {
  UINT8    I2CPA    : 1;           // If 1, Primary I2C has R/W access always
  UINT8    Reserved : 7;
} NUVOTON_RTC_PRIMARY_ACCESS;

typedef struct {
  UINT8    Second;
  UINT8    SecondAlarm;                 // Read-only
  UINT8    Minute;
  UINT8    MinuteAlarm;                 // Read-only
  UINT8    Hour;
  UINT8    HourAlarm;                   // Read-only
  UINT8    DayOfWeek;
  UINT8    Day;
  UINT8    Month;
  UINT8    Year;
} NUVOTON_RTC_DATE_TIME;

typedef struct {
  UINT8                    Address;
  NUVOTON_RTC_DATE_TIME    DateTime;
  NUVOTON_RTC_CONTROL      Control;
} NUVOTON_RTC_TIME_PACKET;

typedef struct {
  UINT8                  Address;
  NUVOTON_RTC_CONTROL    Control;
  NUVOTON_RTC_STATUS     Status;
} NUVOTON_RTC_CONTROL_PACKET;

typedef struct {
  UINT8                         Address;
  NUVOTON_RTC_PRIMARY_ACCESS    PrimaryAccess;
} NUVOTON_RTC_PRIMARY_PACKET;

typedef struct {
  UINT8    Address;
  UINT8    DayOfWeek;
} NUVOTON_RTC_DAY_OF_WEEK_PACKET;

#pragma pack()

///
/// I2C device request
///
/// The EFI_I2C_REQUEST_PACKET describes a single I2C transaction.  The
/// transaction starts with a start bit followed by the first operation
/// in the operation array.  Subsequent operations are separated with
/// repeated start bits and the last operation is followed by a stop bit
/// which concludes the transaction.  Each operation is described by one
/// of the elements in the Operation array.
///
typedef struct {
  ///
  /// Number of elements in the operation array
  ///
  UINTN                OperationCount;

  ///
  /// Description of the I2C operation
  ///
  EFI_I2C_OPERATION    Operation[2];
} I2C_REQUEST_PACKET_2_OPS;

#endif
