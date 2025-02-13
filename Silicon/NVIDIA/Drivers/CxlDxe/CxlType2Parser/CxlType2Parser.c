/** @file
  CXL Type 2 parser.

  SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "CxlType2Parser.h"
#include "../CxlDxe.h"

#include <IndustryStandard/Cxl.h>
#include <IndustryStandard/Pci.h>

#include <Library/CxlLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcieLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/CxlEntityProtocol.h>

typedef enum {
  // Mandatory IDs
  DVSEC_CXL_DEVICE = 0,
  DVSEC_FLEX_BUS_PORT,
  DVSEC_REGISTER_LOCATOR,

  MANDATORY_IDS_COUNT,

  // Optional IDs
  DVSEC_GPF_CXL_DEVICE = MANDATORY_IDS_COUNT,

  TOTAL_IDS_COUNT
} TYPE2_DVSEC_OFFSET_INDEXES;
#define OPTIONAL_IDS_COUNT  (TOTAL_IDS_COUNT - MANDATORY_IDS_COUNT)

/**
  Function to parse a Type 2 CXL device.

  @param  [in]  Controller        The Controller handle of the device.
  @param  [in]  PciIo             The PciIo protocol of the device.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
CxlType2 (
  IN  EFI_HANDLE           Controller,
  IN  EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  EFI_STATUS                          Status;
  UINT16                              DvsecIds[MANDATORY_IDS_COUNT + OPTIONAL_IDS_COUNT] = {
    // Mandatory Offsets for Type 2 device
    [DVSEC_CXL_DEVICE]       = CXL_DVSEC_ID_PCIE_DVSEC_FOR_CXL_DEVICE,
    [DVSEC_FLEX_BUS_PORT]    = CXL_DVSEC_ID_PCIE_DVSEC_FOR_FLEX_BUS_PORT,
    [DVSEC_REGISTER_LOCATOR] = CXL_DVSEC_ID_REGISTER_LOCATOR,
    // Optional Offsets for Type 2 device
    [DVSEC_GPF_CXL_DEVICE] = CXL_DVSEC_ID_GPF_DVSEC_FOR_CXL_DEVICES
  };
  UINT32                              DvsecOffsets[MANDATORY_IDS_COUNT + OPTIONAL_IDS_COUNT] = { 0 };
  UINTN                               OffsetIndex;
  CXL_3_2_DVSEC_FLEX_BUS_PORT_STATUS  PortStatus;
  UINT8                               DevicePortType;
  CXL_ENTITY_PROTOCOL                 *CxlEntityProtocol;

  // Make sure PCIE header says this is a device
  if (!PcieLibIsHeaderType (PciIo, HEADER_TYPE_DEVICE)) {
    return EFI_UNSUPPORTED;
  }

  // Make sure the device is an endpoint
  Status = PcieLibGetDevicePortType (PciIo, &DevicePortType);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: Failed to get device port type: %r\n", __FUNCTION__, Status));
    return EFI_UNSUPPORTED;
  }

  if (DevicePortType != PCIE_DEVICE_PORT_TYPE_PCIE_ENDPOINT) {
    DEBUG ((DEBUG_INFO, "%a: Device is not an endpoint\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  // Make sure ClassCode is NOT for Type 3 device
  if (PcieLibIsClassCode (PciIo, &mCxlType3ClassCode)) {
    return EFI_UNSUPPORTED;
  }

  // Locate the CXL DVSEC capabilities
  Status = CxlLibFindCxlDvsecIds (PciIo, MANDATORY_IDS_COUNT + OPTIONAL_IDS_COUNT, DvsecIds, DvsecOffsets);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: Failed to find any required CXL DVSEC IDs: %r\n", __FUNCTION__, Status));
    return EFI_UNSUPPORTED;
  }

  // Make sure we found all the mandatory CXL DVSEC IDs
  for (OffsetIndex = 0; OffsetIndex < MANDATORY_IDS_COUNT; OffsetIndex++) {
    if (DvsecOffsets[OffsetIndex] == 0) {
      DEBUG ((DEBUG_INFO, "%a: Didn't find a CXL DVSEC offset for mandatory CXL DVSEC ID 0x%x\n", __FUNCTION__, DvsecIds[OffsetIndex]));
      return EFI_UNSUPPORTED;
    }
  }

  // Make sure we trained as CXL.mem AND CXL.cache
  Status = CxlLibGetPortStatus (PciIo, DvsecOffsets[DVSEC_FLEX_BUS_PORT], &PortStatus);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: Failed to get CXL port status: %r\n", __FUNCTION__, Status));
    return EFI_UNSUPPORTED;
  }

  if (!(PortStatus.Bits.CacheEnabled && PortStatus.Bits.MemEnabled && PortStatus.Bits.IoEnabled)) {
    DEBUG ((DEBUG_INFO, "%a: CXL port is not CXL enabled. Cache: %d Mem: %d IO: %d CXL68bFlitAndVh: %d\n", __FUNCTION__, PortStatus.Bits.CacheEnabled, PortStatus.Bits.MemEnabled, PortStatus.Bits.IoEnabled, PortStatus.Bits.Cxl68bFlitAndVhEnabled));
    return EFI_UNSUPPORTED;
  }

  DEBUG ((DEBUG_ERROR, "TODO: %a Parser still needs to be implemented\n", __FUNCTION__));

  // Register the protocol for this entity
  CxlEntityProtocol = AllocateZeroPool (sizeof (CXL_ENTITY_PROTOCOL));
  if (CxlEntityProtocol == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate CxlEntity protocol\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  CxlEntityProtocol->Handle                       = Controller;
  CxlEntityProtocol->Type                         = CXL_TYPE2_DEVICE;
  CxlEntityProtocol->PciIo                        = PciIo;
  CxlEntityProtocol->DvsecOffsets.Devices         = DvsecOffsets[DVSEC_CXL_DEVICE];
  CxlEntityProtocol->DvsecOffsets.FlexBus         = DvsecOffsets[DVSEC_FLEX_BUS_PORT];
  CxlEntityProtocol->DvsecOffsets.RegisterLocator = DvsecOffsets[DVSEC_REGISTER_LOCATOR];
  CxlEntityProtocol->DvsecOffsets.GpfForDevices   = DvsecOffsets[DVSEC_GPF_CXL_DEVICE];

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gNVIDIACxlEntityProtocolGuid,
                  CxlEntityProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r installing CxlEntity protocol\n", __FUNCTION__, Status));
    FreePool (CxlEntityProtocol);
    return Status;
  }

  return EFI_SUCCESS;
}

REGISTER_CXL_PARSER_FUNCTION (CxlType2, "skip-cxl-type2");
