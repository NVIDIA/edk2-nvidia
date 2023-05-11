/** @file
  Api's to communicate with OP-TEE OS (Trusted OS based on ARM TrustZone) via
  secure monitor calls to send data to fTPM TA

  Copyright (c) 2023, NVIDIA Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/Tpm2DeviceLib.h>
#include <Library/Tcg2PhysicalPresenceLib.h>
#include <Library/OpteeNvLib.h>

#define FTPM_SUBMIT_COMMAND  (0)

STATIC VOID                    *OpteeFtpmShmIpBuf       = NULL;
STATIC VOID                    *OpteeFtpmShmOpBuf       = NULL;
STATIC UINT64                  OpteeFtpmShmInputCookie  = 0;
STATIC UINT64                  OpteeFtpmShmOutputCookie = 0;
STATIC OPTEE_MESSAGE_ARG       *FtpmOpteeMessage        = NULL;
STATIC OPTEE_SHM_PAGE_LIST     *PageList;
STATIC UINTN                   OpteeFtpmShmInputSize;
STATIC OPTEE_OPEN_SESSION_ARG  OpteeSessionParam;
STATIC EFI_EVENT               ExitBootServicesEvent = NULL;

/**
  Function to open a session to the fTPM TA in OP-TEE.


  @return EFI_SUCCESS On success, also populates the OpteeSessionParam with
                      the open parameters.
          Other       Failed to communicate with the fTPM TA.
 **/
STATIC
EFI_STATUS
OpenSessionToFtpm (
  VOID
  )
{
  EFI_STATUS  Status;

  ZeroMem (&OpteeSessionParam, sizeof (OPTEE_OPEN_SESSION_ARG));
  CopyMem (&OpteeSessionParam.Uuid, &gNVIDIAFtpmOpteeGuid, sizeof (EFI_GUID));
  Status = OpteeOpenSession (&OpteeSessionParam);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to open %r\n", __FUNCTION__, Status));
  } else {
    if (OpteeSessionParam.Return != OPTEE_SUCCESS) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to open fTPM ssesion , OP-TEE return %u\n",
        __FUNCTION__,
        OpteeSessionParam.Return
        ));
      Status = EFI_UNSUPPORTED;
    }
  }

  return Status;
}

/**
  This service enables the sending of commands to the TPM2.

  @param[in]      InputParameterBlockSize  Size of the TPM2 input parameter block.
  @param[in]      InputParameterBlock      Pointer to the TPM2 input parameter block.
  @param[in,out]  OutputParameterBlockSize Size of the TPM2 output parameter block.
  @param[in]      OutputParameterBlock     Pointer to the TPM2 output parameter block.

  @retval EFI_SUCCESS            The command byte stream was successfully sent to the device and a response was successfully received.
  @retval EFI_DEVICE_ERROR       The command was not successfully sent to the device or a response was not successfully received from the device.
  @retval EFI_BUFFER_TOO_SMALL   The output parameter block is too small.
**/
EFI_STATUS
EFIAPI
Tpm2SubmitCommand (
  IN UINT32      InputParameterBlockSize,
  IN UINT8       *InputParameterBlock,
  IN OUT UINT32  *OutputParameterBlockSize,
  IN UINT8       *OutputParameterBlock
  )
{
  OPTEE_MESSAGE_ARG  *Message;
  UINT8              *ShmInput;
  UINT8              *ShmOutput;
  EFI_STATUS         Status;

  Status = EFI_SUCCESS;
  if (InputParameterBlockSize > OpteeFtpmShmInputSize) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: InSufficient ShmSize. Requested %u Avail %u\n",
      __FUNCTION__,
      InputParameterBlockSize,
      OpteeFtpmShmInputSize
      ));
    Status = EFI_OUT_OF_RESOURCES;
    ASSERT (FALSE);
    goto ExitTpm2SubmitCommand;
  }

  Message = FtpmOpteeMessage;
  ZeroMem (Message, sizeof (OPTEE_MESSAGE_ARG));

  Message->Command  = OPTEE_MESSAGE_COMMAND_INVOKE_FUNCTION;
  Message->Function = FTPM_SUBMIT_COMMAND;
  Message->Session  = OpteeSessionParam.Session;
  ShmInput          = (UINT8 *)OpteeFtpmShmIpBuf;
  ShmOutput         = (UINT8 *)OpteeFtpmShmOpBuf;
  ZeroMem (ShmInput, OpteeFtpmShmInputSize);
  ZeroMem (ShmOutput, OpteeFtpmShmInputSize);

  CopyMem (ShmInput, InputParameterBlock, InputParameterBlockSize);

  Message->Params[0].Attribute                          =  OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INPUT;
  Message->Params[0].Union.Memory.Size                  =  InputParameterBlockSize;
  Message->Params[0].Union.Memory.SharedMemoryReference =  OpteeFtpmShmInputCookie;

  Message->Params[1].Attribute                          =  OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INOUT;
  Message->Params[1].Union.Memory.Size                  =  *OutputParameterBlockSize;
  Message->Params[1].Union.Memory.SharedMemoryReference =  OpteeFtpmShmOutputCookie;
  Message->NumParams                                    =  2;

  if (OpteeCallWithArg ((UINTN)FtpmOpteeMessage) != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send message %r\n", __FUNCTION__, Status));
    Status = EFI_DEVICE_ERROR;
    goto ExitTpm2SubmitCommand;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Sending message %u\n",
    __FUNCTION__,
    OpteeSessionParam.Session
    ));
  DEBUG ((
    DEBUG_INFO,
    "%a: Return %u Origin %u \n",
    __FUNCTION__,
    FtpmOpteeMessage->Return,
    FtpmOpteeMessage->ReturnOrigin
    ));
  DEBUG ((
    DEBUG_INFO,
    "%a: Copying %u bytes\n",
    __FUNCTION__,
    *OutputParameterBlockSize
    ));

  if (Message->Params[1].Union.Memory.Size > *OutputParameterBlockSize) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Provided buffer too small %u expected %u\n",
      __FUNCTION__,
      *OutputParameterBlockSize,
      Message->Params[1].Union.Memory.Size
      ));
    Status                    = EFI_BUFFER_TOO_SMALL;
    *OutputParameterBlockSize =  Message->Params[1].Union.Memory.Size;
    goto ExitTpm2SubmitCommand;
  } else {
    *OutputParameterBlockSize =  Message->Params[1].Union.Memory.Size;
    CopyMem (OutputParameterBlock, ShmOutput, *OutputParameterBlockSize);
  }

