/** @file

  NV Display Controller Driver - MMIO regions

  SPDX-FileCopyrightText: Copyright (c) 2024-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/DeviceTreeNode.h>

#include "NvDisplay.h"

#pragma pack (push, 1)
typedef struct {
  UINT32    Offset;
  UINT32    Size;
} NV_FB_SUB_CARVEOUT_INFO;

typedef struct {
  UINT32                     Signature;
  UINT32                     Version;
  UINT8                      Digest[64];
  UINT32                     SubCarveoutCount;
  NV_FB_SUB_CARVEOUT_INFO    SubCarveouts[8];
  UINT8                      Reserved[116];
} NV_FB_CARVEOUT_HEADER;

///
/// Framebuffer carveout header signature.
///
#define NV_FB_CARVEOUT_HEADER_SIGNATURE  SIGNATURE_32 ('D', 'C', 'D', 'T')
///
/// Framebuffer carveout header version.
///
#define NV_FB_CARVEOUT_HEADER_VERSION  1

///
/// Index of the early framebuffer sub-carveout.
///
#define NV_FB_SUB_CARVEOUT_INDEX_EARLY_FB  0
///
/// Index of the DCE DTB sub-carveout.
///
#define NV_FB_SUB_CARVEOUT_INDEX_DCE_DTB  1

STATIC_ASSERT (
  sizeof (NV_FB_CARVEOUT_HEADER) == 256,
  "NV_FB_CARVEOUT_HEADER is expected to be exactly 256 bytes long."
  );

typedef struct {
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR    Registers;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR    Framebuffer;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR    Dpaux;
  EFI_ACPI_END_TAG_DESCRIPTOR          End;
} NV_DISPLAY_MMIO_REGIONS;
#pragma pack (pop)

/**
  Initializes ACPI address space descriptor.

  @param[out] Desc  Address space descriptor.
  @param[in]  Base  Address space base.
  @param[in]  Size  Address space size.
*/
STATIC
VOID
InitializeAcpiAddressSpaceDescriptor (
  OUT EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *CONST  Desc,
  IN  CONST EFI_PHYSICAL_ADDRESS                Base,
  IN  CONST UINT64                              Size
  )
{
  ZeroMem (Desc, sizeof (*Desc));

  Desc->Desc                 = ACPI_ADDRESS_SPACE_DESCRIPTOR;
  Desc->Len                  = sizeof (*Desc) - 3;
  Desc->AddrRangeMin         = Base;
  Desc->AddrLen              = Size;
  Desc->AddrRangeMax         = Base + Size - 1;
  Desc->ResType              = ACPI_ADDRESS_SPACE_TYPE_MEM;
  Desc->AddrSpaceGranularity = Base + Size > SIZE_4GB ? 64 : 32;
}

/**
  Initialized ACPI end tag descriptor.

  @param[out] Desc  End tag descriptor
*/
STATIC
VOID
InitializeAcpiEndTagDescriptor (
  OUT EFI_ACPI_END_TAG_DESCRIPTOR *CONST  Desc
  )
{
  ZeroMem (Desc, sizeof (*Desc));

  Desc->Desc = ACPI_END_TAG_DESCRIPTOR;
}

/**
  Initializes ACPI framebuffer descriptor.

  @param[out] Desc  Framebuffer descriptor.

  @retval EFI_SUCCESS    Descriptor successfully initialized.
  @retval EFI_NOT_FOUND  No framebuffer region was not found.
*/
STATIC
EFI_STATUS
InitializeAcpiFramebufferDescriptor (
  OUT EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *CONST  Desc
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  Base;
  UINTN                 Size;

  Status = NvDisplayGetFramebufferRegion (&Base, &Size);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  InitializeAcpiAddressSpaceDescriptor (Desc, Base, Size);
  return EFI_SUCCESS;
}

