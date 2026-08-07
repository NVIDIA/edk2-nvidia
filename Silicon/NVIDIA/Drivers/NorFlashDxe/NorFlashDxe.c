/** @file

  NOR Flash Driver

  SPDX-FileCopyrightText: Copyright (c) 2018-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <NorFlashPrivate.h>

EFI_BLOCK_IO_MEDIA  Media = {
  0,         // Media ID gets updated during Start
  FALSE,     // Non removable media
  TRUE,      // Media currently present
  0,         // First logical block
  FALSE,     // Not read only
  FALSE,     // Does not cache write data
  SIZE_64KB, // Block size gets updated during start
  4,         // Alignment required
  0          // Last logical block gets updated during start
};

#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH    Vendor;
  UINT8                 FlashIndex;
} NOR_FLASH_DEVICE_PATH;
#pragma pack()

NOR_FLASH_DEVICE_PATH  mNorFlashDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (NOR_FLASH_DEVICE_PATH)),
        (UINT8)(sizeof (NOR_FLASH_DEVICE_PATH) >> 8)
      }
    },
    { 0x8332de7f, 0x50c3, 0x47ca, { 0x82, 0x4e, 0x83, 0x3a, 0xac, 0x7c, 0xf1, 0x6d }
    }
  },
  0
};

/**
  Erase data from NOR Flash.

  @param[in]       This            Instance to protocol
  @param[in]       MediaId         Media ID for the device
  @param[in]       LBA             Logical block to start erasing from
  @param[in, out]  Token           A pointer to the token associated with the
                                   transaction.
  @param[in]       Size            Number of bytes to be erased


  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
NorFlashEraseBlock (
  IN     EFI_ERASE_BLOCK_PROTOCOL  *This,
  IN     UINT32                    MediaId,
  IN     EFI_LBA                   LBA,
  IN OUT EFI_ERASE_BLOCK_TOKEN     *Token,
  IN     UINTN                     Size
  )
{
  EFI_STATUS              Status;
  NOR_FLASH_PRIVATE_DATA  *Private;

  if ((This == NULL) ||
      (Token == NULL) ||
      (Size == 0))
  {
    return EFI_INVALID_PARAMETER;
  }

  Private = NOR_FLASH_PRIVATE_DATA_FROM_ERASE_BLOCK_PROTOCOL (This);

  if (MediaId != Private->FlashInstance) {
    return EFI_MEDIA_CHANGED;
  }

  Status = NorFlashErase (
             &Private->NorFlashProtocol,
             LBA,
             Size / Private->PrivateFlashAttributes.FlashAttributes.BlockSize,
             FALSE
             );

  if (Token->Event != NULL) {
    Token->TransactionStatus = Status;
    Status                   = EFI_SUCCESS;
    gBS->SignalEvent (Token->Event);
  }

  return Status;
}

/**
  Check if DT node is a valid supported flash device and optionally return
  its chip select.

  @param[in]  DeviceTreeBase       Pointer to DT
  @param[in]  NodeOffset           Offset of DT flash device subnode
  @param[in]  NumChipSelects       Number of chip selects QSPI supports
  @param[out] ChipSelect           Pointer to save chip select, if not NULL

  @retval BOOLEAN                  Flag indicating if DT node is valid flash

**/
STATIC
BOOLEAN
EFIAPI
IsValidFlashNode (
  IN VOID    *DeviceTreeBase,
  IN INT32   NodeOffset,
  IN UINT8   NumChipSelects,
  OUT UINT8  *ChipSelect         OPTIONAL
  )
{
  CONST CHAR8  *NodeName;
  CONST VOID   *Property;
  INT32        Offset;
  INT32        Length;
  BOOLEAN      IsFlash;
  BOOLEAN      IsDisabled;
  BOOLEAN      IsValidCS;
  UINT32       NodeChipSelect;

  NodeName = FdtGetName (DeviceTreeBase, NodeOffset, NULL);

  NodeChipSelect = MAX_UINT32;
  IsDisabled     = FALSE;
  IsFlash        = FALSE;
  IsValidCS      = FALSE;
  if (AsciiStrnCmp (NodeName, "flash@", AsciiStrLen ("flash@")) == 0) {
    IsFlash = TRUE;
  } else if (AsciiStrnCmp (NodeName, "spiflash@", AsciiStrLen ("spiflash@")) == 0) {
    Offset = FdtSubnodeOffset (
               DeviceTreeBase,
               NodeOffset,
               "partition@0"
               );
    if (Offset >= 0) {
      Property = FdtGetProp (DeviceTreeBase, Offset, "label", &Length);
      if ((Property != NULL) && (Length != 0)) {
        if (AsciiStrStr (Property, "flash") != NULL) {
          IsFlash = TRUE;
        }
      }
    }
  }

  Property = FdtGetProp (DeviceTreeBase, NodeOffset, "status", NULL);
  if ((Property != NULL) && (AsciiStrCmp (Property, "disabled") == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: %a disabled\n", __FUNCTION__, NodeName));
    IsDisabled = TRUE;
  }

  if (IsFlash && !IsDisabled) {
    Property = FdtGetProp (
                 DeviceTreeBase,
                 NodeOffset,
                 "reg",
                 &Length
                 );
    if ((Property != NULL) && (Length == sizeof (UINT32))) {
      NodeChipSelect = (UINT8)Fdt32ToCpu (*(CONST UINT32 *)Property);
      if (NodeChipSelect < NumChipSelects) {
        IsValidCS = TRUE;
        if (ChipSelect != NULL) {
          *ChipSelect = (UINT8)NodeChipSelect;
        }
      }
    }
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: %a(0x%x) Flash=%u Disabled=%u CS Valid=%u CS=%u\n",
    __FUNCTION__,
    NodeName,
    NodeOffset,
    IsFlash,
    IsDisabled,
    IsValidCS,
    NodeChipSelect
    ));

  return (IsFlash && !IsDisabled && IsValidCS);
}

