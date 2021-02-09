/** @file
 *  Golden Register Dxe
 *
 *  Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/GoldenRegisterLib.h>

#include <Protocol/KernelCmdLineUpdate.h>

STATIC
CHAR16 mGrNewCommandLineArgument[GR_CMD_MAX_LEN];
STATIC
NVIDIA_KERNEL_CMD_LINE_UPDATE_PROTOCOL mGrCmdLine;

STATIC
VOID
  EFIAPI
OnExitBootServices (
    IN EFI_EVENT  Event,
    IN VOID       *Context
    )
{
  GOLDEN_REGISTER_PRIVATE_DATA *Private;
  UINT32                       NumAddresses;
  GR_DATA_HEADER               *DataHeader;
  UINT32                       Count;
  GR_DATA                      *GrData;

  gBS->CloseEvent (Event);

  Private = (GOLDEN_REGISTER_PRIVATE_DATA *)Context;
  NumAddresses = Private->Size / sizeof (UINT32);

  DataHeader = (GR_DATA_HEADER *)Private->GrOutBase;

  GrData = (GR_DATA *)(Private->GrOutBase + sizeof (GR_DATA_HEADER) + DataHeader->Mb1Size + DataHeader->Mb2Size);

  for (Count = 0; Count < NumAddresses; Count++) {
    GrData->Address = Private->Address[Count];
    GrData->Data = *(UINT32*)(UINTN)(Private->Address[Count]);
    DEBUG ((DEBUG_INFO, "UEFI GR Dump: Address: 0x%x Data: 0x%x\n", GrData->Address, GrData->Data));
    GrData++;
  }

  DataHeader->UefiOffset = DataHeader->Mb2Offset + DataHeader->Mb2Size;
  DataHeader->UefiSize = NumAddresses * sizeof (GR_DATA);

  return;
}

EFI_STATUS
EFIAPI
GoldenRegisterDxeInitialize (
  IN EFI_HANDLE               ImageHandle,
  IN EFI_SYSTEM_TABLE         *SystemTable
)
{
  EFI_STATUS                   Status;
  EFI_EVENT                    ExitBootServicesEvent;
  UINT64                       GrBlobBase;
  UINT32                       Offset;
  UINT32                       Size;
  UINTN                        GrOutBase;
  UINTN                        GrOutSize;
  GOLDEN_REGISTER_PRIVATE_DATA *Private;
  UINT32                       Count;
  EFI_HANDLE                   Handle;

  GrBlobBase = GetGRBlobBaseAddress ();
  if (GrBlobBase == 0) {
    return EFI_NOT_FOUND;
  }

  Status = ValidateGrBlobHeader (GrBlobBase);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "Failed to validate GR blob header\n"));
    return Status;
  }

  Offset = 0;
  Size = 0;
  Status = LocateGrBlobBinary (GrBlobBase, &Offset, &Size);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to locate UEFI GR blob\n"));
    return Status;
  }
  if (Size == 0) {
    DEBUG ((DEBUG_ERROR, "Invalid size of UEFI GR blob\n"));
    return EFI_NOT_FOUND;
  }

  if (!GetGROutputBaseAndSize(&GrOutBase, &GrOutSize)) {
    DEBUG ((DEBUG_ERROR, "Failed to get parameters of UEFI GR output\n"));
    return EFI_NOT_FOUND;
  }
  if (GrOutBase == 0 ||
      GrOutSize == 0) {
    DEBUG ((DEBUG_ERROR, "Invalid parameters of UEFI GR output\n"));
    return EFI_NOT_FOUND;
  }

  Private = NULL;
  Status = gBS->AllocatePool (EfiBootServicesData,
                              sizeof (GOLDEN_REGISTER_PRIVATE_DATA),
                              (VOID **)&Private);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Private->GrBlobBase = GrBlobBase;
  Private->Offset = Offset;
  Private->Size = Size;
  Private->GrOutBase = GrOutBase;
  Private->GrOutSize = GrOutSize;
  Private->Address = NULL;

  Status = gBS->AllocatePool (EfiBootServicesData,
                              2 * Size,
                              (VOID **)&Private->Address);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  for (Count = 0; Count < Size / sizeof (UINT32); Count++) {
    Private->Address[Count] = *(UINT32 *)(Private->GrBlobBase + Private->Offset + (Count * sizeof (UINT32)));
    Status = gDS->AddMemorySpace (EfiGcdMemoryTypeMemoryMappedIo,
                                  (EFI_PHYSICAL_ADDRESS)Private->Address[Count] & ~EFI_PAGE_MASK,
                                  SIZE_4KB,
                                  EFI_MEMORY_UC | EFI_MEMORY_RO);
    if (Status != EFI_ACCESS_DENIED &&
        EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to add address to memory space\n"));
      goto ErrorExit;
    }

    Status = gDS->SetMemorySpaceAttributes ((EFI_PHYSICAL_ADDRESS)Private->Address[Count] & ~EFI_PAGE_MASK,
                                            SIZE_4KB,
                                            EFI_MEMORY_UC | EFI_MEMORY_RO);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to set address memory attributes\n"));
      goto ErrorExit;
    }
  }

  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL,
                               TPL_NOTIFY,
                               OnExitBootServices,
                               Private,
                               &gEfiEventExitBootServicesGuid,
                               &ExitBootServicesEvent);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Handle = NULL;
  mGrCmdLine.ExistingCommandLineArgument = NULL;
  UnicodeSPrintAsciiFormat (mGrNewCommandLineArgument,
                            GR_CMD_MAX_LEN,
                            "bl_debug_data=%lu@0x%lx",
                            Private->GrOutSize,
                            Private->GrOutBase);
  mGrCmdLine.NewCommandLineArgument = mGrNewCommandLineArgument;
  Status = gBS->InstallMultipleProtocolInterfaces (&Handle,
                                                   &gNVIDIAKernelCmdLineUpdateGuid,
                                                   &mGrCmdLine,
                                                   NULL);

ErrorExit:
  if (EFI_ERROR (Status)) {
    if (Private != NULL) {
      if (Private->Address != NULL) {
        gBS->FreePool (Private->Address);
      }
      gBS->FreePool (Private);
    }
  }

  return Status;
}
