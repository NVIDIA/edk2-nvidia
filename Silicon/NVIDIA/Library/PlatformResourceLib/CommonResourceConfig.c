/** @file
*
*  Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/NVIDIADebugLib.h>
#include "CommonResourceConfig.h"

#define MAX_CORE_DISABLE_WORDS  3

/**
  Add one socket's enabled cores bit map array to the EnabledCoresBitMap

**/
STATIC
VOID
EFIAPI
AddSocketCoresToEnabledCoresBitMap (
  IN UINTN   SocketNumber,
  IN UINT32  *SocketCores,
  IN UINTN   MaxSupportedCores,
  IN UINT64  *EnabledCoresBitMap,
  IN UINT32  CoresPerSocket,
  IN UINTN   MaxCoreDisableWords
  )
{
  UINTN  SocketStartingCore;
  UINTN  EnabledCoresBit;
  UINTN  EnabledCoresIndex;
  UINTN  SocketCoresBit;
  UINTN  SocketCoresIndex;
  UINTN  Core;

  SocketStartingCore = CoresPerSocket * SocketNumber;

  ASSERT ((SocketStartingCore + CoresPerSocket) <= MaxSupportedCores);
  NV_ASSERT_RETURN (
    (SocketStartingCore + CoresPerSocket) <= MaxSupportedCores,
    return ,
    "Invalid core info\r\n"
    );
  ASSERT ((ALIGN_VALUE (CoresPerSocket, 32) / 32) <= MaxCoreDisableWords);
  NV_ASSERT_RETURN (
    (ALIGN_VALUE (CoresPerSocket, 32) / 32) <= MaxCoreDisableWords,
    return ,
    "Invalid core info\r\n"
    );

  for (Core = 0; Core < CoresPerSocket; Core++) {
    SocketCoresIndex = Core / 32;
    SocketCoresBit   = Core % 32;

    EnabledCoresIndex = (Core + SocketStartingCore) / 64;
    EnabledCoresBit   = (Core + SocketStartingCore) % 64;

    EnabledCoresBitMap[EnabledCoresIndex] |=
      (SocketCores[SocketCoresIndex] & (1UL << SocketCoresBit)) ? (1ULL << EnabledCoresBit) : 0;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Socket %u cores 0x%x 0x%x 0x%x added as EnabledCores bits %u-%u\n",
    __FUNCTION__,
    SocketNumber,
    SocketCores[2],
    SocketCores[1],
    SocketCores[0],
    SocketStartingCore + CoresPerSocket - 1,
    SocketStartingCore
    ));
}

/**
  Fills in the EnabledCoresBitMap

**/
EFI_STATUS
EFIAPI
CommonConfigGetEnabledCoresBitMap (
  IN CONST COMMON_RESOURCE_CONFIG_INFO  *ConfigInfo,
  IN TEGRA_PLATFORM_RESOURCE_INFO       *PlatformResourceInfo
  )
{
  UINT32  ScratchDisableReg[MAX_CORE_DISABLE_WORDS];
  UINT32  CoresPerSocket;
  UINT32  EnaBitMap[MAX_CORE_DISABLE_WORDS];
  UINTN   Socket;
  UINTN   Index;
  UINTN   SatMcDisableWord;
  UINTN   SatMcDisableBit;
  UINT64  ScratchBase;

  NV_ASSERT_RETURN ((ConfigInfo->MaxCoreDisableWords <= MAX_CORE_DISABLE_WORDS), return EFI_UNSUPPORTED, "%a: bad max words\n", __FUNCTION__);

  CoresPerSocket = PlatformResourceInfo->MaxPossibleCores / PlatformResourceInfo->MaxPossibleSockets;

  for (Socket = 0; Socket < PlatformResourceInfo->MaxPossibleSockets; Socket++) {
    if (!(PlatformResourceInfo->SocketMask & (1UL << Socket))) {
      continue;
    }

    ScratchBase = ConfigInfo->SocketScratchBaseAddr[Socket];
    if (ScratchBase == 0) {
      continue;
    }

    for (Index = 0; Index < ConfigInfo->MaxCoreDisableWords; Index++) {
      ScratchDisableReg[Index] = MmioRead32 (ScratchBase + ConfigInfo->CoreDisableScratchOffset[Index]);
    }

    if (ConfigInfo->SatMcSupported && (Socket == 0)) {
      DEBUG ((DEBUG_ERROR, "%a: Mask core %u on socket 0 for SatMC\n", __FUNCTION__, ConfigInfo->SatMcCore));

      SatMcDisableWord = ConfigInfo->SatMcCore / 32;
      SatMcDisableBit  = ConfigInfo->SatMcCore % 32;

      ScratchDisableReg[SatMcDisableWord] |= (1U << SatMcDisableBit);
    }

    for (Index = 0; Index < ConfigInfo->MaxCoreDisableWords; Index++) {
      ScratchDisableReg[Index] |= ConfigInfo->CoreDisableScratchMask[Index];
      ScratchDisableReg[Index] &= ~ConfigInfo->CoreDisableScratchMask[Index];
      EnaBitMap[Index]          = ~ScratchDisableReg[Index];
    }

    AddSocketCoresToEnabledCoresBitMap (
      Socket,
      EnaBitMap,
      PlatformResourceInfo->MaxPossibleCores,
      PlatformResourceInfo->EnabledCoresBitMap,
      CoresPerSocket,
      ConfigInfo->MaxCoreDisableWords
      );
  }

  return EFI_SUCCESS;
}
