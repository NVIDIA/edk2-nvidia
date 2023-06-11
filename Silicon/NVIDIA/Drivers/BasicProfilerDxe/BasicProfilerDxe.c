/** @file
 *  Basic Profiler Dxe
 *
 *  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <PiPei.h>
#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Ppi/SecPerformance.h>
#include <Protocol/KernelCmdLineUpdate.h>

#define PROFILER_CMD_MAX_LEN   200
#define PROFILER_UEFI_OFFSET   (SIZE_16KB + SIZE_4KB)
#define PROFILER_UEFI_SIZE     SIZE_4KB
#define FW_PROFILER_DATA_SIZE  ALIGN_VALUE (PROFILER_UEFI_SIZE, SIZE_64KB)

typedef struct {
  UINT64    UEFIEntryTimestamp;
  UINT64    ExitBootServicesTimestamp;
} UEFI_PROFILE;

STATIC
CHAR16  mProfilerNewCommandLineArgument[PROFILER_CMD_MAX_LEN];

STATIC
NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL  mProfilerCmdLine;

STATIC
VOID
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINTN                     ProfilerBase;
  VOID                      *Hob;
  FIRMWARE_SEC_PERFORMANCE  *Performance;
  UEFI_PROFILE              *UEFIProfileData;

  gBS->CloseEvent (Event);

  Hob = GetFirstGuidHob (&gEfiFirmwarePerformanceGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (FIRMWARE_SEC_PERFORMANCE)))
  {
    Performance = (FIRMWARE_SEC_PERFORMANCE *)GET_GUID_HOB_DATA (Hob);
  } else {
    DEBUG ((DEBUG_ERROR, "Failed to get sec performance data\n"));
    return;
  }

  if (Performance->ResetEnd != 0) {
    ProfilerBase                               = (UINTN)Context;
    UEFIProfileData                            = (UEFI_PROFILE *)(ProfilerBase + PROFILER_UEFI_OFFSET);
    UEFIProfileData->UEFIEntryTimestamp        = Performance->ResetEnd;
    UEFIProfileData->ExitBootServicesTimestamp = GetTimeInNanoSecond (GetPerformanceCounter ());
  }

  return;
}

EFI_STATUS
EFIAPI
BasicProfilerDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                    Status;
  EFI_EVENT                     ExitBootServicesEvent;
  UINTN                         ProfilerBase;
  UINTN                         ProfilerSize;
  EFI_HANDLE                    Handle;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  } else {
    DEBUG ((DEBUG_ERROR, "Failed to get platform resource data\n"));
    return EFI_NOT_FOUND;
  }

  ProfilerBase = PlatformResourceInfo->ProfilerInfo.Base;
  ProfilerSize = PlatformResourceInfo->ProfilerInfo.Size;

  if ((ProfilerBase == 0) ||
      (ProfilerSize == 0) ||
      (ProfilerSize <= SIZE_64KB))
  {
    DEBUG ((DEBUG_ERROR, "Invalid profiler carveout information\n"));
    return EFI_NOT_FOUND;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  OnExitBootServices,
                  (CONST VOID *)ProfilerBase,
                  &gEfiEventExitBootServicesGuid,
                  &ExitBootServicesEvent
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Handle                                       = NULL;
  mProfilerCmdLine.ExistingCommandLineArgument = NULL;
  UnicodeSPrintAsciiFormat (
    mProfilerNewCommandLineArgument,
    PROFILER_CMD_MAX_LEN,
    "bl_prof_dataptr=%lu@0x%lx bl_prof_ro_ptr=%lu@0x%lx",
    ProfilerSize - FW_PROFILER_DATA_SIZE,
    ProfilerBase + FW_PROFILER_DATA_SIZE,
    FW_PROFILER_DATA_SIZE,
    ProfilerBase
    );
  mProfilerCmdLine.NewCommandLineArgument = mProfilerNewCommandLineArgument;
  Status                                  = gBS->InstallMultipleProtocolInterfaces (
                                                   &Handle,
                                                   &gNVIDIAKernelCmdLineUpdateGuid,
                                                   &mProfilerCmdLine,
                                                   NULL
                                                   );

  return Status;
}
