/** @file

  Renesas USB Controller

  SPDX-FileCopyrightText: Copyright (c) 2023-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Protocol/PciIo.h>

#include <IndustryStandard/Pci.h>

#include "RenesasUsbCtlr.h"

#define PCI_READ16(PciIo, Reg, Val)                                      \
  do {                                                                   \
    EFI_STATUS  Status;                                                  \
    Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16, Reg, 1, Val);  \
    if (EFI_ERROR (Status)) {                                            \
      return EFI_DEVICE_ERROR;                                           \
    }                                                                    \
  } while (FALSE)

#define PCI_WRITE16(PciIo, Reg, Val)                                     \
  do {                                                                   \
    EFI_STATUS  Status;                                                  \
    Status = PciIo->Pci.Write (PciIo, EfiPciIoWidthUint16, Reg, 1, Val); \
    if (EFI_ERROR (Status)) {                                            \
      return EFI_DEVICE_ERROR;                                           \
    }                                                                    \
  } while (FALSE)

#define PCI_READ32(PciIo, Reg, Val)                                      \
  do {                                                                   \
    EFI_STATUS  Status;                                                  \
    Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, Reg, 1, Val);  \
    if (EFI_ERROR (Status)) {                                            \
      return EFI_DEVICE_ERROR;                                           \
    }                                                                    \
  } while (FALSE)

#define PCI_WRITE32(PciIo, Reg, Val)                                     \
  do {                                                                   \
    EFI_STATUS  Status;                                                  \
    Status = PciIo->Pci.Write (PciIo, EfiPciIoWidthUint32, Reg, 1, Val); \
    if (EFI_ERROR (Status)) {                                            \
      return EFI_DEVICE_ERROR;                                           \
    }                                                                    \
  } while (FALSE)

/**
  Wait for bits to be set or cleared

  @param[in]  PciIo          PCI IO protocol
  @param[in]  RegisterOffset PCI register offset
  @param[in]  SetMask        Bit mask of the bits expected to be set
  @param[in]  ClearMask      Bit mask of the bits expected to be cleared

  @retval EFI_SUCCESS        The bits are set/clear as expected
  @retval EFI_TIMEOUT        Timeout
**/
EFI_STATUS
PciWaitBits (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT16               RegisterOffset,
  IN UINT16               SetMask,
  IN UINT16               ClearMask
  )
{
  UINT16  RegValue;
  UINT32  Count;

  for (Count = 0; Count < FW_DL_TIMEOUT_US; Count++) {
    PCI_READ16 (PciIo, RegisterOffset, &RegValue);
    if (((RegValue & SetMask) == SetMask) && ((RegValue & ClearMask) == 0)) {
      return EFI_SUCCESS;
    }

    MicroSecondDelay (10);
  }

  DEBUG ((DEBUG_ERROR, "%a: Firmware upload timeout - %04x\n", __FUNCTION__, RegValue));
  return EFI_TIMEOUT;
}

