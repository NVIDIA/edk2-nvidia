/** @file

  Pcie Library header

  SPDX-FileCopyrightText: Copyright (c) 2025-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PCIE_LIB_H__
#define __PCIE_LIB_H__

#include <Uefi.h>

#include <Protocol/PciIo.h>

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

#endif // __PCIE_LIB_H__
