/** @file

  Copyright (c) 2020-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UsbFirmwareLib.h>

#include <Protocol/UsbFwProtocol.h>

NVIDIA_USBFW_PROTOCOL mUsbFwData;

/**
  Entrypoint of USB Firmware Dxe.

  @param  ImageHandle
  @param  SystemTable

  @return EFI_SUCCESS
  @return EFI_LOAD_ERROR
  @return EFI_OUT_OF_RESOURCES

**/
EFI_STATUS
EFIAPI
UsbFirmwareDxeInitialize (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE  * SystemTable
  )
{
  UINTN      ChipID;

  ChipID = TegraGetChipID();
  if (ChipID != T234_CHIP_ID) {
    return EFI_SUCCESS;
  }

  if (PcdGetBool (PcdTegraUseProdUsbFw)) {
    mUsbFwData.UsbFwBase = xusb_sil_prod_fw;
    mUsbFwData.UsbFwSize = xusb_sil_prod_fw_len;
  } else {
    mUsbFwData.UsbFwBase = xusb_sil_rel_fw;
    mUsbFwData.UsbFwSize = xusb_sil_rel_fw_len;
  }

  return gBS->InstallMultipleProtocolInterfaces (&ImageHandle,
                                                 &gNVIDIAUsbFwProtocolGuid,
                                                 (VOID*)&mUsbFwData,
                                                 NULL);
}
