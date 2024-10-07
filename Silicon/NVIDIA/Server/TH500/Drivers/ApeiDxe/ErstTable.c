/** @file
  NVIDIA Error Record Serialization Table

  SPDX-FileCopyrightText: Copyright (c) 2022-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Apei.h"
#include <TH500/TH500Definitions.h>
#include <LicSwIo.h>
#include <Library/PcdLib.h>

#define ERST_ENTRIES_COUNT  19

typedef struct {
  EFI_ACPI_6_4_ERROR_RECORD_SERIALIZATION_TABLE_HEADER    Header;
  EFI_ACPI_6_4_ERST_SERIALIZATION_INSTRUCTION_ENTRY       Entries[ERST_ENTRIES_COUNT];
} ERST_WITH_ENTRIES;

STATIC ERST_WITH_ENTRIES  ErstTable = {
  .Header                    = {
    .Header                  = {
      .Signature       = EFI_ACPI_6_4_ERROR_RECORD_SERIALIZATION_TABLE_SIGNATURE,
      .Length          = sizeof (ERST_WITH_ENTRIES),
      .Revision        = EFI_ACPI_OEM_REVISION,
      .OemTableId      = MAX_UINT64,
      .OemRevision     = EFI_ACPI_OEM_REVISION,
      .CreatorId       = EFI_ACPI_CREATOR_ID,
      .CreatorRevision = EFI_ACPI_CREATOR_REVISION
    },
    .SerializationHeaderSize = sizeof (EFI_ACPI_6_4_ERROR_RECORD_SERIALIZATION_TABLE_HEADER) -
                               sizeof (EFI_ACPI_DESCRIPTION_HEADER),
    .InstructionEntryCount   = ERST_ENTRIES_COUNT
  },

  /*
   * Action 0: Begin a write operation
   * Implementation: Set Operation field to WRITE in ERST_COMM_STRUCT
   */
  .Entries[0] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_BEGIN_WRITE_OPERATION,
    .Instruction         = EFI_ACPI_6_4_ERST_WRITE_REGISTER_VALUE,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_QWORD
                           /*.Address dynamically assigned */
    },
    .Value = ERST_OPERATION_WRITE,
    .Mask  = ERST_DEFAULT_MASK
  },

  /*
   * Action 1: Begin a read operation
   * Implementation: Set Operation field to READ in ERST_COMM_STRUCT
   */
  .Entries[1] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_BEGIN_READ_OPERATION,
    .Instruction         = EFI_ACPI_6_4_ERST_WRITE_REGISTER_VALUE,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_QWORD
                           /*.Address dynamically assigned */
    },
    .Value = ERST_OPERATION_READ,
    .Mask  = ERST_DEFAULT_MASK
  },

  /*
   * Acction 2: Begin a clear operation
   * Implementation: Set Operation field to CLEAR in ERST_COMM_STRUCT
   */
  .Entries[2] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_BEGIN_CLEAR_OPERATION,
    .Instruction         = EFI_ACPI_6_4_ERST_WRITE_REGISTER_VALUE,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_QWORD
                           /*.Address dynamically assigned */
    },
    .Value = ERST_OPERATION_CLEAR,
    .Mask  = ERST_DEFAULT_MASK
  },

  /*
   * Action 3: End an operation
   * Implementation: Set Operation field to INVALID in ERST_COMM_STRUCT
   */
  .Entries[3] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_END_OPERATION,
    .Instruction         = EFI_ACPI_6_4_ERST_WRITE_REGISTER_VALUE,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_QWORD
                           /*.Address dynamically assigned */
    },
    .Value = ERST_OPERATION_INVALID,
    .Mask  = ERST_DEFAULT_MASK
  },

  /*
   * Action 4: Set the RecordOffset
   * Implementation: Set RecordOffset to the user specified value in ERST_COMM_STRUCT
   */
  .Entries[4] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_SET_RECORD_OFFSET,
    .Instruction         = EFI_ACPI_6_4_ERST_WRITE_REGISTER,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_QWORD
                           /*.Address dynamically assigned */
    },
    .Mask                    = ERST_DEFAULT_MASK
  },

  /*
   * Action 5: Execute the current operation
   * Implementation: Mark the action status as invalid in the ERST_COMM_STRUCT, and then trigger an interrupt to cause RAS FW to do the operation that was set up.
   */
  .Entries[5] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_EXECUTE_OPERATION,
    .Instruction         = EFI_ACPI_6_4_ERST_WRITE_REGISTER_VALUE,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = ERST_STATUS_INVALID_WIDTH,
      .RegisterBitOffset = ERST_STATUS_INVALID_OFFSET,
      .AccessSize        = EFI_ACPI_6_4_DWORD,
      /*.Address dynamically assigned */
    },
    .Value = ERST_STATUS_IS_INVALID,
    .Mask  = ERST_STATUS_INVALID_MASK
  },

  .Entries[6] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_EXECUTE_OPERATION,
    .Instruction         = EFI_ACPI_6_4_ERST_WRITE_REGISTER_VALUE,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 32,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_DWORD,
      /*.Address dynamically assigned */
    },
    .Value = 0x1,
    .Mask  = 0x1
  },

  /*
   * Action 6: Check busy status
   * Implementation: Check if the status "register" in ERST_COMM_STRUCT is invalid. Keep checking it until valid. Then read the interrupt status register
   */
  .Entries[7] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_CHECK_BUSY_STATUS,
    .Instruction         = EFI_ACPI_6_4_ERST_NOOP, // JDS TODO - EFI_ACPI_6_4_ERST_SKIP_NEXT_INSTRUCTION_IF_TRUE
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = ERST_STATUS_INVALID_WIDTH,
      .RegisterBitOffset = ERST_STATUS_INVALID_OFFSET,
      .AccessSize        = EFI_ACPI_6_4_DWORD
                           /*.Address dynamically assigned */
    },
    .Value = ERST_STATUS_IS_VALID,
    .Mask  = ERST_STATUS_INVALID_MASK
  },

  .Entries[8] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_CHECK_BUSY_STATUS,
    .Instruction         = EFI_ACPI_6_4_ERST_NOOP, // JDS TODO - EFI_ACPI_6_4_ERST_GOTO
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = ERST_STATUS_INVALID_WIDTH,
      .RegisterBitOffset = ERST_STATUS_INVALID_OFFSET,
      .AccessSize        = EFI_ACPI_6_4_DWORD
                           /*.Address dynamically assigned */
    },
    .Value = ERST_ENTRIES_COUNT,                   // Intentionally invalid default, to be replaced dynamically
    .Mask  = ERST_GOTO_MASK
  },

  .Entries[9] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_CHECK_BUSY_STATUS,
    .Instruction         = EFI_ACPI_6_4_ERST_READ_REGISTER_VALUE,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 32,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_DWORD
                           /*.Address dynamically assigned */
    },
    .Value = ERST_BUSY_VALUE,
    .Mask  = ERST_BUSY_MASK
  },

  /*
   * Action 7: Check command status
   * Implementation: Read the Status field in ERST_COMM_STRUCT
   */
  .Entries[10] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_GET_COMMAND_STATUS,
    .Instruction         = EFI_ACPI_6_4_ERST_READ_REGISTER,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = ERST_STATUS_WIDTH,
      .RegisterBitOffset = ERST_STATUS_BIT_OFFSET,
      .AccessSize        = EFI_ACPI_6_4_DWORD
                           /*.Address dynamically assigned */
    },
    .Mask                    = ERST_STATUS_MASK
  },

  /*
   * Action 8: Get a valid record identifier
   * Implementation: Read the RecordID field in ERST_COMM_STRUCT
   */
  .Entries[11] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_GET_RECORD_IDENTIFIER,
    .Instruction         = EFI_ACPI_6_4_ERST_READ_REGISTER,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_QWORD
                           /*.Address dynamically assigned */
    },
    .Mask                    = ERST_DEFAULT_MASK
  },

  /*
   * Action 9: Set a record identifier
   * Implementation: Write the RecordID field to the user-specified value in ERST_COMM_STRUCT
   */
  .Entries[12] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_SET_RECORD_IDENTIFIER,
    .Instruction         = EFI_ACPI_6_4_ERST_WRITE_REGISTER,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_QWORD
                           /*.Address dynamically assigned */
    },
    .Mask                    = ERST_DEFAULT_MASK
  },

  /*
   * Action 10: Retrieves the number of error records currently stored on the platform's persistent store.
   * Implementation: Read the RecordCount field in ERST_COMM_STRUCT
   */
  .Entries[13] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_GET_RECORD_COUNT,
    .Instruction         = EFI_ACPI_6_4_ERST_READ_REGISTER,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 32,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_DWORD
                           /*.Address dynamically assigned */
    },
    .Mask                    = ERST_RECORD_COUNT_MASK
  },

  /*
   * Action 11: Starts a dummy write operation that doesn't actually write data
   * Implementation: Set Operation field to DUMMY_WRITE in ERST_COMM_STRUCT
   */
  .Entries[14] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_BEGIN_DUMMY_WRITE_OPERATION,
    .Instruction         = EFI_ACPI_6_4_ERST_WRITE_REGISTER_VALUE,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_QWORD
                           /*.Address dynamically assigned */
    },
    .Value = ERST_OPERATION_DUMMY_WRITE,
    .Mask  = ERST_DEFAULT_MASK
  },

  /*
   * Action 12: Returns the 64-bit physical address OSPM uses as the buffer for reading/writing error records.
   * Implementation: Read the ErrorLogAddressRange.PhysicalBase field in ERST_COMM_STRUCT
   */
  .Entries[15] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_GET_ERROR_LOG_ADDRESS_RANGE,
    .Instruction         = EFI_ACPI_6_4_ERST_READ_REGISTER,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_QWORD
                           /*.Address dynamically assigned */
    },
    .Mask                    = ERST_DEFAULT_MASK
  },

  /*
   * Action 13: Returns the length in bytes of the Error Log Address Range
   * Implementation: Read the ErrorLogAddressRange.Length field in ERST_COMM_STRUCT
   */
  .Entries[16] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_GET_ERROR_LOG_ADDRESS_RANGE_LENGTH,
    .Instruction         = EFI_ACPI_6_4_ERST_READ_REGISTER,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_QWORD
                           /*.Address dynamically assigned */
    },
    .Mask                    = ERST_DEFAULT_MASK
  },

  /*
   * Action 14: Returns the attributes of the Error Log Address Range
   * Implementation: Read the ErrorLogAddressRange.Attributes field in ERST_COMM_STRUCT
   */
  .Entries[17] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_GET_ERROR_LOG_ADDRESS_RANGE_ATTRIBUTES,
    .Instruction         = EFI_ACPI_6_4_ERST_READ_REGISTER,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_QWORD
                           /*.Address dynamically assigned */
    },
    .Mask                    = ERST_DEFAULT_MASK
  },

  /*
   * Action 15: Returns maximum [63:32] and nominal [31:0] execution timing estimates in microseconds
   * Implementation: Read the Timings field in ERST_COMM_STRUCT
   */
  .Entries[18] = {
    .SerializationAction = EFI_ACPI_6_4_ERST_GET_EXECUTE_OPERATION_TIMINGS,
    .Instruction         = EFI_ACPI_6_4_ERST_READ_REGISTER,
    .Flags               = 0,
    .Reserved0           = 0,
    .RegisterRegion      = {
      .AddressSpaceId    = EFI_ACPI_6_4_SYSTEM_MEMORY,
      .RegisterBitWidth  = 64,
      .RegisterBitOffset = 0,
      .AccessSize        = EFI_ACPI_6_4_QWORD
                           /*.Address dynamically assigned */
    },
    .Mask                    = ERST_DEFAULT_MASK
  }
};

