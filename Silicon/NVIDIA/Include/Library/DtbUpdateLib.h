/** @file

  DTB update library

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DTB_UPDATE_LIB_H__
#define __DTB_UPDATE_LIB_H__

/**
  Update all MAC addresses in DTB

  @param[in] Dtb                   Pointer to DTB

  @retval None
**/
VOID
EFIAPI
DtbUpdateMacAddresses (
  VOID  *Dtb
  );

/**
  Update MAC address in DTB node

  @param[in] Dtb                   Pointer to DTB
  @param[in] NodeOffset            Offset of node to update

  @retval None
**/
VOID
EFIAPI
DtbUpdateNodeMacAddress (
  VOID   *Dtb,
  INT32  NodeOffset
  );

#endif
