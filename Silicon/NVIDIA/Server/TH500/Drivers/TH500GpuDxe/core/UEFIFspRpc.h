/** @file

  UEFI client code which implements simple transactions between FSP and UEFI client.

  SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __UEFI_FSP_RPC_H__
#define __UEFI_FSP_RPC_H__

#include <Uefi.h>
#include <Protocol/PciIo.h>

/* UEFI mapping of FSP_RPC defines */
#define FSP_RPC_BYTES_PER_DWORD  sizeof(UINT32)
#define FSP_RPC_NVDM_SIZE        sizeof(UINT32)

// Maximum number of channels supported by FSP RPC code
#define FSP_RPC_MAX_CHANNEL            2U
#define FSP_RPC_DWORDS_PER_EMEM_BLOCK  64U
#define FSP_RPC_EMEM_OFFSET_MAX        8192U

/*
 * NVDM over MCTP Header consists of 4 byte MCTP Transport Header and 1 or 4 byte
 * MCTP Payload Header. MCTP Payload Header contains 1 byte MCTP Message type
 * which is "Vendor Defined - PCI" (0x7E) followed by 2 byte NVIDIA Vendor ID (0x10DE)
 * This is followed by 1 byte NVIDIA Message Type (NVDM). MCTP Meassge type and NVIDIA
 * Vendor ID are only present in first packet in case of multipacket messages hence
 * size of MCTP Payload Header is only 1 byte for these packets.
 * confluence.nvidia.com/display/GFWGH100/FSP+IPC-RPC+Design#FSPIPCRPCDesign-RPCDesign-NvidiaDataModel(NVDM)
 */
#define FSP_RPC_MCTP_TRANSPORT_HEADER_SIZE                sizeof(UINT32)
#define FSP_RPC_MCTP_PAYLOAD_HEADER_SIZE                  sizeof(UINT32)
#define FSP_RPC_HEADER_SIZE_SINGLE_PACKET                 (FSP_RPC_MCTP_TRANSPORT_HEADER_SIZE +              \
                                                                       FSP_RPC_MCTP_PAYLOAD_HEADER_SIZE)
#define FSP_RPC_HEADER_SIZE_FIRST_PACKET_MULTIPACKET      FSP_RPC_HEADER_SIZE_SINGLE_PACKET
#define FSP_RPC_HEADER_SIZE_NON_FIRST_PACKET_MULTIPACKET  FSP_RPC_MCTP_TRANSPORT_HEADER_SIZE

/*
 * Type of packet, can either be SOM, EOM, neither, or both (1-packet messages)
 */
typedef enum {
  MCTP_PACKET_STATE_START,
  MCTP_PACKET_STATE_INTERMEDIATE,
  MCTP_PACKET_STATE_END,
  MCTP_PACKET_STATE_SINGLE_PACKET
} FSP_RPC_MCTP_PACKET_STATE;

///
/// UEFI extensions to the FSP RPC declarations
///

#pragma pack(1)
/// FSP communication
///

typedef struct _Nvdm_Uefi_Egm_Fsp {
  UINT8     subMessageId;
  UINT64    Egm_Base;
  UINT64    Egm_Size;
} Nvdm_Uefi_Egm_Fsp;

typedef struct _Nvdm_Uefi_Ats_Fsp {
  UINT8     subMessageId;
  UINT64    Hbm_Base;
} Nvdm_Uefi_Ats_Fsp;

/* Structure for FSP RPC C2C Init Status Command 'Get' */
typedef struct _Nvdm_Uefi_C2CInit_Fsp_Cmd {
  UINT8    subMessageId;
} Nvdm_Uefi_C2CInit_Fsp_Cmd;

#define NVDM_UEFI_C2CINIT_STATUS_CMD_SUBMESSAGE_ID  0x04

typedef struct _Nvdm_Fsp_Cmd_Response {
  UINT32    taskId;
  UINT32    commandNvdmType;
  UINT32    errorCode;
} Nvdm_Fsp_Cmd_Response;

