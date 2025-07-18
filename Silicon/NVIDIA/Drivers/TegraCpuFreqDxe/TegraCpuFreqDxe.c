/** @file

  Tegra CPU Frequency Driver.

  SPDX-FileCopyrightText: Copyright (c) 2022-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include <Library/NVIDIADebugLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/BpmpIpc.h>
#include <Protocol/DeviceTreeNode.h>
#include <Protocol/TegraCpuFreq.h>
#include <ArmNameSpaceObjects.h>

#include <Library/DeviceDiscoveryDriverLib.h>

#include "Base.h"
#include "ProcessorBind.h"
#include "TegraCpuFreqDxePrivate.h"
#include "Uefi/UefiBaseType.h"

// Additional GUIDs will need to update the GetCpuCppcOffsets and GetRefClockFreq functions
NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra234-ccplex-cluster", &gNVIDIACpuFreqT234  },
  { "nvidia,th500-cpufreq",           &gNVIDIACpuFreqTH500 },
  { "nvidia,tegra264-ccplex-cluster", &gNVIDIACpuFreqT264  },
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

typedef struct {
  UINTN    DesiredPerformance;
  UINTN    GuaranteedPerformance;
  UINTN    MinimumPerformance;
  UINTN    MaximumPerformance;
  UINTN    TimeWindow;
  UINTN    ReferencePerformanceCounter;
  UINTN    DeliveredPerformanceCounter;
  UINTN    PerformanceLimited;
  UINTN    AutonomousSelectionEnable;
  UINTN    AutonomousActivityWindowRegister;
  UINTN    EnergyPerformancePreference;
} CPPC_REGISTER_VALUES;

/**
 * Returns the device handle of the contoller that relates to the given MpIdr.
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
    DEBUG ((DEBUG_ERROR, "%a: No CPU frequency controllers found.\n", __func__));
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
      DEBUG ((DEBUG_ERROR, "%a: Failed to get socket for CPU frequency controller.\n", __func__));
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
 * This function retrieves the base address of the cpufreq controller for the
 * specified cpu.
 *
 * @param[in]  Mpidr              MpIdr of the CPU to get info on.
 * @param[out] BaseAddress        Returns the base address of the desired frequency register.
 *
 * @retval EFI_SUCCESS             The address waS returned.
 * @retval EFI_NOT_FOUND           Mpidr is not valid for this platform.
 * @retval EFI_UNSUPPORTED         Cpu Frequency driver does not support this platform.
 * @retval EFI_INVALID_PARAMETER   BaseAddress is NULL
 */
STATIC
EFI_STATUS
EFIAPI
GetCpuFreqBaseAddress (
  IN UINT64                 Mpidr,
  OUT EFI_PHYSICAL_ADDRESS  *BaseAddress
  )
{
  EFI_STATUS            Status;
  EFI_HANDLE            Handle;
  EFI_GUID              *DeviceGuid;
  EFI_PHYSICAL_ADDRESS  RegionBase;
  UINTN                 RegionSize;

  if (BaseAddress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceHandle (Mpidr, &Handle, &DeviceGuid);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get device handle for CPU frequency controller.\n", __func__));
    return Status;
  }

  Status = DeviceDiscoveryGetMmioRegion (Handle, 0, &RegionBase, &RegionSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get MMIO region for CPU frequency controller.\n", __func__));
    return Status;
  }

  *BaseAddress = RegionBase;
  return EFI_SUCCESS;
}

/**
 * This function retrieves the bpmp handle for the specified core.
 *
 * @param[in]  Mpidr              MpIdr of the CPU to get info on.
 * @param[out] BpmpPhandle        Returns the phandle of the BPMP node.
 *
 * @retval EFI_SUCCESS             The handle was returned.
 * @retval EFI_NOT_FOUND           Mpidr is not valid for this platform.
 * @retval EFI_UNSUPPORTED         Cpu Frequency driver does not support this platform.
 * @retval EFI_INVALID_PARAMETER   BpmpPhandle is NULL.
 */