ExitTpm2SubmitCommand:
  return Status;
}

/**
    AllocateRegisterOpteeBuf

    Allocate and register a shared buffer with Optee.

    @param[in]  BufSize    Size of the buffer to allocate.
    @param[out] BufPhys    Pointer of the buffer allocated on success.
    @param[out] ShmCookie  Cookie used to register with Optee.

    @retval EFI_SUCCESS    On Success.
            Other          On failure.
 **/
STATIC
EFI_STATUS
AllocateRegisterOpteeBuf (
  IN  UINTN   BufSize,
  OUT VOID    **BufPhys,
  OUT UINT64  *ShmCookie
  )
{
  EFI_STATUS        Status;
  VOID              *ShmBuf;
  OPTEE_SHM_COOKIE  *Cookie;

  ShmBuf = AllocateAlignedPages (
             EFI_SIZE_TO_PAGES (BufSize),
             OPTEE_MSG_PAGE_SIZE
             );
  if (ShmBuf == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate Comm Buf"));
    Status = EFI_OUT_OF_RESOURCES;
    goto ExitAllocateRegisterOpteeBuf;
  }

  *BufPhys = ShmBuf;

  Cookie = AllocateAlignedPages (
             EFI_SIZE_TO_PAGES (sizeof (OPTEE_SHM_COOKIE)),
             OPTEE_MSG_PAGE_SIZE
             );
  Cookie->Size = BufSize;
  Cookie->Addr = (UINT8 *)ShmBuf;
  *ShmCookie   = (UINT64)Cookie;

  Status = OpteeRegisterShm (
             ShmBuf,
             *ShmCookie,
             BufSize,
             PageList
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to register I/P Shared Memory buffer %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitAllocateRegisterOpteeBuf;
  }

ExitAllocateRegisterOpteeBuf:
  return Status;
}

/**
 * FreeBuffers
   Free all the buffers allocated.
 */
STATIC
VOID
FreeBuffers (
  VOID
  )
{
  if (OpteeFtpmShmIpBuf != NULL) {
    FreeAlignedPages (
      OpteeFtpmShmIpBuf,
      EFI_SIZE_TO_PAGES (OpteeFtpmShmInputSize)
      );
  }

  if (OpteeFtpmShmOpBuf != NULL) {
    FreeAlignedPages (
      OpteeFtpmShmOpBuf,
      EFI_SIZE_TO_PAGES (OpteeFtpmShmInputSize)
      );
  }

  if (OpteeFtpmShmInputCookie != 0) {
    FreeAlignedPages (
      (VOID *)OpteeFtpmShmInputCookie,
      EFI_SIZE_TO_PAGES (sizeof (OPTEE_SHM_COOKIE))
      );
  }

  if (OpteeFtpmShmOutputCookie != 0) {
    FreeAlignedPages (
      (VOID *)OpteeFtpmShmOutputCookie,
      EFI_SIZE_TO_PAGES (sizeof (OPTEE_SHM_COOKIE))
      );
  }

  if (FtpmOpteeMessage != NULL) {
    FreeAlignedPages (
      FtpmOpteeMessage,
      EFI_SIZE_TO_PAGES (sizeof (OPTEE_MESSAGE_ARG))
      );
  }

  if (PageList != NULL) {
    FreeAlignedPages (
      PageList,
      EFI_SIZE_TO_PAGES (sizeof (OPTEE_SHM_PAGE_LIST))
      );
  }
}

/**
 * ExitBootServicesCallback
   Close the fTPM session and unregister Shared memory buffers.

   @param[in] Event    Event identifier of the callback.
   @param[in] Context  Context data of the callback
 **/
