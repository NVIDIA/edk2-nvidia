/** @file
  UEFI payloads decryption Library

  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/HandleParsingLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PrintLib.h>
#include <Library/OpteeNvLib.h>
#include <Library/FileHandleLib.h>
#include <libfdt_env.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
#include "L4TOpteeDecrypt.h"

/*
 *
  AllocateAlignedPagesForSharedMemory

  Utility function to allocate pages for shared memory between UEFI and Optee.

  @param[in]  Session         Session used to connect to Optee TA
  @param[in]  DataSize        DataSize of communication buffer.

  @retval EFI_SUCCESS          The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES  Failed buffer allocation.
 *
 */
STATIC
EFI_STATUS
AllocateAlignedPagesForSharedMemory (
  IN OPTEE_SESSION            **Session,
  IN UINT64            CONST  DataSize
  )
{
  VOID           *OpteeBuf;
  UINTN          TotalOpteeBufSize = 0;
  UINTN          MsgCookieSizePg   = EFI_SIZE_TO_PAGES (sizeof (OPTEE_SHM_COOKIE));
  UINTN          OpteeMsgBufSizePg = EFI_SIZE_TO_PAGES (sizeof (OPTEE_MESSAGE_ARG));
  UINTN          ShmPageListSizePg = EFI_SIZE_TO_PAGES (sizeof (OPTEE_SHM_PAGE_LIST));
  UINTN          CommBufSizePg     = 0;
  OPTEE_SESSION  *OpteeSession     = NULL;

  OpteeSession = AllocatePool (sizeof (OPTEE_SESSION));
  if (OpteeSession == NULL) {
    ErrorPrint (L"%a: Failed to allocate buffer\r\n", __FUNCTION__);
    return EFI_OUT_OF_RESOURCES;
  }

  CommBufSizePg     = EFI_SIZE_TO_PAGES (DataSize);
  TotalOpteeBufSize = OpteeMsgBufSizePg + MsgCookieSizePg + CommBufSizePg
                      + ShmPageListSizePg;
  /* Allocate one contiguous buffer. */
  OpteeBuf = AllocateAlignedRuntimePages (TotalOpteeBufSize, OPTEE_MSG_PAGE_SIZE);
  if (OpteeBuf == NULL) {
    ErrorPrint (L"%a: Failed to allocate buffer\r\n", __FUNCTION__);
    FreePool (OpteeSession);
    OpteeSession = NULL;
    return EFI_OUT_OF_RESOURCES;
  }

  ZeroMem (OpteeSession, sizeof (OPTEE_SESSION));
  OpteeSession->OpteeMsgArgPa = OpteeBuf;
  OpteeSession->OpteeMsgArgVa = OpteeSession->OpteeMsgArgPa;
  OpteeSession->TotalSize     = EFI_PAGES_TO_SIZE (TotalOpteeBufSize);
  OpteeSession->CommBufPa     = OpteeBuf + EFI_PAGES_TO_SIZE (OpteeMsgBufSizePg);
  OpteeSession->CommBufVa     = OpteeSession->CommBufPa;
  OpteeSession->CommBufSize   = EFI_PAGES_TO_SIZE (CommBufSizePg);
  OpteeSession->MsgCookiePa   = OpteeBuf + EFI_PAGES_TO_SIZE (OpteeMsgBufSizePg) +
                                EFI_PAGES_TO_SIZE (CommBufSizePg);
  OpteeSession->MsgCookieVa = OpteeSession->MsgCookiePa;
  OpteeSession->ShmListPa   = OpteeBuf +  EFI_PAGES_TO_SIZE (OpteeMsgBufSizePg) +
                              EFI_PAGES_TO_SIZE (CommBufSizePg) +
                              EFI_PAGES_TO_SIZE (ShmPageListSizePg);
  OpteeSession->ShmListVa         = OpteeSession->ShmListPa;
  OpteeSession->MsgCookiePa->Addr = OpteeSession->MsgCookieVa->Addr = OpteeSession->CommBufPa;
  OpteeSession->MsgCookiePa->Size = OpteeSession->MsgCookieVa->Size =
    EFI_PAGES_TO_SIZE (CommBufSizePg);

  *Session = OpteeSession;

  return EFI_SUCCESS;
}

