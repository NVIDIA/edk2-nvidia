/** @file
  This driver registers a 5 minute watchdog between when it starts and ReadyToBoot.

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/DebugLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <libfdt.h>

/**
  Event notification function called when the gEfiWatchdogTimerArchProtocolGuid
  is installed.

  @param  Event     Event whose notification function is being invoked.
  @param  Context   The pointer to the notification function's context,
                    which is implementation-dependent.

**/
STATIC
VOID
EFIAPI
WatchDogTimerReady (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_STATUS    Status;
  UINTN         WatchdogTimout;
  VOID          *DtbBase;
  UINTN         DtbSize;
  INTN          NodeOffset;
  CONST UINT32  *Property;
  INT32         PropertyLen;

  WatchdogTimout = MAX_UINT32;
  Status         = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  if (!EFI_ERROR (Status)) {
    NodeOffset = fdt_path_offset (DtbBase, "/firmware/uefi");
    if (NodeOffset > 0) {
      Property = NULL;
      Property = (CONST UINT32 *)fdt_getprop (DtbBase, NodeOffset, "override-boot-watchdog-seconds", &PropertyLen);
      if ((Property != NULL) && (PropertyLen == sizeof (UINT32))) {
        WatchdogTimout = SwapBytes32 (*Property);
      }
    }
  }

  if (WatchdogTimout == MAX_UINT32) {
    WatchdogTimout = PcdGet16 (PcdBootWatchdogTime) * 60;
  }

  Status = gBS->SetWatchdogTimer (WatchdogTimout, 0x0001, 0, NULL);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %d minute boot watchdog installed\r\n", __FUNCTION__, WatchdogTimout / 60));
    gBS->CloseEvent (Event);
  }
}

/**
  Event notification function called when the EFI_EVENT_GROUP_READY_TO_BOOT is
  signaled.

  @param  Event     Event whose notification function is being invoked.
  @param  Context   The pointer to the notification function's context,
                    which is implementation-dependent.

**/
STATIC
VOID
EFIAPI
ReadyToBootSignaled (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  // Clear watchdog, will be reset for OS boot loader via boot manager
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  gBS->CloseEvent (Event);
}

/**
  Entrypoint of this module.

  This driver registers a 5 minute watchdog between when it starts and ReadyToBoot.

  @param  ImageHandle       The firmware allocated handle for the EFI image.
  @param  SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.

**/
EFI_STATUS
EFIAPI
InitializeWatchdog (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   ReadyToBootEvent;
  EFI_EVENT   WatchDogTimerReadyEvent;
  VOID        *Registration;

  WatchDogTimerReadyEvent = EfiCreateProtocolNotifyEvent (
                              &gEfiWatchdogTimerArchProtocolGuid,
                              TPL_CALLBACK,
                              WatchDogTimerReady,
                              NULL,
                              &Registration
                              );
  if (WatchDogTimerReadyEvent == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: failed to create watchdog event\r\n", __FUNCTION__));
    ASSERT (0);
    return (EFI_DEVICE_ERROR);
  }

  Status = EfiCreateEventReadyToBootEx (
             TPL_CALLBACK,
             ReadyToBootSignaled,
             NULL,
             &ReadyToBootEvent
             );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
