/** @file

  DSDT definitions

  SPDX-FileCopyrightText: Copyright (c) 2025-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef DSDT_DEFINITIONS_H__
#define DSDT_DEFINITIONS_H__

// ACPI OSC Status bits
#define OSC_STS_BIT0_RES           (1U << 0)
#define OSC_STS_FAILURE            (1U << 1)
#define OSC_STS_UNRECOGNIZED_UUID  (1U << 2)
#define OSC_STS_UNRECOGNIZED_REV   (1U << 3)
#define OSC_STS_CAPABILITY_MASKED  (1U << 4)
#define OSC_STS_MASK               (OSC_STS_BIT0_RES          |    \
                                       OSC_STS_FAILURE           | \
                                       OSC_STS_UNRECOGNIZED_UUID | \
                                       OSC_STS_UNRECOGNIZED_REV  | \
                                       OSC_STS_CAPABILITY_MASKED)

// ACPI OSC for Platform-Wide Capability
#define OSC_CAP_CPPC_SUPPORT          (1U << 5)
#define OSC_CAP_CPPC2_SUPPORT         (1U << 6)
#define OSC_CAP_PLAT_COORDINATED_LPI  (1U << 7)
#define OSC_CAP_OS_INITIATED_LPI      (1U << 8)

#endif
