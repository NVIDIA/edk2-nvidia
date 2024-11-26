/** @file

  Tegra CPU Frequency Driver.

  SPDX-FileCopyrightText: Copyright (c) 2022-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MpCoreInfoLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/BpmpIpc.h>
#include <Protocol/DeviceTreeNode.h>
#include <Protocol/TegraCpuFreq.h>
#include <ArmNameSpaceObjects.h>

#include <Library/DeviceDiscoveryDriverLib.h>

#include "TegraCpuFreqDxePrivate.h"

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra234-ccplex-cluster", &gNVIDIACpuFreqT234  },
  { "nvidia,th500-cpufreq",           &gNVIDIACpuFreqTH500 },
  { NULL,                             NULL                 }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                                 = L"NVIDIA Tegra CPU Driver",
  .AutoEnableClocks                           = TRUE,
  .AutoResetModule                            = TRUE,
  .AutoDeassertPg                             = TRUE,
  .SkipEdkiiNondiscoverableInstall            = TRUE,
  .SkipAutoDeinitControllerOnExitBootServices = TRUE
};

// Cache the handles as they won't change
STATIC EFI_HANDLE  *mCpuFreqHandles    = NULL;
STATIC UINTN       mCpuFreqHandleCount = 0;
STATIC EFI_GUID    *mCpuFreqGuid       = NULL;

/**
 * Returns the device handle of the relates to the given MpIdr.
 *
 * @param[in]  Mpidr       The MpIdr of the CPU to get the device handle for.
 * @param[out] Handle      The device handle of the CPU frequency node.
 * @param[out] DeviceGuid  The device guid of the CPU frequency node.
 *
 * @retval EFI_SUCCESS     The device handle was returned.
 * @retval EFI_NOT_FOUND   The device handle was not found.
 * @retval EFI_UNSUPPORTED The CPU frequency driver does not support this platform.
 */
STATIC
EFI_STATUS
EFIAPI
GetDeviceHandle (
  IN UINT64       Mpidr,
  OUT EFI_HANDLE  *Handle,
  OUT EFI_GUID    **DeviceGuid
  )
{
  EFI_STATUS                        Status;
  UINT32                            NodeSocket;
  UINT32                            Socket;
  UINTN                             Index;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node;
  INT32                             ParentOffset;

  if (mCpuFreqHandles == NULL) {
    Index = 0;
    while (gDeviceCompatibilityMap[Index].DeviceType != NULL) {
      Status = gBS->LocateHandleBuffer (
                      ByProtocol,
                      gDeviceCompatibilityMap[Index].DeviceType,
                      NULL,
                      &mCpuFreqHandleCount,
                      &mCpuFreqHandles
                      );
      if (!EFI_ERROR (Status)) {
        mCpuFreqGuid = gDeviceCompatibilityMap[Index].DeviceType;
        break;
      }

      Index++;
    }
  }

  // No CPU frequency controllers found
  if (mCpuFreqHandles == NULL) {
    return EFI_UNSUPPORTED;
  }

  if (mCpuFreqHandleCount == 1) {
    *Handle     = mCpuFreqHandles[0];
    *DeviceGuid = mCpuFreqGuid;
    return EFI_SUCCESS;
  } else {
    // Multiple CPU frequency controllers found, find the one that matches the socket
    Status = MpCoreInfoGetProcessorLocation (Mpidr, &Socket, NULL, NULL);
    if (EFI_ERROR (Status)) {
      return EFI_NOT_FOUND;
    }

    for (Index = 0; Index < mCpuFreqHandleCount; Index++) {
      Status = gBS->HandleProtocol (mCpuFreqHandles[Index], &gNVIDIADeviceTreeNodeProtocolGuid, (VOID **)&Node);
      if (EFI_ERROR (Status)) {
        continue;
      }

      Status = DeviceTreeGetParentOffset (Node->NodeOffset, &ParentOffset);
      if (EFI_ERROR (Status)) {
        continue;
      }

      Status = DeviceTreeGetNodePropertyValue32 (ParentOffset, "reg", &NodeSocket);
      if (EFI_ERROR (Status)) {
        continue;
      }

      if (NodeSocket == Socket) {
        *Handle     = mCpuFreqHandles[Index];
        *DeviceGuid = mCpuFreqGuid;
        return EFI_SUCCESS;
      }
    }
  }

  return EFI_NOT_FOUND;
}