/*
 *
  GetImageEncryptionInfo

  Utility function to get the encryption information of the image

  @param[out]  Info            The information of the image

  @retval EFI_SUCCESS          The operation completed successfully.
          EFI_OUT_OF_RESOURCES Failed buffer allocation.
          EFI_XXX              Error status from other APIs called.
 *
 */
EFI_STATUS
EFIAPI
GetImageEncryptionInfo (
  OUT ImageEncryptionInfo  *Info
  )
{
  EFI_STATUS              Status;
  UINT64                  Capabilities = 0;
  OPTEE_MESSAGE_ARG       *MessageArg  = NULL;
  OPTEE_OPEN_SESSION_ARG  OpenSessionArg;
  EFI_GUID                CPD_TA_UUID   = TA_CPUBL_PAYLOAD_DECRYPTION_UUID;
  OPTEE_SESSION           *OpteeSession = NULL;
  UINTN                   ChipID;

  if (!IsOpteePresent ()) {
    ErrorPrint (L"%a: optee is not present\r\n", __FUNCTION__);
    return EFI_UNSUPPORTED;
  }

  if (!OpteeExchangeCapabilities (&Capabilities)) {
    ErrorPrint (L"%a: Failed to exchange capabilities with OP-TEE\r\n", __FUNCTION__);
    return EFI_UNSUPPORTED;
  }

  if (!(Capabilities & OPTEE_SMC_SEC_CAP_DYNAMIC_SHM)) {
    ErrorPrint (L"%a: optee does not supported danamic shm\r\n", __FUNCTION__);
    return EFI_UNSUPPORTED;
  }

  Status = AllocateAlignedPagesForSharedMemory (&OpteeSession, 0);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to allocate shared memory%r\r\n", __FUNCTION__, Status);
    return Status;
  }

  Status = OpteeSetProperties (
             (UINT64)OpteeSession->OpteeMsgArgPa,
             (UINT64)OpteeSession->OpteeMsgArgVa,
             OpteeSession->TotalSize
             );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to set properties %r\r\n", __FUNCTION__, Status);
    goto FreeMemory;
  }

  ZeroMem (&OpenSessionArg, sizeof (OPTEE_OPEN_SESSION_ARG));
  CopyMem (&OpenSessionArg.Uuid, &CPD_TA_UUID, sizeof (EFI_GUID));
  Status = OpteeOpenSession (&OpenSessionArg);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to open session %r\r\n", __FUNCTION__, Status);
    goto FreeMemory;
  } else {
    if (OpenSessionArg.Return != OPTEE_SUCCESS) {
      ErrorPrint (
        L"%a: Failed to open session to cpubl payload decryption TA %u\r\n",
        __FUNCTION__,
        OpenSessionArg.Return
        );
      Status = EFI_UNSUPPORTED;
      goto FreeMemory;
    }
  }

  MessageArg = OpteeSession->OpteeMsgArgVa;
  ZeroMem (MessageArg, sizeof (OPTEE_MESSAGE_ARG));
  MessageArg->Command                 = OPTEE_MESSAGE_COMMAND_INVOKE_FUNCTION;
  MessageArg->Function                = CPUBL_PAYLOAD_DECRYPTION_CMD_IS_IMAGE_DECRYPT_ENABLE;
  MessageArg->Session                 = OpenSessionArg.Session;
  MessageArg->Params[0].Attribute     = OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT;
  MessageArg->Params[0].Union.Value.A = EKB_USER_KEY_KERNEL_ENCRYPTION;
  MessageArg->Params[1].Attribute     = OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_OUTPUT;
  MessageArg->NumParams               = 2;

  if (OpteeCallWithArg ((UINTN)OpteeSession->OpteeMsgArgPa) != 0) {
    MessageArg->Return       = OPTEE_ERROR_COMMUNICATION;
    MessageArg->ReturnOrigin = OPTEE_ORIGIN_COMMUNICATION;
    Status                   = EFI_ACCESS_DENIED;
    ErrorPrint (L"%a: Optee call failed with Status = %r\r\n", __FUNCTION__, Status);
    goto CloseSession;
  }

  Info->ImageEncrypted    = FALSE;
  Info->ImageHeaderSize   = 0;
  Info->ImageLengthOffset = 0;

  if (MessageArg->Params[1].Union.Value.A == 1) {
    Info->ImageEncrypted = TRUE;

    ChipID = TegraGetChipID ();
    if (ChipID == T194_CHIP_ID) {
      Info->ImageHeaderSize   = BOOT_COMPONENT_HEADER_SIZE_4K;
      Info->ImageLengthOffset = BINARY_LEN_OFFSET_IN_4K_BCH;
    } else {
      Info->ImageHeaderSize   = BOOT_COMPONENT_HEADER_SIZE_8K;
      Info->ImageLengthOffset = BINARY_LEN_OFFSET_IN_8K_BCH;
    }
  }

