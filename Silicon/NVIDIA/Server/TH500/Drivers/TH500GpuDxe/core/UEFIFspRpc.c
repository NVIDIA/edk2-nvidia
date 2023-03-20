/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

/*
 * @file  [UEFI] UEFIFspRpc.c
 * @brief UEFI client code which implements simple transactions between FSP and
 *        UEFI client.
 */

///
/// Libraries
///

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

///
/// Protocol(s)
///
#include <Protocol/PciIo.h>

///
///
///
#include <IndustryStandard/Pci.h>

/* nvmisc.h requires NvU32 defined. Map it to UEFI native unsigned 32-bit type */
typedef UINT32 NvU32;
/* Do not use legacy BIT macros, naming conflicts with UEFI builtin BIT definitions */
#define NVIDIA_UNDEF_LEGACY_BIT_MACROS

/* ------------------------- System Includes -------------------------------- */
#include "nvmisc.h"

// Not getting pulled in from nvmisc.h
#ifndef NV_ALIGN_UP
#define NV_ALIGN_UP(v, gran)  (((v) + ((gran) - 1)) & ~((gran)-1))
#endif

/* ------------------------- Application Includes --------------------------- */
#include "fsp/fsp_nvdm_format.h"
#include "fsp/nvdm_payload_cmd_response.h"
#include "dev_fsp_pri.h"

/* Public FSP RPC headers may not have NVDM_TYPE_UEFI_RM defined depending on release version */
#ifndef NVDM_TYPE_UEFI_RM
#define NVDM_TYPE_UEFI_RM  0x1C
#endif

#include "UEFIFspRpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* Debug logging escalation, currently fixes missing message queue response error */
#ifdef DEBUG_INFO
  #undef DEBUG_INFO
#define DEBUG_INFO  DEBUG_ERROR
#else
#define DEBUG_INFO  DEBUG_ERROR
#endif

#ifndef FSP_RPC_RESPONSE_PACKET_SIZE
#define FSP_RPC_RESPONSE_PACKET_SIZE  (0x10+4)
#endif

#define UEFI_FSP_RPC_MSG_QUEUE_POLL_TIMEOUT_INDEX  100000
#define UEFI_FSP_RPC_CMD_QUEUE_POLL_TIMEOUT_INDEX  1000

#define UEFI_STALL_DELAY_UNITS  5

#define CONVERT_DWORD_COUNT_TO_BYTE_SIZE(dword)  ((dword)<<2)

#ifndef FSP_OK
#define FSP_OK  0
#endif

/* UEFI FSP RPC configured for Single Packet mode */
#define NVDM_PAYLOAD_COMMAND_RESPONSE_SIZE \
       FSP_RPC_HEADER_SIZE_SINGLE_PACKET + sizeof(NVDM_PAYLOAD_COMMAND_RESPONSE)

/* ------------------------ Static variables -------------------------------- */

/* ------------------------- Function Prototypes ---------------------------- */
STATIC VOID
EFIAPI
PrintNvdmMessage (
  UINT8  *NvdmMessage,
  UINT8  MsgSize
  );

/* ------------------------- Private Functions ------------------------------ */

STATIC EFI_STATUS EFIAPI
uefifspRpcQueueHeadTailRequestSet (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId,
  IN UINT32               queueHead,
  IN UINT32               queueTail
  );

STATIC EFI_STATUS EFIAPI
uefifspRpcQueueHeadTailGet (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId,
  IN UINT32               *pQueueHead,
  IN UINT32               *pQueueTail
  );

STATIC EFI_STATUS EFIAPI
uefifspRpcMsgQueueHeadTailGet (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId,
  IN UINT32               *pMsgQueueHead,
  IN UINT32               *pMsgQueueTail
  );

STATIC EFI_STATUS EFIAPI
uefifspRpcMsgQueueHeadTailSet (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId,
  IN UINT32               msgQueueHead,
  IN UINT32               msgQueueTail
  );

STATIC UINT8 EFIAPI
uefifspRpcGetSeidFromNvdm (
  UINT8  nvdmType
  );

STATIC UINT32 EFIAPI
uefifspRpcCreateMctpTransportHeader (
  UINT32   nvdmType,
  UINT32   packetSequence,
  BOOLEAN  bLastPacket
  );

STATIC UINT32 EFIAPI
uefifspRpcCreateMctpPayloadHeader (
  UINT32  nvdmType
  );

STATIC BOOLEAN EFIAPI
uefifspRpcValidateMctpPayloadHeader (
  UINT32  mctpPayloadHeader
  );

STATIC FSP_RPC_MCTP_PACKET_STATE EFIAPI
uefifspGetPacketInfo (
  UINT32  mctpHeader
  );

/*
 * @brief Checks if the message queue is empty by comparing QUEUE HEAD and TAIL pointers.
 *
 * @param[in] PciIo         Handle for the Controller's PciIo protocol
 * @param[in] channelId     FSP EMEM channel ID
 *
 * @return TRUE  if message queue is empty.
 *         FALSE otherwise
 */
BOOLEAN
EFIAPI
fspRpcIsMsgQueueEmpty (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId
  )
{
  UINT32  msgQueueHead = 0U;
  UINT32  msgQueueTail = 0U;

  /* Expect FSP_EMEM_CHANNEL_RM for channelId */
  ASSERT (channelId == FSP_EMEM_CHANNEL_RM);

  uefifspRpcMsgQueueHeadTailGet (PciIo, channelId, &msgQueueHead, &msgQueueTail);
  return (msgQueueHead == msgQueueTail);
}

/*!
 * @brief Wait for FSP Message queue to be empty
 *
 * @param[in] PciIo         Handle for the Controller's PciIo protocol
 * @param[in] channelId     FSP EMEM channel ID
 *
 * @return EFI_SUCCESS, or EFI_TIMEOUT
 */
EFI_STATUS
EFIAPI
uefifspPollForMsgQueueEmpty (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId
  )
{
  EFI_STATUS  Status     = EFI_SUCCESS;
  UINT32      TimeoutIdx = UEFI_FSP_RPC_MSG_QUEUE_POLL_TIMEOUT_INDEX;

  while ((!fspRpcIsMsgQueueEmpty (PciIo, channelId)) && (!TimeoutIdx--)) {
    DEBUG_CODE_BEGIN ();
    DEBUG ((DEBUG_INFO, "%a: [%p][TimeoutIdx:%d]\n", __FUNCTION__, PciIo, TimeoutIdx));
    DEBUG_CODE_END ();
    gBS->Stall (UEFI_STALL_DELAY_UNITS);
  }

  if (TimeoutIdx == 0) {
    DEBUG ((DEBUG_ERROR, "%a: [%p][TimeoutIdx:%d] Poll for Message Queue empty timed out.\n", __FUNCTION__, PciIo, TimeoutIdx));
    Status = EFI_TIMEOUT;
  }

  return Status;
}

/*!
 * @brief Wait for FSP Message queue to have a response
 *
 * @param[in] PciIo         Handle for the Controller's PciIo protocol
 * @param[in] channelId     FSP EMEM channel ID
 *
 * @return EFI_SUCCESS, or EFI_TIMEOUT
 */