STATIC
EFI_STATUS
EFIAPI
GetCpuFreqBpmpHandle (
  IN UINT64   Mpidr,
  OUT UINT32  *BpmpPhandle
  )
{
  EFI_STATUS                        Status;
  EFI_HANDLE                        Handle;
  EFI_GUID                          *DeviceGuid;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *Node;

  if (BpmpPhandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceHandle (Mpidr, &Handle, &DeviceGuid);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get device handle for CPU frequency controller.\n", __func__));
    return Status;
  }

  Status = gBS->HandleProtocol (Handle, &gNVIDIADeviceTreeNodeProtocolGuid, (VOID **)&Node);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get device tree node protocol.\n", __func__));
    return Status;
  }

  Status = DeviceTreeGetNodePropertyValue32 (Node->NodeOffset, "nvidia,bpmp", BpmpPhandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get Bpmp node phandle.\n", __func__));
  }

  return Status;
}

/**
 * This function retrieves the cppc offsets for the specified core.
 *
 * @param[in]  Mpidr              MpIdr of the CPU to get info on.
 * @param[out] CppcOffsets        Returns the Cppc offsets structure.
 * @param[out] CppcBitWidths      Returns the Cppc bit widths structure.
                                  Values of 0 indicate the register is not valid.
 *
 * @retval EFI_SUCCESS             The offsets were returned.
 * @retval EFI_NOT_FOUND           Mpidr is not valid for this platform.
 * @retval EFI_UNSUPPORTED         Cpu Frequency driver does not support this platform.
 * @retval EFI_INVALID_PARAMETER   CppcOffsets is NULL.
 */
