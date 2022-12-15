/** @file

  Erot Qspi library

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/ErotQspiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include "ErotQspiCore.h"

EROT_QSPI_PRIVATE_DATA  *mPrivate     = NULL;
UINTN                   mNumErotQspis = 0;

/**
  Fill the Erot QSPI medium header for a packet.

  @param[in]  Header          Pointer to medium header.
  @param[in]  PayloadLength   Length of MCTP payload.

  @retval   None

**/
STATIC
VOID
EFIAPI
ErotQspiFillMediumHeader (
  IN EROT_QSPI_MEDIUM_HEADER  *Header,
  IN UINTN                    PayloadLength
  )
{
  Header->Type        = EROT_QSPI_MSG_TYPE_MCTP;
  Header->Length      = PayloadLength + sizeof (MCTP_TRANSPORT_HEADER);
  Header->Reserved[0] = 0;
  Header->Reserved[1] = 0;
}

/**
  Fill the MCTP transport header for a packet.

  @param[in]  Header        Pointer to MCTP transport header.
  @param[in]  DstEID        Destination EID.
  @param[in]  SrcEID        Source EID.
  @param[in]  PktSeq        Packet sequence number.
  @param[in]  Request       TRUE if packet is an MCTP request.
  @param[in]  Tag           MCTP message tag.

  @retval   None

**/
STATIC
VOID
EFIAPI
ErotQspiFillTransportHeader (
  IN MCTP_TRANSPORT_HEADER  *Header,
  IN UINT8                  DstEID,
  IN UINT8                  SrcEID,
  IN UINT8                  PktSeq,
  IN BOOLEAN                Request,
  IN UINT8                  Tag
  )
{
  PktSeq &= MCTP_TRANSPORT_PACKET_SEQUENCE_MASK;

  Header->HdrVer  = EROT_QSPI_TRANSPORT_HEADER_VERSION;
  Header->DstEID  = DstEID;
  Header->SrcEID  = SrcEID;
  Header->Control = (PktSeq << MCTP_TRANSPORT_PACKET_SEQUENCE_SHIFT) |
                    ((Request) ? MCTP_TRANSPORT_TO : 0) |
                    ((Tag & MCTP_TRANSPORT_MESSAGE_TAG_MASK));
}

/**
  Wait for erot to raise an interupt.

  @param[in]  Private       Pointer to private data structure for erot.
  @param[in]  TimeoutMs     Ms to wait.

  @retval EFI_SUCCESS       Interrupt is pending.
  @retval EFI_TIMEOUT       No interrupt occurred within TimeoutMs.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiWaitForInterrupt (
  IN EROT_QSPI_PRIVATE_DATA  *Private,
  IN  UINTN                  TimeoutMs
  )
{
  UINT64  EndNs;

  EndNs = ErotQspiNsCounter () + EROT_QSPI_MS_TO_NS (TimeoutMs);

  while (!ErotQspiHasInterruptReq (Private)) {
    if (ErotQspiNsCounter () >= EndNs) {
      if (TimeoutMs > 0) {
        DEBUG ((DEBUG_ERROR, "%a: Timed out after %ums\n", __FUNCTION__, TimeoutMs));
      }

      return EFI_TIMEOUT;
    }
  }

  return EFI_SUCCESS;
}

/**
  Issue MCTP Set EID command to erot.

  @param[in]  Private       Pointer to private data structure for erot.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiSetEid (
  IN  EROT_QSPI_PRIVATE_DATA  *Private
  )
{
  MCTP_SET_ENDPOINT_REQUEST   Request;
  MCTP_SET_ENDPOINT_RESPONSE  Response;
  NVIDIA_MCTP_PROTOCOL        *Protocol;
  UINTN                       ResponseLength;
  EFI_STATUS                  Status;
  MCTP_DEVICE_ATTRIBUTES      Attributes;

  Protocol = &Private->Protocol;
  Status   = Protocol->GetDeviceAttributes (Protocol, &Attributes);
  ASSERT_EFI_ERROR (Status);

  MctpControlReqFillCommon (&Request.Common, MCTP_CONTROL_SET_ENDPOINT_ID);
  Request.Operation  = MCTP_SET_ENDPOINT_OPERATION_SET_EID;
  Request.EndpointId = EROT_QSPI_EROT_EID;

  Status = Protocol->DoRequest (
                       Protocol,
                       &Request,
                       sizeof (Request),
                       &Response,
                       sizeof (Response),
                       &ResponseLength
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s request failed: %r\n", __FUNCTION__, Attributes.DeviceName, Status));
    return Status;
  }

  if (ResponseLength != sizeof (Response)) {
    DEBUG ((DEBUG_ERROR, "%a: %s bad resp length: %u!=%u\n", __FUNCTION__, Attributes.DeviceName, ResponseLength, sizeof (Response)));
    return EFI_DEVICE_ERROR;
  }

  if (Response.CompletionCode != MCTP_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s  failed: 0x%x\n",
      __FUNCTION__,
      Attributes.DeviceName,
      Response.CompletionCode
      ));
    return EFI_DEVICE_ERROR;
  }

  if (Response.Status != 0) {
    DEBUG ((
      DEBUG_WARN,
      "%a: WARNING: %s status=0x%x, eid=0x%x\n",
      __FUNCTION__,
      Attributes.DeviceName,
      Response.Status,
      Response.EndpointId
      ));
  }

  Private->ErotEID = Response.EndpointId;

  return EFI_SUCCESS;
}

/**
  Initialize erot if not previously initialized

  @param[in]  Private       Pointer to private data structure for erot.

  @retval EFI_SUCCESS     Operation completed normally.
  @retval Others          Failure occurred.

**/
STATIC
EFI_STATUS
EFIAPI
ErotQspiInitErot (
  IN  EROT_QSPI_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  if (Private->ErotIsInitialized) {
    return EFI_SUCCESS;
  }

  Status = ErotQspiSpbInit (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Error initializing %s: %r\n",
      __FUNCTION__,
      Private->Name,
      Status
      ));
    return Status;
  }

  Private->ErotIsInitialized = TRUE;

  Status = ErotQspiSetEid (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error setting %s EID: %r\n", __FUNCTION__, Private->Name, Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: %s init complete\n", __FUNCTION__, Private->Name));

  return EFI_SUCCESS;
}

