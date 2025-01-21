/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2021-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/
#include "Apei.h"
#include <TH500/TH500Definitions.h>
#include <LicSwIo.h>

typedef struct {
  EFI_ACPI_6_5_ERROR_INJECTION_TABLE_HEADER        Header;
  EFI_ACPI_6_5_EINJ_INJECTION_INSTRUCTION_ENTRY    Entries[EINJ_ENTRIES_COUNT];
} EINJ_WITH_ENTRIES;

STATIC EINJ_WITH_ENTRIES  EinjTable = {
  .Header                                                    = {
    .Header                                                  = {
      .Signature       = EFI_ACPI_6_5_ERROR_INJECTION_TABLE_SIGNATURE,
      .Length          = sizeof (EINJ_WITH_ENTRIES),
      .Revision        = EFI_ACPI_6_5_ERROR_INJECTION_TABLE_REVISION,
      .OemTableId      = MAX_UINT64,
      .OemRevision     = EFI_ACPI_OEM_REVISION,
      .CreatorId       = EFI_ACPI_CREATOR_ID,
      .CreatorRevision = EFI_ACPI_CREATOR_REVISION
    },
    .InjectionHeaderSize                                     = sizeof (EFI_ACPI_6_5_ERROR_INJECTION_TABLE_HEADER) -
                                                               sizeof (EFI_ACPI_DESCRIPTION_HEADER),
    .InjectionFlags      = 0,
    .InjectionEntryCount = EINJ_ENTRIES_COUNT
  },

  /*
   * Entry 0: indicates the beginning of an error injection.
   * Implementation: No-op.
   */
  .Entries[EFI_ACPI_6_5_EINJ_BEGIN_INJECTION_OPERATION] = {
    .InjectionAction = EFI_ACPI_6_5_EINJ_BEGIN_INJECTION_OPERATION,
    .Instruction     = EFI_ACPI_6_5_EINJ_NOOP
  },

  /*
   * Entry 1: return the pointer to the Trigger Action Table
   * Implementation: Read register pointing to TriggerActionTablePtr in
   * RAS_FW_EINJ_COMM_STRUCT.
   */
  .Entries[EFI_ACPI_6_5_EINJ_GET_TRIGGER_ERROR_ACTION_TABLE] = {
    .InjectionAction = EFI_ACPI_6_5_EINJ_GET_TRIGGER_ERROR_ACTION_TABLE,
    .Instruction     = EFI_ACPI_6_5_EINJ_READ_REGISTER,
    .RegisterRegion  = {
      .AddressSpaceId    = EFI_ACPI_6_5_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_5_QWORD
                           /*.Address dynamically assigned */
    },
    .Mask                                                    = EINJ_DEFAULT_MASK
  },

  /*
   * Entry 2: Set error type for the error to inject.
   * Implementation: No-op (entry not used in ACPI5+)
   */
  .Entries[EFI_ACPI_6_5_EINJ_SET_ERROR_TYPE] = {
    .InjectionAction = EFI_ACPI_6_5_EINJ_SET_ERROR_TYPE,
    .Instruction     = EFI_ACPI_6_5_EINJ_NOOP
  },

  /*
   * Entry 3: Get error injection capabilities.
   * Implementation: Read register pointing to SupportedTypes in
   * RAS_FW_EINJ_COMM_STRUCT.
   */
  .Entries[EFI_ACPI_6_5_EINJ_GET_ERROR_TYPE] = {
    .InjectionAction = EFI_ACPI_6_5_EINJ_GET_ERROR_TYPE,
    .Instruction     = EFI_ACPI_6_5_EINJ_READ_REGISTER,
    .RegisterRegion  = {
      .AddressSpaceId    = EFI_ACPI_6_5_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_5_QWORD
                           /*.Address dynamically assigned */
    },
    .Mask                                                    = EINJ_DEFAULT_MASK
  },

  /*
   * Entry 4: End of injection operation.
   * Implementation: No-op
   */
  .Entries[EFI_ACPI_6_5_EINJ_END_OPERATION] = {
    .InjectionAction = EFI_ACPI_6_5_EINJ_END_OPERATION,
    .Instruction     = EFI_ACPI_6_5_EINJ_NOOP
  },

  /*
   * Entry 5: Execute operation.
   * Implementation: No-op (operation execution is carried out by the Trigger
   * Action table)
   */
  .Entries[EFI_ACPI_6_5_EINJ_EXECUTE_OPERATION] = {
    .InjectionAction = EFI_ACPI_6_5_EINJ_EXECUTE_OPERATION,
    .Instruction     = EFI_ACPI_6_5_EINJ_NOOP
  },

  /*
   * Entry 6: Check busy status
   * Implementation: Read register pointing to Busy in
   * RAS_FW_EINJ_COMM_STRUCT.
   */
  .Entries[EFI_ACPI_6_5_EINJ_CHECK_BUSY_STATUS] = {
    .InjectionAction = EFI_ACPI_6_5_EINJ_CHECK_BUSY_STATUS,
    .Instruction     = EFI_ACPI_6_5_EINJ_READ_REGISTER,
    .RegisterRegion  = {
      .AddressSpaceId    = EFI_ACPI_6_5_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_5_QWORD
                           /*.Address dynamically assigned */
    },
    .Mask                                                    = EINJ_DEFAULT_MASK
  },

  /*
   * Entry 7: Check command status
   * Implementation: Read register pointing to Status in
   * RAS_FW_EINJ_COMM_STRUCT.
   */
  .Entries[EFI_ACPI_6_5_EINJ_GET_COMMAND_STATUS] = {
    .InjectionAction = EFI_ACPI_6_5_EINJ_GET_COMMAND_STATUS,
    .Instruction     = EFI_ACPI_6_5_EINJ_READ_REGISTER,
    .RegisterRegion  = {
      .AddressSpaceId    = EFI_ACPI_6_5_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_5_QWORD
                           /*.Address dynamically assigned */
    },
    .Mask                                                    = EINJ_DEFAULT_MASK
  },

  /*
   * Entry 8: Set error type with address
   * Implementation: Write register pointing to SetErrorTypeWithAddressPtr in
   * RAS_FW_EINJ_COMM_STRUCT.
   */
  .Entries[EFI_ACPI_6_X_EINJ_SET_ERROR_TYPE_WITH_ADDRESS] = {
    .InjectionAction = EFI_ACPI_6_X_EINJ_SET_ERROR_TYPE_WITH_ADDRESS,
    .Instruction     = EFI_ACPI_6_5_EINJ_WRITE_REGISTER,
    .RegisterRegion  = {
      .AddressSpaceId    = EFI_ACPI_6_5_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_5_QWORD
                           /*.Address dynamically assigned */
    },
    .Mask                                                    = EINJ_DEFAULT_MASK
  },

  /*
   * Entry 9: Get execution operation timings
   * Implementation: Read register pointing to Timings in
   * RAS_FW_EINJ_COMM_STRUCT.
   */
  .Entries[EFI_ACPI_6_X_EINJ_GET_EXECUTE_OPERATION_TIMINGS] = {
    .InjectionAction = EFI_ACPI_6_X_EINJ_GET_EXECUTE_OPERATION_TIMINGS,
    .Instruction     = EFI_ACPI_6_5_EINJ_READ_REGISTER,
    .RegisterRegion  = {
      .AddressSpaceId    = EFI_ACPI_6_5_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_5_QWORD
                           /*.Address dynamically assigned */
    },
    .Mask                                                    = EINJ_DEFAULT_MASK
  }
};

