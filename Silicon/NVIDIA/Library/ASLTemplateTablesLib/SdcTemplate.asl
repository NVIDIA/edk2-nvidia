/** @file
 * SSDT for SDHCI
 *
 * Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 */

DefinitionBlock("SdcTemplate.aml", "SSDT", 2, "NVIDIA", "SDCTEMP", 0x00000001) {
  Device(SDCT) {
    Name (_HID, EISAID("PNP0D40")) // SDA Standard Compliant SD Host Controller
    Name (_UID, 0)
    Name (_CCA, ZERO)

    Name(_CRS, ResourceTemplate () {
      Memory32Fixed(ReadWrite, 0xFFFFFFFF, 0xFFFFFFFF, REG0)
      Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive, , , INT0) { 0xFF }
    })
  }
}
