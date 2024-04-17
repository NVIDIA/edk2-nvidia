/** @file

  Tegra USB Device controller

  SPDX-FileCopyrightText: Copyright (c) 2023-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DmaLib.h>
#include <Library/BaseMemoryLib.h>
#include <IndustryStandard/Usb.h>
#include <Protocol/UsbDevice.h>
#include <Protocol/XudcController.h>

#define MAX_TFR_LENGTH  (64U * 1024U)

STATIC USB_DEVICE_RX_CALLBACK   mDataReceivedCallback;
STATIC USB_DEVICE_TX_CALLBACK   mDataSentCallback;
STATIC LIST_ENTRY               mTxPacketList;
NVIDIA_XUDCCONTROLLER_PROTOCOL  *mXudcControllerProtocol;

typedef struct _USB_TX_PACKET_LIST {
  LIST_ENTRY    Link;
  VOID          *Buffer;
  UINTN         BufferSize;
} USB_TX_PACKET_LIST;

// Call by low level driver when Rx data is received.
STATIC
VOID
TegraUsbDeviceDataReceived (
  IN UINTN  Size,
  IN VOID   *Buffer
  )
{
  /* Copy the data from low layer Dma buffer to new buffer */
  /* New buffer would be freed in Application, ex: AndroidFastbootApp */
  VOID  *buf = AllocatePool (Size);

  CopyMem (buf, Buffer, Size);
  mDataReceivedCallback (Size, buf);
}

// Call by low level driver when Tx data is sent.
STATIC
VOID
TegraUsbDeviceDataSent (
  IN UINT8  EndpointIndex,
  IN UINTN  Size,
  IN VOID   *Buffer
  )
{
  LIST_ENTRY  *ListEntry;

  DEBUG ((DEBUG_INFO, "TegraUsbDeviceDataSent %u, 0x%p\n", Size, Buffer));
  if (!IsListEmpty (&mTxPacketList)) {
    for (ListEntry = GetFirstNode (&mTxPacketList);
         !IsNull (&mTxPacketList, ListEntry);
         ListEntry = GetNextNode (&mTxPacketList, ListEntry))
    {
      USB_TX_PACKET_LIST  *Entry = (USB_TX_PACKET_LIST *)ListEntry;
      if ((Entry->Buffer == Buffer) && (Entry->BufferSize == Size)) {
        RemoveEntryList (&Entry->Link);
        DmaFreeBuffer (EFI_SIZE_TO_PAGES (Size), (VOID *)Entry->Buffer);
        FreePool (Entry);
        DEBUG ((DEBUG_INFO, "TegraUsbDeviceDataSent Free %u, 0x%p\n", Size, Buffer));
        break;
      }
    }
  }

  mDataSentCallback (EndpointIndex);
}

EFI_STATUS
EFIAPI
TegraUsbDeviceStart (
  IN USB_DEVICE_DESCRIPTOR   *DeviceDescriptor,
  IN VOID                    **Descriptors,
  IN USB_DEVICE_RX_CALLBACK  RxCallback,
  IN USB_DEVICE_TX_CALLBACK  TxCallback
  )
{
  UINT8                     *Ptr;
  EFI_STATUS                Status;
  USB_DEVICE_DESCRIPTOR     *mDeviceDescriptor;
  VOID                      *mDescriptors;
  USB_INTERFACE_DESCRIPTOR  *mInterfaceDescriptor;
  USB_CONFIG_DESCRIPTOR     *mConfigDescriptor;
  USB_ENDPOINT_DESCRIPTOR   *mEndpointDescriptors;

  ASSERT (DeviceDescriptor != NULL);
  ASSERT (Descriptors != NULL);
  ASSERT (Descriptors[0] != NULL);
  ASSERT (RxCallback != NULL);
  ASSERT (TxCallback != NULL);

  mDeviceDescriptor = DeviceDescriptor;
  mDescriptors      = Descriptors[0];

  // Right now we just support one configuration
  ASSERT (mDeviceDescriptor->NumConfigurations == 1);
  // ... and one interface
  mConfigDescriptor = (USB_CONFIG_DESCRIPTOR *)mDescriptors;
  ASSERT (mConfigDescriptor->NumInterfaces == 1);

  Ptr                  = ((UINT8 *)mDescriptors) + sizeof (USB_CONFIG_DESCRIPTOR);
  mInterfaceDescriptor = (USB_INTERFACE_DESCRIPTOR *)Ptr;
  Ptr                 += sizeof (USB_INTERFACE_DESCRIPTOR);

  mEndpointDescriptors = (USB_ENDPOINT_DESCRIPTOR *)Ptr;

  mDataReceivedCallback = RxCallback;
  mDataSentCallback     = TxCallback;

  InitializeListHead (&mTxPacketList);

  ////// Call to Xusb protocol for init.
  Status = mXudcControllerProtocol->XudcStart (DeviceDescriptor, Descriptors, TegraUsbDeviceDataReceived, TegraUsbDeviceDataSent);

  return Status;
}

