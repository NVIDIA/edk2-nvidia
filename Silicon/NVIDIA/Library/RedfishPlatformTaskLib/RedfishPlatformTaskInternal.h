/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef REDFISH_PLATFORM_TASK_INTERNAL_H_
#define REDFISH_PLATFORM_TASK_INTERNAL_H_

#include <Uefi.h>
#include <RedfishBase.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/JsonLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/RedfishLib.h>
#include <Library/RedfishDebugLib.h>
#include <Library/RedfishFeatureUtilityLib.h>
#include <Library/RedfishTaskLib.h>
#include <Library/RedfishMessageLib.h>

#define REDFISH_TASK_UPDATE_URI_MAX       128
#define REDFISH_TASK_UPDATE_URI           "Update"
#define REDFISH_TASK_COMPLETED_STR        "Completed"
#define REDFISH_TASK_EXCEPTION_STR        "Exception"
#define REDFISH_TASK_STATE_ATTRIBUTE      "TaskState"
#define REDFISH_TASK_MSG_ID_ATTRIBUTE     "MessageId"
#define REDFISH_TASK_MSG_ATTRIBUTE        "Message"
#define REDFISH_TASK_MSG_ARRAY_ATTRIBUTE  "Messages"
#define REDFISH_TASK_MSG_ID_SUCCESS       "Base.1.0.Success"
#define REDFISH_TASK_MSG_ID_ERROR         "Base.1.0.GeneralError"
#define REDFISH_TASK_MSG_SUCCESS          "The request completed successfully"
#endif
