/** @file
  Redfish task library platform implementation.

  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "RedfishPlatformTaskInternal.h"

/**

  This function convert task state from REDFISH_TASK_STATE ASCII string.
  Caller does not need to free returned string.

  @param[in]  TaskState  Task state

  @retval     CHAR8 *     Task state in ASCII string.
  @retval     NULL        Errors occur.

**/
CHAR8 *
GetTaskStateString (
  IN REDFISH_TASK_STATE  TaskState
  )
{
  CHAR8  *Buffer;

  Buffer = NULL;
  switch (TaskState) {
    case RedfishTaskStateCompleted:
      Buffer = REDFISH_TASK_COMPLETED_STR;
      break;
    case RedfishTaskStateException:
      Buffer = REDFISH_TASK_EXCEPTION_STR;
      break;
    default:
      return NULL;
  }

  return Buffer;
}

/**

  This function add message to given message array

  @param[in]  JsonArray   Json array
  @param[in]  Message     Message to add
  @param[in]  OnSuccess   TRUE if this is success message.
                          FALSE otherwise.

  @retval     EFI_SUCCESS  Message is attached to JSON array.
  @retval     Others       Errors occur.

**/
EFI_STATUS
RedfishTaskAddMessage (
  IN EDKII_JSON_ARRAY  JsonArray,
  IN CHAR8             *Message,
  IN BOOLEAN           OnSuccess
  )
{
  EDKII_JSON_VALUE  MessageObj;
  EDKII_JSON_VALUE  BufferObj;

  if ((JsonArray == NULL) || IS_EMPTY_STRING (Message)) {
    return EFI_INVALID_PARAMETER;
  }

  MessageObj = NULL;
  BufferObj  = NULL;

  MessageObj = JsonValueInitObject ();
  if (MessageObj == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Message ID
  //
  if (OnSuccess) {
    BufferObj = JsonValueInitAsciiString (REDFISH_TASK_MSG_ID_SUCCESS);
    if (BufferObj == NULL) {
      goto ON_ERROR;
    }
  } else {
    BufferObj = JsonValueInitAsciiString (REDFISH_TASK_MSG_ID_ERROR);
    if (BufferObj == NULL) {
      goto ON_ERROR;
    }
  }

  JsonObjectSetValue (JsonValueGetObject (MessageObj), REDFISH_TASK_MSG_ID_ATTRIBUTE, BufferObj);

  //
  // Message
  //
  BufferObj = JsonValueInitAsciiString (Message);
  if (BufferObj == NULL) {
    goto ON_ERROR;
  }

  JsonObjectSetValue (JsonValueGetObject (MessageObj), REDFISH_TASK_MSG_ATTRIBUTE, BufferObj);
  JsonArrayAppendValue (JsonArray, MessageObj);

  return EFI_SUCCESS;

ON_ERROR:

  if (MessageObj != NULL) {
    JsonValueFree (MessageObj);
  }

  if (BufferObj != NULL) {
    JsonValueFree (BufferObj);
  }

  return EFI_OUT_OF_RESOURCES;
}

/**

  This function update task result to BMC task service. There is
  no standard way defined in Redfish specification that allows
  BIOS to update task state and status. Platform implement
  this function to update task result to BMC by following
  BMC defined interface.

  @param[in]  RedfishService  Instance to Redfish service.
  @param[in]  TaskUri         The URI of task to update result.
  @param[in]  TaskResult      The task state and task status to update.

  @retval     EFI_SUCCESS         Task state and status is updated to BMC.
  @retval     Others              Errors occur.

**/
EFI_STATUS
RedfishTaskUpdate (
  IN REDFISH_SERVICE      RedfishService,
  IN EFI_STRING           TaskUri,
  IN REDFISH_TASK_RESULT  TaskResult
  )
{
  EFI_STATUS            Status;
  REDFISH_RESPONSE      Response;
  CHAR8                 TaskUpdateUri[REDFISH_TASK_UPDATE_URI_MAX];
  EDKII_JSON_VALUE      TaskResultObj;
  EDKII_JSON_VALUE      TaskStateObj;
  EDKII_JSON_ARRAY      MessageArrayObj;
  CHAR8                 *TaskStateStr;
  CHAR8                 *JsonText;
  REDFISH_MESSAGE_DATA  *MessageArray;
  UINTN                 MessageCount;
  UINTN                 Index;

  if ((RedfishService == NULL) || IS_EMPTY_STRING (TaskUri)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((TaskResult.TaskState != RedfishTaskStateCompleted) && (TaskResult.TaskState != RedfishTaskStateException)) {
    DEBUG ((DEBUG_ERROR, "%a: only support completed or exception task state\n", __func__));
    return EFI_UNSUPPORTED;
  }

  ZeroMem (&Response, sizeof (Response));
  JsonText     = NULL;
  MessageArray = NULL;
  MessageCount = 0;
  AsciiSPrint (TaskUpdateUri, REDFISH_TASK_UPDATE_URI_MAX, "%s/%a", TaskUri, REDFISH_TASK_UPDATE_URI);

  //
  // Prepare task data
  //
  TaskStateStr = GetTaskStateString (TaskResult.TaskState);
  if (TaskStateStr == NULL) {
    return EFI_UNSUPPORTED;
  }

  TaskResultObj = JsonValueInitObject ();
  if (TaskResultObj == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  TaskStateObj = JsonValueInitAsciiString (TaskStateStr);
  if (TaskStateObj == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_RELEASE;
  }

  //
  // Get message of this task URI
  //
  Status = RedfishMessageGet (TaskUri, &MessageArray, &MessageCount);
  if (EFI_ERROR (Status)) {
    if (Status != EFI_NOT_FOUND) {
      DEBUG ((DEBUG_ERROR, "%a: failed to get message data: %r\n", __func__, Status));
      goto ON_RELEASE;
    }
  }

  MessageArrayObj = JsonValueInitArray ();
  if (MessageArrayObj == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_RELEASE;
  }

  if ((MessageCount == 0) && (TaskResult.TaskState == RedfishTaskStateCompleted)) {
    //
    // Add success message if there is no message returned and state is completed.
    //
    RedfishTaskAddMessage (MessageArrayObj, REDFISH_TASK_MSG_SUCCESS, TRUE);
  } else {
    for (Index = 0; Index < MessageCount; Index++) {
      RedfishTaskAddMessage (MessageArrayObj, MessageArray[Index].Message, (MessageArray[Index].MessageSeverity == RedfishMessageSeverityOk));
    }
  }

  JsonObjectSetValue (JsonValueGetObject (TaskResultObj), REDFISH_TASK_STATE_ATTRIBUTE, TaskStateObj);
  JsonObjectSetValue (JsonValueGetObject (TaskResultObj), REDFISH_TASK_MSG_ARRAY_ATTRIBUTE, MessageArrayObj);

  JsonText = JsonDumpString (TaskResultObj, EDKII_JSON_COMPACT);
  if (JsonText == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_RELEASE;
  }

  Status = RedfishPatchToUri (
             RedfishService,
             TaskUpdateUri,
             JsonText,
             &Response
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Patch resource: %a failed: %r\n", __func__, TaskUpdateUri, Status));
    DumpJsonValue (DEBUG_ERROR, TaskResultObj);
  }

ON_RELEASE:

  RedfishFreeResponse (
    Response.StatusCode,
    Response.HeaderCount,
    Response.Headers,
    Response.Payload
    );

  if (TaskResultObj != NULL) {
    JsonValueFree (TaskResultObj);
  }

  if (JsonText != NULL) {
    FreePool (JsonText);
  }

  if (MessageArray != NULL) {
    RedfishMessageFree (MessageArray, MessageCount);
  }

  return Status;
}
