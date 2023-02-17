/** @file
  Provides the Simple Network functions.

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Snp.h"
#include "Debug.h"
#include "Rndis.h"

/**
  Changes the state of a network interface from "stopped" to "started".

  @param[in]  This Protocol instance pointer.

  @retval EFI_SUCCESS           The network interface was started.
  @retval EFI_ALREADY_STARTED   The network interface is already in the started state.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
UsbRndisSnpStart (
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This
  )
{
  USB_RNDIS_PRIVATE_DATA  *Private;
  EFI_TPL                 TplPrevious;

  DEBUG ((USB_DEBUG_SNP_TRACE, "%a\n", __FUNCTION__));

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->Mode->State  == EfiSimpleNetworkStarted) {
    return EFI_ALREADY_STARTED;
  }

  Private = USB_RNDIS_PRIVATE_DATA_FROM_SNP_THIS (This);
  if (Private->DeviceLost) {
    return EFI_DEVICE_ERROR;
  }

  TplPrevious = gBS->RaiseTPL (TPL_CALLBACK);

  Private->SnpModeData.State = EfiSimpleNetworkStarted;

  gBS->RestoreTPL (TplPrevious);

  return EFI_SUCCESS;
}

/**
  Changes the state of a network interface from "started" to "stopped".

  @param[in]  This Protocol instance pointer.

  @retval EFI_SUCCESS           The network interface was stopped.
  @retval EFI_ALREADY_STARTED   The network interface is already in the stopped state.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
UsbRndisSnpStop (
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This
  )
{
  USB_RNDIS_PRIVATE_DATA  *Private;
  EFI_TPL                 TplPrevious;

  DEBUG ((USB_DEBUG_SNP_TRACE, "%a\n", __FUNCTION__));

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->Mode->State == EfiSimpleNetworkStopped) {
    return EFI_ALREADY_STARTED;
  }

  Private = USB_RNDIS_PRIVATE_DATA_FROM_SNP_THIS (This);
  if (Private->DeviceLost) {
    return EFI_DEVICE_ERROR;
  }

  TplPrevious = gBS->RaiseTPL (TPL_CALLBACK);

  Private->SnpModeData.State = EfiSimpleNetworkStopped;

  gBS->RestoreTPL (TplPrevious);

  return EFI_SUCCESS;
}

/**
  Resets a network adapter and allocates the transmit and receive buffers
  required by the network interface; optionally, also requests allocation
  of additional transmit and receive buffers.

  @param[in]  This              The protocol instance pointer.
  @param[in]  ExtraRxBufferSize The size, in bytes, of the extra receive buffer space
                                that the driver should allocate for the network interface.
                                Some network interfaces will not be able to use the extra
                                buffer, and the caller will not know if it is actually
                                being used.
  @param[in]  ExtraTxBufferSize The size, in bytes, of the extra transmit buffer space
                                that the driver should allocate for the network interface.
                                Some network interfaces will not be able to use the extra
                                buffer, and the caller will not know if it is actually
                                being used.

  @retval EFI_SUCCESS           The network interface was initialized.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory for the transmit and
                                receive buffers.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
UsbRndisSnpInitialize (
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN UINTN                        ExtraRxBufferSize  OPTIONAL,
  IN UINTN                        ExtraTxBufferSize  OPTIONAL
  )
{
  USB_RNDIS_PRIVATE_DATA  *Private;
  EFI_STATUS              Status;
  EFI_TPL                 TplPrevious;

  DEBUG ((USB_DEBUG_SNP_TRACE, "%a, ExtraRxBufferSize: 0x%x ExtraTxBufferSize: 0x%x\n", __FUNCTION__, ExtraRxBufferSize, ExtraTxBufferSize));

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((ExtraRxBufferSize != 0) || (ExtraTxBufferSize != 0)) {
    return EFI_UNSUPPORTED;
  }

  if (This->Mode->State != EfiSimpleNetworkStarted) {
    return EFI_NOT_STARTED;
  }

  Private = USB_RNDIS_PRIVATE_DATA_FROM_SNP_THIS (This);
  if (Private->DeviceLost) {
    return EFI_DEVICE_ERROR;
  }

  TplPrevious = gBS->RaiseTPL (TPL_CALLBACK);

  Status                     = UsbRndisInitialDevice (Private->UsbIoProtocol, USB_INCREASE_REQUEST_ID (Private->UsbData.RequestId), &Private->UsbData);
  Private->SnpModeData.State = EfiSimpleNetworkInitialized;

  gBS->RestoreTPL (TplPrevious);

  return Status;
}

/**
  Resets a network adapter and re-initializes it with the parameters that were
  provided in the previous call to Initialize().

  @param[in]  This                 The protocol instance pointer.
  @param[in]  ExtendedVerification Indicates that the driver may perform a more
                                   exhaustive verification operation of the device
                                   during reset.

  @retval EFI_SUCCESS           The network interface was reset.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
UsbRndisSnpReset (
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN BOOLEAN                      ExtendedVerification
  )
{
  USB_RNDIS_PRIVATE_DATA  *Private;
  EFI_STATUS              Status;
  EFI_TPL                 TplPrevious;

  DEBUG ((USB_DEBUG_SNP_TRACE, "%a\n", __FUNCTION__));

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->Mode->State != EfiSimpleNetworkInitialized) {
    return EFI_NOT_STARTED;
  }

  Private = USB_RNDIS_PRIVATE_DATA_FROM_SNP_THIS (This);
  if (Private->DeviceLost) {
    return EFI_DEVICE_ERROR;
  }

  TplPrevious = gBS->RaiseTPL (TPL_CALLBACK);

  Status = UsbRndisResetDevice (Private->UsbIoProtocol, USB_INCREASE_REQUEST_ID (Private->UsbData.RequestId));
  USB_RESET_REQUEST_ID (Private->UsbData.RequestId);
  Private->SnpModeData.State = EfiSimpleNetworkStopped;

  gBS->RestoreTPL (TplPrevious);

  return Status;
}

/**
  Resets a network adapter and leaves it in a state that is safe for
  another driver to initialize.

  @param[in]  This  Protocol instance pointer.

  @retval EFI_SUCCESS           The network interface was shutdown.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
UsbRndisSnpShutdown (
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This
  )
{
  USB_RNDIS_PRIVATE_DATA  *Private;
  EFI_STATUS              Status;
  EFI_TPL                 TplPrevious;

  DEBUG ((USB_DEBUG_SNP_TRACE, "%a\n", __FUNCTION__));

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->Mode->State != EfiSimpleNetworkInitialized) {
    return EFI_NOT_STARTED;
  }

  Private = USB_RNDIS_PRIVATE_DATA_FROM_SNP_THIS (This);
  if (Private->DeviceLost) {
    return EFI_DEVICE_ERROR;
  }

  TplPrevious = gBS->RaiseTPL (TPL_CALLBACK);

  Status = UsbRndisShutdownDevice (Private->UsbIoProtocol);
  USB_RESET_REQUEST_ID (Private->UsbData.RequestId);
  Private->SnpModeData.State = EfiSimpleNetworkStopped;

  gBS->RestoreTPL (TplPrevious);

  return Status;
}

/**
  Manages the multicast receive filters of a network interface.

  @param[in]  This             The protocol instance pointer.
  @param[in]  Enable           A bit mask of receive filters to enable on the network interface.
  @param[in]  Disable          A bit mask of receive filters to disable on the network interface.
  @param[in]  ResetMCastFilter Set to TRUE to reset the contents of the multicast receive
                              filters on the network interface to their default values.
  @param[in]  McastFilterCnt   Number of multicast HW MAC addresses in the new
                              MCastFilter list. This value must be less than or equal to
                              the MCastFilterCnt field of EFI_SIMPLE_NETWORK_MODE. This
                              field is optional if ResetMCastFilter is TRUE.
  @param[in]  MCastFilter      A pointer to a list of new multicast receive filter HW MAC
                              addresses. This list will replace any existing multicast
                              HW MAC address list. This field is optional if
                              ResetMCastFilter is TRUE.

  @retval EFI_SUCCESS           The multicast receive filter list was updated.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
UsbRndisSnpReceiveFilters (
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN UINT32                       Enable,
  IN UINT32                       Disable,
  IN BOOLEAN                      ResetMCastFilter,
  IN UINTN                        MCastFilterCnt     OPTIONAL,
  IN EFI_MAC_ADDRESS              *MCastFilter OPTIONAL
  )
{
  USB_RNDIS_PRIVATE_DATA  *Private;
  EFI_STATUS              Status;
  EFI_TPL                 TplPrevious;

  DEBUG ((USB_DEBUG_SNP_TRACE, "%a\n", __FUNCTION__));

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->Mode->State != EfiSimpleNetworkInitialized) {
    return EFI_NOT_STARTED;
  }

  Private = USB_RNDIS_PRIVATE_DATA_FROM_SNP_THIS (This);
  if (Private->DeviceLost) {
    return EFI_DEVICE_ERROR;
  }

  TplPrevious = gBS->RaiseTPL (TPL_CALLBACK);

  //
  // check if we are asked to enable or disable something that the SNP
  // does not even support!
  //
  if (((Enable &~Private->SnpModeData.ReceiveFilterMask) != 0) ||
      ((Disable &~Private->SnpModeData.ReceiveFilterMask) != 0))
  {
    gBS->RestoreTPL (TplPrevious);
    return EFI_INVALID_PARAMETER;
  }

  Private->SnpModeData.ReceiveFilterSetting |= Enable;
  Private->SnpModeData.ReceiveFilterSetting &= ~Disable;

  //
  // Set OID_GEN_CURRENT_PACKET_FILTER to start data transmission
  //
  Private->UsbData.Filter = (NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST | NDIS_PACKET_TYPE_BROADCAST);
  Status                  = RndisSetMessage (
                              Private->UsbIoProtocol,
                              USB_INCREASE_REQUEST_ID (Private->UsbData.RequestId),
                              OID_GEN_CURRENT_PACKET_FILTER,
                              sizeof (Private->UsbData.Filter),
                              (UINT8 *)&Private->UsbData.Filter
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, RndisSetMessage OID_GEN_CURRENT_PACKET_FILTER failed: %r\n", __FUNCTION__, Status));
  }

  gBS->RestoreTPL (TplPrevious);

  return Status;
}

/**
  Modifies or resets the current station address, if supported.

  @param[in]  This  The protocol instance pointer.
  @param[in]  Reset Flag used to reset the station address to the network interfaces
                    permanent address.
  @param[in]  New   The new station address to be used for the network interface.

  @retval EFI_SUCCESS           The network interfaces station address was updated.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
UsbRndisSnpStationAddress (
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN BOOLEAN                      Reset,
  IN EFI_MAC_ADDRESS              *New OPTIONAL
  )
{
  DEBUG ((USB_DEBUG_SNP_TRACE, "%a\n", __FUNCTION__));

  return EFI_UNSUPPORTED;
}

/**
  Resets or collects the statistics on a network interface.

  @param[in]  This            Protocol instance pointer.
  @param[in]  Reset           Set to TRUE to reset the statistics for the network interface.
  @param[in]  StatisticsSize  On input the size, in bytes, of StatisticsTable. On
                              output the size, in bytes, of the resulting table of
                              statistics.
  @param[out] StatisticsTable A pointer to the EFI_NETWORK_STATISTICS structure that
                              contains the statistics.

  @retval EFI_SUCCESS           The statistics were collected from the network interface.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_BUFFER_TOO_SMALL  The Statistics buffer was too small. The current buffer
                                size needed to hold the statistics is returned in
                                StatisticsSize.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
UsbRndisSnpStatistics (
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN BOOLEAN                      Reset,
  IN OUT UINTN                    *StatisticsSize   OPTIONAL,
  OUT EFI_NETWORK_STATISTICS      *StatisticsTable  OPTIONAL
  )
{
  DEBUG ((USB_DEBUG_SNP_TRACE, "%a\n", __FUNCTION__));

  return EFI_UNSUPPORTED;
}

/**
  Converts a multicast IP address to a multicast HW MAC address.

  @param[in]  This The protocol instance pointer.
  @param[in]  IPv6 Set to TRUE if the multicast IP address is IPv6 [RFC 2460]. Set
                   to FALSE if the multicast IP address is IPv4 [RFC 791].
  @param[in]  IP   The multicast IP address that is to be converted to a multicast
                   HW MAC address.
  @param[out] MAC  The multicast HW MAC address that is to be generated from IP.

  @retval EFI_SUCCESS           The multicast IP address was mapped to the multicast
                                HW MAC address.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_BUFFER_TOO_SMALL  The Statistics buffer was too small. The current buffer
                                size needed to hold the statistics is returned in
                                StatisticsSize.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
UsbRndisSnpMcastIpToMac (
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN BOOLEAN                      IPv6,
  IN EFI_IP_ADDRESS               *IP,
  OUT EFI_MAC_ADDRESS             *MAC
  )
{
  DEBUG ((USB_DEBUG_SNP_TRACE, "%a\n", __FUNCTION__));

  return EFI_UNSUPPORTED;
}

/**
  Performs read and write operations on the NVRAM device attached to a
  network interface.

  @param[in]  This        The protocol instance pointer.
  @param[in]  ReadWrite   TRUE for read operations, FALSE for write operations.
  @param[in]  Offset      Byte offset in the NVRAM device at which to start the read or
                          write operation. This must be a multiple of NvRamAccessSize and
                          less than NvRamSize.
  @param[in]  BufferSize  The number of bytes to read or write from the NVRAM device.
                          This must also be a multiple of NvramAccessSize.
  @param[in,out]  Buffer  A pointer to the data buffer.

  @retval EFI_SUCCESS           The NVRAM access was performed.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
UsbRndisSnpNvData (
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN BOOLEAN                      ReadWrite,
  IN UINTN                        Offset,
  IN UINTN                        BufferSize,
  IN OUT VOID                     *Buffer
  )
{
  DEBUG ((USB_DEBUG_SNP_TRACE, "%a\n", __FUNCTION__));

  return EFI_UNSUPPORTED;
}

/**
  Reads the current interrupt status and recycled transmit buffer status from
  a network interface.

  @param[in]  This            The protocol instance pointer.
  @param[out] InterruptStatus A pointer to the bit mask of the currently active interrupts
                              If this is NULL, the interrupt status will not be read from
                              the device. If this is not NULL, the interrupt status will
                              be read from the device. When the  interrupt status is read,
                              it will also be cleared. Clearing the transmit  interrupt
                              does not empty the recycled transmit buffer array.
  @param[out] TxBuf           Recycled transmit buffer address. The network interface will
                              not transmit if its internal recycled transmit buffer array
                              is full. Reading the transmit buffer does not clear the
                              transmit interrupt. If this is NULL, then the transmit buffer
                              status will not be read. If there are no transmit buffers to
                              recycle and TxBuf is not NULL, * TxBuf will be set to NULL.

  @retval EFI_SUCCESS           The status of the network interface was retrieved.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
UsbRndisSnpGetSatus (
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  OUT UINT32                      *InterruptStatus OPTIONAL,
  OUT VOID                        **TxBuf OPTIONAL
  )
{
  USB_RNDIS_PRIVATE_DATA  *Private;
  EFI_TPL                 TplPrevious;

  DEBUG ((USB_DEBUG_SNP_TRACE, "%a\n", __FUNCTION__));

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->Mode->State != EfiSimpleNetworkInitialized) {
    return EFI_NOT_STARTED;
  }

  Private = USB_RNDIS_PRIVATE_DATA_FROM_SNP_THIS (This);
  if (Private->DeviceLost) {
    return EFI_DEVICE_ERROR;
  }

  TplPrevious = gBS->RaiseTPL (TPL_CALLBACK);

  if (InterruptStatus != NULL) {
    *InterruptStatus = 0;
  }

  if (TxBuf != NULL) {
    *TxBuf = NULL;
  }

  Private->SnpProtocol.Mode->MediaPresent = (Private->UsbData.MediaStatus == RNDIS_MEDIA_STATE_CONNECTED ? TRUE : FALSE);

  DEBUG ((USB_DEBUG_SNP_TRACE, "%a, MediaPresent: 0x%x\n", __FUNCTION__, Private->SnpProtocol.Mode->MediaPresent));

  gBS->RestoreTPL (TplPrevious);

  return EFI_SUCCESS;
}

/**
  Places a packet in the transmit queue of a network interface.

  @param[in]  This       The protocol instance pointer.
  @param[in]  HeaderSize The size, in bytes, of the media header to be filled in by
                         the Transmit() function. If HeaderSize is non-zero, then it
                         must be equal to This->Mode->MediaHeaderSize and the DestAddr
                         and Protocol parameters must not be NULL.
  @param[in]  BufferSize The size, in bytes, of the entire packet (media header and
                         data) to be transmitted through the network interface.
  @param[in]  Buffer     A pointer to the packet (media header followed by data) to be
                         transmitted. This parameter cannot be NULL. If HeaderSize is zero,
                         then the media header in Buffer must already be filled in by the
                         caller. If HeaderSize is non-zero, then the media header will be
                         filled in by the Transmit() function.
  @param[in]  SrcAddr    The source HW MAC address. If HeaderSize is zero, then this parameter
                         is ignored. If HeaderSize is non-zero and SrcAddr is NULL, then
                         This->Mode->CurrentAddress is used for the source HW MAC address.
  @param[in]  DestAddr   The destination HW MAC address. If HeaderSize is zero, then this
                         parameter is ignored.
  @param[in]  Protocol   The type of header to build. If HeaderSize is zero, then this
                         parameter is ignored. See RFC 1700, section "Ether Types", for
                         examples.

  @retval EFI_SUCCESS           The packet was placed on the transmit queue.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_NOT_READY         The network interface is too busy to accept this transmit request.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize parameter is too small.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
UsbRndisSnpTransmit (
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN UINTN                        HeaderSize,
  IN UINTN                        BufferSize,
  IN VOID                         *Buffer,
  IN EFI_MAC_ADDRESS              *SrcAddr  OPTIONAL,
  IN EFI_MAC_ADDRESS              *DestAddr OPTIONAL,
  IN UINT16                       *Protocol OPTIONAL
  )
{
  USB_RNDIS_PRIVATE_DATA  *Private;
  ETHERNET_HEADER         *EhternetHeader;
  RNDIS_PACKET_MSG_DATA   *RndisPacketMsg;
  UINTN                   Length;
  EFI_TPL                 TplPrevious;
  UINT8                   *DataPointer;
  EFI_STATUS              Status;

  DEBUG ((USB_DEBUG_SNP_TRACE, "%a\n", __FUNCTION__));

  if ((This == NULL) || (BufferSize == 0) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->Mode->State != EfiSimpleNetworkInitialized) {
    return EFI_NOT_STARTED;
  }

  if (BufferSize < This->Mode->MediaHeaderSize) {
    return EFI_BUFFER_TOO_SMALL;
  }

  if (((HeaderSize != 0) && (DestAddr == NULL)) || ((HeaderSize != 0) && (HeaderSize != This->Mode->MediaHeaderSize)) || (BufferSize < HeaderSize)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = USB_RNDIS_PRIVATE_DATA_FROM_SNP_THIS (This);
  if (Private->DeviceLost) {
    return EFI_DEVICE_ERROR;
  }

  if (BufferSize > Private->UsbData.MaxTransferSize) {
    DEBUG ((DEBUG_ERROR, "%a, buffer size exceeds Max Transfer Size: (%d/%d)\n", __FUNCTION__, BufferSize, Private->UsbData.MaxTransferSize));
    return EFI_UNSUPPORTED;
  } else if (BufferSize > Private->UsbData.MaxFrameSize) {
    DEBUG ((DEBUG_ERROR, "%a, buffer size exceeds MTU: (%d/%d)\n", __FUNCTION__, BufferSize, Private->UsbData.MaxFrameSize));
  }

  DEBUG_CODE_BEGIN ();
  DEBUG ((USB_DEBUG_SNP, "%a, HeaderSize: %d BufferSize: %d\n", __FUNCTION__, HeaderSize, BufferSize));
  if (SrcAddr != NULL) {
    DEBUG ((USB_DEBUG_SNP, "%a, SrcAddr: 0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x\n", __FUNCTION__, SrcAddr->Addr[0], SrcAddr->Addr[1], SrcAddr->Addr[2], SrcAddr->Addr[3], SrcAddr->Addr[4], SrcAddr->Addr[5]));
  }

  if (DestAddr != NULL) {
    DEBUG ((USB_DEBUG_SNP, "%a, DestAddr: 0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x\n", __FUNCTION__, DestAddr->Addr[0], DestAddr->Addr[1], DestAddr->Addr[2], DestAddr->Addr[3], DestAddr->Addr[4], DestAddr->Addr[5]));
  }

  if (Protocol != NULL) {
    DEBUG ((USB_DEBUG_SNP, "%a, Protocol: 0x%x\n", __FUNCTION__, *Protocol));
  }

  DEBUG_CODE_END ();

  TplPrevious = gBS->RaiseTPL (TPL_CALLBACK);

  //
  // Fill the media header in buffer
  //
  if (HeaderSize > 0) {
    EhternetHeader = (ETHERNET_HEADER *)Buffer;
    if (SrcAddr != NULL) {
      CopyMem (&EhternetHeader->SrcAddr, SrcAddr, Private->SnpModeData.HwAddressSize);
    } else {
      CopyMem (&EhternetHeader->SrcAddr, &Private->SnpModeData.CurrentAddress.Addr[0], Private->SnpModeData.HwAddressSize);
    }

    CopyMem (&EhternetHeader->DstAddr, DestAddr, Private->SnpModeData.HwAddressSize);
    EhternetHeader->Type = PXE_SWAP_UINT16 (*Protocol);
  }

  //
  // Prepare RNDIS package
  //
  Length         = sizeof (RNDIS_PACKET_MSG_DATA) + (UINT32)BufferSize;
  RndisPacketMsg = AllocateZeroPool (Length);
  if (RndisPacketMsg == NULL) {
    gBS->RestoreTPL (TplPrevious);
    return EFI_OUT_OF_RESOURCES;
  }

  RndisPacketMsg->MessageType   = RNDIS_PACKET_MSG;
  RndisPacketMsg->MessageLength = Length;
  RndisPacketMsg->DataOffset    = sizeof (RNDIS_PACKET_MSG_DATA) - 8;
  RndisPacketMsg->DataLength    = (UINT32)BufferSize;
  DataPointer                   = ((UINT8 *)RndisPacketMsg) + sizeof (RNDIS_PACKET_MSG_DATA);

  CopyMem (DataPointer, Buffer, BufferSize);
  Status = RndisTransmitMessage (
             Private->UsbIoDataProtocol,
             Private->UsbData.EndPoint.BulkOut,
             (RNDIS_MSG_HEADER *)RndisPacketMsg,
             &Length
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, RndisTransmitMessage: %r Length: %d\n", __FUNCTION__, Status, Length));
  }

  FreePool (RndisPacketMsg);

  if (!EFI_ERROR (Status)) {
    //
    // Transmit successfully. Switch to fast receive mode because we are expecting packets.
    //
    UndisReceiveNow (Private);
  }

  gBS->RestoreTPL (TplPrevious);

  return Status;
}

/**
  Receives a packet from a network interface.

  @param[in]  This       The protocol instance pointer.
  @param[out] HeaderSize The size, in bytes, of the media header received on the network
                         interface. If this parameter is NULL, then the media header size
                         will not be returned.
  @param[in,out]  BufferSize On entry, the size, in bytes, of Buffer. On exit, the size, in
                             bytes, of the packet that was received on the network interface.
  @param[out] Buffer     A pointer to the data buffer to receive both the media header and
                         the data.
  @param[out] SrcAddr    The source HW MAC address. If this parameter is NULL, the
                         HW MAC source address will not be extracted from the media
                         header.
  @param[out] DestAddr   The destination HW MAC address. If this parameter is NULL,
                         the HW MAC destination address will not be extracted from the
                         media header.
  @param[out] Protocol   The media header type. If this parameter is NULL, then the
                         protocol will not be extracted from the media header. See
                         RFC 1700 section "Ether Types" for examples.

  @retval  EFI_SUCCESS           The received data was stored in Buffer, and BufferSize has
                                 been updated to the number of bytes received.
  @retval  EFI_NOT_STARTED       The network interface has not been started.
  @retval  EFI_NOT_READY         The network interface is too busy to accept this transmit
                                 request.
  @retval  EFI_BUFFER_TOO_SMALL  The BufferSize parameter is too small.
  @retval  EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval  EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval  EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
UsbRndisSnpReceive (
  IN EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  OUT UINTN                       *HeaderSize OPTIONAL,
  IN OUT UINTN                    *BufferSize,
  OUT VOID                        *Buffer,
  OUT EFI_MAC_ADDRESS             *SrcAddr    OPTIONAL,
  OUT EFI_MAC_ADDRESS             *DestAddr   OPTIONAL,
  OUT UINT16                      *Protocol   OPTIONAL
  )
{
  USB_RNDIS_PRIVATE_DATA  *Private;
  ETHERNET_HEADER         *EhternetHeader;
  RNDIS_PACKET_MSG_DATA   *RndisPacketMsg;
  UINT8                   *RndisBuffer;
  UINTN                   Length;
  EFI_TPL                 TplPrevious;
  EFI_STATUS              Status;

  DEBUG ((USB_DEBUG_SNP_TRACE, "%a\n", __FUNCTION__));

  if ((This == NULL) || (BufferSize == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->Mode->State != EfiSimpleNetworkInitialized) {
    return EFI_NOT_STARTED;
  }

  Private = USB_RNDIS_PRIVATE_DATA_FROM_SNP_THIS (This);
  if (Private->DeviceLost) {
    //
    // Return EFI_NOT_READY because EFI_DEVICE_ERROR will
    // trigger error storm in MNP driver.
    //
    return EFI_NOT_READY;
  }

  TplPrevious = gBS->RaiseTPL (TPL_CALLBACK);

  RndisBuffer = NULL;
  Status      = RndisReceiveDequeue (
                  &Private->UsbData,
                  (UINT8 **)&RndisBuffer,
                  &Length
                  );
  if (EFI_ERROR (Status)) {
    //
    // Call receive worker when queue is empty.
    //
    RndisReceiveWorker (Private);
    Status = RndisReceiveDequeue (
               &Private->UsbData,
               (UINT8 **)&RndisBuffer,
               &Length
               );
    if (EFI_ERROR (Status)) {
      Status = EFI_NOT_READY;
      goto OnRelease;
    }
  }

  RndisPacketMsg = (RNDIS_PACKET_MSG_DATA *)RndisBuffer;
  if (*BufferSize < RndisPacketMsg->DataLength) {
    DEBUG ((USB_DEBUG_SNP, "%a, buffer too small: (%d/%d)\n", __FUNCTION__, *BufferSize, RndisPacketMsg->DataLength));
    *BufferSize = RndisPacketMsg->DataLength;
    Status      = EFI_BUFFER_TOO_SMALL;
    goto OnRelease;
  }

  DEBUG_CODE_BEGIN ();
  DumpRndisMessage (USB_DEBUG_RNDIS_TRANSFER, __FUNCTION__, (RNDIS_MSG_HEADER *)RndisPacketMsg);
  DEBUG_CODE_END ();

  //
  // Move data to caller's buffer
  //
  CopyMem (Buffer, (RndisBuffer + RndisPacketMsg->DataOffset + 8), RndisPacketMsg->DataLength);
  *BufferSize = RndisPacketMsg->DataLength;

  if (HeaderSize != NULL) {
    *HeaderSize = Private->SnpModeData.MediaHeaderSize;

    EhternetHeader = (ETHERNET_HEADER *)Buffer;

    if (SrcAddr != NULL) {
      CopyMem (SrcAddr, &EhternetHeader->SrcAddr[0], Private->SnpModeData.HwAddressSize);
    }

    if (DestAddr != NULL) {
      CopyMem (DestAddr, &EhternetHeader->DstAddr[0], Private->SnpModeData.HwAddressSize);
    }

    if (Protocol != NULL) {
      *Protocol = (UINT16)PXE_SWAP_UINT16 (EhternetHeader->Type);
    }

    DEBUG_CODE_BEGIN ();
    if (HeaderSize != NULL) {
      DEBUG ((USB_DEBUG_SNP, "%a, HeaderSize: %d BufferSize: %d\n", __FUNCTION__, *HeaderSize, *BufferSize));
    }

    if (SrcAddr != NULL) {
      DEBUG ((USB_DEBUG_SNP, "%a, SrcAddr: 0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x\n", __FUNCTION__, SrcAddr->Addr[0], SrcAddr->Addr[1], SrcAddr->Addr[2], SrcAddr->Addr[3], SrcAddr->Addr[4], SrcAddr->Addr[5]));
    }

    if (DestAddr != NULL) {
      DEBUG ((USB_DEBUG_SNP, "%a, DestAddr: 0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x\n", __FUNCTION__, DestAddr->Addr[0], DestAddr->Addr[1], DestAddr->Addr[2], DestAddr->Addr[3], DestAddr->Addr[4], DestAddr->Addr[5]));
    }

    if (Protocol != NULL) {
      DEBUG ((USB_DEBUG_SNP, "%a, Protocol: 0x%x\n", __FUNCTION__, *Protocol));
    }

    DEBUG_CODE_END ();
  }

OnRelease:

  FREE_NON_NULL (RndisBuffer);

  gBS->RestoreTPL (TplPrevious);

  DEBUG_CODE_BEGIN ();
  if (!EFI_ERROR (Status)) {
    DEBUG ((USB_DEBUG_SNP, "%a, BufferSize: %d\n", __FUNCTION__, *BufferSize));
    DumpRawBuffer (USB_DEBUG_SNP, Buffer, *BufferSize);
  } else {
    DEBUG ((USB_DEBUG_SNP_TRACE, "%a, done: %r\n", __FUNCTION__, Status));
  }

  DEBUG_CODE_END ();

  return Status;
}

/**
  Initial RNDIS SNP service

  @param[in]      Private       Poniter to private data

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
UsbRndisInitialSnpService (
  IN  USB_RNDIS_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Private->DeviceLost) {
    return EFI_DEVICE_ERROR;
  }

  DEBUG ((USB_DEBUG_SNP_TRACE, "%a\n", __FUNCTION__));

  Private->SnpProtocol.Revision       = EFI_SIMPLE_NETWORK_PROTOCOL_REVISION;
  Private->SnpProtocol.Start          = UsbRndisSnpStart;
  Private->SnpProtocol.Stop           = UsbRndisSnpStop;
  Private->SnpProtocol.Initialize     = UsbRndisSnpInitialize;
  Private->SnpProtocol.Reset          = UsbRndisSnpReset;
  Private->SnpProtocol.Shutdown       = UsbRndisSnpShutdown;
  Private->SnpProtocol.ReceiveFilters = UsbRndisSnpReceiveFilters;
  Private->SnpProtocol.StationAddress = UsbRndisSnpStationAddress;
  Private->SnpProtocol.Statistics     = UsbRndisSnpStatistics;
  Private->SnpProtocol.MCastIpToMac   = UsbRndisSnpMcastIpToMac;
  Private->SnpProtocol.NvData         = UsbRndisSnpNvData;
  Private->SnpProtocol.GetStatus      = UsbRndisSnpGetSatus;
  Private->SnpProtocol.Transmit       = UsbRndisSnpTransmit;
  Private->SnpProtocol.Receive        = UsbRndisSnpReceive;
  Private->SnpProtocol.WaitForPacket  = NULL;
  Private->SnpProtocol.Mode           = &Private->SnpModeData;

  Status = UsbRndisInitialRndisDevice (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a, UsbRndisInitialRndisDevice: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Private->SnpProtocol.Mode->State             = EfiSimpleNetworkStopped;
  Private->SnpProtocol.Mode->HwAddressSize     = NET_ETHER_ADDR_LEN;
  Private->SnpProtocol.Mode->MediaHeaderSize   = sizeof (ETHERNET_HEADER);
  Private->SnpProtocol.Mode->MaxPacketSize     = Private->UsbData.MaxFrameSize;
  Private->SnpProtocol.Mode->ReceiveFilterMask = EFI_SIMPLE_NETWORK_RECEIVE_UNICAST
                                                 | EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST
                                                 | EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST
                                                 | EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS;
  Private->SnpProtocol.Mode->ReceiveFilterSetting = EFI_SIMPLE_NETWORK_RECEIVE_UNICAST
                                                    | EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST;
  Private->SnpProtocol.Mode->MaxMCastFilterCount   = MAX_MCAST_FILTER_CNT;
  Private->SnpProtocol.Mode->MCastFilterCount      = 0;
  Private->SnpProtocol.Mode->NvRamSize             = 0;
  Private->SnpProtocol.Mode->NvRamAccessSize       = 0;
  Private->SnpProtocol.Mode->IfType                = NET_IFTYPE_ETHERNET;
  Private->SnpProtocol.Mode->MacAddressChangeable  = FALSE;
  Private->SnpProtocol.Mode->MultipleTxSupported   = FALSE;
  Private->SnpProtocol.Mode->MediaPresentSupported = FALSE;
  Private->SnpProtocol.Mode->MediaPresent          = (Private->UsbData.MediaStatus == RNDIS_MEDIA_STATE_CONNECTED ? TRUE : FALSE);

  //
  // MAC address
  //
  SetMem (&Private->SnpProtocol.Mode->BroadcastAddress, NET_ETHER_ADDR_LEN, 0xff);
  CopyMem (&Private->SnpProtocol.Mode->CurrentAddress, &Private->UsbData.CurrentAddress, NET_ETHER_ADDR_LEN);
  CopyMem (&Private->SnpProtocol.Mode->PermanentAddress, &Private->UsbData.PermanentAddress, NET_ETHER_ADDR_LEN);

  DEBUG_CODE_BEGIN ();
  DEBUG ((USB_DEBUG_SNP, "%a, MediaPresent: 0x%x HwAddressSize: 0x%x\n", __FUNCTION__, Private->SnpProtocol.Mode->MediaPresent, Private->SnpProtocol.Mode->HwAddressSize));
  DEBUG ((
    USB_DEBUG_SNP,
    "%a, BroadcastAddress %02x:%02x:%02x:%02x:%02x:%02x\n",
    __FUNCTION__,
    Private->SnpProtocol.Mode->BroadcastAddress.Addr[0],
    Private->SnpProtocol.Mode->BroadcastAddress.Addr[1],
    Private->SnpProtocol.Mode->BroadcastAddress.Addr[2],
    Private->SnpProtocol.Mode->BroadcastAddress.Addr[3],
    Private->SnpProtocol.Mode->BroadcastAddress.Addr[4],
    Private->SnpProtocol.Mode->BroadcastAddress.Addr[5]
    ));
  DEBUG ((
    USB_DEBUG_SNP,
    "%a, CurrentAddress %02x:%02x:%02x:%02x:%02x:%02x\n",
    __FUNCTION__,
    Private->SnpProtocol.Mode->CurrentAddress.Addr[0],
    Private->SnpProtocol.Mode->CurrentAddress.Addr[1],
    Private->SnpProtocol.Mode->CurrentAddress.Addr[2],
    Private->SnpProtocol.Mode->CurrentAddress.Addr[3],
    Private->SnpProtocol.Mode->CurrentAddress.Addr[4],
    Private->SnpProtocol.Mode->CurrentAddress.Addr[5]
    ));
  DEBUG ((
    USB_DEBUG_SNP,
    "%a, PermanentAddress %02x:%02x:%02x:%02x:%02x:%02x\n",
    __FUNCTION__,
    Private->SnpProtocol.Mode->PermanentAddress.Addr[0],
    Private->SnpProtocol.Mode->PermanentAddress.Addr[1],
    Private->SnpProtocol.Mode->PermanentAddress.Addr[2],
    Private->SnpProtocol.Mode->PermanentAddress.Addr[3],
    Private->SnpProtocol.Mode->PermanentAddress.Addr[4],
    Private->SnpProtocol.Mode->PermanentAddress.Addr[5]
    ));
  DEBUG_CODE_END ();

  return EFI_SUCCESS;
}
