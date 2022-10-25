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

#define QSPI_NUM_CHIP_SELECTS_JETSON  1
#define QSPI_NUM_CHIP_SELECTS_TH500   4

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
  UINT8                              NumChipSelects;
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

/**
  Get QSPI number of chip selects

  @param[in]  This                 Instance of protocol
  @param[out] NumChipSelects       Pointer to store number of chip selects

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
EFI_STATUS
EFIAPI
QspiControllerGetNumChipSelects (
  IN NVIDIA_QSPI_CONTROLLER_PROTOCOL  *This,
  OUT UINT8                           *NumChipSelects
  )
{
  QSPI_CONTROLLER_PRIVATE_DATA  *Private;

  Private         = QSPI_CONTROLLER_PRIVATE_DATA_FROM_PROTOCOL (This);
  *NumChipSelects = Private->NumChipSelects;

  return EFI_SUCCESS;
}

/**
  Detect Number of Chip Selects

  @retval UINT8                    Number of chip selects

**/
STATIC
UINT8
EFIAPI
DetectNumChipSelects (
  VOID
  )
{
  UINT8  NumChipSelects;

  if (IsOpteePresent ()) {
    NumChipSelects = QSPI_NUM_CHIP_SELECTS_JETSON;
  } else {
    NumChipSelects = QSPI_NUM_CHIP_SELECTS_TH500;
  }

  return NumChipSelects;
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
  UINT8                         NumChipSelects;
  TEGRA_BOOT_TYPE               TegraBootType;
  BOOLEAN                       Fbc;

  TegraBootType = GetBootType ();
  Fbc           = InFbc ();

  DEBUG ((DEBUG_ERROR, "Boot Type %d fbc %d\n", TegraBootType, Fbc));

  /* Fall back to emulated store as the QSPI resources
   * may not be setup.
   */
  if ((Fbc == FALSE) || (TegraBootType == TegrablBootRcm)) {
    DEBUG ((DEBUG_ERROR, "Not Initializing QSPI \n"));
    return EFI_SUCCESS;
  }

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

  NumChipSelects = DetectNumChipSelects ();

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
  Private->NumChipSelects      = NumChipSelects;

  Status = QspiInitialize (Private->QspiBaseAddress, NumChipSelects);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "QSPI Initialization Failed.\n"));
    goto ErrorExit;
  }

  Private->QspiControllerProtocol.PerformTransaction = QspiControllerPerformTransaction;
  Private->QspiControllerProtocol.GetNumChipSelects  = QspiControllerGetNumChipSelects;

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
