/** @file
  Generic timer parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "GenericTimerParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/PcdLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/TegraPlatformInfoLib.h>

// For ARMARCH_TMR_HYPVIRT_PPI
#include <TH500/TH500Definitions.h>

STATIC
CM_ARM_GENERIC_TIMER_INFO  GenericTimerInfo_Jetson = {
  SYSTEM_COUNTER_BASE_ADDRESS,
  SYSTEM_COUNTER_READ_BASE,
  FixedPcdGet32 (PcdArmArchTimerSecIntrNum),
  GTDT_GTIMER_FLAGS,
  FixedPcdGet32 (PcdArmArchTimerIntrNum),
  GTDT_GTIMER_FLAGS,
  FixedPcdGet32 (PcdArmArchTimerVirtIntrNum),
  GTDT_GTIMER_FLAGS,
  FixedPcdGet32 (PcdArmArchTimerHypIntrNum),
  GTDT_GTIMER_FLAGS,
  0,
  0
};

STATIC
CM_ARM_GENERIC_TIMER_INFO  GenericTimerInfo_Server = {
  SYSTEM_COUNTER_BASE_ADDRESS,
  SYSTEM_COUNTER_READ_BASE,
  FixedPcdGet32 (PcdArmArchTimerSecIntrNum),
  GTDT_GTIMER_FLAGS_SAVE,
  FixedPcdGet32 (PcdArmArchTimerIntrNum),
  GTDT_GTIMER_FLAGS_SAVE,
  FixedPcdGet32 (PcdArmArchTimerVirtIntrNum),
  GTDT_GTIMER_FLAGS_SAVE,
  FixedPcdGet32 (PcdArmArchTimerHypIntrNum),
  GTDT_GTIMER_FLAGS_SAVE,
  ARMARCH_TMR_HYPVIRT_PPI,
  GTDT_GTIMER_FLAGS_SAVE
};

/** Generic timer parser function

  The following structure is populated:
  typedef struct CmArmGenericTimerInfo {
    /// The physical base address for the counter control frame
    UINT64    CounterControlBaseAddress;

    /// The physical base address for the counter read frame
    UINT64    CounterReadBaseAddress;

    /// The secure PL1 timer interrupt
    UINT32    SecurePL1TimerGSIV;

    /// The secure PL1 timer flags
    UINT32    SecurePL1TimerFlags;

    /// The non-secure PL1 timer interrupt
    UINT32    NonSecurePL1TimerGSIV;

    /// The non-secure PL1 timer flags
    UINT32    NonSecurePL1TimerFlags;

    /// The virtual timer interrupt
    UINT32    VirtualTimerGSIV;

    /// The virtual timer flags
    UINT32    VirtualTimerFlags;

    /// The non-secure PL2 timer interrupt
    UINT32    NonSecurePL2TimerGSIV;

    /// The non-secure PL2 timer flags
    UINT32    NonSecurePL2TimerFlags;

    /// GSIV for the virtual EL2 timer
    UINT32    VirtualPL2TimerGSIV;

    /// Flags for the virtual EL2 timer
    UINT32    VirtualPL2TimerFlags;
  } CM_ARM_GENERIC_TIMER_INFO;

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
GenericTimerParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                 Status;
  CM_ARM_GENERIC_TIMER_INFO  *GenericTimerInfo;
  UINT32                     ChipID;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  ChipID = TegraGetChipID ();
  switch (ChipID) {
    case T194_CHIP_ID:
    case T234_CHIP_ID:
      GenericTimerInfo = &GenericTimerInfo_Jetson;
      break;

    case TH500_CHIP_ID:
      GenericTimerInfo = &GenericTimerInfo_Server;
      break;

    default:
      DEBUG ((DEBUG_ERROR, "%a: Unsupported ChipID 0x%x\n", __FUNCTION__, ChipID));
      return EFI_UNSUPPORTED;
  }

  // Add the CmObj to the Configuration Manager.
  Status = NvAddSingleCmObj (
             ParserHandle,
             CREATE_CM_ARM_OBJECT_ID (EArmObjGenericTimerInfo),
             GenericTimerInfo,
             sizeof (*GenericTimerInfo),
             NULL
             );
  ASSERT_EFI_ERROR (Status);
  return Status;
}

REGISTER_PARSER_FUNCTION (GenericTimerParser, NULL)
