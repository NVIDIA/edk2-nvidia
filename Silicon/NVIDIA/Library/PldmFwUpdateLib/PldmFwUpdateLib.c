/** @file

  PLDM FW update functions

  Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PldmFwUpdateLib.h>

VOID
EFIAPI
PldmFwFillCommon (
  IN  MCTP_PLDM_COMMON  *Common,
  IN  BOOLEAN           IsRequest,
  IN  UINT8             InstanceId,
  IN  UINT8             Command
  )
{
  PldmFillCommon (
    Common,
    IsRequest,
    InstanceId,
    PLDM_TYPE_FW_UPDATE,
    Command
    );
}

EFI_STATUS
EFIAPI
PldmFwCheckRspCompletion (
  IN CONST VOID    *RspBuffer,
  IN CONST CHAR8   *Function,
  IN CONST CHAR16  *DeviceName
  )
{
  CONST MCTP_PLDM_RESPONSE_HEADER  *Rsp;

  Rsp = (CONST MCTP_PLDM_RESPONSE_HEADER *)RspBuffer;

  if (Rsp->CompletionCode != PLDM_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: %s failed: 0x%x\n", Function, DeviceName, Rsp->CompletionCode));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PldmFwCheckRspCompletionAndLength (
  IN CONST VOID    *RspBuffer,
  IN UINTN         RspLength,
  IN UINTN         RspLengthExpected,
  IN CONST CHAR8   *Function,
  IN CONST CHAR16  *DeviceName
  )
{
  EFI_STATUS  Status;

  Status = PldmFwCheckRspCompletion (RspBuffer, Function, DeviceName);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (RspLength != RspLengthExpected) {
    DEBUG ((DEBUG_ERROR, "%a: %s response len=%u, exp=%u\n", Function, DeviceName, RspLength, RspLengthExpected));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

UINTN
EFIAPI
PldmFwGetFwParamsComponentTableOffset (
  IN CONST PLDM_FW_GET_FW_PARAMS_RESPONSE  *GetFwParamsRsp
  )
{
  UINTN  Offset;

  Offset =
    OFFSET_OF (PLDM_FW_GET_FW_PARAMS_RESPONSE, ImageSetActiveVersionString) +
    GetFwParamsRsp->ImageSetActiveVersionStringLength +
    GetFwParamsRsp->ImageSetPendingVersionStringLength;

  return Offset;
}

CONST PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY *
EFIAPI
PldmFwGetFwParamsComponent (
  IN CONST PLDM_FW_GET_FW_PARAMS_RESPONSE  *GetFwParamsRsp,
  IN UINTN                                 ComponentIndex
  )
{
  CONST PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY  *FwParamsComponent;
  UINTN                                          Index;

  ASSERT (ComponentIndex < GetFwParamsRsp->ComponentCount);

  FwParamsComponent = (CONST PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY *)
                      ((CONST UINT8 *)GetFwParamsRsp + PldmFwGetFwParamsComponentTableOffset (GetFwParamsRsp));
  for (Index = 0; Index < ComponentIndex; Index++) {
    FwParamsComponent = (PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY *)
                        ((UINT8 *)FwParamsComponent +
                         OFFSET_OF (PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY, ActiveVersionString) +
                         FwParamsComponent->ActiveVersionStringLength +
                         FwParamsComponent->PendingVersionStringLength);
  }

  return FwParamsComponent;
}

CONST PLDM_FW_DESCRIPTOR *
EFIAPI
PldmFwDescNext (
  IN CONST PLDM_FW_DESCRIPTOR  *Desc
  )
{
  UINTN  DescSize;

  DescSize = Desc->Length + OFFSET_OF (PLDM_FW_DESCRIPTOR, Data);

  return (CONST PLDM_FW_DESCRIPTOR *)((CONST UINT8 *)Desc + DescSize);
}

VOID
EFIAPI
PldmFwPrintFwDesc (
  IN CONST PLDM_FW_DESCRIPTOR  *Desc
  )
{
  UINTN  Index;

  DEBUG ((DEBUG_INFO, "Type=0x%x Len=%u ", Desc->Type, Desc->Length));
  for (Index = 0; Index < Desc->Length; Index++) {
    DEBUG ((DEBUG_INFO, "0x%x(%c) ", Desc->Data[Index], Desc->Data[Index]));
  }

  DEBUG ((DEBUG_INFO, "\n"));
}

VOID
EFIAPI
PldmFwPrintQueryDeviceIdsRsp (
  IN CONST PLDM_FW_QUERY_DEVICE_IDS_RESPONSE  *Rsp,
  IN CONST CHAR16                             *DeviceName
  )
{
  CONST PLDM_FW_DESCRIPTOR  *Desc;
  UINTN                     Index;

  DEBUG ((DEBUG_INFO, "%a: %s DescCount=%u\n", __FUNCTION__, DeviceName, Rsp->Count));
  Desc = Rsp->Descriptors;
  for (Index = 0; Index < Rsp->Count; Index++) {
    DEBUG ((DEBUG_INFO, "Desc %u ", Index));
    PldmFwPrintFwDesc (Desc);
    Desc = PldmFwDescNext (Desc);
  }
}

EFI_STATUS
EFIAPI
PldmFwQueryDeviceIdsCheckRsp (
  IN CONST PLDM_FW_QUERY_DEVICE_IDS_RESPONSE  *Rsp,
  IN UINTN                                    RspLength,
  IN CONST CHAR16                             *DeviceName
  )
{
  EFI_STATUS  Status;
  UINTN       Length;

  Status = PldmFwCheckRspCompletion (Rsp, __FUNCTION__, DeviceName);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Length = OFFSET_OF (PLDM_FW_QUERY_DEVICE_IDS_RESPONSE, Descriptors) + Rsp->Length;
  if (RspLength != Length) {
    DEBUG ((DEBUG_ERROR, "%a: %s bad rsp length: %u!=%u\n", __FUNCTION__, DeviceName, RspLength, Length));
    return EFI_DEVICE_ERROR;
  }

  PldmFwPrintQueryDeviceIdsRsp (Rsp, DeviceName);

  return EFI_SUCCESS;
}

VOID
EFIAPI
PldmFwPrintComponentEntry (
  IN CONST PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY  *ComponentEntry
  )
{
  DEBUG ((
    DEBUG_INFO,
    "Class=0x%x Id=0x%x Ver=0x%x (%.*a) Date=%.8a\n",
    ComponentEntry->Classification,
    ComponentEntry->Id,
    ComponentEntry->ActiveComparisonStamp,
    ComponentEntry->ActiveVersionStringLength,
    ComponentEntry->ActiveVersionString,
    ComponentEntry->ActiveReleaseDate
    ));
}

EFI_STATUS
EFIAPI
PldmFwGetFwParamsCheckRsp (
  IN CONST PLDM_FW_GET_FW_PARAMS_RESPONSE  *Rsp,
  IN UINTN                                 RspLength,
  IN CONST CHAR16                          *DeviceName
  )
{
  CONST PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY  *ComponentEntry;
  EFI_STATUS                                     Status;
  UINTN                                          Length;
  UINTN                                          Index;

  Status = PldmFwCheckRspCompletion (Rsp, __FUNCTION__, DeviceName);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Length = PldmFwGetFwParamsComponentTableOffset (Rsp);
  for (Index = 0; Index < Rsp->ComponentCount; Index++) {
    ComponentEntry = PldmFwGetFwParamsComponent (Rsp, Index);
    Length        +=
      OFFSET_OF (PLDM_FW_COMPONENT_PARAMETER_TABLE_ENTRY, ActiveVersionString) +
      ComponentEntry->ActiveVersionStringLength +
      ComponentEntry->PendingVersionStringLength;
  }

  if (RspLength != Length) {
    DEBUG ((DEBUG_ERROR, "%a: %s bad rsp length: %u!=%u\n", __FUNCTION__, DeviceName, RspLength, Length));
    return EFI_DEVICE_ERROR;
  }

  DEBUG ((DEBUG_INFO, "%a: %s %u components\n", __FUNCTION__, DeviceName, Rsp->ComponentCount));
  for (Index = 0; Index < Rsp->ComponentCount; Index++) {
    ComponentEntry = PldmFwGetFwParamsComponent (Rsp, Index);

    DEBUG ((DEBUG_INFO, "Component %u ", Index));
    PldmFwPrintComponentEntry (ComponentEntry);
  }

  return EFI_SUCCESS;
}

BOOLEAN
EFIAPI
PldmFwDescriptorIsInList (
  IN CONST PLDM_FW_DESCRIPTOR  *Descriptor,
  IN CONST PLDM_FW_DESCRIPTOR  *List,
  UINTN                        Count
  )
{
  UINTN                     Index;
  CONST PLDM_FW_DESCRIPTOR  *ListDescriptor;

  ListDescriptor = List;
  for (Index = 0; Index < Count; Index++) {
    if ((Descriptor->Type == ListDescriptor->Type) &&
        (Descriptor->Length == ListDescriptor->Length) &&
        (CompareMem (Descriptor->Data, ListDescriptor->Data, Descriptor->Length) == 0))
    {
      return TRUE;
    }

    ListDescriptor = PldmFwDescNext (ListDescriptor);
  }

  return FALSE;
}
