/** @file

  PCIe Controller Driver FDT manipulation

  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>

#include <libfdt.h>

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
  IN  CONST VOID  *CONST  Fdt,
  IN  CONST INT32         NodeOffset,
  OUT UINT64      *CONST  GicBase,
  OUT UINT64      *CONST  MsiBase
  )
{
  CONST VOID  *Property;
  INT32       PropertySize;
  UINT32      MsiParentPhandle;
  INTN        MsiParentOffset;

  Property = fdt_getprop (Fdt, NodeOffset, "msi-parent", &PropertySize);
  if (Property == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: cannot retrieve property 'msi-parent': %a\r\n",
      __FUNCTION__,
      fdt_strerror (PropertySize)
      ));
    return FALSE;
  } else if (PropertySize != 2 * sizeof (UINT32)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: invalid size of property 'msi-parent': expected %u, got %d\r\n",
      __FUNCTION__,
      (UINTN)2 * sizeof (UINT32),
      (INTN)PropertySize
      ));
    return FALSE;
  }

  MsiParentPhandle = SwapBytes32 (*(UINT32 *)Property);

  MsiParentOffset = fdt_node_offset_by_phandle (Fdt, MsiParentPhandle);
  if (MsiParentOffset < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to locate GICv2m node by phandle 0x%x: %a\r\n",
      __FUNCTION__,
      (UINTN)MsiParentPhandle,
      fdt_strerror (MsiParentOffset)
      ));
    return FALSE;
  }

  if (EFI_ERROR (DeviceTreeCheckNodeSingleCompatibility ("arm,gic-v2m-frame", NodeOffset))) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: GICv2m not compatible\r\n",
      __FUNCTION__
      ));
    return FALSE;
  }

  Property = fdt_getprop (Fdt, MsiParentOffset, "reg", &PropertySize);
  if (Property == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: cannot retrieve GICv2m property 'reg': %a\r\n",
      __FUNCTION__,
      fdt_strerror (PropertySize)
      ));
    return FALSE;
  } else if (PropertySize != 4 * sizeof (UINT64)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: invalid size of GICv2m property 'reg': expected %u, got %d\r\n",
      __FUNCTION__,
      (UINTN)4 * sizeof (UINT64),
      (INTN)PropertySize
      ));
    return FALSE;
  }

  *GicBase = SwapBytes64 (*(UINT64 *)Property);
  *MsiBase = SwapBytes64 (*((UINT64 *)Property + 2));
  return TRUE;
}

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
  IN  CONST VOID   *CONST  Fdt,
  IN  CONST UINT32         CtrlId,
  OUT INT32        *CONST  NodeOffset
  )
{
  INT32       Offset;
  CONST VOID  *Property;
  INT32       PropertySize;

  Offset = -1;
  while (1) {
    Offset = fdt_node_offset_by_compatible (Fdt, Offset, "nvidia,tegra234-pcie");
    if (Offset < 0) {
      if (Offset != -FDT_ERR_NOTFOUND) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to locate node by compatible: %a\r\n",
          __FUNCTION__,
          fdt_strerror (Offset)
          ));
      }

      return FALSE;
    }

    Property = fdt_getprop (Fdt, Offset, "linux,pci-domain", &PropertySize);
    if (Property == NULL) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to retrieve controller number: %a\r\n",
        __FUNCTION__,
        fdt_strerror (PropertySize)
        ));
      return FALSE;
    } else if (PropertySize != sizeof (UINT32)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: invalid size of controller number: expected %u, got %d\r\n",
        __FUNCTION__,
        (UINTN)sizeof (UINT32),
        (INTN)PropertySize
        ));
      return FALSE;
    }

    if (CtrlId == SwapBytes32 (*(CONST UINT32 *)Property)) {
      *NodeOffset = Offset;
      return TRUE;
    }
  }
}

/**
   Update a specified regulator of the given node to be always-on.

   @param [in] Fdt         Base of the Device Tree to update.
   @param [in] NodeOffset  Offset of the node whose regulator to update.
   @param [in] RegName     Name of the regulator to update.

   @return TRUE   Regulator updated successfully.
   @return FALSE  Failed to update the regulator.

**/
STATIC
BOOLEAN
UpdateFdtRegulatorAlwaysOn (
  IN VOID        *CONST  Fdt,
  IN CONST INT32         NodeOffset,
  IN CONST CHAR8 *CONST  RegName
  )
{
  CONST UINT32  *Property;
  INT32         PropertySize;
  UINT32        RegPhandle;
  INT32         RegNodeOffset;

  Property = fdt_getprop (Fdt, NodeOffset, RegName, &PropertySize);
  if (Property == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to lookup regulator '%a' property of node at offset 0x%x: %a\r\n",
      __FUNCTION__,
      RegName,
      (UINTN)NodeOffset,
      fdt_strerror (PropertySize)
      ));
    return FALSE;
  } else if (PropertySize != sizeof (UINT32)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: invalid size of regulator '%a' property of node at offset 0x%x:"
      " expected %u bytes, got %u bytes\r\n",
      __FUNCTION__,
      RegName,
      (UINTN)NodeOffset,
      sizeof (UINT32),
      (UINTN)PropertySize
      ));
    return FALSE;
  }

  RegPhandle = SwapBytes32 (*Property);

  RegNodeOffset = fdt_node_offset_by_phandle (Fdt, RegPhandle);
  if (RegNodeOffset < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to locate regulator '%a' node by phandle 0x%x: %a\r\n",
      __FUNCTION__,
      RegName,
      (UINTN)RegPhandle,
      fdt_strerror (RegNodeOffset)
      ));
    return FALSE;
  }

  PropertySize = fdt_setprop_empty (Fdt, RegNodeOffset, "regulator-always-on");
  if (PropertySize != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to update regulator '%a' node at offset 0x%x: %a\r\n",
      __FUNCTION__,
      RegName,
      (UINTN)RegNodeOffset,
      fdt_strerror (PropertySize)
      ));
    return FALSE;
  }

  return TRUE;
}

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
  IN VOID        *CONST  Fdt,
  IN CONST INT32         NodeOffset
  )
{
  INT32  Result;
  UINTN  EcamIndex;

  CONST UINT64 (*RegProperty)[2];
  UINT64  EcamRegion[2];

  Result = fdt_setprop_string (Fdt, NodeOffset, "compatible", "pci-host-ecam-generic");
  if (Result != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to update compatible string of node at offset 0x%x: %a\r\n",
      __FUNCTION__,
      (UINTN)NodeOffset,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  Result = fdt_stringlist_search (Fdt, NodeOffset, "reg-names", "ecam");
  if (Result < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to retrieve ecam region details from node at offset 0x%x: %a\r\n",
      __FUNCTION__,
      (UINTN)NodeOffset,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  EcamIndex = (UINTN)Result;

  RegProperty = fdt_getprop (Fdt, NodeOffset, "reg", &Result);
  if (RegProperty == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to lookup property 'reg' of node at offset 0x%x: %a\r\n",
      __FUNCTION__,
      (UINTN)NodeOffset,
      fdt_strerror (Result)
      ));
    return FALSE;
  } else if ((UINTN)Result < (EcamIndex + 1) * sizeof (EcamRegion)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: invalid size of 'reg' property of node at offset 0x%x:"
      " expected at least %u bytes, got %u bytes\r\n",
      __FUNCTION__,
      (UINTN)NodeOffset,
      (EcamIndex + 1) * sizeof (EcamRegion),
      (UINTN)Result
      ));
    return FALSE;
  }

  CopyMem (EcamRegion, RegProperty[EcamIndex], sizeof (EcamRegion));

  Result = fdt_setprop_string (Fdt, NodeOffset, "reg-names", "ecam");
  if (Result != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set property 'reg-names' of node at offset 0x%x: %a\r\n",
      __FUNCTION__,
      (UINTN)NodeOffset,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  Result = fdt_setprop (Fdt, NodeOffset, "reg", EcamRegion, sizeof (EcamRegion));
  if (Result != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set property 'reg' of node at offset 0x%x: %a\r\n",
      __FUNCTION__,
      (UINTN)NodeOffset,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  Result = fdt_nop_property (Fdt, NodeOffset, "power-domains");
  if ((Result != 0) && (Result != -FDT_ERR_NOTFOUND)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to delete property 'power-domains' of node at offset 0x%x: %a\r\n",
      __FUNCTION__,
      (UINTN)NodeOffset,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  /* Disable IOMMU nodes. WARNING: This will likely cause crashes when
     the attached device attempts to perform DMA! */
  Result = fdt_nop_property (Fdt, NodeOffset, "iommus");
  if ((Result != 0) && (Result != -FDT_ERR_NOTFOUND)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to delete property 'iommus' of node at offset 0x%x: %a\r\n",
      __FUNCTION__,
      (UINTN)NodeOffset,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  Result = fdt_nop_property (Fdt, NodeOffset, "iommu-map");
  if ((Result != 0) && (Result != -FDT_ERR_NOTFOUND)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to delete property 'iommu-map' of node at offset 0x%x: %a\r\n",
      __FUNCTION__,
      (UINTN)NodeOffset,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  Result = fdt_nop_property (Fdt, NodeOffset, "iommu-map-mask");
  if ((Result != 0) && (Result != -FDT_ERR_NOTFOUND)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to delete property 'iommu-map-mask' of node at offset 0x%x: %a\r\n",
      __FUNCTION__,
      (UINTN)NodeOffset,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  return (  UpdateFdtRegulatorAlwaysOn (Fdt, NodeOffset, "vpcie3v3-supply")
         && UpdateFdtRegulatorAlwaysOn (Fdt, NodeOffset, "vpcie12v-supply"));
}
