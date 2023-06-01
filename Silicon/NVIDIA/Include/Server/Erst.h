/** @file
  NVIDIA ERST header

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _ERST_H_
#define _ERST_H_

#include <Uefi.h>
#include <IndustryStandard/Acpi.h>

/* ERST "Register" read/write mask/values */
#define ERST_DEFAULT_MASK         0xFFFFFFFFFFFFFFFF
#define ERST_RECORD_COUNT_MASK    0xFFFFFFFF
#define ERST_STATUS_MASK          0x000000FF
#define ERST_BUSY_VALUE           0x1
#define ERST_BUSY_MASK            0x00000001
#define ERST_STATUS_INVALID_MASK  0x00000001
#define ERST_STATUS_IS_VALID      0x0
#define ERST_STATUS_IS_INVALID    0x1
#define ERST_GOTO_MASK            0x1F

/* ACPI spec defines the STATUS width and offset */
#define ERST_STATUS_WIDTH       8
#define ERST_STATUS_BIT_OFFSET  1

/* We make bit 0 of status "register" contain valid indication */
#define ERST_STATUS_INVALID_OFFSET  0
#define ERST_STATUS_INVALID_WIDTH   1

/* Possible Attribute flags for the error log address range */
#define ERST_LOG_ATTRIBUTE_NONVOLATILE  0x2
#define ERST_LOG_ATTRIBUTE_SLOW         0x4

/* Special ID values for ERST IDs */
#define ERST_FIRST_RECORD_ID    0x0
#define ERST_INVALID_RECORD_ID  0xFFFFFFFFFFFFFFFF

/* Flags for register writes in ERST Table */
#define ERST_FLAG_PRESERVE_REGISTER  0x01

/* Indication that ERST init completed successfully and we can install it for OS */
#define ERST_INIT_SUCCESS  (gNVIDIAErrorSerializationProtocolGuid.Data1)

typedef enum {
  ERST_OPERATION_INVALID     = 0,
  ERST_OPERATION_WRITE       = 1,
  ERST_OPERATION_READ        = 2,
  ERST_OPERATION_CLEAR       = 3,
  ERST_OPERATION_DUMMY_WRITE = 4,
} ERST_OPERATION_TYPE;

typedef struct {
  UINT64    PhysicalBase;
  UINT64    Length;
  UINT64    Attributes;
} ERST_ERROR_LOG_INFO;

typedef struct {
  PHYSICAL_ADDRESS       ErstBase;
  UINTN                  ErstSize;
  ERST_ERROR_LOG_INFO    ErrorLogInfo;
} ERST_BUFFER_INFO;

typedef struct {
  /* Fields below are referred to by the ERST table */
  /* Constant, read via ERST */
  UINT64                 Timings;
  ERST_ERROR_LOG_INFO    ErrorLogAddressRange;

  /* Read and written by OS, Write-Only for ErstMm */
  volatile UINT32        Status; // Note: bits 8:1 are status according to spec, using bit 0 as invalid indicator

  /* Read-Only for OS via ERST, Write-Only for ErstMm */
  volatile UINT32        RecordCount;

  /* Written by OS via ERST */
  UINT64                 Operation;        // ERST_OPERATION_TYPE
  UINT64                 RecordOffset;
  UINT64                 RecordID;
} ERST_COMM_STRUCT;
STATIC_ASSERT ((sizeof (ERST_COMM_STRUCT) == 8*8), "Expected ERST_COMM_STRUCT to be 64 bytes");

/* Per ACPI spec: "QWORD:
 * [63:32] value in microseconds that the platform expects would be the maximum amount of time it will take to process and complete an EXECUTE_OPERATION.
 * [31:0] value in microseconds that the platform expects would be the nominal amount of time it will take to process and complete an EXECUTE_OPERATION."
 */
#define ERST_MAX_TIMING_SHIFT     32
#define ERST_NOMINAL_TIMING_MASK  0xFFFFFFFF
// Default to 50 ms typical, 1000 ms max. Default is put in the structure only until the Nor protocol
// is located, at which time the actual timing values are calculated based on the Nor attributes.
#define ERST_DEFAULT_TIMINGS  (((1000ul * 1000) << ERST_MAX_TIMING_SHIFT) | (50 * 1000))

VOID
ErstSetupTable (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  );

#endif