STATIC
VOID
EFIAPI
ExitBootServicesCallBack (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  if (OpteeFtpmShmInputCookie != 0) {
    OpteeUnRegisterShm ((UINT64)OpteeFtpmShmInputCookie);
  }

  if (OpteeFtpmShmOutputCookie != 0) {
    OpteeUnRegisterShm ((UINT64)OpteeFtpmShmOutputCookie);
  }

  if (OpteeSessionParam.Session != 0) {
    OpteeCloseSession (OpteeSessionParam.Session);
  }

  FreeBuffers ();
}

/**
 * AllocatePageAndMessageBuffers
   Each Driver using the OpteeLib should allocate the message
   and Page List buffer.

   @param[]  None

   @retval EFI_SUCCESS           on success.
           EFI_OUT_OF_RESOURCES  on failure.
 **/
STATIC
EFI_STATUS
AllocatePageAndMessageBuffers (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;

  PageList = AllocateAlignedPages (
               EFI_SIZE_TO_PAGES (sizeof (OPTEE_SHM_PAGE_LIST)),
               OPTEE_MSG_PAGE_SIZE
               );

  if (PageList == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Allocate Page List\n",
      __FUNCTION__
      ));
    goto ExitAllocatePageAndMessageBuffers;
  }

  /* Allocate the message Buffer to communicate with Optee */
  FtpmOpteeMessage = AllocateAlignedPages (
                       EFI_SIZE_TO_PAGES (sizeof (OPTEE_MESSAGE_ARG)),
                       OPTEE_MSG_PAGE_SIZE
                       );

  if (FtpmOpteeMessage == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Allocate Message Arg\n",
      __FUNCTION__
      ));
    goto ExitAllocatePageAndMessageBuffers;
  }

ExitAllocatePageAndMessageBuffers:
  return Status;
}

/**
  This service requests use TPM2. Use this function to setup the
  Optee session and shared buffers


  @retval EFI_SUCCESS      Get the control of TPM2 chip.
  @retval Other            Failed to initialize communication to fTPM.
**/
EFI_STATUS
EFIAPI
Tpm2RequestUseTpm (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       OpteeFtpmShmSize;
  EFI_HANDLE  Handle;

  OpteeFtpmShmSize      = PcdGet64 (PcdFtpmShmSize);
  OpteeFtpmShmInputSize = OpteeFtpmShmSize / 2;

  Status = AllocatePageAndMessageBuffers ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Allocate buffersfor Msg/Shm List\n",
      __FUNCTION__
      ));
    goto ExitTpm2RequestUseTpm;
  }

  /* Setup the Message Buffer pointers.*/
  OpteeSetProperties (
    (UINT64)FtpmOpteeMessage,
    (UINT64)FtpmOpteeMessage,
    sizeof (OPTEE_MESSAGE_ARG)
    );

  /* Register the input and output buffer */
  Status = AllocateRegisterOpteeBuf (
             OpteeFtpmShmSize,
             &OpteeFtpmShmIpBuf,
             &OpteeFtpmShmInputCookie
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Register Input Buf %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitTpm2RequestUseTpm;
  }

  Status = AllocateRegisterOpteeBuf (
             OpteeFtpmShmSize,
             &OpteeFtpmShmOpBuf,
             &OpteeFtpmShmOutputCookie
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Register Output Buf %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitTpm2RequestUseTpm;
  }

  /* Open a session to the Optee FtpmTA */
  Status = OpenSessionToFtpm ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to open session to fTPM %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitTpm2RequestUseTpm;
  }

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gNVIDIAFtpmPresentProtocolGuid,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to install fTPM present Protocol %r \n",
      __FUNCTION__,
      Status
      ));
    goto ExitTpm2RequestUseTpm;
  }

  return Status;

ExitTpm2RequestUseTpm:
  if (EFI_ERROR (Status)) {
    FreeBuffers ();
  }

  return Status;
}

/**
  OpteeTpmDeviceLiDestructor

  Destructor for the OPTEE TPM library.

  @param[in] ImageHandle Handle of the driver
  @param[in] Systemtable Pointer to the EFI system table.

  @retval EFI_SUCCESS on Success.
 **/
EFI_STATUS
EFIAPI
OpteeTpmDeviceLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  if (ExitBootServicesEvent != 0) {
    gBS->CloseEvent (ExitBootServicesEvent);
  }

  return EFI_SUCCESS;
}

/**
  OpteeTpmDeviceLibConstructor

  Constructor for the OPTEE TPM library.

  @param[in] ImageHandle Handle of the driver
  @param[in] Systemtable Pointer to the EFI system table.

  @retval EFI_SUCCESS on Success.
          Other       on Failure.
 **/
EFI_STATUS
EFIAPI
OpteeTpmDeviceLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->CreateEvent (
                  EVT_SIGNAL_EXIT_BOOT_SERVICES,
                  TPL_NOTIFY,
                  ExitBootServicesCallBack,
                  NULL,
                  &ExitBootServicesEvent
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to Create ExitBootServices Callback %r\n",
      __FUNCTION__,
      Status
      ));
    goto ExitOpteeTpmDeviceLibConstructor;
  }

ExitOpteeTpmDeviceLibConstructor:
  return Status;
}