CloseSession:
  OpteeCloseSession (MessageArg->Session);

FreeMemory:
  if (OpteeSession != NULL) {
    FreeAlignedPages (OpteeSession->OpteeMsgArgVa, EFI_SIZE_TO_PAGES (OpteeSession->TotalSize));
    FreePool (OpteeSession);
    OpteeSession = NULL;
  }

  return Status;
}

/*
 *
  OpteeDecryptImageInit: Utility function to init the decrypt operation.

  As the encrypted image header size is 8K, in order to initialze the decrypt
  operation, Optee need to get the whole image header. In other words, the caller
  should ensure that the SrcBufferSize is not less than 8K. For convenice, it is
  better to set the size to 8K.

  If there is an error in this util function, then it will make sure the DstBuffer
  is empty with zerolize the buffer.

  @param[in]  Session          Optee Session used for invoke OP-TEE
  @param[in]  SrcBuffer        Source Buffer with Encrypt Image
  @param[in]  SrcFileSize      Size of the Source Buffer
  @param[out] DstBuffer        Destination Buffer with decrypt Data.
  @param[out] DstFileSize      Size of the Destination Buffer.

  @retval EFI_SUCCESS          The operation completed successfully.
          EFI_OUT_OF_RESOURCES Failed buffer allocation.
          EFI_XXX              Error status from other APIs called.
 *
 */