EFI_STATUS
EFIAPI
uefifspPollForMsgQueueResponse (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId
  )
{
  EFI_STATUS  Status     = EFI_SUCCESS;
  UINT32      TimeoutIdx = UEFI_FSP_RPC_MSG_QUEUE_POLL_TIMEOUT_INDEX;

  while ((fspRpcIsMsgQueueEmpty (PciIo, channelId)) && (!TimeoutIdx--)) {
    DEBUG_CODE_BEGIN ();
    DEBUG ((DEBUG_INFO, "%a: [%p][TimeoutIdx:%d]\n", __FUNCTION__, PciIo, TimeoutIdx));
    DEBUG_CODE_END ();
    gBS->Stall (UEFI_STALL_DELAY_UNITS);
  }

  if (TimeoutIdx == 0) {
    DEBUG ((DEBUG_ERROR, "%a: [%p][TimeoutIdx:%d] Poll for Message Queue response timed out.\n", __FUNCTION__, PciIo, TimeoutIdx));
    Status = EFI_TIMEOUT;
  }

  return Status;
}

/*
 * @brief Checks if the queue is empty for sending messages by comparing QUEUE HEAD and TAIL pointers.
 *
 * @param[in] PciIo         Handle for the Controller's PciIo protocol
 * @param[in] channelId     FSP EMEM channel ID
 *
 * @return TRUE  if queue is empty.
 *         FALSE otherwise
 */
BOOLEAN
EFIAPI
fspRpcIsQueueEmpty (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId
  )
{
  UINT32  msgHead = 0U;
  UINT32  msgTail = 0U;

  /* Expect FSP_EMEM_CHANNEL_RM for channelId */
  ASSERT (channelId == FSP_EMEM_CHANNEL_RM);

  uefifspRpcQueueHeadTailGet (PciIo, channelId, &msgHead, &msgTail);
  return (msgHead == msgTail);
}

/*!
 * @brief Update command queue head and tail pointers
 *
 * @param[in] PciIo     PciIo protocol handle
 * @param[in] channelId EMEM channel ID corresponding to the client
 * @param[in] queueHead Offset to write to command queue head
 * @param[in] queueTail Offset to write to command queue tail
 */
STATIC EFI_STATUS
EFIAPI
uefifspRpcQueueHeadTailRequestSet (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId,
  IN UINT32               queueHead,
  IN UINT32               queueTail
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if (NULL == PciIo) {
    return EFI_INVALID_PARAMETER;
  }

  /* Limit Channel ID for UEFI variant */
  if (channelId != FSP_EMEM_CHANNEL_RM) {
    return EFI_INVALID_PARAMETER;
  }

  // The write to HEAD needs to happen after TAIL because it will interrupt FSP
  Status = PciIo->Mem.Write (
                        PciIo,                          // This
                        EfiPciIoWidthUint32,            // Width
                        PCI_BAR_IDX0,                   // BarIndex
                        NV_PFSP_QUEUE_TAIL (channelId), // Offset
                        1,                              // Count
                        &queueTail
                        );
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    return Status;
  }

  Status = PciIo->Mem.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        PCI_BAR_IDX0,
                        NV_PFSP_QUEUE_HEAD (channelId),
                        1,
                        &queueHead
                        );
  if (EFI_ERROR (Status)) {
    ASSERT (0);
  }

  return Status;
}

/*!
 * @brief Update queue head and tail pointers
 *
 * @param[in] PciIo     PciIo protocol handle
 * @param[in] channelId EMEM channel ID corresponding to the client
 * @param[in] queueHead Offset to write to command queue head
 * @param[in] queueTail Offset to write to command queue tail
 */
STATIC EFI_STATUS
EFIAPI
uefifspRpcQueueHeadTailGet (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId,
  IN UINT32               *pQueueHead,
  IN UINT32               *pQueueTail
  )
{
  EFI_STATUS  Status;

  if (NULL == PciIo) {
    return EFI_INVALID_PARAMETER;
  }

  /* Limit Channel ID for UEFI variant */
  if (channelId != FSP_EMEM_CHANNEL_RM) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == pQueueHead) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == pQueueTail) {
    return EFI_INVALID_PARAMETER;
  }

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        PCI_BAR_IDX0,
                        NV_PFSP_QUEUE_HEAD (channelId),
                        1,          // Count
                        pQueueTail  // Value
                        );
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    return Status;
  }

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        PCI_BAR_IDX0,
                        NV_PFSP_QUEUE_TAIL (channelId),
                        1,          // Count
                        pQueueTail  // Value
                        );
  if (EFI_ERROR (Status)) {
    ASSERT (0);
  }

  return Status;
}

/*!
 * @brief Retrieve message queue head and tail pointers
 *
 * @param[in] PciIo         PciIo protocol handle
 * @param[in] channelId     EMEM channel ID corresponding to the client
 * @param[in] pMsgQueueHead Offset to write to command queue head
 * @param[in] pMsgQueueTail Offset to write to command queue tail
 */
STATIC EFI_STATUS
EFIAPI
uefifspRpcMsgQueueHeadTailGet (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId,
  IN UINT32               *pMsgQueueHead,
  IN UINT32               *pMsgQueueTail
  )
{
  EFI_STATUS  Status;

  if (NULL == PciIo) {
    return EFI_INVALID_PARAMETER;
  }

  /* Limit Channel ID for UEFI variant */
  if (channelId != FSP_EMEM_CHANNEL_RM) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == pMsgQueueHead) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == pMsgQueueTail) {
    return EFI_INVALID_PARAMETER;
  }

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        PCI_BAR_IDX0,
                        NV_PFSP_MSGQ_HEAD (channelId),
                        1,            // Count
                        pMsgQueueHead // Value
                        );
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    return Status;
  }

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        PCI_BAR_IDX0,
                        NV_PFSP_MSGQ_TAIL (channelId),
                        1,            // Count
                        pMsgQueueTail // Value
                        );
  if (EFI_ERROR (Status)) {
    ASSERT (0);
  }

  return Status;
}

/*!
 * @brief Update message queue head and tail pointers
 *
 * @param[in] PciIo        PciIo protocol handle
 * @param[in] channelId    EMEM channel ID corresponding to the client
 * @param[in] msgQueueHead Offset to write to command queue head
 * @param[in] msgQueueTail Offset to write to command queue tail
 */
STATIC EFI_STATUS
EFIAPI
uefifspRpcMsgQueueHeadTailSet (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId,
  IN UINT32               msgQueueHead,
  IN UINT32               msgQueueTail
  )
{
  EFI_STATUS  Status;

  if (NULL == PciIo) {
    return EFI_INVALID_PARAMETER;
  }

  /* Limit Channel ID for UEFI variant */
  if (channelId != FSP_EMEM_CHANNEL_RM) {
    return EFI_INVALID_PARAMETER;
  }

  // The write to HEAD needs to happen after TAIL because it will interrupt FSP
  Status = PciIo->Mem.Write (
                        PciIo,                         // This
                        EfiPciIoWidthUint32,           // Width
                        PCI_BAR_IDX0,                  // BarIndex
                        NV_PFSP_MSGQ_HEAD (channelId), // Offset
                        1,                             // Count
                        &msgQueueHead
                        );
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    return Status;
  }

  Status = PciIo->Mem.Write (
                        PciIo,                         // This
                        EfiPciIoWidthUint32,           // Width
                        PCI_BAR_IDX0,                  // BarIndex
                        NV_PFSP_MSGQ_TAIL (channelId), // Offset
                        1,                             // Count
                        &msgQueueTail
                        );
  if (EFI_ERROR (Status)) {
    ASSERT (0);
  }

  return Status;
}

