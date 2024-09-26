/** @file
  Tegra CPU Frequency Protocol

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef TEGRA_CPU_FREQ_PROTOCOL_H
#define TEGRA_CPU_FREQ_PROTOCOL_H

#include <Uefi/UefiSpec.h>
#include <ArchCommonNameSpaceObjects.h>
#include <ArmNameSpaceObjects.h>

#define NVIDIA_TEGRA_CPU_FREQ_PROTOCOL_GUID \
  { \
  0xa20bb97e, 0x4de7, 0x426e, { 0xac, 0xd6, 0x3a, 0x5e, 0xaa, 0x6a, 0xd6, 0xc5 } \
  }

typedef struct _NVIDIA_TEGRA_CPU_FREQ_PROTOCOL NVIDIA_TEGRA_CPU_FREQ_PROTOCOL;

/**
 * This function retrieves information on the CPU frequency of the specified core.
 *
 * @param[in]  This                     The instance of the NVIDIA_TEGRA_CPU_FREQ_PROTOCOL.
 * @param[in]  Mpidr                    MpIdr of the CPU to get info on.
 * @param[out] CurrentFrequency         If provided, returns the current frequency in Hz.
 * @param[out] HighestFrequency         If provided, returns the highest supported frequency in Hz.
 * @param[out] NominalFrequency         If provided, returns the nominal frequency in Hz.
 * @param[out] LowestNonlinearFrequency If provided, returns the lowest frequency non-linear power savings in Hz.
 * @param[out] LowestFrequency          If provided, returns the lowest supported frequency in Hz.
 *
 * @return EFI_SUCCESS                  Frequency information was returned.
 * @return EFI_NOT_FOUND                Mpidr is not valid for this platform.
 * @return EFI_UNSUPPORTED              Cpu Frequency driver does not support this platform.
 */
typedef
EFI_STATUS
(EFIAPI *TEGRA_CPU_FREQ_GET_INFO)(
  IN  NVIDIA_TEGRA_CPU_FREQ_PROTOCOL  *This,
  IN  UINT64                          Mpidr,
  OUT UINT64                          *CurrentFrequency OPTIONAL,
  OUT UINT64                          *HighestFrequency OPTIONAL,
  OUT UINT64                          *NominalFrequency OPTIONAL,
  OUT UINT64                          *LowestNonlinearFrequency OPTIONAL,
  OUT UINT64                          *LowestFrequency OPTIONAL
  );

/**
 * This function sets the CPU frequency of the specified core.
 *
 * @param[in] This                      The instance of the NVIDIA_TEGRA_CPU_FREQ_PROTOCOL.
 * @param[in] Mpidr                     MpIdr of the CPU to set frequency.
 * @param[in] DesiredFrequency          Desired frequency in Hz.
 *
 * @return EFI_SUCCESS                  Frequency was set.
 * @return EFI_INVALID_PARAMETER        Frequency is out of range.
 * @return EFI_NOT_FOUND                Mpidr is not valid for this platform.
 * @return EFI_UNSUPPORTED              Cpu Frequency driver does not support this platform.
 */
typedef
EFI_STATUS
(EFIAPI *TEGRA_CPU_FREQ_SET)(
  IN NVIDIA_TEGRA_CPU_FREQ_PROTOCOL  *This,
  IN UINT64                          Mpidr,
  IN UINT64                          DesiredFrequency
  );

/**
 * This function gets the _CPC information for the specified core.
 *
 * @param[in]  This                     The instance of the NVIDIA_TEGRA_CPU_FREQ_PROTOCOL.
 * @param[in]  Mpidr                    MpIdr of the CPU to get CPC info for.
 * @param[out] CpcInfo                  Cpc info for this core.
 *
 * @return EFI_SUCCESS                  Cpc info was retrieved.
 * @return EFI_INVALID_PARAMETER        CpcInfo is NULL.
 * @return EFI_NOT_FOUND                Mpidr is not valid for this platform.
 * @return EFI_UNSUPPORTED              Cpu Frequency driver does not support this platform.
 */
typedef
EFI_STATUS
(EFIAPI *TEGRA_CPU_FREQ_GET_CPC_INFO)(
  IN NVIDIA_TEGRA_CPU_FREQ_PROTOCOL  *This,
  IN UINT64                          Mpidr,
  IN CM_ARCH_COMMON_CPC_INFO                 *CpcInfo
  );

/// NVIDIA_REGULATOR_PROTOCOL protocol structure.
struct _NVIDIA_TEGRA_CPU_FREQ_PROTOCOL {
  TEGRA_CPU_FREQ_GET_INFO        GetInfo;
  TEGRA_CPU_FREQ_SET             Set;
  TEGRA_CPU_FREQ_GET_CPC_INFO    GetCpcInfo;
};

extern EFI_GUID  gNVIDIATegraCpuFrequencyProtocolGuid;

#endif
