/** @file

  PLDM FW update task lib

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MctpBaseLib.h>
#include <Library/PldmBaseLib.h>
#include <Library/PldmFwUpdateLib.h>
#include <Library/PldmFwUpdatePkgLib.h>
#include <Library/PldmFwUpdateTaskLib.h>
#include <Library/TimerLib.h>
#include <Protocol/MctpProtocol.h>

#define PLDM_FW_TASK_REQUEST_SIZE      128
#define PLDM_FW_TASK_RESPONSE_SIZE     (4 * 1024)
#define PLDM_FW_TASK_RECV_BUFFER_SIZE  1024
#define PLDM_FW_TASK_FW_PARAMS_SIZE    512
#define PLDM_FW_TASK_MS_TO_NS(ms)  ((ms) * 1000 * 1000)

#define PLDM_FW_TASK_MAX_OUTSTANDING_TRANSFER_REQUESTS  1
#define PLDM_FW_TASK_MAX_TRANSFER_SIZE                  PLDM_FW_TASK_RESPONSE_SIZE

typedef enum {
  // IDLE state
  StateStart,
  StateQueryDeviceIdentifiersSetupReq,
  StateQueryDeviceIdentifiersProcessRsp,
  StateGetFwParamsSetupReq,
  StateGetFwParamsProcessRsp,
  StateProcessPackage,
  StateRequestUpdateSetupReq,
  StateRequestUpdateProcessRsp,

  // LEARN COMPONENTS state
  StatePassComponentTableSetupReq,
  StatePassComponentTableProcessRsp,
  StatePassComponentTableNextComponent,

  /// READY XFER state
  StateUpdateComponentSetupReq,
  StateUpdateComponentProcessRsp,

  // DOWNLOAD/VERIFY/APPLY states, driven by FD requests
  StateWaitForRequests,
  StateRequestFwDataHandleReq,
  StateTransferCompleteHandleReq,
  StateVerifyCompleteHandleReq,
  StateApplyCompleteHandleReq,

  // READY XFER state
  StateNextComponent,
  StateActivateFwSetupReq,
  StateActivateFwProcessRsp,

  // IDLE state, update complete
  StateComplete,

  // Common request/response states
  StateReceive,
  StateSendReq,
  StateProcessRsp,
  StateRetryReq,

  // error states
  StateFatalError,
  StateCancelUpdateComponentSetupReq,
  StateCancelUpdateComponentProcessRsp,
  StateCancelUpdateSetupReq,
  StateCancelUpdateProcessRsp,

  StateMax
} PLDM_FW_TASK_STATE;

typedef enum {
  FDStateIdle,
  FDStateLearnComponents,
  FDStateReadyXfer,
  FDStateDownload,
  FDStateVerify,
  FDStateApply,
  FDStateActivate,
  FDStateMax
} PLDM_FW_TASK_FD_STATE;

typedef struct {
  BOOLEAN    Enabled;
  UINT64     EndNs;
} PLDM_FW_TASK_TIMER;

typedef struct {
  // task control and status
  NVIDIA_MCTP_PROTOCOL                           *FD;
  CONST CHAR16                                   *DeviceName;
  UINT64                                         StartNs;

  PLDM_FW_TASK_STATE                             TaskState;
  EFI_STATUS                                     Status;
  BOOLEAN                                        Complete;
  BOOLEAN                                        IsExpectingFDRequests;
  UINTN                                          RetryCount;
  PLDM_FW_TASK_FD_STATE                          FDState;

  // PLDM request/response message tracking
  UINTN                                          RspExtraMs;
  PLDM_FW_TASK_TIMER                             RspTimer;
  PLDM_FW_TASK_TIMER                             RequestFwDataTimer;

  UINT8                                          InstanceId;
  UINT8                                          RecvBuffer[PLDM_FW_TASK_RECV_BUFFER_SIZE];
  UINTN                                          RecvLength;
  UINT8                                          RecvMsgTag;

  UINT8                                          Request[PLDM_FW_TASK_REQUEST_SIZE];
  UINTN                                          RequestLength;
  UINT8                                          RequestMsgTag;
  BOOLEAN                                        RequestIsActive;

  UINT8                                          Response[PLDM_FW_TASK_RESPONSE_SIZE];
  UINTN                                          ResponseLength;
  PLDM_FW_TASK_STATE                             ProcessResponseState;

  // package meta-data
  CONST PLDM_FW_PKG_HDR                          *PkgHdr;
  UINTN                                          PkgLen;
  CONST PLDM_FW_PKG_DEVICE_ID_RECORD             *DeviceIdRecord;
  CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO_AREA    *ImageInfoArea;
  CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO         *ImageInfo;

  // component tracking
  UINTN                                          ComponentImageIndex;
  UINTN                                          NumUpdateComponents;
  UINTN                                          UpdateComponentIndex;
  UINTN                                          LastFwDataRequested;

  // FD info
  UINT8                                          GetFwParamsResponseBuffer[PLDM_FW_TASK_FW_PARAMS_SIZE];
  CONST PLDM_FW_GET_FW_PARAMS_RESPONSE           *GetFwParamsResponse;
  UINTN                                          FwParamsComponentIndex;

  // info from request update response
  UINT16                                         FirmwareDeviceMetaDataLength;
  BOOLEAN                                        FDWillSendGetPackageDataCommand;
} PLDM_FW_UPDATE_TASK;

typedef
PLDM_FW_TASK_STATE
(EFIAPI *PLDM_FW_TASK_STATE_FUNC)(
  IN PLDM_FW_UPDATE_TASK *Task
  );

typedef struct {
  PLDM_FW_TASK_STATE         State;
  PLDM_FW_TASK_STATE_FUNC    Func;
} PLDM_FW_TASK_STATE_TABLE;

UINTN                         mNumDevices       = 0;
UINTN                         mNumTasks         = 0;
UINTN                         mNumTasksComplete = 0;
EFI_STATUS                    mStatus           = EFI_SUCCESS;
UINT16                        mActivationMethod = 0;
PLDM_FW_UPDATE_TASK           *mTasks           = NULL;
PLDM_FW_UPDATE_TASK_ERROR     mError            = PLDM_FW_UPDATE_TASK_ERROR_NONE;
PLDM_FW_UPDATE_TASK_PROGRESS  mProgressFunction = NULL;
UINTN                         mCompletion       = 0;

/**
  Call optional client progress function with percent complete.

  @param[in]  Completion                   Completion percentage (0-100).

  @retval None

**/
STATIC
VOID
EFIAPI
PldmFwTaskProgress (
  IN UINTN  Completion
  )
{
  if ((mProgressFunction != NULL) && (Completion > mCompletion)) {
    mProgressFunction (Completion);
    mCompletion = Completion;
  }
}

