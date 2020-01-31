/** @file

  Android Boot Loader Driver

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  Copyright (c) 2013-2014, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2017, Linaro. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "AndroidBootDxe.h"
#include <Library/PcdLib.h>

INTN
AndroidBootGetChosenNode (
  IN  INTN              NewFdtBase
  )
{
  INTN                  ChosenNode;
  ChosenNode = fdt_subnode_offset ((CONST VOID *)NewFdtBase, 0, "chosen");
  if (ChosenNode < 0) {
    ChosenNode = fdt_add_subnode((VOID *)NewFdtBase, 0, "chosen");
      if (ChosenNode < 0) {
        DEBUG ((DEBUG_ERROR, "%a: fail to find fdt node chosen\n", __FUNCTION__));
        return 0;
    }
  }
  return ChosenNode;
}

EFI_STATUS
AndroidBootSetProperty64 (
  IN  INTN              NewFdtBase,
  IN  INTN              ChosenNode,
  IN  CHAR8             *PropertyName,
  IN  UINT64            Val
  )
{
  INTN                  Err;
  struct fdt_property   *Property;
  int                   Len;

  Property = fdt_get_property_w((VOID *)NewFdtBase, ChosenNode,
                            PropertyName, &Len);
  if (NULL == Property && Len == -FDT_ERR_NOTFOUND) {
    Val = cpu_to_fdt64(Val);
    Err = fdt_appendprop ((VOID *)NewFdtBase, ChosenNode,
                          PropertyName, &Val, sizeof (UINT64));
    if (Err) {
      DEBUG ((DEBUG_ERROR, "%a: fdt_appendprop() fail: %a\n", __FUNCTION__, fdt_strerror (Err)));
      return EFI_INVALID_PARAMETER;
    }
  } else if (Property != NULL) {
    Err = fdt_setprop_u64((VOID *)NewFdtBase, ChosenNode,
                          PropertyName, Val);
    if (Err) {
      DEBUG ((DEBUG_ERROR, "%a: fdt_setprop_u64() fail: %a\n", __FUNCTION__, fdt_strerror (Err)));
      return EFI_INVALID_PARAMETER;
    }
  } else {
    DEBUG ((DEBUG_ERROR, "%a: fail to set fdt Property %a\n", __FUNCTION__, PropertyName));
    return EFI_INVALID_PARAMETER;
  }
  return EFI_SUCCESS;
}

EFI_STATUS
AndroidBootSetRamdiskInfo (
  IN  UINTN                   RamdiskBase,
  IN  UINTN                   RamdiskSize
  )
{
  EFI_STATUS                  Status;
  EFI_PHYSICAL_ADDRESS        NewFdtBase;
  VOID                        *FdtBase;
  INTN                        ChosenNode, Err, NewFdtSize;


  // Retrieve FdtBase via EFI service and verify its header
  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &FdtBase);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to get Device Tree base: %r\n", __FUNCTION__, Status));
    return Status;
  }
  Err = fdt_check_header (FdtBase);
  if (Err != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Device Tree header not valid: Err%d\n", __FUNCTION__, Err));
    return EFI_INVALID_PARAMETER;
  }

  // Allocate a new FDT reserved in EfiBootServicesData
  // to make this buffer persist until the completion of kernel booting
  NewFdtSize = (UINTN)fdt_totalsize (FdtBase) + FDT_ADDITIONAL_ENTRIES_SIZE;
  Status = gBS->AllocatePages (AllocateAnyPages, EfiBootServicesData,
                  EFI_SIZE_TO_PAGES (NewFdtSize), &NewFdtBase);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to allocate a new FDT: %r\n", __FUNCTION__, Status));
    return Status;
  }

  // Load the Original FDT tree into the new region
  Err = fdt_open_into(FdtBase, (VOID*)(INTN)NewFdtBase, NewFdtSize);
  if (Err) {
    DEBUG ((DEBUG_ERROR, "%a: fdt_open_into(): %a\n", __FUNCTION__, fdt_strerror (Err)));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  // Find out the ChosenNode and update Ramdisk info
  ChosenNode = AndroidBootGetChosenNode(NewFdtBase);
  if (!ChosenNode) {
    goto Exit;
  }
  Status = AndroidBootSetProperty64 (
                  NewFdtBase,
                  ChosenNode,
                  "linux,initrd-start",
                  (UINTN) RamdiskBase
                  );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }
  Status = AndroidBootSetProperty64 (
                  NewFdtBase,
                  ChosenNode,
                  "linux,initrd-end",
                  (UINTN)(RamdiskBase + RamdiskSize)
                  );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  // Update a new FDT base to System Configuration Table
  Status = gBS->InstallConfigurationTable (
                  &gFdtTableGuid,
                  (VOID *)(UINTN)NewFdtBase
                  );
  if (!EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

Exit:
  gBS->FreePages (NewFdtBase, EFI_SIZE_TO_PAGES (NewFdtSize));
  return Status;
}


/**
  Verify if there is the Android Boot image file by reading the magic word at the first
  block of the Android Boot image and save the important size information when a container
  is provided.

  @param[in]  BlockIo             BlockIo protocol interface which is already located.
  @param[in]  DiskIo              DiskIo protocol interface which is already located.
  @param[out] IntDataStructure    A pointer to the internal data structure to retain
                                  the important size data of kernel and initrd images
                                  contained in the Android Boot image header.

  @retval EFI_SUCCESS             Operation successful.
  @retval others                  Error occurred
**/
EFI_STATUS
AndroidBootGetVerify (
  IN  EFI_BLOCK_IO_PROTOCOL       *BlockIo,
  IN  EFI_DISK_IO_PROTOCOL        *DiskIo,
  OUT ANDROID_BOOT_DATA           *ImgData OPTIONAL
  )
{
  EFI_STATUS                      Status;
  ANDROID_BOOTIMG_HEADER          *Header;
  UINT32                          Offset;
  UINT32                          SignatureHeaderSize;
  UINTN                           PartitionSize;
  UINTN                           ImageSize;

  // ImgData can be NULL when it needs only for the verification
  if ((BlockIo == NULL) || (DiskIo == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // Get the image header of Android Boot image
  Header = AllocatePool (sizeof(ANDROID_BOOTIMG_HEADER));
  if (Header == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  SignatureHeaderSize = PcdGet32 (PcdBootImgSigningHeaderSize);

  Offset = 0;
  Status = DiskIo->ReadDisk (
                  DiskIo,
                  BlockIo->Media->MediaId,
                  Offset,
                  sizeof (*Header),
                  (VOID *) Header
                  );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  // Make sure the Android Boot image
  if (AsciiStrnCmp ((CONST CHAR8 *)Header->BootMagic,
                    ANDROID_BOOT_MAGIC, ANDROID_BOOT_MAGIC_LENGTH) != 0) {
    Status = EFI_NOT_FOUND;
    if (SignatureHeaderSize != 0) {
      Offset = SignatureHeaderSize;
      Status = DiskIo->ReadDisk (
                      DiskIo,
                      BlockIo->Media->MediaId,
                      Offset,
                      sizeof (*Header),
                      (VOID *) Header
                      );
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      // Make sure the Android Boot image
      if (AsciiStrnCmp ((CONST CHAR8 *)Header->BootMagic,
                        ANDROID_BOOT_MAGIC, ANDROID_BOOT_MAGIC_LENGTH) != 0) {
        Status = EFI_NOT_FOUND;
      }
    }

    if (EFI_ERROR (Status)) {
      goto Exit;
    }
  }

  // The page size is not specified, but it should be power of 2 at least
  if (!IS_VALID_ANDROID_PAGE_SIZE (Header->PageSize)) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  // Make sure that the image fits in the partition
  PartitionSize = (UINTN)(BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;
  ImageSize = Offset + Header->PageSize
                  + ALIGN_VALUE (Header->KernelSize, Header->PageSize)
                  + ALIGN_VALUE (Header->RamdiskSize, Header->PageSize);
  if (ImageSize > PartitionSize) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  // Set up the internal data structure when ImgData is not NULL
  if (ImgData != NULL) {
    // Calculate a size of the kernel image, aligned in BlockSize
    // This size will be a reference when boot manger allocates a pool for LoadFile service
    // Kernel image to be loaded to a buffer allocated by boot manager
    // Ramdisk image to be loaded to a buffer allocated by this LoadFile service
    ImgData->Offset      = Offset;
    ImgData->KernelSize  = Header->KernelSize;
    ImgData->RamdiskSize = Header->RamdiskSize;
    ImgData->PageSize    = Header->PageSize;
  }

  Status = EFI_SUCCESS;

Exit:
  FreePool (Header);

  return Status;
}


/**
  Attempt to load the kernel and initrd from the Android Boot image.
  Allocate pages reserved in BootService for the initrd image to persist until
  the completion of the kernel booting.

  @param[in]  BlockIo             BlockIo protocol interface which is already located.
  @param[in]  DiskIo              DiskIo protocol interface which is already located.
  @param[in]  IntDataStruct       A pointer to the internal data structure to retain
                                  the important size data of kernel and initrd images
                                  contained in the Android Boot image header.
  @param[in]  Buffer              The memory buffer to transfer the file to.

  @retval EFI_SUCCESS             Operation successful.
  @retval others                  Error occurred
**/
EFI_STATUS
AndroidBootLoadFile (
  IN EFI_BLOCK_IO_PROTOCOL        *BlockIo,
  IN EFI_DISK_IO_PROTOCOL         *DiskIo,
  IN ANDROID_BOOT_DATA            *ImgData,
  IN VOID                         *Buffer
  )
{
  EFI_STATUS                      Status;
  UINTN                           Addr;
  UINTN                           BufSize;
  UINTN                           BufBase;


  if ((BlockIo == NULL) || (DiskIo == NULL) || (Buffer == NULL) || (ImgData == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // Android Boot image enabled in EFI stub feature consists of:
  // - Header info in PageSize that contains Android Boot image header
  // - Kernel image in EFI format as built in EFI stub feature
  // - Ramdisk image
  // - more as described in Android Boot image header
  // Note: Every image data is aligned in PageSize

  // Load the kernel
  Addr = ImgData->PageSize + ImgData->Offset;
  BufSize = ImgData->KernelSize;
  BufBase = (UINTN) Buffer;
  Status = DiskIo->ReadDisk (
                  DiskIo,
                  BlockIo->Media->MediaId,
                  Addr,
                  BufSize,
                  (VOID *) BufBase
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to read disk for kernel image: from offset %x" \
                  " to %09p: %r\n", __FUNCTION__, Addr, BufBase, Status));
    return Status;
  }
  DEBUG ((DEBUG_INFO, "%a: Kernel image copied to %09p in size %08x\n", __FUNCTION__, BufBase, BufSize));

  // Load the initial ramdisk if needed
  if (ImgData->RamdiskSize != 0) {
    // Allocate a buffer reserved in EfiBootServicesData
    // to make this buffer persist until the completion of kernel booting
    Status = gBS->AllocatePages (AllocateAnyPages, EfiBootServicesData,
                    EFI_SIZE_TO_PAGES (ImgData->RamdiskSize),
                    (EFI_PHYSICAL_ADDRESS *) &BufBase);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: fail to get a buffer for ramdisk: %r\n", __FUNCTION__, Status));
      return Status;
    }
    Addr += ALIGN_VALUE (ImgData->KernelSize, ImgData->PageSize);
    BufSize = ImgData->RamdiskSize;
    Status = DiskIo->ReadDisk (
                    DiskIo,
                    BlockIo->Media->MediaId,
                    Addr,
                    BufSize,
                    (VOID *) BufBase
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to read disk for ramdisk from offset %x" \
                    " to %09p: %r\n", __FUNCTION__, Addr, BufBase, Status));
      goto Exit;
    }
    DEBUG ((DEBUG_INFO, "%a: RamDisk loaded to %09p in size %08x\n", __FUNCTION__, BufBase, BufSize));

    // Update FDT to set up the start and end address of the ram disk image
    BufSize = ImgData->RamdiskSize;
    Status = AndroidBootSetRamdiskInfo (
                    BufBase,  // Ramdisk Base address
                    BufSize   // Ramdisk Size
                    );
    if (EFI_ERROR (Status)) {
      goto Exit;
    }
    DEBUG ((DEBUG_INFO, "%a: FDT updated for ramdisk info, %09p++%08x\n", __FUNCTION__, BufBase, BufSize));
  }

  return EFI_SUCCESS;

Exit:
  gBS->FreePages ((EFI_PHYSICAL_ADDRESS) BufBase, EFI_SIZE_TO_PAGES (ImgData->RamdiskSize));
  return Status;
}


/**
  Causes the driver to load a specified file.

  @param  This                  Protocol instance pointer.
  @param  FilePath              The device specific path of the file to load.
  @param  BootPolicy            If TRUE, indicates that the request originates from the
                                boot manager is attempting to load FilePath as a boot
                                selection. If FALSE, then FilePath must match as exact file
                                to be loaded.
  @param  BufferSize            On input the size of Buffer in bytes. On output with a return
                                code of EFI_SUCCESS, the amount of data transferred to
                                Buffer. On output with a return code of EFI_BUFFER_TOO_SMALL,
                                the size of Buffer required to retrieve the requested file.
  @param  Buffer                The memory buffer to transfer the file to. IF Buffer is NULL,
                                then the size of the requested file is returned in
                                BufferSize.

  @retval EFI_SUCCESS           The file was loaded.
  @retval EFI_UNSUPPORTED       The device does not support the provided BootPolicy
  @retval EFI_INVALID_PARAMETER FilePath is not a valid device path, or
                                BufferSize is NULL.
  @retval EFI_NO_MEDIA          No medium was present to load the file.
  @retval EFI_DEVICE_ERROR      The file was not loaded due to a device error.
  @retval EFI_NO_RESPONSE       The remote system did not respond.
  @retval EFI_NOT_FOUND         The file was not found.
  @retval EFI_ABORTED           The file load process was manually cancelled.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize is too small to read the current directory entry.
                                BufferSize has been updated with the size needed to complete
                                the request.

**/
EFI_STATUS
EFIAPI
AndroidBootDxeLoadFile (
  IN EFI_LOAD_FILE_PROTOCOL     *This,
  IN EFI_DEVICE_PATH_PROTOCOL   *FilePath,
  IN BOOLEAN                    BootPolicy,
  IN OUT UINTN                  *BufferSize,
  IN VOID                       *Buffer OPTIONAL
  )

{
  EFI_STATUS                    Status;
  ANDROID_BOOT_PRIVATE_DATA     *Private;
  ANDROID_BOOT_DATA             ImgData;


  DEBUG ((DEBUG_INFO, "%a: buffer %09p in size %08x\n", __FUNCTION__, Buffer, *BufferSize));

  // Verify if the valid parameters
  if (This == NULL || BufferSize == NULL || FilePath == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (*BufferSize != 0 && Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (!BootPolicy) {
    return EFI_UNSUPPORTED;
  }

  // Retrieve Private data structure
  Private = ANDROID_BOOT_PRIVATE_DATA_FROM_LOADFILE(This);
  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Verify the image header and set the internal data structure ImgData
  Status = AndroidBootGetVerify (Private->BlockIo, Private->DiskIo, &ImgData);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Check if the given buffer size is big enough
  // EFI_BUFFER_TOO_SMALL gets boot manager allocate a bigger buffer
  if (*BufferSize < ImgData.KernelSize) {
    *BufferSize = ImgData.KernelSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  // Load Android Boot image
  Status = AndroidBootLoadFile (Private->BlockIo, Private->DiskIo, &ImgData, Buffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}


///
/// Load File Protocol instance
///
GLOBAL_REMOVE_IF_UNREFERENCED
EFI_LOAD_FILE_PROTOCOL  mAndroidBootDxeLoadFile = {
  AndroidBootDxeLoadFile
};


/**
  Tests to see if this driver supports a given controller. If a child device is provided,
  it further tests to see if this driver supports creating a handle for the specified child device.

  This function checks to see if the driver specified by This supports the device specified by
  ControllerHandle. Drivers will typically use the device path attached to
  ControllerHandle and/or the services from the bus I/O abstraction attached to
  ControllerHandle to determine if the driver supports ControllerHandle. This function
  may be called many times during platform initialization. In order to reduce boot times, the tests
  performed by this function must be very small, and take as little time as possible to execute. This
  function must not change the state of any hardware devices, and this function must be aware that the
  device specified by ControllerHandle may already be managed by the same driver or a
  different driver. This function must match its calls to AllocatePages() with FreePages(),
  AllocatePool() with FreePool(), and OpenProtocol() with CloseProtocol().
  Because ControllerHandle may have been previously started by the same driver, if a protocol is
  already in the opened state, then it must not be closed with CloseProtocol(). This is required
  to guarantee the state of ControllerHandle is not modified by this function.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For bus drivers, if this parameter is not NULL, then
                                   the bus driver must determine if the bus controller specified
                                   by ControllerHandle and the child controller specified
                                   by RemainingDevicePath are both supported by this
                                   bus driver.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by the driver
                                   specified by This.
  @retval EFI_ACCESS_DENIED        The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by a different
                                   driver or an application that requires exclusive access.
                                   Currently not implemented.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the driver specified by This.
**/
EFI_STATUS
EFIAPI
AndroidBootDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS                      Status;
  UINT32                          *Id;
  EFI_BLOCK_IO_PROTOCOL           *BlockIo = NULL;
  EFI_DISK_IO_PROTOCOL            *DiskIo = NULL;


  // This driver will be accessed while boot manager attempts to connect
  // all drivers to the controllers for each partition entry.
  // - BlockIo       to give a physical access to the flash device to obtain the image
  // - PartitionInfo to see the additional info like GPT type and the partition name, not must
  // - DevicePath    to get the device path and create a child node
  //                    MESSAGING_DEVICE_PATH and MSG_URI_DP required to be a valid boot option
  // Opening BY_DRIVER would not be successful so this opens GET_PROTOCOL
  // so CallerId will be used to avoid multiple attempts from attempting to manage the same controller.

  // Make sure BindingStart not done yet
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiCallerIdGuid,
                  (VOID **) &Id,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (Status != EFI_UNSUPPORTED) {
    return Status;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **)&DiskIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  // Examine if the Android Boot image can be found
  Status = AndroidBootGetVerify (BlockIo, DiskIo, NULL);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: AndroidBoot image found\n", __FUNCTION__));
  }

ErrorExit:
  if (BlockIo != NULL) {
    gBS->CloseProtocol (
                    ControllerHandle,
                    &gEfiBlockIoProtocolGuid,
                    This->DriverBindingHandle,
                    ControllerHandle
                    );
  }
  if (DiskIo != NULL) {
    gBS->CloseProtocol (
                    ControllerHandle,
                    &gEfiDiskIoProtocolGuid,
                    This->DriverBindingHandle,
                    ControllerHandle
                    );
  }
  return Status;
}


/**
  Starts a device controller or a bus controller.

  The Start() function is designed to be invoked from the EFI boot service ConnectController().
  As a result, much of the error checking on the parameters to Start() has been moved into this
  common boot service. It is legal to call Start() from other locations,
  but the following calling restrictions must be followed, or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE.
  2. If RemainingDevicePath is not NULL, then it must be a pointer to a naturally aligned
     EFI_DEVICE_PATH_PROTOCOL.
  3. Prior to calling Start(), the Supported() function for the driver specified by This must
     have been called with the same calling parameters, and Supported() must have returned EFI_SUCCESS.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to start. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For a bus driver, if this parameter is NULL, then handles
                                   for all the children of Controller are created by this driver.
                                   If this parameter is not NULL and the first Device Path Node is
                                   not the End of Device Path Node, then only the handle for the
                                   child device specified by the first Device Path Node of
                                   RemainingDevicePath is created by this driver.
                                   If the first Device Path Node of RemainingDevicePath is
                                   the End of Device Path Node, no child handle is created by this
                                   driver.

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a device error.Currently not implemented.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.
  @retval Others                   The driver failded to start the device.

**/
/**
  Initialize the FVB Driver

  @param[in]  ImageHandle   of the loaded driver
  @param[in]  SystemTable   Pointer to the System Table

**/
EFI_STATUS
EFIAPI
AndroidBootDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS                      Status;
  EFI_BLOCK_IO_PROTOCOL           *BlockIo = NULL;
  EFI_DISK_IO_PROTOCOL            *DiskIo = NULL;
  EFI_DEVICE_PATH_PROTOCOL        *ParentDevicePath;
  EFI_DEVICE_PATH_PROTOCOL        *AndroidBootDevicePath;
  EFI_DEVICE_PATH_PROTOCOL        *Node;
  ANDROID_BOOT_PRIVATE_DATA       *Private;
  UINT32                          *Id;


  // BindingSupported() filters out the unsupported attempts and the multiple attempts
  // from a successful ControllerHandle such that BindingStart() runs only once

  Private = NULL;
  BlockIo = NULL;
  ParentDevicePath = NULL;

  // Get Parent's device path to create a child node and append URI node
  Status = gBS->HandleProtocol (ControllerHandle,
                                &gEfiDevicePathProtocolGuid,
                                (VOID **)&ParentDevicePath);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to get DevicePath: %r\n", __FUNCTION__, Status));
    return Status;
  }

  // Open BlockIo protocol to obtain the access to the flash device
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to open BlockIo: %r\n", __FUNCTION__, Status));
    return Status;
  }

  // Open Disk Io protocol to obtain the access to the flash device
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **)&DiskIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a unable to open DiskIo protocol %r\n", __FUNCTION__, Status));
    return Status;
  }
  // Examine if the Android Boot Image can be found
  Status = AndroidBootGetVerify (BlockIo, DiskIo, NULL);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  // Append URI device path node so this device can be used as boot option
  Node = CreateDeviceNode (MESSAGING_DEVICE_PATH, MSG_URI_DP, sizeof (EFI_DEVICE_PATH_PROTOCOL));
  if (Node == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }
  AndroidBootDevicePath = AppendDevicePathNode (ParentDevicePath, (EFI_DEVICE_PATH_PROTOCOL*) Node);
  FreePool (Node);
  if (AndroidBootDevicePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  // Allocate Private Data and set up the initial data
  Private = AllocateZeroPool (sizeof (ANDROID_BOOT_PRIVATE_DATA));
  if (Private == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }
  Private->Signature = ANDROID_BOOT_SIGNATURE;
  Private->BlockIo = BlockIo;
  Private->DiskIo = DiskIo;
  Private->ParentDevicePath = ParentDevicePath;
  Private->AndroidBootDevicePath = AndroidBootDevicePath;
  Private->ControllerHandle = ControllerHandle;
  Private->ProtocolsInstalled = FALSE;
  CopyMem (&Private->LoadFile, &mAndroidBootDxeLoadFile, sizeof (Private->LoadFile));

  // Install LoadFile and AndroidBootDevicePath protocols on child, AndroidBootHandle
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Private->AndroidBootHandle,
                  &gEfiLoadFileProtocolGuid,
                  &Private->LoadFile,
                  &gEfiDevicePathProtocolGuid,
                  Private->AndroidBootDevicePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to install the prot intf: %r\n", __FUNCTION__, Status));
    goto Exit;
  }
  Private->ProtocolsInstalled = TRUE;

  // Install and open CallerId to link the Private data structure
  Status = gBS->InstallProtocolInterface (
                  &ControllerHandle,
                  &gEfiCallerIdGuid,
                  EFI_NATIVE_INTERFACE,
                  &Private->Id
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to install CallerId: %r\n", __FUNCTION__, Status));
    goto Exit;
  }
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiCallerIdGuid,
                  (VOID **) &Id,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to open CallerId: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  // Setup a parent-child relationship between ControllerHandle and AndroidBootHandle
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiCallerIdGuid,
                  (VOID **) &Id,
                  This->DriverBindingHandle,
                  Private->AndroidBootHandle,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: fail to set up parent-child: %r\n", __FUNCTION__, Status));
    goto Exit;
  }

Exit:
  if (EFI_ERROR (Status)) {
    if (Private != NULL) {
      gBS->CloseProtocol (
                      ControllerHandle,
                      &gEfiCallerIdGuid,
                      This->DriverBindingHandle,
                      Private->AndroidBootHandle
                      );
      gBS->UninstallProtocolInterface (
                      ControllerHandle,
                      &gEfiCallerIdGuid,
                      &Private->Id
                      );
      if (Private->ProtocolsInstalled) {
        gBS->UninstallMultipleProtocolInterfaces (
                        Private->AndroidBootHandle,
                        &gEfiLoadFileProtocolGuid,
                        &Private->LoadFile,
                        &gEfiDevicePathProtocolGuid,
                        Private->AndroidBootDevicePath,
                        NULL
                        );
      }
      FreePool (Private);
    }
    if (AndroidBootDevicePath != NULL) {
      FreePool (AndroidBootDevicePath);
    }

    gBS->CloseProtocol (
                    ControllerHandle,
                    &gEfiDiskIoProtocolGuid,
                    This->DriverBindingHandle,
                    &DiskIo
                    );
    gBS->CloseProtocol (
                    ControllerHandle,
                    &gEfiBlockIoProtocolGuid,
                    This->DriverBindingHandle,
                    &BlockIo
                    );
  } else {
    // BindingStart completed
    DEBUG ((DEBUG_INFO, "%a: done\n", __FUNCTION__));
  }

  return Status;
}


/**
  Stops a device controller or a bus controller.

  The Stop() function is designed to be invoked from the EFI boot service DisconnectController().
  As a result, much of the error checking on the parameters to Stop() has been moved
  into this common boot service. It is legal to call Stop() from other locations,
  but the following calling restrictions must be followed, or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE that was used on a previous call to this
     same driver's Start() function.
  2. The first NumberOfChildren handles of ChildHandleBuffer must all be a valid
     EFI_HANDLE. In addition, all of these handles must have been created in this driver's
     Start() function, and the Start() function must have called OpenProtocol() on
     ControllerHandle with an Attribute of EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped. The handle must
                                support a bus specific I/O protocol for the driver
                                to use to stop the device.
  @param[in]  NumberOfChildren  The number of child device handles in ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed. May be NULL
                                if NumberOfChildren is 0.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.

**/
EFI_STATUS
EFIAPI
AndroidBootDriverBindingStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  )
{
  EFI_STATUS                      Status;
  EFI_LOAD_FILE_PROTOCOL          *LoadFile;
  ANDROID_BOOT_PRIVATE_DATA       *Private;
  UINT32                          *Id;


  if (NumberOfChildren != 0) {
    return EFI_UNSUPPORTED;
  }

  // Attempt to open LoadFile protocol
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiLoadFileProtocolGuid,
                  (VOID **) &LoadFile,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiCallerIdGuid,
                    (VOID **) &Id,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
    Private = ANDROID_BOOT_PRIVATE_DATA_FROM_ID (Id);
  } else {
    Private = ANDROID_BOOT_PRIVATE_DATA_FROM_LOADFILE (LoadFile);
  }

  gBS->CloseProtocol (
                  ControllerHandle,
                  &gEfiCallerIdGuid,
                  This->DriverBindingHandle,
                  &Private->Id
                  );
  gBS->UninstallProtocolInterface (
                  ControllerHandle,
                  &gEfiCallerIdGuid,
                  &Private->Id
                  );
  gBS->UninstallMultipleProtocolInterfaces (
                  Private->AndroidBootHandle,
                  &gEfiLoadFileProtocolGuid,
                  &Private->LoadFile,
                  &gEfiDevicePathProtocolGuid,
                  Private->AndroidBootDevicePath,
                  NULL
                  );
  FreePool (Private->AndroidBootDevicePath);
  FreePool (Private);

  DEBUG ((DEBUG_INFO, "%a: done\n", __FUNCTION__));

  return EFI_SUCCESS;
}