STATIC
EFI_STATUS
OpteeDecryptImageInit (
  IN OPTEE_SESSION  *Session,
  IN VOID           *SrcBuffer,
  IN UINT64         SrcFileSize,
  OUT VOID          *DstBuffer,
  OUT UINT64        *DstFileSize
  )
{
  EFI_STATUS              Status;
  OPTEE_MESSAGE_ARG       *MessageArg = NULL;
  OPTEE_OPEN_SESSION_ARG  OpenSessionArg;
  EFI_GUID                CPD_TA_UUID   = TA_CPUBL_PAYLOAD_DECRYPTION_UUID;
  OPTEE_SESSION           *OpteeSession = Session;

  Status = OpteeSetProperties (
             (UINT64)OpteeSession->OpteeMsgArgPa,
             (UINT64)OpteeSession->OpteeMsgArgVa,
             OpteeSession->TotalSize
             );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to set properties %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  Status = OpteeRegisterShm (
             OpteeSession->CommBufPa,
             (UINT64)OpteeSession->MsgCookiePa,
             OpteeSession->CommBufSize,
             OpteeSession->ShmListPa
             );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to register share memory %r\r\n", __FUNCTION__, Status);
    return Status;
  }

  ZeroMem (&OpenSessionArg, sizeof (OPTEE_OPEN_SESSION_ARG));
  CopyMem (&OpenSessionArg.Uuid, &CPD_TA_UUID, sizeof (EFI_GUID));
  Status = OpteeOpenSession (&OpenSessionArg);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to open session %r\r\n", __FUNCTION__, Status);
    goto UnRegisterShm;
  } else {
    if (OpenSessionArg.Return != OPTEE_SUCCESS) {
      ErrorPrint (L"%a: Failed to open session to secure boot TA %u\r\n", __FUNCTION__, OpenSessionArg.Return);
      Status = EFI_UNSUPPORTED;
      goto UnRegisterShm;
    }
  }

  MessageArg = OpteeSession->OpteeMsgArgVa;
  ZeroMem (MessageArg, sizeof (OPTEE_MESSAGE_ARG));
  MessageArg->Command                                      = OPTEE_MESSAGE_COMMAND_INVOKE_FUNCTION;
  MessageArg->Function                                     = CPUBL_PAYLOAD_DECRYPTION_CMD_DECRYPT_IMAGES;
  MessageArg->Session                                      = OpenSessionArg.Session;
  MessageArg->Params[0].Attribute                          = OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INOUT;
  MessageArg->Params[0].Union.Memory.Size                  = SrcFileSize;
  MessageArg->Params[0].Union.Memory.SharedMemoryReference = (UINT64)OpteeSession->MsgCookiePa;
  MessageArg->Params[1].Attribute                          = OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT;
  MessageArg->Params[1].Union.Value.A                      = JETSON_CPUBL_PAYLOAD_DECRYPTION_INIT;
  MessageArg->NumParams                                    = 2;

  if (OpteeCallWithArg ((UINTN)OpteeSession->OpteeMsgArgPa) != 0) {
    MessageArg->Return       = OPTEE_ERROR_COMMUNICATION;
    MessageArg->ReturnOrigin = OPTEE_ORIGIN_COMMUNICATION;
    Status                   = EFI_ACCESS_DENIED;
    ErrorPrint (L"%a: Optee call failed with Status = %r\r\n", __FUNCTION__, Status);
    goto CloseSession;
  }

  return EFI_SUCCESS;

CloseSession:
  OpteeCloseSession (MessageArg->Session);

UnRegisterShm:
  OpteeUnRegisterShm ((UINT64)OpteeSession->MsgCookiePa);

  return Status;
}

/*
 *
  OpteeDecryptImageUpdate

  Core utility function to decrypt the encrypted images, such as kernel & kernel-DTB
  binary. The caller can decrypt the images block by block by call this function in
  a loop.

  If there is an error in this util function, then it will make sure the DstBuffer
  is empty with zerolize the buffer.

  @param[in]  Session          Optee Session used for invoke OP-TEE
  @param[in]  SrcBuffer        Source Buffer with Encrypt Image
  @param[in]  SrcFileSize      Size of the Source Buffer
  @param[out] DstBuffer        Destination Buffer with decrypt Data.
  @param[out] DstFileSize      Size of the Destination Buffer.

  @retval EFI_SUCCESS          The operation completed successfully.
          EFI_OUT_OF_RESOURCES Failed buffer allocation.
          EFI_XXX              Error status from other APIs called.
 *
 */
