/** @file
  NVIDIA Oem Partition Sample Driver

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/MmServicesTableLib.h>         // gMmst
#include <Library/MemoryAllocationLib.h>        // AllocateZeroPool
#include <Library/PlatformResourceLib.h>        // GetPartitionInfoStMm
#include <Library/StandaloneMmOpteeDeviceMem.h> // GetCpuBlParamsAddrStMm
#include <Library/DebugLib.h>
#include <Protocol/OemPartitionProtocol.h>
#include <Guid/OemPartition.h>
#include <Library/BaseMemoryLib.h>
#include "InternalOemPartitionMm.h"

STATIC OEM_PARTITION_PRIVATE_INFO  mOemPartitionPrivate;
STATIC OEM_PARTITION_PROTOCOL      mOemPartitionMmProtocol;

/**
  Read data from Oem partition.

  @param[out] Data                 Address to read data into
  @param[in]  Offset               Offset to read from
  @param[in]  Size                 Number of bytes to be read

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
OemPartitionReadSpiNor (
  OUT VOID   *Data,
  IN UINT32  Offset,
  IN UINT32  Length
  )
{
  EFI_STATUS  Status;

  if (Offset + Length > mOemPartitionPrivate.PartitionSize) {
    Status = EFI_INVALID_PARAMETER;
    goto ReturnStatus;
  }

  Status = mOemPartitionPrivate.NorFlashProtocol->Read (
                                                    mOemPartitionPrivate.NorFlashProtocol,
                                                    Offset + mOemPartitionPrivate.PartitionBaseAddress,
                                                    Length,
                                                    Data
                                                    );

ReturnStatus:
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: NorFlashRead returned Status %r\n", __FUNCTION__, Status));
  }

  return Status;
}

/**
  Write data to Oem partition.

  @param[in] Data                  Address to write data from
  @param[in] Offset                Offset to read from
  @param[in] Size                  Number of bytes to write

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
OemPartitionWriteSpiNor (
  IN VOID    *Data,
  IN UINT32  Offset,
  IN UINT32  Length
  )
{
  EFI_STATUS  Status;

  if ((Offset + Length) > mOemPartitionPrivate.PartitionSize) {
    Status = EFI_INVALID_PARAMETER;
    goto ReturnStatus;
  }

  Status = mOemPartitionPrivate.NorFlashProtocol->Write (
                                                    mOemPartitionPrivate.NorFlashProtocol,
                                                    Offset + mOemPartitionPrivate.PartitionBaseAddress,
                                                    Length,
                                                    Data
                                                    );

ReturnStatus:
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: NorFlashWrite returned Status %r\n", __FUNCTION__, Status));
  }

  return Status;
}

/**
  Erase data block from Oem partition.

  @param[in] Lba                   Logical block to start erasing from
  @param[in] NumLba                Number of block to be erased

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
OemPartitionEraseSpiNor (
  IN UINT32  Offset,
  IN UINT32  Length
  )
{
  EFI_STATUS  Status;
  UINT32      Lba;
  UINT32      NumLba;

  if ((Offset % mOemPartitionPrivate.BlockSize != 0) ||
      (Length % mOemPartitionPrivate.BlockSize != 0) ||
      ((Offset + Length) > mOemPartitionPrivate.PartitionSize))
  {
    DEBUG ((DEBUG_ERROR, "%a: Offset or Length invalid\n", __FUNCTION__));
    Status = EFI_INVALID_PARAMETER;
    goto ReturnStatus;
  }

  Lba    = (Offset + mOemPartitionPrivate.PartitionBaseAddress) / mOemPartitionPrivate.BlockSize;
  NumLba = Length / mOemPartitionPrivate.BlockSize;

  Status = mOemPartitionPrivate.NorFlashProtocol->Erase (
                                                    mOemPartitionPrivate.NorFlashProtocol,
                                                    Lba,
                                                    NumLba
                                                    );
ReturnStatus:
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: NorFlashErase returned Status %r\n", __FUNCTION__, Status));
  }

  return Status;
}

/**
  Data erased check from Oem partition.

  @param[in] Offset                Offset to be checked
  @param[in] Length                Number of bytes to be checked

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
OemPartitionIsErasedSpiNor (
  IN UINT32  Offset,
  IN UINT32  Length
  )
{
  EFI_STATUS  Status;
  UINT8       *Data;
  UINTN       Index;

  Data = NULL;

  Data = (UINT8 *)AllocateZeroPool (Length);
  if (Data == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = OemPartitionReadSpiNor (Data, Offset, Length);
  if (!EFI_ERROR (Status)) {
    for (Index = 0; Index < Length; Index += 1) {
      if (*(Data + Index) != ERASE_BYTE) {
        Status = EFI_DEVICE_ERROR;
        break;
      }
    }
  }

  FreePool (Data);
  return Status;
}

/**
  Get Oem partition info.

  @param[out] PartitionBaseAddress Oem partition offset in SPI NOR.
  @param[out] PartitionSize        Size in bytes of the partition.
  @param[out] BlockSize            The size in bytes of each block.
  @param[out] NumBlocks            Number of blocks in partition.

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
OemPartitionInfo (
  UINT32  *PartitionBaseAddress,
  UINT32  *PartitionSize,
  UINT32  *BlockSize,
  UINT32  *NumBlocks
  )
{
  *PartitionBaseAddress = mOemPartitionPrivate.PartitionBaseAddress;
  *PartitionSize        = mOemPartitionPrivate.PartitionSize;
  *BlockSize            = mOemPartitionPrivate.BlockSize;
  *NumBlocks            = mOemPartitionPrivate.NumBlocks;

  return EFI_SUCCESS;
}

/**
  Oem partition info initialize.

  @param[in] NorFlashProtocol      SPI Nor flash protocol instance
  @param[in] NorPartitionOffset    Oem partition offset
  @param[in] NorPartitionSize      Oem partition size

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
OemPartitionInitProtocol (
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol,
  UINT32                     NorPartitionOffset,
  UINT32                     NorPartitionSize
  )
{
  EFI_STATUS  Status;

  mOemPartitionPrivate.NorFlashProtocol = NorFlashProtocol;

  if (NorFlashProtocol == NULL) {
    Status = EFI_NO_MEDIA;
    goto Done;
  }

  Status = NorFlashProtocol->GetAttributes (NorFlashProtocol, &mOemPartitionPrivate.NorAttributes);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Couldn't get MM-NorFlash Protocol's Attributes\n", __FUNCTION__));
    goto Done;
  }

  if ((NorPartitionOffset + NorPartitionSize) > mOemPartitionPrivate.NorAttributes.MemoryDensity) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Oem Partition size %u with base address %u doesn't fit in a Nor with size %u\n",
      __FUNCTION__,
      NorPartitionSize,
      NorPartitionOffset,
      mOemPartitionPrivate.NorAttributes.MemoryDensity
      ));
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  mOemPartitionPrivate.BlockSize =  mOemPartitionPrivate.NorAttributes.BlockSize;

  mOemPartitionPrivate.PartitionBaseAddress = NorPartitionOffset;
  if ((mOemPartitionPrivate.PartitionBaseAddress % mOemPartitionPrivate.BlockSize) != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: OEM Partition base address %u isn't a multiple of NorFlash block size %u\n",
      __FUNCTION__,
      mOemPartitionPrivate.PartitionBaseAddress,
      mOemPartitionPrivate.BlockSize
      ));
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  mOemPartitionPrivate.NumBlocks = NorPartitionSize / mOemPartitionPrivate.BlockSize;

  mOemPartitionPrivate.PartitionSize = mOemPartitionPrivate.NumBlocks * (UINTN)mOemPartitionPrivate.BlockSize;
Done:
  return Status;
}

/**
  Oem partition initialization.

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred

**/
STATIC
EFI_STATUS
EFIAPI
OemPartitionLocateStorage (
  VOID
  )
{
  EFI_STATUS                 Status;
  NVIDIA_NOR_FLASH_PROTOCOL  *NorFlashProtocol;
  EFI_PHYSICAL_ADDRESS       CpuBlParamsAddr;
  UINT16                     DeviceInstance;
  UINT64                     PartitionByteOffset;
  UINT64                     PartitionSize;

  NorFlashProtocol = GetSocketNorFlashProtocol (SOCKET_0_NOR_FLASH);

  if (NorFlashProtocol == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Couldn't get MM-NorFlash Protocol for socket %d\n", __FUNCTION__, SOCKET_0_NOR_FLASH));
    Status = EFI_NO_MEDIA;

    goto Done;
  }

  Status = GetCpuBlParamsAddrStMm (&CpuBlParamsAddr);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get CpuBl Addr %r\n",
      __FUNCTION__,
      Status
      ));
    goto Done;
  }

  Status = GetPartitionInfoStMm (
             (UINTN)CpuBlParamsAddr,
             TEGRABL_OEM,
             &DeviceInstance,
             &PartitionByteOffset,
             &PartitionSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:Failed to get %u PartitionInfo %r\n",
      __FUNCTION__,
      TEGRABL_OEM,
      Status
      ));

    goto Done;
  }

  Status = OemPartitionInitProtocol (
             NorFlashProtocol,
             PartitionByteOffset,
             PartitionSize
             );

