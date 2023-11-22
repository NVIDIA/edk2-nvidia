/** @file

  Private Sequential record protocol/header definitions.

  SPDX-FileCopyrightText: Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SEQUENTIAL_RECORD_PVT_H
#define SEQUENTIAL_RECORD_PVT_H

#include <Library/MmServicesTableLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Protocol/SequentialRecord.h>
#include <TH500/TH500MB1Configuration.h>
#include <Guid/NVIDIAMmMb1Record.h>

#define READ_LAST_RECORD     (0)
#define WRITE_NEXT_RECORD    (1)
#define ERASE_PARTITION      (2)
#define CLEAR_EFI_NSVARS     (3)
#define CLEAR_EFI_VARIABLES  (4)

typedef struct {
  /* Operation to perform */
  UINTN         Function;
  /* Return value in the EFI standard. Initialized as EFI_SUCCESS when making a request. */
  EFI_STATUS    ReturnStatus;
  /* Socket number [0-3] */
  UINTN         Socket;
  /* Flag. To be used mostly in CMET record storage.*/
  UINTN         Flag;
  /* Extra data (ie data to write when RAS_FW requests a write, or read data from MM when returning a read request */
  UINT8         Data[]; /* Flexible array member */
} RAS_MM_COMMUNICATE_PAYLOAD;

typedef struct {
  /* Operation to perform */
  UINTN         Command;
  /* Return value in the EFI standard. Initialized as EFI_SUCCESS when making a request. */
  EFI_STATUS    ReturnStatus;
} SATMC_MM_COMMUNICATE_PAYLOAD;

/*
 * The "targets" listed below are entities where a CPER record can be sent and that can be overridden by UEFI MM.
 */
#define PUBLISH_HEST  0x2
#define PUBLISH_BMC   0x8

/*
 * Maximum number of thermal zones for RAS logging.
 */
#define RAS_MAX_THERMAL_ZONES  12

/*
 * Structure to store thermal zone values and store them with the RAS log.
 */
typedef struct {
  UINT32    Valid_N;  /* Uses bits [0-11] to indicate which zones are valid, 0 for valid, 1 for invalid */
  UINT32    Temperature[RAS_MAX_THERMAL_ZONES];
} RAS_THERMAL_ZONES;

/*
 * Header for a RAS log
 */
typedef struct {
  UINT32    LogType;
  UINT32    TotalSize;
} RAS_LOG_HEADER;

/*
 * Format of a RAS log entry.
 * In particular, the Log field contains the actual CPER and begins with EFI_ACPI_6_4_GENERIC_ERROR_DATA_ENTRY_STRUCTURE
 * that contains the severity and the actual details about the CPER (SectionType, ErrorDataLength...)
 */
typedef struct {
  RAS_LOG_HEADER       Header;
  RAS_THERMAL_ZONES    Thermal;
  UINT8                Log[]; /* Flexible array member */
} RAS_LOG_MM_ENTRY;

#endif // SEQUENTIAL_RECORD_PVT_H
