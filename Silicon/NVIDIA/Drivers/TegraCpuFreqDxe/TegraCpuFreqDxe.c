/** @file

  Tegra CPU Frequency Driver.

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Protocol/BpmpIpc.h>
#include <Protocol/DeviceTreeNode.h>
#include <Protocol/TegraCpuFreq.h>
#include <ArmNameSpaceObjects.h>
#include <libfdt.h>

#include <Library/DeviceDiscoveryDriverLib.h>

#include "TegraCpuFreqDxePrivate.h"

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,t234-cpufreq",  &gNVIDIACpuFreqT234  },
  { "nvidia,th500-cpufreq", &gNVIDIACpuFreqTH500 },
  { NULL,                   NULL                 }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                                 = L"NVIDIA Tegra CPU Driver",
  .UseDriverBinding                           = FALSE,
  .AutoEnableClocks                           = TRUE,
  .AutoResetModule                            = TRUE,
  .AutoDeassertPg                             = TRUE,
  .SkipEdkiiNondiscoverableInstall            = TRUE,
  .SkipAutoDeinitControllerOnExitBootServices = TRUE,
  .DirectEnumerationSupport                   = TRUE
};

STATIC
EFI_STATUS
EFIAPI
GetCpuFreqAddresses (
  IN UINT64                 Mpidr,
  OUT EFI_PHYSICAL_ADDRESS  *NdivAddress OPTIONAL,
  OUT EFI_PHYSICAL_ADDRESS  *RefClockAddress OPTIONAL,
  OUT EFI_PHYSICAL_ADDRESS  *CoreClockAddress OPTIONAL,
  OUT UINT64                *RefClockFreq OPTIONAL,
  OUT UINT32                *BpmpPhandle OPTIONAL
  )
{
  EFI_STATUS                        Status;
  UINTN                             HandleCount;
  EFI_HANDLE                        *HandleBuffer;
  EFI_PHYSICAL_ADDRESS              RegionBase;
  UINTN                             RegionSize;
  UINT8                             Socket;
  UINT8                             Cluster;
  UINT8                             Core;
  UINT32                            LinearId;
  UINT32                            Index;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node;
  INT32                             ParentOffset;
  CONST UINT32                      *SocketRegProperty;
  CONST VOID                        *Property    = NULL;
  INT32                             PropertySize = 0;
  UINTN                             ChipID;

  if (!PcdGetBool (PcdAffinityMpIdrSupported)) {
    Core    = GET_MPIDR_AFF0 (Mpidr);
    Cluster = GET_MPIDR_AFF1 (Mpidr);
    Socket  = GET_MPIDR_AFF2 (Mpidr);
  } else {
    Core    = GET_MPIDR_AFF1 (Mpidr);
    Cluster = GET_MPIDR_AFF2 (Mpidr);
    Socket  = GET_MPIDR_AFF3 (Mpidr);
  }

  if ((Core >= PcdGet32 (PcdTegraMaxCoresPerCluster)) ||
      (Cluster >= PcdGet32 (PcdTegraMaxClusters)) ||
      (Socket >= PcdGet32 (PcdTegraMaxSockets)))
  {
    return EFI_NOT_FOUND;
  }

  ChipID = TegraGetChipID ();

  if (ChipID == T194_CHIP_ID) {
    if (BpmpPhandle != NULL) {
      *BpmpPhandle = 0;
    }

    return EFI_SUCCESS;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIACpuFreqT234,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    return Status;
  }

  if (HandleCount == 1) {
    Status = gBS->HandleProtocol (HandleBuffer, &gNVIDIADeviceTreeNodeProtocolGuid, (VOID **)&Node);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Property = fdt_getprop (
                 Node->DeviceTreeBase,
                 Node->NodeOffset,
                 "status",
                 &PropertySize
                 );
    if ((Property != NULL) &&
        (AsciiStrCmp (Property, "okay") != 0))
    {
      return EFI_UNSUPPORTED;
    }

    Status = DeviceDiscoveryGetMmioRegion (HandleBuffer[0], 0, &RegionBase, &RegionSize);
    if (EFI_ERROR (Status)) {
      FreePool (HandleBuffer);
      return Status;
    }

    if (NdivAddress != NULL) {
      LinearId     = Cluster * PcdGet32 (PcdTegraMaxCoresPerCluster) + Core;
      *NdivAddress = RegionBase + SCRATCH_FREQ_CORE_REG (LinearId);
    }

    if (RefClockAddress != NULL) {
      *RefClockAddress = RegionBase + CLUSTER_ACTMON_REFCLK_REG (Cluster, Core);
    }

    if (CoreClockAddress != NULL) {
      *CoreClockAddress = RegionBase + CLUSTER_ACTMON_CORE_REG (Cluster, Core);
    }

    if (RefClockFreq != NULL) {
      *RefClockFreq = REFCLK_FREQ;
    }

    if (BpmpPhandle != NULL) {
      *BpmpPhandle = 0;
    }

    FreePool (HandleBuffer);
    return EFI_SUCCESS;
  } else if (HandleCount != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Unexpected number of cpu frequency controllers - %u\r\n", __FUNCTION__, HandleCount));
    FreePool (HandleBuffer);
    return EFI_UNSUPPORTED;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIACpuFreqTH500,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], &gNVIDIADeviceTreeNodeProtocolGuid, (VOID **)&Node);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Property = fdt_getprop (
                 Node->DeviceTreeBase,
                 Node->NodeOffset,
                 "status",
                 &PropertySize
                 );
    if ((Property != NULL) &&
        (AsciiStrCmp (Property, "okay") != 0))
    {
      return EFI_UNSUPPORTED;
    }

    ParentOffset      = fdt_parent_offset (Node->DeviceTreeBase, Node->NodeOffset);
    SocketRegProperty = fdt_getprop (
                          Node->DeviceTreeBase,
                          ParentOffset,
                          "reg",
                          NULL
                          );
    if ((SocketRegProperty == NULL) || (*SocketRegProperty != SwapBytes32 (Socket))) {
      continue;
    }

    Status = DeviceDiscoveryGetMmioRegion (HandleBuffer[Index], 0, &RegionBase, &RegionSize);
    if (EFI_ERROR (Status)) {
      FreePool (HandleBuffer);
      return Status;
    }

    if (NdivAddress != NULL) {
      *NdivAddress = RegionBase + TH500_SCRATCH_FREQ_CORE_REG (Cluster);
    }

    if (RefClockAddress != NULL) {
      *RefClockAddress = 0;
    }

    if (CoreClockAddress != NULL) {
      *CoreClockAddress = 0;
    }

    if (RefClockFreq != NULL) {
      *RefClockFreq = TH500_REFCLK_FREQ;
    }

    if (BpmpPhandle != NULL) {
      Property = fdt_getprop (
                   Node->DeviceTreeBase,
                   Node->NodeOffset,
                   "nvidia,bpmp",
                   &PropertySize
                   );
      if ((Property == NULL) || (PropertySize < sizeof (UINT32))) {
        DEBUG ((DEBUG_ERROR, "Failed to get Bpmp node phandle.\n"));
      } else {
        CopyMem ((VOID *)BpmpPhandle, Property, sizeof (UINT32));
        *BpmpPhandle = SwapBytes32 (*BpmpPhandle);
      }
    }

    FreePool (HandleBuffer);
    return EFI_SUCCESS;
  }

  return EFI_UNSUPPORTED;
}

STATIC
EFI_STATUS
EFIAPI
GetCpuNdiv (
  IN UINT64   Mpidr,
  OUT UINT16  *Ndiv
  )
{
  EFI_STATUS            Status;
  UINTN                 ChipID;
  TEGRA_PLATFORM_TYPE   PlatformType;
  EFI_PHYSICAL_ADDRESS  NdivAddress;

  if (Ndiv == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ChipID       = TegraGetChipID ();
  PlatformType = TegraGetPlatform ();

  if (ChipID == T194_CHIP_ID) {
    if (PlatformType == TEGRA_PLATFORM_SILICON) {
      if (ArmReadMpidr () == Mpidr) {
        *Ndiv  = GetT194CpuNdiv () & NDIV_MASK;
        Status = EFI_SUCCESS;
      } else {
        Status = EFI_UNSUPPORTED;
      }
    } else {
      Status = EFI_UNSUPPORTED;
    }
  } else {
    Status = GetCpuFreqAddresses (Mpidr, &NdivAddress, NULL, NULL, NULL, NULL);
    if (!EFI_ERROR (Status)) {
      *Ndiv = MmioRead32 (NdivAddress);
    }
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
SetCpuNdiv (
  IN UINT64   Mpidr,
  OUT UINT16  Ndiv
  )
{
  EFI_STATUS            Status;
  UINTN                 ChipID;
  TEGRA_PLATFORM_TYPE   PlatformType;
  EFI_PHYSICAL_ADDRESS  NdivAddress;

  ChipID       = TegraGetChipID ();
  PlatformType = TegraGetPlatform ();

  if (ChipID == T194_CHIP_ID) {
    if (PlatformType == TEGRA_PLATFORM_SILICON) {
      if (ArmReadMpidr () == Mpidr) {
        SetT194CpuNdiv (Ndiv);
        Status = EFI_SUCCESS;
      } else {
        Status = EFI_UNSUPPORTED;
      }
    } else {
      Status = EFI_UNSUPPORTED;
    }
  } else {
    Status = GetCpuFreqAddresses (Mpidr, &NdivAddress, NULL, NULL, NULL, NULL);
    if (!EFI_ERROR (Status)) {
      MmioWrite32 (NdivAddress, Ndiv);
    }
  }

  return Status;
}

STATIC
UINT64
EFIAPI
ConvertNdivToFreq (
  IN BPMP_CPU_NDIV_LIMITS_RESPONSE  *Limits,
  IN UINT16                         Ndiv
  )
{
  return ((UINT64)Limits->ref_clk_hz * (UINT64)Ndiv) / ((UINT64)Limits->pdiv * (UINT64)Limits->mdiv);
}

STATIC
UINT64
EFIAPI
ConvertFreqToNdiv (
  IN BPMP_CPU_NDIV_LIMITS_RESPONSE  *Limits,
  IN UINT64                         Freq
  )
{
  return (Freq * (UINT64)Limits->pdiv * (UINT64)Limits->mdiv) / (UINT64)Limits->ref_clk_hz;
}

STATIC
EFI_STATUS
EFIAPI
TegraCpuGetNdivLimits (
  IN UINT64                          Mpidr,
  OUT BPMP_CPU_NDIV_LIMITS_RESPONSE  *Limits
  )
{
  EFI_STATUS                    Status;
  NVIDIA_BPMP_IPC_PROTOCOL      *BpmpIpcProtocol;
  BPMP_CPU_NDIV_LIMITS_REQUEST  Request;
  INT32                         MessageError;
  UINT32                        BpmpPhandle;

  if (Limits == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetCpuFreqAddresses (Mpidr, NULL, NULL, NULL, NULL, &BpmpPhandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->LocateProtocol (
                  &gNVIDIABpmpIpcProtocolGuid,
                  NULL,
                  (VOID **)&BpmpIpcProtocol
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!PcdGetBool (PcdAffinityMpIdrSupported)) {
    Request.ClusterId = GET_CLUSTER_ID (Mpidr);
  } else {
    Request.ClusterId = GET_MPIDR_AFF2 (Mpidr);
  }

  Status = BpmpIpcProtocol->Communicate (
                              BpmpIpcProtocol,
                              NULL,
                              BpmpPhandle,
                              MRQ_CPU_NDIV_LIMITS,
                              (VOID *)&Request,
                              sizeof (BPMP_CPU_NDIV_LIMITS_REQUEST),
                              (VOID *)Limits,
                              sizeof (BPMP_CPU_NDIV_LIMITS_RESPONSE),
                              &MessageError
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to request NDIV - %r -%d\r\n", __FUNCTION__, Status, MessageError));
  }

  return Status;
}

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
EFI_STATUS
TegraCpuFreqGetInfo (
  IN  NVIDIA_TEGRA_CPU_FREQ_PROTOCOL  *This,
  IN  UINT64                          Mpidr,
  OUT UINT64                          *CurrentFrequency OPTIONAL,
  OUT UINT64                          *HighestFrequency OPTIONAL,
  OUT UINT64                          *NominalFrequency OPTIONAL,
  OUT UINT64                          *LowestNonlinearFrequency OPTIONAL,
  OUT UINT64                          *LowestFrequency OPTIONAL
  )
{
  EFI_STATUS                     Status;
  BPMP_CPU_NDIV_LIMITS_RESPONSE  Limits;
  UINT16                         CurrentNdiv;

  Status = TegraCpuGetNdivLimits (Mpidr, &Limits);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (CurrentFrequency != NULL) {
    Status = GetCpuNdiv (Mpidr, &CurrentNdiv);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    *CurrentFrequency = ConvertNdivToFreq (&Limits, CurrentNdiv);
  }

  if (HighestFrequency != NULL) {
    *HighestFrequency = ConvertNdivToFreq (&Limits, Limits.ndiv_max);
  }

  if (NominalFrequency != NULL) {
    *NominalFrequency = ConvertNdivToFreq (&Limits, Limits.ndiv_max);
  }

  if (LowestNonlinearFrequency != NULL) {
    *LowestNonlinearFrequency = ConvertNdivToFreq (&Limits, Limits.ndiv_min);
  }

  if (LowestFrequency != NULL) {
    *LowestFrequency = ConvertNdivToFreq (&Limits, Limits.ndiv_min);
  }

  return Status;
}

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
EFI_STATUS
TegraCpuFreqSet (
  IN NVIDIA_TEGRA_CPU_FREQ_PROTOCOL  *This,
  IN UINT64                          Mpidr,
  IN UINT64                          DesiredFrequency
  )
{
  EFI_STATUS                     Status;
  BPMP_CPU_NDIV_LIMITS_RESPONSE  Limits;
  UINT16                         DesiredNdiv;

  Status = TegraCpuGetNdivLimits (Mpidr, &Limits);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DesiredNdiv = ConvertFreqToNdiv (&Limits, DesiredFrequency);
  return SetCpuNdiv (Mpidr, DesiredNdiv);
}

STATIC
VOID
EFIAPI
SetAddressStruct (
  IN EFI_ACPI_6_4_GENERIC_ADDRESS_STRUCTURE  *AddressStruct,
  IN UINT8                                   RegisterBitWidth,
  IN UINT8                                   RegisterBitOffset,
  IN UINT8                                   AccessSize,
  IN UINT64                                  Address

  )
{
  AddressStruct->AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY;
  AddressStruct->RegisterBitWidth  = RegisterBitWidth;
  AddressStruct->RegisterBitOffset = RegisterBitOffset;
  AddressStruct->AccessSize        = AccessSize;
  AddressStruct->Address           = Address;
}

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
EFI_STATUS
TegraCpuFreqGetCpcInfo (
  IN NVIDIA_TEGRA_CPU_FREQ_PROTOCOL  *This,
  IN UINT64                          Mpidr,
  IN CM_ARM_CPC_INFO                 *CpcInfo
  )
{
  EFI_STATUS                     Status;
  BPMP_CPU_NDIV_LIMITS_RESPONSE  Limits;
  EFI_PHYSICAL_ADDRESS           DesiredAddress;
  EFI_PHYSICAL_ADDRESS           RefclkClockAddress;
  EFI_PHYSICAL_ADDRESS           CoreClockAddress;
  VOID                           *PerfLimited;
  UINT64                         RefClockFreq;

  if (CpcInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = TegraCpuGetNdivLimits (Mpidr, &Limits);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = GetCpuFreqAddresses (Mpidr, &DesiredAddress, &RefclkClockAddress, &CoreClockAddress, &RefClockFreq, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->AllocatePool (EfiReservedMemoryType, sizeof (UINT32), &PerfLimited);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate buffer for PerfLimited\r\n"));
    return Status;
  }

  ZeroMem (PerfLimited, sizeof (UINT32));

  CpcInfo->Revision = 3;
  SetAddressStruct (&CpcInfo->HighestPerformanceBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  CpcInfo->HighestPerformanceInteger = Limits.ndiv_max;
  SetAddressStruct (&CpcInfo->NominalPerformanceBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  CpcInfo->NominalPerformanceInteger = Limits.ndiv_max;
  SetAddressStruct (&CpcInfo->LowestNonlinearPerformanceBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  CpcInfo->LowestNonlinearPerformanceInteger = Limits.ndiv_min;
  SetAddressStruct (&CpcInfo->LowestPerformanceBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  CpcInfo->LowestPerformanceInteger = Limits.ndiv_min;
  SetAddressStruct (&CpcInfo->GuaranteedPerformanceRegister, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  SetAddressStruct (&CpcInfo->DesiredPerformanceRegister, 32, 0, EFI_ACPI_6_4_DWORD, DesiredAddress);
  SetAddressStruct (&CpcInfo->MinimumPerformanceRegister, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  SetAddressStruct (&CpcInfo->MaximumPerformanceRegister, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  SetAddressStruct (&CpcInfo->PerformanceReductionToleranceRegister, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  SetAddressStruct (&CpcInfo->TimeWindowRegister, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  SetAddressStruct (&CpcInfo->CounterWraparoundTimeBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  if (CoreClockAddress == 0) {
    CpcInfo->CounterWraparoundTimeInteger = MAX_UINT64 / ConvertNdivToFreq (&Limits, Limits.ndiv_max);
  } else {
    CpcInfo->CounterWraparoundTimeInteger = MAX_UINT32 / ConvertNdivToFreq (&Limits, Limits.ndiv_max);
  }

  if (RefclkClockAddress == 0) {
    CpcInfo->ReferencePerformanceCounterRegister.AddressSpaceId    = EFI_ACPI_6_4_FUNCTIONAL_FIXED_HARDWARE;
    CpcInfo->ReferencePerformanceCounterRegister.RegisterBitWidth  = 64;
    CpcInfo->ReferencePerformanceCounterRegister.RegisterBitOffset = 0;
    CpcInfo->ReferencePerformanceCounterRegister.AccessSize        = EFI_ACPI_6_4_QWORD;
    CpcInfo->ReferencePerformanceCounterRegister.Address           = 0x1;
  } else {
    SetAddressStruct (&CpcInfo->ReferencePerformanceCounterRegister, 32, 0, EFI_ACPI_6_4_DWORD, RefclkClockAddress);
  }

  if (CoreClockAddress == 0) {
    CpcInfo->DeliveredPerformanceCounterRegister.AddressSpaceId    = EFI_ACPI_6_4_FUNCTIONAL_FIXED_HARDWARE;
    CpcInfo->DeliveredPerformanceCounterRegister.RegisterBitWidth  = 64;
    CpcInfo->DeliveredPerformanceCounterRegister.RegisterBitOffset = 0;
    CpcInfo->DeliveredPerformanceCounterRegister.AccessSize        = EFI_ACPI_6_4_QWORD;
    CpcInfo->DeliveredPerformanceCounterRegister.Address           = 0x0;
  } else {
    SetAddressStruct (&CpcInfo->DeliveredPerformanceCounterRegister, 32, 0, EFI_ACPI_6_4_DWORD, CoreClockAddress);
  }

  SetAddressStruct (&CpcInfo->PerformanceLimitedRegister, 32, 0, EFI_ACPI_6_4_DWORD, (UINT64)PerfLimited);
  SetAddressStruct (&CpcInfo->CPPCEnableRegister, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  SetAddressStruct (&CpcInfo->AutonomousSelectionEnableBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  CpcInfo->AutonomousSelectionEnableInteger = 0;
  SetAddressStruct (&CpcInfo->AutonomousActivityWindowRegister, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  SetAddressStruct (&CpcInfo->EnergyPerformancePreferenceRegister, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  SetAddressStruct (&CpcInfo->ReferencePerformanceBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  CpcInfo->ReferencePerformanceInteger = ConvertFreqToNdiv (&Limits, RefClockFreq);
  SetAddressStruct (&CpcInfo->LowestFrequencyBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  CpcInfo->LowestFrequencyInteger = HZ_TO_MHZ (ConvertNdivToFreq (&Limits, CpcInfo->LowestPerformanceInteger));
  SetAddressStruct (&CpcInfo->NominalFrequencyBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  CpcInfo->NominalFrequencyInteger = HZ_TO_MHZ (ConvertNdivToFreq (&Limits, CpcInfo->NominalPerformanceInteger));

  return EFI_SUCCESS;
}

NVIDIA_TEGRA_CPU_FREQ_PROTOCOL  mCpuFreqProtocol = {
  TegraCpuFreqGetInfo,
  TegraCpuFreqSet,
  TegraCpuFreqGetCpcInfo
};

/**
  Callback that will be invoked at various phases of the driver initialization

  This function allows for modification of system behavior at various points in
  the driver binding process.

  @param[in] Phase                    Current phase of the driver initialization
  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.
  @param[in] DeviceTreeNode           Pointer to the device tree node protocol is available.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_SUCCESS              Driver does not handle this phase
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryNotify (
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS  Status;
  EFI_STATUS  StatusLocal; // Local status for errors that should not be returned
  UINT64      CurrentFreq;
  UINT64      MaxFreq;

  Status = EFI_SUCCESS;

  switch (Phase) {
    case DeviceDiscoveryEnumerationCompleted:
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &DriverHandle,
                      &gNVIDIATegraCpuFrequencyProtocolGuid,
                      (VOID *)&mCpuFreqProtocol,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        break;
      }

      StatusLocal = mCpuFreqProtocol.GetInfo (
                                       &mCpuFreqProtocol,
                                       ArmReadMpidr (),
                                       &CurrentFreq,
                                       &MaxFreq,
                                       NULL,
                                       NULL,
                                       NULL
                                       );
      if (EFI_ERROR (StatusLocal)) {
        break;
      }

      StatusLocal = mCpuFreqProtocol.Set (
                                       &mCpuFreqProtocol,
                                       ArmReadMpidr (),
                                       MaxFreq
                                       );
      if (EFI_ERROR (StatusLocal)) {
        break;
      }

      break;

    default:
      break;
  }

  return Status;
}
