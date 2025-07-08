/** @file

  CPU Frequency Monitor Driver.

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/TegraCpuFreq.h>

/**
  CPU Frequency Monitor Driver Entry Point.

  This function is the entry point for the CPU Frequency Monitor driver.
  It reads and logs CPU frequency information early in the UEFI boot process.

  @param[in]  ImageHandle   The firmware allocated handle for the UEFI image.
  @param[in]  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The driver was loaded successfully.
  @retval EFI_DEVICE_ERROR  The driver could not be loaded.

**/
EFI_STATUS
EFIAPI
CpuFreqMonitorInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                      Status;
  NVIDIA_TEGRA_CPU_FREQ_PROTOCOL  *CpuFreqProtocol;
  UINT64                          CurrentFreq;
  UINT64                          MaxFreq;
  UINT64                          MinFreq;
  UINT64                          NominalFreq;
  UINT64                          LowestNonlinearFreq;

  DEBUG ((DEBUG_INFO, "%a: Driver loaded, attempting to read CPU frequency\n", __FUNCTION__));

  Status = gBS->LocateProtocol (
                  &gNVIDIATegraCpuFrequencyProtocolGuid,
                  NULL,
                  (VOID **)&CpuFreqProtocol
                  );
  if (!EFI_ERROR (Status)) {
    Status = CpuFreqProtocol->GetInfo (
                                CpuFreqProtocol,
                                ArmReadMpidr (),
                                &CurrentFreq,
                                &MaxFreq,
                                &NominalFreq,
                                &LowestNonlinearFreq,
                                &MinFreq
                                );
    if (!EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: CPU Frequency - Current: %llu MHz, Max: %llu MHz\n",
        __FUNCTION__,
        CurrentFreq / 1000000,
        MaxFreq / 1000000
        ));
      DEBUG ((
        DEBUG_INFO,
        "%a: CPU Frequency - Nominal: %llu MHz, Min: %llu MHz, Lowest Non-linear: %llu MHz\n",
        __FUNCTION__,
        NominalFreq / 1000000,
        MinFreq / 1000000,
        LowestNonlinearFreq / 1000000
        ));
    } else {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get CPU frequency info - %r\n", __FUNCTION__, Status));
    }
  } else {
    DEBUG ((DEBUG_ERROR, "%a: CPU frequency protocol not found - %r\n", __FUNCTION__, Status));
  }

  return EFI_SUCCESS;
}
