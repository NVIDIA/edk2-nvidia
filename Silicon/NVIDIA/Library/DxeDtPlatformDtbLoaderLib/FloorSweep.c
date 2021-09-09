/** @file
*
*  Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
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
*  Portions provided under the following terms:
*  Copyright (c) 2018-2019 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2018-2019 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/HobLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/PcdLib.h>
#include <Library/FloorSweepingLib.h>
#include <libfdt.h>
#include "FloorSweepPrivate.h"

EFI_STATUS
UpdateCpuFloorsweepingConfig (
  IN VOID *Dtb
  )
{
  UINTN Cpu;
  UINT32 Cluster;
  UINT64 Mpidr;
  INT32 CpuMapOffset;
  INT32 FdtErr;
  UINT64 Tmp64;
  UINT32 Tmp32;
  CHAR8  CpuNodeStr[] = "cpu@ffffffff";
  CHAR8  ClusterNodeStr[] = "cluster10";
  UINT32 AddressCells;
  INT32 NodeOffset;
  INT32 PrevNodeOffset;
  INT32 ParentOffset = 0;

  if (!PcdGetBool (PcdFloorsweepCpus)) {
    return EFI_SUCCESS;
  }

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
  PrevNodeOffset = 0;
  for (NodeOffset = fdt_first_subnode(Dtb, ParentOffset);
       NodeOffset > 0;
       NodeOffset = fdt_next_subnode(Dtb, PrevNodeOffset)) {
    CONST VOID  *Property;
    INT32       Length;
    EFI_STATUS  Status;
    UINTN       DtCpuId;
    CONST CHAR8 *DtCpuFormat;

    Property = fdt_getprop(Dtb, NodeOffset, "device_type", &Length);
    if ((Property == NULL) || (AsciiStrCmp(Property, "cpu") != 0)) {
      PrevNodeOffset = NodeOffset;
      continue;
    }

    // retrieve mpidr for this cpu node
    Property = fdt_getprop (Dtb, NodeOffset, "reg", &Length);
    if ((Property == NULL) || ((Length != sizeof (Tmp64)) && (Length != sizeof (Tmp32)))) {
      DEBUG ((DEBUG_ERROR, "Failed to get MPIDR for /cpus/%a, len=%u\n",
              fdt_get_name (Dtb, NodeOffset, NULL), Length));
      return EFI_DEVICE_ERROR;
    }
    if (Length == sizeof (Tmp64)) {
      Tmp64 = *(CONST UINT64 *)Property;
      Mpidr = fdt64_to_cpu (Tmp64);
    } else {
      Tmp32 = *(CONST UINT32 *)Property;
      Mpidr = fdt32_to_cpu (Tmp32);
    }

    Status = CheckAndRemapCpu (Cpu, &Mpidr, &DtCpuFormat, &DtCpuId);
    if (!EFI_ERROR (Status)) {
      AsciiSPrint (CpuNodeStr, sizeof (CpuNodeStr), DtCpuFormat, DtCpuId);
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

      DEBUG ((DEBUG_INFO, "Enabled %a, index=%u, (mpidr: 0x%llx) node in FDT\r\n",
              CpuNodeStr, Cpu, Mpidr));
      PrevNodeOffset = NodeOffset;
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

  Cluster = 0;
  while (TRUE) {
    AsciiSPrint (ClusterNodeStr, sizeof (ClusterNodeStr),"cluster%u", Cluster);
    NodeOffset = fdt_subnode_offset(Dtb, CpuMapOffset, ClusterNodeStr);
    if (NodeOffset >= 0) {
      if (!ClusterIsPresent (Cluster)) {
        FdtErr = fdt_del_node(Dtb, NodeOffset);
        if (FdtErr < 0) {
          DEBUG ((DEBUG_ERROR, "Failed to delete /cpus/cpu-map/%a node: %a\r\n",
                  ClusterNodeStr, fdt_strerror(FdtErr)));
          return EFI_DEVICE_ERROR;
        }
        DEBUG ((DEBUG_INFO, "Deleted cluster%u node in FDT\r\n", Cluster));
      } else {
        INT32       ClusterCpuNodeOffset;
        INT32       PrevClusterCpuNodeOffset;
        CONST VOID  *Property;
        CONST CHAR8 *NodeName;
        PrevClusterCpuNodeOffset = 0;
        fdt_for_each_subnode (ClusterCpuNodeOffset, Dtb, NodeOffset) {
          Property = fdt_getprop(Dtb, ClusterCpuNodeOffset, "cpu", NULL);
          if (Property != NULL) {
            if (fdt_node_offset_by_phandle (Dtb, fdt32_to_cpu(*(UINT32 *)Property)) < 0) {
              NodeName = fdt_get_name (Dtb, ClusterCpuNodeOffset, NULL);
              FdtErr = fdt_del_node(Dtb, ClusterCpuNodeOffset);
              if (FdtErr < 0) {
                DEBUG ((DEBUG_ERROR, "Failed to delete /cpus/cpu-map/%a/%a node: %a\r\n",
                        ClusterNodeStr, NodeName, fdt_strerror(FdtErr)));
                return EFI_DEVICE_ERROR;
              }
              ClusterCpuNodeOffset = PrevClusterCpuNodeOffset;
            }
          }
          PrevClusterCpuNodeOffset = ClusterCpuNodeOffset;
        }
      }
      Cluster++;
    } else {
      break;
    }
  }

  return EFI_SUCCESS;
}

