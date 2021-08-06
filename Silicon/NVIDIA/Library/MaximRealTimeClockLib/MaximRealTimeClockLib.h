/** @file

  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __MAXIM_REAL_TIME_CLOCK_LIB_H__
#define __MAXIM_REAL_TIME_CLOCK_LIB_H__

#include <PiDxe.h>
#include <Pi/PiI2c.h>

#define MAXIM_I2C_ADDRESS_INDEX   1
#define MAXIM_I2C_DELAY_US        15000

#define MAXIC_RTC_CONTROL_ADDRESS 0x03
#define MAXIM_RTC_UPDATE0_ADDRESS 0x04
#define MAXIM_RTC_TIME_ADDRESS    0x07

#define MAXIM_BASE_YEAR           2000

#pragma pack(1)
typedef struct {
  UINT8                           BCD:1;
  UINT8                           TwentyFourHourMode:1;
  UINT8                           Reserved:6;
} MAXIM_RTC_CONTROL;

typedef struct {
  UINT8                           UpdateFromWrite:1;
  UINT8                           ClearFlagsOnRead:1;
  UINT8                           FreezeSeconds:1;
  UINT8                           Reserved1:1;
  UINT8                           ReadBufferUpdate:1;
  UINT8                           Reserved2:3;
} MAXIM_RTC_UPDATE0;

typedef struct {
  UINT8                           Seconds;
  UINT8                           Minutes;
  UINT8                           Hours;
  UINT8                           DayOfWeek;
  UINT8                           Month;
  UINT8                           Years;
  UINT8                           Day;
} MAXIM_RTC_DATE_TIME;

typedef struct {
  UINT8                           Address;
  union {
    MAXIM_RTC_CONTROL             Control;
    MAXIM_RTC_UPDATE0             Update;
    MAXIM_RTC_DATE_TIME           DateTime;
  };
} MAXIM_RTC_UPDATE_DATA;
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
  UINTN OperationCount;

  ///
  /// Description of the I2C operation
  ///
  EFI_I2C_OPERATION Operation [2];
} I2C_REQUEST_PACKET_2_OPS;

#define VRS_CTL_2          0x29
#define VRS_CTL_2_EN_PEC   BIT0
#define VRS_RTC_T_BASE     0x70
#define VRS_RTC_A_BASE     0x74
#define VRS_RTC_ATTEMPTS   0x0f
#define VRS_I2C_DELAY_US   15000


#endif