/**
  Compute data transfer progress across all tasks.

  @retval None

**/
STATIC
VOID
EFIAPI
PldmFwTaskDataProgressCompute (
  VOID
  )
{
  UINTN                      Index;
  CONST PLDM_FW_UPDATE_TASK  *Task;
  UINTN                      TotalCompleted;
  UINTN                      TotalLength;

  TotalCompleted = 0;
  TotalLength    = 0;
  for (Index = 0; Index < mNumTasks; Index++) {
    Task            = &mTasks[Index];
    TotalCompleted += Task->LastFwDataRequested;
    TotalLength    += Task->PkgLen;
  }

  ASSERT (TotalLength != 0);

  // data transfer accounts for 99% of progress
  PldmFwTaskProgress ((TotalCompleted * 99) / TotalLength);
}

/**
  Set global error value, if not previously set.

  @param[in]  Error                         Error code.

  @retval None

**/
STATIC
VOID
EFIAPI
PldmFwTaskSetError (
  IN PLDM_FW_UPDATE_TASK_ERROR  Error
  )
{
  if (mError == PLDM_FW_UPDATE_TASK_ERROR_NONE) {
    mError = Error;
  }
}

/**
  Start task timer.

  @param[in]  Timer                     Timer to start.
  @param[in]  TimeoutMs                 Milliseconds duration of timer.

  @retval None

**/
STATIC
VOID
EFIAPI
PldmFwTaskStartTimer (
  IN  PLDM_FW_TASK_TIMER  *Timer,
  IN  UINTN               TimeoutMs
  )
{
  Timer->Enabled = TRUE;
  Timer->EndNs   = GetTimeInNanoSecond (GetPerformanceCounter ()) + PLDM_FW_TASK_MS_TO_NS (TimeoutMs);
}

/**
  Cancel task timer.

  @param[in]  Timer                     Timer to cancel.

  @retval None

**/
STATIC
VOID
EFIAPI
PldmFwTaskCancelTimer (
  IN  PLDM_FW_TASK_TIMER  *Timer
  )
{
  Timer->Enabled = FALSE;
}

/**
  Check if task timer is expired.

  @param[in]  Timer                     Timer to check.

  @retval BOOLEAN                       TRUE if timer expired.

**/
STATIC
BOOLEAN
EFIAPI
PldmFwTaskTimerIsExpired (
  IN  PLDM_FW_TASK_TIMER  *Timer
  )
{
  return (Timer->Enabled) ?
         (Timer->EndNs <= GetTimeInNanoSecond (GetPerformanceCounter ())) :
         FALSE;
}

/**
  Set Firmware Device state.

  @param[in]  Task                     Pointer to task.
  @param[in]  State                    State value.

  @retval None

**/
STATIC
VOID
EFIAPI
PldmFwTaskSetFDState (
  IN  PLDM_FW_UPDATE_TASK   *Task,
  IN PLDM_FW_TASK_FD_STATE  State
  )
{
  ASSERT (State < FDStateMax);

  Task->FDState = State;
}

/**
  Reset task component information.

  @param[in]  Task                     Pointer to task.

  @retval None

**/
STATIC
VOID
EFIAPI
PldmFwTaskResetComponentInfo (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  Task->ImageInfoArea       = PldmFwPkgGetComponentImageInfoArea (Task->PkgHdr);
  Task->ImageInfo           = Task->ImageInfoArea->ImageInfo;
  Task->ComponentImageIndex = 0;

  Task->FwParamsComponentIndex = 0;
  Task->UpdateComponentIndex   = 0;
  Task->LastFwDataRequested    = 0;
}

/**
  Setup task to send a request.

  @param[in]  Task                     Pointer to task.
  @param[in]  Command                  Request command code.
  @param[in]  RequestLength            Request length in bytes.
  @param[in]  ProcessResponseState     Task state that processes response.

  @retval None

**/
STATIC
VOID
EFIAPI
PldmFwTaskSendReqSetup (
  IN  PLDM_FW_UPDATE_TASK  *Task,
  IN  UINT8                Command,
  IN  UINTN                RequestLength,
  IN  PLDM_FW_TASK_STATE   ProcessResponseState
  )
{
  MCTP_PLDM_REQUEST_HEADER  *Request;

  ASSERT (RequestLength <= PLDM_FW_TASK_REQUEST_SIZE);

  Request          = (MCTP_PLDM_REQUEST_HEADER *)Task->Request;
  Task->RetryCount = PLDM_PN1_RETRIES;
  Task->InstanceId++;
  Task->RequestLength        = RequestLength;
  Task->ProcessResponseState = ProcessResponseState;
  Task->RspExtraMs           = 0;

  PldmFwFillCommon (
    &Request->Common,
    TRUE,
    Task->InstanceId,
    Command
    );
}

