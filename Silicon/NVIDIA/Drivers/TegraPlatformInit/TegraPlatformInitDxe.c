/** @file

  Tegra Platform Init Driver.

  Copyright (c) 2018-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/HobLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/DtPlatformDtbLoaderLib.h>
#include <Library/FloorSweepingLib.h>
#include <libfdt.h>
#include <Guid/ImageAuthentication.h>
#include <UefiSecureBoot.h>
#include <Library/SecureBootVariableLib.h>
#include <Library/TegraPlatformInfoLib.h>

#define TH500_PCI_COMPAT  "nvidia,th500-pcie"

/**
  Check if the Device is an AGX Xavier Device type.

  @retval TRUE  Device is an AGX Xavier.
  @retval FALSE Not an AGX Xavier Device.

**/
STATIC
BOOLEAN
IsAgxXavier (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT32      NumberOfPlatformNodes;

  NumberOfPlatformNodes = 0;
  Status                = GetMatchingEnabledDeviceTreeNodes ("nvidia,p2972-0000", NULL, &NumberOfPlatformNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  NumberOfPlatformNodes = 0;
  Status                = GetMatchingEnabledDeviceTreeNodes ("nvidia,galen", NULL, &NumberOfPlatformNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  NumberOfPlatformNodes = 0;
  Status                = GetMatchingEnabledDeviceTreeNodes ("nvidia,e3360_1099", NULL, &NumberOfPlatformNodes);
  if (Status != EFI_NOT_FOUND) {
    return TRUE;
  }

  return FALSE;
}

STATIC
VOID
SetPhysicalPresencePcd (
  VOID
  )
{
  if ((IsAgxXavier () == TRUE)) {
    DEBUG ((DEBUG_ERROR, "Setting Physical Presence to TRUE\n"));
    PcdSetBoolS (PcdUserPhysicalPresence, TRUE);
  }
}

STATIC
VOID
EFIAPI
SetCpuInfoPcdsFromDtb (
  VOID
  )
{
  VOID        *Dtb;
  UINTN       DtbSize;
  UINTN       MaxClusters;
  UINTN       MaxCoresPerCluster;
  UINTN       MaxSockets;
  INT32       CpuMapOffset;
  INT32       Cluster0Offset;
  INT32       NodeOffset;
  CHAR8       ClusterNodeStr[] = "clusterxxx";
  CHAR8       CoreNodeStr[]    = "corexx";
  EFI_STATUS  Status;
  CHAR8       SocketNodeStr[] = "/socket@xx";
  INT32       SocketOffset;
  CHAR8       CpuMapPathStr[] = "/socket@xx/cpus/cpu-map";
  CHAR8       *CpuMapPathFormat;
  UINTN       Socket;

  Status = DtPlatformLoadDtb (&Dtb, &DtbSize);
  if (EFI_ERROR (Status)) {
    return;
  }

  // count number of socket nodes, 100 limit due to socket@xx string
  for (MaxSockets = 0; MaxSockets < 100; MaxSockets++) {
    AsciiSPrint (SocketNodeStr, sizeof (SocketNodeStr), "/socket@%u", MaxSockets);
    SocketOffset = fdt_path_offset (Dtb, SocketNodeStr);
    if (SocketOffset < 0) {
      break;
    }
  }

  // handle global vs per-socket cpu map
  if (MaxSockets == 0) {
    MaxSockets       = 1;
    CpuMapPathFormat = "/cpus/cpu-map";
  } else {
    CpuMapPathFormat = "/socket@%u/cpus/cpu-map";
  }

  DEBUG ((DEBUG_INFO, "MaxSockets=%u\n", MaxSockets));
  PcdSet32S (PcdTegraMaxSockets, MaxSockets);

  // count clusters across all sockets
  MaxClusters = 0;
  for (Socket = 0; Socket < MaxSockets; Socket++) {
    UINTN  Cluster;

    AsciiSPrint (CpuMapPathStr, sizeof (CpuMapPathStr), CpuMapPathFormat, Socket);
    CpuMapOffset = fdt_path_offset (Dtb, CpuMapPathStr);
    if (CpuMapOffset < 0) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: %a missing in DTB, using Clusters=%u, CoresPerCluster=%u\n",
        __FUNCTION__,
        CpuMapPathStr,
        PcdGet32 (PcdTegraMaxClusters),
        PcdGet32 (PcdTegraMaxCoresPerCluster)
        ));
      return;
    }

    Cluster = 0;
    while (TRUE) {
      AsciiSPrint (ClusterNodeStr, sizeof (ClusterNodeStr), "cluster%u", Cluster);
      NodeOffset = fdt_subnode_offset (Dtb, CpuMapOffset, ClusterNodeStr);
      if (NodeOffset < 0) {
        break;
      }

      MaxClusters++;
      Cluster++;
      ASSERT (Cluster < 1000);    // "clusterxxx" max
    }

    DEBUG ((DEBUG_INFO, "Socket=%u MaxClusters=%u\n", Socket, MaxClusters));
  }

  DEBUG ((DEBUG_INFO, "MaxClusters=%u\n", MaxClusters));
  PcdSet32S (PcdTegraMaxClusters, MaxClusters);

  // Use cluster0 node to find max core subnode
  Cluster0Offset = fdt_subnode_offset (Dtb, CpuMapOffset, "cluster0");
  if (Cluster0Offset < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "No cluster0 in %a, using CoresPerCluster=%u\n",
      CpuMapPathStr,
      PcdGet32 (PcdTegraMaxCoresPerCluster)
      ));
    return;
  }

  MaxCoresPerCluster = 1;
  while (TRUE) {
    AsciiSPrint (CoreNodeStr, sizeof (CoreNodeStr), "core%u", MaxCoresPerCluster);
    NodeOffset = fdt_subnode_offset (Dtb, Cluster0Offset, CoreNodeStr);
    if (NodeOffset < 0) {
      break;
    }

    MaxCoresPerCluster++;
    ASSERT (MaxCoresPerCluster < 100);     // "corexx" max
  }

  DEBUG ((DEBUG_INFO, "MaxCoresPerCluster=%u\n", MaxCoresPerCluster));
  PcdSet32S (PcdTegraMaxCoresPerCluster, MaxCoresPerCluster);
}