STATIC
EFI_ACPI_6_X_EINJ_TRIGGER_ERROR_ACTION_TABLE
  TriggerErrorActionTable = {
  .Header                = {
    .HeaderSize = sizeof (EFI_ACPI_6_5_EINJ_TRIGGER_ACTION_TABLE),
    .Revision   = 1,
    .TableSize  = sizeof (EFI_ACPI_6_X_EINJ_TRIGGER_ERROR_ACTION_TABLE),
    .EntryCount = EINJ_TRIGGER_ACTION_COUNT
  },
  .TriggerActions[0] = {
    .InjectionAction = EFI_ACPI_6_5_EINJ_TRIGGER_ERROR,
    .Instruction     = EFI_ACPI_6_5_EINJ_WRITE_REGISTER_VALUE,
    .RegisterRegion  = {
      .AddressSpaceId    = EFI_ACPI_6_5_SYSTEM_MEMORY,
      .RegisterBitWidth  = 32,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_5_DWORD,
      .Address           = TH500_SW_IO0_BASE + INTR_CTLR_SW_IO_N_INTR_STATUS_SET_0_OFFSET
    },
    .Value = 0x1,
    .Mask  = 0x1
  }
};

VOID
EINJCreateAcpiTable (
  RAS_FW_EINJ_COMM_STRUCT  *EinjComm
  )
{
  UINTN                                          TableHandle;
  EFI_ACPI_6_5_EINJ_INJECTION_INSTRUCTION_ENTRY  *Entry;
  EFI_ACPI_TABLE_PROTOCOL                        *AcpiTableProtocol;
  EFI_STATUS                                     Status = EFI_SUCCESS;

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );

  /* Copy the Trigger Action Table in memory */
  CopyMem (
    (VOID *)&(EinjComm->TriggerErrorActionTable),
    (VOID *)&TriggerErrorActionTable,
    sizeof (TriggerErrorActionTable)
    );

  /*
   * Setup pointers. Note: According to ACPI spec, trigger action table entry
   * "Returns a 64-bit physical memory pointer to the Trigger Action". So it
   * needs a "register" that holds the pointer to the table.
   */
  EinjComm->TriggerActionTableRegister = (UINT64)&(EinjComm->TriggerActionTablePtr);
  EinjComm->TriggerActionTablePtr      = (UINT64)&(EinjComm->TriggerErrorActionTable);
  EinjComm->SetErrorTypeWithAddressPtr = (UINT64)&(EinjComm->SetErrorTypeWithAddress);

  /* Fill all the needed register addresses in the table */
  Entry                         = &(EinjTable.Entries[EFI_ACPI_6_5_EINJ_GET_TRIGGER_ERROR_ACTION_TABLE]);
  Entry->RegisterRegion.Address = EinjComm->TriggerActionTableRegister;

  Entry                         = &(EinjTable.Entries[EFI_ACPI_6_5_EINJ_GET_ERROR_TYPE]);
  Entry->RegisterRegion.Address = (UINT64)&(EinjComm->SupportedTypes);

  Entry                         = &(EinjTable.Entries[EFI_ACPI_6_5_EINJ_CHECK_BUSY_STATUS]);
  Entry->RegisterRegion.Address = (UINT64)&(EinjComm->Busy);

  Entry                         = &(EinjTable.Entries[EFI_ACPI_6_5_EINJ_GET_COMMAND_STATUS]);
  Entry->RegisterRegion.Address = (UINT64)&(EinjComm->Status);

  Entry                         = &(EinjTable.Entries[EFI_ACPI_6_X_EINJ_SET_ERROR_TYPE_WITH_ADDRESS]);
  Entry->RegisterRegion.Address = EinjComm->SetErrorTypeWithAddressPtr;

  Entry                         = &(EinjTable.Entries[EFI_ACPI_6_X_EINJ_GET_EXECUTE_OPERATION_TIMINGS]);
  Entry->RegisterRegion.Address = (UINT64)&(EinjComm->Timings);

  EinjTable.Header.Header.Checksum = CalculateCheckSum8 (
                                       (UINT8 *)(&EinjTable),
                                       EinjTable.Header.Header.Length
                                       );
  EinjTable.Header.Header.OemTableId = PcdGet64 (PcdAcpiDefaultOemTableId);
  CopyMem (EinjTable.Header.Header.OemId, PcdGetPtr (PcdAcpiDefaultOemId), sizeof (EinjTable.Header.Header.OemId));

  Status = AcpiTableProtocol->InstallAcpiTable (
                                AcpiTableProtocol,
                                &EinjTable,
                                EinjTable.Header.Header.Length,
                                &TableHandle
                                );
  ASSERT_EFI_ERROR (Status);

  return;
}

EFI_STATUS
EinjSetupTable (
  IN RAS_FW_BUFFER  *RasFwBufferInfo
  )
{
  RAS_FW_EINJ_COMM_STRUCT  *EinjComm;

  /* RAS_FW should have initialized the shared EINJ structure */
  EinjComm = (RAS_FW_EINJ_COMM_STRUCT *)RasFwBufferInfo->EinjBase;
  if (EinjComm->Signature == EINJ_DISABLED_SIGNATURE) {
    DEBUG ((DEBUG_ERROR, "%a: EINJ is disabled\n", __FUNCTION__));
    return EFI_SUCCESS;
  } else if (EinjComm->Signature != EFI_ACPI_6_5_ERROR_INJECTION_TABLE_SIGNATURE) {
    DEBUG ((DEBUG_ERROR, "%a: EINJComm not initialized\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  EINJCreateAcpiTable (EinjComm);

  return EFI_SUCCESS;
}
