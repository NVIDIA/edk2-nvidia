/** @file

  Copyright (c) 2017-2018, Arm Limited. All rights reserved.
  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  System Control and Management Interface V1.0
    http://infocenter.arm.com/help/topic/com.arm.doc.den0056a/
    DEN0056A_System_Control_and_Management_Interface.pdf
**/

#ifndef BPMP_SCMI_CLOCK_PROTOCOL_PRIVATE_H_
#define BPMP_SCMI_CLOCK_PROTOCOL_PRIVATE_H_

 typedef enum {
   ClockSubcommandGetRate = 1,
   ClockSubcommandSetRate = 2,
   ClockSubcommandRoundRate = 3,
   ClockSubcommandGetParent = 4,
   ClockSubcommandSetParent = 5,
   ClockSubcommandIsEnabled = 6,
   ClockSubcommandEnable = 7,
   ClockSubcommandDisable = 8,
   ClockSubcommandProperties = 9,
   ClockSubcommandPossibleParents = 10,
   ClockSubcommandNumberOfPossibleParents = 11,
   ClockSubcommandGetPossibleParents = 12,
   ClockSubcommandResetReferenceCount = 13,
   ClockSubcommandGetAllInfo = 14,
   ClockSubcommandGetMaxClockId = 15,
   ClockSubcommandGetFmaxAtVMin = 16,
   ClockSubcommandMax
} CLOCK_SUBCOMMAND;

#define CLOCK_MAX_PARENTS     16
#define CLOCK_MAX_NAME_LENGTH 40

#pragma pack (1)

typedef struct {
  UINT32 ClockId:24;
  UINT32 Subcommand:8;
  UINT32 ParentId; //Only used for set parent
  UINT64 Rate;     //Only used for set rate and round rate
} BPMP_CLOCK_REQUEST;

typedef struct {
  UINT32 Flags;
  UINT32 Parent;
  UINT32 Parents[CLOCK_MAX_PARENTS];
  UINT8  NumberOfParents;
  CHAR8  Name[CLOCK_MAX_NAME_LENGTH];
} BPMP_CLOCK_GET_ALL_INFO_RESPONSE;
#pragma pack ()

/** Initialize clock management protocol and install protocol on a given handle.

  @param[in] Handle              Handle to install clock management protocol.

  @retval EFI_SUCCESS            Clock protocol interface installed successfully.
**/
EFI_STATUS
ScmiClockProtocolInit (
  IN EFI_HANDLE *Handle
  );

#endif /* BPMP_SCMI_CLOCK_PROTOCOL_PRIVATE_H_ */