/**
  Task state handler to start task.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskStart (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  return StateQueryDeviceIdentifiersSetupReq;
}

/**
  Task state handler to setup query device identifiers request.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskQueryDeviceIdentifiersSetupReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_QUERY_DEVICE_IDS_REQUEST  *Request;

  Request = (PLDM_FW_QUERY_DEVICE_IDS_REQUEST *)Task->Request;
  PldmFwTaskSendReqSetup (
    Task,
    PLDM_FW_QUERY_DEVICE_IDS,
    sizeof (*Request),
    StateQueryDeviceIdentifiersProcessRsp
    );

  return StateSendReq;
}

/**
  Task state handler to process query device identifiers response.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskQueryDeviceIdentifiersProcessRsp (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_QUERY_DEVICE_IDS_RESPONSE  *Response;
  EFI_STATUS                         Status;

  Response = (PLDM_FW_QUERY_DEVICE_IDS_RESPONSE *)Task->RecvBuffer;
  Status   = PldmFwQueryDeviceIdsCheckRsp (Response, Task->RecvLength, Task->DeviceName);
  if (EFI_ERROR (Status)) {
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_QUERY_DEVICE_IDS_FAILED);
    return StateFatalError;
  }

  if (!PldmFwPkgMatchesFD (
         Task->PkgHdr,
         Response->Count,
         Response->Descriptors,
         &Task->DeviceIdRecord
         ))
  {
    DEBUG ((DEBUG_ERROR, "%a: no FD match in package\n", __FUNCTION__));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_NO_FD_MATCH_IN_PKG);
    return StateFatalError;
  }

  DEBUG ((DEBUG_INFO, "%a: complete %u descriptors\n", __FUNCTION__, Task->DeviceIdRecord->DescriptorCount));

  return StateGetFwParamsSetupReq;
}

/**
  Task state handler to setup Get FW Parameters request.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskGetFwParamsSetupReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_GET_FW_PARAMS_REQUEST  *Request;

  Request = (PLDM_FW_GET_FW_PARAMS_REQUEST *)Task->Request;
  PldmFwTaskSendReqSetup (
    Task,
    PLDM_FW_GET_FW_PARAMS,
    sizeof (*Request),
    StateGetFwParamsProcessRsp
    );

  return StateSendReq;
}

/**
  Task state handler to process Get FW Parameters response

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskGetFwParamsProcessRsp (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_GET_FW_PARAMS_RESPONSE  *Response;
  EFI_STATUS                      Status;

  Response = (PLDM_FW_GET_FW_PARAMS_RESPONSE *)Task->RecvBuffer;

  Status = PldmFwGetFwParamsCheckRsp (Response, Task->RecvLength, Task->DeviceName);
  if (EFI_ERROR (Status)) {
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_GET_FW_PARAMS_FAILED);
    return StateFatalError;
  }

  if (Task->RecvLength > sizeof (Task->GetFwParamsResponseBuffer)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: FwParams size=%u too small\n",
      __FUNCTION__,
      Task->RecvLength
      ));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_GET_FW_PARAMS_BUFFER_TOO_SMALL);
    return StateFatalError;
  }

  CopyMem (Task->GetFwParamsResponseBuffer, Response, Task->RecvLength);
  Task->GetFwParamsResponse = (CONST PLDM_FW_GET_FW_PARAMS_RESPONSE *)
                              Task->GetFwParamsResponseBuffer;

  return StateProcessPackage;
}

/**
  Task state handler to process the PLDM update package.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskProcessPackage (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  CONST PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY  *FwParamsComponent;
  CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO         *ImageInfo;
  UINTN                                          Index;
  UINTN                                          Index1;
  UINTN                                          ImageCount;

  PldmFwTaskResetComponentInfo (Task);

  ImageCount                = Task->ImageInfoArea->ImageCount;
  ImageInfo                 = Task->ImageInfoArea->ImageInfo;
  Task->NumUpdateComponents = 0;
  for (Index = 0; Index < ImageCount; Index++) {
    if (PldmFwPkgComponentIsApplicable (
          Index,
          Task->PkgHdr,
          Task->DeviceIdRecord
          ))
    {
      DEBUG ((DEBUG_INFO, "%a: Component index %u applicable\n", __FUNCTION__, Index));

      for (Index1 = 0; Index1 < Task->GetFwParamsResponse->ComponentCount; Index1++) {
        FwParamsComponent = PldmFwGetFwParamsComponent (
                              Task->GetFwParamsResponse,
                              Index1
                              );
        DEBUG ((DEBUG_INFO, "%a: FwP Id=0x%x CII Id=0x%x\n", __FUNCTION__, FwParamsComponent->Id, ImageInfo->Id));

        if ((FwParamsComponent->Classification == ImageInfo->Classification) &&
            (FwParamsComponent->Id == ImageInfo->Id))
        {
          Task->NumUpdateComponents++;
        }
      }
    }

    ImageInfo = PldmFwPkgGetNextComponentImage (ImageInfo);
  }

  DEBUG ((DEBUG_INFO, "%a: NumUpdateComponents=%u\n", __FUNCTION__, Task->NumUpdateComponents));

  if (Task->NumUpdateComponents == 0) {
    DEBUG ((DEBUG_ERROR, "%a: No Update Components\n", __FUNCTION__));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_NO_UPDATE_COMPONENTS);
    return StateFatalError;
  }

  return StateRequestUpdateSetupReq;
}

/**
  Task state handler to setup the Request Update request.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskRequestUpdateSetupReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  CONST PLDM_FW_PKG_DEVICE_ID_RECORD  *DeviceIdRecord;
  PLDM_FW_REQUEST_UPDATE_REQUEST      *Request;
  UINTN                               RequestLength;

  DeviceIdRecord = Task->DeviceIdRecord;
  Request        = (PLDM_FW_REQUEST_UPDATE_REQUEST *)Task->Request;

  Request->MaxTransferSize                      = PLDM_FW_TASK_MAX_TRANSFER_SIZE;
  Request->NumComponents                        = Task->NumUpdateComponents;
  Request->MaxOutstandingTransferReqs           = PLDM_FW_TASK_MAX_OUTSTANDING_TRANSFER_REQUESTS;
  Request->PackageDataLength                    = DeviceIdRecord->PackageDataLength;
  Request->ComponentImageSetVersionStringType   = DeviceIdRecord->ImageSetVersionStringType;
  Request->ComponentImageSetVersionStringLength = DeviceIdRecord->ImageSetVersionStringLength;
  CopyMem (
    Request->ComponentImageSetVersionString,
    PldmFwPkgGetDeviceIdRecordImageSetVersionString (Task->PkgHdr, DeviceIdRecord),
    DeviceIdRecord->ImageSetVersionStringLength
    );

  RequestLength = OFFSET_OF (
                    PLDM_FW_REQUEST_UPDATE_REQUEST,
                    ComponentImageSetVersionString
                    ) + DeviceIdRecord->ImageSetVersionStringLength;

  PldmFwTaskSendReqSetup (
    Task,
    PLDM_FW_REQUEST_UPDATE,
    RequestLength,
    StateRequestUpdateProcessRsp
    );

  return StateSendReq;
}

/**
  Task state handler to process the Request Update response.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskRequestUpdateProcessRsp (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_REQUEST_UPDATE_RESPONSE  *Response;
  EFI_STATUS                       Status;

  Response = (PLDM_FW_REQUEST_UPDATE_RESPONSE *)Task->RecvBuffer;

  Status = PldmFwCheckRspCompletionAndLength (Response, Task->RecvLength, sizeof (*Response), __FUNCTION__, Task->DeviceName);
  if (EFI_ERROR (Status)) {
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_REQUEST_UPDATE_FAILED);
    return StateFatalError;
  }

  if ((Response->FirmwareDeviceMetaDataLength > 0) ||
      (Response->FDWillSendGetPackageDataCommand != 0))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s reports FD Metadata size %u, WillSend %u, not supported\n",
      __FUNCTION__,
      Task->DeviceName,
      Response->FirmwareDeviceMetaDataLength,
      Response->FDWillSendGetPackageDataCommand
      ));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_REQUEST_UPDATE_UNSUPPORTED);
    return StateFatalError;
  }

  PldmFwTaskSetFDState (Task, FDStateLearnComponents);
  DEBUG ((DEBUG_INFO, "%a: complete\n", __FUNCTION__));

  return StatePassComponentTableSetupReq;
}

/**
  Task state handler to setup the Pass Component Table request.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskPassComponentTableSetupReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_PASS_COMPONENT_TABLE_REQUEST    *Request;
  CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO  *ImageInfo;
  UINT8                                   TransferFlag;
  UINTN                                   RequestLength;

  ImageInfo = Task->ImageInfo;

  if (!PldmFwPkgComponentIsApplicable (
         Task->ComponentImageIndex,
         Task->PkgHdr,
         Task->DeviceIdRecord
         ))
  {
    DEBUG ((DEBUG_INFO, "%a: component %u not applicable\n", __FUNCTION__, Task->ComponentImageIndex));
    return StatePassComponentTableNextComponent;
  }

  if (PldmFwGetNextFwParamsMatchingComponent (
        Task->GetFwParamsResponse,
        &Task->FwParamsComponentIndex,
        ImageInfo->Classification,
        ImageInfo->Id
        ) == NULL)
  {
    DEBUG ((DEBUG_ERROR, "%a: No FD match for component %u\n", __FUNCTION__, Task->ComponentImageIndex));

    return StatePassComponentTableNextComponent;
  }

  TransferFlag  = 0;
  TransferFlag |= (Task->UpdateComponentIndex == 0) ?
                  PLDM_FW_TRANSFER_FLAG_START : 0;
  TransferFlag |= (Task->UpdateComponentIndex == Task->NumUpdateComponents - 1) ?
                  PLDM_FW_TRANSFER_FLAG_END : 0;
  if (TransferFlag == 0) {
    TransferFlag = PLDM_FW_TRANSFER_FLAG_MIDDLE;
  }

  Request = (PLDM_FW_PASS_COMPONENT_TABLE_REQUEST *)Task->Request;

  Request->TransferFlag                 = TransferFlag;
  Request->ComponentClassification      = ImageInfo->Classification;
  Request->ComponentId                  = ImageInfo->Id;
  Request->ComponentClassificationIndex = PldmFwGetFwParamsComponent (
                                            Task->GetFwParamsResponse,
                                            Task->FwParamsComponentIndex
                                            )->ClassificationIndex;
  Request->ComponentComparisonStamp     = ImageInfo->ComparisonStamp;
  Request->ComponentVersionStringType   = ImageInfo->VersionStringType;
  Request->ComponentVersionStringLength = ImageInfo->VersionStringLength;
  CopyMem (
    Request->ComponentVersionString,
    ImageInfo->VersionString,
    ImageInfo->VersionStringLength
    );

  RequestLength = OFFSET_OF (
                    PLDM_FW_PASS_COMPONENT_TABLE_REQUEST,
                    ComponentVersionString
                    ) + ImageInfo->VersionStringLength;

  PldmFwTaskSendReqSetup (
    Task,
    PLDM_FW_PASS_COMPONENT_TABLE,
    RequestLength,
    StatePassComponentTableProcessRsp
    );

  return StateSendReq;
}

/**
  Task state handler to process the Pass Component Table response.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskPassComponentTableProcessRsp (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_PASS_COMPONENT_TABLE_REQUEST   *Request;
  PLDM_FW_PASS_COMPONENT_TABLE_RESPONSE  *Response;
  EFI_STATUS                             Status;

  Request  = (PLDM_FW_PASS_COMPONENT_TABLE_REQUEST *)Task->Request;
  Response = (PLDM_FW_PASS_COMPONENT_TABLE_RESPONSE *)Task->RecvBuffer;

  Status = PldmFwCheckRspCompletionAndLength (Response, Task->RecvLength, sizeof (*Response), __FUNCTION__, Task->DeviceName);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s ComponentId 0x%x failed:%r\n", __FUNCTION__, Task->DeviceName, Request->ComponentId, Status));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_PASS_COMPONENT_TABLE_FAILED);
    return StateFatalError;
  }

  if (Response->ComponentResponse != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s ComponentId 0x%x failed ComponentResponseCode=0x%x\n",
      __FUNCTION__,
      Task->DeviceName,
      Request->ComponentId,
      Response->ComponentResponseCode
      ));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_PASS_COMPONENT_TABLE_BAD_RSP);
    return StateFatalError;
  }

  DEBUG ((DEBUG_INFO, "%a: ComponentId 0x%x complete\n", __FUNCTION__, Request->ComponentId));

  return StatePassComponentTableNextComponent;
}

/**
  Task state handler to update task for next component in
  the Pass Component Table sequence.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskPassComponentTableNextComponent (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  Task->FwParamsComponentIndex++;
  if (PldmFwGetNextFwParamsMatchingComponent (
        Task->GetFwParamsResponse,
        &Task->FwParamsComponentIndex,
        Task->ImageInfo->Classification,
        Task->ImageInfo->Id
        ) != NULL)
  {
    DEBUG ((DEBUG_INFO, "%a: additional FD match for component %u, \n", __FUNCTION__, Task->ComponentImageIndex));

    return StatePassComponentTableSetupReq;
  }

  if (++Task->ComponentImageIndex >= Task->ImageInfoArea->ImageCount) {
    PldmFwTaskResetComponentInfo (Task);
    PldmFwTaskSetFDState (Task, FDStateReadyXfer);

    return StateUpdateComponentSetupReq;
  }

  Task->ImageInfo = PldmFwPkgGetNextComponentImage (Task->ImageInfo);
  return StatePassComponentTableSetupReq;
}

/**
  Task state handler to setup the Update Component request.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskUpdateComponentSetupReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO  *ImageInfo;
  PLDM_FW_UPDATE_COMPONENT_REQUEST        *Request;
  UINTN                                   RequestLength;

  ImageInfo = Task->ImageInfo;

  if (!PldmFwPkgComponentIsApplicable (
         Task->ComponentImageIndex,
         Task->PkgHdr,
         Task->DeviceIdRecord
         ))
  {
    return StateNextComponent;
  }

  if (PldmFwGetNextFwParamsMatchingComponent (
        Task->GetFwParamsResponse,
        &Task->FwParamsComponentIndex,
        ImageInfo->Classification,
        ImageInfo->Id
        ) == NULL)
  {
    DEBUG ((DEBUG_ERROR, "%a: No FD match for component %u\n", __FUNCTION__, Task->ComponentImageIndex));

    return StateNextComponent;
  }

  Request = (PLDM_FW_UPDATE_COMPONENT_REQUEST *)Task->Request;

  Request->ComponentClassification      = ImageInfo->Classification;
  Request->ComponentId                  = ImageInfo->Id;
  Request->ComponentClassificationIndex =
    PldmFwGetFwParamsComponent (Task->GetFwParamsResponse, Task->FwParamsComponentIndex)->ClassificationIndex;
  Request->ComponentComparisonStamp = ImageInfo->ComparisonStamp;
  Request->ComponentImageSize       = ImageInfo->Size;
  Request->UpdateOptionFlags        =
    (ImageInfo->Options & PLDM_FW_PKG_COMPONENT_OPT_FORCE_UPDATE) ?
    PLDM_FW_UPDATE_COMPONENT_REQUEST_FORCE_UPDATE : 0;
  Request->ComponentVersionStringType   = ImageInfo->VersionStringType;
  Request->ComponentVersionStringLength = ImageInfo->VersionStringLength;
  CopyMem (
    Request->ComponentVersionString,
    ImageInfo->VersionString,
    ImageInfo->VersionStringLength
    );

  RequestLength = OFFSET_OF (PLDM_FW_UPDATE_COMPONENT_REQUEST, ComponentVersionString) + ImageInfo->VersionStringLength;

  PldmFwTaskSendReqSetup (
    Task,
    PLDM_FW_UPDATE_COMPONENT,
    RequestLength,
    StateUpdateComponentProcessRsp
    );

  return StateSendReq;
}

/**
  Task state handler to process the Update Component response.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskUpdateComponentProcessRsp (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_UPDATE_COMPONENT_RESPONSE  *Response;
  EFI_STATUS                         Status;

  Response = (PLDM_FW_UPDATE_COMPONENT_RESPONSE *)Task->RecvBuffer;

  Status = PldmFwCheckRspCompletionAndLength (Response, Task->RecvLength, sizeof (*Response), __FUNCTION__, Task->DeviceName);
  if (EFI_ERROR (Status)) {
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_UPDATE_COMPONENT_FAILED);
    return StateFatalError;
  }

  if (Response->ComponentCompatibilityResponse != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s will not update, code=0x%x\n",
      __FUNCTION__,
      Task->DeviceName,
      Response->ComponentCompatibilityResponseCode
      ));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_UPDATE_COMPONENT_WILL_NOT_UPDATE);
    return StateFatalError;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: %s will update, options=0x%x, time=%u\n",
    __FUNCTION__,
    Task->DeviceName,
    Response->UpdateOptionFlagsEnabled,
    Response->TimeBeforeRequestFwData
    ));

  PldmFwTaskSetFDState (Task, FDStateDownload);
  PldmFwTaskStartTimer (
    &Task->RequestFwDataTimer,
    (Response->TimeBeforeRequestFwData > 0) ?
    Response->TimeBeforeRequestFwData + PLDM_FW_UA_T2_MS_MAX :
    PLDM_FW_UA_T2_MS_MAX
    );

  return StateWaitForRequests;
}

/**
  Task state to setup for waiting for requests from FD.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskWaitForRequests (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  Task->IsExpectingFDRequests = TRUE;

  return StateReceive;
}

/**
  Task state handler for the Request FW Data request from FD.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskRequestFwDataHandleReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_REQUEST_FW_DATA_REQUEST         *Request;
  PLDM_FW_REQUEST_FW_DATA_RESPONSE        *Response;
  CONST PLDM_FW_PKG_COMPONENT_IMAGE_INFO  *ImageInfo;
  UINTN                                   Offset;
  UINT32                                  Length;
  UINT8                                   CompletionCode;
  EFI_STATUS                              Status;

  Request = (PLDM_FW_REQUEST_FW_DATA_REQUEST *)Task->RecvBuffer;

  DEBUG ((DEBUG_VERBOSE, "%a: off=0x%x len=0x%x\n", __FUNCTION__, Request->Offset, Request->Length));
  if (Request->Offset + Request->Length <= Task->LastFwDataRequested) {
    DEBUG ((DEBUG_WARN, "%a: WARNING offset=0x%x length=0x%x retried last=0x%x\n", __FUNCTION__, Request->Offset, Request->Length, Task->LastFwDataRequested));
  }

  Task->LastFwDataRequested = Request->Offset + Request->Length;
  PldmFwTaskDataProgressCompute ();

  if (Task->FDState != FDStateDownload) {
    DEBUG ((DEBUG_ERROR, "%a: %s req in FD state=%d\n", __FUNCTION__, Task->DeviceName, Task->FDState));
    CompletionCode = PLDM_FW_COMMAND_NOT_EXPECTED;
    goto SendResponse;
  }

  if (Task->RecvLength < sizeof (*Request)) {
    DEBUG ((DEBUG_ERROR, "%a: %s invalid request size %u\n", __FUNCTION__, Task->DeviceName, Task->RecvLength));
    CompletionCode = PLDM_ERROR_INVALID_LENGTH;
    goto SendResponse;
  }

  Offset = Request->Offset;
  Length = Request->Length;

  ImageInfo = Task->ImageInfo;
  if (Length > PLDM_FW_TASK_RESPONSE_SIZE) {
    CompletionCode = PLDM_FW_INVALID_TRANSFER_LENGTH;
  } else if (Length + Offset > ImageInfo->Size + PLDM_FW_BASELINE_TRANSFER_SIZE) {
    CompletionCode = PLDM_FW_DATA_OUT_OF_RANGE;
  } else {
    CompletionCode = PLDM_SUCCESS;
  }

  Offset += ImageInfo->LocationOffset;

SendResponse:
  Response                 = (PLDM_FW_REQUEST_FW_DATA_RESPONSE *)Task->Response;
  Response->Common         = Request->Common;
  Response->CompletionCode = CompletionCode;
  Task->ResponseLength     = OFFSET_OF (PLDM_FW_REQUEST_FW_DATA_RESPONSE, ImageData);
  if (CompletionCode == PLDM_SUCCESS) {
    CopyMem (Response->ImageData, (CONST UINT8 *)Task->PkgHdr + Offset, Length);
    Task->ResponseLength += Length;
  }

  Status = Task->FD->Send (
                       Task->FD,
                       FALSE,
                       Task->Response,
                       Task->ResponseLength,
                       &Task->RecvMsgTag
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s response failed: %r\n", __FUNCTION__, Task->DeviceName, Status));
  }

  PldmFwTaskStartTimer (&Task->RequestFwDataTimer, PLDM_FW_UA_T2_MS_MAX);

  return StateWaitForRequests;
}

/**
  Task state handler for the Transfer Complete request from FD.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskTransferCompleteHandleReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_TRANSFER_COMPLETE_REQUEST   *Request;
  PLDM_FW_TRANSFER_COMPLETE_RESPONSE  *Response;
  EFI_STATUS                          Status;

  Response = (PLDM_FW_TRANSFER_COMPLETE_RESPONSE *)Task->Response;
  Request  = (PLDM_FW_TRANSFER_COMPLETE_REQUEST *)Task->RecvBuffer;
  if (Task->RecvLength < sizeof (*Request)) {
    DEBUG ((DEBUG_ERROR, "%a: %s invalid request size %u\n", __FUNCTION__, Task->DeviceName, Task->RecvLength));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_TRANSFER_COMPLETE_BAD_LEN);
    return StateFatalError;
  }

  DEBUG ((DEBUG_INFO, "%a: %s transfer result: 0x%x\n", __FUNCTION__, Task->DeviceName, Request->TransferResult));

  Response->Common         = Request->Common;
  Response->CompletionCode = PLDM_SUCCESS;
  Task->ResponseLength     = sizeof (*Response);

  PldmFwTaskCancelTimer (&Task->RequestFwDataTimer);
  Status = Task->FD->Send (
                       Task->FD,
                       FALSE,
                       Task->Response,
                       Task->ResponseLength,
                       &Task->RecvMsgTag
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s response failed: %r\n", __FUNCTION__, Task->DeviceName, Status));
  }

  if (Request->TransferResult != 0) {
    DEBUG ((DEBUG_ERROR, "%a: %s transfer failure: 0x%x\n", __FUNCTION__, Task->DeviceName, Request->TransferResult));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_TRANSFER_COMPLETE_RESULT_ERR);
    return StateFatalError;
  }

  Task->LastFwDataRequested = Task->PkgLen;
  PldmFwTaskDataProgressCompute ();
  PldmFwTaskSetFDState (Task, FDStateVerify);

  return StateWaitForRequests;
}

/**
  Task state handler for the Verify Complete request from FD.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskVerifyCompleteHandleReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_VERIFY_COMPLETE_REQUEST   *Request;
  PLDM_FW_VERIFY_COMPLETE_RESPONSE  *Response;
  EFI_STATUS                        Status;

  Response = (PLDM_FW_VERIFY_COMPLETE_RESPONSE *)Task->Response;
  Request  = (PLDM_FW_VERIFY_COMPLETE_REQUEST *)Task->RecvBuffer;
  if (Task->RecvLength < sizeof (*Request)) {
    DEBUG ((DEBUG_ERROR, "%a: %s invalid request size %u\n", __FUNCTION__, Task->DeviceName, Task->RecvLength));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_VERIFY_COMPLETE_BAD_LEN);
    return StateFatalError;
  }

  DEBUG ((DEBUG_INFO, "%a: %s verify result: 0x%x\n", __FUNCTION__, Task->DeviceName, Request->VerifyResult));

  Response->Common         = Request->Common;
  Response->CompletionCode = PLDM_SUCCESS;
  Task->ResponseLength     = sizeof (*Response);

  Status = Task->FD->Send (
                       Task->FD,
                       FALSE,
                       Task->Response,
                       Task->ResponseLength,
                       &Task->RecvMsgTag
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s response failed: %r\n", __FUNCTION__, Task->DeviceName, Status));
  }

  if (Request->VerifyResult != 0) {
    DEBUG ((DEBUG_ERROR, "%a: %s verify failure: 0x%x\n", __FUNCTION__, Task->DeviceName, Request->VerifyResult));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_VERIFY_COMPLETE_FAILED);
    return StateFatalError;
  }

  PldmFwTaskSetFDState (Task, FDStateApply);

  return StateWaitForRequests;
}

/**
  Task state handler for the Apply Complete request from FD.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskApplyCompleteHandleReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_APPLY_COMPLETE_REQUEST   *Request;
  PLDM_FW_APPLY_COMPLETE_RESPONSE  *Response;
  EFI_STATUS                       Status;
  BOOLEAN                          ApplyFailed;

  Response = (PLDM_FW_APPLY_COMPLETE_RESPONSE *)Task->Response;
  Request  = (PLDM_FW_APPLY_COMPLETE_REQUEST *)Task->RecvBuffer;
  if (Task->RecvLength < sizeof (*Request)) {
    DEBUG ((DEBUG_ERROR, "%a: %s invalid request size %u\n", __FUNCTION__, Task->DeviceName, Task->RecvLength));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_APPLY_COMPLETE_BAD_LEN);
    return StateFatalError;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: mActivationMethod=0x%x, AR=0x%x CAMM=0x%x, RAM=0x%x\n",
    __FUNCTION__,
    mActivationMethod,
    Request->ApplyResult,
    Request->ComponentActivationMethodsModification,
    Task->ImageInfo->RequestedActivationMethod
    ));

  Response->Common         = Request->Common;
  Response->CompletionCode = PLDM_SUCCESS;
  Task->ResponseLength     = sizeof (*Response);

  ApplyFailed = ((Request->ApplyResult != PLDM_FW_APPLY_RESULT_SUCCESS) &&
                 (Request->ApplyResult != PLDM_FW_APPLY_RESULT_SUCCESS_NEW_ACTIVATION));

  mActivationMethod |=
    (Request->ApplyResult == PLDM_FW_APPLY_RESULT_SUCCESS_NEW_ACTIVATION) ?
    Request->ComponentActivationMethodsModification :
    Task->ImageInfo->RequestedActivationMethod;

  Status = Task->FD->Send (
                       Task->FD,
                       FALSE,
                       Task->Response,
                       Task->ResponseLength,
                       &Task->RecvMsgTag
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s response failed: %r\n", __FUNCTION__, Task->DeviceName, Status));
  }

  if (ApplyFailed) {
    DEBUG ((DEBUG_ERROR, "%a: apply failure: 0x%x\n", __FUNCTION__, Request->ApplyResult));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_APPLY_COMPLETE_FAILED);
    return StateFatalError;
  }

  PldmFwTaskSetFDState (Task, FDStateReadyXfer);

  return StateNextComponent;
}

/**
  Task state handler to update task for next component in
  the Update Component sequence.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskNextComponent (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  Task->FwParamsComponentIndex++;
  if (PldmFwGetNextFwParamsMatchingComponent (
        Task->GetFwParamsResponse,
        &Task->FwParamsComponentIndex,
        Task->ImageInfo->Classification,
        Task->ImageInfo->Id
        ) != NULL)
  {
    DEBUG ((DEBUG_INFO, "%a: additional FD match for component %u, \n", __FUNCTION__, Task->ComponentImageIndex));

    return StatePassComponentTableSetupReq;
  }

  if (++Task->ComponentImageIndex >= Task->ImageInfoArea->ImageCount) {
    Task->IsExpectingFDRequests = FALSE;

    return StateActivateFwSetupReq;
  }

  Task->ImageInfo = PldmFwPkgGetNextComponentImage (Task->ImageInfo);

  return StateUpdateComponentSetupReq;
}

/**
  Task state handler to setup the Activate FW request.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskActivateFwSetupReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_ACTIVATE_FW_REQUEST  *Request;

  Request                                 = (PLDM_FW_ACTIVATE_FW_REQUEST *)Task->Request;
  Request->SelfContainedActivationRequest = FALSE;
  PldmFwTaskSendReqSetup (
    Task,
    PLDM_FW_ACTIVATE_FW,
    sizeof (*Request),
    StateActivateFwProcessRsp
    );
  Task->RspExtraMs = 20 * 1000;

  PldmFwTaskSetFDState (Task, FDStateActivate);

  return StateSendReq;
}

/**
  Task state handler to process the Activate FW response.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskActivateFwProcessRsp (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_ACTIVATE_FW_RESPONSE  *Response;
  EFI_STATUS                    Status;

  Response = (PLDM_FW_ACTIVATE_FW_RESPONSE *)Task->Response;

  Status = PldmFwCheckRspCompletionAndLength (Response, Task->RecvLength, sizeof (*Response), __FUNCTION__, Task->DeviceName);
  if (EFI_ERROR (Status)) {
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_ACTIVATE_FW_FAILED);
    return StateFatalError;
  }

  PldmFwTaskSetFDState (Task, FDStateIdle);

  return StateComplete;
}

/**
  Task state handler to update task for completion of FW update sequence.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskComplete (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  Task->Complete = TRUE;

  return StateMax;
}

/**
  Task state handler to receive a PLDM message from the FD.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskReceive (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  MCTP_PLDM_COMMON  *RecvHeader;
  EFI_STATUS        Status;
  UINT8             Command;

  ASSERT (Task->IsExpectingFDRequests || Task->RequestIsActive);

  Task->RecvLength = PLDM_FW_TASK_RECV_BUFFER_SIZE;
  Status           = Task->FD->Recv (
                                 Task->FD,
                                 0,
                                 Task->RecvBuffer,
                                 &Task->RecvLength,
                                 &Task->RecvMsgTag
                                 );

  if (Status == EFI_TIMEOUT) {
    if (PldmFwTaskTimerIsExpired (&Task->RequestFwDataTimer)) {
      DEBUG ((DEBUG_ERROR, "%a: %s request FW data timeout\n", __FUNCTION__, Task->DeviceName));
      PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_REQUEST_FW_DATA_TIMEOUT);
      return StateFatalError;
    }

    if (PldmFwTaskTimerIsExpired (&Task->RspTimer)) {
      DEBUG ((DEBUG_ERROR, "%a: %s timeout waiting on Cmd=0x%x response\n", __FUNCTION__, Task->DeviceName, ((MCTP_PLDM_COMMON  *)Task->Request)->Command));
      return StateRetryReq;
    }

    return StateReceive;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s Receive failed: %r\n", __FUNCTION__, Task->DeviceName, Status));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_RECEIVE_FAILED);
    return StateFatalError;
  }

  if (Task->RecvLength < sizeof (*RecvHeader)) {
    DEBUG ((DEBUG_ERROR, "%a: %s invalid RecvLength %u\n", __FUNCTION__, Task->DeviceName, Task->RecvLength));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_RECEIVE_BAD_LEN);
    return StateFatalError;
  }

  RecvHeader = (MCTP_PLDM_COMMON *)Task->RecvBuffer;
  if ((RecvHeader->MctpType != MCTP_TYPE_PLDM) &&
      (RecvHeader->PldmType != PLDM_TYPE_FW_UPDATE))
  {
    DEBUG ((DEBUG_ERROR, "%a: %s invalid type %u/%u\n", __FUNCTION__, Task->DeviceName, RecvHeader->MctpType, RecvHeader->PldmType));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_RECEIVE_BAD_TYPE);
    return StateFatalError;
  }

  Command = RecvHeader->Command;
  if (RecvHeader->InstanceId & PLDM_RQ) {
    switch (Command) {
      case PLDM_FW_REQUEST_FW_DATA:
        return StateRequestFwDataHandleReq;
      case PLDM_FW_TRANSFER_COMPLETE:
        return StateTransferCompleteHandleReq;
      case PLDM_FW_VERIFY_COMPLETE:
        return StateVerifyCompleteHandleReq;
      case PLDM_FW_APPLY_COMPLETE:
        return StateApplyCompleteHandleReq;
      default:
        DEBUG ((DEBUG_ERROR, "%a: %s unsupported command=0x%x\n", __FUNCTION__, Task->DeviceName, Command));
        PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_UNSUPPORTED_CMD);
        return StateFatalError;
    }
  } else {
    return StateProcessRsp;
  }
}

/**
  Task state handler to send a PLDM request to the FD.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskSendReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  NVIDIA_MCTP_PROTOCOL      *FD;
  MCTP_PLDM_REQUEST_HEADER  *Request;
  EFI_STATUS                Status;
  UINT8                     Command;

  FD      = Task->FD;
  Request = (MCTP_PLDM_REQUEST_HEADER *)Task->Request;
  Command = Request->Common.Command;

  Status = FD->Send (FD, TRUE, Request, Task->RequestLength, &Task->RequestMsgTag);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: %s Send failed Cmd=0x%x: %r\n", __FUNCTION__, Task->DeviceName, Command, Status));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_SEND_REQ_FAILED);
    return StateFatalError;
  }

  Task->RequestIsActive = TRUE;
  PldmFwTaskStartTimer (&Task->RspTimer, PLDM_PT2_MS_MAX + Task->RspExtraMs);

  return StateReceive;
}

/**
  Task state handler to process a PLDM response message.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskProcessRsp (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  MCTP_PLDM_REQUEST_HEADER   *Request;
  MCTP_PLDM_RESPONSE_HEADER  *Response;
  EFI_STATUS                 Status;

  Request  = (MCTP_PLDM_REQUEST_HEADER *)Task->Request;
  Response = (MCTP_PLDM_RESPONSE_HEADER *)Task->RecvBuffer;

  if (!Task->RequestIsActive) {
    DEBUG ((DEBUG_ERROR, "%a: %s rsp seq err, prev cmd=%u\n", __FUNCTION__, Task->DeviceName, Request->Common.Command));
    return StateReceive;
  }

  Task->RequestIsActive = FALSE;
  PldmFwTaskCancelTimer (&Task->RspTimer);

  Status = PldmValidateResponse (Request, Response, Task->RecvLength, Task->RequestMsgTag, Task->RecvMsgTag, Task->DeviceName);
  if (EFI_ERROR (Status)) {
    return StateRetryReq;
  }

  DEBUG ((DEBUG_INFO, "%a: Cmd=0x%x Comp=0x%x\n", __FUNCTION__, Response->Common.Command, Response->CompletionCode));

  return Task->ProcessResponseState;
}

/**
  Task state handler to retry a PLDM request.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskRetryReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  MCTP_PLDM_REQUEST_HEADER  *Request;
  UINT8                     Command;

  Request = (MCTP_PLDM_REQUEST_HEADER *)Task->Request;
  Command = Request->Common.Command;

  if (Task->RetryCount == 0) {
    DEBUG ((DEBUG_ERROR, "%a: %s Cmd=0x%x retries exhausted\n", __FUNCTION__, Task->DeviceName, Command));
    PldmFwTaskSetError (PLDM_FW_UPDATE_TASK_ERROR_REQ_RETRIES_EXHAUSTED);
    return StateFatalError;
  }

  Task->RetryCount--;
  DEBUG ((DEBUG_ERROR, "%a: %s retrying Cmd=0x%x\n", __FUNCTION__, Task->DeviceName, Command));

  return StateSendReq;
}

/**
  Task state handler to update task for a fatal error.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskFatalError (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  ASSERT (mError != PLDM_FW_UPDATE_TASK_ERROR_NONE);

  Task->Status = EFI_PROTOCOL_ERROR;

  return StateComplete;
}

/**
  Task state handler to setup the Cancel Update Component request.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskCancelUpdateComponentSetupReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_CANCEL_UPDATE_COMPONENT_REQUEST  *Request;

  Request = (PLDM_FW_CANCEL_UPDATE_COMPONENT_REQUEST *)Task->Request;
  PldmFwTaskSendReqSetup (
    Task,
    PLDM_FW_CANCEL_UPDATE_COMPONENT,
    sizeof (*Request),
    StateCancelUpdateComponentProcessRsp
    );

  return StateSendReq;
}

/**
  Task state handler to process the Cancel Update Component response.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskCancelUpdateComponentProcessRsp (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_CANCEL_UPDATE_COMPONENT_RESPONSE  *Response;

  Response = (PLDM_FW_CANCEL_UPDATE_COMPONENT_RESPONSE *)Task->RecvBuffer;
  if (Response->CompletionCode != PLDM_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s failed: 0x%x\n",
      __FUNCTION__,
      Task->DeviceName,
      Response->CompletionCode
      ));
  }

  return StateCancelUpdateSetupReq;
}

/**
  Task state handler to setup the Cancel Update request.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskCancelUpdateSetupReq (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_CANCEL_UPDATE_REQUEST  *Request;

  Request = (PLDM_FW_CANCEL_UPDATE_REQUEST *)Task->Request;
  PldmFwTaskSendReqSetup (
    Task,
    PLDM_FW_CANCEL_UPDATE,
    sizeof (*Request),
    StateCancelUpdateProcessRsp
    );

  return StateSendReq;
}

/**
  Task state handler to process the Cancel Update response.

  @param[in]  Task                      Pointer to task.

  @retval PLDM_FW_TASK_STATE            Next task state to execute.

**/
STATIC
PLDM_FW_TASK_STATE
EFIAPI
PldmFwTaskCancelUpdateProcessRsp (
  IN  PLDM_FW_UPDATE_TASK  *Task
  )
{
  PLDM_FW_CANCEL_UPDATE_RESPONSE  *Response;

  Response = (PLDM_FW_CANCEL_UPDATE_RESPONSE *)Task->RecvBuffer;
  if (Response->CompletionCode != PLDM_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s failed: 0x%x\n",
      __FUNCTION__,
      Task->DeviceName,
      Response->CompletionCode
      ));
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s complete: NonFunctioning flag=%u, bitmap=0x%llx\n",
      __FUNCTION__,
      Task->DeviceName,
      Response->NonFunctioningComponentIndication,
      Response->NonFunctioningComponentBitmap
      ));
  }

  return StateComplete;
}

