/** @file

  Copyright (c) 2020, NVIDIA Corporation. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
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
  if (ChipID != T186_CHIP_ID) {
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