/* Structure for FSP RPC C2C Init Status Response */
typedef struct _Nvdm_Uefi_C2CInit_Fsp_Response {
  UINT32    Payload;
} Nvdm_Uefi_C2CInit_Fsp_Response;

typedef struct _Mctp_Header {
  UINT8    Hdr_Version; /* one nibble valid, other reserved */
  UINT8    Destination_Id;
  UINT8    Source_Id;
  UINT8    Tag; /* Message tag, tag owner, packet sequence, end of message, start of message */
} Mctp_Header;

typedef struct _Nvdm_Header {
  UINT8     Msg_Header; /* _IC and _Type */
  UINT16    Vendor_ID;
  UINT8     Nvdm_Message_Type; /* only one bit valid */
} Nvdm_Header;

typedef struct Final_Message_Egm {
  Mctp_Header          Mctp_Header_S;
  Nvdm_Header          Nvdm_Header_S;
  Nvdm_Uefi_Egm_Fsp    Nvdm_Uefi_Egm_Fsp_S;
} FINAL_MESSAGE_EGM;

typedef struct Final_Message_Ats {
  Mctp_Header          Mctp_Header_S;
  Nvdm_Header          Nvdm_Header_S;
  Nvdm_Uefi_Ats_Fsp    Nvdm_Uefi_Ats_Fsp_S;
} FINAL_MESSAGE_ATS;

typedef struct Final_Message_C2CInit_Status_Cmd {
  Mctp_Header                  Mctp_Header_S;
  Nvdm_Header                  Nvdm_Header_S;
  Nvdm_Uefi_C2CInit_Fsp_Cmd    Nvdm_Uefi_C2CInit_S;
} FINAL_MESSAGE_C2CINIT_CMD;

typedef struct Final_Message_C2CInit_Status_Response {
  Mctp_Header                       Mctp_Header_S;
  Nvdm_Header                       Nvdm_Header_S;
  /* Response Header */
  Nvdm_Fsp_Cmd_Response             cmdResponse;
  /* Response Payload */
  Nvdm_Uefi_C2CInit_Fsp_Response    Nvdm_Uefi_C2CInit_Response_S;
} FINAL_MESSAGE_C2CINIT_RESPONSE;

#pragma pack()

///
/// Standard FSP RPC functions
///

/*
 * @brief Checks if the message queue is empty by comparing QUEUE HEAD and TAIL pointers.
 *
 * @param[in] PciIo         Handle for the Controller's PciIo protocol
 * @param[in] channelId     FSP EMEM channel ID
 *
 * @return TRUE  if message queue is empty.
 *         FALSE otherwise
 */
BOOLEAN EFIAPI
fspRpcIsMsgQueueEmpty (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId
  );

/*
 * @brief Checks if the queue is empty for sending messages by comparing QUEUE HEAD and TAIL pointers.
 *
 * @param[in] PciIo         Handle for the Controller's PciIo protocol
 * @param[in] channelId     FSP EMEM channel ID
 *
 * @return TRUE  if queue is empty.
 *         FALSE otherwise
 */
BOOLEAN EFIAPI
fspRpcIsQueueEmpty (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               channelId
  );

///
/// UEFI Extensions to the standard FSP functions
///

/*
 * @brief Configure the EMEM data write to enable auto increment
 *
 * @param[in] PciIo         PciIo protocol handle
 * @param[in] offet        Index of channel to configure for block and offset.
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
  );

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
  );

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
  );

/*
 * @brief Request the C2C Init Status
 *
 * @param[in]   PciIo         PciIo protocol handle
 * @param[out]  C2CInitStatus C2C Init Status Response
 *
 * @return Status
 *            EFI_SUCCESS
 *            EFI_OUT_OF_RESOURCES  - allocation failed
 *            EFI_TIMEOUT           - Timeout on command response
 *            EFI_INVALID_PARAMETER - NULL PciIo
 *            EFI_UNSUPPORTED       - C2C Init Status not implemented
 */
EFI_STATUS
EFIAPI
FspRpcGetC2CInitStatus (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  UINT32                  *C2CInitStatus
  );

#endif // __UEFI_FSP_RPC_H__
