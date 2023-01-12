/** @file

  FMP erot support functions

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <LastAttemptStatus.h>
#include <Guid/SystemResourceTable.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/ErotLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PldmFwUpdateLib.h>
#include <Library/PldmFwUpdatePkgLib.h>
#include <Library/PldmFwUpdateTaskLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include "FmpErotSupport.h"

#define FMP_EROT_SOCKET                        0
#define FMP_EROT_SYSTEM_FW_DEVICE_NAME         "GLACIERDSD"
#define FMP_EROT_SYSTEM_FW_DEVICE_NAME_LENGTH  10
#define FMP_EROT_NVIDIA_IANA_ID                0x1647UL
#define FMP_EROT_QUERY_DEVICE_IDS_RSP_SIZE     128
#define FMP_EROT_GET_FW_PARAMS_RSP_SIZE        256

// last attempt status error codes
enum {
  LAS_ERROR_BAD_IMAGE_POINTER = LAST_ATTEMPT_STATUS_DEVICE_LIBRARY_MIN_ERROR_CODE_VALUE,
  LAS_ERROR_FMP_LIB_UNINITIALIZED,
  LAS_ERROR_INVALID_PACKAGE_HEADER,
  LAS_ERROR_UNSUPPORTED_PACKAGE_TYPE,
  LAS_ERROR_TASK_LIB_INIT_FAILED,
  LAS_ERROR_TASK_CREATE_FAILED,

  LAS_ERROR_PLDM_FW_UPDATE_TASK_ERROR_START,
  LAS_ERROR_MAX = LAS_ERROR_PLDM_FW_UPDATE_TASK_ERROR_START + PLDM_FW_UPDATE_TASK_ERROR_MAX
};

#pragma pack(1)
typedef struct {
  UINT16    Type;
  UINT16    Length;
  UINT8     NameType;
  UINT8     NameLength;
  CHAR8     Name[FMP_EROT_SYSTEM_FW_DEVICE_NAME_LENGTH];
  UINT8     StrapId;
} FMP_EROT_SYSTEM_FW_DESCRIPTOR;
#pragma pack()

STATIC BOOLEAN     mInitialized    = FALSE;
STATIC EFI_STATUS  mVersionStatus  = EFI_UNSUPPORTED;
STATIC UINT32      mVersion        = 0;
STATIC CHAR16      *mVersionString = NULL;

STATIC UINT16                               mComponentId        = 0;
STATIC PLDM_FW_QUERY_DEVICE_IDS_RESPONSE    *mQueryDeviceIdsRsp = NULL;
STATIC PLDM_FW_GET_FW_PARAMS_RESPONSE       *mGetFwParamsRsp    = NULL;
STATIC CONST PLDM_FW_DESCRIPTOR_IANA_ID     mNvIanaIdDesc       = {
  PLDM_FW_DESCRIPTOR_TYPE_IANA_ENTERPRISE,
  sizeof (UINT32),
  FMP_EROT_NVIDIA_IANA_ID
};
STATIC CONST FMP_EROT_SYSTEM_FW_DESCRIPTOR  mSystemFwDesc = {
  PLDM_FW_DESCRIPTOR_TYPE_VENDOR,
  sizeof (FMP_EROT_SYSTEM_FW_DESCRIPTOR) - OFFSET_OF (PLDM_FW_DESCRIPTOR,Data),
  PLDM_FW_STRING_TYPE_ASCII,
  FMP_EROT_SYSTEM_FW_DEVICE_NAME_LENGTH,
  FMP_EROT_SYSTEM_FW_DEVICE_NAME
};

EFI_STATUS
EFIAPI
FmpErotGetVersion (
  OUT UINT32 *Version, OPTIONAL
  OUT CHAR16  **VersionString   OPTIONAL
  )
{
  UINTN  VersionStringSize;

  if (!mInitialized) {
    return EFI_UNSUPPORTED;
  }

  if (EFI_ERROR (mVersionStatus)) {
    DEBUG ((DEBUG_ERROR, "%a: bad status: %r\n", __FUNCTION__, mVersionStatus));
    return mVersionStatus;
  }

  if (Version != NULL) {
    *Version = mVersion;
  }

  if (VersionString != NULL) {
    // version string must be in allocated pool memory that caller frees
    VersionStringSize = StrSize (mVersionString) * sizeof (CHAR16);
    *VersionString    = (CHAR16 *)AllocateRuntimeCopyPool (
                                    VersionStringSize,
                                    mVersionString
                                    );
    if (*VersionString == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  DEBUG ((DEBUG_INFO, "%a: version 0x%08x (%s)\n", __FUNCTION__, mVersion, mVersionString));

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FmpErotCheckImage (
  IN  CONST VOID  *Image,
  IN  UINTN       ImageSize,
  OUT UINT32      *ImageUpdatable,
  OUT UINT32      *LastAttemptStatus
  )
{
  CONST PLDM_FW_PKG_HDR               *Hdr;
  EFI_STATUS                          Status;
  CONST PLDM_FW_PKG_DEVICE_ID_RECORD  *DeviceIdRecord;

  if ((ImageUpdatable == NULL) || (LastAttemptStatus == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Image == NULL) {
    *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_BAD_IMAGE_POINTER;
    return EFI_INVALID_PARAMETER;
  }

  if (!mInitialized) {
    *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_FMP_LIB_UNINITIALIZED;
    return EFI_NOT_READY;
  }

  Hdr    = (CONST PLDM_FW_PKG_HDR *)Image;
  Status = PldmFwPkgHdrValidate (Hdr, ImageSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "PkgHdr validation failed: %r\n", Status));
    *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_INVALID_PACKAGE_HEADER;
    return EFI_ABORTED;
  }

  if (!PldmFwPkgMatchesFD (
         Hdr,
         mQueryDeviceIdsRsp->Count,
         mQueryDeviceIdsRsp->Descriptors,
         &DeviceIdRecord
         ))
  {
    DEBUG ((DEBUG_ERROR, "%a: FD not in pkg\n", __FUNCTION__));
    *ImageUpdatable    = IMAGE_UPDATABLE_INVALID;
    *LastAttemptStatus = LAS_ERROR_UNSUPPORTED_PACKAGE_TYPE;
    return EFI_ABORTED;
  }

  *LastAttemptStatus = LAST_ATTEMPT_STATUS_SUCCESS;
  *ImageUpdatable    = IMAGE_UPDATABLE_VALID;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FmpErotSetImage (
  IN  CONST VOID *Image,
  IN  UINTN ImageSize,
  IN  CONST VOID *VendorCode, OPTIONAL
  IN  EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS  Progress, OPTIONAL
  IN  UINT32                                         CapsuleFwVersion,
  OUT CHAR16                                         **AbortReason,
  OUT UINT32                                         *LastAttemptStatus
  )
{
  EFI_STATUS                 Status;
  UINTN                      Index;
  NVIDIA_MCTP_PROTOCOL       *Erot;
  UINTN                      NumErots;
  BOOLEAN                    Failed;
  CONST PLDM_FW_PKG_HDR      *Hdr;
  PLDM_FW_UPDATE_TASK_ERROR  Error;
  UINT16                     ActivationMethod;

  if (LastAttemptStatus == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Image == NULL) {
    *LastAttemptStatus = LAS_ERROR_BAD_IMAGE_POINTER;
    return EFI_INVALID_PARAMETER;
  }

  if (!mInitialized) {
    *LastAttemptStatus = LAS_ERROR_FMP_LIB_UNINITIALIZED;
    return EFI_NOT_READY;
  }

  Hdr      = (CONST PLDM_FW_PKG_HDR *)Image;
  Failed   = FALSE;
  NumErots = ErotGetNumErots ();

  Status = PldmFwUpdateTaskLibInit (NumErots);
  if (EFI_ERROR (Status)) {
    *LastAttemptStatus = LAS_ERROR_TASK_LIB_INIT_FAILED;
    return EFI_ABORTED;
  }

  for (Index = 0; Index < NumErots; Index++) {
    Erot   = ErotGetMctpProtocolByIndex (Index);
    Status = PldmFwUpdateTaskCreate (Erot, Image, ImageSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: FW update %u failed: %r\n", __FUNCTION__, Index, Status));
      *LastAttemptStatus = LAS_ERROR_TASK_CREATE_FAILED;
      return EFI_ABORTED;
    }
  }

  Status = PldmFwUpdateTaskExecuteAll (&Error, &ActivationMethod);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FW update execute failed err=0x%x: %r\n", __FUNCTION__, Error, Status));

    *LastAttemptStatus = LAS_ERROR_PLDM_FW_UPDATE_TASK_ERROR_START + Error;
    return EFI_ABORTED;
  }

  *LastAttemptStatus = LAST_ATTEMPT_STATUS_SUCCESS;
  DEBUG ((DEBUG_INFO, "%a: exit success\n", __FUNCTION__));

  if (ActivationMethod >= PLDM_FW_ACTIVATION_DC_POWER_CYCLE) {
    Print (L"\nPower cycle required to activate new firmware.\n");
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
FmpErotQueryDeviceIds (
  NVIDIA_MCTP_PROTOCOL          *Protocol,
  CONST MCTP_DEVICE_ATTRIBUTES  *Attributes
  )
{
  PLDM_FW_QUERY_DEVICE_IDS_REQUEST  QueryDeviceIdsReq;
  UINTN                             RspLength;
  EFI_STATUS                        Status;

  mQueryDeviceIdsRsp = (PLDM_FW_QUERY_DEVICE_IDS_RESPONSE *)
                       AllocateRuntimePool (FMP_EROT_QUERY_DEVICE_IDS_RSP_SIZE);
  if (mQueryDeviceIdsRsp == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: rsp alloc failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  PldmFwFillCommon (
    &QueryDeviceIdsReq.Common,
    TRUE,
    0,
    PLDM_FW_QUERY_DEVICE_IDS
    );
  Status = Protocol->DoRequest (
                       Protocol,
                       &QueryDeviceIdsReq,
                       sizeof (QueryDeviceIdsReq),
                       mQueryDeviceIdsRsp,
                       FMP_EROT_QUERY_DEVICE_IDS_RSP_SIZE,
                       &RspLength
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s QDI req failed: %r\n", __FUNCTION__, Attributes->DeviceName, Status));
    return Status;
  }

  Status = PldmFwQueryDeviceIdsCheckRsp (mQueryDeviceIdsRsp, RspLength, Attributes->DeviceName);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
FmpErotGetFwParams (
  NVIDIA_MCTP_PROTOCOL          *Protocol,
  CONST MCTP_DEVICE_ATTRIBUTES  *Attributes
  )
{
  EFI_STATUS                     Status;
  PLDM_FW_GET_FW_PARAMS_REQUEST  GetFwParamsReq;
  UINTN                          RspLength;

  mGetFwParamsRsp = (PLDM_FW_GET_FW_PARAMS_RESPONSE *)
                    AllocateRuntimePool (FMP_EROT_GET_FW_PARAMS_RSP_SIZE);
  if (mGetFwParamsRsp == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: rsp alloc failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  PldmFwFillCommon (
    &GetFwParamsReq.Common,
    TRUE,
    1,
    PLDM_FW_GET_FW_PARAMS
    );
  Status = Protocol->DoRequest (
                       Protocol,
                       &GetFwParamsReq,
                       sizeof (GetFwParamsReq),
                       mGetFwParamsRsp,
                       FMP_EROT_GET_FW_PARAMS_RSP_SIZE,
                       &RspLength
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s GFP req failed: %r\n", __FUNCTION__, Attributes->DeviceName, Status));
    return Status;
  }

  Status = PldmFwGetFwParamsCheckRsp (mGetFwParamsRsp, RspLength, Attributes->DeviceName);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

/**
  Get system firmware version info from erot.

  @retval EFI_SUCCESS               No errors found.
  @retval Others                    Error detected.

**/
STATIC
EFI_STATUS
EFIAPI
FmpErotGetVersionInfo (
  VOID
  )
{
  NVIDIA_MCTP_PROTOCOL                           *Protocol;
  MCTP_DEVICE_ATTRIBUTES                         Attributes;
  EFI_STATUS                                     Status;
  CONST PLDM_FW_DESCRIPTOR                       *Desc;
  UINTN                                          Index;
  UINTN                                          VersionStrLen;
  UINT64                                         Version64;
  CONST PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY  *ComponentEntry;

  Protocol = ErotGetMctpProtocolBySocket (FMP_EROT_SOCKET);
  if (Protocol == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: no protocol\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  Status = Protocol->GetDeviceAttributes (Protocol, &Attributes);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: no attr\n", __FUNCTION__));
    return Status;
  }

  Status = FmpErotQueryDeviceIds (Protocol, &Attributes);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // initial descriptor must be NVIDIA IANA ID
  Desc = mQueryDeviceIdsRsp->Descriptors;
  PldmFwPrintFwDesc (Desc);
  if (CompareMem (Desc, &mNvIanaIdDesc, sizeof (mNvIanaIdDesc)) != 0) {
    DEBUG ((DEBUG_ERROR, "%a: invalid initial desc, t=0x%x l=%u id=0x%x\n", __FUNCTION__, Desc->Type, Desc->Length, *(UINT32 *)Desc->Data));
    return EFI_DEVICE_ERROR;
  }

  // find system firmware descriptor, get component id
  Desc = PldmFwDescNext (Desc);
  for (Index = 1; Index < mQueryDeviceIdsRsp->Count; Index++) {
    PldmFwPrintFwDesc (Desc);
    if (CompareMem (Desc, &mSystemFwDesc, OFFSET_OF (FMP_EROT_SYSTEM_FW_DESCRIPTOR, StrapId)) == 0) {
      mComponentId = ((CONST FMP_EROT_SYSTEM_FW_DESCRIPTOR *)Desc)->StrapId;
      break;
    }

    Desc = PldmFwDescNext (Desc);
  }

  if (Index == mQueryDeviceIdsRsp->Count) {
    DEBUG ((DEBUG_ERROR, "%a: FD %a not found\n", __FUNCTION__, FMP_EROT_SYSTEM_FW_DEVICE_NAME));
    return EFI_NOT_FOUND;
  }

  DEBUG ((DEBUG_INFO, "%a: FD ComponentId=0x%x\n", __FUNCTION__, mComponentId));

  // find component FW params
  Status = FmpErotGetFwParams (Protocol, &Attributes);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < mGetFwParamsRsp->ComponentCount; Index++) {
    ComponentEntry = PldmFwGetFwParamsComponent (mGetFwParamsRsp, Index);
    if (ComponentEntry->Id == mComponentId) {
      break;
    }
  }

  if (Index == mGetFwParamsRsp->ComponentCount) {
    DEBUG ((DEBUG_ERROR, "%a: ComponentId=0x%x not found in %u entries\n", __FUNCTION__, mComponentId, mGetFwParamsRsp->ComponentCount));
    return EFI_NOT_FOUND;
  }

  if (ComponentEntry->ActiveVersionStringType != PLDM_FW_STRING_TYPE_ASCII) {
    DEBUG ((DEBUG_ERROR, "%a: bad str type=%u\n", __FUNCTION__, ComponentEntry->ActiveVersionStringType));
    return EFI_UNSUPPORTED;
  }

  // allocate unicode buffer and convert ascii version
  VersionStrLen  = (ComponentEntry->ActiveVersionStringLength + 1) * sizeof (CHAR16);
  mVersionString = (CHAR16 *)AllocateRuntimePool (VersionStrLen);
  if (mVersionString == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: string alloc failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  UnicodeSPrintAsciiFormat (
    mVersionString,
    VersionStrLen,
    "%.*a",
    ComponentEntry->ActiveVersionStringLength,
    ComponentEntry->ActiveVersionString
    );

  // erot only returns version string, convert it to 4-byte hex version value
  Status = StrHexToUint64S (mVersionString, NULL, &Version64);
  if (EFI_ERROR (Status) || (Version64 > MAX_UINT32)) {
    DEBUG ((DEBUG_ERROR, "%a: error converting %s 0x%llx: %r\n", __FUNCTION__, mVersionString, Version64, Status));
    return EFI_UNSUPPORTED;
  }

  mVersion       = (UINT32)Version64;
  mVersionStatus = EFI_SUCCESS;

  DEBUG ((
    DEBUG_INFO,
    "%a: got version=0x%x (%s) Pending=%.*a\n",
    __FUNCTION__,
    mVersion,
    mVersionString,
    ComponentEntry->PendingVersionStringLength,
    &ComponentEntry->ActiveVersionString[ComponentEntry->ActiveVersionStringLength]
    ));

  return EFI_SUCCESS;
}

/**
  FmpErotLib constructor.

  @param[in]  ImageHandle       Image handle
  @param[in]  SystemTable       Pointer to system table

  @retval EFI_SUCCESS           Initialization successful
  @retval others                Error occurred

**/
EFI_STATUS
EFIAPI
FmpErotLibConstructor (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = ErotLibInit ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: lib init error: %r\n", __FUNCTION__, Status));
    goto Done;
  }

  Status = FmpErotGetVersionInfo ();
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  mInitialized = TRUE;

Done:
  // must exit with good status, API disabled if errors occurred above
  if (EFI_ERROR (Status)) {
    if (mQueryDeviceIdsRsp != NULL) {
      FreePool (mQueryDeviceIdsRsp);
      mQueryDeviceIdsRsp = NULL;
    }

    if (mGetFwParamsRsp != NULL) {
      FreePool (mGetFwParamsRsp);
      mGetFwParamsRsp = NULL;
    }

    if (mVersionString != NULL) {
      FreePool (mVersionString);
      mVersionString = NULL;
    }

    ErotLibDeinit ();
  }

  return EFI_SUCCESS;
}
