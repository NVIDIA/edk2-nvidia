/** @file

  PLDM FW update definitions and helper functions

  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PLDM_FW_UPDATE_LIB_H__
#define __PLDM_FW_UPDATE_LIB_H__

#include <Library/PldmBaseLib.h>

#define PLDM_FW_BASELINE_TRANSFER_SIZE  32

// inventory commands
#define PLDM_FW_QUERY_DEVICE_IDS          0x01
#define PLDM_FW_GET_FW_PARAMS             0x02
#define PLDM_FW_QUERY_DOWNSTREAM_DEVICES  0x03
#define PLDM_FW_QUERY_DOWNSTREAM_IDS      0x04
#define PLDM_FW_GET_DOWNSTREAM_FW_PARAMS  0x05

// update commands
#define PLDM_FW_REQUEST_UPDATE                        0x10
#define PLDM_FW_GET_PACKAGE_DATA                      0x11
#define PLDM_FW_GET_DEVICE_META_DATA                  0x12
#define PLDM_FW_PASS_COMPONENT_TABLE                  0x13
#define PLDM_FW_UPDATE_COMPONENT                      0x14
#define PLDM_FW_REQUEST_FW_DATA                       0x15
#define PLDM_FW_TRANSFER_COMPLETE                     0x16
#define PLDM_FW_VERIFY_COMPLETE                       0x17
#define PLDM_FW_APPLY_COMPLETE                        0x18
#define PLDM_FW_GET_META_DATA                         0x19
#define PLDM_FW_ACTIVATE_FW                           0x1a
#define PLDM_FW_GET_STATUS                            0x1b
#define PLDM_FW_CANCEL_UPDATE_COMPONENT               0x1c
#define PLDM_FW_CANCEL_UPDATE                         0x1d
#define PLDM_FW_ACTIVATE_PENDING_COMPONENT_IMAGE_SET  0x1e
#define PLDM_FW_ACTIVATE_PENDING_COMPONENT_IMAGE      0x1f
#define PLDM_FW_REQUEST_DOWNSTREAM_DEVICE_UPDATE      0x20

// error codes
#define PLDM_FW_NOT_IN_UPDATE_MODE                       0x80
#define PLDM_FW_ALREADY_IN_UPDATE_MODE                   0x81
#define PLDM_FW_DATA_OUT_OF_RANGE                        0x82
#define PLDM_FW_INVALID_TRANSFER_LENGTH                  0x83
#define PLDM_FW_INVALID_STATE_FOR_COMMAND                0x84
#define PLDM_FW_INCOMPLETE_UPDATE                        0x85
#define PLDM_FW_BUSY_IN_BACKGROUND                       0x86
#define PLDM_FW_CANCEL_PENDING                           0x87
#define PLDM_FW_COMMAND_NOT_EXPECTED                     0x88
#define PLDM_FW_RETRY_REQUEST_FW_DATA                    0x89
#define PLDM_FW_UNABLE_TO_INITIATE_UPDATE                0x8a
#define PLDM_FW_ACTIVATION_NOT_REQUIRED                  0x8b
#define PLDM_FW_SELF_CONTAINED_ACTIVATION_NOT_PERMITTED  0x8c
#define PLDM_FW_NO_DEVICE_METADATA                       0x8d
#define PLDM_FW_RETRY_REQUEST_UPDATE                     0x8e
#define PLDM_FW_NO_PACKAGE_DATA                          0x8f
#define PLDM_FW_INVALID_TRANSFER_HANDLE                  0x90
#define PLDM_FW_INVALID_TRANSFER_OPERATION_FLAG          0x91
#define PLDM_FW_ACTIVATE_PENDING_IMAGE_NOT_PERMITTED     0x92
#define PLDM_FW_PACKAGE_DATA_ERROR                       0x93

// PLDM FW Update timing
// UAFD_T1: Number of request retries when a response is received that requires a retry
#define PLDM_FW_UAFD_T1_RETRIES  2
// UA_T1: Retry interval to send next cancel command
#define PLDM_FW_UA_T1_MS_MIN  500
#define PLDM_FW_UA_T1_MS_MAX  (5 * 1000)
// UA_T2: Request firmware data idle timeout
#define PLDM_FW_UA_T2_MS_MIN  (60 * 1000)
#define PLDM_FW_UA_T2_MS_MAX  (90 * 1000)
// UA_T3: State change timeout
#define PLDM_FW_UA_T3_MS_MIN  (180 * 1000)
// UA_T4: Retry request for update
#define PLDM_FW_UA_T4_MS_MIN  (1 * 1000)
#define PLDM_FW_UA_T4_MS_MAX  (5 * 1000)
// UA_T5: Get Package Data timeout
#define PLDM_FW_UA_T5_MS_MIN  (1 * 1000)
#define PLDM_FW_UA_T5_MS_MAX  (5 * 1000)

// FW String types
#define PLDM_FW_STRING_TYPE_UNKNOWN   0x00
#define PLDM_FW_STRING_TYPE_ASCII     0x01
#define PLDM_FW_STRING_TYPE_UTF_8     0x02
#define PLDM_FW_STRING_TYPE_UTF_16    0x03
#define PLDM_FW_STRING_TYPE_UTF_16LE  0x04
#define PLDM_FW_STRING_TYPE_UTF_16BE  0x05

// Component classifications
#define PLDM_FW_COMPONENT_CLASS_UNKNOWN               0x0000
#define PLDM_FW_COMPONENT_CLASS_OTHER                 0x0001
#define PLDM_FW_COMPONENT_CLASS_DRIVER                0x0002
#define PLDM_FW_COMPONENT_CLASS_CONFIG_SW             0x0003
#define PLDM_FW_COMPONENT_CLASS_APP_SW                0x0004
#define PLDM_FW_COMPONENT_CLASS_INSTRUMENTATION       0x0005
#define PLDM_FW_COMPONENT_CLASS_FW_BIOS               0x0006
#define PLDM_FW_COMPONENT_CLASS_DIAG_SW               0x0007
#define PLDM_FW_COMPONENT_CLASS_OS                    0x0008
#define PLDM_FW_COMPONENT_CLASS_MIDDLEWARE            0x0009
#define PLDM_FW_COMPONENT_CLASS_FW                    0x000a
#define PLDM_FW_COMPONENT_CLASS_BIOS_FCODE            0x000b
#define PLDM_FW_COMPONENT_CLASS_SUPPORT_SERVICE_PACK  0x000c
#define PLDM_FW_COMPONENT_CLASS_SW_BUNDLE             0x000d
#define PLDM_FW_COMPONENT_CLASS_VENDOR_DEFINED_START  0x8000
#define PLDM_FW_COMPONENT_CLASS_DOWNSTREAM_DEVICE     0xffff

// FW descriptor types
#define PLDM_FW_DESCRIPTOR_TYPE_PCI_VENDOR       0x0000
#define PLDM_FW_DESCRIPTOR_TYPE_IANA_ENTERPRISE  0x0001
#define PLDM_FW_DESCRIPTOR_TYPE_UUID             0x0002
#define PLDM_FW_DESCRIPTOR_TYPE_PNP_VENDOR       0x0003
#define PLDM_FW_DESCRIPTOR_TYPE_ACPI_VENDOR      0x0004
#define PLDM_FW_DESCRIPTOR_TYPE_IEEE_COMPANY     0x0005
#define PLDM_FW_DESCRIPTOR_TYPE_SCSI_VENDOR      0x0006
#define PLDM_FW_DESCRIPTOR_TYPE_VENDOR           0xffff

// field values
#define PLDM_FW_UPDATE_COMPONENT_REQUEST_FORCE_UPDATE  BIT0

#define PLDM_FW_ACTIVATION_RESERVED               0xffc0
#define PLDM_FW_ACTIVATION_AC_POWER_CYCLE         0x0020
#define PLDM_FW_ACTIVATION_DC_POWER_CYCLE         0x0010
#define PLDM_FW_ACTIVATION_SYSTEM_REBOOT          0x0008
#define PLDM_FW_ACTIVATION_MEDIUM_SPECIFIC_RESET  0x0004
#define PLDM_FW_ACTIVATION_SELF_CONTAINED         0x0002
#define PLDM_FW_ACTIVATION_AUTOMATIC              0x0001

#define PLDM_FW_COMPONENT_COMPATIBILITY_OK     0
#define PLDM_FW_COMPONENT_COMPATIBILITY_ERROR  1

#define PLDM_FW_COMPONENT_COMPATIBILITY_CODE_OK  0

#define PLDM_FW_TRANSFER_FLAG_START   0x01
#define PLDM_FW_TRANSFER_FLAG_MIDDLE  0x02
#define PLDM_FW_TRANSFER_FLAG_END     0x04

// result codes
// TransferComplete TransferResult field DSP0267 spec-defined error values
#define PLDM_FW_TRANSFER_RESULT_SPEC_RANGE_MIN           0x00
#define PLDM_FW_TRANSFER_RESULT_SPEC_RANGE_MAX           0x1f
#define PLDM_FW_TRANSFER_RESULT_SUCCESS                  0x00
#define PLDM_FW_TRANSFER_RESULT_IMAGE_CORRUPT            0x01
#define PLDM_FW_TRANSFER_RESULT_VERSION_MISMATCH         0x02
#define PLDM_FW_TRANSFER_RESULT_FD_ABORTED               0x03
#define PLDM_FW_TRANSFER_RESULT_TIMEOUT                  0x09
#define PLDM_FW_TRANSFER_RESULT_GENERIC_ERROR            0x0a
#define PLDM_FW_TRANSFER_RESULT_FD_LOW_POWER             0x0b
#define PLDM_FW_TRANSFER_RESULT_FD_NEEDS_RESET           0x0c
#define PLDM_FW_TRANSFER_RESULT_FD_STORE_ERROR           0x0d
#define PLDM_FW_TRANSFER_RESULT_INVALID_OPAQUE_DATA      0x0e
#define PLDM_FW_TRANSFER_RESULT_DOWNSTREAM_FAILURE       0x0f
#define PLDM_FW_TRANSFER_RESULT_SECURITY_REVISION_ERROR  0x10
// TransferComplete TransferResult field vendor-defined error values
//   for NVIDIA codes, see PLDM_FW_NV_TRANSFER_RESULT enum
#define PLDM_FW_TRANSFER_RESULT_VENDOR_RANGE_MIN  0x70
#define PLDM_FW_TRANSFER_RESULT_VENDOR_RANGE_MAX  0x8f

// VerifyComplete VerifyResult field DSP0267 spec-defined error values
#define PLDM_FW_VERIFY_RESULT_SPEC_RANGE_MIN           0x00
#define PLDM_FW_VERIFY_RESULT_SPEC_RANGE_MAX           0x1f
#define PLDM_FW_VERIFY_RESULT_SUCCESS                  0x00
#define PLDM_FW_VERIFY_RESULT_VERIFY_FAILED            0x01
#define PLDM_FW_VERIFY_RESULT_VERSION_MISMATCH         0x02
#define PLDM_FW_VERIFY_RESULT_SECURITY_CHECK_FAILED    0x03
#define PLDM_FW_VERIFY_RESULT_IMAGE_INCOMPLETE         0x04
#define PLDM_FW_VERIFY_RESULT_TIMEOUT                  0x09
#define PLDM_FW_VERIFY_RESULT_GENERIC_ERROR            0x0a
#define PLDM_FW_VERIFY_RESULT_SECURITY_REVISION_ERROR  0x10
// VerifyComplete VerifyResult field vendor-defined error values
//   for NVIDIA codes, see PLDM_FW_NV_VERIFY_RESULT enum
#define PLDM_FW_VERIFY_RESULT_VENDOR_RANGE_MIN  0x90
#define PLDM_FW_VERIFY_RESULT_VENDOR_RANGE_MAX  0xaf

// ApplyComplete ApplyResult field DSP0267 spec-defined error values
#define PLDM_FW_APPLY_RESULT_SPEC_RANGE_MIN                 0x00
#define PLDM_FW_APPLY_RESULT_SPEC_RANGE_MAX                 0x1f
#define PLDM_FW_APPLY_RESULT_SUCCESS                        0x00
#define PLDM_FW_APPLY_RESULT_SUCCESS_NEW_ACTIVATION         0x01
#define PLDM_FW_APPLY_RESULT_MEMORY_WRITE_ERROR             0x02
#define PLDM_FW_APPLY_RESULT_TIMEOUT                        0x09
#define PLDM_FW_APPLY_RESULT_GENERIC_ERROR                  0x0a
#define PLDM_FW_APPLY_RESULT_FAILED_NEEDS_TRANSFER_RESTART  0x0b
#define PLDM_FW_APPLY_RESULT_SECURITY_REVISION_ERROR        0x10
// ApplyComplete ApplyResult field vendor-defined error values
//   for NVIDIA codes, see PLDM_FW_NV_APPLY_RESULT enum
#define PLDM_FW_APPLY_RESULT_VENDOR_RANGE_MIN  0xb0
#define PLDM_FW_APPLY_RESULT_VENDOR_RANGE_MAX  0xcf

#pragma pack(1)

typedef struct {
  UINT16    Type;
  UINT16    Length;
  UINT8     Data[1];
} PLDM_FW_DESCRIPTOR;

typedef struct {
  UINT16    Type;
  UINT16    Length;
  UINT32    Id;
} PLDM_FW_DESCRIPTOR_IANA_ID;

typedef MCTP_PLDM_REQUEST_HEADER PLDM_FW_QUERY_DEVICE_IDS_REQUEST;

typedef struct {
  MCTP_PLDM_COMMON      Common;
  UINT8                 CompletionCode;
  UINT32                Length;
  UINT8                 Count;
  PLDM_FW_DESCRIPTOR    Descriptors[1];
} PLDM_FW_QUERY_DEVICE_IDS_RESPONSE;

typedef MCTP_PLDM_REQUEST_HEADER PLDM_FW_GET_FW_PARAMS_REQUEST;

typedef struct {
  UINT16    Classification;
  UINT16    Id;
  UINT8     ClassificationIndex;
  UINT32    ActiveComparisonStamp;
  UINT8     ActiveVersionStringType;
  UINT8     ActiveVersionStringLength;
  CHAR8     ActiveReleaseDate[8];
  UINT32    PendingComparisonStamp;
  UINT8     PendingVersionStringType;
  UINT8     PendingVersionStringLength;
  CHAR8     PendingReleaseDate[8];
  UINT16    ActivationMethods;
  UINT32    CapabilitiesDuringUpdate;
  UINT8     ActiveVersionString[1];
} PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               CompletionCode;
  UINT32              CapabilitiesDuringUpdate;
  UINT16              ComponentCount;
  UINT8               ImageSetActiveVersionStringType;
  UINT8               ImageSetActiveVersionStringLength;
  UINT8               ImageSetPendingVersionStringType;
  UINT8               ImageSetPendingVersionStringLength;
  UINT8               ImageSetActiveVersionString[1];
} PLDM_FW_GET_FW_PARAMS_RESPONSE;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT32              MaxTransferSize;
  UINT16              NumComponents;
  UINT8               MaxOutstandingTransferReqs;
  UINT16              PackageDataLength;
  UINT8               ComponentImageSetVersionStringType;
  UINT8               ComponentImageSetVersionStringLength;
  UINT8               ComponentImageSetVersionString[1];
} PLDM_FW_REQUEST_UPDATE_REQUEST;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               CompletionCode;
  UINT16              FirmwareDeviceMetaDataLength;
  UINT8               FDWillSendGetPackageDataCommand;
} PLDM_FW_REQUEST_UPDATE_RESPONSE;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               TransferFlag;
  UINT16              ComponentClassification;
  UINT16              ComponentId;
  UINT8               ComponentClassificationIndex;
  UINT32              ComponentComparisonStamp;
  UINT8               ComponentVersionStringType;
  UINT8               ComponentVersionStringLength;
  UINT8               ComponentVersionString[1];
} PLDM_FW_PASS_COMPONENT_TABLE_REQUEST;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               CompletionCode;
  UINT8               ComponentResponse;
  UINT8               ComponentResponseCode;
} PLDM_FW_PASS_COMPONENT_TABLE_RESPONSE;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT16              ComponentClassification;
  UINT16              ComponentId;
  UINT8               ComponentClassificationIndex;
  UINT32              ComponentComparisonStamp;
  UINT32              ComponentImageSize;
  UINT32              UpdateOptionFlags;
  UINT8               ComponentVersionStringType;
  UINT8               ComponentVersionStringLength;
  UINT8               ComponentVersionString[1];
} PLDM_FW_UPDATE_COMPONENT_REQUEST;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               CompletionCode;
  UINT8               ComponentCompatibilityResponse;
  UINT8               ComponentCompatibilityResponseCode;
  UINT32              UpdateOptionFlagsEnabled;
  UINT16              TimeBeforeRequestFwData;
} PLDM_FW_UPDATE_COMPONENT_RESPONSE;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT32              Offset;
  UINT32              Length;
} PLDM_FW_REQUEST_FW_DATA_REQUEST;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               CompletionCode;
  UINT32              ImageData[1];
} PLDM_FW_REQUEST_FW_DATA_RESPONSE;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               TransferResult;
} PLDM_FW_TRANSFER_COMPLETE_REQUEST;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               CompletionCode;
} PLDM_FW_TRANSFER_COMPLETE_RESPONSE;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               VerifyResult;
} PLDM_FW_VERIFY_COMPLETE_REQUEST;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               CompletionCode;
} PLDM_FW_VERIFY_COMPLETE_RESPONSE;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               ApplyResult;
  UINT16              ComponentActivationMethodsModification;
} PLDM_FW_APPLY_COMPLETE_REQUEST;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               CompletionCode;
} PLDM_FW_APPLY_COMPLETE_RESPONSE;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               SelfContainedActivationRequest;
} PLDM_FW_ACTIVATE_FW_REQUEST;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               CompletionCode;
  UINT16              EstimatedTimeForSelfContainedActivation;
} PLDM_FW_ACTIVATE_FW_RESPONSE;

typedef MCTP_PLDM_REQUEST_HEADER  PLDM_FW_CANCEL_UPDATE_COMPONENT_REQUEST;
typedef MCTP_PLDM_RESPONSE_HEADER PLDM_FW_CANCEL_UPDATE_COMPONENT_RESPONSE;

typedef MCTP_PLDM_REQUEST_HEADER PLDM_FW_CANCEL_UPDATE_REQUEST;

typedef struct {
  MCTP_PLDM_COMMON    Common;
  UINT8               CompletionCode;
  UINT8               NonFunctioningComponentIndication;
  UINT64              NonFunctioningComponentBitmap;
} PLDM_FW_CANCEL_UPDATE_RESPONSE;

#pragma pack()

/**
  Fill common fields in PLDM FW request payload

  @param[in]  Common        Pointer to PLDM common header structure.
  @param[in]  IsRequest     TRUE to build request header.
  @param[in]  InstanceId    InstanceId for header.
  @param[in]  Command       PLDM FW command code for this header.

  @retval None

**/
VOID
EFIAPI
PldmFwFillCommon (
  IN  MCTP_PLDM_COMMON  *Common,
  IN  BOOLEAN           IsRequest,
  IN  UINT8             InstanceId,
  IN  UINT8             Command
  );