/*!
 * @brief Convert NVDM Type to SEID type
 *
 * @param[in] nvdmType       NVIDIA Message type
 *
 * @return SEID field for MCTP Transport Header
 */
STATIC UINT8
EFIAPI
uefifspRpcGetSeidFromNvdm (
  UINT8  nvdmType
  )
{
  UINT8  seid;

  switch (nvdmType) {
    case NVDM_TYPE_INFOROM:
      seid = 1;
      break;
    case NVDM_TYPE_HULK:
    default:
      seid = 0;
      break;
  }

  return seid;
}

/*!
 * @brief Create MCTP header
 *
 * @param[in] packetSequence Packet sequence (First packet = 1)
 * @param[in] numberPackets  Total number of packets
 * @param[in] nvdmType       NVIDIA Message type
 *
 * @return Constructed MCTP header
 */
STATIC UINT32
uefifspRpcCreateMctpTransportHeader (
  UINT32   nvdmType,
  UINT32   packetSequence,
  BOOLEAN  bLastPacket
  )
{
  UINT8  som  = 0U;
  UINT8  eom  = 0U;
  UINT8  seid = uefifspRpcGetSeidFromNvdm (nvdmType);
  UINT8  seq  = 0U;

  if ((packetSequence == 0U) && (bLastPacket == TRUE)) {
    // Single packet message
    som = 1U;
    eom = 1U;
  } else if ((bLastPacket == TRUE)) {
    // End packet in a multipacket message
    eom = 1U;
    seq = packetSequence % 4U;
  } else if (packetSequence == 0U) {
    // Start packet in a multipacket message
    som = 1U;
  } else {
    // Intermediate packet in a multipacket message
    seq = packetSequence % 4U;
  }

  return REF_NUM (MCTP_HEADER_SOM, som)   |
         REF_NUM (MCTP_HEADER_EOM, eom)   |
         REF_NUM (MCTP_HEADER_SEID, seid) |
         REF_NUM (MCTP_HEADER_SEQ, seq);
}

/*!
 * @brief Create MCTP payload header
 *
 * @param[in] nvdmType NVDM type to include in header
 *
 * @return Constructed NVDM payload header
 */
STATIC UINT32
EFIAPI
uefifspRpcCreateMctpPayloadHeader (
  UINT32  nvdmType
  )
{
  return REF_DEF (MCTP_MSG_HEADER_TYPE, _VENDOR_PCI) |
         REF_DEF (MCTP_MSG_HEADER_VENDOR_ID, _NV)    |
         REF_NUM (MCTP_MSG_HEADER_NVDM_TYPE, nvdmType);
}

/*!
 * @brief Validate all the expected fields in the payload header.
 *
 * @param[in] mctpPayloadHeader Payload header from the message.
 *
 * @return TRUE  if all expected fields are present in the header.
 *         FALSE otherwise
 */
STATIC BOOLEAN
EFIAPI
uefifspRpcValidateMctpPayloadHeader (
  UINT32  mctpPayloadHeader
  )
{
  UINT16  mctpVendorId;
  UINT8   mctpMessageType;

  mctpMessageType = REF_VAL (MCTP_MSG_HEADER_TYPE, mctpPayloadHeader);
  if (mctpMessageType != MCTP_MSG_HEADER_TYPE_VENDOR_PCI) {
    DEBUG ((
      DEBUG_INFO,
      "Invalid MCTP Message type 0x%0x, expecting 0x7e (Vendor Defined PCI)\n",
      mctpMessageType
      ));

    return FALSE;
  }

  mctpVendorId = REF_VAL (MCTP_MSG_HEADER_VENDOR_ID, mctpPayloadHeader);
  if (mctpVendorId != MCTP_MSG_HEADER_VENDOR_ID_NV) {
    DEBUG ((
      DEBUG_INFO,
      "Invalid PCI Vendor Id 0x%0x, expecting 0x10de (Nvidia)\n",
      mctpVendorId
      ));

    return FALSE;
  }

  return TRUE;
}

/*!
 * @brief determine whether message received fits in a single packet.
 *
 * @param[in] mctpHeader MCTP Header retrieved from the packet
 *
 * @return TRUE  if message can fit in a single packet
 *         FALSE otherwise
 */
STATIC FSP_RPC_MCTP_PACKET_STATE
EFIAPI
uefifspGetPacketInfo (
  UINT32  mctpHeader
  )
{
  UINT8  som;
  UINT8  eom;

  som = REF_VAL (MCTP_HEADER_SOM, mctpHeader);
  eom = REF_VAL (MCTP_HEADER_EOM, mctpHeader);

  if ((som == 1U) && (eom == 0U)) {
    return MCTP_PACKET_STATE_START;
  } else if ((som == 0U) && (eom == 1U)) {
    return MCTP_PACKET_STATE_END;
  } else if ((som == 1U) && (eom == 1U)) {
    return MCTP_PACKET_STATE_SINGLE_PACKET;
  } else {
    return MCTP_PACKET_STATE_INTERMEDIATE;
  }
}

///
/// Diagnostic code for FSP communication
///

STATIC
VOID
EFIAPI
PrintNvdmMessage (
  UINT8  *NvdmMessage,
  UINT8  MsgSize
  )
{
  UINT8  Index;

  if (NULL == NvdmMessage) {
    return;
  }

  if (MsgSize == 0) {
    return;
  }

  for ( Index = 0; Index < MsgSize; Index++) {
    DEBUG ((
      DEBUG_INFO,
      "%a0x%02x%a",
      ((Index == 0) ? "Msg = {" : ""),
      *(NvdmMessage+Index),
      (((Index+1) == (MsgSize)) ? "}\n" : ", ")
      ));
  }
}

/*!
 * @brief Dump debug registers for FSP
 *
 * @param[in] PciIo       PciIo protocol handle
 *
 * @return EFI_SUCCESS, or error if failed
 */
STATIC  EFI_STATUS
EFIAPI
uefifspDumpDebugState (
  IN EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  return EFI_SUCCESS;
}

///
/// FSP RPC code to process packets for EGM and ATS Range Info.
///

/*
 * @brief Configure the EMEM data write to enable auto increment
 *
 * @param[in] PciIo         PciIo protocol handle
*  @param[in] offset        Index of channel to configure for block and offset.
 * @param[in] bAutoIncWr    Boolean of state to set for auto-inrement writes.
 * @param[in] bAutoIncRd    Boolean of state to set for auto-increment reads.
 *
 * @return Status
 *            EFI_SUCCESS
 *            EFI_INVALID_PARAMETER - NULL PciIo pointer
 */
