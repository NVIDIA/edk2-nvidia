/** @file

   ACPI power meter definitions

   Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.

   SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ACPI_PWR_METER_DEFINES_H__
#define __ACPI_PWR_METER_DEFINES_H__

#define BIT(x)  (1 << x)

// Supported capabilities
#define PWR_METER_SUPPORTS_MEASUREMENT       0
#define PWR_METER_SUPPORTS_TRIP_POINTS       1
#define PWR_METER_SUPPORTS_HW_LIMITS         2
#define PWR_METER_SUPPORTS_NOTIFY_HW_LIMITS  3
#define PWR_METER_SUPPORTS_REPORT_DISCHARGE  8      // Required for pwr meters that are battery-type devices

// Measurement unit
#define PWR_METER_MEASUREMENT_IN_MW  0x00000000

// Measurement type
#define PWR_METER_MEASURE_IP_PWR  0x00000000
#define PWR_METER_MEASURE_OP_PWR  0x00000001

// Measurement accuracy
#define PWR_METER_MEASUREMENT_ACCURACY_80   0x00013880
#define PWR_METER_MEASUREMENT_ACCURACY_100  0x000186A0

// Measurement sampling time
#define PWR_METER_MEASUREMENT_SAMPLING_TIME_UNKNOWN  0xFFFFFFFF
#define PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS     0x00000032          // 50ms average interval
#define PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC     0x000003E8          // 1s average interval

// Hysteresis margin
#define PWR_METER_HYSTERESIS_MARGIN_UNKNOWN  0xFFFFFFFF

// Hardware limit configuration by OSPM
#define PWR_METER_HW_LIMIT_RO  0x00000000
#define PWR_METER_HW_LIMIT_RW  0xFFFFFFFF

// Return error codes
#define PWR_METER_SUCCESS         0x00000000
#define PWR_METER_OUT_OF_RANGE    0x00000001
#define PWR_METER_HW_TIMEOUT      0x00000002
#define PWR_METER_UNKNOWN_HW_ERR  0x00000003
#define PWR_METER_ERR_RETURN      0xFFFFFFFF

// Notification event value
#define PWR_METER_NOTIFY_CONFIG    0x80
#define PWR_METER_NOTIFY_TRIP      0x81
#define PWR_METER_NOTIFY_CAP       0x82
#define PWR_METER_NOTIFY_CAPPING   0x83
#define PWR_METER_NOTIFY_INTERVAL  0x84

#endif //__ACPI_PWR_METER_DEFINES_H__
