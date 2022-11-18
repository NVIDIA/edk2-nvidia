/** @file
  Definition of debug functions.

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

 **/

#ifndef DEBUG_H_
#define DEBUG_H_

#include <Uefi.h>
#include "Rndis.h"

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

#define USB_DEBUG_RNDIS           DEBUG_VERBOSE   // RNDIS data detail
#define USB_DEBUG_RNDIS_TRACE     DEBUG_VERBOSE   // RNDIS protocol trace
#define USB_DEBUG_RNDIS_CONTROL   DEBUG_VERBOSE   // RNDIS control message
#define USB_DEBUG_RNDIS_TRANSFER  DEBUG_VERBOSE   // RNDIS bulk-in and bulk-out
#define USB_DEBUG_SNP             DEBUG_VERBOSE   // SNP data detail
#define USB_DEBUG_SNP_TRACE       DEBUG_VERBOSE   // SNP protocol trace
#define USB_DEBUG_DRIVER_BINDING  DEBUG_VERBOSE   // Driver binding trace
#define USB_DEBUG_QUEUE           DEBUG_VERBOSE   // Receiver queue trace

/**
  Dump the RNDIS message.

  @param[in]  ErrorLevel        Error output level
  @param[in]  Message           Extra message to show on screen.
  @param[in]  RndisMsg          RNDIS message to dump

  @retval EFI_SUCCESS           Buffer dump successfully.
  @retval Others                Error occurs.

**/
EFI_STATUS
DumpRndisMessage (
  IN  UINTN             ErrorLevel,
  IN  CONST CHAR8       *Message,
  IN  RNDIS_MSG_HEADER  *RndisMsg
  );

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
  IN  UINTN  ErrorLevel,
  IN UINT8   *Buffer,
  IN UINTN   Length
  );

#endif
