/** @file

  DW EQoS device tree binding driver

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/


#ifndef __DT_ACPI_MAC_UDPATE_H__
#define __DT_ACPI_MAC_UDPATE_H__

#include "DwEqosSnpDxe.h"
#include "DtAcpiMacUpdate.h"

#include <Library/UefiLib.h>

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
  );

#endif // __DT_ACPI_MAC_UDPATE_H__