/**
  Check PLDM FW response completion code

  @param[in]  RspBuffer     Pointer to PLDM response message buffer.
  @param[in]  Function      Caller's function name for error messages.
  @param[in]  DeviceName    Device name where message was received.

  @retval EFI_SUCCESS       Operation completed normally.
  @retval Others            Failure occurred.

**/
EFI_STATUS
EFIAPI
PldmFwCheckRspCompletion (
  IN CONST VOID    *RspBuffer,
  IN CONST CHAR8   *Function,
  IN CONST CHAR16  *DeviceName
  );

/**
  Check PLDM FW response completion code and length.

  @param[in]  RspBuffer         Pointer to PLDM response message buffer.
  @param[in]  RspLength         Response message length.
  @param[in]  RspLengthExpected Expected response message length.
  @param[in]  Function          Caller's function name for error messages.
  @param[in]  DeviceName        Device name where message was received.

  @retval EFI_SUCCESS           Operation completed normally.
  @retval Others                Failure occurred.

**/
EFI_STATUS
EFIAPI
PldmFwCheckRspCompletionAndLength (
  IN CONST VOID    *RspBuffer,
  IN UINTN         RspLength,
  IN UINTN         RspLengthExpected,
  IN CONST CHAR8   *Function,
  IN CONST CHAR16  *DeviceName
  );