/**
  Initializes ACPI DT registers descriptor.

  @param[out] Desc            DT registers descriptor.
  @param[in]  Registers       DT registers regions.
  @param[in]  RegistersCount  Number of DT registers regions.
  @param[in]  RegistersName   Name of the DT registers region.

  @retval EFI_SUCCESS    Descriptor successfully initialized.
  @retval EFI_NOT_FOUND  No DT registers region with the specified name was found.
*/
STATIC
EFI_STATUS
InitializeAcpiDtRegistersDescriptor (
  OUT EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR      *CONST  Desc,
  IN  CONST NVIDIA_DEVICE_TREE_REGISTER_DATA *CONST  Registers,
  IN  CONST UINTN                                    RegistersCount,
  IN  CONST CHAR8                            *CONST  RegistersName
  )
{
  UINTN                                   Index;
  CONST NVIDIA_DEVICE_TREE_REGISTER_DATA  *Reg;

  for (Index = 0; Index < RegistersCount; ++Index) {
    Reg = &Registers[Index];

    if (AsciiStrCmp (Reg->Name, RegistersName) == 0) {
      InitializeAcpiAddressSpaceDescriptor (Desc, Reg->BaseAddress, Reg->Size);
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Retrieve start/end address of a framebuffer sub-carveout.

  @param[in]  FbBase        Base of the framebuffer carveout.
  @param[in]  FbSize        Size of the framebuffer carveout.
  @param[in]  Index         Index of the sub-carveout.
  @param[out] StartAddress  Start address of the sub-carveout.
  @param[out] EndAddress    End address of the sub-carveout.

  @retval EFI_SUCCESS            Sub-carveout successfully retrieved.
  @retval EFI_UNSUPPORTED        Framebuffer carveout too small for header.
  @retval EFI_UNSUPPORTED        Framebuffer carveout header signature invalid.
  @retval EFI_NOT_FOUND          Framebuffer carveout header version unknown.
  @retval EFI_NOT_FOUND          Specified sub-carveout does not exist.
  @retval EFI_INVALID_PARAMETER  Number of sub-carveouts is too large.
  @retval EFI_INVALID_PARAMETER  The sub-carveout is not fully contained within
                                 the framebuffer carveout.
*/
STATIC
EFI_STATUS
GetFbSubCarveout (
  IN  CONST EFI_PHYSICAL_ADDRESS   FbBase,
  IN  CONST UINTN                  FbSize,
  IN  CONST UINTN                  Index,
  OUT EFI_PHYSICAL_ADDRESS *CONST  StartAddress,
  OUT EFI_PHYSICAL_ADDRESS *CONST  EndAddress
  )
{
  NV_FB_CARVEOUT_HEADER    *Hdr;
  NV_FB_SUB_CARVEOUT_INFO  *SubCarveout;

  if (!(FbSize >= sizeof (*Hdr))) {
    return EFI_UNSUPPORTED;
  }

  Hdr = (NV_FB_CARVEOUT_HEADER *)FbBase;
  if (Hdr->Signature != NV_FB_CARVEOUT_HEADER_SIGNATURE) {
    return EFI_UNSUPPORTED;
  }

  if (Hdr->Version != NV_FB_CARVEOUT_HEADER_VERSION) {
    return EFI_NOT_FOUND;
  }

  if (!(Hdr->SubCarveoutCount <= ARRAY_SIZE (Hdr->SubCarveouts))) {
    return EFI_INVALID_PARAMETER;
  } else if (!(Index < Hdr->SubCarveoutCount)) {
    return EFI_NOT_FOUND;
  }

  SubCarveout = &Hdr->SubCarveouts[Index];
  if (!(  (SubCarveout->Offset <= FbSize)
       && (SubCarveout->Size <= FbSize - SubCarveout->Offset)))
  {
    return EFI_INVALID_PARAMETER;
  }

  *StartAddress = FbBase + SubCarveout->Offset;
  *EndAddress   = FbBase + SubCarveout->Offset + SubCarveout->Size;
  return EFI_SUCCESS;
}

/**
  Retrieves base and size of the framebuffer region.

  @param[out] Base  Base of the framebuffer region.
  @param[out] Size  Size of the framebuffer region.

  @retval EFI_SUCCESS       Region details retrieved successfully.
  @retval EFI_NOT_FOUND     The framebuffer region was not found.
  @retval EFI_DEVICE_ERROR  The framebuffer region was invalid.
*/
EFI_STATUS
NvDisplayGetFramebufferRegion (
  OUT EFI_PHYSICAL_ADDRESS *CONST  Base,
  OUT UINTN                *CONST  Size
  )
{
  EFI_STATUS                    Status;
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;
  TEGRA_BASE_AND_SIZE_INFO      *FbInfo;
  EFI_PHYSICAL_ADDRESS          StartAddress, EndAddress;
  CONST UINTN                   Alignment = RUNTIME_PAGE_ALLOCATION_GRANULARITY;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob == NULL) || (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (*PlatformResourceInfo))) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to retrieve platform resource information\r\n",
      __FUNCTION__
      ));
    return EFI_NOT_FOUND;
  }

  PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  FbInfo               = &PlatformResourceInfo->FrameBufferInfo;

  if ((FbInfo->Base == 0) || (FbInfo->Size == 0)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: no framebuffer region present\r\n",
      __FUNCTION__
      ));
    return EFI_NOT_FOUND;
  }

  Status = GetFbSubCarveout (
             FbInfo->Base,
             FbInfo->Size,
             NV_FB_SUB_CARVEOUT_INDEX_EARLY_FB,
             &StartAddress,
             &EndAddress
             );
  if (Status == EFI_UNSUPPORTED) {
    /* Sub-carveouts not supported, fall back to using the entire
       framebuffer region. */
    StartAddress = (EFI_PHYSICAL_ADDRESS)FbInfo->Base;
    EndAddress   = StartAddress + FbInfo->Size;
  } else if (Status == EFI_NOT_FOUND) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: no framebuffer sub-carveout present\r\n",
      __FUNCTION__
      ));
    return EFI_NOT_FOUND;
  } else if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to retrieve framebuffer sub-carveout: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return EFI_DEVICE_ERROR;
  }

  StartAddress = ALIGN_VALUE (StartAddress, Alignment);
  EndAddress   = EndAddress & ~(Alignment - 1);

  if (!(StartAddress < EndAddress)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: invalid framebuffer region: 0x%016lx:0x%016lx\r\n",
      __FUNCTION__,
      StartAddress,
      EndAddress
      ));
    return EFI_DEVICE_ERROR;
  }

  *Base = StartAddress;
  *Size = EndAddress - StartAddress;

  return EFI_SUCCESS;
}

