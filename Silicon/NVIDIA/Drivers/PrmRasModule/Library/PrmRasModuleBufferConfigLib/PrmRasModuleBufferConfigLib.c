/** @file

  The boot services environment configuration library for the Context Buffer PRM module.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PrmConfig.h>
#include <../../PrmRasModule.h>

#include <PrmContextBuffer.h>
#include <PrmDataBuffer.h>

STATIC EFI_HANDLE  mPrmConfigProtocolHandle;

// {06A95D5B-06C8-4004-A55F-230BABCC649A}
STATIC CONST EFI_GUID  mPrmRasModuleGuid = {
  0x06a95d5b, 0x06c8, 0x4004, { 0xa5, 0x5f, 0x23, 0x0b, 0xab, 0xcc, 0x64, 0x9a }
};

// {ad16d36e-1933-480e-9b52-d17de5b4e632}
STATIC CONST EFI_GUID  mPrmRasModuleHandlerGuid = NVIDIA_RAS_PRM_HANDLER_GUID;

/**
  Allocates and populates the static data buffer for this PRM module.

  @param[out] StaticDataBuffer  A pointer to a pointer to the static data buffer.

  @retval EFI_SUCCESS           The static data buffer was allocated and filled successfully.
  @retval EFI_INVALID_PARAMETER The StaticDataBuffer pointer argument is NULL.
  @retval EFI_OUT_OF_RESOURCES  Insufficient memory resources to allocate the static data buffer.

**/
EFI_STATUS
GetStaticDataBuffer (
  OUT PRM_DATA_BUFFER  **StaticDataBuffer
  )
{
  PRM_DATA_BUFFER  *DataBuffer;
  UINTN            DataBufferLength;

  if (StaticDataBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *StaticDataBuffer = NULL;

  //
  // Length of the data buffer = Buffer Header Size + Buffer Data Size
  //
  DataBufferLength = sizeof (PRM_DATA_BUFFER_HEADER) + sizeof (PRM_RAS_MODULE_STATIC_DATA_CONTEXT_BUFFER);

  DataBuffer = AllocateRuntimeZeroPool (DataBufferLength);
  if (DataBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Initialize the data buffer header
  //
  DataBuffer->Header.Signature = PRM_DATA_BUFFER_HEADER_SIGNATURE;
  DataBuffer->Header.Length    = (UINT32)DataBufferLength;

  *StaticDataBuffer = DataBuffer;
  return EFI_SUCCESS;
}

/**
  Constructor of the PRM configuration library.

  @param[in] ImageHandle        The image handle of the driver.
  @param[in] SystemTable        The EFI System Table pointer.

  @retval EFI_SUCCESS           The shell command handlers were installed successfully.
  @retval EFI_UNSUPPORTED       The shell level required was not found.
**/
EFI_STATUS
EFIAPI
PrmRasModuleBufferConfigLibConstructor (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                        Status;
  PRM_CONTEXT_BUFFER                *PrmContextBuffer;
  PRM_DATA_BUFFER                   *StaticDataBuffer;
  PRM_CONFIG_PROTOCOL               *PrmConfigProtocol;
  VOID                              *AcpiParameterBuffer;
  ACPI_PARAMETER_BUFFER_DESCRIPTOR  *AcpiParamBufferDescriptor;

  PrmContextBuffer          = NULL;
  StaticDataBuffer          = NULL;
  PrmConfigProtocol         = NULL;
  AcpiParameterBuffer       = NULL;
  AcpiParamBufferDescriptor = NULL;

  /*
    In this sample PRM module, the protocol describing this sample module's resources is simply
    installed in the constructor.

    However, if some data is not available until later, this constructor could register a callback
    on the dependency for the data to be available (e.g. ability to communicate with some device)
    and then install the protocol. The requirement is that the protocol is installed before end of DXE.
  */

  //
  // Allocate and populate the static data buffer
  //
  Status = GetStaticDataBuffer (&StaticDataBuffer);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status) || (StaticDataBuffer == NULL)) {
    goto Done;
  }

  //
  // Allocate and populate the context buffer
  //

  //
  // This context buffer is not actually used by PRM handler at OS runtime. The OS will allocate
  // the actual context buffer passed to the PRM handler.
  //
  // This context buffer is used internally in the firmware to associate a PRM handler with a
  // a static data buffer and a runtime MMIO ranges array so those can be placed into the
  // PRM_HANDLER_INFORMATION_STRUCT and PRM_MODULE_INFORMATION_STRUCT respectively for the PRM handler.
  //
  PrmContextBuffer = AllocateZeroPool (sizeof (*PrmContextBuffer));
  ASSERT (PrmContextBuffer != NULL);
  if (PrmContextBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  CopyGuid (&PrmContextBuffer->HandlerGuid, &mPrmRasModuleHandlerGuid);
  PrmContextBuffer->Signature        = PRM_CONTEXT_BUFFER_SIGNATURE;
  PrmContextBuffer->Version          = PRM_CONTEXT_BUFFER_INTERFACE_VERSION;
  PrmContextBuffer->StaticDataBuffer = StaticDataBuffer;

  AcpiParameterBuffer = AllocateRuntimeZeroPool (sizeof (UINT32));
  ASSERT (AcpiParameterBuffer != NULL);
  if (AcpiParameterBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  // Allocate the ACPI Parameter Buffer Descriptor structure for a single PRM handler
  AcpiParamBufferDescriptor = AllocateZeroPool (sizeof (*AcpiParamBufferDescriptor));
  ASSERT (AcpiParamBufferDescriptor != NULL);
  if (AcpiParamBufferDescriptor == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  PrmConfigProtocol = AllocateZeroPool (sizeof (*PrmConfigProtocol));
  ASSERT (PrmConfigProtocol != NULL);
  if (PrmConfigProtocol == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  CopyGuid (&PrmConfigProtocol->ModuleContextBuffers.ModuleGuid, &mPrmRasModuleGuid);
  PrmConfigProtocol->ModuleContextBuffers.BufferCount = 1;
  PrmConfigProtocol->ModuleContextBuffers.Buffer      = PrmContextBuffer;

  // Populate the ACPI Parameter Buffer Descriptor structure
  CopyGuid (&AcpiParamBufferDescriptor->HandlerGuid, &mPrmRasModuleHandlerGuid);
  AcpiParamBufferDescriptor->AcpiParameterBufferAddress = (UINT64)(UINTN)AcpiParameterBuffer;

  // Populate the PRM Module Context Buffers structure
  PrmConfigProtocol->ModuleContextBuffers.AcpiParameterBufferDescriptorCount = 1;
  PrmConfigProtocol->ModuleContextBuffers.AcpiParameterBufferDescriptors     = AcpiParamBufferDescriptor;

  //
  // Install the PRM Configuration Protocol for this module. This indicates the configuration
  // library has completed resource initialization for the PRM module.
  //
  Status = gBS->InstallProtocolInterface (
                  &mPrmConfigProtocolHandle,
                  &gPrmConfigProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  (VOID *)PrmConfigProtocol
                  );

Done:
  if (EFI_ERROR (Status)) {
    if (StaticDataBuffer != NULL) {
      FreePool (StaticDataBuffer);
    }

    if (PrmContextBuffer != NULL) {
      FreePool (PrmContextBuffer);
    }

    if (AcpiParameterBuffer != NULL) {
      FreePool (AcpiParameterBuffer);
    }

    if (AcpiParamBufferDescriptor != NULL) {
      FreePool (AcpiParamBufferDescriptor);
    }

    if (PrmConfigProtocol != NULL) {
      FreePool (PrmConfigProtocol);
    }
  }

  return Status;
}
