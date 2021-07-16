/** @file
  PCI Root Bridge Configuration I/O protocol.

  PCI Root Bridge Configuration I/O protocol is used by PCI Bus Driver to perform
  PCI Configuration cycles on a PCI Root Bridge.

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.<BR>
  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
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

#ifndef __PCI_ROOT_BRIDGE_CONFIGURATION_IO_H__
#define __PCI_ROOT_BRIDGE_CONFIGURATION_IO_H__

#include <Library/BaseLib.h>

///
/// *******************************************************
/// EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH
/// *******************************************************
///
typedef enum {
  NvidiaPciWidthUint8,
  NvidiaPciWidthUint16,
  NvidiaPciWidthUint32,
  NvidiaPciWidthMaximum
} NVIDIA_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH;

///
/// *******************************************************
/// NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL_OPERATION
/// *******************************************************
///
typedef struct _NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL;

/**
  Enables a PCI driver to access PCI controller registers in the PCI root bridge memory space.

  @param  This                  A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param  Width                 Signifies the width of the memory operations.
  @param  Address               The base address of the memory operations.
  @param  Buffer                For read operations, the destination buffer to store the results. For write
                                operations, the source buffer to write data from.

  @retval EFI_SUCCESS           The data was read from or written to the PCI root bridge.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL_IO_MEM)(
  IN     NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL *This,
  IN     NVIDIA_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH         Width,
  IN     UINT64                                           Address,
  IN OUT VOID                                             *Buffer
  );

///
/// Provides the basic PCI configuration that is
/// used to abstract accesses to PCI controllers behind a PCI Root Bridge Controller.
///
struct _NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL {
  ///
  /// Read PCI controller registers in the PCI root bridge memory space.
  ///
  NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL_IO_MEM  Read;
  ///
  /// Write PCI controller registers in the PCI root bridge memory space.
  ///
  NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL_IO_MEM  Write;

  ///
  /// The segment number and bus infor that this PCI root bridge resides.
  ///
  UINT32                                                   SegmentNumber;
  UINT8                                                    MinBusNumber;
  UINT8                                                    MaxBusNumber;
};

#endif