STATIC
EFI_STATUS
EFIAPI
GetCpuCppcOffsets (
  IN UINT64                 Mpidr,
  OUT CPPC_REGISTER_VALUES  *CppcOffsets,
  OUT CPPC_REGISTER_VALUES  *CppcBitWidths
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;
  EFI_GUID    *DeviceGuid;
  UINT32      Cluster;
  UINT32      Core;

  if (CppcOffsets == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: CppcOffsets is NULL.\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  if (CppcBitWidths == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: CppcBitWidths is NULL.\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (CppcOffsets, sizeof (CPPC_REGISTER_VALUES));
  // Set all widths to 0 to indicate they are not valid
  ZeroMem (CppcBitWidths, sizeof (CPPC_REGISTER_VALUES));

  Status = GetDeviceHandle (Mpidr, &Handle, &DeviceGuid);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = MpCoreInfoGetProcessorLocation (Mpidr, NULL, &Cluster, &Core);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get cluster and core for CPU frequency controller.\n", __func__));
    return EFI_NOT_FOUND;
  }

  if (CompareGuid (DeviceGuid, &gNVIDIACpuFreqT234)) {
    CppcOffsets->DesiredPerformance            = T234_SCRATCH_FREQ_CORE_REG (Cluster, Core);
    CppcBitWidths->DesiredPerformance          = 32;
    CppcOffsets->ReferencePerformanceCounter   = T234_CLUSTER_ACTMON_REFCLK_REG (Cluster, Core);
    CppcBitWidths->ReferencePerformanceCounter = 32;
    CppcOffsets->DeliveredPerformanceCounter   = T234_CLUSTER_ACTMON_CORE_REG (Cluster, Core);
    CppcBitWidths->DeliveredPerformanceCounter = 32;
  } else if (CompareGuid (DeviceGuid, &gNVIDIACpuFreqTH500)) {
    CppcOffsets->DesiredPerformance   = TH500_SCRATCH_FREQ_CORE_REG (Cluster);
    CppcBitWidths->DesiredPerformance = 32;
  } else if (CompareGuid (DeviceGuid, &gNVIDIACpuFreqT264)) {
    CppcOffsets->DesiredPerformance   = T264_SCRATCH_FREQ_CORE_REG (Cluster);
    CppcBitWidths->DesiredPerformance = 32;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Unsupported CPU frequency controller.\n", __func__));
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/** This functions returns the ref clock frequency
 *
 * @param[in]  Mpidr        The MpIdr of the CPU to get info on.
 * @param[out] RefClockFreq  The reference clock frequency in Hz.
 *
 * @retval EFI_SUCCESS            The reference clock frequency was returned.
 * @retval EFI_UNSUPPORTED        The CPU frequency driver does not support this platform.
 * @retval EFI_INVALID_PARAMETER  RefClockFreq is NULL.
 */
STATIC
EFI_STATUS
EFIAPI
GetRefClockFreq (
  IN UINT64   Mpidr,
  OUT UINT64  *RefClockFreq
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;
  EFI_GUID    *DeviceGuid;

  if (RefClockFreq == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceHandle (Mpidr, &Handle, &DeviceGuid);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get device handle for CPU frequency controller.\n", __func__));
    return Status;
  }

  if (CompareGuid (DeviceGuid, &gNVIDIACpuFreqT234)) {
    *RefClockFreq = T234_REFCLK_FREQ;
  } else if (CompareGuid (DeviceGuid, &gNVIDIACpuFreqTH500)) {
    *RefClockFreq = TH500_REFCLK_FREQ;
  } else if (CompareGuid (DeviceGuid, &gNVIDIACpuFreqT264)) {
    *RefClockFreq = T264_REFCLK_FREQ;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Unsupported CPU frequency controller.\n", __func__));
    return EFI_UNSUPPORTED;
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

  Status = GetCpuFreqBpmpHandle (Mpidr, &BpmpPhandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get BPMP handle for CPU frequency controller.\n", __func__));
    return Status;
  }

  Status = gBS->LocateProtocol (
                  &gNVIDIABpmpIpcProtocolGuid,
                  NULL,
                  (VOID **)&BpmpIpcProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate BPMP IPC protocol.\n", __func__));
    return Status;
  }

  Status = MpCoreInfoGetProcessorLocation (Mpidr, NULL, &Request.ClusterId, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get cluster id for CPU frequency controller.\n", __func__));
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
    DEBUG ((DEBUG_ERROR, "%a: Failed to request NDIV - %r -%d\r\n", __func__, Status, MessageError));
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
  EFI_PHYSICAL_ADDRESS           BaseAddress;
  CPPC_REGISTER_VALUES           CppcOffsets;
  CPPC_REGISTER_VALUES           CppcBitWidths;
  EFI_PHYSICAL_ADDRESS           DesiredAddress;

  Status = TegraCpuGetNdivLimits (Mpidr, &Limits);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get NDIV limits for CPU frequency controller.\n", __func__));
    return Status;
  }

  if (CurrentFrequency != NULL) {
    Status = GetCpuFreqBaseAddress (Mpidr, &BaseAddress);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get base address for CPU frequency controller.\n", __func__));
      return Status;
    }

    Status = GetCpuCppcOffsets (Mpidr, &CppcOffsets, &CppcBitWidths);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get CPPC offsets for CPU frequency controller.\n", __func__));
      return Status;
    }

    if (CppcBitWidths.DesiredPerformance != 0) {
      DesiredAddress    = BaseAddress + CppcOffsets.DesiredPerformance;
      CurrentNdiv       = MmioBitFieldRead32 (DesiredAddress, 0, CppcBitWidths.DesiredPerformance-1);
      *CurrentFrequency = ConvertNdivToFreq (&Limits, CurrentNdiv);
    } else {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get current frequency for CPU frequency controller, no desired frequency supported.\n", __func__));
      return EFI_UNSUPPORTED;
    }
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
  EFI_PHYSICAL_ADDRESS           BaseAddress;
  CPPC_REGISTER_VALUES           CppcOffsets;
  CPPC_REGISTER_VALUES           CppcBitWidths;
  EFI_PHYSICAL_ADDRESS           DesiredAddress;

  Status = TegraCpuGetNdivLimits (Mpidr, &Limits);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get NDIV limits for CPU frequency controller.\n", __func__));
    return Status;
  }

  DesiredNdiv = ConvertFreqToNdiv (&Limits, DesiredFrequency);
  if ((DesiredNdiv < Limits.ndiv_min) || (DesiredNdiv > Limits.ndiv_max)) {
    DEBUG ((DEBUG_ERROR, "%a: Desired frequency is out of range. Request %u, Max %u, Min %u\n", __func__, DesiredNdiv, Limits.ndiv_max, Limits.ndiv_min));
    return EFI_INVALID_PARAMETER;
  }

  Status = GetCpuFreqBaseAddress (Mpidr, &BaseAddress);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get base address for CPU frequency controller.\n", __func__));
    return Status;
  }

  Status = GetCpuCppcOffsets (Mpidr, &CppcOffsets, &CppcBitWidths);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get CPPC offsets for CPU frequency controller.\n", __func__));
    return Status;
  }

  if (CppcBitWidths.DesiredPerformance == 0) {
    DEBUG ((DEBUG_ERROR, "%a: DesiredPerformance register not found for CPU frequency controller.\n", __func__));
    return EFI_UNSUPPORTED;
  }

  DesiredAddress = BaseAddress + CppcOffsets.DesiredPerformance;
  MmioBitFieldWrite32 (DesiredAddress, 0, CppcBitWidths.DesiredPerformance-1, DesiredNdiv);
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
  if (RegisterBitWidth == 0) {
    AddressStruct->AccessSize = EFI_ACPI_6_4_UNDEFINED;
    AddressStruct->Address    = 0;
  } else {
    AddressStruct->AccessSize = AccessSize;
    AddressStruct->Address    = Address;
  }
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
  OUT CM_ARCH_COMMON_CPC_INFO        *CpcInfo
  )
{
  EFI_STATUS                     Status;
  BPMP_CPU_NDIV_LIMITS_RESPONSE  Limits;
  EFI_PHYSICAL_ADDRESS           BaseAddress;
  CPPC_REGISTER_VALUES           CppcOffsets;
  CPPC_REGISTER_VALUES           CppcBitWidths;
  UINT64                         RefClockFreq;
  VOID                           *PerfLimited;

  if (CpcInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = TegraCpuGetNdivLimits (Mpidr, &Limits);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = GetCpuFreqBaseAddress (Mpidr, &BaseAddress);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get base address for CPU frequency controller.\n", __func__));
    return Status;
  }

  Status = GetCpuCppcOffsets (Mpidr, &CppcOffsets, &CppcBitWidths);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get CPPC offsets for CPU frequency controller.\n", __func__));
    return Status;
  }

  Status = GetRefClockFreq (Mpidr, &RefClockFreq);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get reference clock frequency for CPU frequency controller.\n", __func__));
    return Status;
  }

  if (CppcBitWidths.PerformanceLimited != 0) {
    PerfLimited = (VOID *)(BaseAddress + CppcOffsets.PerformanceLimited);
  } else {
    Status = gBS->AllocatePool (EfiReservedMemoryType, sizeof (UINT32), &PerfLimited);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate buffer for PerfLimited\r\n", __func__));
      return Status;
    }

    CppcBitWidths.PerformanceLimited = 32;
    ZeroMem (PerfLimited, sizeof (UINT32));
  }

  CpcInfo->Revision = 3;
  SetAddressStruct (&CpcInfo->HighestPerformanceBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  CpcInfo->HighestPerformanceInteger = Limits.ndiv_max;
  SetAddressStruct (&CpcInfo->NominalPerformanceBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  CpcInfo->NominalPerformanceInteger = Limits.ndiv_max;
  SetAddressStruct (&CpcInfo->LowestNonlinearPerformanceBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  CpcInfo->LowestNonlinearPerformanceInteger = Limits.ndiv_min;
  SetAddressStruct (&CpcInfo->LowestPerformanceBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  CpcInfo->LowestPerformanceInteger = Limits.ndiv_min;
  SetAddressStruct (&CpcInfo->GuaranteedPerformanceRegister, CppcBitWidths.GuaranteedPerformance, 0, EFI_ACPI_6_4_DWORD, BaseAddress + CppcOffsets.GuaranteedPerformance);
  // DesiredAddress is required
  NV_ASSERT_RETURN (CppcBitWidths.DesiredPerformance != 0, return EFI_UNSUPPORTED, "%a: DesiredPerformance register not found for CPU frequency controller.\n", __func__);

  SetAddressStruct (&CpcInfo->DesiredPerformanceRegister, CppcBitWidths.DesiredPerformance, 0, EFI_ACPI_6_4_DWORD, BaseAddress + CppcOffsets.DesiredPerformance);
  SetAddressStruct (&CpcInfo->MinimumPerformanceRegister, CppcBitWidths.MinimumPerformance, 0, EFI_ACPI_6_4_DWORD, BaseAddress + CppcOffsets.MinimumPerformance);
  SetAddressStruct (&CpcInfo->MaximumPerformanceRegister, CppcBitWidths.MaximumPerformance, 0, EFI_ACPI_6_4_DWORD, BaseAddress + CppcOffsets.MaximumPerformance);
  SetAddressStruct (&CpcInfo->PerformanceReductionToleranceRegister, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  SetAddressStruct (&CpcInfo->TimeWindowRegister, CppcBitWidths.TimeWindow, 0, EFI_ACPI_6_4_DWORD, BaseAddress + CppcOffsets.TimeWindow);
  SetAddressStruct (&CpcInfo->CounterWraparoundTimeBuffer, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);

  // ReferencePerformanceCounter register indicating that it is not support uses FFH register which is 64bits by specification.
  if (CppcBitWidths.ReferencePerformanceCounter == 0) {
    CpcInfo->CounterWraparoundTimeInteger = MAX_UINT64 / ConvertNdivToFreq (&Limits, Limits.ndiv_max);
  } else {
    CpcInfo->CounterWraparoundTimeInteger = MAX_UINT32 / ConvertNdivToFreq (&Limits, Limits.ndiv_max);
  }

  if (CppcBitWidths.ReferencePerformanceCounter == 0) {
    CpcInfo->ReferencePerformanceCounterRegister.AddressSpaceId    = EFI_ACPI_6_4_FUNCTIONAL_FIXED_HARDWARE;
    CpcInfo->ReferencePerformanceCounterRegister.RegisterBitWidth  = 64;
    CpcInfo->ReferencePerformanceCounterRegister.RegisterBitOffset = 0;
    CpcInfo->ReferencePerformanceCounterRegister.AccessSize        = EFI_ACPI_6_4_QWORD;
    CpcInfo->ReferencePerformanceCounterRegister.Address           = 0x1;
  } else {
    SetAddressStruct (&CpcInfo->ReferencePerformanceCounterRegister, CppcBitWidths.ReferencePerformanceCounter, 0, EFI_ACPI_6_4_DWORD, BaseAddress + CppcOffsets.ReferencePerformanceCounter);
  }

  if (CppcBitWidths.DeliveredPerformanceCounter == 0) {
    CpcInfo->DeliveredPerformanceCounterRegister.AddressSpaceId    = EFI_ACPI_6_4_FUNCTIONAL_FIXED_HARDWARE;
    CpcInfo->DeliveredPerformanceCounterRegister.RegisterBitWidth  = 64;
    CpcInfo->DeliveredPerformanceCounterRegister.RegisterBitOffset = 0;
    CpcInfo->DeliveredPerformanceCounterRegister.AccessSize        = EFI_ACPI_6_4_QWORD;
    CpcInfo->DeliveredPerformanceCounterRegister.Address           = 0x0;
  } else {
    SetAddressStruct (&CpcInfo->DeliveredPerformanceCounterRegister, CppcBitWidths.DeliveredPerformanceCounter, 0, EFI_ACPI_6_4_DWORD, BaseAddress + CppcOffsets.DeliveredPerformanceCounter);
  }

  SetAddressStruct (&CpcInfo->PerformanceLimitedRegister, CppcBitWidths.PerformanceLimited, 0, EFI_ACPI_6_4_DWORD, (UINT64)PerfLimited);
  SetAddressStruct (&CpcInfo->CPPCEnableRegister, 0, 0, EFI_ACPI_6_4_UNDEFINED, 0);
  SetAddressStruct (&CpcInfo->AutonomousSelectionEnableBuffer, CppcBitWidths.AutonomousSelectionEnable, 0, EFI_ACPI_6_4_DWORD, BaseAddress + CppcOffsets.AutonomousSelectionEnable);
  CpcInfo->AutonomousSelectionEnableInteger = 0;
  SetAddressStruct (&CpcInfo->AutonomousActivityWindowRegister, CppcBitWidths.AutonomousActivityWindowRegister, 0, EFI_ACPI_6_4_DWORD, BaseAddress + CppcOffsets.AutonomousActivityWindowRegister);
  SetAddressStruct (&CpcInfo->EnergyPerformancePreferenceRegister, CppcBitWidths.EnergyPerformancePreference, 0, EFI_ACPI_6_4_DWORD, BaseAddress + CppcOffsets.EnergyPerformancePreference);
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
