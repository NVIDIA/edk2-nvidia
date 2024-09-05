/** @file

  DTB update library

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DTB_UPDATE_LIB_H__
#define __DTB_UPDATE_LIB_H__

/**
  Update DTB for UEFI

  @param[in] Dtb                   Pointer to DTB

  @retval None

**/
VOID
EFIAPI
DtbUpdateForUefi (
  VOID  *Dtb
  );

/**
  Update DTB for Kernel

  @param[in] Dtb                   Pointer to DTB

  @retval None

**/
VOID
EFIAPI
DtbUpdateForKernel (
  VOID  *Dtb
  );

#endif
