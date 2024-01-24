/** @file
*
*  AML generation protocol definition.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __AML_GENERATION_PROTOCOL_H__
#define __AML_GENERATION_PROTOCOL_H__

#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <IndustryStandard/Acpi10.h>

//
// Define for forward reference.
//
typedef struct _NVIDIA_AML_GENERATION_PROTOCOL NVIDIA_AML_GENERATION_PROTOCOL;

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
typedef
EFI_STATUS
(EFIAPI *NVIDIA_AML_GENERATION_INITIALIZE_TABLE)(
  IN NVIDIA_AML_GENERATION_PROTOCOL *This,
  IN EFI_ACPI_DESCRIPTION_HEADER    *Header
  );

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
typedef
EFI_STATUS
(EFIAPI *NVIDIA_AML_GENERATION_APPEND_DEVICE)(
  IN NVIDIA_AML_GENERATION_PROTOCOL *This,
  IN EFI_ACPI_DESCRIPTION_HEADER    *Device
  );

/**
  Return a pointer to the current table being generated.

  @param[in]  This              Instance of AML generation protocol.
  @param[out] Table             Returned pointer to the current table.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_READY         There is no table currently being generated.
  @retval EFI_INVALID_PARAMETER This or Table was NULL
**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_AML_GENERATION_GET_TABLE)(
  IN NVIDIA_AML_GENERATION_PROTOCOL *This,
  OUT EFI_ACPI_DESCRIPTION_HEADER   **Table
  );

/**
  Starts a scope section for AML generation. Currently nested scope sections
  are not supported.

  @param[in]  This              Instance of AML generation protocol.
  @param[in]  ScopeName         Buffer containing name of the scope section.
                                Must have a length between 1 and 4. If the
                                length is < 4, then the name will be padded
                                to 4 bytes using the '_' character. The first
                                character in the given name must be inclusive of
                                'A'-'Z' and '_'. The rest of the characters must
                                be inclusive of 'A'-'Z', '0'-'9', and '_'.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_BAD_BUFFER_SIZE   The given ScopeName's length was not between 1-4
  @retval EFI_NOT_READY         There is not an AML table being generated,
                                or a scope section has already been started.
  @retval EFI_OUT_OF_RESOURCES  Not enough memory to add a scope section.
  @retval EFI_INVALID_PARAMETER This or ScopeName was NULL, or ScopeName's
                                characters do not follow the AML spec guidelines.
**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_AML_GENERATION_START_SCOPE)(
  IN NVIDIA_AML_GENERATION_PROTOCOL *This,
  IN CHAR8                          *ScopeName
  );

/**
  Ends the current scope for the AML generation protocol.

  @param[in]  This              Instance of AML generation protocol.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_INVALID_PARAMETER This was NULL.
**/
typedef
EFI_STATUS
(EFIAPI *NVIDIA_AML_GENERATION_END_SCOPE)(
  IN NVIDIA_AML_GENERATION_PROTOCOL *This
  );

// NVIDIA_AML_GENERATION_PROTOCOL protocol structure.
struct _NVIDIA_AML_GENERATION_PROTOCOL {
  NVIDIA_AML_GENERATION_INITIALIZE_TABLE    InitializeTable;
  NVIDIA_AML_GENERATION_APPEND_DEVICE       AppendDevice;
  NVIDIA_AML_GENERATION_GET_TABLE           GetTable;
  NVIDIA_AML_GENERATION_START_SCOPE         StartScope;
  NVIDIA_AML_GENERATION_END_SCOPE           EndScope;
  UINT32                                    DeviceCount;
};

#endif /* __AML_GENERATION_PROTOCOL_H__ */