/**
  Check Get FW Params response payload for errors.

  @param[in]  Rsp                   Pointer to Get FW Params response.
  @param[in]  RspLength             Length of response message.
  @param[in]  DeviceName            Device name that sent response.

  @retval EFI_SUCCESS               No errors found.
  @retval Others                    Error detected.

**/
EFI_STATUS
EFIAPI
PldmFwGetFwParamsCheckRsp (
  IN CONST PLDM_FW_GET_FW_PARAMS_RESPONSE  *Rsp,
  IN UINTN                                 RspLength,
  IN CONST CHAR16                          *DeviceName
  );

/**
  Get offset of FW parameters component table in Get FW Params response.

  @param[in]  GetFwParamsRsp        Pointer to Get FW Params response buffer.

  @retval UINTN                     Offset of component table.

**/
UINTN
EFIAPI
PldmFwGetFwParamsComponentTableOffset (
  IN CONST PLDM_FW_GET_FW_PARAMS_RESPONSE  *GetFwParamsRsp
  );

/**
  Get FW parameters component table entry by index.

  @param[in]  GetFwParamsResponse   Pointer to Get FW Params response buffer.
  @param[in]  ComponentIndex        Index of desired component table entry.

  @retval PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY*  Pointer to table entry.

**/
CONST PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY *
EFIAPI
PldmFwGetFwParamsComponent (
  IN CONST PLDM_FW_GET_FW_PARAMS_RESPONSE  *GetFwParamsResponse,
  IN UINTN                                 ComponentIndex
  );

