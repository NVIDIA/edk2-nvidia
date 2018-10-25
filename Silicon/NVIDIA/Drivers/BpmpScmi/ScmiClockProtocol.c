/** @file

  Copyright (c) 2017-2018, Arm Limited. All rights reserved.
  Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  System Control and Management Interface V1.0
    http://infocenter.arm.com/help/topic/com.arm.doc.den0056a/
    DEN0056A_System_Control_and_Management_Interface.pdf
**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/ArmScmiClockProtocol.h>
#include <Protocol/BpmpIpc.h>
#include <Protocol/ClockParents.h>

#include "BpmpScmiClockProtocolPrivate.h"

STATIC NVIDIA_BPMP_IPC_PROTOCOL *mBpmpIpcProtocol = NULL;

/** Return version of the clock management protocol supported by SCP firmware.

  @param[in]  This     A Pointer to SCMI_CLOCK_PROTOCOL Instance.

  @param[out] Version  Version of the supported SCMI Clock management protocol.

  @retval EFI_SUCCESS       The version is returned.
  @retval EFI_DEVICE_ERROR  SCP returns an SCMI error.
  @retval !(EFI_SUCCESS)    Other errors.
**/
STATIC
EFI_STATUS
ClockGetVersion (
  IN  SCMI_CLOCK_PROTOCOL  *This,
  OUT UINT32               *Version
  )
{
  if (Version == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *Version = 0x10000;
  return EFI_SUCCESS;
}

/** Return total number of clock devices supported by the clock management
  protocol.

  @param[in]  This         A Pointer to SCMI_CLOCK_PROTOCOL Instance.

  @param[out] TotalClocks  Total number of clocks supported.

  @retval EFI_SUCCESS       Total number of clocks supported is returned.
  @retval EFI_DEVICE_ERROR  SCP returns an SCMI error.
  @retval !(EFI_SUCCESS)    Other errors.
**/
STATIC
EFI_STATUS
ClockGetTotalClocks (
  IN  SCMI_CLOCK_PROTOCOL  *This,
  OUT UINT32               *TotalClocks
  )
{
  EFI_STATUS Status;
  BPMP_CLOCK_REQUEST Request;

  if ((This == NULL) || (TotalClocks == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Request.Subcommand = ClockSubcommandGetMaxClockId;
  Request.ClockId    = 0;

  Status = mBpmpIpcProtocol->Communicate (
                               mBpmpIpcProtocol,
                               NULL,
                               MRQ_CLK,
                               (VOID *)&Request,
                               sizeof (UINT32),
                               (VOID *)TotalClocks,
                               sizeof (UINT32),
                               NULL
                               );
  if (!EFI_ERROR (Status)) {
    *TotalClocks += 1;
  }
  if (*TotalClocks > SCMI_CLOCK_PROTOCOL_NUM_CLOCKS_MASK) {
    *TotalClocks = SCMI_CLOCK_PROTOCOL_NUM_CLOCKS_MASK;
  }
  return Status;
}

/** Return attributes of a clock device.

  @param[in]  This        A Pointer to SCMI_CLOCK_PROTOCOL Instance.
  @param[in]  ClockId     Identifier for the clock device.

  @param[out] Enabled         If TRUE, the clock device is enabled.
  @param[out] ClockAsciiName  A NULL terminated ASCII string with the clock
                              name, of up to 16 bytes.

  @retval EFI_SUCCESS          Clock device attributes are returned.
  @retval EFI_DEVICE_ERROR     SCP returns an SCMI error.
  @retval !(EFI_SUCCESS)       Other errors.
**/
STATIC
EFI_STATUS
ClockGetClockAttributes (
  IN  SCMI_CLOCK_PROTOCOL  *This,
  IN  UINT32               ClockId,
  OUT BOOLEAN              *Enabled,
  OUT CHAR8                *ClockAsciiName
  )
{
  EFI_STATUS                       Status;
  BPMP_CLOCK_REQUEST               Request;
  BPMP_CLOCK_GET_ALL_INFO_RESPONSE Response;
  UINT32                           IsEnabled;
  INT32                            MessageError;

  if ((This == NULL) ||
      (Enabled == NULL) ||
      (ClockAsciiName == NULL) ||
      (ClockId >= SCMI_CLOCK_PROTOCOL_NUM_CLOCKS_MASK)) {
    return EFI_INVALID_PARAMETER;
  }

  Request.Subcommand = ClockSubcommandIsEnabled;
  Request.ClockId    = ClockId;

  Status = mBpmpIpcProtocol->Communicate (
                               mBpmpIpcProtocol,
                               NULL,
                               MRQ_CLK,
                               (VOID *)&Request,
                               sizeof (UINT32),
                               (VOID *)&IsEnabled,
                               sizeof (UINT32),
                               &MessageError
                               );
  if (EFI_ERROR (Status)) {
    if ((Status == EFI_PROTOCOL_ERROR) && (MessageError == BPMP_EINVAL)) {
      //Clock is not visible to the MRQ
      Status = EFI_NOT_FOUND;
    }
    return Status;
  }
  *Enabled = (IsEnabled != 0);

  Request.Subcommand = ClockSubcommandGetAllInfo;
  Request.ClockId    = ClockId;

  Status = mBpmpIpcProtocol->Communicate (
                               mBpmpIpcProtocol,
                               NULL,
                               MRQ_CLK,
                               (VOID *)&Request,
                               sizeof (UINT32),
                               (VOID *)&Response,
                               sizeof (BPMP_CLOCK_GET_ALL_INFO_RESPONSE),
                               &MessageError
                               );
  if (EFI_ERROR (Status)) {
    if ((Status == EFI_PROTOCOL_ERROR) && (MessageError == BPMP_EINVAL)) {
      //Clock is not visible to the MRQ
      Status = EFI_NOT_FOUND;
    }
    return Status;
  }

  CopyMem (ClockAsciiName, Response.Name, SCMI_MAX_STR_LEN - 1);
  ClockAsciiName [SCMI_MAX_STR_LEN - 1] = '\0';

  if (AsciiStrSize (Response.Name) > SCMI_MAX_STR_LEN) {
    DEBUG ((EFI_D_VERBOSE, "String %a, too large truncated to %a\r\n", Response.Name, ClockAsciiName));
    Status = EFI_WARN_BUFFER_TOO_SMALL;
  }
  return Status;
}

/** Return list of rates supported by a given clock device.

  @param[in] This        A pointer to SCMI_CLOCK_PROTOCOL Instance.
  @param[in] ClockId     Identifier for the clock device.

  @param[out] Format      SCMI_CLOCK_RATE_FORMAT_DISCRETE: Clock device
                          supports range of clock rates which are non-linear.

                          SCMI_CLOCK_RATE_FORMAT_LINEAR: Clock device supports
                          range of linear clock rates from Min to Max in steps.

  @param[out] TotalRates  Total number of rates.

  @param[in,out] RateArraySize  Size of the RateArray.

  @param[out] RateArray   List of clock rates.

  @retval EFI_SUCCESS          List of clock rates is returned.
  @retval EFI_DEVICE_ERROR     SCP returns an SCMI error.
  @retval EFI_BUFFER_TOO_SMALL RateArraySize is too small for the result.
                               It has been updated to the size needed.
  @retval !(EFI_SUCCESS)       Other errors.
**/
STATIC
EFI_STATUS
ClockDescribeRates (
  IN     SCMI_CLOCK_PROTOCOL     *This,
  IN     UINT32                   ClockId,
  OUT    SCMI_CLOCK_RATE_FORMAT  *Format,
  OUT    UINT32                  *TotalRates,
  IN OUT UINT32                  *RateArraySize,
  OUT    SCMI_CLOCK_RATE         *RateArray
  )
{
  return EFI_UNSUPPORTED;
}

/** Get clock rate.

  @param[in]  This        A Pointer to SCMI_CLOCK_PROTOCOL Instance.
  @param[in]  ClockId     Identifier for the clock device.

  @param[out]  Rate       Clock rate.

  @retval EFI_SUCCESS          Clock rate is returned.
  @retval EFI_DEVICE_ERROR     SCP returns an SCMI error.
  @retval !(EFI_SUCCESS)       Other errors.
**/
STATIC
EFI_STATUS
ClockRateGet (
  IN  SCMI_CLOCK_PROTOCOL  *This,
  IN  UINT32               ClockId,
  OUT UINT64               *Rate
  )
{
  EFI_STATUS         Status;
  BPMP_CLOCK_REQUEST Request;
  INT32              MessageError;

  if ((This == NULL) || (Rate == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (ClockId >= SCMI_CLOCK_PROTOCOL_NUM_CLOCKS_MASK) {
    return EFI_INVALID_PARAMETER;
  }

  Request.Subcommand = ClockSubcommandGetRate;
  Request.ClockId    = ClockId;

  Status = mBpmpIpcProtocol->Communicate (
                               mBpmpIpcProtocol,
                               NULL,
                               MRQ_CLK,
                               (VOID *)&Request,
                               sizeof (UINT32),
                               (VOID *)Rate,
                               sizeof (UINT64),
                               &MessageError
                               );
  if ((Status == EFI_PROTOCOL_ERROR) && (MessageError == BPMP_EINVAL)) {
    //Clock is not visible to the MRQ
    Status = EFI_NOT_FOUND;
  }
  return Status;
}

/** Set clock rate.

  @param[in]  This        A Pointer to SCMI_CLOCK_PROTOCOL Instance.
  @param[in]  ClockId     Identifier for the clock device.
  @param[in]  Rate        Clock rate.

  @retval EFI_SUCCESS          Clock rate set success.
  @retval EFI_DEVICE_ERROR     SCP returns an SCMI error.
  @retval !(EFI_SUCCESS)       Other errors.
**/
STATIC
EFI_STATUS
ClockRateSet (
  IN SCMI_CLOCK_PROTOCOL  *This,
  IN UINT32               ClockId,
  IN UINT64               Rate
  )
{
  EFI_STATUS         Status;
  BPMP_CLOCK_REQUEST Request;
  UINT64             NewRate;
  INT32              MessageError;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (ClockId >= SCMI_CLOCK_PROTOCOL_NUM_CLOCKS_MASK) {
    return EFI_INVALID_PARAMETER;
  }

  Request.Subcommand = ClockSubcommandSetRate;
  Request.ClockId    = ClockId;
  Request.ParentId   = 0;
  Request.Rate       = Rate;

  Status = mBpmpIpcProtocol->Communicate (
                               mBpmpIpcProtocol,
                               NULL,
                               MRQ_CLK,
                               (VOID *)&Request,
                               sizeof (BPMP_CLOCK_REQUEST),
                               (VOID *)&NewRate,
                               sizeof (UINT64),
                               &MessageError
                               );
  if ((Status == EFI_PROTOCOL_ERROR) && (MessageError == BPMP_EINVAL)) {
    //Clock is not visible to the MRQ
    Status = EFI_NOT_FOUND;
  }
  if (Rate != NewRate) {
    DEBUG ((EFI_D_INFO,
            "%a: Clock %d, attempt set to %16ld, was set to %16ld\r\n",
            __FUNCTION__,
            ClockId,
            Rate,
            NewRate
            ));
  }
  return Status;
}

/** Enable/Disable specified clock.

  @param[in]  This        A Pointer to SCMI_CLOCK_PROTOCOL Instance.
  @param[in]  ClockId     Identifier for the clock device.
  @param[in]  Enable      TRUE to enable, FALSE to disable.

  @retval EFI_SUCCESS          Clock enable/disable successful.
  @retval EFI_DEVICE_ERROR     SCP returns an SCMI error.
  @retval !(EFI_SUCCESS)       Other errors.
**/
STATIC
EFI_STATUS
ClockEnable (
  IN SCMI_CLOCK_PROTOCOL  *This,
  IN UINT32               ClockId,
  IN BOOLEAN              Enable
  )
{
  EFI_STATUS         Status;
  BPMP_CLOCK_REQUEST Request;
  INT32              MessageError;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (ClockId >= SCMI_CLOCK_PROTOCOL_NUM_CLOCKS_MASK) {
    return EFI_INVALID_PARAMETER;
  }

  if (Enable) {
    Request.Subcommand = ClockSubcommandEnable;
  } else {
    Request.Subcommand = ClockSubcommandDisable;
  }
  Request.ClockId    = ClockId;

  Status = mBpmpIpcProtocol->Communicate (
                               mBpmpIpcProtocol,
                               NULL,
                               MRQ_CLK,
                               (VOID *)&Request,
                               sizeof (UINT32),
                               NULL,
                               0,
                               &MessageError
                               );
  if ((Status == EFI_PROTOCOL_ERROR) && (MessageError == BPMP_EINVAL)) {
    //Clock is not visible to the MRQ
    Status = EFI_NOT_FOUND;
  }
  return Status;
}

// Instance of the SCMI clock management protocol.
STATIC CONST SCMI_CLOCK_PROTOCOL ScmiClockProtocol = {
  ClockGetVersion,
  ClockGetTotalClocks,
  ClockGetClockAttributes,
  ClockDescribeRates,
  ClockRateGet,
  ClockRateSet,
  ClockEnable
 };

/**
  This function checks is the given parent is a parent of the specified clock.

  @param[in]     This                The instance of the NVIDIA_CLOCK_PARENTS_PROTOCOL.
  @param[in]     ClockId             ClockId to check parent against
  @param[in]     ParentId            ClockId of the parent

  @return EFI_SUCCESS                Parent is supported by clock
  @return EFI_NOT_FOUND              Parent is not supported by clock.
  @return others                     Failed to check if parent is supported
**/
EFI_STATUS
ClockParentsIsParent (
  IN  NVIDIA_CLOCK_PARENTS_PROTOCOL *This,
  IN  UINT32                        ClockId,
  IN  UINT32                        ParentId
  )
{
  EFI_STATUS Status;
  UINT32 NumberOfParents;
  UINT32 *ParentIds = NULL;
  UINT32 ParentIndex;

  Status = This->GetParents (This, ClockId, &NumberOfParents, &ParentIds);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (ParentIndex = 0; ParentIndex < NumberOfParents; ParentIndex++) {
    if (ParentIds[ParentIndex] == ParentId) {
      break;
    }
  }

  if (ParentIndex != NumberOfParents) {
    Status = EFI_SUCCESS;
  } else {
    Status = EFI_NOT_FOUND;
  }
  FreePool (ParentIds);
  return Status;
}

/**
  This function sets the parent for the specified clock.

  @param[in]     This                The instance of the NVIDIA_CLOCK_PARENTS_PROTOCOL.
  @param[in]     ClockId             ClockId to set parent for
  @param[in]     ParentId            ClockId of the parent

  @return EFI_SUCCESS                Parent is set for clock
  @return EFI_NOT_FOUND              Parent is not supported by clock.
  @return others                     Failed to set parent
**/
EFI_STATUS
ClockParentsSetParent (
  IN  NVIDIA_CLOCK_PARENTS_PROTOCOL *This,
  IN  UINT32                        ClockId,
  IN  UINT32                        ParentId
  )
{
  EFI_STATUS         Status;
  BPMP_CLOCK_REQUEST Request;
  INT32              MessageError;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (ClockId >= SCMI_CLOCK_PROTOCOL_NUM_CLOCKS_MASK) {
    return EFI_INVALID_PARAMETER;
  }

  Request.Subcommand = ClockSubcommandSetParent;
  Request.ClockId    = ClockId;
  Request.ParentId   = ParentId;

  Status = mBpmpIpcProtocol->Communicate (
                               mBpmpIpcProtocol,
                               NULL,
                               MRQ_CLK,
                               (VOID *)&Request,
                               sizeof (BPMP_CLOCK_REQUEST),
                               NULL,
                               0,
                               &MessageError
                               );
  if ((Status == EFI_PROTOCOL_ERROR) && (MessageError == BPMP_EINVAL)) {
    //Clock is not visible to the MRQ
    Status = EFI_NOT_FOUND;
  }
  return Status;
}

/**
  This function gets the current parent of the specified clock.

  @param[in]     This                The instance of the NVIDIA_CLOCK_PARENTS_PROTOCOL.
  @param[in]     ClockId             ClockId to check parent of
  @param[out]    ParentId            ClockId of the parent

  @return EFI_SUCCESS                Parent is supported by clock
  @return others                     Failed to get parent
**/
EFI_STATUS
ClockParentsGetParent (
  IN  NVIDIA_CLOCK_PARENTS_PROTOCOL *This,
  IN  UINT32                        ClockId,
  OUT UINT32                        *ParentId
  )
{
  EFI_STATUS         Status;
  BPMP_CLOCK_REQUEST Request;
  INT32              MessageError;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (ClockId >= SCMI_CLOCK_PROTOCOL_NUM_CLOCKS_MASK) {
    return EFI_INVALID_PARAMETER;
  }

  Request.Subcommand = ClockSubcommandGetParent;
  Request.ClockId    = ClockId;

  Status = mBpmpIpcProtocol->Communicate (
                               mBpmpIpcProtocol,
                               NULL,
                               MRQ_CLK,
                               (VOID *)&Request,
                               sizeof (BPMP_CLOCK_REQUEST),
                               ParentId,
                               sizeof (UINT32),
                               &MessageError
                               );
  if ((Status == EFI_PROTOCOL_ERROR) && (MessageError == BPMP_EINVAL)) {
    //Clock is not visible to the MRQ
    Status = EFI_NOT_FOUND;
  }
  return Status;
}

/**
  This function gets the supported parents of the specified clock.

  @param[in]     This                The instance of the NVIDIA_CLOCK_PARENTS_PROTOCOL.
  @param[in]     ClockId             ClockId to check parents of.
  @param[out]    NumberOfParents     Number of parents supported
  @param[out]    ParentIds           Array of parent clock IDs supported. The caller should free this buffer.

  @return EFI_SUCCESS                Parent list is retrieved
  @return others                     Failed to get parent list
**/
EFI_STATUS
ClockParentsGetParents (
  IN  NVIDIA_CLOCK_PARENTS_PROTOCOL *This,
  IN  UINT32                        ClockId,
  OUT UINT32                        *NumberOfParents,
  OUT UINT32                        **ParentIds
  )
{
  EFI_STATUS                       Status;
  BPMP_CLOCK_REQUEST               Request;
  BPMP_CLOCK_GET_ALL_INFO_RESPONSE Response;
  INT32                            MessageError;

  if ((This == NULL) ||
      (NumberOfParents == NULL) ||
      (ParentIds == NULL) ||
      (ClockId >= SCMI_CLOCK_PROTOCOL_NUM_CLOCKS_MASK)) {
    return EFI_INVALID_PARAMETER;
  }

  Request.Subcommand = ClockSubcommandGetAllInfo;
  Request.ClockId    = ClockId;

  Status = mBpmpIpcProtocol->Communicate (
                               mBpmpIpcProtocol,
                               NULL,
                               MRQ_CLK,
                               (VOID *)&Request,
                               sizeof (UINT32),
                               (VOID *)&Response,
                               sizeof (BPMP_CLOCK_GET_ALL_INFO_RESPONSE),
                               &MessageError
                               );
  if (EFI_ERROR (Status)) {
    if ((Status == EFI_PROTOCOL_ERROR) && (MessageError == BPMP_EINVAL)) {
      //Clock is not visible to the MRQ
      Status = EFI_NOT_FOUND;
    }
    return Status;
  }

  *NumberOfParents = Response.NumberOfParents;
  *ParentIds = (UINT32 *)AllocateCopyPool (sizeof (UINT32) * Response.NumberOfParents, Response.Parents);
  if (*ParentIds == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
  }

  return Status;
}

// Instance of the clock parents protocol.
STATIC CONST NVIDIA_CLOCK_PARENTS_PROTOCOL mClockParentsProtocol = {
  ClockParentsIsParent,
  ClockParentsSetParent,
  ClockParentsGetParent,
  ClockParentsGetParents
 };

/** Initialize clock management protocol and install protocol on a given handle.

  @param[in] Handle              Handle to install clock management protocol.

  @retval EFI_SUCCESS            Clock protocol interface installed successfully.
**/
EFI_STATUS
ScmiClockProtocolInit (
  IN EFI_HANDLE* Handle
  )
{
  EFI_STATUS Status = gBS->LocateProtocol (
                             &gNVIDIABpmpIpcProtocolGuid,
                             NULL,
                             (VOID **)&mBpmpIpcProtocol
                             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return gBS->InstallMultipleProtocolInterfaces (
                Handle,
                &gArmScmiClockProtocolGuid,
                &ScmiClockProtocol,
                &gNVIDIAClockParentsProtocolGuid,
                &mClockParentsProtocol,
                NULL
                );
}

