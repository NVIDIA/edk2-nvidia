/** @file

  PCIe Controller Driver FDT manipulation

  Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PCIE_CONTROLLER_DT_H__
#define __PCIE_CONTROLLER_DT_H__

/**
   Parse the GIC and MSI base addresses from the given PCIe Controller FDT node.

   @param [in]  Fdt         Base of the FDT to parse
   @param [in]  NodeOffset  Node offset of the PCIE controller FDT node
   @param [out] GicBase     GIC base address
   @param [out] MsiBase     MSI base address

   @retval TRUE  Information parsed successfully
   @retval FALSE Failed to parse the information
*/
BOOLEAN
ParseGicMsiBase (
  IN  CONST VOID  *Fdt,
  IN  INT32       NodeOffset,
  OUT UINT64      *GicBase,
  OUT UINT64      *MsiBase
  );

/**
  Finds the FDT node of a specified PCIe controller.

  @param[in]  Fdt        Base of the FDT to search
  @param[in]  CtrlId     Controller number
  @param[out] NodeOffset Where to store the resulting node offset

  @retval TRUE  Node found successfully
  @retval FALSE Failed to find the node

**/
BOOLEAN
FindFdtPcieControllerNode (
  IN  CONST VOID  *Fdt,
  IN  UINT32      CtrlId,
  OUT INT32       *NodeOffset
  );

/**
   Patch the given PCIe controller node in the given Device Tree so
   that the kernel can successfully take over managing the controller
   and the attached devices without UEFI having to shut it down.

   @param [in] Fdt         Base of the Device Tree to patch.
   @param [in] NodeOffset  Offset of the PCIe controller node.

   @return TRUE   Node patched successfully.
   @return FALSE  Failed to patch the node.

**/
BOOLEAN
UpdateFdtPcieControllerNode (
  IN VOID   *Fdt,
  IN INT32  NodeOffset
  );

#endif