/**
  Load firmware for Renesas USB controller uPD72020x

  @param[in]    PciIo          PCI IO protocol
  @param[in]    FirmwareBase   Point to uPD72020x firmware binary
  @param[in]    FirmwareSize   Size of uPD72020x firmware binary

  @retval EFI_SUCCESS   Firmware uploaded successfully.
  @retval other         Some errors occur when uploading firmware.
**/
EFI_STATUS
FirmwareUpload72020x (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               *FirmwareBase,
  IN UINTN                FirmwareSize
  )
{
  EFI_STATUS  Status;
  UINTN       Index;
  UINT16      ExtRomCtlSts;
  UINT16      FwDlCtlSts;
  UINT16      SetDataMsk;
  UINT16      DataReg;

  //
  // If external ROM is installed, no need to upload FW.
  //
  PCI_READ16 (PciIo, PCI_RENESAS_EXT_ROM_CTL_STS_REG, &ExtRomCtlSts);
  if ((ExtRomCtlSts & EXT_ROM_CTL_STS_EXT_ROM_EXISTS_MSK) != 0) {
    DEBUG ((DEBUG_WARN, "%a: External ROM exists. Skip upload.\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  //
  // If firmware download is locked, skip upload
  //
  PCI_READ16 (PciIo, PCI_RENESAS_FW_DL_CTL_STS_REG, &FwDlCtlSts);
  if ((FwDlCtlSts & FW_DL_CTL_STS_DOWNLOAD_LOCK_MSK) != 0) {
    DEBUG ((DEBUG_WARN, "%a: Firmware is locked. Skip upload.\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  //
  // Start firmware upload
  //
  FwDlCtlSts |= FW_DL_CTL_STS_DOWNLOAD_ENABLE_MSK;
  PCI_WRITE16 (PciIo, PCI_RENESAS_FW_DL_CTL_STS_REG, &FwDlCtlSts);

  //
  // Upload firmware
  //
  for (Index = 0; Index < (FirmwareSize / sizeof (UINT32)); Index++) {
    // The FW is written alternatively to 2 data registers
    if ((Index & 1) == 0) {
      SetDataMsk = FW_DL_CTL_STS_SET_DATA0_MSK;
      DataReg    = PCI_RENESAS_DATA0_REG;
    } else {
      SetDataMsk = FW_DL_CTL_STS_SET_DATA1_MSK;
      DataReg    = PCI_RENESAS_DATA1_REG;
    }

    // Wait till the previous write completes
    Status = PciWaitBits (PciIo, PCI_RENESAS_FW_DL_CTL_STS_REG, 0, SetDataMsk);
    if (EFI_ERROR (Status)) {
      break;
    }

    // Write data to register
    PCI_WRITE32 (PciIo, DataReg, &FirmwareBase[Index]);
    MicroSecondDelay (10);

    // Trigger the write
    // The first and second data must be written at once.
    PCI_READ16 (PciIo, PCI_RENESAS_FW_DL_CTL_STS_REG, &FwDlCtlSts);
    if (Index == 0) {
      continue;
    } else if (Index == 1) {
      FwDlCtlSts |= FW_DL_CTL_STS_SET_DATA0_MSK | FW_DL_CTL_STS_SET_DATA1_MSK;
    } else if (Index > 1) {
      FwDlCtlSts |= SetDataMsk;
    }

    PCI_WRITE16 (PciIo, PCI_RENESAS_FW_DL_CTL_STS_REG, &FwDlCtlSts);
  }

  // Wait till the writes complete
  SetDataMsk = FW_DL_CTL_STS_SET_DATA0_MSK | FW_DL_CTL_STS_SET_DATA1_MSK;
  Status     = PciWaitBits (PciIo, PCI_RENESAS_FW_DL_CTL_STS_REG, 0, SetDataMsk);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // End firmware upload
  //
  PCI_READ16 (PciIo, PCI_RENESAS_FW_DL_CTL_STS_REG, &FwDlCtlSts);
  FwDlCtlSts &= ~FW_DL_CTL_STS_DOWNLOAD_ENABLE_MSK;
  PCI_WRITE16 (PciIo, PCI_RENESAS_FW_DL_CTL_STS_REG, &FwDlCtlSts);

  //
  // Wait for result code to return '001b'
  //
  Status = PciWaitBits (
             PciIo,
             PCI_RENESAS_FW_DL_CTL_STS_REG,
             FW_DL_CTL_STS_RESULT_CODE_SUCCESS_SET_MSK,
             FW_DL_CTL_STS_RESULT_CODE_SUCCESS_CLEAR_MSK
             );

  DEBUG ((DEBUG_INFO, "%a: %r\n", __FUNCTION__, Status));
  return Status;
}

/**
  Callback after PCI enumeration completes.

k @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
VOID
EFIAPI
RenesasUsbCtlrCallback (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_HANDLE           *Handles = NULL;
  UINTN                HandleCount;
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  UINTN                Index;
  UINT16               VendorId;
  UINT16               DeviceId;
  UINT32               *UPD72020xFwBase;
  UINTN                UPD72020xFwSize;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  gBS->CloseEvent (Event);

  //
  // Get USB FW binary
  //
  Status = GetSectionFromFv (
             &gNVIDIAuPD72020xFirmwareGuid,
             EFI_SECTION_RAW,
             0,
             (VOID **)&UPD72020xFwBase,
             &UPD72020xFwSize
             );
  if (EFI_ERROR (Status) || (UPD72020xFwBase == NULL)) {
    DEBUG ((DEBUG_WARN, "%a: Firmware image for uPD72020x not found.\n", __FUNCTION__));
    FreePool (Handles);
    return;
  }

  //
  // Check and load FW for Renesas USB controllers if present.
  //
  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gEfiPciIoProtocolGuid,
                    (VOID **)&PciIo
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16, PCI_VENDOR_ID_OFFSET, 1, &VendorId);
    ASSERT_EFI_ERROR (Status);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (VendorId != PCI_VENDOR_ID_RENESAS) {
      continue;
    }

    Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16, PCI_DEVICE_ID_OFFSET, 1, &DeviceId);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if ((DeviceId == PCI_DEVICE_ID_UPD720201) || (DeviceId == PCI_DEVICE_ID_UPD720202)) {
      Status = FirmwareUpload72020x (PciIo, UPD72020xFwBase, UPD72020xFwSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Fail to load firmware for uPD72020x\n", __FUNCTION__));
      }
    }
  }

  FreePool (Handles);
  FreePool (UPD72020xFwBase);
}

/**
  Entry Point for module RenesasUsbCtlr.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some errors occur when executing this entry point.
**/
EFI_STATUS
EFIAPI
RenesasUsbCtlrEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   Event;
  VOID        *Registration;
  UINT32      LoadRenesasUsbFw;
  UINTN       BufferSize;

  //
  // Loading Renesas firmware is controlled dynamically by variable "LoadRenesasUsbFw"
  //
  BufferSize = sizeof (LoadRenesasUsbFw);
  Status     = gRT->GetVariable (
                      L"LoadRenesasUsbFw",
                      &gNVIDIAPublicVariableGuid,
                      NULL,
                      &BufferSize,
                      &LoadRenesasUsbFw
                      );
  if (EFI_ERROR (Status) || (LoadRenesasUsbFw == 0)) {
    DEBUG ((DEBUG_INFO, "%a: No request to load Renesas USB firmware.\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  //
  // Register a callback at the end of PCI enumeration
  //
  Event = EfiCreateProtocolNotifyEvent (
            &gEfiPciEnumerationCompleteProtocolGuid,
            TPL_CALLBACK,
            RenesasUsbCtlrCallback,
            NULL,
            &Registration
            );
  if (Event == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to create callback\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  return EFI_SUCCESS;
}
