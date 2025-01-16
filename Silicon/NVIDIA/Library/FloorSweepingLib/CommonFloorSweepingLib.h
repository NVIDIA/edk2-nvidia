/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __COMMON_FLOOR_SWEEPING_LIB_H__
#define __COMMON_FLOOR_SWEEPING_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <TH500/TH500Definitions.h>
#include <TH500/TH500MB1Configuration.h>

/**
  Floorsweep PCIe

**/
EFI_STATUS
EFIAPI
CommonFloorSweepPcie (
  IN  VOID  *Dtb
  );

/**
  Floorsweep ScfCache

**/
EFI_STATUS
EFIAPI
CommonFloorSweepScfCache (
  IN  VOID  *Dtb
  );

/**
  Floorsweep Cpus

**/
EFI_STATUS
EFIAPI
CommonFloorSweepCpus (
  VOID
  );

/**
  Floorseep IPs

**/
EFI_STATUS
EFIAPI
CommonFloorSweepIps (
  VOID
  );

/**
  Initialize global structures

**/
EFI_STATUS
EFIAPI
CommonInitializeGlobalStructures (
  OUT CONST TEGRA_FLOOR_SWEEPING_INFO  **FloorSweepingInfo
  );

EFI_STATUS
EFIAPI
UpdateCpuFloorsweepingConfig (
  IN INT32  CpusOffset
  );

EFI_STATUS
EFIAPI
TH500FloorSweepCpus (
  VOID
  );

#endif // __COMMON_FLOOR_SWEEPING_LIB_H__
