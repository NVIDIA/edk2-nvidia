/** @file
*
*  AML patching protocol implementation.
*
*  Copyright (c) 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiDxe.h>

#include <Protocol/AmlPatchProtocol.h>

#include <Library/DebugLib.h>
#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiMultiPhase.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <IndustryStandard/Acpi10.h>
#include <IndustryStandard/AcpiAml.h>

#include "AmlPatchDxePrivate.h"

/**
  Validate that the data at the given location has the given opcode.
    - For integer data, assumes the opcode is one byte before the location
      pointed to by the given AmlTableData.
    - For descriptor data, assumes the opcode is properly stored in the header.

  @param[in]  OpCode            Expected OpCode for the data.
  @param[in]  AmlTableData      Pointer to the start of the data to verify.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_INVALID_PARAMETER AmlTableData was NULL, or the given location
                                does not have the given OpCode.
**/
STATIC
EFI_STATUS
EFIAPI
ValidateOpCode(
  IN UINT8  OpCode,
  IN UINT8  *AmlTableData
) {
  if (AmlTableData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (OpCode == AML_BYTE_PREFIX
      || OpCode == AML_WORD_PREFIX
      || OpCode == AML_DWORD_PREFIX
      || OpCode == AML_QWORD_PREFIX) {
    // For integers, the opcode is one byte before the offset table's offset
    // We also assume the new data doesn't have the opcode (just the value)
    if (*((UINT8*)AmlTableData - 1) != OpCode) {
      return EFI_INVALID_PARAMETER;
    }
  } else {
    // For the rest (currently descriptors) the first byte is the opcode.
    if (*(UINT8*)AmlTableData != OpCode) {
      return EFI_INVALID_PARAMETER;
    }
  }

  return EFI_SUCCESS;
}

/**
  Validate new data for an existing Aml node. The given current and new sizes
  must match for the new data to be valid. The given opcode determines
  what is considered valid for the current data. Current validation checks are:
    - For Integers:
      - The byte before the given CurrentData must match the given opcode
    - For descriptors:
      - The first byte of the current data must match the given opcode and the
        first byte of the new data must also match the given opcode.
      - The stored lengths in the header of the new and current data must match.
      - For interrupt descriptors, the interrupt table lengths must match

  @param[in] OffsetTableOpCode  OpCode for the Aml node that is being validated
  @param[in] CurrentData        Pointer to the start of the current data stored
                                for the Aml node.
  @param[in] CurrentSize        Size of the current data for the Aml node.
  @param[in] NewData            Pointer to the start of the new data for the Aml node.
  @param[in] NewSize            Size of the new data.

  @retval EFI_SUCCESS           The new data is valid.
  @retval EFI_UNSUPPORTED       The given opcode is not supported.
  @retval EFI_BAD_BUFFER_SIZE   The given sizes do not match.
  @retval EFI_INVALID_PARAMETER The given CurrentData or NewData was NULL,
                                or the NewData is not a valid replacement
                                for the CurrentData
**/
STATIC
EFI_STATUS
EFIAPI
ValidateNewAmlNode (
  IN UINT8  OffsetTableOpCode,
  IN VOID   *CurrentData,
  IN UINTN  CurrentSize,
  IN VOID   *NewData,
  IN UINTN  NewSize
) {
  EFI_STATUS  Status;

  if (CurrentData == NULL || NewData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (NewSize != CurrentSize) {
    return EFI_BAD_BUFFER_SIZE;
  }

  Status = ValidateOpCode(OffsetTableOpCode, CurrentData);

  if (EFI_ERROR(Status)) {
    return Status;
  }

  // If the data is an integer, assume we are only given the integer data and
  // not the opcode.
  // If it isn't an integer, need to also check the new data's opcode.
  if (OffsetTableOpCode != AML_BYTE_PREFIX
      && OffsetTableOpCode != AML_WORD_PREFIX
      && OffsetTableOpCode != AML_DWORD_PREFIX
      && OffsetTableOpCode != AML_QWORD_PREFIX) {
    Status = ValidateOpCode(OffsetTableOpCode, NewData);
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }

  // Any specific validation that needs to be done for opcodes
  switch (OffsetTableOpCode) {
    // Currently no extra integer validation
    // Since our size check limits bounds
    case AML_BYTE_PREFIX:
    case AML_WORD_PREFIX:
    case AML_DWORD_PREFIX:
    case AML_QWORD_PREFIX:
      break;
    case ACPI_IRQ_NOFLAG_DESCRIPTOR:
    case ACPI_IRQ_DESCRIPTOR:
    case ACPI_DMA_DESCRIPTOR:
    case ACPI_START_DEPENDENT_DESCRIPTOR:
    case ACPI_START_DEPENDENT_EX_DESCRIPTOR:
    case ACPI_END_DEPENDENT_DESCRIPTOR:
    case ACPI_IO_PORT_DESCRIPTOR:
    case ACPI_FIXED_LOCATION_IO_PORT_DESCRIPTOR:
    case ACPI_END_TAG_DESCRIPTOR: {
      if (((ACPI_SMALL_RESOURCE_HEADER*)NewData)->Bits.Length
          != ((ACPI_SMALL_RESOURCE_HEADER*)CurrentData)->Bits.Length) {
        return EFI_INVALID_PARAMETER;
      }
      break;
    }
    case ACPI_24_BIT_MEMORY_RANGE_DESCRIPTOR:
    case ACPI_32_BIT_MEMORY_RANGE_DESCRIPTOR:
    case ACPI_32_BIT_FIXED_MEMORY_RANGE_DESCRIPTOR:
    case ACPI_DWORD_ADDRESS_SPACE_DESCRIPTOR:
    case ACPI_WORD_ADDRESS_SPACE_DESCRIPTOR:
    // Ommitting general address space descriptor from switch because
    // it has the same value as the QWORD address space descriptor.
    // Logic for the two should be the same anyways.
    // case ACPI_ADDRESS_SPACE_DESCRIPTOR:
    case ACPI_QWORD_ADDRESS_SPACE_DESCRIPTOR: {
      if (((ACPI_LARGE_RESOURCE_HEADER*)NewData)->Length
          != ((ACPI_LARGE_RESOURCE_HEADER*)CurrentData)->Length) {
        return EFI_INVALID_PARAMETER;
      }
      break;
    }
    case ACPI_EXTENDED_INTERRUPT_DESCRIPTOR: {
      EFI_ACPI_EXTENDED_INTERRUPT_DESCRIPTOR *CurrentInterrupt;
      EFI_ACPI_EXTENDED_INTERRUPT_DESCRIPTOR *NewInterrupt;
      CurrentInterrupt = (EFI_ACPI_EXTENDED_INTERRUPT_DESCRIPTOR*)CurrentData;
      NewInterrupt = (EFI_ACPI_EXTENDED_INTERRUPT_DESCRIPTOR*)NewData;

      // Make sure the new interrupt has the correct package length and
      // the correct table length
      if (NewInterrupt->Header.Length != CurrentInterrupt->Header.Length
          || NewInterrupt->InterruptTableLength != CurrentInterrupt->InterruptTableLength) {
        return EFI_INVALID_PARAMETER;
      }
      break;
    }
    default:
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Retrieve the size of the Aml node described by the given metadata.

  @param[in]  AmlNodeInfo       Pointer to the metadata for the node. The opcode
                                in the AmlOffsetEntry determines how the size is calculated.
  @param[out] Size              Size calculated for the given Aml node.

  @retval EFI_SUCCESS           The size was returned successfully.
  @retval EFI_UNSUPPORTED       The given opcode is not supported.
  @retval EFI_INVALID_PARAMETER The given AmlNodeInfo or Size was NULL
**/
STATIC
EFI_STATUS
EFIAPI
GetAmlNodeSize(
  IN NVIDIA_AML_NODE_INFO *AmlNodeInfo,
  OUT UINTN               *Size
) {
  AML_OFFSET_TABLE_ENTRY  *OffsetEntry;
  VOID                    *NodeStart;

  if (AmlNodeInfo == NULL || Size == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  OffsetEntry = AmlNodeInfo->AmlOffsetEntry;
  NodeStart = ((UINT8*)AmlNodeInfo->AmlTable) + OffsetEntry->Offset;

  switch (OffsetEntry->Opcode) {
    case AML_BYTE_PREFIX:
      *Size = sizeof(UINT8);
      break;
    case AML_WORD_PREFIX:
      *Size = sizeof(UINT16);
      break;
    case AML_DWORD_PREFIX:
      *Size = sizeof(UINT32);
      break;
    case AML_QWORD_PREFIX:
      *Size = sizeof(UINT64);
      break;
    case ACPI_IRQ_NOFLAG_DESCRIPTOR:
    case ACPI_IRQ_DESCRIPTOR:
    case ACPI_DMA_DESCRIPTOR:
    case ACPI_START_DEPENDENT_DESCRIPTOR:
    case ACPI_START_DEPENDENT_EX_DESCRIPTOR:
    case ACPI_END_DEPENDENT_DESCRIPTOR:
    case ACPI_IO_PORT_DESCRIPTOR:
    case ACPI_FIXED_LOCATION_IO_PORT_DESCRIPTOR:
    case ACPI_END_TAG_DESCRIPTOR: {
      *Size = sizeof(ACPI_SMALL_RESOURCE_HEADER)
                + ((ACPI_SMALL_RESOURCE_HEADER*)NodeStart)->Bits.Length;
      break;
    }
    case ACPI_24_BIT_MEMORY_RANGE_DESCRIPTOR:
    case ACPI_32_BIT_MEMORY_RANGE_DESCRIPTOR:
    case ACPI_32_BIT_FIXED_MEMORY_RANGE_DESCRIPTOR:
    case ACPI_DWORD_ADDRESS_SPACE_DESCRIPTOR:
    case ACPI_WORD_ADDRESS_SPACE_DESCRIPTOR:
    case ACPI_EXTENDED_INTERRUPT_DESCRIPTOR:
    // Ommitting general address space descriptor from switch because
    // it has the same value as the QWORD address space descriptor.
    // Logic for the two should be the same anyways.
    // case ACPI_ADDRESS_SPACE_DESCRIPTOR:
    case ACPI_QWORD_ADDRESS_SPACE_DESCRIPTOR: {
      *Size = sizeof(ACPI_LARGE_RESOURCE_HEADER)
                + ((ACPI_LARGE_RESOURCE_HEADER*)NodeStart)->Length;
      break;
    }
    default:
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Find the aml offset entry for the given path name in the given offset table.

  @param[in]  OffsetTable       Pointer to the start of the offset table
  @param[in]  PathName          Path name of the node in the aml table.
  @param[out] OffsetEntry       Offset entry of the node in the AML table.

  @retval EFI_SUCCESS           The offset was found successfully.
  @retval EFI_NOT_FOUND         The path name could not be found.
  @retval EFI_INVALID_PARAMETER The given OffsetTable, PathName, or OffsetEntry was NULL.
**/
STATIC
EFI_STATUS
EFIAPI
FindAmlOffsetEntry(
  IN AML_OFFSET_TABLE_ENTRY   *OffsetTable,
  IN CHAR8                    *PathName,
  OUT AML_OFFSET_TABLE_ENTRY  **OffsetEntry
) {
  UINTN Index;
  UINTN PathLen;

  if (OffsetTable == NULL || PathName == NULL || OffsetEntry == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Index = 0;
  // Find offset table entry associated with given SdcPathName
  PathLen = AsciiStrLen(PathName);
  while (OffsetTable[Index].Pathname != NULL &&
         AsciiStrnCmp(PathName, OffsetTable[Index].Pathname, PathLen) != 0) {
    Index++;
  }

  if (OffsetTable[Index].Pathname == NULL) {
    return EFI_NOT_FOUND;
  }

  *OffsetEntry = &OffsetTable[Index];
  return EFI_SUCCESS;
}

/**
  Register an array of AML Tables and their corresponding offset tables. These
  are the arrays that will be used to find, verify, and update nodes by the
  rest of the patching interface. The given arrays should match in length, and
  an AML table at a particular index should have its offset table at the matching
  index in the offset table array.

  @param[in]  This              Instance of AML patching protocol.
  @param[in]  AmlTables         Array of pointers to AML tables to be used by the
                                interface. Iteration through the AML tables will
                                be done in the order they are given.
  @param[in]  OffsetTables      Array of pointers to AML offset tables to be used
                                by the interface. An offset table should have the
                                same index as its corresponding AML table does
                                in the given AmlTables array.
  @param[in]  NumTables         The number of AML tables in the given array.
                                Must be nonzero. The array of AML tables and
                                the array of offset tables should each have
                                a length of NumTables.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_OUT_OF_RESOURCES  Could not allocate enough memory to
                                store the given arrays.
  @retval EFI_INVALID_PARAMETER One of the given arrays was NULL,
                                or the given NumTables was 0.
**/
EFI_STATUS
EFIAPI
RegisterAmlTables (
  IN NVIDIA_AML_PATCH_PROTOCOL    *This,
  IN EFI_ACPI_DESCRIPTION_HEADER  **AmlTables,
  IN AML_OFFSET_TABLE_ENTRY       **OffsetTables,
  IN UINTN                        NumTables
) {
  EFI_STATUS                    Status;
  NVIDIA_AML_PATCH_PRIVATE_DATA *Private;
  UINTN                         Index;

  if (AmlTables == NULL || OffsetTables == NULL || NumTables == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NVIDIA_AML_PATCH_PRIVATE_DATA_FROM_PROTOCOL(This);

  Private->NumAmlTables = NumTables;

  Status = gBS->AllocatePool(
    EfiBootServicesData,
    NumTables * sizeof(EFI_ACPI_DESCRIPTION_HEADER *),
    (VOID**)&Private->RegisteredAmlTables
  );

  if (EFI_ERROR(Status)) {
    Private->NumAmlTables = 0;
    Private->RegisteredAmlTables = NULL;
    return Status;
  }

  Status = gBS->AllocatePool(
    EfiBootServicesData,
    NumTables * sizeof(AML_OFFSET_TABLE_ENTRY *),
    (VOID**)&Private->RegisteredOffsetTables
  );

  if (EFI_ERROR(Status)) {
    Private->NumAmlTables = 0;
    Private->RegisteredAmlTables = NULL;
    Private->RegisteredOffsetTables = NULL;
    return Status;
  }

  for (Index = 0; Index < NumTables; Index++) {
    Private->RegisteredAmlTables[Index] = AmlTables[Index];
    Private->RegisteredOffsetTables[Index] = OffsetTables[Index];
  }

  return EFI_SUCCESS;
}

/**
  Find the AML Node for the given path name. Will use the registered AML Tables
  and offset tables to search for a node with the given path name.

  @param[in]  This              Instance of AML patching protocol.
  @param[in]  PathName          Ascii string containing a the path name for the
                                desired node. The path name must appear in one
                                of the registered offset tables.
  @param[out] AmlNodeInfo       Pointer to an NVIDIA_AML_NODE_INFO struct that will be
                                populated with the AML table, offset entry, and
                                size of the node if found.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_FOUND         Could not find the given PathName in any of the
                                registered offset tables.
  @retval EFI_NOT_READY         AML and offset tables have not been registered yet.
  @retval EFI_INVALID_PARAMETER This, PathName, or AmlNodeInfo was NULL
**/
EFI_STATUS
EFIAPI
FindNode (
  IN NVIDIA_AML_PATCH_PROTOCOL  *This,
  IN CHAR8                      *PathName,
  OUT NVIDIA_AML_NODE_INFO      *AmlNodeInfo
) {
  EFI_STATUS                    Status;
  NVIDIA_AML_PATCH_PRIVATE_DATA *Private;
  UINTN                         Index;
  UINTN                         FoundSize;
  EFI_ACPI_DESCRIPTION_HEADER   *CurrentAmlTable;
  AML_OFFSET_TABLE_ENTRY        *CurrentOffsetTable;
  AML_OFFSET_TABLE_ENTRY        *OffsetEntry;

  if (This == NULL || PathName == NULL || AmlNodeInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NVIDIA_AML_PATCH_PRIVATE_DATA_FROM_PROTOCOL(This);

  if (Private->RegisteredOffsetTables == NULL
      || Private->RegisteredAmlTables == NULL) {
    return EFI_NOT_READY;
  }

  for (Index = 0; Index < Private->NumAmlTables; Index++) {
    CurrentOffsetTable = Private->RegisteredOffsetTables[Index];
    CurrentAmlTable = Private->RegisteredAmlTables[Index];
    Status = FindAmlOffsetEntry(CurrentOffsetTable, PathName, &OffsetEntry);
    if (!EFI_ERROR(Status)) {
      AmlNodeInfo->AmlTable = CurrentAmlTable;
      AmlNodeInfo->AmlOffsetEntry = OffsetEntry;
      Status = GetAmlNodeSize(AmlNodeInfo, &FoundSize);
      if (!EFI_ERROR(Status)) {
        AmlNodeInfo->Size = FoundSize;
      } else if (Status == EFI_UNSUPPORTED) {
        // Unsupported means we can't determine size and Get/Set data.
        // Still want to return AmlNodeInfo though because it is still
        // possible to patch the name of the node.
        // (Get and Set will check again to see if the opcode is supported).
        AmlNodeInfo->Size = 0;
        Status = EFI_SUCCESS;
      }
      return Status;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Find the AML Node for the given path name. Will use the AML Tables and offset
  tables to search for a node with the given path name.

  @param[in]  This              Instance of AML patching protocol.
  @param[in]  AmlNodeInfo       Pointer to the AmlNodeInfo for the node whose
                                data will be retrieved.
  @param[out] Data              Pointer to a buffer that will be populated with
                                the data of the node.
  @param[in] Size               Size of the buffer. Should be greater than or
                                equal to the size of the AML node's data.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_UNSUPPORTED       The opcode of the AML node is not supported.
  @retval EFI_BAD_BUFFER_SIZE   The size in the given AmlNodeInfo does not match
                                the size found for the AML node.
  @retval EFI_BUFFER_TOO_SMALL  The given data buffer is too small for the data
                                of the AML node.
  @retval EFI_INVALID_PARAMETER This, AmlNodeInfo, or Data was NULL,
                                or the given AmlNodeInfo opcode did not match
                                the data's opcode.
**/
EFI_STATUS
EFIAPI
GetNodeData(
  IN NVIDIA_AML_PATCH_PROTOCOL  *This,
  IN NVIDIA_AML_NODE_INFO       *AmlNodeInfo,
  OUT VOID                      *Data,
  IN  UINTN                     Size
) {
  EFI_STATUS  Status;
  UINTN       FoundSize;
  VOID        *AmlDataStart;

  if (This == NULL || AmlNodeInfo == NULL || Data == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetAmlNodeSize(AmlNodeInfo, &FoundSize);

  if (EFI_ERROR(Status)) {
    return Status;
  }

  if (AmlNodeInfo->Size != FoundSize) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if (Size < FoundSize) {
    return EFI_BUFFER_TOO_SMALL;
  }

  AmlDataStart = ((UINT8*)AmlNodeInfo->AmlTable)
                  + AmlNodeInfo->AmlOffsetEntry->Offset;

  Status = ValidateOpCode(
    AmlNodeInfo->AmlOffsetEntry->Opcode,
    AmlDataStart
  );

  if (EFI_ERROR(Status)) {
    return Status;
  }

  CopyMem(Data, AmlDataStart, FoundSize);

  return EFI_SUCCESS;
}

/**
  Set the data of the AML Node with the given AmlNodeInfo.

  @param[in]  This              Instance of AML patching protocol.
  @param[in]  AmlNodeInfo       Pointer to the AmlNodeInfo for the node whose
                                data will be set.
  @param[in]  Data              Pointer to a buffer with the data to set the
                                node's data to. The opcode and any internal length
                                fields must match what is already present for
                                the node in both the AML and offset tables.
  @param[in]  Size              The number of bytes that should be set for
                                the given node. This must match the current
                                size of the node in the AML table. Must be nonzero.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_BAD_BUFFER_SIZE   The given Size, the size in AmlNodeInfo, or the
                                found size do not all match.
  @retval EFI_UNSUPPORTED       The opcode of the aml node is not supported.
  @retval EFI_INVALID_PARAMETER This, AmlNodeInfo, or Data was NULL,
                                or the given AmlNodeInfo or the given data
                                did not match what was expected for the node.
**/
EFI_STATUS
EFIAPI
SetNodeData (
  IN NVIDIA_AML_PATCH_PROTOCOL  *This,
  IN NVIDIA_AML_NODE_INFO       *AmlNodeInfo,
  IN VOID                       *Data,
  IN UINTN                      Size
) {
  EFI_STATUS  Status;
  UINTN       FoundSize;
  VOID        *AmlDataStart;

  if (This == NULL || AmlNodeInfo == NULL || Data == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetAmlNodeSize(AmlNodeInfo, &FoundSize);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  if (FoundSize != AmlNodeInfo->Size) {
    return EFI_BAD_BUFFER_SIZE;
  }

  AmlDataStart = ((UINT8*)AmlNodeInfo->AmlTable)
                  + AmlNodeInfo->AmlOffsetEntry->Offset;

  Status = ValidateNewAmlNode(
    AmlNodeInfo->AmlOffsetEntry->Opcode,
    AmlDataStart,
    FoundSize,
    Data,
    Size
  );

  if (EFI_ERROR(Status)) {
    return Status;
  }

  CopyMem(AmlDataStart, Data, Size);

  return EFI_SUCCESS;
}

/**
  Update the name of the AML Node with the given AmlNodeInfo. The name is located
  using the NamesegOffset of the AML offset entry. Does not update the
  name stored in the AML offset entry.

  @param[in]  This              Instance of AML patching protocol.
  @param[in]  AmlNodeInfo       Pointer to the AmlNodeInfo for the node whose
                                name will be updated.
  @param[in]  NewName           Pointer to a buffer with the new name. Must have
                                a length between 1 and 4. If the length is < 4,
                                then the name will be padded to 4 bytes using
                                the '_' character. The first character in the
                                given name must be inclusive of 'A'-'Z' and '_'.
                                The rest of the characters must be inclusive of
                                'A'-'Z', '0'-'9', and '_'.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_BAD_BUFFER_SIZE   The given NewName's length was not between 1-4
  @retval EFI_INVALID_PARAMETER This, AmlNodeInfo, or NewName was NULL,
                                or NewName's characters do not follow the
                                AML spec guidelines.
**/
EFI_STATUS
EFIAPI
UpdateNodeName(
  IN NVIDIA_AML_PATCH_PROTOCOL  *This,
  IN NVIDIA_AML_NODE_INFO       *AmlNodeInfo,
  IN CHAR8                      *NewName
) {
  UINT8 *NameStart;
  UINTN NewNameLength;
  CHAR8 *CurrChar;

  if (This == NULL || AmlNodeInfo == NULL || NewName == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  NewNameLength = AsciiStrLen(NewName);

  if (NewNameLength > AML_NAME_LENGTH || NewNameLength == 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if ((*NewName < 'A' || *NewName > 'Z') && *NewName != '_') {
    return EFI_INVALID_PARAMETER;
  }

  for (CurrChar = NewName + 1; CurrChar < NewName + NewNameLength; CurrChar++) {
    if ((*CurrChar < 'A' || *CurrChar > 'Z')
        && (*CurrChar < '0' || *CurrChar > '9')
        && *CurrChar != '_') {
      return EFI_INVALID_PARAMETER;
    }
  }

  NameStart = (UINT8*)AmlNodeInfo->AmlTable
                + AmlNodeInfo->AmlOffsetEntry->NamesegOffset;

  CopyMem(NameStart, NewName, NewNameLength);
  if (NewNameLength < AML_NAME_LENGTH) {
    SetMem(NameStart + NewNameLength, AML_NAME_LENGTH - NewNameLength, '_');
  }

  return EFI_SUCCESS;
}

/**
  Initialize the AML Patch Driver.

  @param[in]  ImageHandle   of the loaded driver
  @param[in]  SystemTable   Pointer to the System Table

**/
EFI_STATUS
EFIAPI
AmlPatchDxeEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS                    Status;
  NVIDIA_AML_PATCH_PRIVATE_DATA *Private;

  Status = gBS->AllocatePool(EfiBootServicesData,
                             sizeof(NVIDIA_AML_PATCH_PRIVATE_DATA),
                             (VOID**)&Private);

  if (EFI_ERROR(Status) || Private == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Private->Signature = NVIDIA_AML_PATCH_SIGNATURE;
  Private->AmlPatchProtocol.RegisterAmlTables = RegisterAmlTables;
  Private->AmlPatchProtocol.FindNode = FindNode;
  Private->AmlPatchProtocol.GetNodeData = GetNodeData;
  Private->AmlPatchProtocol.SetNodeData = SetNodeData;
  Private->AmlPatchProtocol.UpdateNodeName = UpdateNodeName;

  Status = gBS->InstallMultipleProtocolInterfaces (
    &ImageHandle,
    &gNVIDIAAmlPatchProtocolGuid,
    &Private->AmlPatchProtocol,
    NULL
  );

  return Status;
}