/**
  Check for flash part in device tree.

  Looks through all subnodes of the QSPI node to see if any of them has
  spiflash subnode.

  @param[in]   Controller          The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]   NumChipSelects      Number of Qspi chip selects.

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
**/
EFI_STATUS
CheckNorFlashCompatibility (
  IN EFI_HANDLE  Controller,
  IN UINT8       NumChipSelects
  )
{
  EFI_STATUS                        Status;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode;
  INT32                             SubNode;

  // Check whether device tree node protocol is available.
  DeviceTreeNode = NULL;
  Status         = gBS->HandleProtocol (
                          Controller,
                          &gNVIDIADeviceTreeNodeProtocolGuid,
                          (VOID **)&DeviceTreeNode
                          );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SubNode = 0;
  FdtForEachSubnode (SubNode, DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset) {
    if (IsValidFlashNode (DeviceTreeNode->DeviceTreeBase, SubNode, NumChipSelects, NULL)) {
      return EFI_SUCCESS;
    }
  }

  return EFI_UNSUPPORTED;
}

/**
  Fixup internal data so that EFI can be call in virtual mode.
  Call the passed in Child Notify event and convert any pointers in
  lib to virtual mode.

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
VOID
EFIAPI
NorVirtualNotifyEvent (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  NOR_FLASH_PRIVATE_DATA  *Private;

  Private = (NOR_FLASH_PRIVATE_DATA *)Context;
  EfiConvertPointer (0x0, (VOID **)&Private->CommandBuffer);
  EfiConvertPointer (0x0, (VOID **)&Private->QspiController->PerformTransaction);
  EfiConvertPointer (0x0, (VOID **)&Private->QspiController);
  return;
}

/**
  Tests to see if this driver supports a given controller.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by the driver
                                   specified by This.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the driver specified by This.
**/
EFI_STATUS
EFIAPI
NorFlashDxeDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                       Status;
  EFI_STATUS                       CompatibilityStatus;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL  *QspiInstance;
  UINT8                            NumChipSelects;

  // Check whether driver has already been started.
  QspiInstance = NULL;
  Status       = gBS->OpenProtocol (
                        Controller,
                        &gNVIDIAQspiControllerProtocolGuid,
                        (VOID **)&QspiInstance,
                        This->DriverBindingHandle,
                        Controller,
                        EFI_OPEN_PROTOCOL_BY_DRIVER
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = QspiInstance->GetNumChipSelects (QspiInstance, &NumChipSelects);
  ASSERT_EFI_ERROR (Status);

  CompatibilityStatus = CheckNorFlashCompatibility (Controller, NumChipSelects);

  Status = gBS->CloseProtocol (
                  Controller,
                  &gNVIDIAQspiControllerProtocolGuid,
                  This->DriverBindingHandle,
                  Controller
                  );
  ASSERT_EFI_ERROR (Status);

  return CompatibilityStatus;
}

