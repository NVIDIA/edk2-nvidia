/** @file
  Provides the debug functions.

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Debug.h"

/**
  Dump the byte data in Buffer.

  @param[in]  ErrorLevel        Error output level
  @param[in]  Buffer            Buffer to dump
  @param[in]  Length            Buffer length in byte.

  @retval EFI_SUCCESS           Buffer dump successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
DumpRawBuffer (
  IN UINTN  ErrorLevel,
  IN UINT8  *Buffer,
  IN UINTN  Length
  )
{
  UINTN  Index;

  if ((Buffer == NULL) || (Length == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < Length; Index++) {
    DEBUG ((ErrorLevel, "%02X ", Buffer[Index]));
    if ((Index + 1) % 16 == 0) {
      DEBUG ((ErrorLevel, "\n"));
    } else if ((Index + 1) % 4 == 0) {
      DEBUG ((ErrorLevel, " "));
    }
  }

  DEBUG ((ErrorLevel, "\n"));

  return EFI_SUCCESS;
}

/**
  Dump the RNDIS message.

  @param[in]  ErrorLevel        Error output level
  @param[in]  Message           Extra message to show on screen.
  @param[in]  RndisMessage          RNDIS message to dump

  @retval EFI_SUCCESS           Buffer dump successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
DumpRndisMessage (
  IN  UINTN             ErrorLevel,
  IN  CONST CHAR8       *Message,
  IN  RNDIS_MSG_HEADER  *RndisMessage
  )
{
  UINTN  Length;

  DEBUG ((ErrorLevel, "%a Dump (%p)-> ", (Message == NULL ? __FUNCTION__ : Message), (UINTN)RndisMessage));

  Length = 0;
  switch (RndisMessage->MessageType) {
    case RNDIS_PACKET_MSG:
      DEBUG ((ErrorLevel, "RNDIS_PACKET_MSG:\n"));
      Length  = sizeof (RNDIS_PACKET_MSG_DATA);
      Length += ((RNDIS_PACKET_MSG_DATA *)RndisMessage)->DataLength + ((RNDIS_PACKET_MSG_DATA *)RndisMessage)->OutOfBandDataLength + ((RNDIS_PACKET_MSG_DATA *)RndisMessage)->PerPacketInfoLength;
      break;
    case RNDIS_INITIALIZE_MSG:
      DEBUG ((ErrorLevel, "RNDIS_INITIALIZE_MSG:\n"));
      Length = sizeof (RNDIS_INITIALIZE_MSG_DATA);
      break;
    case RNDIS_INITIALIZE_CMPLT:
      DEBUG ((ErrorLevel, "RNDIS_INITIALIZE_CMPLT:\n"));
      Length = sizeof (RNDIS_INITIALIZE_CMPLT_DATA);
      break;
    case RNDIS_HLT_MSG:
      DEBUG ((ErrorLevel, "RNDIS_HLT_MSG:\n"));
      Length = sizeof (RNDIS_HALT_MSG_DATA);
      break;
    case RNDIS_QUERY_MSG:
      DEBUG ((ErrorLevel, "RNDIS_QUERY_MSG:\n"));
      Length = sizeof (RNDIS_QUERY_MSG_DATA);
      break;
    case RNDIS_QUERY_CMPLT:
      DEBUG ((ErrorLevel, "RNDIS_QUERY_CMPLT:\n"));
      Length = sizeof (RNDIS_QUERY_CMPLT_DATA);
      break;
    case RNDIS_SET_MSG:
      DEBUG ((ErrorLevel, "RNDIS_SET_MSG:\n"));
      Length = sizeof (RNDIS_SET_MSG_DATA);
      break;
    case RNDIS_SET_CMPLT:
      DEBUG ((ErrorLevel, "RNDIS_SET_CMPLT:\n"));
      Length = sizeof (RNDIS_SET_CMPLT_DATA);
      break;
    case RNDIS_RESET_MSG:
      DEBUG ((DEBUG_INFO, "RNDIS_RESET_MSG:\n"));
      Length = sizeof (RNDIS_RESET_MSG_DATA);
      break;
    case RNDIS_RESET_CMPLT:
      DEBUG ((ErrorLevel, "RNDIS_RESET_CMPLT:\n"));
      Length = sizeof (RNDIS_RESET_CMPLT_DATA);
      break;
    case RNDIS_INDICATE_STATUS_MSG:
      DEBUG ((ErrorLevel, "RNDIS_INDICATE_STATUS_MSG:\n"));
      Length = sizeof (RNDIS_INDICATE_STATUS_MSG_DATA);
      break;
    case RNDIS_KEEPALIVE_MSG:
      DEBUG ((ErrorLevel, "RNDIS_KEEPALIVE_MSG:\n"));
      Length = sizeof (RNDIS_KEEPALIVE_MSG_DATA);
      break;
    case RNDIS_KEEPALIVE_CMPLT:
      DEBUG ((ErrorLevel, "RNDIS_KEEPALIVE_CMPLT:\n"));
      Length = sizeof (RNDIS_KEEPALIVE_CMPLT_DATA);
      break;
    default:
      DEBUG ((ErrorLevel, "!!UNKNOWN!!\n"));
      return EFI_UNSUPPORTED;
  }

  return DumpRawBuffer (ErrorLevel, (UINT8 *)RndisMessage, Length);
}
