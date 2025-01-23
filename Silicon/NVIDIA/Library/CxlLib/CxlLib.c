/** @file

  Common routines for working with CXL

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Cxl.h>

#include <Library/CxlLib.h>
#include <Library/DebugLib.h>
#include <Library/PcieLib.h>

// Create a constant for parsers to compare against
// CXL Type 3 has Base Class Code (Memory controller 0x05), Sub Class Code (CXL memory device 0x02) and Programming Interface (0x10)
CONST UINT8  mCxlType3ClassCode[3] = {
  [2] = PCI_CLASS_MEMORY_CONTROLLER,
  [1] = PCI_CLASS_MEMORY_CXL,
  [0] = PCI_IF_MEMORY_CXL
};

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
  )
{
  EFI_STATUS                                                    Status;
  UINT32                                                        DvsecOffset;
  UINT32                                                        NextCapOffset;
  PCI_EXPRESS_EXTENDED_CAPABILITIES_DESIGNATED_VENDOR_SPECIFIC  DvsecHeader;

  if ((PciIo == NULL) || (Offset == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DvsecOffset = *Offset;

  do {
    //
    // Mask it to DWORD alignment per PCI spec
    //
    DvsecOffset &= 0xFFC;

    // Find the next DVSEC capability
    Status = PcieLibLocatePciExpressCapability (
               PciIo,
               PCI_EXPRESS_EXTENDED_CAPABILITY_DESIGNATED_VENDOR_SPECIFIC_ID,
               &DvsecOffset,
               &NextCapOffset
               );
    if (EFI_ERROR (Status)) {
      break;
    }

    // Check if it is for CXL vendor type
    Status = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint32,
                          DvsecOffset + OFFSET_OF (PCI_EXPRESS_EXTENDED_CAPABILITIES_DESIGNATED_VENDOR_SPECIFIC, DesignatedVendorSpecificHeader1),
                          1,
                          &DvsecHeader.DesignatedVendorSpecificHeader1
                          );
    if (EFI_ERROR (Status)) {
      break;
    }

    if (DvsecHeader.DesignatedVendorSpecificHeader1.Bits.DvsecVendorId == CXL_DVSEC_VENDOR_ID) {
      *Offset = DvsecOffset;
      if (NextCap != NULL) {
        *NextCap = NextCapOffset;
      }

      return EFI_SUCCESS;
    }

    DvsecOffset = NextCapOffset;
  } while (NextCapOffset != 0);

  return EFI_NOT_FOUND;
}

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
  )
{
  EFI_STATUS                                                    Status;
  PCI_EXPRESS_EXTENDED_CAPABILITIES_DESIGNATED_VENDOR_SPECIFIC  DvsecHeader;

  if ((PciIo == NULL) || (CxlDvsecId == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  // Check if it is the desired ID
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint16,
                        CxlDvsecOffset + OFFSET_OF (PCI_EXPRESS_EXTENDED_CAPABILITIES_DESIGNATED_VENDOR_SPECIFIC, DesignatedVendorSpecificHeader2),
                        1,
                        &DvsecHeader.DesignatedVendorSpecificHeader2
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *CxlDvsecId = DvsecHeader.DesignatedVendorSpecificHeader2.Bits.DvsecId;
  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS  Status;
  UINT32      DvsecOffset;
  UINT32      NextCapOffset;
  UINT16      DvsecId;

  if ((PciIo == NULL) || (Offset == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DvsecOffset = *Offset;

  do {
    //
    // Mask it to DWORD alignment per PCI spec
    //
    DvsecOffset &= 0xFFC;

    // Find the next CXL DVSEC capability

    Status = CxlLibLocateCxlDvsec (
               PciIo,
               &DvsecOffset,
               &NextCapOffset
               );
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = CxlLibGetCxlDvsecId (
               PciIo,
               DvsecOffset,
               &DvsecId
               );
    if (EFI_ERROR (Status)) {
      break;
    }

    if (DvsecId == CxlDvsecId) {
      *Offset = DvsecOffset;
      if (NextCap != NULL) {
        *NextCap = NextCapOffset;
      }

      return EFI_SUCCESS;
    }

    DvsecOffset = NextCapOffset;
  } while (NextCapOffset != 0);

  return EFI_NOT_FOUND;
}

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
  )
{
  EFI_STATUS  Status;
  UINT32      DvsecOffset;
  UINT32      NextCapOffset;
  UINT16      DvsecId;
  UINT32      IdsIndex;
  BOOLEAN     Found = FALSE;

  if (Count == 0) {
    return EFI_INVALID_PARAMETER;
  }

  if ((PciIo == NULL) || (CxlDvsecIds == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DvsecOffset = 0;

  do {
    //
    // Mask it to DWORD alignment per PCI spec
    //
    DvsecOffset &= 0xFFC;

    // Find the next CXL DVSEC capability

    Status = CxlLibLocateCxlDvsec (
               PciIo,
               &DvsecOffset,
               &NextCapOffset
               );
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = CxlLibGetCxlDvsecId (
               PciIo,
               DvsecOffset,
               &DvsecId
               );
    if (EFI_ERROR (Status)) {
      break;
    }

    for (IdsIndex = 0; IdsIndex < Count; IdsIndex++) {
      if (DvsecId == CxlDvsecIds[IdsIndex]) {
        DEBUG ((DEBUG_VERBOSE, "%a: Found CXL DVSEC ID %u for DVSEC at offset 0x%x\n", __FUNCTION__, CxlDvsecIds[IdsIndex], DvsecOffset));
        Found = TRUE;
        if (Offsets != NULL) {
          Offsets[IdsIndex] = DvsecOffset;
        }
      }
    }

    DvsecOffset = NextCapOffset;
  } while (NextCapOffset != 0);

  return Found ? EFI_SUCCESS : EFI_NOT_FOUND;
}

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
  )
{
  EFI_STATUS                   Status;
  CXL_1_1_DVSEC_FLEX_BUS_PORT  FlexBusPortDvsec;

  if ((PciIo == NULL) || (FlexBusPortDvsecOffset == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  // Check if it is the desired ID
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint16,
                        FlexBusPortDvsecOffset + OFFSET_OF (CXL_1_1_DVSEC_FLEX_BUS_PORT, PortStatus),
                        1,
                        &FlexBusPortDvsec.PortStatus
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PortStatus->Uint16 = FlexBusPortDvsec.PortStatus.Uint16;
  return EFI_SUCCESS;
}