/**
 * This function retrieves the addresses of the CPU frequency registers for the specified core.
 *
 * @param[in]  Mpidr              MpIdr of the CPU to get info on.
 * @param[out] NdivAddress        If provided, returns the address of the NDIV register.
 * @param[out] RefClockAddress    If provided, returns the address of the reference clock register.
 * @param[out] CoreClockAddress   If provided, returns the address of the core clock register.
 * @param[out] RefClockFreq       If provided, returns the reference clock frequency.
 * @param[out] BpmpPhandle        If provided, returns the phandle of the BPMP node.
 *
 * @retval EFI_SUCCESS             The addresses were returned.
 * @retval EFI_NOT_FOUND           Mpidr is not valid for this platform.
 * @retval EFI_UNSUPPORTED         Cpu Frequency driver does not support this platform.
 */
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
  EFI_HANDLE                        Handle;
  EFI_GUID                          *DeviceGuid;
  EFI_PHYSICAL_ADDRESS              RegionBase;
  UINTN                             RegionSize;
  UINT32                            Cluster;
  UINT32                            Core;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node;
  EFI_PHYSICAL_ADDRESS              LocalNdivAddress;
  EFI_PHYSICAL_ADDRESS              LocalRefClockAddress;
  EFI_PHYSICAL_ADDRESS              LocalCoreClockAddress;
  UINT64                            LocalRefClockFreq;

  Status = GetDeviceHandle (Mpidr, &Handle, &DeviceGuid);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = MpCoreInfoGetProcessorLocation (Mpidr, NULL, &Cluster, &Core);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  Status = gBS->HandleProtocol (Handle, &gNVIDIADeviceTreeNodeProtocolGuid, (VOID **)&Node);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DeviceDiscoveryGetMmioRegion (Handle, 0, &RegionBase, &RegionSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  LocalNdivAddress      = 0;
  LocalRefClockAddress  = 0;
  LocalCoreClockAddress = 0;
  LocalRefClockFreq     = 0;

  if (DeviceGuid == &gNVIDIACpuFreqT234) {
    LocalNdivAddress      = RegionBase + T234_SCRATCH_FREQ_CORE_REG (Cluster, Core);
    LocalRefClockAddress  = RegionBase + T234_CLUSTER_ACTMON_REFCLK_REG (Cluster, Core);
    LocalCoreClockAddress = RegionBase + T234_CLUSTER_ACTMON_CORE_REG (Cluster, Core);
    LocalRefClockFreq     = T234_REFCLK_FREQ;
  } else if (DeviceGuid == &gNVIDIACpuFreqTH500) {
    LocalNdivAddress  = RegionBase + TH500_SCRATCH_FREQ_CORE_REG (Cluster);
    LocalRefClockFreq = TH500_REFCLK_FREQ;
  }

  if (NdivAddress != NULL) {
    *NdivAddress = LocalNdivAddress;
  }

  if (RefClockAddress != NULL) {
    *RefClockAddress = LocalRefClockAddress;
  }

  if (CoreClockAddress != NULL) {
    *CoreClockAddress = LocalCoreClockAddress;
  }

  if (RefClockFreq != NULL) {
    *RefClockFreq = LocalRefClockFreq;
  }

  if (BpmpPhandle != NULL) {
    Status = DeviceTreeGetNodePropertyValue32 (Node->NodeOffset, "nvidia,bpmp", BpmpPhandle);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to get Bpmp node phandle.\n"));
    }
  }

  return EFI_SUCCESS;
}

/**
 * This function converts an NDIV value to a frequency.
 *
 * @param[in] Limits  The NDIV limits for the CPU.
 * @param[in] Ndiv    The NDIV value to convert.
 *
 * @return The frequency in Hz.
 */
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

/**
 * This function converts a frequency to an NDIV value.
 *
 * @param[in] Limits  The NDIV limits for the CPU.
 * @param[in] Freq    The frequency to convert.
 *
 * @return The NDIV value.
 */
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

/**
 * This function retrieves the NDIV limits for the specified core.
 *
 * @param[in]  Mpidr  MpIdr of the CPU to get info on.
 * @param[out] Limits The NDIV limits for the CPU.
 *
 * @retval EFI_SUCCESS     The NDIV limits were returned.
 * @retval EFI_NOT_FOUND   Mpidr is not valid for this platform.
 * @retval EFI_UNSUPPORTED The CPU frequency driver does not support this platform.
 */
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

  Status = MpCoreInfoGetProcessorLocation (Mpidr, NULL, &Request.ClusterId, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
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
  EFI_PHYSICAL_ADDRESS           NdivAddress;

  Status = TegraCpuGetNdivLimits (Mpidr, &Limits);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (CurrentFrequency != NULL) {
    Status = GetCpuFreqAddresses (Mpidr, &NdivAddress, NULL, NULL, NULL, NULL);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    CurrentNdiv       = MmioRead32 (NdivAddress);
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
  EFI_PHYSICAL_ADDRESS           NdivAddress;

  Status = TegraCpuGetNdivLimits (Mpidr, &Limits);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DesiredNdiv = ConvertFreqToNdiv (&Limits, DesiredFrequency);
  Status      = GetCpuFreqAddresses (Mpidr, &NdivAddress, NULL, NULL, NULL, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmioWrite32 (NdivAddress, DesiredNdiv);
  return EFI_SUCCESS;
}

/**
 * This function builds the address structure for the given parameters.
 */
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
  IN CM_ARCH_COMMON_CPC_INFO         *CpcInfo
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
  EFI_STATUS  StatusLocal;   // Local status for errors that should not be returned
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

      // Set the boot CPU to max frequency
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
