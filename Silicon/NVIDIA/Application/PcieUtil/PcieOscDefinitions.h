/** @file
  Definitions for PCIe OSC Control bits.

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef PCIE_OSC_DEFINITIONS_H_
#define PCIE_OSC_DEFINITIONS_H_

#define BIT(x)  (1 << (x))

// OSC Control Bit definitions - copied from NVIDIA's PcieControllerPrivate.h
#define PCIE_FW_OSC_CTRL_PCIE_NATIVE_HP      BIT(0)
#define PCIE_FW_OSC_CTRL_SHPC_NATIVE_HP      BIT(1)
#define PCIE_FW_OSC_CTRL_PCIE_NATIVE_PME     BIT(2)
#define PCIE_FW_OSC_CTRL_PCIE_AER            BIT(3)
#define PCIE_FW_OSC_CTRL_PCIE_CAP_STRUCTURE  BIT(4)
#define PCIE_FW_OSC_CTRL_LTR                 BIT(5)
#define PCIE_FW_OSC_CTRL_RSVD                BIT(6)
#define PCIE_FW_OSC_CTRL_PCIE_DPC            BIT(7)
#define PCIE_FW_OSC_CTRL_PCIE_CMPL_TO        BIT(8)
#define PCIE_FW_OSC_CTRL_PCIE_SFI            BIT(9)

#endif // PCIE_OSC_DEFINITIONS_H_
