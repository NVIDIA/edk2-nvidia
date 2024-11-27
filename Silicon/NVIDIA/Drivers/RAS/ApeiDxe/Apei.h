/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2021-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _APEIDXE_APEI_H_
#define _APEIDXE_APEI_H_

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/DeviceDiscoveryLib.h>
#include <Library/MpCoreInfoLib.h>
#include <Library/RASFwInterfaceLib.h>
#include <Protocol/AcpiTable.h>
#include <Protocol/Apei.h>
#include <Protocol/MmCommunication2.h>
#include <Protocol/RasNsCommPcieDpcDataProtocol.h>
#include <Server/RASNSInterface.h>
#include <Library/FdtLib.h>
#include <Server/Erst.h>

/* ACPI table creation default values */
#define EFI_ACPI_OEM_REVISION      0x00000001
#define EFI_ACPI_CREATOR_ID        SIGNATURE_32('N','V','D','A')
#define EFI_ACPI_CREATOR_REVISION  0x00000001
#define EFI_ACPI_VENDOR_ID         SIGNATURE_32('N','V','D','A')

/* SDEI Table for RAS event notification */
#define EFI_ACPI_6_X_SDEI_TABLE_SIGNATURE  SIGNATURE_32('S','D','E','I')
#define EFI_ACPI_6_X_SDEI_TABLE_REVISION   0x01
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER    Header;
} EFI_ACPI_6_X_SDEI_TABLE;

/* For GSIV events, create an SDEI clone with msb set */
#define GSIV_TO_SDEI_SOURCE_ID(id)  (0x8000 | id)

/* Minimum polling interval for polled error sources (100ms) */
#define MINIMUM_POLLING_INTERVAL  100

/*
 * The below values are defined in the ACPI spec but are missing from all
 * UEFI headers.
 */
#define EFI_ACPI_6_X_EINJ_SET_ERROR_TYPE_WITH_ADDRESS    0x08
#define EFI_ACPI_6_X_EINJ_GET_EXECUTE_OPERATION_TIMINGS  0x09

/* There are 10 unique entries supported in EINJ */
#define EINJ_ENTRIES_COUNT  10

/**
 * Query RAS_FW for error sources and build the HEST/BERT tables accordingly.
 *
 * @param[in] RasFwInterface   Instance of the RASFwInterface library
 * @param[in] SkipHestTable    Whether the HEST table must be built
 * @param[in] SkipBertTable    Whether the BERT table must be built
 * @param[in] SocketCount      Number of sockets in the system
 *
 * @return EFI_SUCCESS         If the tables were created successfully
**/
EFI_STATUS
HestBertSetupTables (
  IN RAS_FW_INTERFACE  *RasFwInterface,
  IN BOOLEAN           SkipHestTable,
  IN BOOLEAN           SkipBertTable,
  IN UINT32            SocketCount
  );

/**
 * Set Time Of Day in RAS FW
 *
 * @param[in] RasFwInterface   Instance of the RASFwInterface library
 * @param[in] SocketCount      Number of sockets in the system
 *
 * @return EFI_SUCCESS         If ToD was sent successfully
**/
EFI_STATUS
SetTimeOfDay (
  IN RAS_FW_INTERFACE  *RasFwInterface,
  IN UINT32            SocketCount
  );

/**
 * Build the EINJ table based on data from the shared NS buffer from RAS_FW.
 *
 * @param[in] RasFwInterface   Details about the RAS_FW/NS buffer
 *
 * @return EFI_SUCCESS         If the table was created successfully
**/
EFI_STATUS
EinjSetupTable (
  IN RAS_FW_INTERFACE  *RasFwInterface
  );

#endif