/**
  Starts a device controller or a bus controller.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to start.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a device error.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.
  @retval Others                   The driver failded to start the device.
**/
EFI_STATUS
EFIAPI
NorFlashDxeDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                        Status;
  NOR_FLASH_PRIVATE_DATA            *Private;
  NVIDIA_QSPI_CONTROLLER_PROTOCOL   *QspiInstance;
  UINT64                            ClockSpeed;
  EFI_DEVICE_PATH_PROTOCOL          *ParentDevicePath;
  EFI_DEVICE_PATH_PROTOCOL          *NorFlashDevicePath;
  VOID                              *Interface;
  BOOLEAN                           QspiClockSupport;
  UINT8                             NumChipSelects;
  UINT8                             ChipSelect;
  INT32                             SubNode;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode;
  UINTN                             FlashIndex;
  UINTN                             NumInitialized;

  FlashIndex     = 0;
  NumInitialized = 0;

  // Open Qspi Controller Protocol
  QspiInstance = NULL;
  Status       = gBS->OpenProtocol (
                        Controller,
                        &gNVIDIAQspiControllerProtocolGuid,
                        (VOID **)&QspiInstance,
                        This->DriverBindingHandle,
                        Controller,
                        EFI_OPEN_PROTOCOL_BY_DRIVER
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to open QSPI Protocol\n", __FUNCTION__));
    goto DriverErrorExit;
  }

  QspiInstance->GetNumChipSelects (QspiInstance, &NumChipSelects);

  // Get device tree node protocol for Qspi
  DeviceTreeNode = NULL;
  Status         = gBS->HandleProtocol (
                          Controller,
                          &gNVIDIADeviceTreeNodeProtocolGuid,
                          (VOID **)&DeviceTreeNode
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: DT protocol failed: %r\n", __FUNCTION__, Status));
    goto DriverErrorExit;
  }

  // Get Parent's device path.
  Status = gBS->HandleProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&ParentDevicePath
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to get parent's device path\n", __FUNCTION__));
    goto DriverErrorExit;
  }

  // Open caller ID protocol for child
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gEfiCallerIdGuid,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install callerid protocol\n", __FUNCTION__));
    goto DriverErrorExit;
  }

  SubNode = 0;
  FdtForEachSubnode (SubNode, DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset) {
    if (!IsValidFlashNode (DeviceTreeNode->DeviceTreeBase, SubNode, NumChipSelects, &ChipSelect)) {
      continue;
    }

    // Allocate Private Data
    Private = AllocateRuntimeZeroPool (sizeof (NOR_FLASH_PRIVATE_DATA));
    if (Private == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    Private->Signature            = NOR_FLASH_SIGNATURE;
    Private->QspiControllerHandle = Controller;
    Private->QspiController       = QspiInstance;
    Private->QspiChipSelect       = ChipSelect;

    QspiClockSupport = FALSE;
    if ((QspiInstance->GetClockSpeed != NULL) &&
        (QspiInstance->SetClockSpeed != NULL))
    {
      QspiClockSupport = TRUE;
    }

    if (QspiClockSupport == TRUE) {
      // Check QSPI Bus Frequency
      Status = QspiInstance->GetClockSpeed (QspiInstance, &ClockSpeed);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: QSPI bus frequency could not be retrieved.\n", __FUNCTION__));
        goto ErrorExit;
      }

      DEBUG ((DEBUG_ERROR, "%a: Default QSPI bus frequency: %u\n", __FUNCTION__, ClockSpeed / 2));

      if (ClockSpeed > NOR_FAST_CMD_THRESH_FREQ) {
        Status = QspiInstance->SetClockSpeed (QspiInstance, NOR_FAST_CMD_THRESH_FREQ);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: QSPI bus frequency could not be set for SFDP.\n", __FUNCTION__));
          goto ErrorExit;
        }

        UINT64  NewClockSpeed;
        Status = QspiInstance->GetClockSpeed (QspiInstance, &NewClockSpeed);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: QSPI bus frequency could not be retrieved.\n", __FUNCTION__));
          goto ErrorExit;
        }

        DEBUG ((DEBUG_ERROR, "%a: New QSPI bus frequency: %u\n", __FUNCTION__, NewClockSpeed / 2));
        Private->PrivateFlashAttributes.FastReadSupport = TRUE;
      }
    }

    // Pre-silicon platforms use slow read regardless of clock speed.
    if (TegraGetPlatform () != TEGRA_PLATFORM_SILICON) {
      Private->PrivateFlashAttributes.FastReadSupport = FALSE;
    }

    // Read NOR flash's SFDP
    Status = ReadNorFlashSFDP (Private);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: SFDP Read Failed\n", __FUNCTION__));
      goto ErrorExit;
    }

    DEBUG ((
      DEBUG_ERROR,
      "%a: NOR Flash Uniform Memory Density: 0x%lx\n",
      __FUNCTION__,
      Private->PrivateFlashAttributes.FlashAttributes.MemoryDensity
      ));
    DEBUG ((
      DEBUG_ERROR,
      "%a: NOR Flash Uniform Block Size: 0x%lx\n",
      __FUNCTION__,
      Private->PrivateFlashAttributes.FlashAttributes.BlockSize
      ));
    DEBUG ((
      DEBUG_ERROR,
      "%a: NOR Flash Hybrid Memory Density: 0x%lx\n",
      __FUNCTION__,
      Private->PrivateFlashAttributes.HybridMemoryDensity
      ));
    DEBUG ((
      DEBUG_ERROR,
      "%a: NOR Flash Hybrid Block Size: 0x%lx\n",
      __FUNCTION__,
      Private->PrivateFlashAttributes.HybridBlockSize
      ));
    DEBUG ((
      DEBUG_ERROR,
      "%a: NOR Flash Write Page Size: 0x%lx\n",
      __FUNCTION__,
      Private->PrivateFlashAttributes.PageSize
      ));

    if (QspiClockSupport == TRUE) {
      if (ClockSpeed > NOR_FAST_CMD_THRESH_FREQ) {
        Status = QspiInstance->SetClockSpeed (QspiInstance, ClockSpeed);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: QSPI bus frequency could not be set for SFDP.\n", __FUNCTION__));
          goto ErrorExit;
        }

        UINT64  RestoredClockSpeed;
        Status = QspiInstance->GetClockSpeed (QspiInstance, &RestoredClockSpeed);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: QSPI bus frequency could not be retrieved.\n", __FUNCTION__));
          goto ErrorExit;
        }

        DEBUG ((DEBUG_ERROR, "%a: Restored QSPI bus frequency: %u\n", __FUNCTION__, RestoredClockSpeed / 2));
      }
    }

    // Allocate Command Buffer
    Private->CommandBuffer = AllocateRuntimeZeroPool (
                               NOR_CMD_SIZE + NOR_ADDR_SIZE +
                               Private->PrivateFlashAttributes.PageSize
                               );
    if (Private->CommandBuffer == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    // Append Vendor device path to parent device path.
    mNorFlashDevicePath.FlashIndex = FlashIndex;
    NorFlashDevicePath             = AppendDevicePathNode (
                                       ParentDevicePath,
                                       (EFI_DEVICE_PATH_PROTOCOL *)&mNorFlashDevicePath
                                       );
    if (NorFlashDevicePath == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to append path\n", __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    Private->ParentDevicePath   = ParentDevicePath;
    Private->NorFlashDevicePath = NorFlashDevicePath;

    // Install Protocols
    Private->NorFlashProtocol.FvbAttributes = EFI_FVB2_READ_ENABLED_CAP |
                                              EFI_FVB2_READ_STATUS |
                                              EFI_FVB2_STICKY_WRITE |
                                              EFI_FVB2_ERASE_POLARITY |
                                              EFI_FVB2_WRITE_STATUS |
                                              EFI_FVB2_WRITE_ENABLED_CAP;
    Private->NorFlashProtocol.GetAttributes = NorFlashGetAttributes;
    Private->NorFlashProtocol.Read          = NorFlashRead;
    Private->NorFlashProtocol.Write         = NorFlashWrite;
    Private->NorFlashProtocol.Erase         = NorFlashUniformErase;

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Private->NorFlashHandle,
                    &gNVIDIANorFlashProtocolGuid,
                    &Private->NorFlashProtocol,
                    &gEfiDevicePathProtocolGuid,
                    Private->NorFlashDevicePath,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to install NOR flash protocols\n", __FUNCTION__));
      goto ErrorExit;
    }

    Status = gBS->CreateEventEx (
                    EVT_NOTIFY_SIGNAL,
                    TPL_NOTIFY,
                    NorVirtualNotifyEvent,
                    Private,
                    &gEfiEventVirtualAddressChangeGuid,
                    &Private->VirtualAddrChangeEvent
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to create virtual address callback event\r\n", __FUNCTION__));
      goto ErrorExit;
    }

    if (PcdGetBool (PcdTegraNorBlockProtocols)) {
      Media.MediaId   = Private->FlashInstance;
      Media.BlockSize = Private->PrivateFlashAttributes.FlashAttributes.BlockSize;
      Media.LastBlock = (Private->PrivateFlashAttributes.FlashAttributes.MemoryDensity /
                         Private->PrivateFlashAttributes.FlashAttributes.BlockSize) - 1;

      Private->BlockIoProtocol.Reset       = NULL;
      Private->BlockIoProtocol.ReadBlocks  = NorFlashReadBlock;
      Private->BlockIoProtocol.WriteBlocks = NorFlashWriteBlock;
      Private->BlockIoProtocol.FlushBlocks = NULL;
      Private->BlockIoProtocol.Revision    = EFI_BLOCK_IO_PROTOCOL_REVISION;
      Private->BlockIoProtocol.Media       = &Media;

      Private->EraseBlockProtocol.Revision               = EFI_ERASE_BLOCK_PROTOCOL_REVISION;
      Private->EraseBlockProtocol.EraseLengthGranularity = 1;
      Private->EraseBlockProtocol.EraseBlocks            = NorFlashEraseBlock;

      Status = gBS->InstallMultipleProtocolInterfaces (
                      &Private->NorFlashHandle,
                      &gEfiBlockIoProtocolGuid,
                      &Private->BlockIoProtocol,
                      &gEfiEraseBlockProtocolGuid,
                      &Private->EraseBlockProtocol,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to install NOR flash block protocols\n", __FUNCTION__));
        goto ErrorExit;
      }
    }

    Private->ProtocolsInstalled = TRUE;

    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiCallerIdGuid,
                    (VOID **)&Interface,
                    This->DriverBindingHandle,
                    Private->NorFlashHandle,
                    EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to open caller ID protocol\n", __FUNCTION__));
      goto ErrorExit;
    }

    DEBUG ((DEBUG_INFO, "%a: flash%u initialized\n", __FUNCTION__, FlashIndex));
    FlashIndex++;
    NumInitialized++;

