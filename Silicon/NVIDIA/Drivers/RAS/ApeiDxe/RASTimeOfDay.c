/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/
#include "Apei.h"
#include <TH500/TH500Definitions.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/TimerLib.h>
#include <Library/TimeBaseLib.h>

/**
 * Send the time of day read from the UEFI RTC to RAS FW.
 *
 * @param  RasFwInterface
 * @param  SocketCount
 *
 * @return EFI_STATUS
 **/
EFI_STATUS
SetTimeOfDay (
  IN RAS_FW_INTERFACE  *RasFwInterface,
  IN UINT32            SocketCount
  )
{
  EFI_STATUS                 Status;
  EFI_MM_COMMUNICATE_HEADER  CommunicationHeader;
  EFI_TIME                   Now;
  UINT64                     TodInSeconds;
  UINT64                     *pTodInSeconds;

  Status = gRT->GetTime (&Now, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  TodInSeconds = EfiTimeToEpoch (&Now);

  for (UINT32 Socket = 0; Socket < SocketCount; Socket++) {
    CopyGuid (&(CommunicationHeader.HeaderGuid), &gEfiApeiSetTimeOfDayGuid);
    CommunicationHeader.MessageLength = sizeof (UINT64);
    pTodInSeconds                     = (UINT64 *)(RasFwInterface->RasFwBufferInfo[Socket].CommBase + sizeof (CommunicationHeader.HeaderGuid) +
                                                   sizeof (CommunicationHeader.MessageLength));

    *pTodInSeconds = TodInSeconds;

    Status = RasFwInterface->GuidedComm (Socket, &CommunicationHeader, &(RasFwInterface->RasFwBufferInfo[Socket]));
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}