STATIC
EFI_STATUS
OpteeDecryptImageUpdate (
  IN OPTEE_SESSION  *Session,
  IN VOID           *SrcBuffer,
  IN UINT64         SrcFileSize,
  OUT VOID          *DstBuffer,
  OUT UINT64        *DstFileSize
  )
{
  EFI_STATUS         Status;
  OPTEE_MESSAGE_ARG  *MessageArg   = NULL;
  OPTEE_SESSION      *OpteeSession = Session;

  MessageArg                                               = OpteeSession->OpteeMsgArgVa;
  MessageArg->Command                                      = OPTEE_MESSAGE_COMMAND_INVOKE_FUNCTION;
  MessageArg->Function                                     = CPUBL_PAYLOAD_DECRYPTION_CMD_DECRYPT_IMAGES;
  MessageArg->Params[0].Attribute                          = OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INOUT;
  MessageArg->Params[0].Union.Memory.Size                  = SrcFileSize;
  MessageArg->Params[0].Union.Memory.SharedMemoryReference = (UINT64)OpteeSession->MsgCookiePa;
  MessageArg->Params[1].Attribute                          = OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT;
  MessageArg->Params[1].Union.Value.A                      = JETSON_CPUBL_PAYLOAD_DECRYPTION_UPDATE;
  MessageArg->NumParams                                    = 2;

  if (OpteeCallWithArg ((UINTN)OpteeSession->OpteeMsgArgPa) != 0) {
    MessageArg->Return       = OPTEE_ERROR_COMMUNICATION;
    MessageArg->ReturnOrigin = OPTEE_ORIGIN_COMMUNICATION;
    Status                   = EFI_ACCESS_DENIED;
    ErrorPrint (L"%a: Optee call failed with Status = %r\r\n", __FUNCTION__, Status);
    goto Error;
  }

  *DstFileSize = MessageArg->Params[0].Union.Memory.Size;

  return EFI_SUCCESS;

Error:
  OpteeCloseSession (MessageArg->Session);
  OpteeUnRegisterShm ((UINT64)OpteeSession->MsgCookiePa);

  return Status;
}

/*
 *
  OpteeDecryptImageFinal

  Utility function to complete the decrypt operation. For a decryption operation,
  this function must be call at the end.

  If there is an error in this util function, then it will make sure the DstBuffer
  is empty with zerolize the buffer.

  @param[in]  Session          Optee Session used for invoke OP-TEE
  @param[in]  SrcBuffer        Source Buffer with Encrypt Image
  @param[in]  SrcFileSize      Size of the Source Buffer
  @param[out] DstBuffer        Destination Buffer with decrypt Data.
  @param[out] DstFileSize      Size of the Destination Buffer.

  @retval EFI_SUCCESS          The operation completed successfully.
          EFI_OUT_OF_RESOURCES Failed buffer allocation.
          EFI_XXX              Error status from other APIs called.
 *
 */
STATIC
EFI_STATUS
OpteeDecryptImageFinal (
  IN OPTEE_SESSION  *Session,
  IN VOID           *SrcBuffer,
  IN UINT64         SrcFileSize,
  OUT VOID          *DstBuffer,
  OUT UINT64        *DstFileSize
  )
{
  EFI_STATUS         Status        = EFI_SUCCESS;
  OPTEE_MESSAGE_ARG  *MessageArg   = NULL;
  OPTEE_SESSION      *OpteeSession = Session;

  MessageArg                                               = OpteeSession->OpteeMsgArgVa;
  MessageArg->Command                                      = OPTEE_MESSAGE_COMMAND_INVOKE_FUNCTION;
  MessageArg->Function                                     = CPUBL_PAYLOAD_DECRYPTION_CMD_DECRYPT_IMAGES;
  MessageArg->Params[0].Attribute                          = OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INOUT;
  MessageArg->Params[0].Union.Memory.Size                  = SrcFileSize;
  MessageArg->Params[0].Union.Memory.SharedMemoryReference = (UINT64)OpteeSession->MsgCookiePa;
  MessageArg->Params[1].Attribute                          = OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT;
  MessageArg->Params[1].Union.Value.A                      = JETSON_CPUBL_PAYLOAD_DECRYPTION_FINAL;
  MessageArg->NumParams                                    = 2;

  if (OpteeCallWithArg ((UINTN)OpteeSession->OpteeMsgArgPa) != 0) {
    MessageArg->Return       = OPTEE_ERROR_COMMUNICATION;
    MessageArg->ReturnOrigin = OPTEE_ORIGIN_COMMUNICATION;
    Status                   = EFI_ACCESS_DENIED;
    ErrorPrint (L"%a: Optee call failed with Status = %r\r\n", __FUNCTION__, Status);
  }

  *DstFileSize = MessageArg->Params[0].Union.Memory.Size;

  OpteeCloseSession (MessageArg->Session);
  OpteeUnRegisterShm ((UINT64)OpteeSession->MsgCookiePa);

  return Status;
}

