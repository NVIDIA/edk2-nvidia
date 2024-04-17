/** @file
  Xudc Controller Protocol

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __XUDCCONTROLLER_PROTOCOL_H__
#define __XUDCCONTROLLER_PROTOCOL_H__

#include <Uefi/UefiSpec.h>
#include <IndustryStandard/Usb.h>

#define NVIDIA_XUDCCONTROLLER_PROTOCOL_GUID \
  { \
  0xefbaacf8, 0x4899, 0x45af, { 0x85, 0x74, 0xdc, 0x2e, 0x6a, 0x81, 0x9d, 0x33 } \
  }

typedef struct _NVIDIA_XUDCCONTROLLER_PROTOCOL NVIDIA_XUDCCONTROLLER_PROTOCOL;

/*
 * Note: This Protocol is just  the bare minimum for Android Fastboot. It
 * only makes sense for devices that only do Bulk Transfers and only have one
 * endpoint.
 */

/*
  Callback to be called when data is received.
  Buffer is callee-allocated and it's the caller's responsibility to free it with
  FreePool.

  @param[in] Size        Size in bytes of data.
  @param[in] Buffer      Pointer to data.
*/
typedef
VOID
(*XUDC_RX_CALLBACK) (
  IN UINTN  Size,
  IN VOID   *Buffer
  );

/*
  Callback to be called when the host asks for data by sending an IN token
  (excluding during the data stage of a control transfer).
  When this function is called, data previously buffered by calling Send() has
  been sent.

  @param[in]Endpoint    Endpoint index, as specified in endpoint descriptors, of
                        the endpoint the IN token was sent to.
*/
typedef
VOID
(*XUDC_TX_CALLBACK) (
  IN UINT8  EndpointIndex,
  IN UINTN  Size,
  IN VOID   *Buffer
  );

/*
  Put data in the Tx buffer to be sent on the next IN token.
  Don't call this function again until the TxCallback has been called.

  @param[in]Endpoint    Endpoint index, as specified in endpoint descriptors, of
                        the endpoint to send the data from.
  @param[in]Size        Size in bytes of data.
  @param[in]Buffer      Pointer to data.

  @retval EFI_SUCCESS           The data was queued successfully.
  @retval EFI_INVALID_PARAMETER There was an error sending the data.
*/
typedef
EFI_STATUS
(*XUDC_SEND) (
  IN       UINT8  EndpointIndex,
  IN       UINTN  Size,
  IN CONST VOID   *Buffer
  );

/*
  Restart the USB peripheral controller and respond to enumeration.

  @param[in] DeviceDescriptor   pointer to device descriptor
  @param[in] Descriptors        Array of pointers to buffers, where
                                Descriptors[n] contains the response to a
                                GET_DESCRIPTOR request for configuration n. From
                                USB Spec section 9.4.3:
                                "The first interface descriptor follows the
                                configuration descriptor. The endpoint
                                descriptors for the first interface follow the
                                first interface descriptor. If there are
                                additional interfaces, their interface
                                descriptor and endpoint descriptors follow the
                                first interfaceâ€™s endpoint descriptors".

                                The size of each buffer is the TotalLength
                                member of the Configuration Descriptor.

                                The size of the array is
                                DeviceDescriptor->NumConfigurations.
  @param[in]RxCallback          See USB_DEVICE_RX_CALLBACK
  @param[in]TxCallback          See USB_DEVICE_TX_CALLBACK
*/
typedef
EFI_STATUS
(*XUDC_START) (
  IN USB_DEVICE_DESCRIPTOR  *DeviceDescriptor,
  IN VOID                   **Descriptors,
  IN XUDC_RX_CALLBACK       RxCallback,
  IN XUDC_TX_CALLBACK       TxCallback
  );

/*
  Set the Total Rx length. Currently it is used in fastboot data mode.

  @param[in]Endpoint    Endpoint index, as specified in endpoint descriptors, of
                        the endpoint to receive the data from.
  @param[in]Size        Size in bytes of data.
*/
typedef
VOID
(*XUDC_SET_RX_LENGTH) (
  IN       UINT8  EndpointIndex,
  IN       UINTN  Size
  );

struct _NVIDIA_XUDCCONTROLLER_PROTOCOL {
  XUDC_START            XudcStart;
  XUDC_SEND             XudcSend;
  XUDC_SET_RX_LENGTH    XudcSetRxLength;
};

extern EFI_GUID  gNVIDIAXudcControllerProtocolGuid;

#endif
