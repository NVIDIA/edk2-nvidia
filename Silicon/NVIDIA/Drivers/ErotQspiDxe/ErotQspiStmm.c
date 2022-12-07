/** @file

  ERoT over Stmm QSPI driver

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/ErotQspiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MmServicesTableLib.h>
#include <Library/StandaloneMmOpteeDeviceMem.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/TimerLib.h>
#include <Protocol/MctpProtocol.h>
#include <Protocol/QspiController.h>

#define EROT_QSPI_STMM_MAX_EROTS  4

/**
  Entry point of this driver.

  @param[in]  ImageHandle     Image handle of this driver.
  @param[in]  MmSystemTable   Pointer to the System Table

**/
EFI_STATUS
EFIAPI
ErotQspiStmmInitialize (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *MmSystemTable
  )
{
  EFI_STATUS                       Status;
  UINTN                            Index;
  UINTN                            NumHandles;
  UINTN                            HandleBufferSize;
  EFI_HANDLE                       HandleBuffer[EROT_QSPI_STMM_MAX_EROTS];
  EROT_QSPI_PRIVATE_DATA           *Private;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *Qspi;
  UINT8                            ChipSelect;
  UINT8                            Socket;
  UINT32                           *SocketIdProtocol;
  TEGRA_PLATFORM_TYPE              PlatformType;

  PlatformType = GetPlatformTypeMm ();
  if (PlatformType == TEGRA_PLATFORM_VDK) {
    return EFI_UNSUPPORTED;
  }

  HandleBufferSize = sizeof (HandleBuffer);
  Status           = gMmst->MmLocateHandle (
                              ByProtocol,
                              &gNVIDIAQspiControllerProtocolGuid,
                              NULL,
                              &HandleBufferSize,
                              HandleBuffer
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error locating QSPI handles: %r\n", Status));
    return Status;
  }

  NumHandles = HandleBufferSize / sizeof (EFI_HANDLE);

  Status = ErotQspiLibInit (NumHandles);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: lib init failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  for (Index = 0; Index < NumHandles; Index++) {
    Status = gMmst->MmHandleProtocol (
                      HandleBuffer[Index],
                      &gNVIDIAQspiControllerProtocolGuid,
                      (VOID **)&Qspi
                      );
    if (EFI_ERROR (Status) || (Qspi == NULL)) {
      DEBUG ((DEBUG_ERROR, "Failed to get qspi for index %u: %r\n", Index, Status));
      continue;
    }

    SocketIdProtocol = NULL;
    Status           = gMmst->MmHandleProtocol (
                                HandleBuffer[Index],
                                &gNVIDIASocketIdProtocolGuid,
                                (VOID **)&SocketIdProtocol
                                );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: SocketId failed for index %u: %r\n", __FUNCTION__, Index, Status));
      continue;
    }

    ChipSelect = EROT_QSPI_CHIP_SELECT_DEFAULT;
    Socket     = (UINT8)(*SocketIdProtocol);
    Status     = ErotQspiAddErot (Qspi, ChipSelect, Socket);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to add device for handle index %u: %r\n", __FUNCTION__, Index, Status));
      continue;
    }
  }

  if (mNumErotQspis == 0) {
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  Private = mPrivate;
  for (Index = 0; Index < mNumErotQspis; Index++, Private++) {
    Status = gMmst->MmInstallProtocolInterface (
                      &Private->Handle,
                      &gNVIDIAMctpProtocolGuid,
                      EFI_NATIVE_INTERFACE,
                      &Private->Protocol
                      );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: protocol install failed, index %u: %r\n", __FUNCTION__, Index, Status));
      continue;
    }
  }

  DEBUG ((DEBUG_INFO, "%a: Found %u Erots\n", __FUNCTION__, mNumErotQspis));

  Status = EFI_SUCCESS;

Done:
  if (EFI_ERROR (Status)) {
    ErotQspiLibDeinit ();
  }

  return Status;
}