/*
 *
  ReadEncryptedImage: The helper function to read image from the file system or
  partition.

  @param[in]  Handle           Handle of encrypted image file
  @param[in]  DiskIo           DiskIo structure of encrypted image in partition
  @param[in]  BlockIo          BlockIo structure of encrypted image in partition
  @param[in]  Offset           Offset of the image read from partition.
  @param[in]  BufferSize       The size of the buffer.
  @param[out] Buffer           The buffer that store the image.

  @retval EFI_SUCCESS          The operation completed successfully.
          EFI_OUT_OF_RESOURCES Failed buffer allocation.
          EFI_XXX              Error status from other APIs called.
 *
 */
STATIC
EFI_STATUS
ReadEncryptedImage (
  IN EFI_FILE_HANDLE        *Handle OPTIONAL,
  IN EFI_DISK_IO_PROTOCOL   *DiskIo OPTIONAL,
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo OPTIONAL,
  IN UINT64                 Offset OPTIONAL,
  IN UINT64                 *BufferSize,
  OUT VOID                  *Buffer
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if ((Handle == NULL) && ((BlockIo == NULL) && (DiskIo == NULL))) {
    ErrorPrint (L"%a: Handle and BlockIo&DiskIo can not be NULL at same time\r\n", __FUNCTION__);
    return EFI_INVALID_PARAMETER;
  }

  if ((Buffer == NULL) || (BufferSize == NULL)) {
    ErrorPrint (L"%a: Buffer and BufferSize can not be NULL\r\n", __FUNCTION__);
    return EFI_INVALID_PARAMETER;
  }

  if (Handle != NULL) {
    Status = FileHandleRead (*Handle, BufferSize, Buffer);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to read data from file system\r\n", __FUNCTION__);
    }
  } else {
    Status = DiskIo->ReadDisk (
                       DiskIo,
                       BlockIo->Media->MediaId,
                       Offset,
                       *BufferSize,
                       Buffer
                       );
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to read data from partition\r\n", __FUNCTION__);
    }
  }

  return Status;
}

/*
 *
  OpteeDecryptImage: Utility function to decrypt uefi payload.

  If there is an error in this util function, then it will make sure the DstBuffer
  is empty with zerolize the buffer.

  @param[in]  Handle           Handle of encrypted image file
  @param[in]  DiskIo           DiskIo structure of encrypted image in partition
  @param[in]  BlockIo          BlockIo structure of encrypted image in partition
  @param[in]  ImageHeaderSize  The image header size of the encrypted image
  @param[in]  SrcFileSize      File Size of encrypted image file
  @param[out] DstBuffer        Destination Buffer of decrypt image
  @param[out] DstFileSize      Size of the decrypt image

  @retval EFI_SUCCESS          The operation completed successfully.
          EFI_XXX              Othrer Error status.
 *
 */
