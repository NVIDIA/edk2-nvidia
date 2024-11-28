/** @file

  FMP erot support functions

  SPDX-FileCopyrightText: Copyright (c) 2022 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <LastAttemptStatus.h>
#include <Guid/SystemResourceTable.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/ErotLib.h>
#include <Library/FmpDeviceLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PldmFwUpdateLib.h>
#include <Library/PldmFwUpdatePkgLib.h>
#include <Library/PldmFwUpdateTaskLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "FmpErotSupport.h"

#define FMP_EROT_SOCKET                     0
#define FMP_EROT_EC_FW_COMPONENT_ID         0xFF00
#define FMP_EROT_NVIDIA_IANA_ID             0x1647UL
#define FMP_EROT_QUERY_DEVICE_IDS_RSP_SIZE  128
#define FMP_EROT_GET_FW_PARAMS_RSP_SIZE     256

// last attempt status error codes
enum {
  LAS_ERROR_BAD_IMAGE_POINTER = LAST_ATTEMPT_STATUS_DEVICE_LIBRARY_MIN_ERROR_CODE_VALUE,
  LAS_ERROR_FMP_LIB_UNINITIALIZED,
  LAS_ERROR_INVALID_PACKAGE_HEADER,
  LAS_ERROR_UNSUPPORTED_PACKAGE_TYPE,
  LAS_ERROR_UNKNOWN_PACKAGE_FW_VERSION,
  LAS_ERROR_FW_VERSION_MISMATCH,
  LAS_ERROR_TASK_LIB_INIT_FAILED,
  LAS_ERROR_TASK_CREATE_FAILED,

  LAS_ERROR_PLDM_FW_UPDATE_TASK_ERROR_START,
  LAS_ERROR_MAX = LAS_ERROR_PLDM_FW_UPDATE_TASK_ERROR_START + PLDM_FW_UPDATE_TASK_ERROR_MAX
};

#pragma pack(1)

typedef struct {
  UINT16    Id;
  UINT16    Revision;
  UINT32    ImageOffset;
  UINT32    FlashOffset;
  UINT8     ApCfgKeyIdx;
  UINT8     ApFwImagesCount;
  UINT8     SecVersion;
  UINT8     ApStrap;
  UINT32    FwVersion;
  UINT16    BuildYear;
  UINT8     BuildDay;
  UINT8     BuildMonth;
} FMP_EROT_PKG_METADATA_HDR;

#pragma pack()

STATIC BOOLEAN     mInitialized     = FALSE;
STATIC EFI_STATUS  mVersionStatus   = EFI_UNSUPPORTED;
STATIC UINT32      mVersion         = 0;
STATIC CHAR16      *mVersionString  = NULL;
STATIC UINT32      mActiveBootChain = MAX_UINT32;
STATIC EFI_EVENT   mEndOfDxeEvent   = NULL;
STATIC EFI_HANDLE  mImageHandle     = NULL;

STATIC PLDM_FW_QUERY_DEVICE_IDS_RESPONSE  *mQueryDeviceIdsRsp = NULL;
STATIC PLDM_FW_GET_FW_PARAMS_RESPONSE     *mGetFwParamsRsp    = NULL;
STATIC CONST PLDM_FW_DESCRIPTOR_IANA_ID   mNvIanaIdDesc       = {
  PLDM_FW_DESCRIPTOR_TYPE_IANA_ENTERPRISE,
  sizeof (UINT32),
  FMP_EROT_NVIDIA_IANA_ID
};

FMP_DEVICE_LIB_REGISTER_FMP_INSTALLER  mInstaller = NULL;

EFI_STATUS
EFIAPI
UpdateImageProgress (
  IN  UINTN  Completion
  );

STATIC
EFI_STATUS
EFIAPI
FmpErotGetPkgMetadataFwVersion (
  CONST PLDM_FW_PKG_HDR  *Hdr,
  UINT32                 *FwVersion
  )
{
  FMP_EROT_PKG_METADATA_HDR  *PkgHdr;

  PkgHdr = (FMP_EROT_PKG_METADATA_HDR *)((UINT8 *)Hdr + Hdr->Size);

  DEBUG ((DEBUG_INFO, "%a: Package Rev=0x%x FwVer: 0x%x %u/%u/%u\n", __FUNCTION__, PkgHdr->Revision, PkgHdr->FwVersion, PkgHdr->BuildMonth, PkgHdr->BuildDay, PkgHdr->BuildYear));

  *FwVersion = PkgHdr->FwVersion;

  return EFI_SUCCESS;
}

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
  UINT32                     PkgFwVersion;

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

  UpdateImageProgress (0);

  Hdr      = (CONST PLDM_FW_PKG_HDR *)Image;
  Failed   = FALSE;
  NumErots = ErotGetNumErots ();

  Status = FmpErotGetPkgMetadataFwVersion (Hdr, &PkgFwVersion);
  if (EFI_ERROR (Status)) {
    *LastAttemptStatus = LAS_ERROR_UNKNOWN_PACKAGE_FW_VERSION;
    return EFI_ABORTED;
  }

  if (CapsuleFwVersion != PkgFwVersion) {
    DEBUG ((DEBUG_ERROR, "%a: FwVersion mismatch capsule=0x%x, pkg=0x%x\n", __FUNCTION__, CapsuleFwVersion, PkgFwVersion));
    *LastAttemptStatus = LAS_ERROR_FW_VERSION_MISMATCH;
    return EFI_ABORTED;
  }

  Status = PldmFwUpdateTaskLibInit (NumErots, UpdateImageProgress);
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
  CHAR16                                         ReleaseDate[9] = { L'\0' };
  BOOLEAN                                        ErotComponentFound;
  CONST PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY  *FwComponentEntry;
  CONST CHAR8                                    *ComponentVersionString;
  UINTN                                          ComponentVersionStringLength;
  CONST CHAR8                                    *ComponentReleaseDate;

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

  // find component FW params
  Status = FmpErotGetFwParams (Protocol, &Attributes);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (mGetFwParamsRsp->ComponentCount != 2) {
    DEBUG ((DEBUG_ERROR, "%a: Bad component count=%u\n", __FUNCTION__, mGetFwParamsRsp->ComponentCount));
    return EFI_UNSUPPORTED;
  }

  ErotComponentFound = FALSE;
  ComponentEntry     = NULL;
  for (Index = 0; Index < mGetFwParamsRsp->ComponentCount; Index++) {
    FwComponentEntry = PldmFwGetFwParamsComponent (mGetFwParamsRsp, Index);
    if (FwComponentEntry->Id == FMP_EROT_EC_FW_COMPONENT_ID) {
      ErotComponentFound = TRUE;
    } else {
      ComponentEntry = FwComponentEntry;
      DEBUG ((DEBUG_INFO, "%a: FD ComponentId=0x%x\n", __FUNCTION__, ComponentEntry->Id));
    }
  }

  if ((ErotComponentFound != TRUE) || (ComponentEntry == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Bad components erot=%u fw=0x%p\n", __FUNCTION__, ErotComponentFound, ComponentEntry));
    return EFI_UNSUPPORTED;
  }

  if ((ComponentEntry->ActiveVersionStringType != PLDM_FW_STRING_TYPE_ASCII) ||
      ((ComponentEntry->PendingVersionStringLength != 0) &&
       (ComponentEntry->PendingVersionStringType != PLDM_FW_STRING_TYPE_ASCII)))
  {
    DEBUG ((DEBUG_ERROR, "%a: bad str type=%u\n", __FUNCTION__, ComponentEntry->ActiveVersionStringType));
    return EFI_UNSUPPORTED;
  }

  // if booting chain 0, use pending version, if any, since we are booting it
  if ((ComponentEntry->PendingVersionStringLength != 0) &&
      (mActiveBootChain == 0))
  {
    // pending string follows active string
    ComponentVersionString       = (CHAR8 *)&ComponentEntry->ActiveVersionString[ComponentEntry->ActiveVersionStringLength];
    ComponentVersionStringLength = ComponentEntry->PendingVersionStringLength;
    ComponentReleaseDate         = ComponentEntry->PendingReleaseDate;
  } else {
    ComponentVersionString       = (CHAR8 *)ComponentEntry->ActiveVersionString;
    ComponentVersionStringLength = ComponentEntry->ActiveVersionStringLength;
    ComponentReleaseDate         = ComponentEntry->ActiveReleaseDate;
  }

  // allocate unicode buffer and convert ascii version
  VersionStrLen  = (ComponentVersionStringLength + 1) * sizeof (CHAR16);
  mVersionString = (CHAR16 *)AllocateRuntimePool (VersionStrLen);
  if (mVersionString == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: string alloc failed\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  UnicodeSPrintAsciiFormat (
    mVersionString,
    VersionStrLen,
    "%.*a",
    ComponentVersionStringLength,
    ComponentVersionString
    );
  Status = PcdSetPtrS (PcdFirmwareVersionString, &VersionStrLen, mVersionString);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to set version pcd to %s: %r\n", __FUNCTION__, mVersionString, Status));
  }

  // convert ascii release date
  VersionStrLen = sizeof (ReleaseDate);
  UnicodeSPrintAsciiFormat (
    ReleaseDate,
    VersionStrLen,
    "%.*a",
    sizeof (ComponentEntry->ActiveReleaseDate),
    ComponentReleaseDate
    );
  Status = PcdSetPtrS (PcdFirmwareReleaseDateString, &VersionStrLen, ReleaseDate);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to set date pcd to %s: %r\n", __FUNCTION__, ReleaseDate, Status));
  }

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
    "%a: got version=0x%x str=%s date=%s chain=%u\n",
    __FUNCTION__,
    mVersion,
    mVersionString,
    ReleaseDate,
    mActiveBootChain
    ));
  DEBUG ((
    DEBUG_INFO,
    "%a: Active=%.*a Pending=%.*a\n",
    __FUNCTION__,
    ComponentEntry->ActiveVersionStringLength,
    ComponentEntry->ActiveVersionString,
    ComponentEntry->PendingVersionStringLength,
    &ComponentEntry->ActiveVersionString[ComponentEntry->ActiveVersionStringLength]
    ));

  return EFI_SUCCESS;
}

/**
  Handle EndOfDxe event - install FMP protocol.

  @param[in]  Event         Event pointer.
  @param[in]  Context       Event notification context.

  @retval None

**/
STATIC
VOID
EFIAPI
FmpErotEndOfDxeNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;

  FmpParamLibInit ();

  Status = ErotLibInit ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: lib init error: %r\n", __FUNCTION__, Status));
    goto Done;
  }

  Status = FmpErotGetVersionInfo ();
  if (EFI_ERROR (Status)) {
    // Retry once if fails
    Status = FmpErotGetVersionInfo ();
    if (EFI_ERROR (Status)) {
      goto Done;
    }
  }

  if (mInstaller == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: installer not registered!\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto Done;
  }

  DEBUG ((DEBUG_INFO, "%a: installing FMP\n", __FUNCTION__));
  mInitialized = TRUE;
  Status       = mInstaller (mImageHandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: FMP installer failed: %r\n", __FUNCTION__, Status));
    mInitialized = FALSE;
  }

Done:
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

    // Install FMP protocol even on failure, library API is disabled
    if (mInstaller != NULL) {
      Status = mInstaller (mImageHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: no erot, FMP installer failed: %r\n", __FUNCTION__, Status));
      }
    }
  }
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
  EFI_STATUS                          Status;
  CONST TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;
  VOID                                *Hob;

  mImageHandle = ImageHandle;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
    mActiveBootChain     = PlatformResourceInfo->ActiveBootChain;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Error getting active boot chain\n", __FUNCTION__));
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  FmpErotEndOfDxeNotify,
                  NULL,
                  &gEfiEndOfDxeEventGroupGuid,
                  &mEndOfDxeEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error creating exit boot services event: %r\n", __FUNCTION__, Status));
    goto Done;
  }

Done:
  // must exit with good status, API disabled if errors occurred above
  if (EFI_ERROR (Status)) {
    if (mEndOfDxeEvent != NULL) {
      gBS->CloseEvent (mEndOfDxeEvent);
      mEndOfDxeEvent = NULL;
    }

    mImageHandle     = NULL;
    mActiveBootChain = MAX_UINT32;
  }

  return EFI_SUCCESS;
}
