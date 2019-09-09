/** @file
  DW EMAC SNP DXE driver

  SPDX-License-Identifier: BSD-2-Clause-Patent

  The original software modules are licensed as follows:

  Copyright (c) 2012 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NetLib.h>
#include <Library/UefiLib.h>
#include <libfdt.h>

#include "DwEqosSnpDxe.h"
#include "DtAcpiMacUpdate.h"

/**
  Callback that gets invoked to update mac address in OS handoff (DT/ACPI)

  This function should be called each time the mac address is changed and
  if the acpi/dt tables are updated.

  @param[in] Context                    Context (SIMPLE_NETWORK_DRIVER *)

**/
VOID
UpdateDTACPIMacAddress (
  IN EFI_EVENT Event,
  IN  VOID *Context
  )
{
  EFI_STATUS Status;
  SIMPLE_NETWORK_DRIVER *Snp = (SIMPLE_NETWORK_DRIVER *)Context;
  VOID                  *DtBase;

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &DtBase);
  if (!EFI_ERROR (Status)) {
    INT32 NodeOffset;
    INT32 DtStatus;

    NodeOffset = fdt_path_offset (DtBase, Snp->DeviceTreePath);
    if (NodeOffset < 0) {
      DEBUG ((DEBUG_ERROR, "Failed to get node %a in kernel device tree\r\n", Snp->DeviceTreePath));
      return;
    }
    DtStatus = fdt_setprop (DtBase, NodeOffset, "mac-address", Snp->SnpMode.CurrentAddress.Addr, NET_ETHER_ADDR_LEN);
    if (DtStatus == -FDT_ERR_NOSPACE) {
      VOID *NewDt;
      NewDt = AllocatePool (fdt_totalsize (DtBase) + SIZE_4KB);
      if (NewDt == NULL) {
        DEBUG ((DEBUG_ERROR, "Failed to increase size of Dt\r\n"));
        return;
      }
      DtStatus = fdt_open_into (DtBase, NewDt, fdt_totalsize (DtBase) + SIZE_4KB);
      if (DtStatus != 0) {
        DEBUG ((DEBUG_ERROR, "Failed to move kernel device tree %a\r\n", fdt_strerror(DtStatus)));;
        return;
      }
      NodeOffset = fdt_path_offset (NewDt, Snp->DeviceTreePath);
      if (NodeOffset < 0) {
        DEBUG ((DEBUG_ERROR, "Failed to get node %a in kernel device tree\r\n", Snp->DeviceTreePath));
        return;
      }
      DtStatus = fdt_setprop (NewDt, NodeOffset, "mac-address", Snp->SnpMode.CurrentAddress.Addr, NET_ETHER_ADDR_LEN);
      if (DtStatus != 0) {
        DEBUG ((DEBUG_ERROR, "Failed to set mac-address in kernel device tree %a\r\n", fdt_strerror(DtStatus)));;
        return;
      }
      Status = gBS->InstallConfigurationTable (&gFdtTableGuid, NewDt);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to update configuration table\r\n"));
        return;
      }
    } else if (DtStatus != 0) {
      DEBUG ((DEBUG_ERROR, "Failed to set mac-address in kernel device tree %a\r\n", fdt_strerror(DtStatus)));;
      return;
    }
  }
}
