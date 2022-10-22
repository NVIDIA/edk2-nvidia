/** @file
*
*  Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef RASNSINTERFACE_H_
#define RASNSINTERFACE_H_

#include <Uefi.h>
#include <IndustryStandard/Acpi.h>

/*
 * RAS_FW shares a single memory region split into 3 sub-sections:
 * CommBase: 64 KB mailbox to use in a similar way to MM_COMMUNICATE protocol
 * EinjBase: 4KB to hold EINJ requests from the OS
 * PcieBase: 4KB to hold ACPI interfaces for PCIe-related features
 * CperBase: contains error records and ack registers (rest of allocated memory)
 */
#define RAS_FW_COMM_SIZE  SIZE_64KB
#define RAS_FW_EINJ_SIZE  SIZE_4KB
#define RAS_FW_PCIE_SIZE  SIZE_4KB

typedef struct {
  PHYSICAL_ADDRESS    Base;
  UINTN               Size;
  PHYSICAL_ADDRESS    CommBase;
  UINTN               CommSize;
  PHYSICAL_ADDRESS    EinjBase;
  UINTN               EinjSize;
  PHYSICAL_ADDRESS    PcieBase;
  UINTN               PcieSize;
  PHYSICAL_ADDRESS    CperBase;
  UINTN               CperSize;
} RAS_FW_BUFFER;

/* Default EINJ read/write mask */
#define EINJ_DEFAULT_MASK  0x3FFFFFFFFFFF
/* Size in bytes of the OEM Defined data */
#define OEM_DATA_LENGTH  128
/* Entries in the Trigger Action table */
#define EINJ_TRIGGER_ACTION_COUNT  1
/* DeviceId for vendor specific EINJ */
#define EINJ_VENDOR_DEVICE_ID  0x500
/* Convert GSIV source id to SDEI source id */
#define GSIV_TO_SDEI_SOURCE_ID(id)  (0x8000 | id)
/* Signature when EINJ is disabled */
#define EINJ_DISABLED_SIGNATURE  SIGNATURE_32(' ', ' ', ' ', ' ')

/*
 * Definition of the "Trigger Action Table" as defined in ACPI 6.4
 */
typedef struct {
  EFI_ACPI_6_4_EINJ_TRIGGER_ACTION_TABLE           Header;
  EFI_ACPI_6_4_EINJ_INJECTION_INSTRUCTION_ENTRY    TriggerActions[EINJ_TRIGGER_ACTION_COUNT];
} EFI_ACPI_6_X_EINJ_TRIGGER_ERROR_ACTION_TABLE;

/*
 * Definition of the "SET_ERROR_TYPE_WITH_ADDRESS Data Structure" as defined in
 * ACPI 6.4
 */
typedef struct {
  UINT32    ErrorType;
  UINT32    VendorErrorTypeExtOffset;
  UINT32    Flags;
  UINT32    ProcessorIdentification;
  UINT64    MemoryAddress;
  UINT64    MemoryAddressRange;
  UINT32    PCIeSBDF;
} EFI_ACPI_6_X_EINJ_SET_ERROR_TYPE_WITH_ADDRESS;

/*
 * Definition of the "Vendor Error Type Extension Structure" as defined in
 * ACPI 6.4
 */
typedef struct {
  UINT32    Length;
  UINT32    SBDF;
  UINT16    VendorID;
  UINT16    DeviceID;
  UINT8     RevID;
  UINT8     Reserved[3];
} EFI_ACPI_6_X_EINJ_VENDOR_ERROR_TYPE;

typedef struct {
  /* Fields below are referred to by the EINJ table */
  UINT64                                           Signature;
  UINT64                                           Status;
  UINT64                                           Busy;
  UINT64                                           SetErrorType;
  UINT64                                           SupportedTypes;
  UINT64                                           Timings;
  UINT64                                           TriggerActionTableRegister;
  UINT64                                           TriggerActionTablePtr;
  UINT64                                           SetErrorTypeWithAddressPtr;

  /* Fields below are pointed to by pointers defined above */
  EFI_ACPI_6_X_EINJ_TRIGGER_ERROR_ACTION_TABLE     TriggerErrorActionTable;
  EFI_ACPI_6_X_EINJ_SET_ERROR_TYPE_WITH_ADDRESS    SetErrorTypeWithAddress;
  EFI_ACPI_6_X_EINJ_VENDOR_ERROR_TYPE              VendorErrorType;
} RAS_FW_EINJ_COMM_STRUCT;

/* Per ACPI spec: "QWORD : [63:32] value in microseconds that the platform
 * expects would be the maximum amount of time it will take to process and
 * complete an EXECUTE_OPERATION. [31:0] value in microseconds that the
 * platform expects would be the nominal amount of time it will take to
 * process and complete an EXECUTE_OPERATION."
 *
 * Setting it at 10ms (10000 us) for both.
 */
#define EINJ_DEFAULT_TIMING       10000
#define EINJ_MAX_TIMING_SHIFT     32
#define EINJ_NOMINAL_TIMING_MASK  0xFFFFFFFF

#define MAX_SOCKETS      4
#define PCIE_PER_SOCKET  10

/*
 * The NS buffer shared between RAS-FW and Non-Secure world is going to have
 * the data in this structure format for all the PCIe controllers of all
 * sockets.
 */
typedef struct {
  UINT32    IsInDPC;
  UINT32    SocketID;
  UINT32    SegmentID;
  UINT32    ErrSrc;
} RAS_FW_PCIE_DPC_INFO;

typedef struct {
  RAS_FW_PCIE_DPC_INFO    PcieDpcInfo[MAX_SOCKETS][PCIE_PER_SOCKET];
} RAS_FW_PCIE_DPC_COMM_STRUCT;

#endif
