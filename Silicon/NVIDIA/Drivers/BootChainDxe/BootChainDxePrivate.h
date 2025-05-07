/** @file

  Boot Chain Protocol Driver

  SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BOOT_CHAIN_DXE_PRIVATE_H__
#define __BOOT_CHAIN_DXE_PRIVATE_H__

#include <Uefi/UefiBaseType.h>
#include <Protocol/BootChainProtocol.h>
#include <Protocol/BrBctUpdateProtocol.h>

#define NUM_BOOT_CHAINS             2
#define BOOT_CHAIN_MAX_RESET_COUNT  3

#define STATUS_SUCCESS                          0
#define STATUS_IN_PROGRESS                      1
#define STATUS_ERROR_NO_OPERATION_REQUIRED      2
#define STATUS_ERROR_CANCELED_FOR_FMP_CONFLICT  3
#define STATUS_ERROR_READING_STATUS             4
#define STATUS_ERROR_MAX_RESET_COUNT            5
#define STATUS_ERROR_SETTING_RESET_COUNT        6
#define STATUS_ERROR_SETTING_IN_PROGRESS        7
#define STATUS_ERROR_IN_PROGRESS_FAILED         8
#define STATUS_ERROR_BAD_BOOT_CHAIN_NEXT        9
#define STATUS_ERROR_READING_NEXT               10
#define STATUS_ERROR_UPDATING_FW_CHAIN          11
#define STATUS_ERROR_BOOT_CHAIN_FAILED          12
#define STATUS_ERROR_READING_RESET_COUNT        13
#define STATUS_ERROR_BOOT_NEXT_EXISTS           14
#define STATUS_ERROR_READING_SCRATCH            15
#define STATUS_ERROR_SETTING_SCRATCH            16
#define STATUS_ERROR_UPDATE_BR_BCT_FLAG_SET     17
#define STATUS_ERROR_SETTING_PREVIOUS           18
#define STATUS_ERROR_BOOT_CHAIN_IS_FAILED       19

typedef enum {
  BC_CURRENT,
  BC_NEXT,
  BC_STATUS,
  BC_PREVIOUS,
  BC_RESET_COUNT,
  AUTO_UPDATE_BR_BCT,
  BC_VARIABLE_INDEX_MAX
} BC_VARIABLE_INDEX;

typedef struct {
  CHAR16      *Name;
  UINT32      Attributes;
  UINT8       Bytes;
  EFI_GUID    *Guid;
} BC_VARIABLE;

extern UINT32                         mBootChain;
extern NVIDIA_BR_BCT_UPDATE_PROTOCOL  *mBrBctUpdateProtocol;
extern NVIDIA_BOOT_CHAIN_PROTOCOL     mProtocol;
extern EFI_EVENT                      mEndOfDxeEvent;
extern BOOLEAN                        mUpdateBrBctFlag;
extern BC_VARIABLE                    mBCVariables[];

/**
  Delete a boot chain variable.

  @param[in]  VariableIndex     Index of variable to set

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
BCDeleteVariable (
  IN  BC_VARIABLE_INDEX  VariableIndex
  );

/**
  Get a boot chain variable.

  @param[in]  VariableIndex     Index of variable to set
  @param[Out] Value             Pointer to return variable's value

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
BCGetVariable (
  IN  BC_VARIABLE_INDEX  VariableIndex,
  OUT UINT32             *Value
  );

/**
  Set a boot chain variable.

  @param[in]  VariableIndex     Index of variable to set
  @param[in]  Value             Value to set

  @retval EFI_SUCCESS           Operation successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
BCSetVariable (
  IN  BC_VARIABLE_INDEX  VariableIndex,
  IN  UINT32             Value
  );

/**
  Check the bootloader scratch register and AutoUpdateBrBct UEFI variable
  to see if the BR-BCT should be updated at End Of Dxe.

  @retval BOOLEAN           Flag TRUE if BR-BCT should be updated

**/
BOOLEAN
EFIAPI
BrBctUpdateNeeded (
  VOID
  );

/**
  Reset into new boot chain

  @param[in]  BootChain     BootChain to boot

  @retval None

**/
VOID
EFIAPI
BootChainReset (
  UINT32  BootChain
  );

/**
  Finish the boot chain update sequence.

  @param[In] BCStatus       BootChainStatus variable value

  @retval EFI_SUCCESS       Operation successful
  @retval others            Error occurred

**/
VOID
EFIAPI
BootChainFinishUpdate (
  IN     UINT32  BCStatus
  );

/**
  Set the boot chain scratch register to boot the given BootChain.

  @param[in]  BootChain     BootChain

  @retval EFI_SUCCESS       Operation successful
  @retval others            Error occurred

**/
EFI_STATUS
EFIAPI
BootChainSetScratchRegister (
  IN  UINT32  BootChain
  );

/**
  Boot Chain Protocol Driver initialization entry point.

  @param[in]  ImageHandle       Image handle
  @param[in]  SystemTable       Pointer to system table

  @retval EFI_SUCCESS           Initialization successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
BootChainDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

#endif