EFI_STATUS
EFIAPI
FspConfigurationSetAutoIncrement (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               offset,
  IN BOOLEAN              bAutoIncWr,
  IN BOOLEAN              bAutoIncRd
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT32      RegVal = 0;
  UINT32      offsetBlks;
  UINT32      offsetDwords;
  UINT32      channelId = FSP_EMEM_CHANNEL_RM;

  if (NULL == PciIo) {
    return EFI_INVALID_PARAMETER;
  }

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        PCI_BAR_IDX0,
                        NV_PFSP_EMEMC (channelId),
                        1,
                        &RegVal
                        );

  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo read of '%a' returned '%r'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMC(channelId)", Status));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo read of '%a' = '0x%08x'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMC(channelId)", RegVal));

  RegVal       = FLD_SET_DRF (_PFSP, _EMEMC, _AINCW, _TRUE, RegVal);
  offsetBlks   = offset / FSP_RPC_DWORDS_PER_EMEM_BLOCK;
  offsetDwords = offset % FSP_RPC_DWORDS_PER_EMEM_BLOCK;

  RegVal = FLD_SET_DRF_NUM (_PFSP, _EMEMC, _OFFS, offsetDwords, RegVal);
  RegVal = FLD_SET_DRF_NUM (_PFSP, _EMEMC, _BLK, offsetBlks, RegVal);

  if (bAutoIncWr) {
    RegVal = FLD_SET_DRF (_PFSP, _EMEMC, _AINCW, _TRUE, RegVal);
  } else {
    RegVal = FLD_SET_DRF (_PFSP, _EMEMC, _AINCW, _FALSE, RegVal);
  }

  if (bAutoIncRd) {
    RegVal = FLD_SET_DRF (_PFSP, _EMEMC, _AINCR, _TRUE, RegVal);
  } else {
    RegVal = FLD_SET_DRF (_PFSP, _EMEMC, _AINCR, _FALSE, RegVal);
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo write of '%a' = '0x%08x'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMC(channelId)", RegVal));

  Status = PciIo->Mem.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        PCI_BAR_IDX0,
                        NV_PFSP_EMEMC (channelId),
                        1,
                        &RegVal
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] PciIo write '%a' returned '%r'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMC(channelId)", Status));
  }

  return Status;
}

/*
 * @brief Write the ATS Range via FSP RPC interface
 *
 * @param[in] PciIo         PciIo protocol handle
 * @param[in] HbmBasePa     HbM  base physical address
 *
 * @return Status
 *            EFI_SUCCESS
 *            EFI_OUT_OF_RESOURCES
 *            EFI_INVALID_PARAMETER - NULL PciIo pointer
 */