/**
  Task state table

**/

#define PLDM_FW_TASK_STATE_TABLE_ENTRY(TaskStateName) \
  [State##TaskStateName] = {State##TaskStateName, PldmFwTask##TaskStateName} \

STATIC PLDM_FW_TASK_STATE_TABLE  mPldmFwTaskStateTable[] = {
  // IDLE state
  PLDM_FW_TASK_STATE_TABLE_ENTRY (Start),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (QueryDeviceIdentifiersSetupReq),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (QueryDeviceIdentifiersProcessRsp),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (GetFwParamsSetupReq),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (GetFwParamsProcessRsp),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (ProcessPackage),

  PLDM_FW_TASK_STATE_TABLE_ENTRY (RequestUpdateSetupReq),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (RequestUpdateProcessRsp),

  // LEARN COMPONENTS state
  PLDM_FW_TASK_STATE_TABLE_ENTRY (PassComponentTableSetupReq),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (PassComponentTableProcessRsp),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (PassComponentTableNextComponent),
  /// READY XFER state
  PLDM_FW_TASK_STATE_TABLE_ENTRY (UpdateComponentSetupReq),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (UpdateComponentProcessRsp),

  // DOWNLOAD/VERIFY/APPLY states, driven by FD requests
  PLDM_FW_TASK_STATE_TABLE_ENTRY (WaitForRequests),

  PLDM_FW_TASK_STATE_TABLE_ENTRY (RequestFwDataHandleReq),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (TransferCompleteHandleReq),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (VerifyCompleteHandleReq),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (ApplyCompleteHandleReq),

  // READY XFER state
  PLDM_FW_TASK_STATE_TABLE_ENTRY (NextComponent),

  PLDM_FW_TASK_STATE_TABLE_ENTRY (ActivateFwSetupReq),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (ActivateFwProcessRsp),

  // IDLE state, update complete
  PLDM_FW_TASK_STATE_TABLE_ENTRY (Complete),

  // Common request/response states
  PLDM_FW_TASK_STATE_TABLE_ENTRY (Receive),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (SendReq),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (ProcessRsp),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (RetryReq),

  // error states
  PLDM_FW_TASK_STATE_TABLE_ENTRY (FatalError),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (CancelUpdateComponentSetupReq),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (CancelUpdateComponentProcessRsp),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (CancelUpdateSetupReq),
  PLDM_FW_TASK_STATE_TABLE_ENTRY (CancelUpdateProcessRsp),
};

_Static_assert (sizeof (mPldmFwTaskStateTable) == StateMax * sizeof (mPldmFwTaskStateTable[0]), "bad table size");

/**
  Task state machine processing loop.

  @retval None

**/
STATIC
VOID
EFIAPI
PldmFwTaskStateMachineLoop (
  VOID
  )
{
  PLDM_FW_UPDATE_TASK  *Task;
  UINTN                Index;
  PLDM_FW_TASK_STATE   State;
  UINT64               EndNs;

  while (TRUE) {
    for (Index = 0; Index < mNumTasks; Index++) {
      Task = &mTasks[Index];
      if (Task->Complete) {
        continue;
      }

      State = Task->TaskState;
      ASSERT (State < StateMax);

      ASSERT (mPldmFwTaskStateTable[State].State == State);
      Task->TaskState = mPldmFwTaskStateTable[State].Func (Task);
      if (Task->Complete) {
        EndNs = GetTimeInNanoSecond (GetPerformanceCounter ());
        DEBUG ((
          DEBUG_INFO,
          "%a: State machine %u %s complete %llums: %r\n",
          __FUNCTION__,
          Index,
          Task->DeviceName,
          (EndNs - Task->StartNs) / PLDM_FW_TASK_MS_TO_NS (1),
          Task->Status
          ));

        if (EFI_ERROR (Task->Status)) {
          mStatus = EFI_PROTOCOL_ERROR;
        }

        if (++mNumTasksComplete == mNumTasks) {
          if (mStatus == EFI_SUCCESS) {
            PldmFwTaskProgress (100);
          }

          return;
        }
      }
    }
  }
}

EFI_STATUS
EFIAPI
PldmFwUpdateTaskExecuteAll (
  OUT PLDM_FW_UPDATE_TASK_ERROR  *Error,
  OUT UINT16                     *ActivationMethod
  )
{
  PldmFwTaskStateMachineLoop ();

  DEBUG ((DEBUG_INFO, "%a: alltasks done, activation=0x%x, err=0x%x: %r\n", __FUNCTION__, mActivationMethod, mError, mStatus));

  *ActivationMethod = mActivationMethod;
  *Error            = mError;

  return mStatus;
}

EFI_STATUS
EFIAPI
PldmFwUpdateTaskCreate (
  IN  NVIDIA_MCTP_PROTOCOL  *FD,
  IN  CONST VOID            *Package,
  IN  UINTN                 Length
  )
{
  PLDM_FW_UPDATE_TASK     *Task;
  MCTP_DEVICE_ATTRIBUTES  Attributes;
  EFI_STATUS              Status;

  Task = &mTasks[mNumTasks++];
  ZeroMem (Task, sizeof (*Task));

  Task->FD      = FD;
  Task->PkgHdr  = (CONST PLDM_FW_PKG_HDR *)Package;
  Task->PkgLen  = Length;
  Task->StartNs = GetTimeInNanoSecond (GetPerformanceCounter ());

  Status = FD->GetDeviceAttributes (FD, &Attributes);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Task->DeviceName = Attributes.DeviceName;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PldmFwUpdateTaskLibInit (
  IN  UINTN                         NumDevices,
  IN  PLDM_FW_UPDATE_TASK_PROGRESS  ProgressFunction
  )
{
  UINTN  Index;

  for (Index = 0; Index < StateMax; Index++) {
    if ((mPldmFwTaskStateTable[Index].State != Index) ||
        (mPldmFwTaskStateTable[Index].Func == NULL))
    {
      DEBUG ((DEBUG_ERROR, "%a: Bad table entry %u\n", __FUNCTION__, Index));
      return EFI_UNSUPPORTED;
    }
  }

  mNumDevices       = NumDevices;
  mNumTasks         = 0;
  mNumTasksComplete = 0;
  mStatus           = EFI_SUCCESS;
  mActivationMethod = 0;
  mTasks            = (PLDM_FW_UPDATE_TASK *)
                      AllocateRuntimePool (mNumDevices * sizeof (PLDM_FW_UPDATE_TASK));
  if (mTasks == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: mTasks allocation failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  mError            = PLDM_FW_UPDATE_TASK_ERROR_NONE;
  mProgressFunction = ProgressFunction;
  mCompletion       = 0;

  return EFI_SUCCESS;
}
