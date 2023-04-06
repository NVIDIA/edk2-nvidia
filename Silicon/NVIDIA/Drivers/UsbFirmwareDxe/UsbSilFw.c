/** @file

  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <PiDxe.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/FwImageLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <libfdt.h>

#include <Protocol/UsbFwProtocol.h>

#define USB_FW_IMAGE_NAME  L"xusb-fw"

NVIDIA_USBFW_PROTOCOL  mUsbFwData;

STATIC
BOOLEAN
EFIAPI
UsbFirmwarePlatformIsSupported (
  VOID
  )
{
  EFI_STATUS   Status;
  VOID         *Dtb;
  UINTN        DtbSize;
  CONST CHAR8  *Model;
  INT32        Length;

  Status = DtPlatformLoadDtb (&Dtb, &DtbSize);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  Model = (CONST CHAR8 *)fdt_getprop (Dtb, 0, "model", &Length);
  if ((Length > 0) && (Model != NULL)) {
    if (0 == AsciiStrCmp (Model, "e3360_1099")) {
      DEBUG ((EFI_D_ERROR, "%a: Xavier-SLT unsupported\r\n", __FUNCTION__));
      return FALSE;
    }
  }

  return TRUE;
}

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
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  CHAR8                     *UsbFwBuffer;
  NVIDIA_FW_IMAGE_PROTOCOL  *FwImage;
  FW_IMAGE_ATTRIBUTES       Attributes;

  if (!UsbFirmwarePlatformIsSupported ()) {
    return EFI_UNSUPPORTED;
  }

  FwImage = FwImageFindProtocol (USB_FW_IMAGE_NAME);
  if (FwImage == NULL) {
    DEBUG ((DEBUG_ERROR, "USB FW image %s not found\r\n", USB_FW_IMAGE_NAME));
    return EFI_NOT_FOUND;
  }

  Status = FwImage->GetAttributes (FwImage, &Attributes);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get %s attributes\r\n", USB_FW_IMAGE_NAME));
    return Status;
  }

  mUsbFwData.UsbFwSize = Attributes.Bytes;
  UsbFwBuffer          = AllocateZeroPool (mUsbFwData.UsbFwSize);
  mUsbFwData.UsbFwBase = UsbFwBuffer;
  Status               = FwImage->Read (FwImage, 0, mUsbFwData.UsbFwSize, mUsbFwData.UsbFwBase, FW_IMAGE_RW_FLAG_NONE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to read Partition\r\n"));
    return Status;
  }

  if (0 == AsciiStrnCmp (
             (CONST CHAR8 *)mUsbFwData.UsbFwBase,
             (CONST CHAR8 *)PcdGetPtr (PcdSignedImageHeaderSignature),
             sizeof (UINT32)
             ))
  {
    mUsbFwData.UsbFwSize -= PcdGet32 (PcdSignedImageHeaderSize);
    mUsbFwData.UsbFwBase  = UsbFwBuffer + PcdGet32 (PcdSignedImageHeaderSize);
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gNVIDIAUsbFwProtocolGuid,
                  (VOID *)&mUsbFwData,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install USB firmware protocol - %r\r\n", __FUNCTION__, Status));
  }

  return Status;
}