/**
  Print component table entry

  @param[in]  ComponentEntry        Pointer to component table entry.

  @retval None

**/
VOID
EFIAPI
PldmFwPrintComponentEntry (
  IN CONST PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY  *ComponentEntry
  );

/**
  Get next FW descriptor.

  @param[in]  Desc                  Pointer to current FW descriptor.

  @retval PLDM_FW_DESCRIPTOR*       Pointer to next FW descriptor.

**/
CONST PLDM_FW_DESCRIPTOR *
EFIAPI
PldmFwDescNext (
  IN CONST PLDM_FW_DESCRIPTOR  *Desc
  );

/**
  Print FW descriptor.

  @param[in]  Desc                  Pointer to FW descriptor.

  @retval None

**/
VOID
EFIAPI
PldmFwPrintFwDesc (
  IN CONST PLDM_FW_DESCRIPTOR  *Desc
  );

/**
  Print Query Device Ids descriptors.

  @param[in]  QueryDeviceIdsRsp     Pointer to Query Device Ids response.

  @retval None

**/
VOID
EFIAPI
PldmFwPrintQueryDeviceIdsDescriptors (
  IN CONST PLDM_FW_QUERY_DEVICE_IDS_RESPONSE  *QueryDeviceIdsRsp
  );

/**
  Check Query Device Ids response payload for errors.

  @param[in]  Rsp                   Pointer to Query Device Ids response.
  @param[in]  RspLength             Length of response message.
  @param[in]  DeviceName            Device name that sent response.

  @retval EFI_SUCCESS               No errors found.
  @retval Others                    Error detected.

**/
EFI_STATUS
EFIAPI
PldmFwQueryDeviceIdsCheckRsp (
  IN CONST PLDM_FW_QUERY_DEVICE_IDS_RESPONSE  *Rsp,
  IN UINTN                                    RspLength,
  IN CONST CHAR16                             *DeviceName
  );