// NVIDIA_MCTP_PROTOCOL.GetDeviceAttributes ()
STATIC
EFI_STATUS
EFIAPI
ErotQspiGetDeviceAttributes (
  IN  NVIDIA_MCTP_PROTOCOL    *This,
  OUT MCTP_DEVICE_ATTRIBUTES  *Attributes
  )
{
  EROT_QSPI_PRIVATE_DATA  *Private;

  if ((This == NULL) || (Attributes == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CR (
              This,
              EROT_QSPI_PRIVATE_DATA,
              Protocol,
              EROT_QSPI_PRIVATE_DATA_SIGNATURE
              );

  Attributes->DeviceName = Private->Name;
  Attributes->DeviceType = DEVICE_TYPE_EROT;
  Attributes->Socket     = Private->Socket;

  return EFI_SUCCESS;
}

// NVIDIA_MCTP_PROTOCOL.DoRequest ()
STATIC
EFI_STATUS
EFIAPI
ErotQspiDoRequest (
  IN  NVIDIA_MCTP_PROTOCOL  *This,
  IN  VOID                  *Request,
  IN  UINTN                 RequestLength,
  OUT VOID                  *ResponseBuffer,
  IN UINTN                  ResponseBufferLength,
  OUT UINTN                 *ResponseLength
  )
{
  EROT_QSPI_PRIVATE_DATA  *Private;
  EFI_STATUS              Status;
  UINT8                   MsgTag;
  UINT8                   RecvMsgTag;

  if ((This == NULL) || (Request == NULL) || (ResponseBuffer == NULL) || (ResponseLength == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CR (
              This,
              EROT_QSPI_PRIVATE_DATA,
              Protocol,
              EROT_QSPI_PRIVATE_DATA_SIGNATURE
              );

  Status = ErotQspiInitErot (Private);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = This->Send (This, TRUE, Request, RequestLength, &MsgTag);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *ResponseLength = ResponseBufferLength;
  Status          = This->Recv (
                            This,
                            QSPI_MCTP_MT2_MS_MAX,
                            ResponseBuffer,
                            ResponseLength,
                            &RecvMsgTag
                            );
  if (Status == EFI_SUCCESS) {
    if (RecvMsgTag != MsgTag) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: invalid msg tag %u != %u\n",
        __FUNCTION__,
        MsgTag,
        RecvMsgTag
        ));
      Status = EFI_PROTOCOL_ERROR;
    }
  }

  return Status;
}

// NVIDIA_MCTP_PROTOCOL.Recv ()
STATIC
EFI_STATUS
EFIAPI
ErotQspiRecv (
  IN  NVIDIA_MCTP_PROTOCOL  *This,
  IN  UINTN                 TimeoutMs,
  OUT VOID                  *Message,
  IN OUT UINTN              *Length,
  OUT UINT8                 *MsgTag
  )
{
  EROT_QSPI_PRIVATE_DATA   *Private;
  EROT_QSPI_PACKET         *Packet;
  EROT_QSPI_MEDIUM_HEADER  *MediumHdr;
  UINT8                    *MsgPtr;
  UINTN                    PacketLength;
  UINTN                    PayloadLength;
  UINTN                    FirstPayloadLength;
  UINTN                    MsgLength;
  EFI_STATUS               Status;
  BOOLEAN                  EndOfMsg;
  BOOLEAN                  StartOfMsg;
  UINT8                    NextSeq;
  UINT8                    PktSeq;
  UINT8                    PktTag;
  UINT8                    TransportControl;

  if ((This == NULL) || (Message == NULL) || (Length == NULL) || (MsgTag == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CR (
              This,
              EROT_QSPI_PRIVATE_DATA,
              Protocol,
              EROT_QSPI_PRIVATE_DATA_SIGNATURE
              );

  Status = ErotQspiInitErot (Private);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ErotQspiWaitForInterrupt (Private, TimeoutMs);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Packet    = &Private->Packet;
  MediumHdr = &Packet->MediumHdr;
  MsgPtr    = (UINT8 *)Message;
  MsgLength = 0;

  StartOfMsg = TRUE;
  EndOfMsg   = FALSE;
  while (!EndOfMsg) {
    if (!StartOfMsg) {
      Status = ErotQspiWaitForInterrupt (Private, QSPI_MCTP_PT_MS_MAX);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: interrupt failed MsgLen=%u, SOM=%u EOM=%u NextSeq=%u: %r\n",
          __FUNCTION__,
          MsgLength,
          StartOfMsg,
          EndOfMsg,
          NextSeq,
          Status
          ));
        return Status;
      }
    }

    Status = ErotQspiRecvPacket (Private, &PacketLength);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: RecvPacket MsgLen=%u, SOM=%u EOM=%u NextSeq=%u failed: %r\n",
        __FUNCTION__,
        MsgLength,
        StartOfMsg,
        EndOfMsg,
        NextSeq,
        Status
        ));
      return Status;
    }

    if ((MediumHdr->Type != EROT_QSPI_MSG_TYPE_MCTP) ||
        (MediumHdr->Length + sizeof (EROT_QSPI_MEDIUM_HEADER) != PacketLength))
    {
      DEBUG ((
        DEBUG_ERROR,
        "%a: invalid medium hdr type=%u, length=%u/%u\n",
        __FUNCTION__,
        MediumHdr->Type,
        MediumHdr->Length,
        PacketLength
        ));
      DEBUG ((
        DEBUG_ERROR,
        "%a: dropping packet MsgLen=%u, SOM=%u EOM=%u NextSeq=%u\n",
        __FUNCTION__,
        MsgLength,
        StartOfMsg,
        EndOfMsg,
        NextSeq
        ));
      continue;
    }

    PayloadLength = PacketLength - OFFSET_OF (EROT_QSPI_PACKET, Payload);

    if (*Length < MsgLength + PayloadLength) {
      DEBUG ((DEBUG_ERROR, "%a: length error %u < %u\n", __FUNCTION__, *Length, MsgLength + PayloadLength));
      return EFI_BUFFER_TOO_SMALL;
    }

    TransportControl = Packet->TransportHdr.Control;
    PktTag           = TransportControl & MCTP_TRANSPORT_MESSAGE_TAG_MASK;
    PktSeq           = (TransportControl >> MCTP_TRANSPORT_PACKET_SEQUENCE_SHIFT) &
                       MCTP_TRANSPORT_PACKET_SEQUENCE_MASK;
    EndOfMsg = ((TransportControl & MCTP_TRANSPORT_EOM) != 0);

    if (StartOfMsg) {
      if ((TransportControl & MCTP_TRANSPORT_SOM) == 0) {
        DEBUG ((DEBUG_ERROR, "%a: Missing SOM bit 0x%x\n", __FUNCTION__, TransportControl));
        return EFI_PROTOCOL_ERROR;
      }

      *MsgTag            = PktTag;
      FirstPayloadLength = PayloadLength;
      StartOfMsg         = FALSE;
    } else {
      if ((PktTag != *MsgTag) || (PktSeq != NextSeq)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Bad Tag or Seq 0x%x Expected Tag %u Seq %u\n",
          __FUNCTION__,
          TransportControl,
          *MsgTag,
          NextSeq
          ));
        return EFI_PROTOCOL_ERROR;
      }

      if (!EndOfMsg && (PayloadLength != FirstPayloadLength)) {
        DEBUG ((DEBUG_ERROR, "%a: Bad middle packet length %u!=%u\n", __FUNCTION__, PayloadLength, FirstPayloadLength));
        return EFI_PROTOCOL_ERROR;
      }
    }

    CopyMem (MsgPtr, Packet->Payload, PayloadLength);

    NextSeq    = (PktSeq + 1) & MCTP_TRANSPORT_PACKET_SEQUENCE_MASK;
    MsgLength += PayloadLength;
    MsgPtr    += PayloadLength;
  }

  *Length = MsgLength;

  return EFI_SUCCESS;
}