Done:
  return Status;
}

/**
  Communication service MMI Handler entry.

  This MMI handler provides services for Oem partition access.

  @param[in]     DispatchHandle  The unique handle assigned to this handler by MmiHandlerRegister().
  @param[in]     RegisterContext Points to an optional handler context which was specified when the
                                 handler was registered.
  @param[in, out] CommBuffer     A pointer to a collection of data in memory that will
                                 be conveyed from a non-MM environment into an MM environment.
  @param[in, out] CommBufferSize The size of the CommBuffer.

  @retval EFI_SUCCESS                         The interrupt was handled and quiesced. No other handlers
                                              should still be called.
  @retval EFI_WARN_INTERRUPT_SOURCE_QUIESCED  The interrupt has been quiesced but other handlers should
                                              still be called.
  @retval EFI_WARN_INTERRUPT_SOURCE_PENDING   The interrupt is still pending and other handlers should still
                                              be called.
  @retval EFI_INTERRUPT_PENDING               The interrupt could not be quiesced.
**/
STATIC
EFI_STATUS
EFIAPI
MmOemPartitionHandler (
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  )
{
  EFI_STATUS                        Status;
  OEM_PARTITION_COMMUNICATE_HEADER  *MmFunctionHeader;
  UINTN                             CommBufferPayloadSize;
  UINTN                             TempCommBufferSize;
  OEM_PARTITION_COMMUNICATE_BUFFER  *DataPayload;

  DataPayload = NULL;
  //
  // If input is invalid, stop processing this SMI
  //
  if ((CommBuffer == NULL) || (CommBufferSize == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Input is INVALID", __FUNCTION__));
    return EFI_SUCCESS;
  }

  TempCommBufferSize = *CommBufferSize;

  if (TempCommBufferSize < sizeof (OEM_PARTITION_COMMUNICATE_HEADER)) {
    DEBUG ((DEBUG_ERROR, "%a: MM communication buffer size invalid!\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  CommBufferPayloadSize = TempCommBufferSize - sizeof (OEM_PARTITION_COMMUNICATE_HEADER);

  Status           = EFI_SUCCESS;
  MmFunctionHeader = (OEM_PARTITION_COMMUNICATE_HEADER *)CommBuffer;

  switch (MmFunctionHeader->Function) {
    case OEM_PARTITION_FUNC_GET_INFO:
      if (CommBufferPayloadSize != sizeof (OEM_PARTITION_COMMUNICATE_GET_INFO)) {
        DEBUG ((DEBUG_ERROR, "%a: Command [%d], payload buffer invalid!\n", __FUNCTION__, MmFunctionHeader->Function));
        Status = EFI_INVALID_PARAMETER;
        break;
      }

      DataPayload                            = (OEM_PARTITION_COMMUNICATE_BUFFER *)(MmFunctionHeader + 1);
      DataPayload->Info.PartitionBaseAddress = mOemPartitionPrivate.PartitionBaseAddress;
      DataPayload->Info.PartitionSize        = mOemPartitionPrivate.PartitionSize;
      DataPayload->Info.BlockSize            = mOemPartitionPrivate.BlockSize;
      DataPayload->Info.NumBlocks            = mOemPartitionPrivate.NumBlocks;
      Status                                 = EFI_SUCCESS;
      break;
    case OEM_PARTITION_FUNC_READ:
      if (CommBufferPayloadSize != sizeof (OEM_PARTITION_COMMUNICATE_READ)) {
        DEBUG ((DEBUG_ERROR, "%a: Command [%d], payload buffer invalid!\n", __FUNCTION__, MmFunctionHeader->Function));
        Status = EFI_INVALID_PARAMETER;
        break;
      }

      DataPayload = (OEM_PARTITION_COMMUNICATE_BUFFER *)(MmFunctionHeader + 1);
      Status      = mOemPartitionMmProtocol.Read (DataPayload->Read.Data, DataPayload->Read.Offset, DataPayload->Read.Length);
      break;
    case OEM_PARTITION_FUNC_WRITE:
      if (CommBufferPayloadSize != sizeof (OEM_PARTITION_COMMUNICATE_WRITE)) {
        DEBUG ((DEBUG_ERROR, "%a: Command [%d], payload buffer invalid!\n", __FUNCTION__, MmFunctionHeader->Function));
        Status = EFI_INVALID_PARAMETER;
        break;
      }

      DataPayload = (OEM_PARTITION_COMMUNICATE_BUFFER *)(MmFunctionHeader + 1);
      Status      = mOemPartitionMmProtocol.Write (DataPayload->Write.Data, DataPayload->Write.Offset, DataPayload->Write.Length);
      break;
    case OEM_PARTITION_FUNC_ERASE:
      if (CommBufferPayloadSize != sizeof (OEM_PARTITION_COMMUNICATE_ERASE)) {
        DEBUG ((DEBUG_ERROR, "%a: Command [%d], payload buffer invalid!\n", __FUNCTION__, MmFunctionHeader->Function));
        Status = EFI_INVALID_PARAMETER;
        break;
      }

      DataPayload = (OEM_PARTITION_COMMUNICATE_BUFFER *)(MmFunctionHeader + 1);
      Status      = mOemPartitionMmProtocol.Erase (DataPayload->Erase.Offset, DataPayload->Erase.Length);
      break;
    case OEM_PARTITION_FUNC_IS_ERASED:
      DEBUG ((DEBUG_ERROR, "%a: Command [%d], payload buffer invalid!\n", __FUNCTION__, MmFunctionHeader->Function));
      if (CommBufferPayloadSize != sizeof (OEM_PARTITION_COMMUNICATE_IS_ERASED)) {
        Status = EFI_INVALID_PARAMETER;
        break;
      }

      DataPayload = (OEM_PARTITION_COMMUNICATE_BUFFER *)(MmFunctionHeader + 1);
      Status      = mOemPartitionMmProtocol.IsErased (DataPayload->IsErased.Offset, DataPayload->IsErased.Length);
      break;

    default:
      Status = EFI_UNSUPPORTED;
      break;
  }

  MmFunctionHeader->ReturnStatus = Status;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
OemPartitionMmInitialize (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle = NULL;

  ZeroMem (&mOemPartitionMmProtocol, sizeof (mOemPartitionMmProtocol));
  ZeroMem (&mOemPartitionPrivate, sizeof (mOemPartitionPrivate));

  //
  // Get info required for communicating with Spinor
  //
  Status = OemPartitionLocateStorage ();
  if (!EFI_ERROR (Status)) {
    mOemPartitionMmProtocol.Info     = OemPartitionInfo;
    mOemPartitionMmProtocol.Read     = OemPartitionReadSpiNor;
    mOemPartitionMmProtocol.Write    = OemPartitionWriteSpiNor;
    mOemPartitionMmProtocol.Erase    = OemPartitionEraseSpiNor;
    mOemPartitionMmProtocol.IsErased = OemPartitionIsErasedSpiNor;

    Status = gMmst->MmInstallProtocolInterface (
                      &mOemPartitionPrivate.Handle,
                      &gNVIDIAOemPartitionProtocolGuid,
                      EFI_NATIVE_INTERFACE,
                      &mOemPartitionMmProtocol
                      );
    if (!EFI_ERROR (Status)) {
      //
      // Register Oem partition access MM handler
      //
      Status = gMmst->MmiHandlerRegister (MmOemPartitionHandler, &gNVIDIAOemPartitionGuid, &Handle);
      if (EFI_ERROR (Status)) {
        ASSERT_EFI_ERROR (Status);
      }
    }
  }

  /* Always return success from the Init function, due to issues with Init failure is acceptable*/
  return EFI_SUCCESS;
}
