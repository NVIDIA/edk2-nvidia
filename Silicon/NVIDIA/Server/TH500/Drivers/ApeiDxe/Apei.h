/** @file
*
*  Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
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
#include <Library/ArmSmcLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Protocol/AcpiTable.h>
#include <Protocol/Apei.h>
#include <Protocol/MmCommunication2.h>
#include <Protocol/RasNsCommPcieDpcDataProtocol.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <Server/RASNSInterface.h>
#include <libfdt.h>

/* ACPI table creation default values */
#define EFI_ACPI_OEM_ID            {'N','V','I','D','I','A'}
#define EFI_ACPI_OEM_TABLE_ID      SIGNATURE_64('T','H','5','0','0',' ',' ',' ')
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

/* If an error source has the following event ID, it is a BERT record */
#define BERT_EVENT_ID  0xFFBE

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

/*
 * Operation Ids for RAS_FW, to be passed as Arg3 via FFA direct messaging
 */
#define RAS_FW_NS_BUFFER_REQ       0xC0270001
#define RAS_FW_GUID_COMMUNICATION  0xC0270002

/*
 * Unique ID (UUID) that identifies the RAS_FW secure partition
 */
#define RAS_FW_UUID_0  0x3c99b242
#define RAS_FW_UUID_1  0xc93d11eb
#define RAS_FW_UUID_2  0x91012fbd
#define RAS_FW_UUID_3  0xec0769ff

/*
 * FFA function IDs that are missing from ArmFfaSvc.h
 */
#define ARM_SVC_ID_FFA_PARTITION_INFO_GET  0x84000068
#define ARM_SVC_ID_FFA_RXTX_MAP            0xC4000066
#define ARM_SVC_ID_FFA_RXTX_UNMAP          0x84000067
#define ARM_SVC_ID_FFA_RX_RELEASE          0x84000065

/* LIC SW IO Set register offset */
#define INTR_CTLR_SW_IO_N_INTR_STATUS_SET_0_OFFSET  0x04

/* There are 10 unique entries supported in EINJ */
#define EINJ_ENTRIES_COUNT  10

/**
 * Query RAS_FW for error sources and build the HEST/BERT tables accordingly.
 *
 * @param[in] RasFwBufferInfo  Details about the RAS_FW/NS buffer
 *
 * @return EFI_SUCCESS         If the tables were created successfully
**/
EFI_STATUS
HestBertSetupTables (
  IN RAS_FW_BUFFER  *RasFwBufferInfo,
  IN BOOLEAN        SkipHestTable,
  IN BOOLEAN        SkipBertTable
  );

/**
 * Set Time Of Day in RAS FW
 *
 * @param[in] RasFwBufferInfo  Details about the RAS_FW/NS buffer
 *
 * @return EFI_SUCCESS         If ToD was sent successfully
**/
EFI_STATUS
SetTimeOfDay (
  IN RAS_FW_BUFFER  *RasFwBufferInfo
  );

/**
 * Build the EINJ table based on data from the shared NS buffer from RAS_FW.
 *
 * @param[in] RasFwBufferInfo  Details about the RAS_FW/NS buffer
 *
 * @return EFI_SUCCESS         If the table was created successfully
**/
EFI_STATUS
EinjSetupTable (
  IN RAS_FW_BUFFER  *RasFwBufferInfo
  );

/**
 * Query RAS_FW via FFA to get the information about the shared buffer between
 * RAS_FW and NS world.
 *
 * @param[out] RasFwBufferInfo  Details about the RAS_FW/NS buffer
 *
 * @return EFI_SUCCESS         If the buffer information was found
**/
EFI_STATUS
FfaGetRasFwBuffer (
  OUT RAS_FW_BUFFER  *RasFwBufferInfo
  );

/**
 * Call RAS_FW with a GUID-based request.
 *
 * @param[in] CommunicateHeader  GUID-based data structure to send
 * @param[in] RasFwBufferInfo    Details about the RAS_FW/NS buffer
 *
 * @return EFI_SUCCESS         If the buffer information was found
 *
**/
EFI_STATUS
FfaGuidedCommunication (
  IN EFI_MM_COMMUNICATE_HEADER  *CommunicateHeader,
  IN RAS_FW_BUFFER              *RasFwBufferInfo
  );

/**
 * Call an SMC to send an FFA request. This function is similar to
 * ArmCallSmc except that it returns extra GP registers as needed for FFA.
 *
 * @param Args    GP registers to send with the SMC request
 */
VOID
CallFfaSmc (
  IN OUT ARM_SMC_ARGS  *Args
  );

#endif
