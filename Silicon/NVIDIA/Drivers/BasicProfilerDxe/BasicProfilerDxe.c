/** @file
 *  Basic Profiler Dxe
 *
 *  SPDX-FileCopyrightText: Copyright (c) 2023-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <PiPei.h>
#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Library/BaseMemoryLib.h>
#include <Ppi/SecPerformance.h>
#include <Protocol/KernelCmdLineUpdate.h>

#define PROFILER_CMD_MAX_LEN   200
#define PROFILER_UEFI_OFFSET   (SIZE_16KB + SIZE_4KB)
#define PROFILER_UEFI_SIZE     SIZE_4KB
#define FW_PROFILER_DATA_SIZE  ALIGN_VALUE (PROFILER_UEFI_SIZE, SIZE_64KB)

// Linux kernel profiler record structure format
// Total structure size is 56 + 8 = 64 bytes per record
#define MAX_PROFILE_STRLEN  55

#pragma pack(1)
typedef struct {
  CHAR8     Str[MAX_PROFILE_STRLEN + 1];   // 56 bytes: profiling point string
  UINT64    Timestamp;                     // 8 bytes: timer counter value
} PROFILER_RECORD;
#pragma pack()

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
  PROFILER_RECORD           *ProfilerRecords;
  UINT64                    ExitBootServicesTime;

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
    ProfilerBase    = (UINTN)Context;
    ProfilerRecords = (PROFILER_RECORD *)(ProfilerBase + PROFILER_UEFI_OFFSET);
    // Convert ticks to nanoseconds, then divide by 1000 to convert nanoseconds to microseconds
    ExitBootServicesTime = GetTimeInNanoSecond (GetPerformanceCounter ()) / 1000;

    // Initialize both records to zero
    SetMem (ProfilerRecords, sizeof (PROFILER_RECORD) * 2, 0);

    // Record 1: UEFI Entry Timestamp (ResetEnd) - stored in microseconds
    AsciiStrCpyS (
      ProfilerRecords[0].Str,
      MAX_PROFILE_STRLEN + 1,
      "UEFIEntryTimestamp"
      );
    // Performance->ResetEnd is already in nanoseconds from SEC phase, divide by 1000 to convert to microseconds
    ProfilerRecords[0].Timestamp = Performance->ResetEnd / 1000;

    // Record 2: Exit Boot Services Timestamp - stored in microseconds
    AsciiStrCpyS (
      ProfilerRecords[1].Str,
      MAX_PROFILE_STRLEN + 1,
      "ExitBootServicesTimestamp"
      );
    ProfilerRecords[1].Timestamp = ExitBootServicesTime;
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
      (ProfilerSize < SIZE_64KB))
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

  Status = gRT->SetVariable (
                  L"ProfilerBase",
                  &gNVIDIAPublicVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof (ProfilerBase),
                  &ProfilerBase
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to set ProfilerBase variable: %r\n", Status));
    return Status;
  }

  Status = gRT->SetVariable (
                  L"ProfilerSize",
                  &gNVIDIAPublicVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof (ProfilerSize),
                  &ProfilerSize
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to set ProfilerSize variable: %r\n", Status));
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
