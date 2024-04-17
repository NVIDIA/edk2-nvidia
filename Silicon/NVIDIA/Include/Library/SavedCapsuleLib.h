/** @file

  Saved Capsule Library

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SAVED_CAPSULE_LIB_H__
#define __SAVED_CAPSULE_LIB_H__

/**
  Store the capsule for access after reset

  @param[in]  CapsuleData       Pointer to capsule data
  @param[in]  Size              Size of capsule data

  @retval EFI_SUCCESS           Operation completed successfully
  @retval Others                An error occurred

**/
EFI_STATUS
EFIAPI
CapsuleStore (
  IN VOID   *CapsuleData,
  IN UINTN  Size
  );

/**
  Load a saved capsule into buffer

  @param[in]  Buffer            Pointer to buffer to load capsule
  @param[in]  Size              Size of capsule buffer

  @retval EFI_SUCCESS           Operation completed successfully
  @retval Others                An error occurred

**/
EFI_STATUS
EFIAPI
CapsuleLoad (
  IN VOID   *Buffer,
  IN UINTN  Size
  );

/**
  Initialize library

  @retval EFI_SUCCESS           Operation completed successfully
  @retval Others                An error occurred

**/
EFI_STATUS
EFIAPI
SavedCapsuleLibInitialize (
  VOID
  );

#endif
