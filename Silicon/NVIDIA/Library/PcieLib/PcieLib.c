/** @file

  Common routines for working with PCI & PCIE

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/Pci.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcieLib.h>

//
// Functions that deal with PCI
//

/**
  Read the PCI header type field.

  @param[in] PciIo     Pci Io Protocol.
  @param[out] HeaderType The header type.

  @retval EFI_SUCCESS The header type was read successfully.
  @retval EFI_INVALID_PARAMETER PciIo or HeaderType is NULL.
  @retval Other       The header type could not be read.

**/
EFI_STATUS
EFIAPI
PcieLibGetHeaderType (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  OUT UINT8               *HeaderType
  )
{
  EFI_STATUS  Status;
  UINT8       PciHeaderType;

  if (PciIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (HeaderType == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Check if this is a device
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        PCI_HEADER_TYPE_OFFSET,
                        1,
                        &PciHeaderType
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: PCI Header Type read failed with error %r\n", __FUNCTION__, Status));
    return Status;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: PCI Header Type is 0x%x\n", __FUNCTION__, PciHeaderType));
  *HeaderType = PciHeaderType;
  return EFI_SUCCESS;
}

/**
  Check if the PCI header type matches the given type.

  @param[in] PciIo       Pci Io Protocol.
  @param[in] HeaderType  The header type to check for.

  @retval TRUE         Header is of the given type.
  @retval FALSE        Header is not of the given type, or could not be read.

**/
BOOLEAN
PcieLibIsHeaderType (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT8                HeaderType
  )
{
  EFI_STATUS  Status;
  UINT8       PciHeaderType;

  if (PciIo == NULL) {
    return FALSE;
  }

  // Check if this is a device
  Status = PcieLibGetHeaderType (PciIo, &PciHeaderType);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: PCI Header Type read failed with error %r\n", __FUNCTION__, Status));
    return FALSE;
  }

  PciHeaderType &= HEADER_LAYOUT_CODE;
  if (PciHeaderType == HeaderType) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/**
  Read the PCI class code field into the supplied array.

  @param[in] PciIo     Pci Io Protocol.
  @param[out] Class    Pointer to the class code array to populate.

  @retval EFI_SUCCESS The header type was read successfully.
  @retval Other       The header type could not be read.

**/
EFI_STATUS
EFIAPI
PcieLibGetClassCode (
  IN  EFI_PCI_IO_PROTOCOL  *PciIo,
  OUT UINT8 (*Class)[3]
  )
{
  EFI_STATUS  Status;
  UINT8       ClassCode[3];

  // Read the PCIE Header Class Code
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        PCI_CLASSCODE_OFFSET,
                        sizeof (ClassCode),
                        ClassCode
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to read PCIE Header Class Code: %r\n", __FUNCTION__, Status));
    return Status;
  }

  CopyMem (Class, ClassCode, sizeof (ClassCode));
  return EFI_SUCCESS;
}

/**
  Check if the PCI class code matches the given class code.

  @param[in] PciIo       Pci Io Protocol.
  @param[in] Class       Pointer to the class code array to check against.

  @retval TRUE           Class code matches.
  @retval FALSE          Class code does not match, could not be read, or an input is NULL.

**/
BOOLEAN
PcieLibIsClassCode (
  IN  EFI_PCI_IO_PROTOCOL  *PciIo,
  IN  CONST UINT8 (*Class)[3]
  )
{
  EFI_STATUS  Status;
  UINT8       ClassCode[3];

  if (PciIo == NULL) {
    return FALSE;
  }

  if (Class == NULL) {
    return FALSE;
  }

  // Read the PCIE Header Class Code
  Status = PcieLibGetClassCode (PciIo, &ClassCode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get PCIE Header Class Code: %r\n", __FUNCTION__, Status));
    return FALSE;
  }

  DEBUG ((DEBUG_INFO, "%a: Checking PCIE Class Code: %02x:%02x:%02x\n", __FUNCTION__, ClassCode[2], ClassCode[1], ClassCode[0]));
  return (CompareMem (ClassCode, Class, sizeof (ClassCode)) == 0);
}

/**
 * Adapted from PciCommand.c
  Check if capability is supported.

  @param[in] PciIo     Pci Io Protocol

  @retval TRUE         Capability supported.
  @retval FALSE        Capability not supported or could not be read.

**/
BOOLEAN
PcieLibCapabilitySupport (
  IN EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  EFI_STATUS  Status;
  UINT8       Capability;

  if (PciIo == NULL) {
    return FALSE;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        PCI_PRIMARY_STATUS_OFFSET,
                        1,
                        &Capability
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: PCI Primary Status (Capability) read failed with error %r\n", __FUNCTION__, Status));
    return FALSE;
  }

  if ((Capability & EFI_PCI_STATUS_CAPABILITY) != 0) {
    return TRUE;
  }

  return FALSE;
}

/**
  Get the offset of the next PCI capability matching the given capability ID.

  When searching for multiple capabilities of the same ID, pass in the NextCap
  offset from the previous call as the starting offset.

  @param[in] PciIo         Pci Io Protocol.
  @param[in] CapId         The capability ID to locate.
  @param[inout] Offset     Starting offset on entry, found offset on success.
  @param[out] NextCap      Offset to the next capability.

  @retval EFI_SUCCESS      Successfully located capability register block.
  @retval EFI_UNSUPPORTED  Pci device does not support capability.
  @retval EFI_INVALID_PARAMETER PciIo is NULL or Offset is NULL
  @retval EFI_NOT_FOUND    Pci device support but can not find capability register block.

**/
EFI_STATUS
PcieLibLocatePciCapability (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT8                CapId,
  IN OUT UINT8            *Offset,
  OUT UINT8               *NextCap OPTIONAL
  )
{
  EFI_STATUS              Status;
  UINT8                   CapabilityOffset;
  EFI_PCI_CAPABILITY_HDR  CapabilityEntry;

  if (PciIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Offset == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (!PcieLibCapabilitySupport (PciIo)) {
    DEBUG ((DEBUG_ERROR, "%a: Capability not supported\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  if (*Offset != 0) {
    CapabilityOffset = *Offset;
  } else {
    CapabilityOffset = 0;
    Status           = PciIo->Pci.Read (
                                    PciIo,
                                    EfiPciIoWidthUint8,
                                    PCI_CAPBILITY_POINTER_OFFSET,
                                    1,
                                    &CapabilityOffset
                                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: PCI Capability Pointer read failed with error %r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  while ((CapabilityOffset >= 0x40) && ((CapabilityOffset & 0x03) == 0x00)) {
    PciIo->Pci.Read (
                 PciIo,
                 EfiPciIoWidthUint16,
                 CapabilityOffset,
                 1,
                 &CapabilityEntry
                 );

    if (CapabilityEntry.CapabilityID == CapId) {
      *Offset = CapabilityOffset;
      if (NextCap != NULL) {
        *NextCap = CapabilityEntry.NextItemPtr;
      }

      return EFI_SUCCESS;
    }

    /*
     * Certain PCI device may incorrectly have capability pointing to itself,
     * break to avoid dead loop.
     */
    if (CapabilityOffset == CapabilityEntry.NextItemPtr) {
      break;
    }

    CapabilityOffset = CapabilityEntry.NextItemPtr;
  }

  return EFI_NOT_FOUND;
}

/**
  Get the offset of the next PCI capability matching the given capability ID,
  using Cfg space MMIO rather than PciIo protocol.

  When searching for multiple capabilities of the same ID, pass in the NextCap
  offset from the previous call as the starting offset.

  @param[in] CfgBase       Config space base address.
  @param[in] CapId         The capability ID to locate.
  @param[inout] Offset     Starting offset on entry, found offset on success.
  @param[out] NextCap      Offset to the next capability.

  @retval EFI_SUCCESS      Successfully located capability register block.
  @retval EFI_UNSUPPORTED  Pci device does not support capability.
  @retval EFI_INVALID_PARAMETER CfgBase is 0 or Offset is NULL
  @retval EFI_NOT_FOUND    Pci device support but can not find capability register block.

**/
EFI_STATUS
PcieLibLocatePciCapabilityViaConfig (
  IN UINT64     CfgBase,
  IN UINT8      CapId,
  IN OUT UINT8  *Offset,
  OUT UINT8     *NextCap OPTIONAL
  )
{
  UINT8  CapabilityOffset;
  volatile union {
    EFI_PCI_CAPABILITY_HDR    Entry;
    UINT16                    Entry16;
  } Capability;

  if (CfgBase == 0) {
    DEBUG ((DEBUG_ERROR, "%a: CfgBase is 0\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  if (Offset == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Offset is NULL\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: CfgBase 0x%lx, Offset 0x%x\n", __FUNCTION__, CfgBase, *Offset));
  if (*Offset != 0) {
    CapabilityOffset = *Offset;
  } else {
    CapabilityOffset = MmioRead16 (CfgBase + PCI_CAPBILITY_POINTER_OFFSET) & 0x00FF;
  }

  Capability.Entry16 = 0;
  while ((CapabilityOffset >= 0x40) && ((CapabilityOffset & 0x03) == 0x00)) {
    Capability.Entry16 = MmioRead16 (CfgBase + CapabilityOffset);
    DEBUG ((DEBUG_VERBOSE, "%a: Offset 0x%x Capability %02x: ID 0x%x Next 0x%x\n", __FUNCTION__, CapabilityOffset, Capability.Entry16, Capability.Entry.CapabilityID, Capability.Entry.NextItemPtr));

    if (Capability.Entry.CapabilityID == CapId) {
      *Offset = CapabilityOffset;
      if (NextCap != NULL) {
        *NextCap = Capability.Entry.NextItemPtr;
      }

      return EFI_SUCCESS;
    }

    /*
     * Certain PCI device may incorrectly have capability pointing to itself,
     * break to avoid dead loop.
     */
    if (CapabilityOffset == Capability.Entry.NextItemPtr) {
      break;
    }

    CapabilityOffset = Capability.Entry.NextItemPtr;
  }

  return EFI_NOT_FOUND;
}

/**
  Get the Device Port Type of the Pcie device.

  @param[in] PciIo         Pci Io Protocol.
  @param[out] NextCap      The Device Port Type.

  @retval EFI_SUCCESS      Successfully read Device Port Type.
  @retval EFI_UNSUPPORTED  Pci device does not support capability.
  @retval EFI_INVALID_PARAMETER PciIo is NULL or DevicePortType is NULL.
  @retval Others           Other error.

**/
EFI_STATUS
EFIAPI
PcieLibGetDevicePortType (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  OUT UINT8               *DevicePortType
  )
{
  EFI_STATUS             Status;
  UINT8                  CapabilityOffset;
  PCI_CAPABILITY_PCIEXP  CapabilityBlock;

  if (PciIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (DevicePortType == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CapabilityOffset = 0;
  Status           = PcieLibLocatePciCapability (
                       PciIo,
                       EFI_PCI_CAPABILITY_ID_PCIEXP,
                       &CapabilityOffset,
                       NULL
                       );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint16,
                        CapabilityOffset + OFFSET_OF (PCI_CAPABILITY_PCIEXP, Capability),
                        1,
                        &CapabilityBlock.Capability
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((DEBUG_VERBOSE, "%a: DevicePortType is 0x%x\n", __FUNCTION__, CapabilityBlock.Capability.Bits.DevicePortType));

  *DevicePortType = CapabilityBlock.Capability.Bits.DevicePortType;
  return EFI_SUCCESS;
}

//
// Functions that deal with PCIE
//

/**
  Get the offset of the next PCIE capability matching the given capability ID.

  When searching for multiple capabilities of the same ID, pass in the NextCap
  offset from the previous call as the starting offset.

  @param[in] PciIo       Pci Io Protocol.
  @param[in] CapId       The capability ID to locate.
  @param[inout] Offset   Starting offset on entry, found offset on success.
  @param[out] NextCap    Offset to the next capability.

  @retval EFI_SUCCESS      Successfully located capability register block.
  @retval EFI_UNSUPPORTED  Pcie device does not support capability.
  @retval EFI_INVALID_PARAMETER PciIo is NULL or Offset is NULL.
  @retval EFI_NOT_FOUND    Pcie device support but can not find register block.

**/
EFI_STATUS
EFIAPI
PcieLibLocatePciExpressCapability (
  IN     EFI_PCI_IO_PROTOCOL  *PciIo,
  IN     UINT16               CapId,
  IN OUT UINT32               *Offset,
  OUT UINT32                  *NextCap OPTIONAL
  )
{
  EFI_STATUS                                Status;
  UINT32                                    CapabilityOffset;
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER  CapabilityEntry;
  UINTN                                     Segment, Bus, Device, Function;

  if (PciIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Offset == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (*Offset != 0) {
    CapabilityOffset = *Offset;
  } else {
    CapabilityOffset = EFI_PCIE_CAPABILITY_BASE_OFFSET;
  }

  while (CapabilityOffset != 0) {
    /* Mask it to DWORD alignment per PCI spec */
    CapabilityOffset &= 0xFFC;
    Status            = PciIo->Pci.Read (
                                     PciIo,
                                     EfiPciIoWidthUint32,
                                     CapabilityOffset,
                                     1,
                                     &CapabilityEntry
                                     );
    if (EFI_ERROR (Status)) {
      break;
    }

    if (*(UINT32 *)(&CapabilityEntry) == MAX_UINT32) {
      Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
      ASSERT_EFI_ERROR (Status);
      DEBUG ((
        DEBUG_WARN,
        "%a: [%04x:%02x:%02x.%x] failed to access config space at offset 0x%x\n",
        __FUNCTION__,
        Segment,
        Bus,
        Device,
        Function,
        CapabilityOffset
        ));
      break;
    }

    if (CapabilityEntry.CapabilityId == CapId) {
      *Offset = CapabilityOffset;
      if (NextCap != NULL) {
        *NextCap = CapabilityEntry.NextCapabilityOffset;
      }

      return EFI_SUCCESS;
    }

    CapabilityOffset = CapabilityEntry.NextCapabilityOffset;
  }

  return EFI_NOT_FOUND;
}
