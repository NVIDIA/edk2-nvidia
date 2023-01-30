/** @file
  Debug Print Error Level library instance that retrieves the current error
  level from the CPU Bootloader Parameters. If they are not available, it falls
  back to PcdDebugPrintErrorLevel.  This generic library instance does not
  support the setting of the global debug print error level mask for the platform.

  Copyright (c) 2011 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Uefi.h>
#include <PiDxe.h>
#include <Library/DebugPrintErrorLevelLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>

#include <TH500/TH500Definitions.h>
#include <TH500/TH500MB1Configuration.h>

UINT32   mDebugLevel    = 0;
BOOLEAN  mDebugLevelSet = FALSE;

/**
  Returns the debug print error level mask for the current module.

  @return  Debug print error level mask for the current module.

**/
UINT32
EFIAPI
GetDebugPrintErrorLevel (
  VOID
  )
{
  VOID                          *Hob;
  TEGRABL_EARLY_BOOT_VARIABLES  *TH500HobConfig;

  if (mDebugLevelSet == FALSE) {
    Hob = GetFirstGuidHob (&gNVIDIATH500MB1DataGuid);
    if ((GET_GUID_HOB_DATA_SIZE (Hob) == (sizeof (TEGRABL_EARLY_BOOT_VARIABLES) * TH500_MAX_SOCKETS))) {
      TH500HobConfig = (TEGRABL_EARLY_BOOT_VARIABLES *)GET_GUID_HOB_DATA (Hob);
      mDebugLevel    = TH500HobConfig->Data.Mb1Data.UefiDebugLevel;
    } else {
      mDebugLevel =  PcdGet32 (PcdDebugPrintErrorLevel);
    }

    mDebugLevelSet = TRUE;
  }

  return mDebugLevel;
}

/**
  Sets the global debug print error level mask fpr the entire platform.

  @param   ErrorLevel     Global debug print error level.

  @retval  TRUE           The debug print error level mask was sucessfully set.
  @retval  FALSE          The debug print error level mask could not be set.

**/
BOOLEAN
EFIAPI
SetDebugPrintErrorLevel (
  UINT32  ErrorLevel
  )
{
  //
  // This library instance does not support setting the global debug print error
  // level mask.
  //
  return FALSE;
}
