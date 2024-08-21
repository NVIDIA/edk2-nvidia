/** @file

  DW EQoS device tree binding driver

  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DT_ACPI_MAC_UDPATE_H__
#define __DT_ACPI_MAC_UDPATE_H__

#include "DwEqosSnpDxe.h"
#include "DtAcpiMacUpdate.h"

#include <Library/UefiLib.h>

#define NET_ETHER_ADDR_LEN_DS  18
#define BYTE(data, pos)  ((data >> (pos * 8)) & 0xFF)

/**
  Callback that gets invoked to update mac address in OS handoff (ACPI)

  This function should be called each time the mac address is changed and
  if the acpi tables are updated.

  @param[in] Context                    Context (SIMPLE_NETWORK_DRIVER *)

**/
VOID
UpdateACPIMacAddress (
  IN EFI_EVENT  Event,
  IN  VOID      *Context
  );

#endif // __DT_ACPI_MAC_UDPATE_H__
