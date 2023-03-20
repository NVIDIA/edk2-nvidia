/** @file
  EFI APEI Protocol as defined in the PI 1.x specification.

  This protocol provides a means of locating/updating APEI Tables.

  Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
  Copyright (c) 2017, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD
  License which accompanies this distribution.  The full text of the license may
  be found at http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _APEI_H_
#define _APEI_H_

//
// Need include this header file for  data structure.
//
#include <Uefi/UefiAcpiDataTable.h>

#define EFI_APEI_PROTOCOL_GUID \
  { \
    0xb5aabe64, 0xf09a, 0x4b94, \
    { 0x8e, 0xfa, 0x2e, 0x23, 0x4d, 0x00, 0x6d, 0x3c } \
  }

typedef struct _EFI_APEI_PROTOCOL EFI_APEI_PROTOCOL;

typedef struct _EFI_APEI_GENERIC_ADDRESS_STRUCTURE {
  UINT16    AddrerssSpaceId;
  UINT16    RegisterBitWidth;
  UINT16    RegisterBitOffset;
  UINT16    AccessSize;
  UINTN     Address;
} EFI_APEI_GENERIC_ADDRESS_STRUCTURE;

typedef struct _EFI_APEI_ERROR_SOURCE {
  UINT16                                GhesType;
  UINT16                                SourceId;
  UINT32                                NumberRecordstoPreAllocate;
  UINT32                                MaxSectionsPerRecord;
  UINT32                                MaxRawDataLength;
  EFI_APEI_GENERIC_ADDRESS_STRUCTURE    ErrorStatusAddress;
  UINT32                                EventId;
  UINT32                                ErrorStatusBlockLength;
  EFI_APEI_GENERIC_ADDRESS_STRUCTURE    ReadAckRegister;
  UINTN                                 ReadAckPreserve;
  UINTN                                 ReadAckWrite;
  UINT16                                NotificationType;
  UINT16                                SourceIdSdei;
  UINT32                                PollInterval;
} EFI_APEI_ERROR_SOURCE;

typedef struct _EFI_APEI_ERROR_SOURCE_INFO {
  UINTN     ErrorRecordsRegionBase;
  UINTN     ErrorRecordsRegionSize;
  UINT32    NumErrorSource;
} EFI_APEI_ERROR_SOURCE_INFO;

typedef struct _EFI_HEST_SUBTABLE_INFO {
  //  if we need more info for
  //  VOID   *Info;
  UINT32    Length;
} EFI_HEST_SUBTABLE_INFO;

/**
  Updates error source information to the specified APEI Table.

  This function provides an interface updates the hardware error source
  information to the APEI table. The hardware error source describes a
  standardized mechanism platforms may use to describe their error sources.
  Use of this interface is the preferred way for platforms to describe their
  error sources as it is platform and processor-architecture independent and
  allows the platform to describe the operational parameters associated with
  error sources.
  This mechanism allows for the platform to describe error sources in detail;
  communicating operational parameters (i.e. severity levels, masking bits,
  and threshold values) to OS as necessary. It also allows the platform to
  report error sources for which OS would typically not implement support
  (for example, chipset-specific error registers).

  @param[in]            This          The EFI_APEI_PROTOCOL instance.
  @param[in]       Signature          The signature of the table you want to
                                      update. For now, we only support HEST.

  @retval EFI_SUCCESS                 APEI Table successfully updated.
  @retval Other          Some error occurred when executing this entry point.
**/

typedef
EFI_STATUS
(EFIAPI *EFI_APEI_UPDATE)(
  IN CONST EFI_APEI_PROTOCOL     *This,
  IN UINT32                      Signature
  );

/**
  The EFI_APEI_PROTOCOL service provides the interfaces that are used to locate
  APEI Tables, get error source information including (number of error sources,
  it’s error record region map etc.), map memory for each error record region
  and add the error entries to error source structure array.
**/

typedef struct _EFI_APEI_PROTOCOL {
  EFI_APEI_UPDATE    UpdateApei;
} EFI_APEI_PROTOCOL;

extern EFI_GUID  gEfiApeiProtocolGuid;
extern EFI_GUID  gEfiApeiGetErrorSourcesGuid;
extern EFI_GUID  gEfiApeiSetTimeOfDayGuid;

#endif
