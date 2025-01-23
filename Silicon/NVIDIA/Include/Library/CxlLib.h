/** @file

  CXL Library header

  SPDX-FileCopyrightText: Copyright (c) 2025-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CXL_LIB_H__
#define __CXL_LIB_H__

#include <Uefi.h>

#include <Protocol/PciIo.h>

// TODO: These class codes should be upstreamed to Pci23.h in edk2 repository
///
/// PCI_CLASS_MEMORY_CONTROLLER, Base Class 05h.
///
///@{
#define PCI_CLASS_MEMORY_CXL  0x02 // CXL MEMORY DEVICE
#define   PCI_IF_MEMORY_CXL   0x10 // CXL PROGRAMMING INTERFACE
///@}

// TODO: figure out how to incorporate this into the IndustryStandard headers
// Compute Express Link Specification Revision 3.2, Version 1.0
// 8.2.1.3.3 DVSEC Flex Bus Port Status (Offset 0Eh)
typedef union {
  struct {
    UINT16    CacheEnabled                           : 1;            // bit 0
    UINT16    IoEnabled                              : 1;            // bit 1
    UINT16    MemEnabled                             : 1;            // bit 2
    UINT16    CxlSyncHdrBypassEnabled                : 1;            // bit 3
    UINT16    DriftBufferEnabled                     : 1;            // bit 4
    UINT16    Cxl68bFlitAndVhEnabled                 : 1;            // bit 5 //Rev 1
    UINT16    CxlMultiLogicalDeviceEnabled           : 1;            // bit 6 //Rev 1
    UINT16    EvenHalfFailed                         : 1;            // bit 7 //Rev 2
    UINT16    CxlCorrectableProtocolIdFramingError   : 1;            // bit 8
    UINT16    CxlUncorrectableProtocolIdFramingError : 1;            // bit 9
    UINT16    CxlUnexpectedProtocolIdDropped         : 1;            // bit 10
    UINT16    CxlRetimersPresentMismatched           : 1;            // bit 11
    UINT16    FlexBusEnableBitsPhase2Mismatch        : 1;            // bit 12
    UINT16    CxlLatencyOptimized256bFlitEnabled     : 1;            // bit 13 //Rev 2
    UINT16    CxlPbrFlitEnabled                      : 1;            // bit 14 //Rev 2
    UINT16    CxlIoThrottleRequiredAt64Gtps          : 1;            // bit 15 //Rev 2
  } Bits;
  UINT16    Uint16;
} CXL_3_2_DVSEC_FLEX_BUS_PORT_STATUS;

extern CONST UINT8  mCxlType3ClassCode[3];

/**
  Get the offset of the next DVSEC capability matching the CXL Vendor ID.

  When searching for multiple capabilities, pass in the NextCap
  offset from the previous call as the starting offset to continue the search.

  @param[in] PciIo       Pci Io Protocol.
  @param[inout] Offset   Starting offset on entry, found offset on success.
  @param[out] NextCap    Offset to the next capability.

  @retval EFI_SUCCESS      Successfully located a CXL DVSEC register block.
  @retval EFI_INVALID_PARAMETER PciIo is NULL or Offset is NULL.
  @retval EFI_NOT_FOUND    CXL DVSEC register block not found.
  @retval Others           Other error.

**/
EFI_STATUS
EFIAPI
CxlLibLocateCxlDvsec (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN OUT UINT32           *Offset,
  OUT UINT32              *NextCap OPTIONAL
  );

/**
  Read the DVSEC ID from the CXL DVSEC capability that starts at the
  given offset.

  @param[in] PciIo       Pci Io Protocol.
  @param[in] Offset      Offset to the CXL DVSEC capability.
  @param[out] CxlDvsecId The CXL DVSEC ID.

  @retval EFI_SUCCESS      Successfully read the CXL DVSEC ID.
  @retval EFI_INVALID_PARAMETER PciIo is NULL or CxlDvsecOffset is 0.
  @retval Others           Other error.

**/
EFI_STATUS
EFIAPI
CxlLibGetCxlDvsecId (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               CxlDvsecOffset,
  OUT UINT16              *CxlDvsecId
  );

/**
  Search for a CXL DVSEC capability with the given CXL DVSEC ID starting at
  the given capability offset.

  When searching for multiple capabilities, pass in the NextCap
  offset from the previous call as the starting offset to continue the search.

  @param[in] PciIo       Pci Io Protocol.
  @param[in] CxlDvsecId  The CXL DVSEC ID to locate.
  @param[in] Offset      Starting offset on entry, found offset on success.
  @param[out] NextCap    Offset to the next capability.

  @retval EFI_SUCCESS      Successfully read the CXL DVSEC ID.
  @retval EFI_INVALID_PARAMETER PciIo is NULL or Offset is NULL.
  @retval EFI_NOT_FOUND    CXL DVSEC ID not found.
  @retval Others           Other error.

**/
EFI_STATUS
EFIAPI
CxlLibLocateCxlDvsecId (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT16               CxlDvsecId,
  IN OUT UINT32           *Offset,
  OUT UINT32              *NextCap OPTIONAL
  );

/**
  Search for CXL DVSEC capabilities with the given CXL DVSEC IDs starting at
  the given capability offset.

  Note: if multiple capabilities with the same CXL DVSEC ID are found, only
  the last one will be returned.

  Note: if a CXL DVSEC ID is specified multiple times in the list, only the
  first one will be used.

  Note: if an ID is not found, the offset for that ID will be unmodified.

  @param[in] PciIo       Pci Io Protocol.
  @param[in] Count       The number of different CXL DVSEC IDs to locate.
  @param[in] CxlDvsecId  The list of CXL DVSEC IDs to locate.
  @param[out] Offsets    The list of CXL DVSEC IDs offsets found.

  @retval EFI_SUCCESS      Successfully found at least one requested CXL DVSEC ID.
  @retval EFI_INVALID_PARAMETER PciIo is NULL, CxlDvsecIds is NULL or Count is 0.
  @retval EFI_NOT_FOUND    None of the requested CXL DVSEC IDs was found.
  @retval Others           Other error.

**/
EFI_STATUS
EFIAPI
CxlLibFindCxlDvsecIds (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               Count,
  IN UINT16               *CxlDvsecIds,
  OUT UINT32              *Offsets OPTIONAL
  );

/**
  Read the Port Status for the Flex Bus Port capability at the given offset.

  @param[in] PciIo       Pci Io Protocol.
  @param[in] FlexBusPortDvsecOffset  Offset to the Flex Bus Port capability.
  @param[out] PortStatus The Flex Bus Port Status.

  @retval EFI_SUCCESS      Successfully read the Flex Bus Port Status.
  @retval EFI_INVALID_PARAMETER PciIo is NULL or FlexBusPortDvsecOffset is 0.
  @retval Others           Other error.

**/
EFI_STATUS
EFIAPI
CxlLibGetPortStatus (
  IN EFI_PCI_IO_PROTOCOL                  *PciIo,
  IN UINT32                               FlexBusPortDvsecOffset,
  OUT CXL_3_2_DVSEC_FLEX_BUS_PORT_STATUS  *PortStatus
  );

#endif // __CXL_LIB_H__
