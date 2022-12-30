/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * [UEFI] UEFIFspRpc.h: UEFI FSP client functions
 */

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

#endif // __UEFI_FSP_RPC_H__
