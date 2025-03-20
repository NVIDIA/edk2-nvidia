/** @file

  DTB update for BpmpIpc

  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/HobLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>

#include "DtbUpdateLibPrivate.h"

/**
 * Update DTB BPMP IPC memory regions, if necessary.
 *
 * @retval None
**/
STATIC
VOID
EFIAPI
DtbUpdateBpmpIpcRegions (
  VOID
  )
{
  CHAR8                             BpmpPathStr[20];
  CHAR8                             *BpmpPathFormat;
  INT32                             NodeOffset;
  VOID                              *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO      *PlatformResourceInfo;
  TEGRA_RESOURCE_INFO               *ResourceInfo;
  CONST NVDA_MEMORY_REGION          *BpmpIpcRegions;
  UINT32                            Socket;
  UINT32                            MemoryPhandle;
  UINT32                            MaxSockets;
  CONST VOID                        *Property;
  UINT32                            PropertySize;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  RegisterArray[1];
  UINT32                            RegisterCount;
  EFI_STATUS                        Status;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob == NULL) ||
      (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get platform resource hob\n", __FUNCTION__));
    return;
  }

  PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  ResourceInfo         = PlatformResourceInfo->ResourceInfo;
  BpmpIpcRegions       = ResourceInfo->BpmpIpcRegions;
  if (BpmpIpcRegions == NULL) {
    DEBUG ((DEBUG_INFO, "%a: no BPMP IPC regions\n", __FUNCTION__));
    return;
  }

  MaxSockets = PcdGet32 (PcdTegraMaxSockets);
  for (Socket = 0; Socket < MaxSockets; Socket++) {
    if (!(PlatformResourceInfo->SocketMask & (1UL << Socket))) {
      continue;
    }

    if (PcdGetBool (PcdBpmpContainedInSocket)) {
      BpmpPathFormat = "/socket@%u/bpmp";
    } else {
      if (Socket == 0) {
        BpmpPathFormat = "/bpmp";
      } else {
        BpmpPathFormat = "/bpmp_s%u";
      }
    }

    if (BpmpIpcRegions[Socket].MemoryLength == 0) {
      DEBUG ((DEBUG_ERROR, "%a: BPMP IPC socket%u size 0\n", __FUNCTION__, Socket));
      continue;
    }

    ASSERT (AsciiStrSize (BpmpPathFormat) < sizeof (BpmpPathStr));
    AsciiSPrint (BpmpPathStr, sizeof (BpmpPathStr), BpmpPathFormat, Socket);
    Status = DeviceTreeGetNodeByPath (BpmpPathStr, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: socket%u no bpmp node: %r\n", __FUNCTION__, Socket, Status));
      continue;
    }

    Status = DeviceTreeGetNodeProperty (NodeOffset, "status", &Property, &PropertySize);
    if (!EFI_ERROR (Status) && (AsciiStrCmp (Property, "okay") != 0)) {
      DEBUG ((DEBUG_ERROR, "%a: socket%u bpmp node disabled\n", __FUNCTION__, Socket));
      continue;
    }

    Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "memory-region", &MemoryPhandle);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: socket%u bad bpmp memory-region: %r\n", __FUNCTION__, Socket, Status));
      continue;
    }

    Status = DeviceTreeGetNodeByPHandle (MemoryPhandle, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: socket%u err finding phandle=0x%x: %r\n", __FUNCTION__, Socket, MemoryPhandle, Status));
      continue;
    }

    RegisterCount = ARRAY_SIZE (RegisterArray);
    Status        = DeviceTreeGetRegisters (NodeOffset, RegisterArray, &RegisterCount);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: socket%u shmem 0x%x err getting reg: %r\n", __FUNCTION__, Socket, MemoryPhandle, Status));
      continue;
    }

    RegisterArray[0].BaseAddress = BpmpIpcRegions[Socket].MemoryBaseAddress;
    RegisterArray[0].Size        = BpmpIpcRegions[Socket].MemoryLength;

    Status = DeviceTreeSetRegisters (NodeOffset, RegisterArray, RegisterCount);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: socket%u shmem 0x%x err setting reg: %r\n", __FUNCTION__, Socket, MemoryPhandle, Status));
      continue;
    }

    DEBUG ((
      DEBUG_INFO,
      "%a: socket%u updated bpmp-shmem phandle=0x%x 0x%llx 0x%llx\n",
      __FUNCTION__,
      Socket,
      MemoryPhandle,
      BpmpIpcRegions[Socket].MemoryBaseAddress,
      BpmpIpcRegions[Socket].MemoryLength
      ));
  }
}

/**
  constructor to register update functions

**/
RETURN_STATUS
EFIAPI
DtbUpdateBpmpIpcInitialize (
  VOID
  )
{
  DTB_UPDATE_REGISTER_FUNCTION (DtbUpdateBpmpIpcRegions, DTB_UPDATE_ALL);

  return RETURN_SUCCESS;
}