/**
  Causes the driver to load a specified file.

  @param  This                  Protocol instance pointer.
  @param  FilePath              The device specific path of the file to load.
  @param  BootPolicy            If TRUE, indicates that the request originates from the
                                boot manager is attempting to load FilePath as a boot
                                selection. If FALSE, then FilePath must match as exact file
                                to be loaded.
  @param  BufferSize            On input the size of Buffer in bytes. On output with a return
                                code of EFI_SUCCESS, the amount of data transferred to
                                Buffer. On output with a return code of EFI_BUFFER_TOO_SMALL,
                                the size of Buffer required to retrieve the requested file.
  @param  Buffer                The memory buffer to transfer the file to. IF Buffer is NULL,
                                then the size of the requested file is returned in
                                BufferSize.

  @retval EFI_SUCCESS           The file was loaded.
  @retval EFI_UNSUPPORTED       The device does not support the provided BootPolicy
  @retval EFI_INVALID_PARAMETER FilePath is not a valid device path, or
                                BufferSize is NULL.
  @retval EFI_NO_MEDIA          No medium was present to load the file.
  @retval EFI_DEVICE_ERROR      The file was not loaded due to a device error.
  @retval EFI_NO_RESPONSE       The remote system did not respond.
  @retval EFI_NOT_FOUND         The file was not found.
  @retval EFI_ABORTED           The file load process was manually cancelled.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize is too small to read the current directory entry.
                                BufferSize has been updated with the size needed to complete
                                the request.

**/
EFI_STATUS
EFIAPI
RamloadLoadFile (
  IN EFI_LOAD_FILE_PROTOCOL     *This,
  IN EFI_DEVICE_PATH_PROTOCOL   *FilePath,
  IN BOOLEAN                    BootPolicy,
  IN OUT UINTN                  *BufferSize,
  IN VOID                       *Buffer OPTIONAL
  )
{
  // Verify if the valid parameters
  if (This == NULL || BufferSize == NULL || FilePath == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (*BufferSize != 0 && Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (!BootPolicy) {
    return EFI_UNSUPPORTED;
  }

  // Check if the given buffer size is big enough
  // EFI_BUFFER_TOO_SMALL gets boot manager allocate a bigger buffer
  if (*BufferSize < PcdGet64 (PcdRamLoadedKernelSize)) {
  *BufferSize = PcdGet64 (PcdRamLoadedKernelSize);
    return EFI_BUFFER_TOO_SMALL;
  }

  CopyMem (Buffer, (VOID *)(UINTN)PcdGet64 (PcdRamLoadedKernelAddress), PcdGet64 (PcdRamLoadedKernelSize));

  if (PcdGet64 (PcdRamLoadedInitrdSize) != 0) {
    AndroidBootSetRamdiskInfo (PcdGet64 (PcdRamLoadedInitrdAddress), PcdGet64 (PcdRamLoadedInitrdSize));
  }

  return EFI_SUCCESS;
}

///
/// Ramload LoadFile Protocol instance
///
EFI_LOAD_FILE_PROTOCOL mRamloadLoadFile = {
  RamloadLoadFile
};
///
/// Driver Binding Protocol instance
///
EFI_DRIVER_BINDING_PROTOCOL mAndroidBootDriverBinding = {
  AndroidBootDriverBindingSupported,
  AndroidBootDriverBindingStart,
  AndroidBootDriverBindingStop,
  0x0,
  NULL,
  NULL
};

//
// Device path for the handle that incorporates our ramload load file instance.
//
#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH       VenHwNode;
  EFI_DEVICE_PATH_PROTOCOL EndNode;
} SINGLE_VENHW_NODE_DEVPATH;
#pragma pack()

STATIC SINGLE_VENHW_NODE_DEVPATH mLoadFileDevicePath = {
  {
    { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH) } },
    { 0 }
  },

  {
    END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE,
    { sizeof (EFI_DEVICE_PATH_PROTOCOL) }
  }
};

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers including
  both device drivers and bus drivers.

  @param[in]  ImageHandle       The firmware allocated handle for the UEFI image.
  @param[in]  SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval Others                An unexpected error occurred.

**/
EFI_STATUS
EFIAPI
AndroidBootDxeDriverEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS            Status;

  // Install UEFI Driver Model protocol(s).
  Status = EfiLibInstallDriverBinding (
             ImageHandle,
             SystemTable,
             &mAndroidBootDriverBinding,
             ImageHandle
             );

  if (PcdGetBool(PcdRamLoadedKernelSupport)) {
    EFI_HANDLE LoadFileHandle = 0;

    CopyMem (&mLoadFileDevicePath.VenHwNode.Guid, &gNVIDIARamloadKernelGuid, sizeof (EFI_GUID));

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &LoadFileHandle,
                    &gEfiLoadFileProtocolGuid,
                    &mRamloadLoadFile,
                    &gEfiDevicePathProtocolGuid,
                    &mLoadFileDevicePath,
                    NULL);
  }

  return Status;
}