VOID
ErstCreateAcpiTable (
  ERST_COMM_STRUCT  *ErstComm
  )
{
  UINTN                                              TableHandle;
  EFI_ACPI_6_4_ERST_SERIALIZATION_INSTRUCTION_ENTRY  *Entry;
  EFI_ACPI_TABLE_PROTOCOL                            *AcpiTableProtocol;
  EFI_STATUS                                         Status;
  UINT64                                             RegisterAddress;
  UINTN                                              EntryIndex;
  UINTN                                              ActionIndex;
  UINT8                                              PreviousAction;

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );

  /* Fill all the needed register addresses in the table */
  PreviousAction = 0xFF;
  ActionIndex    = 0;
  for (EntryIndex = 0; EntryIndex < ERST_ENTRIES_COUNT; EntryIndex++) {
    Entry = &(ErstTable.Entries[EntryIndex]);
    if (PreviousAction != Entry->SerializationAction) {
      ActionIndex = 0;
    } else {
      ActionIndex++;
    }

    switch (Entry->SerializationAction) {
      case EFI_ACPI_6_4_ERST_BEGIN_WRITE_OPERATION:
      case EFI_ACPI_6_4_ERST_BEGIN_READ_OPERATION:
      case EFI_ACPI_6_4_ERST_BEGIN_CLEAR_OPERATION:
      case EFI_ACPI_6_4_ERST_END_OPERATION:
      case EFI_ACPI_6_4_ERST_BEGIN_DUMMY_WRITE_OPERATION:
        RegisterAddress = (UINT64)&(ErstComm->Operation);
        break;
      case EFI_ACPI_6_4_ERST_SET_RECORD_OFFSET:
        RegisterAddress = (UINT64)&(ErstComm->RecordOffset);
        break;
      case EFI_ACPI_6_4_ERST_EXECUTE_OPERATION:
        if (ActionIndex == 0) {
          RegisterAddress = (UINT64)&(ErstComm->Status);
        } else if (ActionIndex == 1) {
          RegisterAddress = TH500_SW_IO6_BASE + INTR_CTLR_SW_IO_N_INTR_STATUS_SET_0_OFFSET;
        }

        break;
      case EFI_ACPI_6_4_ERST_CHECK_BUSY_STATUS:
        if (ActionIndex == 0) {
          RegisterAddress = (UINT64)&(ErstComm->Status);
        } else if (ActionIndex == 1) {
          ASSERT (RegisterAddress == (UINT64)&(ErstComm->Status));
          Entry->Value = EntryIndex-1;
        } else if (ActionIndex == 2) {
          RegisterAddress = TH500_SW_IO6_BASE + INTR_CTLR_SW_IO_N_INTR_STATUS_0_OFFSET;
        }

        break;
      case EFI_ACPI_6_4_ERST_GET_COMMAND_STATUS:
        RegisterAddress = (UINT64)&(ErstComm->Status);
        break;
      case EFI_ACPI_6_4_ERST_GET_RECORD_IDENTIFIER:
      case EFI_ACPI_6_4_ERST_SET_RECORD_IDENTIFIER:
        RegisterAddress = (UINT64)&(ErstComm->RecordID);
        break;
      case EFI_ACPI_6_4_ERST_GET_RECORD_COUNT:
        RegisterAddress = (UINT64)&(ErstComm->RecordCount);
        break;
      case EFI_ACPI_6_4_ERST_GET_ERROR_LOG_ADDRESS_RANGE:
        RegisterAddress = (UINT64)&(ErstComm->ErrorLogAddressRange.PhysicalBase);
        break;
      case EFI_ACPI_6_4_ERST_GET_ERROR_LOG_ADDRESS_RANGE_LENGTH:
        RegisterAddress = (UINT64)&(ErstComm->ErrorLogAddressRange.Length);
        break;
      case EFI_ACPI_6_4_ERST_GET_ERROR_LOG_ADDRESS_RANGE_ATTRIBUTES:
        RegisterAddress = (UINT64)&(ErstComm->ErrorLogAddressRange.Attributes);
        break;
      case EFI_ACPI_6_4_ERST_GET_EXECUTE_OPERATION_TIMINGS:
        RegisterAddress = (UINT64)&(ErstComm->Timings);
        break;
      default:
        ASSERT (!"Invalid Action detected in ACPI ERST table");
    }

    Entry->RegisterRegion.Address = RegisterAddress;
    PreviousAction                = Entry->SerializationAction;
  }

  ErstTable.Header.Header.Checksum = CalculateCheckSum8 (
                                       (UINT8 *)(&ErstTable),
                                       ErstTable.Header.Header.Length
                                       );
  ErstTable.Header.Header.OemTableId = PcdGet64 (PcdAcpiDefaultOemTableId);
  CopyMem (ErstTable.Header.Header.OemId, PcdGetPtr (PcdAcpiDefaultOemId), sizeof (ErstTable.Header.Header.OemId));

  Status = AcpiTableProtocol->InstallAcpiTable (
                                AcpiTableProtocol,
                                &ErstTable,
                                ErstTable.Header.Header.Length,
                                &TableHandle
                                );
  ASSERT_EFI_ERROR (Status);

  return;
}

/**
 * Notification when MmCommunicate2 protocol is installed,
 * indicating that the PCD for the buffer is valid
 * @param Event   - Event that is notified
 * @param Context - Context that was present when registed.
 */
VOID
ErstSetupTable (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  ERST_COMM_STRUCT  *ErstComm;
  UINT64            ErstBufferBase;
  UINT64            ErstBufferSize;

  ErstBufferBase = PcdGet64 (PcdErstBufferBase);
  ErstBufferSize = PcdGet64 (PcdErstBufferSize);

  if (ErstBufferSize < sizeof (ERST_COMM_STRUCT)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ErstBufferSize (0x%llx) is less that sizeof(ERST_COMM_STRUCT) (0x%x)\n",
      __FUNCTION__,
      ErstBufferSize,
      sizeof (ERST_COMM_STRUCT)
      ));
    return;
  }

  ErstComm = (ERST_COMM_STRUCT  *)ErstBufferBase;

  if (ErstComm->Status == ERST_INIT_SUCCESS) {
    ErstCreateAcpiTable (ErstComm);
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Skipping ERST table install because ERST init failed\n", __FUNCTION__));
  }
}