EFI_STATUS
EFIAPI
FspConfigurationAtsRange (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  UINT64                  HbmBasePa
  )
{
  EFI_STATUS         Status             = EFI_SUCCESS;
  UINT8              *cmdQueueBuffer    = NULL;
  UINT32             nvdmType           = 0;
  UINT32             packetSequence     = 0;      /* One packet */
  BOOLEAN            bLastPacket        = TRUE;
  UINT32             cmdQueueSize       = NV_ALIGN_UP (sizeof (FINAL_MESSAGE_ATS), sizeof (UINT32));
  UINT32             cmdQueueSizeDwords = cmdQueueSize/sizeof (UINT32);
  UINT32             cmdQueueOffset     = 0;
  UINT32             channelId          = FSP_EMEM_CHANNEL_RM;
  UINT32             Index;
  UINT32             queueHead               = 0;
  UINT32             queueTail               = 0;
  UINT32             msgQueueHead            = 0;
  UINT32             msgQueueTail            = 0;
  FINAL_MESSAGE_ATS  *Nvdm_Final_Message_Ats = NULL;
  UINT32             RegVal;
  UINT32             OffsetDwords;
  UINT32             *msgQueueBuffer   = NULL;
  UINT8              msgQueueSizeBytes = NV_ALIGN_UP (NVDM_PAYLOAD_COMMAND_RESPONSE_SIZE, sizeof (UINT32));
  UINT8              msgQueueSizeDwords;
  BOOLEAN            bResponseAck = FALSE;

  /* Allocate command queue buffer (DWORD aligned) */
  cmdQueueBuffer = AllocateZeroPool (cmdQueueSize);

  if (cmdQueueBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  msgQueueBuffer = (UINT32 *)AllocateZeroPool (msgQueueSizeBytes);

  if (msgQueueBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Buffer Allocation failed '%r'\n", __FUNCTION__, Status));
    goto fspRpcResponseReceivePacketFree_exit;
  }

  Nvdm_Final_Message_Ats = (FINAL_MESSAGE_ATS *)cmdQueueBuffer;

  /* State check */
  DEBUG_CODE_BEGIN ();
  /* Check command queue empty */
  Status = uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

  Status = uefifspRpcMsgQueueHeadTailGet (PciIo, channelId, &msgQueueHead, &msgQueueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: [%p] ERROR: Message Queue status check returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
  }

  DEBUG ((DEBUG_INFO, "%a: Message Queue [Channel:%d, Head:0x%04x, Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, channelId, msgQueueHead, msgQueueTail, ((msgQueueHead == msgQueueTail) ? "TRUE" : "FALSE")));
  Status = uefifspDumpDebugState (PciIo);
  if (EFI_ERROR (Status)) {
    ASSERT (0);
  }

  DEBUG_CODE_END ();

  /* Check command queue empty */
  Status = uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

  Index = UEFI_FSP_RPC_CMD_QUEUE_POLL_TIMEOUT_INDEX;
  while ((queueHead != queueTail) && (--Index)) {
    Status = uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: [%p] ERROR: Command Queue status check returned '%r'\n", __FUNCTION__, PciIo, Status));
      ASSERT (0);
    }

    if (queueHead != queueTail) {
      gBS->Stall (UEFI_STALL_DELAY_UNITS);
    }
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

  if (!Index) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: Command Queue empty check timed out.\n", __FUNCTION__, PciIo));
    Status = uefifspDumpDebugState (PciIo);
    if (EFI_ERROR (Status)) {
      ASSERT (0);
    }

    goto uefifspRpcResponseReceivePacket_exit;
  }

  /* Build message Buffer - ATS Message Type */
  *((UINT32 *)&Nvdm_Final_Message_Ats->Mctp_Header_S) = uefifspRpcCreateMctpTransportHeader (nvdmType, packetSequence, bLastPacket);

  /* ATS Message Type */
  *((UINT32 *)&Nvdm_Final_Message_Ats->Nvdm_Header_S)      = uefifspRpcCreateMctpPayloadHeader (NVDM_TYPE_UEFI_RM);
  Nvdm_Final_Message_Ats->Nvdm_Uefi_Ats_Fsp_S.subMessageId = 0x3;  // Sub-Message for ATS Range Info from UEFI DXE to FSP
  Nvdm_Final_Message_Ats->Nvdm_Uefi_Ats_Fsp_S.Hbm_Base     = HbmBasePa;

  PrintNvdmMessage ((UINT8 *)cmdQueueBuffer, sizeof (FINAL_MESSAGE_ATS));

  /*                                         PciIo, offset, writeAutoInc, readAutoInc */
  Status = FspConfigurationSetAutoIncrement (PciIo, 0, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: EMEMC configuration returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
    goto uefifspRpcResponseReceivePacket_exit;
  }

  /* Check command queue empty */
  Status = uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
    goto uefifspRpcResponseReceivePacket_exit;
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

  Index = UEFI_FSP_RPC_CMD_QUEUE_POLL_TIMEOUT_INDEX;
  while ((queueHead != queueTail) && (Index--)) {
    uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        PCI_BAR_IDX0,
                        NV_PFSP_EMEMC (channelId),
                        1,
                        &RegVal
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: EMEMC read returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
    goto uefifspRpcResponseReceivePacket_exit;
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo read of '%a' returned '%r'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMC(channelId)", Status));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo read of '%a' [0x%08x] = '0x%08x'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMC(channelId)", NV_PFSP_EMEMC (channelId), RegVal));

  for (Index = 0; Index < cmdQueueSizeDwords; Index++) {
    DEBUG ((DEBUG_INFO, "%a: [%p] PciIo write of '%a' = '0x%08x'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMD(channelId)", ((((UINT32 *)(VOID *)cmdQueueBuffer)[Index]))));
    Status = PciIo->Mem.Write (
                          PciIo,
                          EfiPciIoWidthUint32,
                          PCI_BAR_IDX0,
                          NV_PFSP_EMEMD (channelId),
                          1,
                          &(((UINT32 *)(VOID *)cmdQueueBuffer)[Index])
                          );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: EMEMD(%d) write returned '%r'\n", __FUNCTION__, PciIo, Index, Status));
      ASSERT (0);
      goto uefifspRpcResponseReceivePacket_exit;
    }

    DEBUG ((DEBUG_ERROR, "%a: [%p] PciIo write of '%a', Index '%d' returned '%r'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMD(channelId)", Index, Status));
  }

  DEBUG ((DEBUG_ERROR, "%a: [%p] PciIo write of '%a' returned '%r'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMD(channelId)", Status));

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        PCI_BAR_IDX0,
                        NV_PFSP_EMEMC (channelId),
                        1,
                        &RegVal
                        );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: EMEMC read returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
    goto uefifspRpcResponseReceivePacket_exit;
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo read of '%a' returned '%r'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMC(channelId)", Status));
  OffsetDwords = (DRF_VAL (_PFSP, _EMEMC, _BLK, RegVal) * FSP_RPC_DWORDS_PER_EMEM_BLOCK) + DRF_VAL (_PFSP, _EMEMC, _OFFS, RegVal);
  DEBUG ((DEBUG_INFO, "%a: [%p] Sanity of '%a', '%a'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMC(channelId)", ((OffsetDwords == cmdQueueSize) ? "TRUE" : "FALSE")));

  /* Compute queue head and tail from message size */
  queueTail = cmdQueueOffset + cmdQueueSize - FSP_RPC_BYTES_PER_DWORD;
  queueHead = cmdQueueOffset;
  DEBUG ((DEBUG_INFO, "%a: [0x%04x:0x%04x] check message size against queueTail HD\n", __FUNCTION__, cmdQueueSize, queueTail));

  /* Trigger */
  Status = uefifspRpcQueueHeadTailRequestSet (PciIo, channelId, queueHead, queueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: Command Queue Head/Tail configuration returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
    goto uefifspRpcResponseReceivePacket_exit;
  }

  /* Check command queue set */
  Status = uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
    goto uefifspRpcResponseReceivePacket_exit;
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

  Status = uefifspPollForMsgQueueResponse (PciIo, channelId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ERROR: message queue poll returned status '%r'\n", __FUNCTION__, Status));
    /* Allow timeout to progress as follow on message queue check will gate additional code execution */
    if (Status != EFI_TIMEOUT) {
      goto uefifspRpcResponseReceivePacket_exit;
    }
  }

  /* If there is no timeout, process the FSP Response Payload from the Message Queue */
  Status = uefifspRpcMsgQueueHeadTailGet (PciIo, channelId, &msgQueueHead, &msgQueueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ERROR: message queue get returned status '%r'\n", __FUNCTION__, Status));
  }

  DEBUG ((DEBUG_ERROR, "%a: Message Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, msgQueueHead, msgQueueTail, ((msgQueueHead == msgQueueTail) ? "TRUE" : "FALSE")));

  /* Message Queue Response payload processing */
  if (msgQueueHead != msgQueueTail) {
    msgQueueSizeBytes  = MIN ((msgQueueTail-msgQueueHead+FSP_RPC_BYTES_PER_DWORD), FSP_RPC_RESPONSE_PACKET_SIZE) + sizeof (UINT32);
    msgQueueSizeDwords = (NV_ALIGN_UP (msgQueueSizeBytes, sizeof (UINT32))/FSP_RPC_BYTES_PER_DWORD);
    DEBUG ((DEBUG_INFO, "%a: MsgQueue [Max byte index:%d, Max dword index:%d]\n", __FUNCTION__, msgQueueSizeBytes, msgQueueSizeDwords));

    /*                                         PciIo, offset, writeAutoInc, readAutoInc */
    Status = FspConfigurationSetAutoIncrement (PciIo, 0, FALSE, TRUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: EMEMC configuration returned '%r'\n", __FUNCTION__, PciIo, Status));
      ASSERT (0);
      goto uefifspRpcResponseReceivePacket_exit;
    }

    /* read message queue from head to tail and then clear message queue */
    for (Index = msgQueueHead; CONVERT_DWORD_COUNT_TO_BYTE_SIZE (Index) <= msgQueueTail; Index++) {
      Status = PciIo->Mem.Read (
                            PciIo,
                            EfiPciIoWidthUint32,
                            PCI_BAR_IDX0,
                            NV_PFSP_EMEMD (channelId),
                            1,
                            &RegVal
                            );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: EMEMD Index=%d check returned '%r'\n", __FUNCTION__, PciIo, Index, Status));
        ASSERT (0);
        goto uefifspRpcResponseReceivePacket_exit;
      }

      DEBUG ((DEBUG_INFO, "%a: [%p][Index:%d] PciIo read of '%a' [0x%08x] = '0x%08x'\n", __FUNCTION__, PciIo, Index, "NV_PFSP_EMEMD (channelId)", NV_PFSP_EMEMD (channelId), RegVal));
      msgQueueBuffer[Index] = RegVal;
    }

    /* Verify message queue size against response packet size before processing */
    if (CONVERT_DWORD_COUNT_TO_BYTE_SIZE (msgQueueSizeDwords) >= FSP_RPC_RESPONSE_PACKET_SIZE) {
      UINT32  nvdmMsgHeaderNvdmType      = REF_VAL (MCTP_MSG_HEADER_NVDM_TYPE, msgQueueBuffer[1]);
      UINT32  nvdmResponsePayloadThread  = msgQueueBuffer[2];
      UINT32  nvdmResponsePayloadCmdId   = msgQueueBuffer[3];
      UINT32  nvdmResponsePayloadErrCode = msgQueueBuffer[4];

      /* Current UEFI implementation only supports single packet */
      if ( MCTP_PACKET_STATE_SINGLE_PACKET != uefifspGetPacketInfo (msgQueueBuffer[0])) {
        DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: Index=%d Packet Info '0x%08x'\n", __FUNCTION__, PciIo, 0, uefifspGetPacketInfo (msgQueueBuffer[0])));
        goto uefifspRpcResponseReceivePacket_exit;
      }

      /* Process MctpPayload Message Heaader */
      if (!uefifspRpcValidateMctpPayloadHeader (msgQueueBuffer[1])) {
        DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: Index=%d MCTP Payload Header check failed '0x%08x\n", __FUNCTION__, PciIo, 1, msgQueueBuffer[1]));
        goto uefifspRpcResponseReceivePacket_exit;
      }

      /* Process NVMT payload for FSP response payload type */
      if ( NVDM_TYPE_FSP_RESPONSE == nvdmMsgHeaderNvdmType) {
        DEBUG ((DEBUG_INFO, "%a: MCTP message header NVDM Type - matched 'NVDM_TYPE_UEFI_RM'.\n", __FUNCTION__));
        if ((nvdmResponsePayloadErrCode == FSP_OK) && (nvdmResponsePayloadCmdId == NVDM_TYPE_UEFI_RM)) {
          DEBUG ((DEBUG_INFO, "%a: MCTP message Cmd and ErrCode matched.\n", __FUNCTION__));
          bResponseAck = TRUE;
        }

        DEBUG_CODE_BEGIN ();
        DEBUG ((DEBUG_INFO, "%a: FSP Response Packet Thread ID '0x%08x'\n", __FUNCTION__, nvdmResponsePayloadThread));
        DEBUG ((DEBUG_INFO, "%a: FSP Response Packet Command ID '0x%08x'\n", __FUNCTION__, nvdmResponsePayloadCmdId));
        DEBUG ((DEBUG_INFO, "%a: FSP Response Packet Error Code '0x%08x'\n", __FUNCTION__, nvdmResponsePayloadErrCode));
        DEBUG_CODE_END ();
      } else {
        DEBUG ((DEBUG_ERROR, "%a: ERROR; Expected MCTP message header NVDM Type - matching 'NVDM_TYPE_UEFI_RM'.\n", __FUNCTION__));
      }
    }
  } else {
    DEBUG_CODE_BEGIN ();
    {
      EFI_STATUS  DebugStatus;
      DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR Expected Message Queue Response [Head:0x%04x, Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, msgQueueHead, msgQueueTail, ((msgQueueHead == msgQueueTail) ? "TRUE" : "FALSE")));
      DebugStatus = uefifspDumpDebugState (PciIo);
      /* Only propagate error status with DebugDumpState status if no prior error */
      if (!EFI_ERROR (Status)) {
        Status = DebugStatus;
      }
    }
    DEBUG_CODE_END ();
  }

  if (bResponseAck) {
    /* ACK packet with update where tail equals head */
    msgQueueTail = msgQueueHead;

    Status = uefifspRpcMsgQueueHeadTailSet (PciIo, channelId, msgQueueHead, msgQueueTail);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: [%p] Message Queue Set (Head:0x%02x,Tail:0x%02x) returned '%r'\n", __FUNCTION__, PciIo, msgQueueHead, msgQueueTail, Status));
      goto uefifspRpcResponseReceivePacket_exit;
    }

    DEBUG_CODE_BEGIN ();
    Status = uefifspRpcMsgQueueHeadTailGet (PciIo, channelId, &msgQueueHead, &msgQueueTail);
    if (EFI_ERROR (Status)) {
      ASSERT (0);
    }

    DEBUG ((DEBUG_ERROR, "%a: Message Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, msgQueueHead, msgQueueTail, ((msgQueueHead == msgQueueTail) ? "TRUE" : "FALSE")));
    Status = uefifspDumpDebugState (PciIo);
    if (EFI_ERROR (Status)) {
      ASSERT (0);
    }

    Status = uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: Command Queue Head/Tail configuration returned '%r'\n", __FUNCTION__, PciIo, Status));
      ASSERT (0);
    }

    DEBUG ((DEBUG_INFO, "%a: [%p] [Head:0x%04x, Tail:0x%04x] check command queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

    DEBUG_CODE_END ();
  } /* Response ACK */

uefifspRpcResponseReceivePacket_exit:
  if (NULL != msgQueueBuffer) {
    FreePool (msgQueueBuffer);
  }

  DEBUG_CODE_BEGIN ();
  Status = uefifspDumpDebugState (PciIo);
  if (EFI_ERROR (Status)) {
    ASSERT (0);
  }

  DEBUG_CODE_END ();

fspRpcResponseReceivePacketFree_exit:
  if (cmdQueueBuffer) {
    FreePool (cmdQueueBuffer);
  }

  return Status;
}

/*
 * @brief Write the EGM Base and Size via FSP RPC interface
 *
 * @param[in] PciIo         PciIo protocol handle
 * @param[in] EgmBasePa     EGM base physical address
 * @param[in] EgmSize       EGM size
 *
 * @return Status
 *            EFI_SUCCESS
 *            EFI_OUT_OF_RESOURCES
 *            EFI_INVALID_PARAMETER - NULL PciIo
 */
EFI_STATUS
EFIAPI
FspConfigurationEgmBaseAndSize (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  UINT64                  EgmBasePa,
  UINT64                  EgmSize
  )
{
  EFI_STATUS         Status             = EFI_SUCCESS;
  UINT8              *cmdQueueBuffer    = NULL;
  UINT32             nvdmType           = 0;
  UINT32             packetSequence     = 0;      /* One packet */
  BOOLEAN            bLastPacket        = TRUE;
  UINT32             cmdQueueSize       = NV_ALIGN_UP (sizeof (FINAL_MESSAGE_EGM), sizeof (UINT32));
  UINT32             cmdQueueSizeDwords = cmdQueueSize/sizeof (UINT32);
  UINT32             cmdQueueOffset     = 0;
  UINT32             channelId          = FSP_EMEM_CHANNEL_RM;
  UINT32             Index;
  UINT32             queueHead               = 0;
  UINT32             queueTail               = 0;
  UINT32             msgQueueHead            = 0;
  UINT32             msgQueueTail            = 0;
  FINAL_MESSAGE_EGM  *Nvdm_Final_Message_Egm = NULL;
  UINT32             RegVal;
  UINT32             OffsetDwords;
  UINT32             *msgQueueBuffer   = NULL;
  UINT8              msgQueueSizeBytes = NV_ALIGN_UP (NVDM_PAYLOAD_COMMAND_RESPONSE_SIZE, sizeof (UINT32));
  UINT8              msgQueueSizeDwords;
  BOOLEAN            bResponseAck = FALSE;

  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_INFO, "%a: [%p] Params [egm-base-pa:0x%016lx,egm-size:0x%016lx]\n", __FUNCTION__, PciIo, EgmBasePa, EgmSize));
  DEBUG_CODE_END ();

  /* Allocate command queue buffer (DWORD aligned) */
  cmdQueueBuffer = AllocateZeroPool (cmdQueueSize);

  if (cmdQueueBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  msgQueueBuffer = (UINT32 *)AllocateZeroPool (msgQueueSizeBytes);

  if (msgQueueBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Buffer Allocation failed '%r'\n", __FUNCTION__, Status));
    goto uefifspRpcCmdQueueBuffer_exit;
  }

  Nvdm_Final_Message_Egm = (FINAL_MESSAGE_EGM *)cmdQueueBuffer;

  /* State check */
  DEBUG_CODE_BEGIN ();
  /* Check command queue empty */
  Status = uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

  Status = uefifspRpcMsgQueueHeadTailGet (PciIo, channelId, &msgQueueHead, &msgQueueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: [%p] ERROR: Message Queue status check returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
  }

  DEBUG ((DEBUG_INFO, "%a: Message Queue [Channel:%d, Head:0x%04x, Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, channelId, msgQueueHead, msgQueueTail, ((msgQueueHead == msgQueueTail) ? "TRUE" : "FALSE")));
  Status = uefifspDumpDebugState (PciIo);
  if (EFI_ERROR (Status)) {
    ASSERT (0);
  }

  DEBUG_CODE_END ();

  /* Check command queue empty */
  Status = uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

  Index = UEFI_FSP_RPC_CMD_QUEUE_POLL_TIMEOUT_INDEX;
  while ((queueHead != queueTail) && (--Index)) {
    Status = uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: [%p] ERROR: Command Queue status check returned '%r'\n", __FUNCTION__, PciIo, Status));
      ASSERT (0);
    }

    if (queueHead != queueTail) {
      gBS->Stall (UEFI_STALL_DELAY_UNITS);
    }
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

  if (!Index) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: Command Queue empty check timed out.\n", __FUNCTION__, PciIo));
    Status = uefifspDumpDebugState (PciIo);
    if (EFI_ERROR (Status)) {
      ASSERT (0);
    }

    goto uefifspRpcResponseReceivePacket_exit;
  }

  /* Build message Buffer - EGM_Message_Type */
  *((UINT32 *)&Nvdm_Final_Message_Egm->Mctp_Header_S) = uefifspRpcCreateMctpTransportHeader (nvdmType, packetSequence, bLastPacket);

  /* EGM Message Type */
  *((UINT32 *)&Nvdm_Final_Message_Egm->Nvdm_Header_S)      = uefifspRpcCreateMctpPayloadHeader (NVDM_TYPE_UEFI_RM);
  Nvdm_Final_Message_Egm->Nvdm_Uefi_Egm_Fsp_S.subMessageId = 0x1;  // Sub-Message for EGM info from UEFI DXE to FSP
  Nvdm_Final_Message_Egm->Nvdm_Uefi_Egm_Fsp_S.Egm_Base     = EgmBasePa;
  Nvdm_Final_Message_Egm->Nvdm_Uefi_Egm_Fsp_S.Egm_Size     = EgmSize;

  PrintNvdmMessage ((UINT8 *)cmdQueueBuffer, sizeof (FINAL_MESSAGE_EGM));

  /*                                         PciIo, offset, writeAutoInc, readAutoInc */
  Status = FspConfigurationSetAutoIncrement (PciIo, 0, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: EMEMC configuration returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
    goto uefifspRpcResponseReceivePacket_exit;
  }

  // sync this block from ATS Range Info section
  /* Check command queue empty */
  Status = uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
    goto uefifspRpcResponseReceivePacket_exit;
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

  Index = UEFI_FSP_RPC_CMD_QUEUE_POLL_TIMEOUT_INDEX;
  while ((queueHead != queueTail) && (Index--)) {
    uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        PCI_BAR_IDX0,
                        NV_PFSP_EMEMC (channelId),
                        1,
                        &RegVal
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: EMEMC read returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
    goto uefifspRpcResponseReceivePacket_exit;
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo read of '%a' returned '%r'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMC(channelId)", Status));
  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo read of '%a' [0x%08x] = '0x%08x'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMC(channelId)", NV_PFSP_EMEMC (channelId), RegVal));

  for (Index = 0; Index < cmdQueueSizeDwords; Index++) {
    DEBUG ((DEBUG_INFO, "%a: [%p] PciIo write of '%a' = '0x%08x'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMD(channelId)", ((((UINT32 *)(VOID *)cmdQueueBuffer)[Index]))));
    Status = PciIo->Mem.Write (
                          PciIo,
                          EfiPciIoWidthUint32,
                          PCI_BAR_IDX0,
                          NV_PFSP_EMEMD (channelId),
                          1,
                          &(((UINT32 *)(VOID *)cmdQueueBuffer)[Index])
                          );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: EMEMD(%d) write returned '%r'\n", __FUNCTION__, PciIo, Index, Status));
      ASSERT (0);
      goto uefifspRpcResponseReceivePacket_exit;
    }

    DEBUG ((DEBUG_ERROR, "%a: [%p] PciIo write of '%a', Index '%d' returned '%r'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMD(channelId)", Index, Status));
  }

  DEBUG ((DEBUG_ERROR, "%a: [%p] PciIo write of '%a' returned '%r'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMD(channelId)", Status));

  Status = PciIo->Mem.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        PCI_BAR_IDX0,
                        NV_PFSP_EMEMC (channelId),
                        1,
                        &RegVal
                        );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: EMEMC read returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
    goto uefifspRpcResponseReceivePacket_exit;
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] PciIo read of '%a' returned '%r'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMC(channelId)", Status));
  OffsetDwords = (DRF_VAL (_PFSP, _EMEMC, _BLK, RegVal) * FSP_RPC_DWORDS_PER_EMEM_BLOCK) + DRF_VAL (_PFSP, _EMEMC, _OFFS, RegVal);
  DEBUG ((DEBUG_INFO, "%a: [%p] Sanity of '%a', '%a'\n", __FUNCTION__, PciIo, "NV_PFSP_EMEMC(channelId)", ((OffsetDwords == cmdQueueSize) ? "TRUE" : "FALSE")));

  /* Compute queue head and tail from message size */
  queueTail = cmdQueueOffset + cmdQueueSize - FSP_RPC_BYTES_PER_DWORD;
  queueHead = cmdQueueOffset;
  DEBUG ((DEBUG_INFO, "%a: [0x%04x:0x%04x] check message size against queueTail HD\n", __FUNCTION__, cmdQueueSize, queueTail));

  /* Trigger */
  Status = uefifspRpcQueueHeadTailRequestSet (PciIo, channelId, queueHead, queueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: Command Queue Head/Tail configuration returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
    goto uefifspRpcResponseReceivePacket_exit;
  }

  /* Check command queue set */
  Status = uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
    ASSERT (0);
    goto uefifspRpcResponseReceivePacket_exit;
  }

  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue check returned '%r'\n", __FUNCTION__, PciIo, Status));
  DEBUG ((DEBUG_INFO, "%a: [%p] Command Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

  Status = uefifspPollForMsgQueueResponse (PciIo, channelId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ERROR: message queue poll returned status '%r'\n", __FUNCTION__, Status));
    /* Allow timeout to progress as follow on message queue check will gate additional code execution */
    if (Status != EFI_TIMEOUT) {
      goto uefifspRpcResponseReceivePacket_exit;
    }
  }

  /* If there is no timeout, process the FSP Response Payload from the Message Queue */
  Status = uefifspRpcMsgQueueHeadTailGet (PciIo, channelId, &msgQueueHead, &msgQueueTail);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ERROR: message queue get returned status '%r'\n", __FUNCTION__, Status));
  }

  DEBUG ((DEBUG_ERROR, "%a: Message Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, msgQueueHead, msgQueueTail, ((msgQueueHead == msgQueueTail) ? "TRUE" : "FALSE")));

  /* Message Queue Response payload processing */
  if (msgQueueHead != msgQueueTail) {
    msgQueueSizeBytes  = MIN ((msgQueueTail-msgQueueHead+FSP_RPC_BYTES_PER_DWORD), FSP_RPC_RESPONSE_PACKET_SIZE) + sizeof (UINT32);
    msgQueueSizeDwords = (NV_ALIGN_UP (msgQueueSizeBytes, sizeof (UINT32))/FSP_RPC_BYTES_PER_DWORD);
    DEBUG ((DEBUG_INFO, "%a: MsgQueue [Max byte index:%d, Max dword index:%d]\n", __FUNCTION__, msgQueueSizeBytes, msgQueueSizeDwords));

    /*                                         PciIo, offset, writeAutoInc, readAutoInc */
    Status = FspConfigurationSetAutoIncrement (PciIo, 0, FALSE, TRUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: EMEMC configuration returned '%r'\n", __FUNCTION__, PciIo, Status));
      ASSERT (0);
      goto uefifspRpcResponseReceivePacket_exit;
    }

    /* read message queue from head to tail and then clear message queue */
    for (Index = msgQueueHead; CONVERT_DWORD_COUNT_TO_BYTE_SIZE (Index) <= msgQueueTail; Index++) {
      Status = PciIo->Mem.Read (
                            PciIo,
                            EfiPciIoWidthUint32,
                            PCI_BAR_IDX0,
                            NV_PFSP_EMEMD (channelId),
                            1,
                            &RegVal
                            );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: EMEMD Index=%d check returned '%r'\n", __FUNCTION__, PciIo, Index, Status));
        ASSERT (0);
        goto uefifspRpcResponseReceivePacket_exit;
      }

      DEBUG ((DEBUG_INFO, "%a: [%p][Index:%d] PciIo read of '%a' [0x%08x] = '0x%08x'\n", __FUNCTION__, PciIo, Index, "NV_PFSP_EMEMD (channelId)", NV_PFSP_EMEMD (channelId), RegVal));
      msgQueueBuffer[Index] = RegVal;
    }

    /* Verify message queue size against response packet size before processing */
    if (CONVERT_DWORD_COUNT_TO_BYTE_SIZE (msgQueueSizeDwords) >= FSP_RPC_RESPONSE_PACKET_SIZE) {
      UINT32  nvdmMsgHeaderNvdmType      = REF_VAL (MCTP_MSG_HEADER_NVDM_TYPE, msgQueueBuffer[1]);
      UINT32  nvdmResponsePayloadThread  = msgQueueBuffer[2];
      UINT32  nvdmResponsePayloadCmdId   = msgQueueBuffer[3];
      UINT32  nvdmResponsePayloadErrCode = msgQueueBuffer[4];

      /* Current UEFI implementation only supports single packet */
      if ( MCTP_PACKET_STATE_SINGLE_PACKET != uefifspGetPacketInfo (msgQueueBuffer[0])) {
        DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: Index=%d Packet Info '0x%08x'\n", __FUNCTION__, PciIo, 0, uefifspGetPacketInfo (msgQueueBuffer[0])));
        goto uefifspRpcResponseReceivePacket_exit;
      }

      /* Process MctpPayload Message Heaader */
      if (!uefifspRpcValidateMctpPayloadHeader (msgQueueBuffer[1])) {
        DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: Index=%d MCTP Payload Header check failed '0x%08x\n", __FUNCTION__, PciIo, 1, msgQueueBuffer[1]));
        goto uefifspRpcResponseReceivePacket_exit;
      }

      /* Process NVMT payload for FSP response payload type */
      if ( NVDM_TYPE_FSP_RESPONSE == nvdmMsgHeaderNvdmType) {
        DEBUG ((DEBUG_INFO, "%a: MCTP message header NVDM Type - matched 'NVDM_TYPE_UEFI_RM'.\n", __FUNCTION__));
        if ((nvdmResponsePayloadErrCode == FSP_OK) && (nvdmResponsePayloadCmdId == NVDM_TYPE_UEFI_RM)) {
          DEBUG ((DEBUG_INFO, "%a: MCTP message Cmd and ErrCode matched.\n", __FUNCTION__));
          bResponseAck = TRUE;
        }

        DEBUG_CODE_BEGIN ();
        DEBUG ((DEBUG_INFO, "%a: FSP Response Packet Thread ID '0x%08x'\n", __FUNCTION__, nvdmResponsePayloadThread));
        DEBUG ((DEBUG_INFO, "%a: FSP Response Packet Command ID '0x%08x'\n", __FUNCTION__, nvdmResponsePayloadCmdId));
        DEBUG ((DEBUG_INFO, "%a: FSP Response Packet Error Code '0x%08x'\n", __FUNCTION__, nvdmResponsePayloadErrCode));
        DEBUG_CODE_END ();
      } else {
        DEBUG ((DEBUG_ERROR, "%a: ERROR; Expected MCTP message header NVDM Type - matching 'NVDM_TYPE_UEFI_RM'.\n", __FUNCTION__));
      }
    }
  } else {
    DEBUG_CODE_BEGIN ();
    {
      EFI_STATUS  DebugStatus;
      DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR Expected Message Queue Response [Head:0x%04x, Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, PciIo, msgQueueHead, msgQueueTail, ((msgQueueHead == msgQueueTail) ? "TRUE" : "FALSE")));
      DebugStatus = uefifspDumpDebugState (PciIo);
      /* Only propagate error status with DebugDumpState status if no prior error */
      if (!EFI_ERROR (Status)) {
        Status = DebugStatus;
      }
    }
    DEBUG_CODE_END ();
  }

  if (bResponseAck) {
    /* ACK packet with update where tail equals head */
    msgQueueTail = msgQueueHead;

    Status = uefifspRpcMsgQueueHeadTailSet (PciIo, channelId, msgQueueHead, msgQueueTail);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: [%p] Message Queue Set (Head:0x%02x,Tail:0x%02x) returned '%r'\n", __FUNCTION__, PciIo, msgQueueHead, msgQueueTail, Status));
      goto uefifspRpcResponseReceivePacket_exit;
    }

    DEBUG_CODE_BEGIN ();
    Status = uefifspRpcMsgQueueHeadTailGet (PciIo, channelId, &msgQueueHead, &msgQueueTail);
    if (EFI_ERROR (Status)) {
      ASSERT (0);
    }

    DEBUG ((DEBUG_ERROR, "%a: Message Queue [Head:0x%04x,Tail:0x%04x] check queue empty[%a] \n", __FUNCTION__, msgQueueHead, msgQueueTail, ((msgQueueHead == msgQueueTail) ? "TRUE" : "FALSE")));
    Status = uefifspDumpDebugState (PciIo);
    if (EFI_ERROR (Status)) {
      ASSERT (0);
    }

    Status = uefifspRpcQueueHeadTailGet (PciIo, channelId, &queueHead, &queueTail);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: [%p] ERROR: Command Queue Head/Tail configuration returned '%r'\n", __FUNCTION__, PciIo, Status));
      ASSERT (0);
    }

    DEBUG ((DEBUG_INFO, "%a: [%p] [Head:0x%04x, Tail:0x%04x] check command queue empty[%a] \n", __FUNCTION__, PciIo, queueHead, queueTail, ((queueHead == queueTail) ? "TRUE" : "FALSE")));

    DEBUG_CODE_END ();
  } /* Response ACK */

uefifspRpcResponseReceivePacket_exit:
  if (NULL != msgQueueBuffer) {
    FreePool (msgQueueBuffer);
  }

  DEBUG_CODE_BEGIN ();
  Status = uefifspDumpDebugState (PciIo);
  if (EFI_ERROR (Status)) {
    ASSERT (0);
  }

  DEBUG_CODE_END ();

uefifspRpcCmdQueueBuffer_exit:
  if (cmdQueueBuffer) {
    FreePool (cmdQueueBuffer);
  }

  return Status;
}
