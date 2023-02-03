/** @file
*
*  Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-FileCopyrightText: Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <PiDxe.h>
#include <Library/HobLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PlatformResourceLib.h>

#include <Protocol/RamDisk.h>
#include <Protocol/DevicePath.h>

EFI_STATUS
RamDiskOSEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                    Status;
  UINTN                         Base;
  UINTN                         Size;
  EFI_RAM_DISK_PROTOCOL         *RamDisk;
  EFI_DEVICE_PATH_PROTOCOL      *DevicePath;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  } else {
    return EFI_NOT_FOUND;
  }

  Base = PlatformResourceInfo->RamdiskOSInfo.Base;
  Size = PlatformResourceInfo->RamdiskOSInfo.Size;

  if ((Base == 0) || (Size == 0)) {
    return EFI_NOT_FOUND;
  }

  Status = gBS->LocateProtocol (&gEfiRamDiskProtocolGuid, NULL, (VOID **)&RamDisk);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Couldn't find the RAM Disk protocol - %r\n", __FUNCTION__, Status));
    return Status;
  }

  DevicePath = NULL;
  Status     = RamDisk->Register (
                          (UINTN)Base,
                          (UINT64)Size,
                          &gEfiVirtualDiskGuid,
                          NULL,
                          &DevicePath
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to register RAM Disk - %r\n", __FUNCTION__, Status));
  }

  return Status;
}