EFI_STATUS
EFIAPI
OpteeDecryptImage (
  IN EFI_FILE_HANDLE        *Handle OPTIONAL,
  IN EFI_DISK_IO_PROTOCOL   *DiskIo OPTIONAL,
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo OPTIONAL,
  IN UINTN                  ImageHeaderSize,
  IN UINT64                 SrcFileSize,
  OUT VOID                  **DstBuffer,
  OUT UINT64                *DstFileSize
  )
{
  EFI_STATUS     Status         = EFI_SUCCESS;
  OPTEE_SESSION  *OpteeSession  = NULL;
  VOID           *Data          = NULL;
  VOID           *Block         = NULL;
  UINT64         BlockSize      = OPTEE_DECRYPT_UPDATE_BLOCK_SIZE;
  UINT64         FirstBlockSize = ImageHeaderSize;
  UINT64         LastBlockSize;
  UINT64         num_block, i;
  UINT64         OutSize = 0;
  UINT64         Offset  = 0;

  if ((Handle == NULL) && ((BlockIo == NULL) && (DiskIo == NULL))) {
    ErrorPrint (L"%a: Handle and BlockIo&DiskIo can not be NULL at same time\r\n", __FUNCTION__);
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  if ((*DstBuffer == NULL) || (DstFileSize == NULL)) {
    ErrorPrint (L"%a: DstBuffer and DstFileSize can not be NULL\r\n", __FUNCTION__);
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  if (SrcFileSize < ImageHeaderSize) {
    ErrorPrint (L"%a: SrcFileSize can not be less than 8K \r\n", __FUNCTION__);
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  Status = AllocateAlignedPagesForSharedMemory (&OpteeSession, BlockSize);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to allocate shared memoery\r\n", __FUNCTION__);
    goto Exit;
  }

  *DstFileSize = 0;
  Data         = *DstBuffer;
  Block        = OpteeSession->CommBufVa;
  num_block    = (SrcFileSize - FirstBlockSize) / BlockSize;

  if ((SrcFileSize - FirstBlockSize) % BlockSize) {
    num_block    += 1;
    LastBlockSize = (SrcFileSize - FirstBlockSize) % BlockSize;
  } else {
    LastBlockSize = BlockSize;
  }

  Status = ReadEncryptedImage (
             Handle,
             DiskIo,
             BlockIo,
             Offset,
             &FirstBlockSize,
             Block
             );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to read data\r\n", __FUNCTION__);
    goto Exit;
  }

  Offset += FirstBlockSize;

  Status = OpteeDecryptImageInit (OpteeSession, Block, FirstBlockSize, Block, &OutSize);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: OpteeDecryptImageInit failed\r\n", __FUNCTION__);
    goto Exit;
  }

  for (i = 0; i < num_block - 1; i++) {
    Status = ReadEncryptedImage (
               Handle,
               DiskIo,
               BlockIo,
               Offset,
               &BlockSize,
               Block
               );
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: Failed to read data\r\n", __FUNCTION__);
      goto Exit;
    }

    Offset += BlockSize;

    Status = OpteeDecryptImageUpdate (OpteeSession, Block, BlockSize, Block, &OutSize);
    if (EFI_ERROR (Status)) {
      ErrorPrint (L"%a: OpteeDecryptImageUpdate failed\r\n", __FUNCTION__);
      goto Exit;
    }

    memcpy (Data + *DstFileSize, Block, OutSize);
    *DstFileSize += OutSize;
  }

  Status = ReadEncryptedImage (
             Handle,
             DiskIo,
             BlockIo,
             Offset,
             &LastBlockSize,
             Block
             );
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: Failed to read data\r\n", __FUNCTION__);
    goto Exit;
  }

  Status = OpteeDecryptImageFinal (OpteeSession, Block, LastBlockSize, Block, &OutSize);
  if (EFI_ERROR (Status)) {
    ErrorPrint (L"%a: OpteeDecryptImageFinal failed\r\n", __FUNCTION__);
    goto Exit;
  }

  memcpy (Data + *DstFileSize, Block, OutSize);
  *DstFileSize += OutSize;

Exit:
  if (OpteeSession != NULL) {
    FreeAlignedPages (OpteeSession->OpteeMsgArgVa, EFI_SIZE_TO_PAGES (OpteeSession->TotalSize));
    FreePool (OpteeSession);
    OpteeSession = NULL;
  }

  return Status;
}
