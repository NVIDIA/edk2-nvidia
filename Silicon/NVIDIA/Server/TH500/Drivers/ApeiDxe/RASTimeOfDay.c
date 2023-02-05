/** @file
*
*  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
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
 * @param  RasFwBufferInfo
 *
 * @return EFI_STATUS
 **/
EFI_STATUS
SetTimeOfDay (
  IN RAS_FW_BUFFER  *RasFwBufferInfo
  )
{
  EFI_STATUS                 Status;
  EFI_MM_COMMUNICATE_HEADER  CommunicationHeader;
  EFI_TIME                   now;
  UINT64                     *pTodInSeconds;

  CopyGuid (&(CommunicationHeader.HeaderGuid), &gEfiApeiSetTimeOfDayGuid);
  CommunicationHeader.MessageLength = sizeof (UINT64);
  pTodInSeconds                     = (UINT64 *)(RasFwBufferInfo->CommBase + sizeof (CommunicationHeader.HeaderGuid) +
                                                 sizeof (CommunicationHeader.MessageLength));

  Status = gRT->GetTime (&now, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *pTodInSeconds = EfiTimeToEpoch (&now);

  return FfaGuidedCommunication (&CommunicationHeader, RasFwBufferInfo);
}
