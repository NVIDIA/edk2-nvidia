/** @file

  QSPI Driver for Standalone MM image.

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/MmServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PlatformResourceLib.h>
#include <Protocol/QspiController.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>

#define QSPI_CONTROLLER_SIGNATURE  SIGNATURE_32('Q','S','P','I')

typedef enum {
  CONTROLLER_TYPE_QSPI,
  CONTROLLER_TYPE_SPI,
  CONTROLLER_TYPE_UNSUPPORTED
} QSPI_CONTROLLER_TYPE;

typedef struct {
  UINT32                             Signature;
  EFI_PHYSICAL_ADDRESS               QspiBaseAddress;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL    QspiControllerProtocol;
  EFI_EVENT                          VirtualAddrChangeEvent;
  BOOLEAN                            WaitCyclesSupported;
  QSPI_CONTROLLER_TYPE               ControllerType;
  UINT32                             ClockId;
} QSPI_CONTROLLER_PRIVATE_DATA;

#define QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL(a)  CR(a, QSPI_CONTROLLER_PRIVATE_DATA, QspiControllerProtocol, QSPI_CONTROLLER_SIGNATURE)

/**
  Perform a single transaction on QSPI bus.

  @param[in] This                  Instance of protocol
  @param[in] Packet                Transaction context

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
EFI_STATUS
EFIAPI
QspiControllerPerformTransaction (
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL  *This,
  IN QSPI_TRANSACTION_PACKET          *Packet
  )
{
  QSPI_CONTROLLER_PRIVATE_DATA  *Private;

  Private = QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL (This);

  if (!Private->WaitCyclesSupported && (Packet->WaitCycles != 0)) {
    return EFI_UNSUPPORTED;
  }

  return QspiPerformTransaction (Private->QspiBaseAddress, Packet);
}

EFI_STATUS
EFIAPI
QspiControllerStMmInitialize (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *MmSystemTable
  )
{
  EFI_STATUS                    Status;
  EFI_VIRTUAL_ADDRESS           QspiBaseAddress;
  UINTN                         QspiRegionSize;
  QSPI_CONTROLLER_PRIVATE_DATA  *Private;
  BOOLEAN                       WaitCyclesSupported;

  DEBUG ((DEBUG_ERROR, "%a: Looking for Dev Region with qspi", __FUNCTION__));

  Status = GetQspiDeviceRegion (&QspiBaseAddress, &QspiRegionSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to find any QSPI device region installed %r\n",
      __FUNCTION__,
      Status
      ));
    return EFI_SUCCESS;
  }

  Private = AllocateRuntimeZeroPool (sizeof (QSPI_CONTROLLER_PRIVATE_DATA));
  if (Private == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  WaitCyclesSupported          = TRUE;
  Private->Signature           = QSPI_CONTROLLER_SIGNATURE;
  Private->QspiBaseAddress     = QspiBaseAddress;
  Private->WaitCyclesSupported = WaitCyclesSupported;
  Private->ControllerType      = CONTROLLER_TYPE_QSPI;
  Private->ClockId             = MAX_UINT32;

  Status = QspiInitialize (Private->QspiBaseAddress);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "QSPI Initialization Failed.\n"));
    goto ErrorExit;
  }

  Private->QspiControllerProtocol.PerformTransaction = QspiControllerPerformTransaction;

  Status = gMmst->MmInstallProtocolInterface (
                    &ImageHandle,
                    &gNVIDIAQspiControllerProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    &Private->QspiControllerProtocol
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install QspiControllerProtocol \n", __FUNCTION__));
    goto ErrorExit;
  }

ErrorExit:
  return EFI_SUCCESS;
}
