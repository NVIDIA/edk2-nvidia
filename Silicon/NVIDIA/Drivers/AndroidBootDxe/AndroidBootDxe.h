/** @file

  Android Boot Loader Driver's private data structure and interfaces declaration

  Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
  Copyright (c) 2013-2014, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2017, Linaro.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/

#ifndef __EFI_ANDROID_BOOT_DXE_H__
#define __EFI_ANDROID_BOOT_DXE_H__

#include <Uefi.h>
#include <libfdt.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/AndroidBootImgLib.h>
#include <Library/TegraPlatformInfoLib.h>

#include <Guid/LinuxEfiInitrdMedia.h>

#include <Protocol/DriverBinding.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
#include <Protocol/LoadFile.h>
#include <Protocol/LoadFile2.h>


#define FDT_ADDITIONAL_ENTRIES_SIZE 0x400
#define KERNEL_OFFSET               0x80000
#define ANDROID_BOOT_SIGNATURE      SIGNATURE_64 ('A','N','D','R','O','I','D','!')

// Android Boot Data structure
typedef struct {
  UINT32              Offset;
  UINT32              KernelSize;
  UINT32              RamdiskSize;
  UINT32              PageSize;
} ANDROID_BOOT_DATA;

// Private data structure
typedef struct {
  UINT64                            Signature;

  EFI_LOAD_FILE_PROTOCOL            LoadFile;
  EFI_PARTITION_INFO_PROTOCOL       *PartitionInfo;
  EFI_BLOCK_IO_PROTOCOL             *BlockIo;
  EFI_DISK_IO_PROTOCOL              *DiskIo;
  EFI_DEVICE_PATH_PROTOCOL          *ParentDevicePath;
  EFI_DEVICE_PATH_PROTOCOL          *AndroidBootDevicePath;
  CHAR16                            *KernelArgs;

  UINT32                            Id;
  BOOLEAN                           ProtocolsInstalled;

  EFI_HANDLE                        ControllerHandle;
  EFI_HANDLE                        DriverBindingHandle;
  EFI_HANDLE                        AndroidBootHandle;

} ANDROID_BOOT_PRIVATE_DATA;

//
// Device path for the handle that incorporates our ramload and initrd load file
// instance.
//
#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH       VenHwNode;
  EFI_DEVICE_PATH_PROTOCOL EndNode;
} SINGLE_VENHW_NODE_DEVPATH;

typedef struct {
  VENDOR_DEVICE_PATH       VenMediaNode;
  EFI_DEVICE_PATH_PROTOCOL EndNode;
} INITRD_DEVICE_PATH;
#pragma pack()

#define ANDROID_BOOT_PRIVATE_DATA_FROM_ID(a)        CR (a, ANDROID_BOOT_PRIVATE_DATA, Id, ANDROID_BOOT_SIGNATURE)
#define ANDROID_BOOT_PRIVATE_DATA_FROM_LOADFILE(a)  CR (a, ANDROID_BOOT_PRIVATE_DATA, LoadFile, ANDROID_BOOT_SIGNATURE)


#endif // __EFI_ANDROID_BOOT_DXE_H__