/**
  Print Query Device Ids response.

  @param[in]  Rsp                   Pointer to Query Device Ids response.
  @param[in]  DeviceName            Device name that sent response.

  @retval None

**/
VOID
EFIAPI
PldmFwPrintQueryDeviceIdsRsp (
  IN CONST PLDM_FW_QUERY_DEVICE_IDS_RESPONSE  *Rsp,
  IN CONST CHAR16                             *DeviceName
  );

/**
  Check if descriptor is in list.

  @param[in]  Descriptor                FW Descriptor.
  @param[in]  List                      List of descriptors to check.

  @retval BOOLEAN                       TRUE if descriptor is in list.

**/
BOOLEAN
EFIAPI
PldmFwDescriptorIsInList (
  IN CONST PLDM_FW_DESCRIPTOR  *Descriptor,
  IN CONST PLDM_FW_DESCRIPTOR  *List,
  UINTN                        Count
  );

/**
  Get next matching component in FW params component table.

  @param[in]      GetFwParamsRsp           Pointer to Get FW Params response
  @param[in][out] FwParamsComponentIndex   Pointer to FW Params Component Index
  @param[in]      Classification           Component classification.
  @param[in]      Id                       Component ID.

  @retval PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY * Next matching component or
                                                    NULL if not found.

**/
CONST PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY *
EFIAPI
PldmFwGetNextFwParamsMatchingComponent (
  IN CONST PLDM_FW_GET_FW_PARAMS_RESPONSE  *GetFwParamsRsp,
  IN OUT UINTN                             *FwParamsComponentIndex,
  IN UINT16                                Classification,
  IN UINT16                                Id
  );

#endif
