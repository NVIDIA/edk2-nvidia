/** @file

  Copyright (c) 2019 - 2020, NVIDIA CORPORATION. All rights reserved.
  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  The original software modules are licensed as follows:

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.
  Copyright (c) 2011 - 2014, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Portions provided under the following terms:
  Copyright (c) 2019-2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019-2020 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/


#ifndef EMAC_DXE_UTIL_H__
#define EMAC_DXE_UTIL_H__

#include <Protocol/SimpleNetwork.h>
#include "osi_core.h"
#include "osi_dma.h"

typedef struct {
  struct osi_core_priv_data   *osi_core;
  struct osi_dma_priv_data    *osi_dma;
  void                        *tx_buffers[TX_DESC_CNT];
  void                        *tx_completed_buffer;
  void                        *rx_user_buffer;
  long                        rx_user_buffer_size;
} EMAC_DRIVER;

EFI_STATUS
EFIAPI
EmacDxeInitialization (
  IN  EMAC_DRIVER             *EmacDriver,
  IN  UINTN                   MacBaseAddress,
  IN  UINT32                  MacType
  );

#endif // EMAC_DXE_UTIL_H__