ErrorExit:
    if (EFI_ERROR (Status)) {
      if (Private != NULL) {
        gBS->CloseProtocol (
               Controller,
               &gEfiCallerIdGuid,
               This->DriverBindingHandle,
               Private->NorFlashHandle
               );
        gBS->CloseEvent (Private->VirtualAddrChangeEvent);
        if (Private->ProtocolsInstalled) {
          gBS->UninstallMultipleProtocolInterfaces (
                 Private->NorFlashHandle,
                 &gNVIDIANorFlashProtocolGuid,
                 &Private->NorFlashProtocol,
                 &gEfiDevicePathProtocolGuid,
                 Private->NorFlashDevicePath,
                 NULL
                 );

          if (PcdGetBool (PcdTegraNorBlockProtocols)) {
            gBS->UninstallMultipleProtocolInterfaces (
                   Private->NorFlashHandle,
                   &gEfiBlockIoProtocolGuid,
                   &Private->BlockIoProtocol,
                   &gEfiEraseBlockProtocolGuid,
                   &Private->EraseBlockProtocol,
                   NULL
                   );
          }
        }

        if (Private->NorFlashDevicePath != NULL) {
          FreePool (Private->NorFlashDevicePath);
        }

        if (Private->CommandBuffer != NULL) {
          FreePool (Private->CommandBuffer);
        }

        FreePool (Private);
      }
    }
  }

