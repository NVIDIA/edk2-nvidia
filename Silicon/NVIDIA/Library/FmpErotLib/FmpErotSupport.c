/** @file

  FMP erot support functions

  Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/ErotLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PldmFwUpdateLib.h>
#include <Library/PrintLib.h>
#include "FmpErotLibPrivate.h"

#define FMP_EROT_SOCKET          0
#define FMP_EROT_FW_DEVICE_NAME  "GLACIERDSD"

#pragma pack(1)
typedef struct {
  UINT8    StrType;
  UINT8    StrLength;
  CHAR8    Str[1];
} FMP_EROT_VENDOR_FW_DESCRIPTOR;
#pragma pack()

STATIC BOOLEAN     mInitialized    = FALSE;
STATIC EFI_STATUS  mVersionStatus  = EFI_UNSUPPORTED;
STATIC UINT32      mVersion        = 0;
STATIC CHAR16      *mVersionString = NULL;

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
  PLDM_FW_QUERY_DEVICE_IDS_REQUEST               QueryDeviceIdsReq;
  PLDM_FW_QUERY_DEVICE_IDS_RESPONSE              *QueryDeviceIdsRsp;
  PLDM_FW_GET_FW_PARAMS_REQUEST                  GetFwParamsReq;
  PLDM_FW_GET_FW_PARAMS_RESPONSE                 *GetFwParamsRsp;
  UINT8                                          RspBuffer[1024];
  UINTN                                          RspLength;
  EFI_STATUS                                     Status;
  UINT8                                          InstanceId;
  UINT16                                         ComponentId;
  CONST PLDM_FW_DESCRIPTOR                       *Desc;
  UINTN                                          Index;
  CONST FMP_EROT_VENDOR_FW_DESCRIPTOR            *VendorDesc;
  CONST PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY  *ComponentEntry;
  UINTN                                          VersionStrLen;
  UINT64                                         Version64;

  Protocol = ErotGetMctpProtocolBySocket (FMP_EROT_SOCKET);
  if (Protocol == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: no protocol\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  Status = Protocol->GetDeviceAttributes (Protocol, &Attributes);
  ASSERT_EFI_ERROR (Status);

  InstanceId = 0;
  PldmFwFillCommon (
    &QueryDeviceIdsReq.Common,
    TRUE,
    InstanceId++,
    PLDM_FW_QUERY_DEVICE_IDS
    );
  Status = Protocol->DoRequest (
                       Protocol,
                       &QueryDeviceIdsReq,
                       sizeof (QueryDeviceIdsReq),
                       RspBuffer,
                       sizeof (RspBuffer),
                       &RspLength
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s QDI req failed: %r\n", __FUNCTION__, Attributes.DeviceName, Status));
    return Status;
  }

  QueryDeviceIdsRsp = (PLDM_FW_QUERY_DEVICE_IDS_RESPONSE *)RspBuffer;
  Status            = PldmFwQueryDeviceIdsCheckRsp (QueryDeviceIdsRsp, RspLength, Attributes.DeviceName);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Desc = QueryDeviceIdsRsp->Descriptors;
  PldmFwPrintFwDesc (Desc);
  if ((QueryDeviceIdsRsp->Count < 1) ||
      (Desc->Type != PLDM_FW_DESCRIPTOR_TYPE_IANA_ENTERPRISE) ||
      (Desc->Length != sizeof (UINT32)) ||
      (*(UINT32 *)(Desc->Data) != FMP_EROT_NVIDIA_IANA_ID))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: invalid initial desc, t=0x%x l=%u id=0x%x\n",
      __FUNCTION__,
      Desc->Type,
      Desc->Length,
      *(UINT32 *)(Desc->Data)
      ));
    return EFI_DEVICE_ERROR;
  }

  // find device id for system firmware
  Desc = PldmFwDescNext (Desc);
  for (Index = 1; Index < QueryDeviceIdsRsp->Count; Index++) {
    PldmFwPrintFwDesc (Desc);
    if (Desc->Type == PLDM_FW_DESCRIPTOR_TYPE_VENDOR) {
      VendorDesc = (CONST FMP_EROT_VENDOR_FW_DESCRIPTOR *)Desc->Data;
      if ((VendorDesc->StrType != PLDM_FW_STRING_TYPE_ASCII) ||
          (VendorDesc->StrLength != AsciiStrLen (FMP_EROT_FW_DEVICE_NAME)) ||
          (AsciiStrnCmp (VendorDesc->Str, FMP_EROT_FW_DEVICE_NAME, VendorDesc->StrLength) != 0) ||
          (Desc->Length != VendorDesc->StrLength + OFFSET_OF (FMP_EROT_VENDOR_FW_DESCRIPTOR, Str) + 1))
      {
        continue;
      }

      // Last byte of descriptor data is strap id/component id
      ComponentId = Desc->Data[Desc->Length - 1];
      break;
    }

    Desc = PldmFwDescNext (Desc);
  }

  if (Index == QueryDeviceIdsRsp->Count) {
    DEBUG ((DEBUG_ERROR, "%a: FD %a not found\n", __FUNCTION__, FMP_EROT_FW_DEVICE_NAME));
    return EFI_NOT_FOUND;
  }

  DEBUG ((DEBUG_INFO, "%a: FD ComponentId=0x%x\n", __FUNCTION__, ComponentId));

  PldmFwFillCommon (
    &GetFwParamsReq.Common,
    TRUE,
    InstanceId++,
    PLDM_FW_GET_FW_PARAMS
    );
  Status = Protocol->DoRequest (
                       Protocol,
                       &GetFwParamsReq,
                       sizeof (GetFwParamsReq),
                       RspBuffer,
                       sizeof (RspBuffer),
                       &RspLength
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s GFP req failed: %r\n", __FUNCTION__, Attributes.DeviceName, Status));
    return Status;
  }

  GetFwParamsRsp = (PLDM_FW_GET_FW_PARAMS_RESPONSE *)RspBuffer;
  Status         = PldmFwGetFwParamsCheckRsp (GetFwParamsRsp, RspLength, Attributes.DeviceName);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < GetFwParamsRsp->ComponentCount; Index++) {
    ComponentEntry = PldmFwGetFwParamsComponent (GetFwParamsRsp, Index);
    if ((ComponentEntry->Id == ComponentId) &&
        (ComponentEntry->ActiveVersionStringType == PLDM_FW_STRING_TYPE_ASCII))
    {
      break;
    }
  }

  if (Index == GetFwParamsRsp->ComponentCount) {
    DEBUG ((DEBUG_ERROR, "%a: ComponentId=0x%x not found in %u entries\n", __FUNCTION__, ComponentId, GetFwParamsRsp->ComponentCount));
    return EFI_NOT_FOUND;
  }

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
    FreePool (mVersionString);
    return Status;
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
    ErotLibDeinit ();
  }

  return EFI_SUCCESS;
}
