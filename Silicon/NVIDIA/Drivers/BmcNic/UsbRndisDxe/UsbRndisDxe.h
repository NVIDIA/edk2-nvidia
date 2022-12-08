/** @file
  Definition of USB RNDIS driver.

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

 **/

#ifndef USB_RDIS_DXE_H_
#define USB_RDIS_DXE_H_

#include <Uefi.h>

//
// Include Protocols that are consumed
//
#include <Protocol/SimpleNetwork.h>
#include <Protocol/UsbIo.h>
#include <Protocol/DevicePath.h>

//
// Include Protocols that are produced
//

//
// Include GUIDs that are consumed
//

//
// Include Library Classes commonly used by UEFI Drivers
//
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/NetLib.h>

//
// Include additional Library Classes that are used
//

//
// Define driver version Driver Binding Protocol
//
#define USB_RNDIS_VERSION  0x0A

//
// Private Context Data Structure
//
#define USB_RNDIS_PRIVATE_DATA_SIGNATURE  SIGNATURE_32 ('R','N','D','S')
#define USB_QUEUE_NODE_SIGNATURE          SIGNATURE_32 ('Q','N','O','D')

//
// Queue node
//
typedef struct {
  UINTN         Signature;
  LIST_ENTRY    Link;
  UINT8         *Buffer;
  UINTN         BufferSize;
} USB_QUEUE_NODE;

#define USB_QUEUE_NODE_FROM_LINK(a) \
  CR(                        \
    a,                       \
    USB_QUEUE_NODE,          \
    Link,             \
    USB_QUEUE_NODE_SIGNATURE \
    )

//
// USB endpoint data
//
typedef struct {
  UINT8    BulkIn;
  UINT8    BulkOut;
  UINT8    Interrupt;
} USB_ENDPOINT_DATA;

//
// USB private data
//
typedef struct {
  USB_ENDPOINT_DATA    EndPoint;
  UINT32               RequestId;
  UINT32               MediaStatus;
  UINT32               LinkSpeed;
  UINT32               MaxFrameSize;
  UINT32               Filter;
  UINT32               Medium;
  UINT32               MaxPacketsPerTransfer;
  UINT32               MaxTransferSize;
  UINT32               PacketAlignmentFactor;
  EFI_MAC_ADDRESS      PermanentAddress;
  EFI_MAC_ADDRESS      CurrentAddress;
  LIST_ENTRY           ReceiveQueue;
  UINTN                QueueCount;
} USB_PRIVATE_DATA;

//
// Driver private data
//
typedef struct {
  UINTN                          Signature;
  UINT32                         Id;
  EFI_HANDLE                     Controller;
  EFI_HANDLE                     ControllerData;
  EFI_HANDLE                     Handle;

  //
  // Pointers to consumed protocols
  //
  EFI_USB_IO_PROTOCOL            *UsbIoProtocol;
  EFI_USB_IO_PROTOCOL            *UsbIoDataProtocol;
  EFI_DEVICE_PATH_PROTOCOL       *DevicePathProtocol;

  //
  // Pointers to produced protocols
  //
  EFI_SIMPLE_NETWORK_PROTOCOL    SnpProtocol;

  //
  // Private functions and data fields
  //
  EFI_SIMPLE_NETWORK_MODE        SnpModeData;
  USB_PRIVATE_DATA               UsbData;

  EFI_EVENT                      ReceiverTimer;
  UINTN                          ReceiverSlowPullCount;
} USB_RNDIS_PRIVATE_DATA;

#define USB_RNDIS_PRIVATE_DATA_FROM_SNP_THIS(a) \
  CR(                                      \
    a,                                     \
    USB_RNDIS_PRIVATE_DATA,          \
    SnpProtocol,                    \
    USB_RNDIS_PRIVATE_DATA_SIGNATURE \
    )

#define USB_RNDIS_PRIVATE_DATA_FROM_ID(a) \
  CR(                                      \
    a,                                     \
    USB_RNDIS_PRIVATE_DATA,          \
    Id,                    \
    USB_RNDIS_PRIVATE_DATA_SIGNATURE \
    )

//
// Useful macro to release buffer while it is not NULL
//
#define FREE_NON_NULL(a) \
  if ((a) != NULL) { \
    FreePool ((a));  \
    (a) = NULL;      \
  }

#endif
