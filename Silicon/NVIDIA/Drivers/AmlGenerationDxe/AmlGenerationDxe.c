/** @file
*
*  AML generation protocol implementation.
*
*  Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/
#include <PiDxe.h>

#include <Protocol/AmlGenerationProtocol.h>

#include <Library/DebugLib.h>
#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiMultiPhase.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <IndustryStandard/Acpi10.h>
#include <IndustryStandard/AcpiAml.h>

#include "AmlGenerationDxePrivate.h"

/**
  Initialize an AML table to be generated with a given header. Cleans up previous
  table if necessary. The new table will be used in all future functions for the
  given instance of the protocol.

  @param[in]  This              Instance of AML generation protocol.
  @param[in]  Header            Header to initialize the AML table with.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory for the new table.
  @retval EFI_INVALID_PARAMETER This or Header was NULL.
**/
EFI_STATUS
EFIAPI
InitializeTable(
  IN NVIDIA_AML_GENERATION_PROTOCOL *This,
  IN EFI_ACPI_DESCRIPTION_HEADER    *Header
) {
  NVIDIA_AML_GENERATION_PRIVATE_DATA  *Private;
  EFI_STATUS                          Status;

  if (This == NULL || Header == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  Private = NVIDIA_AML_GENERATION_PRIVATE_DATA_FROM_PROTOCOL(This);

  if (Private->CurrentTable != NULL) {
    Status = gBS->FreePool(Private->CurrentTable);

    if (EFI_ERROR(Status)) {
      Private->CurrentTable = NULL;
      return Status;
    }
  }

  Status = gBS->AllocatePool(
    EfiBootServicesData,
    sizeof(EFI_ACPI_DESCRIPTION_HEADER),
    (VOID**)&Private->CurrentTable
  );

  if (EFI_ERROR(Status) || Private->CurrentTable == NULL) {
    Private->CurrentTable = NULL;
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem(Private->CurrentTable, Header, sizeof(EFI_ACPI_DESCRIPTION_HEADER));

  Private->CurrentTable->Length = sizeof(EFI_ACPI_DESCRIPTION_HEADER);

  Private->ScopeStart = NULL;

  return EFI_SUCCESS;
}

/**
  Set the package length for the given Scope object header.

  @param[in]  ScopeHeader       Pointer to start of AML Scope header.
  @param[out] Length            New length value to set the package length to.
                                Must be <= 0x0FFFFFFF per the AML spec.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_INVALID_PARAMETER ScopeHeader was NULL or the given length was
                                outside the required bounds.
**/
STATIC
EFI_STATUS
EFIAPI
SetScopePackageLength(
  IN AML_SCOPE_HEADER *ScopeHeader,
  IN UINT32           Length
) {
  UINT8 LeadByte;

  if (ScopeHeader == NULL || Length > 0x0FFFFFFF) {
    return EFI_INVALID_PARAMETER;
  }

  // Upper 2 bits of LeadByte need to be 11 since we are using 4 bytes total
  // Lowest 4 bits of LeadByte are lowest 4 bits of length
  LeadByte = 0xC0 | (Length & 0xF);

  // Shift upper bytes over 4 bits, then combine with LeadByte
  ScopeHeader->PkgLength = ((Length << 4) & 0xFFFFFF00) | LeadByte;

  return EFI_SUCCESS;
}

/**
  Get the package length for the given Scope object header.

  @param[in]  ScopeHeader       Pointer to start of AML Scope header.
  @param[out] Length            Length of the given scope object.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_INVALID_PARAMETER ScopeHeader or Length was NULL.
**/
STATIC
EFI_STATUS
EFIAPI
GetScopePackageLength(
  IN AML_SCOPE_HEADER *ScopeHeader,
  OUT UINT32          *Length
) {
  UINT32  UpperBytes;

  if (ScopeHeader == NULL || Length == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Upper 3 bytes are shifted 4 bits right, then combined with lowest 4 bits
  // of the lead byte to get resulting length.
  UpperBytes = (ScopeHeader->PkgLength >> 4) & 0x0FFFFFF0;
  *Length = UpperBytes | (ScopeHeader->PkgLength & 0xF);

  return EFI_SUCCESS;
}

/**
  Get the number of bytes taken up by the given device.

  @param[in]  DeviceStart       Pointer to start of AML Device object.
  @param[out] DeviceSize        Number of bytes taken up by given AML Device
                                object. Includes Device opcode bytes.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_BAD_BUFFER_SIZE   The given Size, the size in AmlNodeInfo, or the
                                found size do not all match.
  @retval EFI_UNSUPPORTED       The opcode of the aml node is not supported.
  @retval EFI_INVALID_PARAMETER DeviceStart or DeviceSize was NULL or didn't
                                point to a valid Device object.
**/
STATIC
EFI_STATUS
EFIAPI
GetDeviceLength(
  IN VOID     *DeviceStart,
  OUT UINT32  *DeviceSize
) {
  UINT8 LeadByte;
  UINT8 *DeviceLengthStart;
  UINTN LengthSize;

  if (DeviceStart == NULL || DeviceSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (*(UINT8*)DeviceStart != 0x5B || *((UINT8*)DeviceStart + 1) != 0x82) {
    return EFI_INVALID_PARAMETER;
  }

  DeviceLengthStart = (UINT8*)DeviceStart + 2;
  LeadByte = *DeviceLengthStart;
  LengthSize = 1 + ((LeadByte >> 6) & 0x3);

  if (LengthSize == 1) {
    *DeviceSize = (LeadByte) & 0x3F;
  } else if (LengthSize == 2) {
    *DeviceSize = ((*(UINT16*)DeviceLengthStart >> 4) & 0x0FF0) | (LeadByte & 0xF);
  } else if (LengthSize == 3) {
    *DeviceSize = ((*(UINT32*)DeviceLengthStart >> 4) & 0x0FFFF0) | (LeadByte & 0xF);
  } else {
    *DeviceSize = ((*(UINT32*)DeviceLengthStart >> 4) & 0x0FFFFFFF0) | (LeadByte & 0xF);
  }

  // Add Two Bytes to include opcode bytes
  *DeviceSize += 2;

  return EFI_SUCCESS;
}

/**
  Appends a device to the current AML table being generated. If there is a scope
  section that has been started, the appended device will be also be included in
  the scope section.

  @param[in]  This              Instance of AML generation protocol.
  @param[in]  Device            Pointer to the start of an AML table containing
                                the device to append to the generated AML table.
                                The given AML table should only contain a single
                                Device after the header.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory to extend the table.
  @retval EFI_NOT_READY         There is not currently a table being generated.
  @retval EFI_INVALID_PARAMETER This or Device was NULL, or the given AML Table
                                didn't have only a single Device after the header.
**/
EFI_STATUS
EFIAPI
AppendDevice(
  IN NVIDIA_AML_GENERATION_PROTOCOL *This,
  IN EFI_ACPI_DESCRIPTION_HEADER    *Device
) {
  NVIDIA_AML_GENERATION_PRIVATE_DATA  *Private;
  EFI_STATUS                          Status;
  UINT8                               *DeviceStart;
  UINT32                              DeviceSize;
  UINT32                              ScopeLength;
  UINTN                               NewLength;
  EFI_ACPI_DESCRIPTION_HEADER         *NewTable;
  AML_SCOPE_HEADER                    *ScopeHeader;

  if (This == NULL || Device == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NVIDIA_AML_GENERATION_PRIVATE_DATA_FROM_PROTOCOL(This);

  if (Private->CurrentTable == NULL) {
    return EFI_NOT_READY;
  }

  DeviceStart = (UINT8*)Device + sizeof(EFI_ACPI_DESCRIPTION_HEADER);
  if (*(UINT8*)DeviceStart != AML_EXT_OP
      || *((UINT8*)DeviceStart + 1) != AML_EXT_DEVICE_OP) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetDeviceLength(DeviceStart, &DeviceSize);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  if (DeviceSize != Device->Length - sizeof(EFI_ACPI_DESCRIPTION_HEADER)) {
    return EFI_INVALID_PARAMETER;
  }

  NewLength = Private->CurrentTable->Length + DeviceSize;

  Status = gBS->AllocatePool(EfiBootServicesData, NewLength, (VOID**)&NewTable);

  if (EFI_ERROR(Status) || NewTable == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  CopyMem(NewTable, Private->CurrentTable, Private->CurrentTable->Length);
  CopyMem(
    (UINT8*)NewTable + Private->CurrentTable->Length,
    DeviceStart,
    DeviceSize
  );

  if (Private->ScopeStart != NULL) {
    Private->ScopeStart = (UINT8*)NewTable + ((UINT8*)Private->ScopeStart - (UINT8*)Private->CurrentTable);
  }
  gBS->FreePool(Private->CurrentTable);
  Private->CurrentTable = NewTable;
  Private->CurrentTable->Length = NewLength;

  if (Private->ScopeStart != NULL) {
    ScopeHeader = (AML_SCOPE_HEADER*)Private->ScopeStart;
    Status = GetScopePackageLength(ScopeHeader, &ScopeLength);
    if (EFI_ERROR(Status)) {
      return Status;
    }

    Status = SetScopePackageLength(ScopeHeader, ScopeLength + DeviceSize);
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Return a pointer to the current table being generated.

  @param[in]  This              Instance of AML generation protocol.
  @param[out] Table             Returned pointer to the current table.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_READY         There is no table currently being generated.
  @retval EFI_INVALID_PARAMETER This or Table was NULL
**/
EFI_STATUS
EFIAPI
GetTable(
  IN NVIDIA_AML_GENERATION_PROTOCOL *This,
  OUT EFI_ACPI_DESCRIPTION_HEADER   **Table
) {
  NVIDIA_AML_GENERATION_PRIVATE_DATA  *Private;

  if (This == NULL || Table == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NVIDIA_AML_GENERATION_PRIVATE_DATA_FROM_PROTOCOL(This);

  if (Private->CurrentTable == NULL) {
    return EFI_NOT_READY;
  }

  *Table = Private->CurrentTable;

  return EFI_SUCCESS;
}

/**
  Starts a scope section for AML generation. Currently nested scope sections
  are not supported.

  @param[in]  This              Instance of AML generation protocol.
  @param[in]  ScopeName         Buffer containing name of the scope section.
                                Length of the string must be 4

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_BAD_BUFFER_SIZE   The given ScopeName's length was not 4.
  @retval EFI_NOT_READY         There is not an AML table being generated,
                                or a scope section has already been started.
  @retval EFI_OUT_OF_RESOURCES  Not enough memory to add a scope section.
  @retval EFI_INVALID_PARAMETER This or ScopeName was NULL.
**/
EFI_STATUS
EFIAPI
StartScope(
  IN NVIDIA_AML_GENERATION_PROTOCOL *This,
  IN CHAR8                          *ScopeName
) {
  NVIDIA_AML_GENERATION_PRIVATE_DATA  *Private;
  EFI_STATUS                          Status;
  UINTN                               NewLength;
  EFI_ACPI_DESCRIPTION_HEADER         *NewTable;
  AML_SCOPE_HEADER                    *ScopeHeader;
  UINTN                               ScopeNameLength;

  if (This == NULL || ScopeName == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NVIDIA_AML_GENERATION_PRIVATE_DATA_FROM_PROTOCOL(This);

  if (Private->CurrentTable == NULL || Private->ScopeStart != NULL) {
    return EFI_NOT_READY;
  }

  ScopeNameLength = AsciiStrLen(ScopeName);

  if (ScopeNameLength != AML_NAME_LENGTH) {
    return EFI_BAD_BUFFER_SIZE;
  }

  NewLength = Private->CurrentTable->Length + sizeof(AML_SCOPE_HEADER);

  Status = gBS->AllocatePool(EfiBootServicesData, NewLength, (VOID**)&NewTable);

  if (EFI_ERROR(Status) || NewTable == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem(NewTable, Private->CurrentTable, Private->CurrentTable->Length);

  ScopeHeader = (AML_SCOPE_HEADER*) ((UINT8*)NewTable + NewTable->Length);

  ScopeHeader->OpCode = AML_SCOPE_OP;
  CopyMem(ScopeHeader->Name, ScopeName, AML_NAME_LENGTH);
  // Scope Length should include package bytes and name bytes but not opcode
  SetScopePackageLength(ScopeHeader, sizeof(AML_SCOPE_HEADER) - 1);

  Private->ScopeStart = ScopeHeader;

  Status = gBS->FreePool(Private->CurrentTable);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  Private->CurrentTable = NewTable;
  Private->CurrentTable->Length = NewLength;

  return EFI_SUCCESS;
}

/**
  Ends the current scope for the AML generation protocol.

  @param[in]  This              Instance of AML generation protocol.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_INVALID_PARAMETER This was NULL.
**/
EFI_STATUS
EFIAPI
EndScope(
  IN NVIDIA_AML_GENERATION_PROTOCOL *This
) {
  NVIDIA_AML_GENERATION_PRIVATE_DATA  *Private;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NVIDIA_AML_GENERATION_PRIVATE_DATA_FROM_PROTOCOL(This);

  Private->ScopeStart = NULL;

  return EFI_SUCCESS;
}

/**
  Initialize the AML Generation Driver.

  @param[in]  ImageHandle   of the loaded driver
  @param[in]  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Initialization succeeded.
  @retval EFI_OUT_OF_RESOURCES  Not enough memory for initialization.
**/
EFI_STATUS
EFIAPI
AmlGenerationDxeEntryPoint(
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
) {
  NVIDIA_AML_GENERATION_PRIVATE_DATA  *Private;
  EFI_STATUS                          Status;

  Status = gBS->AllocatePool(EfiBootServicesData,
                             sizeof(NVIDIA_AML_GENERATION_PRIVATE_DATA),
                             (VOID**)&Private);

  if (EFI_ERROR(Status) || Private == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Private->Signature = NVIDIA_AML_GENERATION_SIGNATURE;
  Private->CurrentTable = NULL;
  Private->ScopeStart = NULL;
  Private->AmlGenerationProtocol.InitializeTable = InitializeTable;
  Private->AmlGenerationProtocol.AppendDevice = AppendDevice;
  Private->AmlGenerationProtocol.GetTable = GetTable;
  Private->AmlGenerationProtocol.StartScope = StartScope;
  Private->AmlGenerationProtocol.EndScope = EndScope;

  Status = gBS->InstallMultipleProtocolInterfaces (
    &ImageHandle,
    &gNVIDIAAmlGenerationProtocolGuid,
    &Private->AmlGenerationProtocol,
    NULL
  );

  return Status;
}