// NVIDIA_MCTP_PROTOCOL.Send ()
STATIC
EFI_STATUS
EFIAPI
ErotQspiSend (
  IN  NVIDIA_MCTP_PROTOCOL  *This,
  IN  BOOLEAN               IsRequest,
  IN  CONST VOID            *Message,
  IN  UINTN                 Length,
  IN OUT UINT8              *MsgTag
  )
{
  EROT_QSPI_PRIVATE_DATA  *Private;
  MCTP_TRANSPORT_HEADER   *Header;
  EROT_QSPI_PACKET        *Packet;
  UINTN                   PayloadLength;
  BOOLEAN                 StartMessage;
  CONST UINT8             *MsgPtr;
  EFI_STATUS              Status;
  UINT8                   PktSeq;

  if ((This == NULL) || (Message == NULL) || (MsgTag == NULL) || (Length == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = CR (
              This,
              EROT_QSPI_PRIVATE_DATA,
              Protocol,
              EROT_QSPI_PRIVATE_DATA_SIGNATURE
              );

  Status = ErotQspiInitErot (Private);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Packet = &Private->Packet;
  Header = &Private->Packet.TransportHdr;

  if (IsRequest) {
    *MsgTag = Private->MsgTag++;
  }

  PktSeq       = 0;
  MsgPtr       = (CONST UINT8 *)Message;
  StartMessage = TRUE;
  while (Length > 0) {
    PayloadLength = MIN (Length, sizeof (Packet->Payload));
    ErotQspiFillMediumHeader (&Packet->MediumHdr, PayloadLength);
    ErotQspiFillTransportHeader (
      Header,
      Private->ErotEID,
      Private->MyEID,
      PktSeq++,
      IsRequest,
      *MsgTag
      );
    if (StartMessage) {
      Header->Control |= MCTP_TRANSPORT_SOM;
      StartMessage     = FALSE;
    }

    if (Length == PayloadLength) {
      Header->Control |= MCTP_TRANSPORT_EOM;
    }

    CopyMem (Private->Packet.Payload, MsgPtr, PayloadLength);

    Status = ErotQspiSendPacket (
               Private,
               OFFSET_OF (EROT_QSPI_PACKET, Payload) + PayloadLength
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error sending packet\n", __FUNCTION__));
      return Status;
    }

    Length -= PayloadLength;
    MsgPtr += PayloadLength;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ErotQspiAddErot (
  IN  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *Qspi,
  IN  UINT8                            ChipSelect,
  IN  UINT8                            Socket
  )
{
  EROT_QSPI_PRIVATE_DATA  *Private;

  if (mPrivate == NULL) {
    return EFI_NOT_READY;
  }

  Private            = &mPrivate[mNumErotQspis];
  Private->Signature = EROT_QSPI_PRIVATE_DATA_SIGNATURE;
  Private->MyEID     = EROT_QSPI_CONTROLLER_EID;
  UnicodeSPrintAsciiFormat (Private->Name, sizeof (Private->Name), "Erot%u", Socket);

  Private->Qspi                         = Qspi;
  Private->ChipSelect                   = ChipSelect;
  Private->Socket                       = Socket;
  Private->Protocol.GetDeviceAttributes = ErotQspiGetDeviceAttributes;
  Private->Protocol.DoRequest           = ErotQspiDoRequest;
  Private->Protocol.Recv                = ErotQspiRecv;
  Private->Protocol.Send                = ErotQspiSend;

  mNumErotQspis++;

  DEBUG ((DEBUG_INFO, "%a: %s added\n", __FUNCTION__, Private->Name));

  return EFI_SUCCESS;
}

VOID
EFIAPI
ErotQspiLibDeinit (
  VOID
  )
{
  EROT_QSPI_PRIVATE_DATA  *Private;
  UINTN                   Index;

  Private = mPrivate;
  for (Index = 0; Index < mNumErotQspis; Index++, Private++) {
    ErotQspiSpbDeinit (Private);
  }

  mNumErotQspis = 0;
  if (mPrivate != NULL) {
    FreePool (mPrivate);
    mPrivate = NULL;
  }
}

EFI_STATUS
EFIAPI
ErotQspiLibInit (
  IN  UINTN  NumDevices
  )
{
  mPrivate = (EROT_QSPI_PRIVATE_DATA *)AllocateRuntimeZeroPool (
                                         NumDevices * sizeof (EROT_QSPI_PRIVATE_DATA)
                                         );
  if (mPrivate == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: mPrivate allocation failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  return EFI_SUCCESS;
}
