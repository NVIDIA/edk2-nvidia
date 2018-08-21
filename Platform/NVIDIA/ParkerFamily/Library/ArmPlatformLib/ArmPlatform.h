/** @file
*
*  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
*  Copyright (c) 2013-2014, ARM Limited. All rights reserved.
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

#ifndef __ARM_GALEN_H__
#define __ARM_GALEN_H__

/***********************************************************************************
// Platform Memory Map
************************************************************************************/

// Motherboard Peripheral and On-chip peripheral
#define ARM_VE_BOARD_PERIPH_BASE              0x1C010000

// On-Chip Peripherals
#define ARM_GALEN_PERIPHERALS_BASE             0
#define ARM_GALEN_PERIPHERALS_SZ               SIZE_1GB

// PCIe MSI address window
#define ARM_GALEN_GIV2M_MSI_BASE               0x2c1c0000
#define ARM_GALEN_GIV2M_MSI_SZ                 SIZE_256KB

// PCIe MSI to SPI mapping range
#define ARM_GALEN_GIV2M_MSI_SPI_BASE           224
#define ARM_GALEN_GIV2M_MSI_SPI_COUNT          127 //TRM says last SPI is 351, 351-224=127

// SOC peripherals (HDLCD, UART, I2C, I2S, USB, SMC-PL354, etc)
#define ARM_GALEN_SOC_PERIPHERALS_BASE         0x7FF50000
#define ARM_GALEN_SOC_PERIPHERALS_SZ           (SIZE_64KB * 9)

// 6GB of DRAM from the 64bit address space
#define ARM_GALEN_EXTRA_SYSTEM_MEMORY_BASE     0x0880000000
#define ARM_GALEN_EXTRA_SYSTEM_MEMORY_SZ       (SIZE_2GB + SIZE_4GB)

//
// ACPI table information used to initialize tables.
//
#define EFI_ACPI_NVIDIA_OEM_ID           'N','V','I','D','I','A'   // OEMID 6 bytes long
#define EFI_ACPI_NVIDIA_OEM_TABLE_ID     SIGNATURE_64('N','V','-','G','A','L','E','N') // OEM table id 8 bytes long
#define EFI_ACPI_NVIDIA_OEM_REVISION     0x20140727
#define EFI_ACPI_NVIDIA_CREATOR_ID       SIGNATURE_32('N','V','D','A')
#define EFI_ACPI_NVIDIA_CREATOR_REVISION 0x00000099

// A macro to initialise the common header part of EFI ACPI tables as defined by
// EFI_ACPI_DESCRIPTION_HEADER structure.
#define NVIDIA_ACPI_HEADER(Signature, Type, Revision) {              \
    Signature,                      /* UINT32  Signature */       \
    sizeof (Type),                  /* UINT32  Length */          \
    Revision,                       /* UINT8   Revision */        \
    0,                              /* UINT8   Checksum */        \
    { EFI_ACPI_NVIDIA_OEM_ID },        /* UINT8   OemId[6] */        \
    EFI_ACPI_NVIDIA_OEM_TABLE_ID,      /* UINT64  OemTableId */      \
    EFI_ACPI_NVIDIA_OEM_REVISION,      /* UINT32  OemRevision */     \
    EFI_ACPI_NVIDIA_CREATOR_ID,        /* UINT32  CreatorId */       \
    EFI_ACPI_NVIDIA_CREATOR_REVISION   /* UINT32  CreatorRevision */ \
  }

#define GALEN_WATCHDOG_COUNT  2

// Define if the exported ACPI Tables are based on ACPI 5.0 spec or latest
//#define ARM_GALEN_ACPI_5_0

//
// Address of the system registers that contain the MAC address
// assigned to the PCI Gigabyte Ethernet device.
//

#define ARM_GALEN_SYS_PCIGBE_L  (ARM_VE_BOARD_PERIPH_BASE + 0x74)
#define ARM_GALEN_SYS_PCIGBE_H  (ARM_VE_BOARD_PERIPH_BASE + 0x78)

#endif