DriverErrorExit:
  if (NumInitialized > 0) {
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    gBS->UninstallMultipleProtocolInterfaces (
           Controller,
           &gEfiCallerIdGuid,
           NULL,
           NULL
           );
    gBS->CloseProtocol (
           Controller,
           &gNVIDIAQspiControllerProtocolGuid,
           This->DriverBindingHandle,
           Controller
           );
  }

  return Status;
}

/**
  Stops a device controller or a bus controller.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped.
  @param[in]  NumberOfChildren  The number of child device handles in ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.
**/
EFI_STATUS
EFIAPI
NorFlashDxeDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Controller,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer
  )
{
  EFI_STATUS                 Status;
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol;
  NOR_FLASH_PRIVATE_DATA     *Private;
  UINT32                     Index;

  if (NumberOfChildren == 0) {
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < NumberOfChildren; Index++) {
    Status = gBS->OpenProtocol (
                    ChildHandleBuffer[Index],
                    &gNVIDIANorFlashProtocolGuid,
                    (VOID **)&NorFlashProtocol,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      // Not handled by this driver
      continue;
    }

    Private = NOR_FLASH_PRIVATE_DATA_FROM_NOR_FLASH_PROTOCOL (NorFlashProtocol);

    Status = gBS->CloseProtocol (
                    Controller,
                    &gEfiCallerIdGuid,
                    This->DriverBindingHandle,
                    ChildHandleBuffer[Index]
                    );
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }

    gBS->CloseEvent (Private->VirtualAddrChangeEvent);
    if (Private->ProtocolsInstalled) {
      Status = gBS->UninstallMultipleProtocolInterfaces (
                      ChildHandleBuffer[Index],
                      &gNVIDIANorFlashProtocolGuid,
                      &Private->NorFlashProtocol,
                      &gEfiDevicePathProtocolGuid,
                      Private->NorFlashDevicePath,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }

      if (PcdGetBool (PcdTegraNorBlockProtocols)) {
        Status = gBS->UninstallMultipleProtocolInterfaces (
                        ChildHandleBuffer[Index],
                        &gEfiBlockIoProtocolGuid,
                        &Private->BlockIoProtocol,
                        &gEfiEraseBlockProtocolGuid,
                        &Private->EraseBlockProtocol,
                        NULL
                        );
        if (EFI_ERROR (Status)) {
          return EFI_DEVICE_ERROR;
        }
      }
    }

    if (Private->NorFlashDevicePath != NULL) {
      FreePool (Private->NorFlashDevicePath);
    }

    if (Private->CommandBuffer != NULL) {
      FreePool (Private->CommandBuffer);
    }

    FreePool (Private);
  }

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  Controller,
                  &gEfiCallerIdGuid,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = gBS->CloseProtocol (
                  Controller,
                  &gNVIDIAQspiControllerProtocolGuid,
                  This->DriverBindingHandle,
                  Controller
                  );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

EFI_DRIVER_BINDING_PROTOCOL  gNorFlashDxeDriverBinding = {
  NorFlashDxeDriverBindingSupported,
  NorFlashDxeDriverBindingStart,
  NorFlashDxeDriverBindingStop,
  0x1,
  NULL,
  NULL
};

/**
  The user Entry Point for module NorFlashDxe. The user code starts with this function.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some errors occur when executing this entry point.
**/
EFI_STATUS
EFIAPI
InitializeNorFlashDxe (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  // TODO: Add component name support.
  return EfiLibInstallDriverBinding (
           ImageHandle,
           SystemTable,
           &gNorFlashDxeDriverBinding,
           ImageHandle
           );
}