// Build a Tx dma allocation list.
// It is used for free when data is sent.
STATIC
VOID
BuildUsbTxList (
  IN UINTN  Size,
  IN VOID   *Buffer
  )
{
  USB_TX_PACKET_LIST  *NewEntry;

  NewEntry = AllocatePool (sizeof (*NewEntry));
  ASSERT (NewEntry != NULL);

  NewEntry->Buffer     = Buffer;
  NewEntry->BufferSize = Size;
  DEBUG ((DEBUG_INFO, "BuildUsbTxList %u, 0x%p\n", Size, Buffer));
  InsertTailList (&mTxPacketList, &NewEntry->Link);
}

// Check if fastboot is in data download mode
STATIC
VOID
CheckUsbDownloadMode (
  IN        UINTN  Size,
  IN  CONST VOID   *Buffer
  )
{
  CHAR8  OutputString[512+1];

  AsciiStrnCpyS (OutputString, sizeof OutputString, Buffer, Size);

  if (AsciiStrnCmp (OutputString, "DATA", AsciiStrLen ("DATA")) == 0) {
    CHAR8   *CharBuf      = OutputString + AsciiStrLen ("DATA");
    UINT64  mNumDataBytes = AsciiStrHexToUint64 (CharBuf);
    DEBUG ((DEBUG_INFO, "Fastboot data mode DataBytes %lu\n", mNumDataBytes));

    // Set the Rx total length from fastboot response data
    mXudcControllerProtocol->XudcSetRxLength (1, (UINTN)mNumDataBytes);
  }
}

EFI_STATUS
TegraUsbDeviceSend (
  IN        UINT8  EndpointIndex,
  IN        UINTN  Size,
  IN  CONST VOID   *Buffer
  )
{
  EFI_STATUS   Status;
  UINT32       tfr_length;
  UINT8        *buf;
  CONST UINT8  *SrcPtr = Buffer;

  DEBUG ((DEBUG_INFO, "TegraUsbDeviceSend %u, 0x%p\n", Size, Buffer));

  if ((Buffer == NULL) || (Size == 0)) {
    Status = EFI_INVALID_PARAMETER;
    return Status;
  }

  // Check for fastboot Usb Download mode
  CheckUsbDownloadMode (Size, Buffer);

  /* It has chance that the buffer is not align for DMA device */
  /* Need allocate a DMA memory for it */
  while (Size != 0U) {
    if (Size > MAX_TFR_LENGTH) {
      tfr_length = MAX_TFR_LENGTH;
    } else {
      tfr_length = Size;
    }

    Status = DmaAllocateBuffer (EfiRuntimeServicesData, EFI_SIZE_TO_PAGES (tfr_length), (VOID **)&buf);
    if (Status != EFI_SUCCESS) {
      Status = EFI_PROTOCOL_ERROR;
      DEBUG ((EFI_D_ERROR, "%a: Error on allocate USB device Tx buffer\r\n", __FUNCTION__));
      return Status;
    }

    CopyMem ((VOID *)buf, (VOID *)SrcPtr, tfr_length);

    // Build a list for Tx dma allocation memory.
    // The list would be used for free when sent callback.
    BuildUsbTxList (tfr_length, buf);

    Status = mXudcControllerProtocol->XudcSend (EndpointIndex, tfr_length, buf);

    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "%a: XudcSend fail\r\n", __FUNCTION__));

      // Remove the entry from Tx dma list
      TegraUsbDeviceDataSent (1, tfr_length, buf);
      DmaFreeBuffer (EFI_SIZE_TO_PAGES (tfr_length), buf);
      return Status;
    }

    Size   = Size - tfr_length;
    buf    = NULL;
    SrcPtr = SrcPtr + tfr_length;
  }

  return Status;
}

USB_DEVICE_PROTOCOL  mTegraUsbDevice = {
  TegraUsbDeviceStart,
  TegraUsbDeviceSend
};

EFI_STATUS
TegraUsbDeviceControllerEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (
                  &gNVIDIAXudcControllerProtocolGuid,
                  NULL,
                  (VOID **)&mXudcControllerProtocol
                  );

  if (EFI_ERROR (Status) || (mXudcControllerProtocol == NULL)) {
    DEBUG ((
      EFI_D_ERROR,
      "%a: Couldn't get gNVIDIAXudcControllerProtocolGuid Handle: %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gUsbDeviceProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mTegraUsbDevice
                  );

  return Status;
}