/**
  Retrieve address space descriptors of the NV display MMIO regions.

  On call, *Size must be the size of avaliable memory pointed to by
  Desc; if Desc is NULL, *Size must be 0.

  On return, *Size will contain the minimum size required for the
  descriptors.

  @param[in]     DriverHandle      Driver handle.
  @param[in]     ControllerHandle  ControllerHandle.
  @param[out]    Desc              Address space descriptors.
  @param[in,out] Size              Size of the descriptors.

  @retval EFI_SUCCESS            Operation successful.
  @retval EFI_INVALID_PARAMETER  Size is NULL.
  @retval EFI_INVALID_PARAMETER  Desc is NULL, but *Size is non-zero.
  @retval EFI_BUFFER_TOO_SMALL   Desc is not NULL, but *Size is too small.
  @retval EFI_OUT_OF_RESOURCES   Memory allocation failed.
  @retval EFI_NOT_FOUND          At least one display MMIO region was not found.
  @retval !=EFI_SUCCESS          Operation failed.
*/
EFI_STATUS
NvDisplayGetMmioRegions (
  IN  CONST EFI_HANDLE                          DriverHandle,
  IN  CONST EFI_HANDLE                          ControllerHandle,
  OUT EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *CONST  Desc  OPTIONAL,
  OUT UINTN                             *CONST  Size
  )
{
  EFI_STATUS                        Status;
  NV_DISPLAY_MMIO_REGIONS           *Regions;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DtNode;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  *DtRegs = NULL;
  UINT32                            DtRegCount;

  if ((Size == NULL) || ((Desc == NULL) && (*Size > 0))) {
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  if ((Desc != NULL) && (*Size < sizeof (*Regions))) {
    Status = EFI_BUFFER_TOO_SMALL;
    goto Exit;
  }

  *Size = sizeof (*Regions);

  if (Desc != NULL) {
    Regions = (NV_DISPLAY_MMIO_REGIONS *)Desc;

    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gNVIDIADeviceTreeNodeProtocolGuid,
                    (VOID **)&DtNode,
                    DriverHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: could not retrieve DT node protocol: %r\r\n",
        __FUNCTION__,
        Status
        ));
      goto Exit;
    }

    DtRegCount = 0;
    Status     = DeviceTreeGetRegisters (DtNode->NodeOffset, DtRegs, &DtRegCount);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      DtRegs = (NVIDIA_DEVICE_TREE_REGISTER_DATA *)AllocatePool (DtRegCount * sizeof (*DtRegs));
      if (DtRegs == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      Status = DeviceTreeGetRegisters (DtNode->NodeOffset, DtRegs, &DtRegCount);
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: failed to read DT registers: %r\r\n", __FUNCTION__, Status));
      goto Exit;
    }

    Status = InitializeAcpiDtRegistersDescriptor (&Regions->Registers, DtRegs, DtRegCount, "nvdisplay");
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: failed to initialize 'nvdisplay' descriptor: %r\r\n", __FUNCTION__, Status));
      goto Exit;
    }

    Status = InitializeAcpiFramebufferDescriptor (&Regions->Framebuffer);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: failed to initialize framebuffer descriptor: %r\r\n", __FUNCTION__, Status));
      goto Exit;
    }

    Status = InitializeAcpiDtRegistersDescriptor (&Regions->Dpaux, DtRegs, DtRegCount, "dpaux0");
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: failed to initialize 'dpaux0' descriptor: %r\r\n", __FUNCTION__, Status));
      goto Exit;
    }

    InitializeAcpiEndTagDescriptor (&Regions->End);
  }

  Status = EFI_SUCCESS;

Exit:
  if (DtRegs != NULL) {
    FreePool (DtRegs);
  }

  return Status;
}
