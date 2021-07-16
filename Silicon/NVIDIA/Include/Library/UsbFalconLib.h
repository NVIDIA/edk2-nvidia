/** @file

  Falcon Register Access

  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2019 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef USB_FALCON_LIB_H_
#define USB_FALCON_LIB_H_

#include <PiDxe.h>

/* Falcon firmware header */

struct tegra_xhci_fw_cfgtbl {
  UINT32 boot_loadaddr_in_imem;
  UINT32 boot_codedfi_offset;
  UINT32 boot_codetag;
  UINT32 boot_codesize;
  UINT32 phys_memaddr;
  UINT16 reqphys_memsize;
  UINT16 alloc_phys_memsize;
  UINT32 rodata_img_offset;
  UINT32 rodata_section_start;
  UINT32 rodata_section_end;
  UINT32 main_fnaddr;
  UINT32 fwimg_cksum;
  UINT32 fwimg_created_time;
  UINT32 imem_resident_start;
  UINT32 imem_resident_end;
  UINT32 idirect_start;
  UINT32 idirect_end;
  UINT32 l2_imem_start;
  UINT32 l2_imem_end;
  UINT32 version_id;
  UINT8 init_ddirect;
  UINT8 reserved[3];
  UINT32 phys_addr_log_buffer;
  UINT32 total_log_entries;
  UINT32 dequeue_ptr;
  UINT32 dummy_var[2];
  UINT32 fwimg_len;
  UINT8 magic[8];
  UINT32 ss_low_power_entry_timeout;
  UINT8 num_hsic_port;
  UINT8 ss_portmap;
  UINT8 build_log:4;
  UINT8 build_type:4;
  UINT8 padding[137]; /* Padding to make 256-bytes cfgtbl */
};

/* Falcon CSB Registers */

#define IMEM_BLOCK_SIZE                     256

#define XUSB_CSB_MEMPOOL_ILOAD_ATTR_0       0x0101a00
#define XUSB_CSB_MEMPOOL_ILOAD_BASE_LO_0    0x0101a04
#define XUSB_CSB_MEMPOOL_ILOAD_BASE_HI_0    0x0101a08
#define XUSB_CSB_MEMPOOL_L2IMEMOP_SIZE_0    0x0101a10
#define XUSB_CSB_MEMPOOL_L2IMEMOP_TRIG_0    0x0101a14
#define XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT_0  0x0101a18
#define L2IMEMOP_RESULT_VLD                 (1 << 31)
#define XUSB_CSB_MEMPOOL_APMAP_0            0x010181c
#define XUSB_CSB_MEMPOOL_IDIRECT_PC         0x0101814
#define FALCON_CPUCTL_0                     0x100
#define FALCON_BOOTVEC_0                    0x104
#define FALCON_DMACTL_0                     0x10c
#define FALCON_IMFILLRNG1_0                 0x154
#define FALCON_IMFILLCTL_0                  0x158
#define XUSB_BAR2_ARU_C11_CSBRANGE          0x9c
#define XUSB_BAR2_CSB_BASE_ADDR             0x2000


VOID
FalconSetHostCfgAddr (
  IN  UINTN Address
  );

VOID
FalconSetHostBase2Addr (
  IN  UINTN Address
  );

VOID *
FalconMapReg (
  IN  UINTN Address
  );

UINT32
FalconRead32 (
  IN  UINTN Address
  );

UINT32
FalconWrite32 (
  IN  UINTN Address,
  IN  UINT32 Value
  );

EFI_STATUS
FalconFirmwareLoad (
  IN  UINT8 *Firmware,
  IN  UINT32 FirmwareSize,
  IN  BOOLEAN LoadIfrRom
  );

#endif /* USB_FALCON_LIB_H_ */
