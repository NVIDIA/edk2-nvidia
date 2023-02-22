/** @file

  ERoT over NS SPI driver

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/ErotQspiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/MctpProtocol.h>
#include <Protocol/QspiController.h>
#include <Protocol/DeviceTreeNode.h>
#include <libfdt.h>

/**
  Check if qspi DT node has erot subnode.

  @param[in]  DeviceTreeBase  DT base pointer.
  @param[in]  QspiOffset      Node offset of Qspi in DT.
  @param[in]  NumChipSelects  Qspi number of chip selects.
  @param[out] ChipSelect      Pointer to return erot chip select.

  @retval BOOLEAN       TRUE if qspi node has erot subnode.

**/
STATIC
BOOLEAN
EFIAPI
ErotQspiNodeHasErot (
  IN CONST VOID  *DeviceTreeBase,
  IN INT32       QspiOffset,
  IN UINT8       NumChipSelects,
  OUT UINT8      *ChipSelect
  )
{
  CONST CHAR8  *NodeName;
  CONST CHAR8  *QspiName;
  CONST VOID   *Property;
  UINT32       NodeChipSelect;
  INT32        SubNode;
  INT32        Length;

  QspiName = fdt_get_name (DeviceTreeBase, QspiOffset, NULL);

  SubNode = 0;
  fdt_for_each_subnode (SubNode, DeviceTreeBase, QspiOffset) {
    NodeName = fdt_get_name (DeviceTreeBase, SubNode, NULL);
    if (AsciiStrnCmp (NodeName, "erot@", AsciiStrLen ("erot@")) == 0) {
      break;
    }
  }
  if (SubNode < 0) {
    DEBUG ((DEBUG_INFO, "%a: no erot on %a\n", __FUNCTION__, QspiName));
    return FALSE;
  }

  Property = fdt_getprop (DeviceTreeBase, SubNode, "status", NULL);
  if ((Property != NULL) && (AsciiStrCmp (Property, "disabled") == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: %a disabled\n", __FUNCTION__, NodeName));
    return FALSE;
  }

  Property = fdt_getprop (DeviceTreeBase, SubNode, "reg", &Length);
  if ((Property != NULL) && (Length == sizeof (UINT32))) {
    NodeChipSelect = (UINT8)fdt32_to_cpu (*(CONST UINT32 *)Property);
    if (NodeChipSelect < NumChipSelects) {
      *ChipSelect = (UINT8)NodeChipSelect;
      DEBUG ((DEBUG_INFO, "%a: %a has %a CS=%u\n", __FUNCTION__, QspiName, NodeName, *ChipSelect));
      return TRUE;
    }
  }

  DEBUG ((DEBUG_ERROR, "%a: %a bad CS\n", __FUNCTION__, NodeName));

  return FALSE;
}

/**
  Check if qspi has erot subnode in DT.

  @param[in]  QspiController  Qspi handle.
  @param[in]  NumChipSelects  Qspi number of chip selects.
  @param[out] ChipSelect      Pointer to return erot chip select.

  @retval BOOLEAN       TRUE if qspi has erot subnode.

**/
STATIC
BOOLEAN
EFIAPI
ErotQspiHasErot (
  IN EFI_HANDLE  QspiController,
  IN UINT8       NumChipSelects,
  OUT UINT8      *ChipSelect,
  OUT UINT8      *Socket

  )
{
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode;
  EFI_STATUS                        Status;
  INT32                             SocketOffset;
  CONST CHAR8                       *SocketName;
  CONST CHAR8                       *SocketPrefix = "socket@";

  DeviceTreeNode = NULL;
  Status         = gBS->HandleProtocol (
                          QspiController,
                          &gNVIDIADeviceTreeNodeProtocolGuid,
                          (VOID **)&DeviceTreeNode
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: DT protocol failed: %r\n", __FUNCTION__, Status));
    return FALSE;
  }

  if (!ErotQspiNodeHasErot (
         DeviceTreeNode->DeviceTreeBase,
         DeviceTreeNode->NodeOffset,
         NumChipSelects,
         ChipSelect
         ))
  {
    return FALSE;
  }

  SocketOffset = fdt_parent_offset (
                   DeviceTreeNode->DeviceTreeBase,
                   DeviceTreeNode->NodeOffset
                   );
  if (SocketOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: no socket parent\n", __FUNCTION__));
    return FALSE;
  }

  SocketName = fdt_get_name (DeviceTreeNode->DeviceTreeBase, SocketOffset, NULL);
  if ((AsciiStrnCmp (SocketName, SocketPrefix, AsciiStrLen (SocketPrefix)) != 0) ||
      (AsciiStrLen (SocketName) != AsciiStrLen (SocketPrefix) + 1))
  {
    DEBUG ((DEBUG_ERROR, "%a: bad socket %a\n", __FUNCTION__, SocketName));
    return FALSE;
  }

  *Socket = SocketName[AsciiStrLen (SocketPrefix)] - '0';

  DEBUG ((DEBUG_INFO, "%a: returning socket=%u\n", __FUNCTION__, *Socket));

  return TRUE;
}

/**
  Entry point of this driver.

  @param[in] ImageHandle  Image handle this driver.
  @param[in] SystemTable  Pointer to the System Table.

  @retval EFI_SUCCESS     The entry point is executed successfully.
  @retval other           Some error occurred when executing this entry point.

**/
EFI_STATUS
EFIAPI
ErotQspiDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EROT_QSPI_PRIVATE_DATA           *Private;
  EFI_STATUS                       Status;
  EFI_STATUS                       ReturnStatus;
  UINTN                            Index;
  UINTN                            NumHandles;
  EFI_HANDLE                       *HandleBuffer;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *Qspi;
  UINT8                            ChipSelect;
  UINT8                            NumChipSelects;
  UINT8                            Socket;

  HandleBuffer = NULL;
  Status       = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gNVIDIAQspiControllerProtocolGuid,
                        NULL,
                        &NumHandles,
                        &HandleBuffer
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error locating QSPI handles: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = ErotQspiLibInit (NumHandles);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Couldn't initialize ErotQspi Lib: %r\n", __FUNCTION__, Status));
    goto Done;
  }

  for (Index = 0; Index < NumHandles; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gNVIDIAQspiControllerProtocolGuid,
                    (VOID **)&Qspi
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: HandleProtocol for index %u failed: %r\n", __FUNCTION__, Index, Status));
      goto Done;
    }

    Qspi->GetNumChipSelects (Qspi, &NumChipSelects);
    if (!ErotQspiHasErot (HandleBuffer[Index], NumChipSelects, &ChipSelect, &Socket)) {
      continue;
    }

    // TODO: need to get erot gpio pin from DT for NS erot support
    Status = ErotQspiAddErot (Qspi, ChipSelect, Socket, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: AddErot for index %u failed: %r\n", __FUNCTION__, Index, Status));
      goto Done;
    }
  }

  if (mNumErotQspis == 0) {
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  Private = mPrivate;
  for (Index = 0; Index < mNumErotQspis; Index++, Private++) {
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Private->Handle,
                    &gNVIDIAMctpProtocolGuid,
                    &Private->Protocol,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: protocol install for index %u failed: %r\n", __FUNCTION__, Index, Status));
      goto Done;
    }
  }

  DEBUG ((DEBUG_INFO, "%a: Found %u Erots\n", __FUNCTION__, mNumErotQspis));

Done:
  FreePool (HandleBuffer);

  ReturnStatus = Status;
  if (EFI_ERROR (ReturnStatus)) {
    Private = mPrivate;
    for (Index = 0; Index < mNumErotQspis; Index++, Private++) {
      if (Private->Handle != NULL) {
        Status = gBS->UninstallMultipleProtocolInterfaces (
                        Private->Handle,
                        &gNVIDIAMctpProtocolGuid,
                        &Private->Protocol,
                        NULL
                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: uninstall for index %u failed: %r\n", __FUNCTION__, Index, Status));
        }
      }
    }

    ErotQspiLibDeinit ();
  }

  return ReturnStatus;
}