STATIC
EFI_STATUS
EFIAPI
UseEmulatedVariableStore (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status;

  PcdSetBoolS (PcdEmuVariableNvModeEnable, TRUE);
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEdkiiNvVarStoreFormattedGuid,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error installing EmuVariableNvModeEnableProtocol\n", __FUNCTION__));
  }

  return Status;
}

/**
  Setup PCDs for Oem Table Id based on dtb info
  for Th500
**/
STATIC
EFI_STATUS
EFIAPI
SetOemTableIdPcdForTh500 (
  VOID
  )
{
  UINT32      SocketIndex;
  CHAR8       OemTableIdStr[] = "T241xxx";
  BOOLEAN     GpuPresent;
  EFI_STATUS  Status;
  UINT32      NumberOfPciHandles;
  UINT32      *PciHandles;
  VOID        *DeviceTreeBase;
  INT32       NodeOffset;
  UINT32      SocketCount;
  UINT32      Index;

  GpuPresent         = FALSE;
  NumberOfPciHandles = 0;
  SocketCount        = 0;
  PciHandles         = NULL;

  // Identify if a GPU is present or not
  Status = GetMatchingEnabledDeviceTreeNodes (TH500_PCI_COMPAT, NULL, &NumberOfPciHandles);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    NumberOfPciHandles = 0;
  } else {
    PciHandles = AllocateZeroPool (sizeof (UINT32) * NumberOfPciHandles);
    if (PciHandles == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate array for cpuidle cores\r\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    Status = GetMatchingEnabledDeviceTreeNodes (TH500_PCI_COMPAT, PciHandles, &NumberOfPciHandles);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get PCI handles %r\r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  for (Index = 0; Index < NumberOfPciHandles; Index++) {
    GetDeviceTreeNode (PciHandles[Index], &DeviceTreeBase, &NodeOffset);
    if ((NULL != fdt_getprop (DeviceTreeBase, NodeOffset, "c2c-partitions", NULL)) ||
        (NULL != fdt_getprop (DeviceTreeBase, NodeOffset, "hbm-ranges", NULL)))
    {
      GpuPresent = TRUE;
      break;
    }
  }

  // Find socket count in the system
  for (SocketIndex = 0; SocketIndex < PcdGet32 (PcdTegraMaxSockets); SocketIndex++ ) {
    if (!IsSocketEnabled (SocketIndex)) {
      continue;
    }

    SocketCount++;
  }

  // Set Pcd for Oem Table ID
  if (GpuPresent) {
    AsciiSPrint (OemTableIdStr, sizeof (OemTableIdStr), "T241x%u", SocketCount);
  } else {
    AsciiSPrint (OemTableIdStr, sizeof (OemTableIdStr), "T241c%u", SocketCount);
  }

  PcdSet64S (PcdAcpiDefaultOemTableId, *(UINT64 *)(OemTableIdStr));

  return EFI_SUCCESS;
}

STATIC
VOID
EFIAPI
SetBandwidthLatencyInfoPcdsFromdtb (
  IN VOID  *Dtb
  )
{
  CONST UINT32  *Property;

  UINT32  CpuToLocalCpuReadLatency;
  UINT32  CpuToLocalCpuWriteLatency;
  UINT32  CpuToLocalHbmReadLatency;
  UINT32  CpuToLocalHbmWriteLatency;
  UINT32  CpuToRemoteCpuReadLatency;
  UINT32  CpuToRemoteCpuWriteLatency;
  UINT32  CpuToRemoteHbmReadLatency;
  UINT32  CpuToRemoteHbmWriteLatency;
  UINT32  GpuToLocalHbmReadLatency;
  UINT32  GpuToLocalHbmWriteLatency;
  UINT32  GpuToLocalCpuReadLatency;
  UINT32  GpuToLocalCpuWriteLatency;
  UINT32  GpuToRemoteHbmReadLatency;
  UINT32  GpuToRemoteHbmWriteLatency;
  UINT32  GpuToRemoteCpuReadLatency;
  UINT32  GpuToRemoteCpuWriteLatency;
  UINT32  CpuToLocalCpuAccessBandwidth;
  UINT32  CpuToLocalHbmAccessBandwidth;
  UINT32  CpuToRemoteCpuAccessBandwidth;
  UINT32  CpuToRemoteHbmAccessBandwidth;
  UINT32  GpuToLocalHbmAccessBandwidth;
  UINT32  GpuToLocalCpuAccessBandwidth;
  UINT32  GpuToRemoteHbmAccessBandwidth;
  UINT32  GpuToRemoteCpuAccessBandwidth;
  INTN    AcpiNode;

  AcpiNode = fdt_path_offset (Dtb, "/firmware/acpi");
  if (AcpiNode >= 0) {
    // Obtain Latency info
    Property = fdt_getprop (Dtb, AcpiNode, "cpu-localcpu-read-latency", NULL);
    if (Property != NULL) {
      CpuToLocalCpuReadLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToLocalCpuReadLatency, CpuToLocalCpuReadLatency);
      DEBUG ((EFI_D_INFO, "Cpu To Local Cpu Read Latency = 0x%X\n", PcdGet32 (PcdCpuToLocalCpuReadLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To Local Cpu Read Latency not found, using 0x%X\n", PcdGet32 (PcdCpuToLocalCpuReadLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-localcpu-write-latency", NULL);
    if (Property != NULL) {
      CpuToLocalCpuWriteLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToLocalCpuWriteLatency, CpuToLocalCpuWriteLatency);
      DEBUG ((EFI_D_INFO, "Cpu To Local Cpu Write Latency = 0x%X\n", PcdGet32 (PcdCpuToLocalCpuWriteLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To Local Cpu Write Latency not found, using 0x%X\n", PcdGet32 (PcdCpuToLocalCpuWriteLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-localhbm-read-latency", NULL);
    if (Property != NULL) {
      CpuToLocalHbmReadLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToLocalHbmReadLatency, CpuToLocalHbmReadLatency);
      DEBUG ((EFI_D_INFO, "Cpu To local HBM Read Latency = 0x%X\n", PcdGet32 (PcdCpuToLocalHbmReadLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To Local HBM Read Latency not found, using 0x%X\n", PcdGet32 (PcdCpuToLocalHbmReadLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-localhbm-write-latency", NULL);
    if (Property != NULL) {
      CpuToLocalHbmWriteLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToLocalHbmWriteLatency, CpuToLocalHbmWriteLatency);
      DEBUG ((EFI_D_INFO, "Cpu To LocalHbm Write Latency = 0x%X\n", PcdGet32 (PcdCpuToLocalHbmWriteLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To LocalHbm Write Latency not found, using 0x%X\n", PcdGet32 (PcdCpuToLocalHbmWriteLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-remotehbm-read-latency", NULL);
    if (Property != NULL) {
      CpuToRemoteHbmReadLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToRemoteHbmReadLatency, CpuToRemoteHbmReadLatency);
      DEBUG ((EFI_D_INFO, "Cpu To RemoteHbm Read Latency = 0x%X\n", PcdGet32 (PcdCpuToRemoteHbmReadLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To RemoteHbm Read Latency not found, using 0x%X\n", PcdGet32 (PcdCpuToRemoteHbmReadLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-remotehbm-write-latency", NULL);
    if (Property != NULL) {
      CpuToRemoteHbmWriteLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToRemoteHbmWriteLatency, CpuToRemoteHbmWriteLatency);
      DEBUG ((EFI_D_INFO, "Cpu To RemoteHbm Write Latency = 0x%X\n", PcdGet32 (PcdCpuToRemoteHbmWriteLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To RemoteHbm Write Latency not found, using 0x%X\n", PcdGet32 (PcdCpuToRemoteHbmWriteLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-remotecpu-read-latency", NULL);
    if (Property != NULL) {
      CpuToRemoteCpuReadLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToRemoteCpuReadLatency, CpuToRemoteCpuReadLatency);
      DEBUG ((EFI_D_INFO, "Cpu To Remote Cpu Read Latency = 0x%X\n", PcdGet32 (PcdCpuToRemoteCpuReadLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To Remote Cpu Read Latency not found, using 0x%X\n", PcdGet32 (PcdCpuToRemoteCpuReadLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-remotecpu-write-latency", NULL);
    if (Property != NULL) {
      CpuToRemoteCpuWriteLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToRemoteCpuWriteLatency, CpuToRemoteCpuWriteLatency);
      DEBUG ((EFI_D_INFO, "Cpu To Remote Cpu Write Latency = 0x%X\n", PcdGet32 (PcdCpuToRemoteCpuWriteLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To Remote Cpu Write Latency not found, using 0x%X\n", PcdGet32 (PcdCpuToRemoteCpuWriteLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-localhbm-read-latency", NULL);
    if (Property != NULL) {
      GpuToLocalHbmReadLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToLocalHbmReadLatency, GpuToLocalHbmReadLatency);
      DEBUG ((EFI_D_INFO, "Gpu To Local HBM Read Latency = 0x%X\n", PcdGet32 (PcdGpuToLocalHbmReadLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Local HBM Read Latency not found, using 0x%X\n", PcdGet32 (PcdGpuToLocalHbmReadLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-localhbm-write-latency", NULL);
    if (Property != NULL) {
      GpuToLocalHbmWriteLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToLocalHbmWriteLatency, GpuToLocalHbmWriteLatency);
      DEBUG ((EFI_D_INFO, "Gpu To Local HBM Write Latency = 0x%X\n", PcdGet32 (PcdGpuToLocalHbmWriteLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Local HBM Write Latency not found, using 0x%X\n", PcdGet32 (PcdGpuToLocalHbmWriteLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-localcpu-read-latency", NULL);
    if (Property != NULL) {
      GpuToLocalCpuReadLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToLocalCpuReadLatency, GpuToLocalCpuReadLatency);
      DEBUG ((EFI_D_INFO, "Gpu To Local Cpu Read Latency = 0x%X\n", PcdGet32 (PcdGpuToLocalCpuReadLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Local Cpu Read Latency not found, using 0x%X\n", PcdGet32 (PcdGpuToLocalCpuReadLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-localcpu-write-latency", NULL);
    if (Property != NULL) {
      GpuToLocalCpuWriteLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToLocalCpuWriteLatency, GpuToLocalCpuWriteLatency);
      DEBUG ((EFI_D_INFO, "Gpu To Local Cpu Write Latency = 0x%X\n", PcdGet32 (PcdGpuToLocalCpuWriteLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Local Cpu Write Latency not found, using 0x%X\n", PcdGet32 (PcdGpuToLocalCpuWriteLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-remotehbm-read-latency", NULL);
    if (Property != NULL) {
      GpuToRemoteHbmReadLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToRemoteHbmReadLatency, GpuToRemoteHbmReadLatency);
      DEBUG ((EFI_D_INFO, "Gpu To Remote Gpu HBM Read Latency = 0x%X\n", PcdGet32 (PcdGpuToRemoteHbmReadLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Remote Gpu HBM Read Latency not found, using 0x%X\n", PcdGet32 (PcdGpuToRemoteHbmReadLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-remotehbm-write-latency", NULL);
    if (Property != NULL) {
      GpuToRemoteHbmWriteLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToRemoteHbmWriteLatency, GpuToRemoteHbmWriteLatency);
      DEBUG ((EFI_D_INFO, "Gpu To Remote Gpu HBM Write Latency = 0x%X\n", PcdGet32 (PcdGpuToRemoteHbmWriteLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Remote Gpu HBM Write Latency not found, using 0x%X\n", PcdGet32 (PcdGpuToRemoteHbmWriteLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-remotecpu-read-latency", NULL);
    if (Property != NULL) {
      GpuToRemoteCpuReadLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToRemoteCpuReadLatency, GpuToRemoteCpuReadLatency);
      DEBUG ((EFI_D_INFO, "Gpu To Remote Cpu Read Latency = 0x%X\n", PcdGet32 (PcdGpuToRemoteCpuReadLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Remote Cpu Read Latency not found, using 0x%X\n", PcdGet32 (PcdGpuToRemoteCpuReadLatency)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-remotecpu-write-latency", NULL);
    if (Property != NULL) {
      GpuToRemoteCpuWriteLatency = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToRemoteCpuWriteLatency, GpuToRemoteCpuWriteLatency);
      DEBUG ((EFI_D_INFO, "Gpu To Remote Cpu Write Latency = 0x%X\n", PcdGet32 (PcdGpuToRemoteCpuWriteLatency)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Remote Cpu Write Latency not found, using 0x%X\n", PcdGet32 (PcdGpuToRemoteCpuWriteLatency)));
    }

    // Obtain Bandwidth info
    Property = fdt_getprop (Dtb, AcpiNode, "cpu-localcpu-accessbandwidth", NULL);
    if (Property != NULL) {
      CpuToLocalCpuAccessBandwidth = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToLocalCpuAccessBandwidth, CpuToLocalCpuAccessBandwidth);
      DEBUG ((EFI_D_INFO, "Cpu To Local Cpu Access Bandwidth = 0x%X\n", PcdGet32 (PcdCpuToLocalCpuAccessBandwidth)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To Local Cpu Access Bandwidth not found, using 0x%X\n", PcdGet32 (PcdCpuToLocalCpuAccessBandwidth)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-localhbm-accessbandwidth", NULL);
    if (Property != NULL) {
      CpuToLocalHbmAccessBandwidth = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToLocalHbmAccessBandwidth, CpuToLocalHbmAccessBandwidth);
      DEBUG ((EFI_D_INFO, "Cpu To LocalHbm Access Bandwidth = 0x%X\n", PcdGet32 (PcdCpuToLocalHbmAccessBandwidth)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To LocalHbm Access Bandwidth not found, using 0x%X\n", PcdGet32 (PcdCpuToLocalHbmAccessBandwidth)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-remotecpu-accessbandwidth", NULL);
    if (Property != NULL) {
      CpuToRemoteCpuAccessBandwidth = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToRemoteCpuAccessBandwidth, CpuToRemoteCpuAccessBandwidth);
      DEBUG ((EFI_D_INFO, "Cpu To Remote Cpu Access Bandwidth = 0x%X\n", PcdGet32 (PcdCpuToRemoteCpuAccessBandwidth)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To Remote Cpu Access Bandwidth not found, using 0x%X\n", PcdGet32 (PcdCpuToRemoteCpuAccessBandwidth)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-remotehbm-accessbandwidth", NULL);
    if (Property != NULL) {
      CpuToRemoteHbmAccessBandwidth = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToRemoteHbmAccessBandwidth, CpuToRemoteHbmAccessBandwidth);
      DEBUG ((EFI_D_INFO, "Cpu To Remote Hbm Access Bandwidth = 0x%X\n", PcdGet32 (PcdCpuToRemoteHbmAccessBandwidth)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To Remote Hbm Access Bandwidth not found, using 0x%X\n", PcdGet32 (PcdCpuToRemoteHbmAccessBandwidth)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-localhbm-accessbandwidth", NULL);
    if (Property != NULL) {
      GpuToLocalHbmAccessBandwidth = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToLocalHbmAccessBandwidth, GpuToLocalHbmAccessBandwidth);
      DEBUG ((EFI_D_INFO, "Gpu To Local HBM Access Bandwidth = 0x%X\n", PcdGet32 (PcdGpuToLocalHbmAccessBandwidth)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Local HBM Access Bandwidth not found, using 0x%X\n", PcdGet32 (PcdGpuToLocalHbmAccessBandwidth)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-remotehbm-accessbandwidth", NULL);
    if (Property != NULL) {
      GpuToRemoteHbmAccessBandwidth = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToRemoteHbmAccessBandwidth, GpuToRemoteHbmAccessBandwidth);
      DEBUG ((EFI_D_INFO, "Gpu To Remote HBM Access Bandwidth = 0x%X\n", PcdGet32 (PcdGpuToRemoteHbmAccessBandwidth)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Remote HBM Access Bandwidth not found, using 0x%X\n", PcdGet32 (PcdGpuToRemoteHbmAccessBandwidth)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-localcpu-accessbandwidth", NULL);
    if (Property != NULL) {
      GpuToLocalCpuAccessBandwidth = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToLocalCpuAccessBandwidth, GpuToLocalCpuAccessBandwidth);
      DEBUG ((EFI_D_INFO, "Gpu To Local Cpu Access Bandwidth = 0x%X\n", PcdGet32 (PcdGpuToLocalCpuAccessBandwidth)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Local Cpu Access Bandwidth not found, using 0x%X\n", PcdGet32 (PcdGpuToLocalCpuAccessBandwidth)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-remotecpu-accessbandwidth", NULL);
    if (Property != NULL) {
      GpuToRemoteCpuAccessBandwidth = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToRemoteCpuAccessBandwidth, GpuToRemoteCpuAccessBandwidth);
      DEBUG ((EFI_D_INFO, "Gpu To Remote Cpu Access Bandwidth = 0x%X\n", PcdGet32 (PcdGpuToRemoteCpuAccessBandwidth)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Remote Cpu Access Bandwidth not found, using 0x%X\n", PcdGet32 (PcdGpuToRemoteCpuAccessBandwidth)));
    }
  }
}

/**
  Setup PCDs for CPU and GPU domain distance info based on DT
**/
STATIC
VOID
EFIAPI
SetCpuGpuDistanceInfoPcdsFromDtb (
  IN VOID  *Dtb
  )
{
  CONST UINT32  *Property;
  UINT32        CpuToRemoteCpuDistance;
  UINT32        GpuToRemoteGpuDistance;
  UINT32        CpuToLocalHbmDistance;
  UINT32        CpuToRemoteHbmDistance;
  UINT32        HbmToLocalCpuDistance;
  UINT32        HbmToRemoteCpuDistance;
  UINT32        HbmToLocalGpuDistance;
  UINT32        HbmToRemoteGpuDistance;
  UINT32        GpuToLocalHbmDistance;
  UINT32        GpuToRemoteHbmDistance;
  INTN          AcpiNode;

  AcpiNode = fdt_path_offset (Dtb, "/firmware/acpi");
  if (AcpiNode >= 0) {
    // Obtain Distance info
    Property = fdt_getprop (Dtb, AcpiNode, "cpu-distance-remotecpu", NULL);
    if (Property != NULL) {
      CpuToRemoteCpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToRemoteCpuDistance, CpuToRemoteCpuDistance);
      DEBUG ((EFI_D_INFO, "Cpu To Remote Cpu Distance = 0x%X\n", PcdGet32 (PcdCpuToRemoteCpuDistance)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To Remote Cpu Distance not found, using 0x%X\n", PcdGet32 (PcdCpuToRemoteCpuDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-distance-remotegpu", NULL);
    if (Property != NULL) {
      GpuToRemoteGpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToRemoteGpuDistance, GpuToRemoteGpuDistance);
      DEBUG ((EFI_D_INFO, "Gpu To Remote Gpu Distance = 0x%X\n", PcdGet32 (PcdGpuToRemoteGpuDistance)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Remote Gpu Distance not found, using 0x%X\n", PcdGet32 (PcdGpuToRemoteGpuDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-distance-localhbm", NULL);
    if (Property != NULL) {
      CpuToLocalHbmDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToLocalHbmDistance, CpuToLocalHbmDistance);
      DEBUG ((EFI_D_INFO, "Cpu To Local Hbm Distance = 0x%X\n", PcdGet32 (PcdCpuToLocalHbmDistance)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To Local Hbm Distance not found, using 0x%X\n", PcdGet32 (PcdCpuToLocalHbmDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "cpu-distance-remotehbm", NULL);
    if (Property != NULL) {
      CpuToRemoteHbmDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdCpuToRemoteHbmDistance, CpuToRemoteHbmDistance);
      DEBUG ((EFI_D_INFO, "Cpu To Other Hbm Distance = 0x%X\n", PcdGet32 (PcdCpuToRemoteHbmDistance)));
    } else {
      DEBUG ((DEBUG_INFO, "Cpu To Other Hbm Distance not found, using 0x%X\n", PcdGet32 (PcdCpuToRemoteHbmDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "hbm-distance-localcpu", NULL);
    if (Property != NULL) {
      HbmToLocalCpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdHbmToLocalCpuDistance, HbmToLocalCpuDistance);
      DEBUG ((EFI_D_INFO, "Local Hbm To Cpu Distance = 0x%X\n", PcdGet32 (PcdHbmToLocalCpuDistance)));
    } else {
      DEBUG ((DEBUG_INFO, "Local Hbm To Cpu Distance not found, using 0x%X\n", PcdGet32 (PcdHbmToLocalCpuDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "hbm-distance-remotecpu", NULL);
    if (Property != NULL) {
      HbmToRemoteCpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdHbmToRemoteCpuDistance, HbmToRemoteCpuDistance);
      DEBUG ((EFI_D_INFO, "Remote Hbm To Cpu Distance = 0x%X\n", PcdGet32 (PcdHbmToRemoteCpuDistance)));
    } else {
      DEBUG ((DEBUG_INFO, "Remote Hbm To Cpu Distance not found, using 0x%X\n", PcdGet32 (PcdHbmToRemoteCpuDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "hbm-distance-localcpu", NULL);
    if (Property != NULL) {
      HbmToLocalCpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdHbmToLocalCpuDistance, HbmToLocalCpuDistance);
      DEBUG ((EFI_D_INFO, "Hbm To Local Cpu Distance = 0x%X\n", PcdGet32 (PcdHbmToLocalCpuDistance)));
    } else {
      DEBUG ((DEBUG_INFO, "Hbm To Local Cpu Distance not found, using 0x%X\n", PcdGet32 (PcdHbmToLocalCpuDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "hbm-distance-remotecpu", NULL);
    if (Property != NULL) {
      HbmToRemoteCpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdHbmToRemoteCpuDistance, HbmToRemoteCpuDistance);
      DEBUG ((EFI_D_INFO, "Hbm To Remote Cpu Distance = 0x%X\n", PcdGet32 (PcdHbmToRemoteCpuDistance)));
    } else {
      DEBUG ((DEBUG_INFO, "Hbm To Remote Cpu Distance not found, using 0x%X\n", PcdGet32 (PcdHbmToRemoteCpuDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-distance-localhbm", NULL);
    if (Property != NULL) {
      GpuToLocalHbmDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToLocalHbmDistance, GpuToLocalHbmDistance);
      DEBUG ((EFI_D_INFO, "Gpu To Local Hbm Distance = 0x%X\n", PcdGet32 (PcdGpuToLocalHbmDistance)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Local Hbm Distance not found, using 0x%X\n", PcdGet32 (PcdGpuToLocalHbmDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "gpu-distance-remotehbm", NULL);
    if (Property != NULL) {
      GpuToRemoteHbmDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdGpuToRemoteHbmDistance, GpuToRemoteHbmDistance);
      DEBUG ((EFI_D_INFO, "Gpu To Other Hbm Distance = 0x%X\n", PcdGet32 (PcdGpuToRemoteHbmDistance)));
    } else {
      DEBUG ((DEBUG_INFO, "Gpu To Other Hbm Distance not found, using 0x%X\n", PcdGet32 (PcdGpuToRemoteHbmDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "hbm-distance-localgpu", NULL);
    if (Property != NULL) {
      HbmToLocalGpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdHbmToLocalGpuDistance, HbmToLocalGpuDistance);
      DEBUG ((EFI_D_INFO, "Local Hbm To Gpu Distance = 0x%X\n", PcdGet32 (PcdHbmToLocalGpuDistance)));
    } else {
      DEBUG ((DEBUG_INFO, "Local Hbm To Gpu Distance not found, using 0x%X\n", PcdGet32 (PcdHbmToLocalGpuDistance)));
    }

    Property = fdt_getprop (Dtb, AcpiNode, "hbm-distance-remotegpu", NULL);
    if (Property != NULL) {
      HbmToRemoteGpuDistance = SwapBytes32 (Property[0]);
      PcdSet32S (PcdHbmToRemoteGpuDistance, HbmToRemoteGpuDistance);
      DEBUG ((EFI_D_INFO, "Remote Hbm To Gpu Distance = 0x%X\n", PcdGet32 (PcdHbmToRemoteGpuDistance)));
    } else {
      DEBUG ((DEBUG_INFO, "Remote Hbm To Gpu Distance not found, using 0x%X\n", PcdGet32 (PcdHbmToRemoteGpuDistance)));
    }
  }
}

/**
  Set up PCDs for multiple Platforms based on DT info
**/
STATIC
VOID
EFIAPI
SetGicInfoPcdsFromDtb (
  IN UINTN  ChipID
  )
{
  UINT32                            NumGicControllers;
  UINT32                            GicHandle;
  TEGRA_GIC_INFO                    *GicInfo;
  EFI_STATUS                        Status;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterData;
  UINT32                            RegisterSize;

  GicHandle    = 0;
  Status       = EFI_SUCCESS;
  RegisterData = NULL;
  GicInfo      = NULL;

  GicInfo = (TEGRA_GIC_INFO *)AllocatePool (sizeof (TEGRA_GIC_INFO));
  if (GicInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  if (!GetGicInfo (GicInfo)) {
    Status = EFI_D_ERROR;
    goto Exit;
  }

  // To set PCDs, begin with a single GIC controller in the DT
  NumGicControllers = 1;

  // Obtain Gic Handle Info
  Status = GetMatchingEnabledDeviceTreeNodes (GicInfo->GicCompatString, &GicHandle, &NumGicControllers);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "No GIC controllers found %r\r\n", Status));
    goto Exit;
  }

  // Obtain Register Info using the Gic Handle
  RegisterSize = 0;
  Status       = GetDeviceTreeRegisters (GicHandle, RegisterData, &RegisterSize);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    if (RegisterData != NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    RegisterData = (NVIDIA_DEVICE_TREE_REGISTER_DATA *)AllocatePool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * RegisterSize);
    if (RegisterData == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    Status = GetDeviceTreeRegisters (GicHandle, RegisterData, &RegisterSize);
    if (EFI_ERROR (Status)) {
      goto Exit;
    }
  } else if (EFI_ERROR (Status)) {
    goto Exit;
  }

  if (RegisterData == NULL) {
    ASSERT (FALSE);
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  // Set Pcd values by looping through the RegisterSize for each platform

  if (ChipID == T194_CHIP_ID) {
    // RegisterData[0] has Gic Distributor Base and Size
    PcdSet64S (PcdGicDistributorBase, RegisterData[0].BaseAddress);

    // RegisterData[1] has Interrupt Interface Base and Size
    PcdSet64S (PcdGicInterruptInterfaceBase, RegisterData[1].BaseAddress);

    DEBUG ((
      EFI_D_INFO,
      "Found GIC distributor and Interrupt Interface Base@ 0x%Lx (0x%Lx)\n",
      PcdGet64 (PcdGicDistributorBase),
      PcdGet64 (PcdGicInterruptInterfaceBase)
      ));
  } else {
    // RegisterData[0] has Gic Distributor Base and Size
    PcdSet64S (PcdGicDistributorBase, RegisterData[0].BaseAddress);

    // RegisterData[1] has GIC Redistributor Base and Size
    PcdSet64S (PcdGicRedistributorsBase, RegisterData[1].BaseAddress);

    // RegisterData[2] has GicH Base and Size
    // RegisterData[3] has GicV Base and Size

    DEBUG ((
      EFI_D_INFO,
      "Found GIC distributor and (re)distributor Base @ 0x%Lx (0x%Lx)\n",
      PcdGet64 (PcdGicDistributorBase),
      PcdGet64 (PcdGicRedistributorsBase)
      ));
  }

Exit:
  if (RegisterData != NULL) {
    FreePool (RegisterData);
    RegisterData = NULL;
  }

  if (GicInfo != NULL) {
    FreePool (GicInfo);
    GicInfo = NULL;
  }

  return;
}

/**
  Runtime Configuration Of Tegra Platform.
**/
EFI_STATUS
EFIAPI
TegraPlatformInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                    Status;
  UINTN                         ChipID;
  TEGRA_PLATFORM_TYPE           PlatformType;
  VOID                          *DtbBase;
  UINTN                         DtbSize;
  CONST VOID                    *Property;
  INT32                         Length;
  BOOLEAN                       T234SkuSet;
  UINTN                         EmmcMagic;
  BOOLEAN                       EmulatedVariablesUsed;
  INTN                          UefiNode;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;

  EmulatedVariablesUsed = FALSE;

  ChipID = TegraGetChipID ();
  DEBUG ((DEBUG_INFO, "%a: Tegra Chip ID:  0x%x\n", __FUNCTION__, ChipID));

  PlatformType = TegraGetPlatform ();
  Status       = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    if (ChipID == T194_CHIP_ID) {
      LibPcdSetSku (T194_SKU);
    } else if (ChipID == T234_CHIP_ID) {
      T234SkuSet = FALSE;
      Property   = fdt_getprop (DtbBase, 0, "model", &Length);
      if ((Property != NULL) && (Length != 0)) {
        if (AsciiStrStr (Property, "SLT") != NULL) {
          LibPcdSetSku (T234SLT_SKU);
          T234SkuSet = TRUE;
        }
      }

      if (T234SkuSet == FALSE) {
        LibPcdSetSku (T234_SKU);
      }
    } else if (ChipID == TH500_CHIP_ID) {
      LibPcdSetSku (TH500_SKU);
    }

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &ImageHandle,
                    &gNVIDIAIsSiliconDeviceGuid,
                    NULL,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error installing gNVIDIAIsSiliconDeviceGuid\n", __FUNCTION__));
    }
  } else {
    if (ChipID == T234_CHIP_ID) {
      LibPcdSetSku (T234_PRESIL_SKU);
    } else if (ChipID == TH500_CHIP_ID) {
      LibPcdSetSku (TH500_PRESIL_SKU);
    }

    // Override boot timeout for pre-si platforms
    EmmcMagic = *((UINTN *)(TegraGetSystemMemoryBaseAddress (ChipID) + SYSIMG_EMMC_MAGIC_OFFSET));
    if ((EmmcMagic != SYSIMG_EMMC_MAGIC) && (EmmcMagic == SYSIMG_DEFAULT_MAGIC)) {
      EmulatedVariablesUsed = TRUE;
    }

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &ImageHandle,
                    &gNVIDIAIsPresiliconDeviceGuid,
                    NULL,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error installing gNVIDIAIsPresiliconDeviceGuid\n", __FUNCTION__));
    }
  }

  /*TODO: Retaining above logic for backward compatibility. Remove once all DTBs are updated.*/
  UefiNode = fdt_path_offset (DtbBase, "/firmware/uefi");
  if (UefiNode >= 0) {
    if (NULL != fdt_get_property (DtbBase, UefiNode, "use-emulated-variables", NULL)) {
      DEBUG ((DEBUG_ERROR, "Platform Override To Use Emulated Variable Store\n"));
      EmulatedVariablesUsed = TRUE;
    }
  }

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  } else {
    DEBUG ((DEBUG_ERROR, "Failed to get PlatformResourceInfo\n"));
    return EFI_NOT_FOUND;
  }

  if ((PlatformResourceInfo->BootType == TegrablBootRcm) ||
      (PcdGetBool (PcdEmuVariableNvModeEnable) == TRUE))
  {
    EmulatedVariablesUsed = TRUE;
  }

  if (EmulatedVariablesUsed) {
    // Enable emulated variable NV mode in variable driver when ram loading images and emmc
    // is not enabled.
    Status = UseEmulatedVariableStore (ImageHandle);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (PlatformType == TEGRA_PLATFORM_SILICON) {
      DEBUG ((DEBUG_ERROR, "Do not use STMM as emulated variables are being used.\n"));
      PcdSetBoolS (PcdTegraStmmEnabled, FALSE);
    }
  }

  // Set Pcds
  SetCpuInfoPcdsFromDtb ();
  SetGicInfoPcdsFromDtb (ChipID);
  SetPhysicalPresencePcd ();

  Status = FloorSweepDtb (DtbBase);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "DTB floorsweeping failed.\n"));
    ASSERT (FALSE);
    return Status;
  }

  SetCpuGpuDistanceInfoPcdsFromDtb (DtbBase);
  SetBandwidthLatencyInfoPcdsFromdtb (DtbBase);

  if (ChipID == TH500_CHIP_ID) {
    SetOemTableIdPcdForTh500 ();
  }

  return EFI_SUCCESS;
}
