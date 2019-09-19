/** @file
*
*  Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
*  Copyright (c) 2017, Linaro, Ltd. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/HobLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/PcdLib.h>
#include <libfdt.h>
#include "FloorSweepPrivate.h"

STATIC
UINT32
GetNumCores (
  VOID
  )
{
  UINT64 Data;

  WriteNvgChannelIdx(TEGRA_NVG_CHANNEL_NUM_CORES);
  Data = ReadNvgChannelData();

  return (Data & 0xF);
}

STATIC
UINT32
LogicalToMpidr (
  IN UINT32 LogicalCore
  )
{
  UINT32 NumCores;
  UINT32 Mpidr = 0;
  UINT64 Data = 0;

  NumCores = GetNumCores();
  if (LogicalCore < NumCores) {
    WriteNvgChannelIdx (TEGRA_NVG_CHANNEL_LOGICAL_TO_MPIDR);

    /* Write the logical core id */
    WriteNvgChannelData (LogicalCore);

    /* Read-back the MPIDR */
    Data = ReadNvgChannelData ();
    Mpidr = (Data & 0xFFFFFFFF);

    DEBUG ((DEBUG_INFO, "NVG: Logical CPU: %u; MPIDR: 0x%x\n", LogicalCore, Mpidr));
  } else {
    DEBUG ((DEBUG_ERROR, "Core: %u is not present\r\n", LogicalCore));
  }

  return Mpidr;
}

EFI_STATUS
UpdateCpuFloorsweepingConfig (
  IN VOID *Dtb
  )
{
  UINTN Cpu;
  UINT32 Cluster;
  UINT32 Mpidr;
  INT32 CpuMapOffset;
  INT32 FdtErr;
  UINT64 Tmp64;
  UINT32 Tmp32;
  CHAR8  CpuNodeStr[] = "cpu@ffffffff";
  CHAR8  ClusterNodeStr[] = "cluster10";
  BOOLEAN NodePresent;
  UINT32 NumCores;
  UINT32 AddressCells;

  INT32 NodeOffset;
  INT32 ParentOffset = 0;

  if (!PcdGetBool (PcdFloorsweepCpus)) {
    return EFI_SUCCESS;
  }

  NumCores = GetNumCores();

  ParentOffset = fdt_path_offset (Dtb, "/cpus");
  if (ParentOffset < 0) {
    DEBUG ((DEBUG_ERROR, "Failed to find cpus subnode\r\n"));
    return EFI_DEVICE_ERROR;
  }

  AddressCells  = fdt_address_cells (Dtb, ParentOffset);

  /* Update the correct MPIDR and enable the DT nodes of each enabled CPU;
   * disable the DT nodes of the floorswept cores.*/
  NodeOffset = 0;
  Cpu = 0;
  for (NodeOffset = fdt_first_subnode(Dtb, ParentOffset);
       NodeOffset > 0;
       NodeOffset = fdt_next_subnode(Dtb, NodeOffset)) {
    CONST VOID *Property;
    INT32      Length;

    Property = fdt_getprop(Dtb, NodeOffset, "device_type", &Length);
    if ((Property == NULL) || (AsciiStrCmp(Property, "cpu") != 0)) {
      continue;
    }

    if (Cpu < NumCores) {
      Mpidr = LogicalToMpidr(Cpu);
      Mpidr &= 0x00ffffffUL;

      AsciiSPrint (CpuNodeStr, sizeof (CpuNodeStr),"cpu@%x", Mpidr);
      FdtErr = fdt_set_name (Dtb, NodeOffset, CpuNodeStr);
      if (FdtErr < 0) {
        DEBUG ((DEBUG_ERROR, "Failed to set name to %a: %a\r\n", CpuNodeStr, fdt_strerror (FdtErr)));
        return EFI_DEVICE_ERROR;
      }

      if (AddressCells == 2) {
        Tmp64 = cpu_to_fdt64 ((UINT64)Mpidr);
        FdtErr = fdt_setprop (Dtb, NodeOffset, "reg", &Tmp64, sizeof(Tmp64));
      } else {
        Tmp32 = cpu_to_fdt32 ((UINT32)Mpidr);
        FdtErr = fdt_setprop (Dtb, NodeOffset, "reg", &Tmp32, sizeof(Tmp32));
      }
      if (FdtErr < 0) {
        DEBUG ((DEBUG_ERROR, "Failed to add MPIDR to /cpus/%a/reg: %a\r\n", CpuNodeStr, fdt_strerror(FdtErr)));
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((DEBUG_INFO, "Enabled cpu-%u (mpidr: 0x%x) node in FDT\r\n", Cpu, Mpidr));
    } else {
      FdtErr = fdt_del_node(Dtb, NodeOffset);
      if (FdtErr < 0) {
        DEBUG ((DEBUG_ERROR, "Failed to delete /cpus/cpu@%u node: %a\r\n", Cpu, fdt_strerror(FdtErr)));
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((DEBUG_INFO, "Deleted cpu-%u node in FDT\r\n", Cpu));
    }
    Cpu++;
  }

  CpuMapOffset = fdt_subnode_offset(Dtb, ParentOffset, "cpu-map");

  if (CpuMapOffset < 0) {
    DEBUG ((DEBUG_ERROR, "/cpus/cpu-map does not exist\r\n"));
    return EFI_DEVICE_ERROR;
  }

  Cluster = (NumCores + 1)/2;
  while (TRUE) {
    AsciiSPrint (ClusterNodeStr, sizeof (ClusterNodeStr),"cluster%u", Cluster);
    NodeOffset = fdt_subnode_offset(Dtb, CpuMapOffset, ClusterNodeStr);

    NodePresent = (NodeOffset >= 0);

    if (NodePresent) {
      FdtErr = fdt_del_node(Dtb, NodeOffset);
      if (FdtErr < 0) {
        DEBUG ((DEBUG_ERROR, "Failed to delete /cpus/cpu-map/%a node: %a\r\n",
              ClusterNodeStr, fdt_strerror(FdtErr)));
        return EFI_DEVICE_ERROR;
      }

      DEBUG ((DEBUG_INFO, "Deleted cluster%u node in FDT\r\n", Cluster));
      Cluster++;
    } else {
      break;
    }
  }

  return EFI_SUCCESS;
}

