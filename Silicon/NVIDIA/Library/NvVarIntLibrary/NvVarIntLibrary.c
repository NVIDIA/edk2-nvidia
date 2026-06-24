/** @file

  Library to compute variable-store integrity measurements.

  The APIs can be called during a variable update (before the FVB Write) or
  at bootup to measure the variables on flash.

  SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/MmServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/HashApiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NvVarIntLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>
#include <Library/SortLib.h>
#include <Guid/GlobalVariable.h>
#include <Guid/ImageAuthentication.h>
#include <Guid/MemoryTypeInformation.h>
#include <Guid/MtcVendor.h>
#include <IndustryStandard/Tpm20.h>
#include <Protocol/SmmVariable.h>
#include <Library/PrintLib.h>
#include <Library/MmVarLib.h>

#define HEADER_SZ_BYTES                   (1)
#define VAR_INT_INITIAL_NAME_BUFFER_SIZE  (128 * sizeof (CHAR16))
#define NV_VAR_INT_MAX_DIGEST_SIZE        SHA512_DIGEST_SIZE

#ifndef NV_VAR_INT_VERBOSE_MEASURE_LOGS
#define NV_VAR_INT_VERBOSE_MEASURE_LOGS  0
#endif

/* FNV-1a 64-bit constants for compact verbose-log data fingerprints. */
#define NV_VAR_INT_FNV1A_64_OFFSET_BASIS  14695981039346656037ULL
#define NV_VAR_INT_FNV1A_64_PRIME         1099511628211ULL

typedef struct {
  CHAR16      *VarName;
  EFI_GUID    *VarGuid;
  VOID        *Data;
  UINTN       Size;
  UINT32      Attr;
} MEASURE_VAR_TYPE;

typedef struct {
  CHAR16      *VarName;
  EFI_GUID    VarGuid;
  UINT32      Attributes;
  UINTN       DataSize;
  VOID        *Data;
  BOOLEAN     DataOwned;
} NV_VAR_MEASURE_DESCRIPTOR;

typedef enum {
  NvVarMeasureComponentBootServiceOnly,
  NvVarMeasureComponentRuntime,
  NvVarMeasureComponentMax
} NV_VAR_MEASURE_COMPONENT;

STATIC
CONST CHAR8 *
NvVarMeasureComponentName (
  IN NV_VAR_MEASURE_COMPONENT  Component
  )
{
  switch (Component) {
    case NvVarMeasureComponentBootServiceOnly:
      return "nv-bs-only";
    case NvVarMeasureComponentRuntime:
      return "nv-rt";
    default:
      return "unknown";
  }
}

#if NV_VAR_INT_VERBOSE_MEASURE_LOGS
STATIC
UINT64
ComputeDataChecksum (
  IN VOID   *Data OPTIONAL,
  IN UINTN  DataSize
  )
{
  UINT8   *Data8;
  UINTN   Index;
  UINT64  Checksum;

  Checksum = NV_VAR_INT_FNV1A_64_OFFSET_BASIS;
  Data8    = (UINT8 *)Data;

  for (Index = 0; Index < DataSize; Index++) {
    Checksum ^= Data8[Index];
    Checksum *= NV_VAR_INT_FNV1A_64_PRIME;
  }

  return Checksum;
}

STATIC
VOID
LogDataPreview (
  IN VOID   *Data OPTIONAL,
  IN UINTN  DataSize
  )
{
  UINT8  *Data8;

  if ((Data == NULL) || (DataSize == 0)) {
    DEBUG ((DEBUG_ERROR, "empty"));
    return;
  }

  Data8 = (UINT8 *)Data;
  DEBUG ((DEBUG_ERROR, "%02x", Data8[0]));
  if (DataSize > 1) {
    DEBUG ((DEBUG_ERROR, "%02x", Data8[1]));
  }

  if (DataSize > 2) {
    DEBUG ((DEBUG_ERROR, "%02x", Data8[2]));
  }

  if (DataSize > 3) {
    DEBUG ((DEBUG_ERROR, "%02x", Data8[3]));
  }

  if (DataSize > 4) {
    DEBUG ((DEBUG_ERROR, "%02x", Data8[4]));
  }

  if (DataSize > 5) {
    DEBUG ((DEBUG_ERROR, "%02x", Data8[5]));
  }

  if (DataSize > 6) {
    DEBUG ((DEBUG_ERROR, "%02x", Data8[6]));
  }

  if (DataSize > 7) {
    DEBUG ((DEBUG_ERROR, "%02x", Data8[7]));
  }
}

#endif

STATIC
VOID
LogNvVarMeasureDescriptors (
  IN NV_VAR_MEASURE_COMPONENT   Component,
  IN CHAR16                     *TriggerName OPTIONAL,
  IN EFI_GUID                   *TriggerGuid OPTIONAL,
  IN NV_VAR_MEASURE_DESCRIPTOR  *DescriptorList,
  IN UINTN                      DescriptorCount
  )
{
 #if NV_VAR_INT_VERBOSE_MEASURE_LOGS
  NV_VAR_MEASURE_DESCRIPTOR  *Descriptor;
  CONST CHAR8                *Mode;
  UINTN                      Index;

  if ((DescriptorCount != 0) && (DescriptorList == NULL)) {
    return;
  }

  Mode = (TriggerName == NULL) ? "full" : "pending";

  if ((TriggerName != NULL) && (TriggerGuid != NULL)) {
    DEBUG ((
      DEBUG_ERROR,
      "VarIntMeasureList: component=%a mode=%a count=%lu trigger=%s guid=%g\n",
      NvVarMeasureComponentName (Component),
      Mode,
      (UINT64)DescriptorCount,
      TriggerName,
      TriggerGuid
      ));
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "VarIntMeasureList: component=%a mode=%a count=%lu trigger=<none>\n",
      NvVarMeasureComponentName (Component),
      Mode,
      (UINT64)DescriptorCount
      ));
  }

  for (Index = 0; Index < DescriptorCount; Index++) {
    Descriptor = &DescriptorList[Index];
    DEBUG ((
      DEBUG_ERROR,
      "VarIntMeasureVar: component=%a mode=%a index=%lu name=%s guid=%g attr=0x%08x size=%lu checksum=0x%lx data=",
      NvVarMeasureComponentName (Component),
      Mode,
      (UINT64)Index,
      Descriptor->VarName,
      &Descriptor->VarGuid,
      Descriptor->Attributes,
      (UINT64)Descriptor->DataSize,
      ComputeDataChecksum (Descriptor->Data, Descriptor->DataSize)
      ));
    LogDataPreview (Descriptor->Data, Descriptor->DataSize);
    DEBUG ((DEBUG_ERROR, "\n"));
  }

 #else
  (VOID)Component;
  (VOID)TriggerName;
  (VOID)TriggerGuid;
  (VOID)DescriptorList;
  (VOID)DescriptorCount;
 #endif
}

STATIC HASH_API_CONTEXT           HashContext   = NULL;
STATIC VOID                       **BootOptions = NULL;
STATIC UINTN                      BootCount     = 0;
STATIC UINT16                     *BootOrder;
STATIC EFI_SMM_VARIABLE_PROTOCOL  *mSmmVariable = NULL;
STATIC UINT8                      mNvBootServiceOnlyHash[NV_VAR_INT_MAX_DIGEST_SIZE];
STATIC UINT8                      mNvRuntimeHash[NV_VAR_INT_MAX_DIGEST_SIZE];
STATIC UINTN                      mNvBootServiceOnlyCount        = 0;
STATIC UINTN                      mNvRuntimeCount                = 0;
STATIC BOOLEAN                    mNvBootServiceOnlyHashValid    = FALSE;
STATIC BOOLEAN                    mNvRuntimeHashValid            = FALSE;
STATIC BOOLEAN                    mNvVarIntAfterExitBootServices = FALSE;

STATIC MEASURE_VAR_TYPE  SecureVarsV0[] = {
  { EFI_SECURE_BOOT_MODE_NAME,    &gEfiGlobalVariableGuid,        NULL, 0 },
  { EFI_PLATFORM_KEY_NAME,        &gEfiGlobalVariableGuid,        NULL, 0 },
  { EFI_KEY_EXCHANGE_KEY_NAME,    &gEfiGlobalVariableGuid,        NULL, 0 },
  { EFI_IMAGE_SECURITY_DATABASE,  &gEfiImageSecurityDatabaseGuid, NULL, 0 },
  { EFI_IMAGE_SECURITY_DATABASE1, &gEfiImageSecurityDatabaseGuid, NULL, 0 }
};

STATIC MEASURE_VAR_TYPE  ExcludedVars[] = {
  /* Use a NULL VarName to exclude all variables that belong to a GUID. */
  { MTC_VARIABLE_NAME,                         &gMtcVendorGuid,                NULL, 0 },
  { EFI_MEMORY_TYPE_INFORMATION_VARIABLE_NAME, &gEfiMemoryTypeInformationGuid, NULL, 0 }
};

VOID
EFIAPI
NvVarIntNotifyExitBootServices (
  VOID
  )
{
  if (mNvVarIntAfterExitBootServices == FALSE) {
    DEBUG ((DEBUG_INFO, "VarIntRuntimeState: after_ebs=1\n"));
  }

  mNvVarIntAfterExitBootServices = TRUE;
}

STATIC
EFI_STATUS
GetActiveDigestSize (
  OUT UINTN  *DigestSize
  )
{
  if (DigestSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  switch (PcdGet32 (PcdHashApiLibPolicy)) {
    case HASH_ALG_SHA256:
    case HASH_ALG_SM3_256:
      *DigestSize = SHA256_DIGEST_SIZE;
      break;

    case HASH_ALG_SHA384:
      *DigestSize = SHA384_DIGEST_SIZE;
      break;

    case HASH_ALG_SHA512:
      *DigestSize = SHA512_DIGEST_SIZE;
      break;

    default:
      *DigestSize = 0;
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetSmmVariableProtocol (
  OUT EFI_SMM_VARIABLE_PROTOCOL  **SmmVariable
  )
{
  EFI_STATUS  Status;

  if (SmmVariable == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (mSmmVariable == NULL) {
    Status = gMmst->MmLocateProtocol (
                      &gEfiSmmVariableProtocolGuid,
                      NULL,
                      (VOID **)&mSmmVariable
                      );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to locate SMM variable protocol %r\n", __FUNCTION__, Status));
      mSmmVariable = NULL;
      return Status;
    }
  }

  *SmmVariable = mSmmVariable;
  return EFI_SUCCESS;
}

BOOLEAN
EFIAPI
NvVarIntIsExcludedVar (
  IN CHAR16    *VarName,
  IN EFI_GUID  *VarGuid
  )
{
  UINTN  Index;

  if (VarGuid == NULL) {
    return FALSE;
  }

  for (Index = 0; Index < ARRAY_SIZE (ExcludedVars); Index++) {
    if ((ExcludedVars[Index].VarGuid == NULL) ||
        (CompareGuid (ExcludedVars[Index].VarGuid, VarGuid) == FALSE))
    {
      continue;
    }

    if (ExcludedVars[Index].VarName == NULL) {
      return TRUE;
    }

    if ((VarName != NULL) && (StrCmp (ExcludedVars[Index].VarName, VarName) == 0)) {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
BOOLEAN
IsDeleteRequest (
  IN UINT32  Attributes,
  IN UINTN   DataSize
  )
{
  return (BOOLEAN)(
                   (((Attributes & EFI_VARIABLE_APPEND_WRITE) == 0) && (DataSize == 0)) ||
                   ((Attributes & (EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS)) == 0)
                   );
}

BOOLEAN
EFIAPI
NvVarIntCanUpdateMeasurement (
  IN CHAR16    *VarName,
  IN EFI_GUID  *VarGuid,
  IN UINT32    Attributes,
  IN UINTN     DataSize
  )
{
  if ((VarName == NULL) || (VarGuid == NULL)) {
    return FALSE;
  }

  if (NvVarIntIsExcludedVar (VarName, VarGuid) == TRUE) {
    return FALSE;
  }

  if ((IsDeleteRequest (Attributes, DataSize) == FALSE) &&
      ((Attributes & EFI_VARIABLE_NON_VOLATILE) == 0))
  {
    return FALSE;
  }

  return TRUE;
}

STATIC
BOOLEAN
IsMeasurableNvVarForComponent (
  IN UINT32                    Attributes,
  IN NV_VAR_MEASURE_COMPONENT  Component
  )
{
  if ((Attributes & EFI_VARIABLE_NON_VOLATILE) == 0) {
    return FALSE;
  }

  switch (Component) {
    case NvVarMeasureComponentBootServiceOnly:
      return (BOOLEAN)(
                       ((Attributes & EFI_VARIABLE_BOOTSERVICE_ACCESS) != 0) &&
                       ((Attributes & EFI_VARIABLE_RUNTIME_ACCESS) == 0)
                       );

    case NvVarMeasureComponentRuntime:
      return (BOOLEAN)((Attributes & EFI_VARIABLE_RUNTIME_ACCESS) != 0);

    default:
      return FALSE;
  }
}

STATIC
EFI_STATUS
EnsureHashContext (
  VOID
  )
{
  if (HashContext != NULL) {
    return EFI_SUCCESS;
  }

  HashContext = AllocateRuntimeZeroPool (HashApiGetContextSize ());
  if (HashContext == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
HashUpdateChecked (
  IN VOID   *Data,
  IN UINTN  DataSize
  )
{
  if (DataSize == 0) {
    return EFI_SUCCESS;
  }

  if (Data == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (HashApiUpdate (HashContext, Data, DataSize) != TRUE) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to update hash\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
HashVariableRecord (
  IN CHAR16    *VarName,
  IN EFI_GUID  *VarGuid,
  IN UINT32    Attributes,
  IN VOID      *Data,
  IN UINTN     DataSize
  )
{
  EFI_STATUS  Status;
  UINT64      DataSize64;

  if ((VarName == NULL) || (VarGuid == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DataSize64 = DataSize;

  Status = HashUpdateChecked (VarName, StrSize (VarName));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = HashUpdateChecked (VarGuid, sizeof (*VarGuid));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = HashUpdateChecked (&Attributes, sizeof (Attributes));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = HashUpdateChecked (&DataSize64, sizeof (DataSize64));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return HashUpdateChecked (Data, DataSize);
}

STATIC
EFI_STATUS
GetVariableInfo (
  IN  EFI_SMM_VARIABLE_PROTOCOL  *SmmVariable,
  IN  CHAR16                     *VarName,
  IN  EFI_GUID                   *VarGuid,
  OUT UINTN                      *DataSize,
  OUT UINT32                     *Attributes
  )
{
  EFI_STATUS  Status;

  if ((SmmVariable == NULL) || (VarName == NULL) || (VarGuid == NULL) ||
      (DataSize == NULL) || (Attributes == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  *DataSize   = 0;
  *Attributes = 0;

  Status = SmmVariable->SmmGetVariable (
                          VarName,
                          VarGuid,
                          Attributes,
                          DataSize,
                          NULL
                          );
  if ((Status == EFI_SUCCESS) || (Status == EFI_BUFFER_TOO_SMALL)) {
    return EFI_SUCCESS;
  }

  return Status;
}

STATIC
EFI_STATUS
GetVariableData (
  IN     EFI_SMM_VARIABLE_PROTOCOL  *SmmVariable,
  IN     CHAR16                     *VarName,
  IN     EFI_GUID                   *VarGuid,
  OUT    VOID                       **Data,
  IN OUT UINTN                      *DataSize,
  IN OUT UINT32                     *Attributes
  )
{
  EFI_STATUS  Status;

  if ((SmmVariable == NULL) || (VarName == NULL) || (VarGuid == NULL) ||
      (Data == NULL) || (DataSize == NULL) || (Attributes == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  *Data = NULL;

  if (*DataSize == 0) {
    return EFI_SUCCESS;
  }

  *Data = AllocatePool (*DataSize);
  if (*Data == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = SmmVariable->SmmGetVariable (
                          VarName,
                          VarGuid,
                          Attributes,
                          DataSize,
                          *Data
                          );
  if (EFI_ERROR (Status)) {
    FreePool (*Data);
    *Data = NULL;
  }

  return Status;
}

BOOLEAN
EFIAPI
NvVarIntIsNoOpUpdate (
  IN CHAR16    *VarName,
  IN EFI_GUID  *VarGuid,
  IN UINT32    Attributes,
  IN VOID      *Data,
  IN UINTN     DataSize
  )
{
  EFI_STATUS                 Status;
  EFI_SMM_VARIABLE_PROTOCOL  *SmmVariable;
  VOID                       *ExistingData;
  UINTN                      ExistingDataSize;
  UINT32                     ExistingAttributes;
  UINT32                     EffectiveAttributes;
  BOOLEAN                    SamePayload;

  if ((VarName == NULL) || (VarGuid == NULL)) {
    return FALSE;
  }

  if ((IsDeleteRequest (Attributes, DataSize) == TRUE) ||
      ((Attributes & EFI_VARIABLE_APPEND_WRITE) != 0) ||
      ((Data == NULL) && (DataSize != 0)))
  {
    return FALSE;
  }

  Status = GetSmmVariableProtocol (&SmmVariable);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  ExistingData       = NULL;
  ExistingDataSize   = 0;
  ExistingAttributes = 0;
  Status             = GetVariableInfo (
                         SmmVariable,
                         VarName,
                         VarGuid,
                         &ExistingDataSize,
                         &ExistingAttributes
                         );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  EffectiveAttributes = Attributes & ~EFI_VARIABLE_APPEND_WRITE;
  if ((ExistingAttributes != EffectiveAttributes) || (ExistingDataSize != DataSize)) {
    return FALSE;
  }

  if (ExistingDataSize == 0) {
    return TRUE;
  }

  Status = GetVariableData (
             SmmVariable,
             VarName,
             VarGuid,
             &ExistingData,
             &ExistingDataSize,
             &ExistingAttributes
             );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  SamePayload = (BOOLEAN)(CompareMem (ExistingData, Data, DataSize) == 0);
  FreePool (ExistingData);

  return SamePayload;
}

STATIC
BOOLEAN
IsSameVariable (
  IN CHAR16    *FirstName,
  IN EFI_GUID  *FirstGuid,
  IN CHAR16    *SecondName,
  IN EFI_GUID  *SecondGuid
  )
{
  if ((FirstName == NULL) || (FirstGuid == NULL) ||
      (SecondName == NULL) || (SecondGuid == NULL))
  {
    return FALSE;
  }

  return (BOOLEAN)(
                   (StrCmp (FirstName, SecondName) == 0) &&
                   (CompareGuid (FirstGuid, SecondGuid) == TRUE)
                   );
}

STATIC
EFI_STATUS
AddNvVarMeasureDescriptor (
  IN OUT NV_VAR_MEASURE_DESCRIPTOR  **DescriptorList,
  IN OUT UINTN                      *DescriptorCount,
  IN OUT UINTN                      *DescriptorCapacity,
  IN     CHAR16                     *VarName,
  IN     EFI_GUID                   *VarGuid,
  IN     UINT32                     Attributes,
  IN     UINTN                      DataSize,
  IN     VOID                       *Data OPTIONAL,
  IN     BOOLEAN                    DataOwned
  )
{
  EFI_STATUS                 Status;
  NV_VAR_MEASURE_DESCRIPTOR  *NewDescriptorList;
  NV_VAR_MEASURE_DESCRIPTOR  *Descriptor;
  CHAR16                     *NameCopy;
  UINTN                      NameSize;
  UINTN                      NewCapacity;

  if ((DescriptorList == NULL) || (DescriptorCount == NULL) ||
      (DescriptorCapacity == NULL) || (VarName == NULL) || (VarGuid == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;

  if (*DescriptorCount == *DescriptorCapacity) {
    NewCapacity = (*DescriptorCapacity == 0) ? 32 : (*DescriptorCapacity * 2);
    if ((NewCapacity < *DescriptorCapacity) ||
        (NewCapacity > (MAX_UINTN / sizeof (**DescriptorList))))
    {
      return EFI_OUT_OF_RESOURCES;
    }

    NewDescriptorList = AllocateZeroPool (NewCapacity * sizeof (*NewDescriptorList));
    if (NewDescriptorList == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    if (*DescriptorList != NULL) {
      CopyMem (
        NewDescriptorList,
        *DescriptorList,
        *DescriptorCount * sizeof (**DescriptorList)
        );
      FreePool (*DescriptorList);
    }

    *DescriptorList     = NewDescriptorList;
    *DescriptorCapacity = NewCapacity;
  }

  NameSize = StrSize (VarName);
  NameCopy = AllocatePool (NameSize);
  if (NameCopy == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (NameCopy, VarName, NameSize);

  Descriptor             = &(*DescriptorList)[*DescriptorCount];
  Descriptor->VarName    = NameCopy;
  Descriptor->Attributes = Attributes;
  Descriptor->DataSize   = DataSize;
  Descriptor->Data       = Data;
  Descriptor->DataOwned  = DataOwned;
  CopyGuid (&Descriptor->VarGuid, VarGuid);

  *DescriptorCount += 1;
  return Status;
}

STATIC
VOID
FreeNvVarMeasureDescriptors (
  IN NV_VAR_MEASURE_DESCRIPTOR  *DescriptorList,
  IN UINTN                      DescriptorCount
  )
{
  UINTN  Index;

  if (DescriptorList == NULL) {
    return;
  }

  for (Index = 0; Index < DescriptorCount; Index++) {
    if (DescriptorList[Index].VarName != NULL) {
      FreePool (DescriptorList[Index].VarName);
    }

    if ((DescriptorList[Index].DataOwned == TRUE) && (DescriptorList[Index].Data != NULL)) {
      FreePool (DescriptorList[Index].Data);
    }
  }

  FreePool (DescriptorList);
}

STATIC
INTN
CompareGuidForMeasurementSort (
  IN CONST EFI_GUID  *FirstGuid,
  IN CONST EFI_GUID  *SecondGuid
  )
{
  UINTN  Index;

  if (FirstGuid->Data1 < SecondGuid->Data1) {
    return -1;
  } else if (FirstGuid->Data1 > SecondGuid->Data1) {
    return 1;
  }

  if (FirstGuid->Data2 < SecondGuid->Data2) {
    return -1;
  } else if (FirstGuid->Data2 > SecondGuid->Data2) {
    return 1;
  }

  if (FirstGuid->Data3 < SecondGuid->Data3) {
    return -1;
  } else if (FirstGuid->Data3 > SecondGuid->Data3) {
    return 1;
  }

  for (Index = 0; Index < sizeof (FirstGuid->Data4); Index++) {
    if (FirstGuid->Data4[Index] < SecondGuid->Data4[Index]) {
      return -1;
    } else if (FirstGuid->Data4[Index] > SecondGuid->Data4[Index]) {
      return 1;
    }
  }

  return 0;
}

STATIC
INTN
EFIAPI
CompareNvVarMeasureDescriptor (
  IN CONST VOID  *FirstBuffer,
  IN CONST VOID  *SecondBuffer
  )
{
  CONST NV_VAR_MEASURE_DESCRIPTOR  *FirstDescriptor;
  CONST NV_VAR_MEASURE_DESCRIPTOR  *SecondDescriptor;
  INTN                             Result;

  FirstDescriptor  = (CONST NV_VAR_MEASURE_DESCRIPTOR *)FirstBuffer;
  SecondDescriptor = (CONST NV_VAR_MEASURE_DESCRIPTOR *)SecondBuffer;

  Result = CompareGuidForMeasurementSort (
             &FirstDescriptor->VarGuid,
             &SecondDescriptor->VarGuid
             );
  if (Result != 0) {
    return Result;
  }

  return StrCmp (FirstDescriptor->VarName, SecondDescriptor->VarName);
}

STATIC
VOID
SortNvVarMeasureDescriptors (
  IN OUT NV_VAR_MEASURE_DESCRIPTOR  *DescriptorList,
  IN     UINTN                      DescriptorCount
  )
{
  if ((DescriptorList == NULL) || (DescriptorCount < 2)) {
    return;
  }

  PerformQuickSort (
    DescriptorList,
    DescriptorCount,
    sizeof (DescriptorList[0]),
    CompareNvVarMeasureDescriptor
    );
}

STATIC
EFI_STATUS
HashNvVarMeasureDescriptors (
  IN NV_VAR_MEASURE_DESCRIPTOR  *DescriptorList,
  IN UINTN                      DescriptorCount
  )
{
  EFI_STATUS                 Status;
  NV_VAR_MEASURE_DESCRIPTOR  *Descriptor;
  UINTN                      Index;

  if ((DescriptorCount != 0) && (DescriptorList == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < DescriptorCount; Index++) {
    Descriptor = &DescriptorList[Index];

    Status = HashVariableRecord (
               Descriptor->VarName,
               &Descriptor->VarGuid,
               Descriptor->Attributes,
               Descriptor->Data,
               Descriptor->DataSize
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
 *
 * MeasureBootVars
 * Measure the Boot variables present on the flash and optionally
 * the variable being updated.
 *
 * @param[in] VarName  Optional Variable name.
 * @param[in] VarGuid  Optional Variable guid.
 * @param[in] VarGuid  Optional Variable Attributes.
 * @param[in] VarGuid  Optional Variabkle Data Buffer.
 * @param[in] VarGuid  Optional Size of the Variable Data.
 *
 * @result    EFI_SUCCESS Succesfully computed the measurement.
 *            other       Failed to compute measurement.
 */
EFI_STATUS
EFIAPI
MeasureBootVars (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL
  )
{
  EFI_STATUS  Status;
  UINT32      Attr;
  BOOLEAN     UpdatingBootOrder;
  UINTN       BootOptionSize;
  CHAR16      BootOptionName[] = L"Bootxxxx";
  UINTN       Index;

  BootOrder         = NULL;
  BootCount         = 0;
  UpdatingBootOrder = FALSE;

  /*
   * If the BootOrder is being updated then use the new incoming data to
   * get the updated bootorder data.
   */
  if ((VarName != NULL) &&
      (StrCmp (VarName, EFI_BOOT_ORDER_VARIABLE_NAME) == 0))
  {
    UpdatingBootOrder = TRUE;
    BootOrder         = Data;
    BootCount         = (DataSize / sizeof (UINT16));
    DEBUG ((DEBUG_INFO, "Updating BootOrder Count %u %p %u\n", BootCount, BootOrder, Attributes));
    if (HashApiUpdate (HashContext, Data, DataSize) != TRUE) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to update the HashContext\n",
        __FUNCTION__
        ));
      Status = EFI_UNSUPPORTED;
      goto ExitMeasureBootVars;
    }
  } else {
    /* Get the registered boot options */
    Status = MmGetVariable3 (
               EFI_BOOT_ORDER_VARIABLE_NAME,
               &gEfiGlobalVariableGuid,
               (VOID **)&BootOrder,
               &BootCount,
               &Attr
               );
    if ((Status == EFI_SUCCESS)) {
      if (HashApiUpdate (HashContext, BootOrder, BootCount) != TRUE) {
        DEBUG ((DEBUG_ERROR, "%a:%d Failed to update Hash\n", __FUNCTION__, __LINE__));
        Status = EFI_UNSUPPORTED;
        goto ExitMeasureBootVars;
      }

      BootCount /= sizeof (UINT16);
    } else {
      /* If we couldn't get the BootOrder Variable, then exit but return
       * success, its possible this is the first boot.
       */
      DEBUG ((
        DEBUG_ERROR,
        "%a:%d Failed to get BootOrder %r\n",
        __FUNCTION__,
        __LINE__,
        Status
        ));
      Status = EFI_SUCCESS;
      goto ExitMeasureBootVars;
    }
  }

  BootOptions = AllocateZeroPool (BootCount * sizeof (VOID *));
  if (BootOptions == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitMeasureBootVars, "%a: Failed to allocate BootOptions - %r", __FUNCTION__, Status);
  }

  for (Index = 0; Index < BootCount; Index++) {
    UnicodeSPrint (
      BootOptionName,
      sizeof (BootOptionName),
      L"Boot%04x",
      BootOrder[Index]
      );

    /* If a new BootOption is being added, use the data from the updatevariable to
     * compute the new hash and move on to the new next boot option.
     */
    if ((VarName != NULL) &&
        (StrCmp (BootOptionName, VarName) == 0))
    {
      DEBUG ((DEBUG_INFO, "Update %s Size %u %p\n", VarName, DataSize, Data));
      HashApiUpdate (HashContext, Data, DataSize);
      continue;
    }

    BootOptionSize = 0;
    Status         = MmGetVariable3 (
                       BootOptionName,
                       &gEfiGlobalVariableGuid,
                       &BootOptions[Index],
                       &BootOptionSize,
                       &Attr
                       );
    if ((EFI_ERROR (Status))) {
      /* This can happen because the BootOrder gets updated before the Boot
       * option is actually added.
       */
      if (Status == EFI_NOT_FOUND) {
        Status = EFI_SUCCESS;
      }

      continue;
    }

    DEBUG ((DEBUG_INFO, "Adding %s Size %u %p\n", BootOptionName, BootOptionSize, BootOptions[Index]));
    HashApiUpdate (HashContext, BootOptions[Index], BootOptionSize);
    FreePool (BootOptions[Index]);
  }

ExitMeasureBootVars:

  if (BootOptions != NULL) {
    FreePool (BootOptions);
    BootOptions = NULL;
  }

  if ((UpdatingBootOrder == TRUE)) {
    BootOrder = NULL;
    BootCount = 0;
  }

  return Status;
}

/**
 *
 * MeasureBootNextVar
 * Measure BootNext and the Boot#### option selected by BootNext.
 *
 * @param[in] VarName     Optional Variable name.
 * @param[in] VarGuid     Optional Variable guid.
 * @param[in] Attributes  Optional Variable Attributes.
 * @param[in] Data        Optional Variable Data Buffer.
 * @param[in] DataSize    Optional Size of the Variable Data.
 *
 * @result EFI_SUCCESS  Successfully computed the measurement.
 *         other        Failed to compute measurement.
 */
STATIC
EFI_STATUS
EFIAPI
MeasureBootNextVar (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL
  )
{
  EFI_STATUS  Status;
  UINT32      Attr;
  UINT16      BootNext;
  UINTN       BootNextSize;
  VOID        *BootOption;
  UINTN       BootOptionSize;
  CHAR16      BootOptionName[] = L"Bootxxxx";

  BootNext       = 0;
  BootNextSize   = sizeof (BootNext);
  BootOption     = NULL;
  BootOptionSize = 0;

  if ((VarName != NULL) &&
      (StrCmp (VarName, EFI_BOOT_NEXT_VARIABLE_NAME) == 0) &&
      (CompareGuid (VarGuid, &gEfiGlobalVariableGuid) == TRUE))
  {
    if (((Attributes & EFI_VARIABLE_NON_VOLATILE) == 0) ||
        (Data == NULL) ||
        (DataSize != sizeof (BootNext)))
    {
      Status = EFI_SUCCESS;
      goto ExitMeasureBootNextVar;
    }

    CopyMem (&BootNext, Data, sizeof (BootNext));
  } else {
    Status = MmGetVariable3 (
               EFI_BOOT_NEXT_VARIABLE_NAME,
               &gEfiGlobalVariableGuid,
               (VOID **)&BootOption,
               &BootNextSize,
               &Attr
               );
    if (EFI_ERROR (Status)) {
      Status = EFI_SUCCESS;
      goto ExitMeasureBootNextVar;
    }

    if (BootNextSize != sizeof (BootNext)) {
      Status = EFI_DEVICE_ERROR;
      goto ExitMeasureBootNextVar;
    }

    CopyMem (&BootNext, BootOption, sizeof (BootNext));
    FreePool (BootOption);
    BootOption = NULL;
  }

  if (HashApiUpdate (HashContext, &BootNext, sizeof (BootNext)) != TRUE) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to update BootNext hash\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ExitMeasureBootNextVar;
  }

  UnicodeSPrint (
    BootOptionName,
    sizeof (BootOptionName),
    L"Boot%04x",
    BootNext
    );

  if ((VarName != NULL) &&
      (StrCmp (BootOptionName, VarName) == 0) &&
      (CompareGuid (VarGuid, &gEfiGlobalVariableGuid) == TRUE))
  {
    if (((Attributes & EFI_VARIABLE_NON_VOLATILE) == 0) ||
        (Data == NULL))
    {
      Status = EFI_SUCCESS;
      goto ExitMeasureBootNextVar;
    }

    BootOption     = Data;
    BootOptionSize = DataSize;
  } else {
    Status = MmGetVariable3 (
               BootOptionName,
               &gEfiGlobalVariableGuid,
               &BootOption,
               &BootOptionSize,
               &Attr
               );
    if (EFI_ERROR (Status)) {
      Status = EFI_SUCCESS;
      goto ExitMeasureBootNextVar;
    }
  }

  if (HashApiUpdate (HashContext, BootOption, BootOptionSize) != TRUE) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to update BootNext option hash\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ExitMeasureBootNextVar;
  }

  Status = EFI_SUCCESS;

ExitMeasureBootNextVar:
  if ((BootOption != NULL) && (BootOption != Data)) {
    FreePool (BootOption);
  }

  return Status;
}

/*
 * RemoveDuplicateSignatureList
 * Util function to scan and remove duplicate signatures.
 * (based on what is done in the AuthVariableLib)
 *
 * @param  Data        Existing Var Data.
 * @param  DataSize    Existing Var Size.
 * @param  NewData     New Var Data being added.
 * @param  NewDataSize New Var Data Size.
 *
 * @return EFI_SUCCESS          Removed the duplicates in the var Data.
 *         EFI_OUT_OF_RESOURCES Failed to allocate temp buffer needed.
 */
STATIC
EFI_STATUS
RemoveDupSignatureList (
  IN     VOID   *Data,
  IN     UINTN  DataSize,
  IN OUT VOID   *NewData,
  IN OUT UINTN  *NewDataSize
  )
{
  EFI_SIGNATURE_LIST  *CertList;
  EFI_SIGNATURE_DATA  *Cert;
  UINTN               CertCount;
  EFI_SIGNATURE_LIST  *NewCertList;
  EFI_SIGNATURE_DATA  *NewCert;
  UINTN               NewCertCount;
  UINTN               Index;
  UINTN               Index2;
  UINTN               Size;
  UINT8               *Tail;
  UINTN               CopiedCount;
  UINTN               SignatureListSize;
  BOOLEAN             IsNewCert;
  UINT8               *TempData;
  UINTN               TempDataSize;

  if (*NewDataSize == 0) {
    return EFI_SUCCESS;
  }

  TempDataSize = *NewDataSize;
  TempData     = AllocateZeroPool (TempDataSize);
  if (TempData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Tail = TempData;

  NewCertList = (EFI_SIGNATURE_LIST *)NewData;
  while ((*NewDataSize > 0) && (*NewDataSize >= NewCertList->SignatureListSize)) {
    NewCert      = (EFI_SIGNATURE_DATA *)((UINT8 *)NewCertList + sizeof (EFI_SIGNATURE_LIST) + NewCertList->SignatureHeaderSize);
    NewCertCount = (NewCertList->SignatureListSize - sizeof (EFI_SIGNATURE_LIST) - NewCertList->SignatureHeaderSize) / NewCertList->SignatureSize;

    CopiedCount = 0;
    for (Index = 0; Index < NewCertCount; Index++) {
      IsNewCert = TRUE;

      Size     = DataSize;
      CertList = (EFI_SIGNATURE_LIST *)Data;
      while ((Size > 0) && (Size >= CertList->SignatureListSize)) {
        if (CompareGuid (&CertList->SignatureType, &NewCertList->SignatureType) &&
            (CertList->SignatureSize == NewCertList->SignatureSize))
        {
          Cert      = (EFI_SIGNATURE_DATA *)((UINT8 *)CertList + sizeof (EFI_SIGNATURE_LIST) + CertList->SignatureHeaderSize);
          CertCount = (CertList->SignatureListSize - sizeof (EFI_SIGNATURE_LIST) - CertList->SignatureHeaderSize) / CertList->SignatureSize;
          for (Index2 = 0; Index2 < CertCount; Index2++) {
            //
            // Iterate each Signature Data in this Signature List.
            //
            if (CompareMem (NewCert, Cert, CertList->SignatureSize) == 0) {
              IsNewCert = FALSE;
              break;
            }

            Cert = (EFI_SIGNATURE_DATA *)((UINT8 *)Cert + CertList->SignatureSize);
          }
        }

        if (!IsNewCert) {
          break;
        }

        Size    -= CertList->SignatureListSize;
        CertList = (EFI_SIGNATURE_LIST *)((UINT8 *)CertList + CertList->SignatureListSize);
      }

      if (IsNewCert) {
        //
        // New EFI_SIGNATURE_DATA, keep it.
        //
        if (CopiedCount == 0) {
          //
          // Copy EFI_SIGNATURE_LIST header for only once.
          //
          CopyMem (Tail, NewCertList, sizeof (EFI_SIGNATURE_LIST) + NewCertList->SignatureHeaderSize);
          Tail = Tail + sizeof (EFI_SIGNATURE_LIST) + NewCertList->SignatureHeaderSize;
        }

        CopyMem (Tail, NewCert, NewCertList->SignatureSize);
        Tail += NewCertList->SignatureSize;
        CopiedCount++;
      }

      NewCert = (EFI_SIGNATURE_DATA *)((UINT8 *)NewCert + NewCertList->SignatureSize);
    }

    //
    // Update SignatureListSize in the kept EFI_SIGNATURE_LIST.
    //
    if (CopiedCount != 0) {
      SignatureListSize           = sizeof (EFI_SIGNATURE_LIST) + NewCertList->SignatureHeaderSize + (CopiedCount * NewCertList->SignatureSize);
      CertList                    = (EFI_SIGNATURE_LIST *)(Tail - SignatureListSize);
      CertList->SignatureListSize = (UINT32)SignatureListSize;
    }

    *NewDataSize -= NewCertList->SignatureListSize;
    NewCertList   = (EFI_SIGNATURE_LIST *)((UINT8 *)NewCertList + NewCertList->SignatureListSize);
  }

  TempDataSize = (Tail - (UINT8 *)TempData);
  CopyMem (NewData, TempData, TempDataSize);
  *NewDataSize = TempDataSize;
  DEBUG ((DEBUG_INFO, "%a: NewSize %u\n", __FUNCTION__, *NewDataSize));

  if (TempData != NULL) {
    FreePool (TempData);
  }

  return EFI_SUCCESS;
}

/**
 * MeasureSecureDbVars
 * Compute the Measurement for the SecureDb variables stored in the varstore
 * and optionally a secure variable being updated.
 *
 * @param[in] VarName  Optional Variable name.
 * @param[in] VarGuid  Optional Variable guid.
 * @param[in] VarGuid  Optional Variable Attributes.
 * @param[in] VarGuid  Optional Variabkle Data Buffer.
 * @param[in] VarGuid  Optional Size of the Variable Data.
 *
 * @result    EFI_SUCCESS Succesfully computed the measurement.
 *            Other Failed to update the HashValue.
 */
STATIC
EFI_STATUS
EFIAPI
MeasureSecureDbVarsInternal (
  IN  CHAR16            *VarName   OPTIONAL,
  IN  EFI_GUID          *VarGuid   OPTIONAL,
  IN  UINT32            Attributes OPTIONAL,
  IN  VOID              *Data      OPTIONAL,
  IN  UINTN             DataSize   OPTIONAL,
  IN  MEASURE_VAR_TYPE  *SecureVarList,
  IN  UINTN             SecureVarCount
  )
{
  EFI_STATUS  Status;
  UINTN       Index;
  UINT8       *PayloadPtr;
  UINT8       *CurPtr;
  UINT8       *CopyPtr;
  UINTN       PayloadSize;
  BOOLEAN     AppendWrite;
  UINTN       VarSize;

  Status      = EFI_SUCCESS;
  CurPtr      = NULL;
  CopyPtr     = NULL;
  PayloadPtr  = Data;
  PayloadSize = DataSize;

  for (Index = 0; Index < SecureVarCount; Index++) {
    AppendWrite = FALSE;
    DEBUG ((
      DEBUG_INFO,
      "%a: First add %s VarName \n",
      __FUNCTION__,
      SecureVarList[Index].VarName
      ));
    if (HashApiUpdate (HashContext, SecureVarList[Index].VarName, sizeof (SecureVarList[Index].VarName)) != TRUE) {
      DEBUG ((DEBUG_ERROR, "%a:%d Failed to update Hash\n", __FUNCTION__, __LINE__));
      Status = EFI_UNSUPPORTED;
      goto ExitMeasureSecureBootVars;
    }

    if (HashApiUpdate (HashContext, SecureVarList[Index].VarGuid, sizeof (SecureVarList[Index].VarGuid)) != TRUE ) {
      DEBUG ((DEBUG_ERROR, "%a:%d Failed to update Hash\n", __FUNCTION__, __LINE__));
      Status = EFI_UNSUPPORTED;
      goto ExitMeasureSecureBootVars;
    }

    /* If this SetVariable call is to a variable we're monitoring and if its a
     * Write then use the new Data.
     * For an Append Write, setup a new buffer and update the hash.
     */
    if ((VarName != NULL) && (VarGuid != NULL) &&
        (StrCmp (SecureVarList[Index].VarName, VarName) == 0) &&
        (CompareGuid (VarGuid, SecureVarList[Index].VarGuid) == TRUE))
    {
      DEBUG ((DEBUG_INFO, "VarName %s Attr 0x%x \n", VarName, Attributes));
      if ((Attributes & EFI_VARIABLE_APPEND_WRITE) == EFI_VARIABLE_APPEND_WRITE) {
        AppendWrite = TRUE;
      } else {
        AppendWrite = FALSE;
      }

      /*
       * If there is an attempt to create a volatile variable, skip it.
       */
      if ((Attributes & EFI_VARIABLE_NON_VOLATILE) == 0) {
        DEBUG ((
          DEBUG_INFO,
          "Don't add volatile Variable %s skip\n",
          SecureVarList[Index].VarName
          ));
        continue;
      }

      /*
       * If we're replacing the variable, just use the new variable contents.
       * If the SetVariable get calls with an AppendWrite to create a new var
       * use the new variable contents.
       * If the Payload is NULL (Delete), skip this Variable.
       */
      if (((Attributes & EFI_VARIABLE_APPEND_WRITE) == 0) ||
          ((DoesVariableExist (VarName, VarGuid, NULL, NULL) == FALSE) && (AppendWrite == TRUE)))
      {
        /* Replacing the existing Variable Contents */
        if (PayloadSize != 0) {
          DEBUG ((
            DEBUG_INFO,
            "Updating %s with new value Size %u \n",
            SecureVarList[Index].VarName,
            PayloadSize
            ));
          if (HashApiUpdate (HashContext, PayloadPtr, PayloadSize) != TRUE) {
            DEBUG ((
              DEBUG_ERROR,
              "%a:%d Failed to update Hash \n",
              __FUNCTION__,
              __LINE__
              ));
            Status = EFI_UNSUPPORTED;
            goto ExitMeasureSecureBootVars;
          }
        } else {
          /* If this is a Var Delete cleanup any buffer allocated before */
          SecureVarList[Index].Size = 0;
          if (SecureVarList[Index].Data != NULL) {
            FreePool (SecureVarList[Index].Data);
            SecureVarList[Index].Data = NULL;
          }
        }

        continue;
      }
    }

    VarSize = 0;
    if ((DoesVariableExist (
           SecureVarList[Index].VarName,
           SecureVarList[Index].VarGuid,
           &VarSize,
           &SecureVarList[Index].Attr
           ) == TRUE))
    {
      if ((SecureVarList[Index].Attr & EFI_VARIABLE_NON_VOLATILE) == 0) {
        DEBUG ((
          DEBUG_INFO,
          "Variable %s is Volatile skip\n",
          SecureVarList[Index].VarName
          ));
        continue;
      }

      /* The Variable Exists on flash, before reading it
       * check if our data buffer is large enough to hold it.
       */
      if ((VarSize != SecureVarList[Index].Size) ||
          (SecureVarList[Index].Data == NULL))
      {
        if (SecureVarList[Index].Data != NULL) {
          FreePool (SecureVarList[Index].Data);
          SecureVarList[Index].Data = NULL;
        }

        Status = MmGetVariable3 (
                   SecureVarList[Index].VarName,
                   SecureVarList[Index].VarGuid,
                   &SecureVarList[Index].Data,
                   &SecureVarList[Index].Size,
                   &SecureVarList[Index].Attr
                   );
        NV_ASSERT_EFI_ERROR_RETURN (Status, goto ExitMeasureSecureBootVars);
      } else {
        ZeroMem (SecureVarList[Index].Data, VarSize);
        Status = MmGetVariable (
                   SecureVarList[Index].VarName,
                   SecureVarList[Index].VarGuid,
                   SecureVarList[Index].Data,
                   SecureVarList[Index].Size
                   );
        NV_ASSERT_EFI_ERROR_RETURN (Status, goto ExitMeasureSecureBootVars);
      }
    } else {
      DEBUG ((
        DEBUG_INFO,
        "%a: Failed to GetVariable %s %r\n",
        __FUNCTION__,
        SecureVarList[Index].VarName,
        Status
        ));
      Status = EFI_SUCCESS;
      continue;
    }

    /* If this is an Append Write to a SecureDb Variable.
     * Then ensure that there are no duplicate signatures
     * in the data being appended. Note we've probably over
     * allocated memory for this variable, but let this be for
     * now.
     */
    if (AppendWrite == TRUE) {
      CurPtr = AllocateRuntimeZeroPool (PayloadSize);
      if (CurPtr == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitMeasureSecureBootVars, "%a: Failed to Allocate Buf - %r", __FUNCTION__, Status);
      }

      CopyMem (CurPtr, PayloadPtr, PayloadSize);
      DEBUG ((
        DEBUG_INFO,
        "Removing Duplicates: Orig %u Payload %u\n",
        VarSize,
        PayloadSize
        ));
      Status = RemoveDupSignatureList (
                 SecureVarList[Index].Data,
                 VarSize,
                 CurPtr,
                 &PayloadSize
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to filter out %r\n",
          __FUNCTION__,
          Status
          ));
      }

      /* After removing duplicates check if there are any new signatures
       * to be added.
       */
      if (PayloadSize != 0) {
        SecureVarList[Index].Size = VarSize + PayloadSize;
        SecureVarList[Index].Data = ReallocateRuntimePool (
                                      VarSize,
                                      SecureVarList[Index].Size,
                                      SecureVarList[Index].Data
                                      );
        CopyPtr  = SecureVarList[Index].Data;
        CopyPtr += VarSize;
        CopyMem (CopyPtr, CurPtr, PayloadSize);
      }

      FreePool (CurPtr);
    }

    DEBUG ((
      DEBUG_INFO,
      "%a: Adding %s Size %u\n",
      __FUNCTION__,
      SecureVarList[Index].VarName,
      SecureVarList[Index].Size
      ));
    if (HashApiUpdate (HashContext, SecureVarList[Index].Data, SecureVarList[Index].Size) != TRUE) {
      DEBUG ((DEBUG_ERROR, "Failed to update Hash %r\n", Status));
      Status = EFI_UNSUPPORTED;
      goto ExitMeasureSecureBootVars;
    }
  }

ExitMeasureSecureBootVars:
  return Status;
}

EFI_STATUS
EFIAPI
MeasureSecureDbVars (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL
  )
{
  return MeasureSecureDbVarsInternal (
           VarName,
           VarGuid,
           Attributes,
           Data,
           DataSize,
           SecureVarsV0,
           sizeof (SecureVarsV0) / sizeof (SecureVarsV0[0])
           );
}

STATIC
EFI_STATUS
PreparePendingUpdateData (
  IN  VOID     *CurrentData,
  IN  UINTN    CurrentDataSize,
  IN  UINT32   CurrentAttributes,
  IN  UINT32   UpdateAttributes,
  IN  VOID     *UpdateData,
  IN  UINTN    UpdateDataSize,
  OUT VOID     **PendingData,
  OUT UINTN    *PendingDataSize,
  OUT UINT32   *PendingAttributes,
  OUT BOOLEAN  *PendingDataAllocated
  )
{
  UINT8  *MergedData;

  if ((PendingData == NULL) || (PendingDataSize == NULL) ||
      (PendingAttributes == NULL) || (PendingDataAllocated == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  *PendingData          = NULL;
  *PendingDataSize      = 0;
  *PendingAttributes    = UpdateAttributes & ~EFI_VARIABLE_APPEND_WRITE;
  *PendingDataAllocated = FALSE;

  if (IsDeleteRequest (UpdateAttributes, UpdateDataSize) == TRUE) {
    return EFI_SUCCESS;
  }

  if ((UpdateAttributes & EFI_VARIABLE_APPEND_WRITE) == 0) {
    *PendingData     = UpdateData;
    *PendingDataSize = UpdateDataSize;
    return EFI_SUCCESS;
  }

  *PendingAttributes = CurrentAttributes;
  if (CurrentDataSize > (MAX_UINTN - UpdateDataSize)) {
    return EFI_OUT_OF_RESOURCES;
  }

  *PendingDataSize = CurrentDataSize + UpdateDataSize;
  if (*PendingDataSize == 0) {
    return EFI_SUCCESS;
  }

  MergedData = AllocatePool (*PendingDataSize);
  if (MergedData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (CurrentDataSize != 0) {
    CopyMem (MergedData, CurrentData, CurrentDataSize);
  }

  if (UpdateDataSize != 0) {
    CopyMem (&MergedData[CurrentDataSize], UpdateData, UpdateDataSize);
  }

  *PendingData          = MergedData;
  *PendingDataAllocated = TRUE;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
MeasureNvVarsForComponent (
  IN  NV_VAR_MEASURE_COMPONENT  Component,
  IN  CHAR16                    *VarName   OPTIONAL,
  IN  EFI_GUID                  *VarGuid   OPTIONAL,
  IN  UINT32                    Attributes OPTIONAL,
  IN  VOID                      *Data      OPTIONAL,
  IN  UINTN                     DataSize   OPTIONAL,
  OUT UINTN                     *MeasuredCount OPTIONAL
  )
{
  EFI_STATUS                 Status;
  EFI_SMM_VARIABLE_PROTOCOL  *SmmVariable;
  CHAR16                     *NameBuffer;
  UINTN                      NameBufferSize;
  UINTN                      NextNameSize;
  EFI_GUID                   NextGuid;
  NV_VAR_MEASURE_DESCRIPTOR  *DescriptorList;
  UINTN                      DescriptorCount;
  UINTN                      DescriptorCapacity;
  VOID                       *VariableData;
  UINTN                      VariableDataSize;
  UINT32                     VariableAttributes;
  BOOLEAN                    MatchedUpdate;
  BOOLEAN                    PendingUpdate;
  VOID                       *PendingData;
  UINTN                      PendingDataSize;
  UINT32                     PendingAttributes;
  BOOLEAN                    PendingDataAllocated;
  CHAR16                     *NewNameBuffer;

  if (MeasuredCount != NULL) {
    *MeasuredCount = 0;
  }

  NameBuffer           = NULL;
  DescriptorList       = NULL;
  DescriptorCount      = 0;
  DescriptorCapacity   = 0;
  VariableData         = NULL;
  PendingData          = NULL;
  PendingDataAllocated = FALSE;
  PendingUpdate        = FALSE;
  MatchedUpdate        = FALSE;
  PendingDataSize      = 0;
  PendingAttributes    = Attributes & ~EFI_VARIABLE_APPEND_WRITE;

  Status = GetSmmVariableProtocol (&SmmVariable);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NameBufferSize = VAR_INT_INITIAL_NAME_BUFFER_SIZE;
  NameBuffer     = AllocateZeroPool (NameBufferSize);
  if (NameBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ZeroMem (&NextGuid, sizeof (NextGuid));

  while (TRUE) {
    NextNameSize = NameBufferSize;
    Status       = SmmVariable->SmmGetNextVariableName (
                                  &NextNameSize,
                                  NameBuffer,
                                  &NextGuid
                                  );
    if (Status == EFI_BUFFER_TOO_SMALL) {
      NewNameBuffer = AllocatePool (NextNameSize);
      if (NewNameBuffer == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto ExitMeasureNvVarsForComponent;
      }

      CopyMem (NewNameBuffer, NameBuffer, MIN (NameBufferSize, NextNameSize));
      FreePool (NameBuffer);
      NameBuffer     = NewNameBuffer;
      NameBufferSize = NextNameSize;
      continue;
    }

    if (Status == EFI_NOT_FOUND) {
      Status = EFI_SUCCESS;
      break;
    }

    if (EFI_ERROR (Status)) {
      goto ExitMeasureNvVarsForComponent;
    }

    if (NvVarIntIsExcludedVar (NameBuffer, &NextGuid) == TRUE) {
      continue;
    }

    VariableData       = NULL;
    VariableDataSize   = 0;
    VariableAttributes = 0;
    Status             = GetVariableInfo (
                           SmmVariable,
                           NameBuffer,
                           &NextGuid,
                           &VariableDataSize,
                           &VariableAttributes
                           );
    if (Status == EFI_NOT_FOUND) {
      Status = EFI_SUCCESS;
      continue;
    }

    if (EFI_ERROR (Status)) {
      goto ExitMeasureNvVarsForComponent;
    }

    if (IsSameVariable (NameBuffer, &NextGuid, VarName, VarGuid) == TRUE) {
      MatchedUpdate = TRUE;
      if (((Attributes & EFI_VARIABLE_APPEND_WRITE) != 0) &&
          (IsDeleteRequest (Attributes, DataSize) == FALSE) &&
          (IsMeasurableNvVarForComponent (VariableAttributes, Component) == TRUE))
      {
        Status = GetVariableData (
                   SmmVariable,
                   NameBuffer,
                   &NextGuid,
                   &VariableData,
                   &VariableDataSize,
                   &VariableAttributes
                   );
        if (Status == EFI_NOT_FOUND) {
          Status = EFI_SUCCESS;
          continue;
        }

        if (EFI_ERROR (Status)) {
          goto ExitMeasureNvVarsForComponent;
        }
      }

      if (IsDeleteRequest (Attributes, DataSize) == FALSE) {
        Status = PreparePendingUpdateData (
                   VariableData,
                   VariableDataSize,
                   VariableAttributes,
                   Attributes,
                   Data,
                   DataSize,
                   &PendingData,
                   &PendingDataSize,
                   &PendingAttributes,
                   &PendingDataAllocated
                   );
        if (EFI_ERROR (Status)) {
          goto ExitMeasureNvVarsForComponent;
        }

        PendingUpdate = IsMeasurableNvVarForComponent (PendingAttributes, Component);
        if (PendingUpdate == TRUE) {
          Status = AddNvVarMeasureDescriptor (
                     &DescriptorList,
                     &DescriptorCount,
                     &DescriptorCapacity,
                     NameBuffer,
                     &NextGuid,
                     PendingAttributes,
                     PendingDataSize,
                     PendingData,
                     FALSE
                     );
          if (EFI_ERROR (Status)) {
            goto ExitMeasureNvVarsForComponent;
          }

          PendingUpdate = FALSE;
        }
      }
    } else {
      if (IsMeasurableNvVarForComponent (VariableAttributes, Component) == FALSE) {
        continue;
      }

      Status = GetVariableData (
                 SmmVariable,
                 NameBuffer,
                 &NextGuid,
                 &VariableData,
                 &VariableDataSize,
                 &VariableAttributes
                 );
      if (Status == EFI_NOT_FOUND) {
        Status = EFI_SUCCESS;
        continue;
      }

      if (EFI_ERROR (Status)) {
        goto ExitMeasureNvVarsForComponent;
      }

      if (IsMeasurableNvVarForComponent (VariableAttributes, Component) == FALSE) {
        if (VariableData != NULL) {
          FreePool (VariableData);
          VariableData = NULL;
        }

        continue;
      }

      Status = AddNvVarMeasureDescriptor (
                 &DescriptorList,
                 &DescriptorCount,
                 &DescriptorCapacity,
                 NameBuffer,
                 &NextGuid,
                 VariableAttributes,
                 VariableDataSize,
                 VariableData,
                 TRUE
                 );
      if (EFI_ERROR (Status)) {
        goto ExitMeasureNvVarsForComponent;
      }

      VariableData = NULL;
    }

    if (VariableData != NULL) {
      FreePool (VariableData);
      VariableData = NULL;
    }
  }

  if ((VarName != NULL) && (VarGuid != NULL) &&
      (NvVarIntIsExcludedVar (VarName, VarGuid) == FALSE))
  {
    if ((MatchedUpdate == FALSE) &&
        (IsDeleteRequest (Attributes, DataSize) == FALSE) &&
        (IsMeasurableNvVarForComponent (Attributes & ~EFI_VARIABLE_APPEND_WRITE, Component) == TRUE))
    {
      PendingUpdate     = TRUE;
      PendingData       = Data;
      PendingDataSize   = DataSize;
      PendingAttributes = Attributes & ~EFI_VARIABLE_APPEND_WRITE;
    }

    if (PendingUpdate == TRUE) {
      Status = AddNvVarMeasureDescriptor (
                 &DescriptorList,
                 &DescriptorCount,
                 &DescriptorCapacity,
                 VarName,
                 VarGuid,
                 PendingAttributes,
                 PendingDataSize,
                 PendingData,
                 FALSE
                 );
      if (EFI_ERROR (Status)) {
        goto ExitMeasureNvVarsForComponent;
      }
    }
  }

  SortNvVarMeasureDescriptors (DescriptorList, DescriptorCount);
  if (MeasuredCount != NULL) {
    *MeasuredCount = DescriptorCount;
  }

  if (VarName == NULL) {
    LogNvVarMeasureDescriptors (
      Component,
      VarName,
      VarGuid,
      DescriptorList,
      DescriptorCount
      );
  }

  Status = HashNvVarMeasureDescriptors (
             DescriptorList,
             DescriptorCount
             );

ExitMeasureNvVarsForComponent:
  if (VariableData != NULL) {
    FreePool (VariableData);
  }

  if (PendingDataAllocated == TRUE) {
    FreePool (PendingData);
  }

  if (NameBuffer != NULL) {
    FreePool (NameBuffer);
  }

  FreeNvVarMeasureDescriptors (DescriptorList, DescriptorCount);

  return Status;
}

STATIC
VOID
LogNvVarComponentHash (
  IN CONST CHAR8               *Action,
  IN NV_VAR_MEASURE_COMPONENT  Component,
  IN UINTN                     Count,
  IN UINT8                     *Digest
  )
{
  if ((Action == NULL) || (Digest == NULL)) {
    return;
  }

  DEBUG ((
    DEBUG_ERROR,
    "VarIntComponentHash: action=%a component=%a count=%lu digest=%02x%02x%02x%02x%02x%02x%02x%02x\n",
    Action,
    NvVarMeasureComponentName (Component),
    (UINT64)Count,
    Digest[0],
    Digest[1],
    Digest[2],
    Digest[3],
    Digest[4],
    Digest[5],
    Digest[6],
    Digest[7]
    ));
}

STATIC
EFI_STATUS
ComputeNvVarComponentHash (
  IN  NV_VAR_MEASURE_COMPONENT  Component,
  IN  CHAR16                    *VarName   OPTIONAL,
  IN  EFI_GUID                  *VarGuid   OPTIONAL,
  IN  UINT32                    Attributes OPTIONAL,
  IN  VOID                      *Data      OPTIONAL,
  IN  UINTN                     DataSize   OPTIONAL,
  OUT UINT8                     *Digest,
  OUT UINTN                     *MeasuredCount OPTIONAL
  )
{
  EFI_STATUS  Status;

  if (Digest == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EnsureHashContext ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Not enough resources to allocate HashContext %r\n", __FUNCTION__, Status));
    return Status;
  }

  if (HashApiInit (HashContext) == FALSE) {
    DEBUG ((DEBUG_ERROR, "%a: HashApiInit Failed\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  Status = MeasureNvVarsForComponent (
             Component,
             VarName,
             VarGuid,
             Attributes,
             Data,
             DataSize,
             MeasuredCount
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (HashApiFinal (HashContext, Digest) == FALSE) {
    DEBUG ((DEBUG_ERROR, "%a: Finalizing component hash failed\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetExistingNvVarAttributes (
  IN  CHAR16    *VarName,
  IN  EFI_GUID  *VarGuid,
  OUT UINT32    *Attributes,
  OUT BOOLEAN   *Found
  )
{
  EFI_STATUS                 Status;
  EFI_SMM_VARIABLE_PROTOCOL  *SmmVariable;
  UINTN                      DataSize;

  if ((VarName == NULL) || (VarGuid == NULL) || (Attributes == NULL) || (Found == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Attributes = 0;
  *Found      = FALSE;
  DataSize    = 0;

  Status = GetSmmVariableProtocol (&SmmVariable);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = GetVariableInfo (
             SmmVariable,
             VarName,
             VarGuid,
             &DataSize,
             Attributes
             );
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  *Found = TRUE;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ComputeAllNvVarMeasurement (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL,
  OUT UINT8     *Meas
  )
{
  EFI_STATUS  Status;
  UINT8       BootServiceOnlyHash[NV_VAR_INT_MAX_DIGEST_SIZE];
  UINT8       ComponentHash[NV_VAR_INT_MAX_DIGEST_SIZE];
  UINT8       RuntimeHash[NV_VAR_INT_MAX_DIGEST_SIZE];
  UINTN       BootServiceOnlyCount;
  UINTN       ComponentCount;
  UINTN       DigestSize;
  UINT32      ExistingAttributes;
  UINT32      PendingAttributes;
  UINTN       RuntimeCount;
  BOOLEAN     BootServiceOnlyHashValid;
  BOOLEAN     ExistingFound;
  BOOLEAN     UpdateBootServiceOnly;
  BOOLEAN     UpdateRuntime;
  BOOLEAN     DeleteRequest;
  BOOLEAN     FullRecompute;
  BOOLEAN     RuntimeHashValid;

  if (Meas == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  FullRecompute = (BOOLEAN)(VarName == NULL);

  Status = GetActiveDigestSize (&DigestSize);
  if (EFI_ERROR (Status)) {
    goto ExitComputeAllNvVarMeasurement;
  }

  ZeroMem (BootServiceOnlyHash, sizeof (BootServiceOnlyHash));
  ZeroMem (RuntimeHash, sizeof (RuntimeHash));
  BootServiceOnlyCount     = mNvBootServiceOnlyCount;
  RuntimeCount             = mNvRuntimeCount;
  BootServiceOnlyHashValid = mNvBootServiceOnlyHashValid;
  RuntimeHashValid         = mNvRuntimeHashValid;

  if (BootServiceOnlyHashValid == TRUE) {
    CopyMem (BootServiceOnlyHash, mNvBootServiceOnlyHash, DigestSize);
  }

  if (RuntimeHashValid == TRUE) {
    CopyMem (RuntimeHash, mNvRuntimeHash, DigestSize);
  }

  Status = EnsureHashContext ();
  if (EFI_ERROR (Status)) {
    NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitComputeAllNvVarMeasurement, "%a: Not Enough Resources to allocate HashContext - %r", __FUNCTION__, Status);
    goto ExitComputeAllNvVarMeasurement;
  }

  if (VarName == NULL) {
    if (mNvVarIntAfterExitBootServices == TRUE) {
      if (BootServiceOnlyHashValid == FALSE) {
        DEBUG ((
          DEBUG_ERROR,
          "VarIntCombinedHash: cannot preserve boot-service-only component after EBS because cache is invalid\n"
          ));
        Status = EFI_NOT_READY;
        goto ExitComputeAllNvVarMeasurement;
      }

      DEBUG ((
        DEBUG_INFO,
        "VarIntCombinedHash: preserving boot-service-only component after EBS count=%lu\n",
        (UINT64)BootServiceOnlyCount
        ));
    } else {
      ComponentCount = 0;
      Status         = ComputeNvVarComponentHash (
                         NvVarMeasureComponentBootServiceOnly,
                         NULL,
                         NULL,
                         0,
                         NULL,
                         0,
                         ComponentHash,
                         &ComponentCount
                         );
      if (EFI_ERROR (Status)) {
        goto ExitComputeAllNvVarMeasurement;
      }

      CopyMem (BootServiceOnlyHash, ComponentHash, DigestSize);
      BootServiceOnlyCount     = ComponentCount;
      BootServiceOnlyHashValid = TRUE;
      LogNvVarComponentHash (
        "update",
        NvVarMeasureComponentBootServiceOnly,
        BootServiceOnlyCount,
        BootServiceOnlyHash
        );
    }

    ComponentCount = 0;
    Status         = ComputeNvVarComponentHash (
                       NvVarMeasureComponentRuntime,
                       NULL,
                       NULL,
                       0,
                       NULL,
                       0,
                       ComponentHash,
                       &ComponentCount
                       );
    if (EFI_ERROR (Status)) {
      goto ExitComputeAllNvVarMeasurement;
    }

    CopyMem (RuntimeHash, ComponentHash, DigestSize);
    RuntimeCount     = ComponentCount;
    RuntimeHashValid = TRUE;
    LogNvVarComponentHash (
      "update",
      NvVarMeasureComponentRuntime,
      RuntimeCount,
      RuntimeHash
      );
  } else {
    ExistingAttributes    = 0;
    ExistingFound         = FALSE;
    DeleteRequest         = IsDeleteRequest (Attributes, DataSize);
    PendingAttributes     = Attributes & ~EFI_VARIABLE_APPEND_WRITE;
    UpdateBootServiceOnly = FALSE;
    UpdateRuntime         = FALSE;

    if (VarGuid != NULL) {
      Status = GetExistingNvVarAttributes (
                 VarName,
                 VarGuid,
                 &ExistingAttributes,
                 &ExistingFound
                 );
      if (EFI_ERROR (Status)) {
        goto ExitComputeAllNvVarMeasurement;
      }
    }

    if (((Attributes & EFI_VARIABLE_APPEND_WRITE) != 0) && (ExistingFound == TRUE)) {
      PendingAttributes = ExistingAttributes;
    }

    if (ExistingFound == TRUE) {
      UpdateBootServiceOnly = IsMeasurableNvVarForComponent (
                                ExistingAttributes,
                                NvVarMeasureComponentBootServiceOnly
                                );
      UpdateRuntime = IsMeasurableNvVarForComponent (
                        ExistingAttributes,
                        NvVarMeasureComponentRuntime
                        );
    }

    if (DeleteRequest == FALSE) {
      UpdateBootServiceOnly = (BOOLEAN)(
                                        (UpdateBootServiceOnly == TRUE) ||
                                        (IsMeasurableNvVarForComponent (
                                           PendingAttributes,
                                           NvVarMeasureComponentBootServiceOnly
                                           ) == TRUE)
                                        );
      UpdateRuntime = (BOOLEAN)(
                                (UpdateRuntime == TRUE) ||
                                (IsMeasurableNvVarForComponent (
                                   PendingAttributes,
                                   NvVarMeasureComponentRuntime
                                   ) == TRUE)
                                );
    }

    if ((UpdateBootServiceOnly == TRUE) || (BootServiceOnlyHashValid == FALSE)) {
      if (mNvVarIntAfterExitBootServices == TRUE) {
        DEBUG ((
          DEBUG_ERROR,
          "VarIntCombinedHash: cannot recompute boot-service-only component after EBS update=%u cache_valid=%u\n",
          UpdateBootServiceOnly,
          BootServiceOnlyHashValid
          ));
        Status = EFI_NOT_READY;
        goto ExitComputeAllNvVarMeasurement;
      }

      ComponentCount = 0;
      Status         = ComputeNvVarComponentHash (
                         NvVarMeasureComponentBootServiceOnly,
                         UpdateBootServiceOnly ? VarName : NULL,
                         UpdateBootServiceOnly ? VarGuid : NULL,
                         UpdateBootServiceOnly ? Attributes : 0,
                         UpdateBootServiceOnly ? Data : NULL,
                         UpdateBootServiceOnly ? DataSize : 0,
                         ComponentHash,
                         &ComponentCount
                         );
      if (EFI_ERROR (Status)) {
        goto ExitComputeAllNvVarMeasurement;
      }

      CopyMem (BootServiceOnlyHash, ComponentHash, DigestSize);
      BootServiceOnlyCount     = ComponentCount;
      BootServiceOnlyHashValid = TRUE;
    }

    if ((UpdateRuntime == TRUE) || (RuntimeHashValid == FALSE)) {
      ComponentCount = 0;
      Status         = ComputeNvVarComponentHash (
                         NvVarMeasureComponentRuntime,
                         UpdateRuntime ? VarName : NULL,
                         UpdateRuntime ? VarGuid : NULL,
                         UpdateRuntime ? Attributes : 0,
                         UpdateRuntime ? Data : NULL,
                         UpdateRuntime ? DataSize : 0,
                         ComponentHash,
                         &ComponentCount
                         );
      if (EFI_ERROR (Status)) {
        goto ExitComputeAllNvVarMeasurement;
      }

      CopyMem (RuntimeHash, ComponentHash, DigestSize);
      RuntimeCount     = ComponentCount;
      RuntimeHashValid = TRUE;
    }
  }

  if ((BootServiceOnlyHashValid == FALSE) || (RuntimeHashValid == FALSE)) {
    Status = EFI_NOT_READY;
    goto ExitComputeAllNvVarMeasurement;
  }

  if (HashApiInit (HashContext) == FALSE) {
    DEBUG ((DEBUG_ERROR, "%a: HashApiInit Failed\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ExitComputeAllNvVarMeasurement;
  }

  Status = HashUpdateChecked (BootServiceOnlyHash, DigestSize);
  if (EFI_ERROR (Status)) {
    goto ExitComputeAllNvVarMeasurement;
  }

  Status = HashUpdateChecked (RuntimeHash, DigestSize);
  if (EFI_ERROR (Status)) {
    goto ExitComputeAllNvVarMeasurement;
  }

  if (HashApiFinal (HashContext, Meas) == FALSE) {
    DEBUG ((DEBUG_ERROR, "%a: Finalizing Hash Failed\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto ExitComputeAllNvVarMeasurement;
  }

  if (FullRecompute == TRUE) {
    CopyMem (mNvBootServiceOnlyHash, BootServiceOnlyHash, DigestSize);
    mNvBootServiceOnlyCount     = BootServiceOnlyCount;
    mNvBootServiceOnlyHashValid = BootServiceOnlyHashValid;
    CopyMem (mNvRuntimeHash, RuntimeHash, DigestSize);
    mNvRuntimeCount     = RuntimeCount;
    mNvRuntimeHashValid = RuntimeHashValid;

    DEBUG ((
      DEBUG_ERROR,
      "VarIntCombinedHash: bs_count=%lu rt_count=%lu payload=%02x%02x%02x%02x%02x%02x%02x%02x\n",
      (UINT64)mNvBootServiceOnlyCount,
      (UINT64)mNvRuntimeCount,
      Meas[0],
      Meas[1],
      Meas[2],
      Meas[3],
      Meas[4],
      Meas[5],
      Meas[6],
      Meas[7]
      ));
  }

  Status = EFI_SUCCESS;

ExitComputeAllNvVarMeasurement:
  return Status;
}

/*
 * ComputeVarMeasurementInternal
 * Legacy V0 helper to compute the original boot/security variable measurement.
 * This function can be called during a pre-update variable call or to compute
 * the measurement of the stored variables during boot.
 *
 * @param[in]  VarInt      Variable Integrity number.
 * @param[in]  VarName     Name of the variable being updated.
 * @param[in]  VarGuid     GUID of the variable.
 * @param[in]  Attributes  Attributes of the variable.
 * @param[in]  *Data       Variable Data being updated.
 * @param[out] Meas        New measurement computed.
 *
 * @retval    EFI_SUCCESS   computed the measurement.
 *            Other         failed to compute a valid measurement.
 */
STATIC
EFI_STATUS
ComputeVarMeasurementInternal (
  IN  CHAR16            *VarName   OPTIONAL,
  IN  EFI_GUID          *VarGuid   OPTIONAL,
  IN  UINT32            Attributes OPTIONAL,
  IN  VOID              *Data      OPTIONAL,
  IN  UINTN             DataSize   OPTIONAL,
  IN  MEASURE_VAR_TYPE  *SecureVarList,
  IN  UINTN             SecureVarCount,
  IN  BOOLEAN           IncludeBootNext,
  OUT UINT8             *Meas
  )
{
  EFI_STATUS  Status;

  if (HashContext == NULL) {
    HashContext = AllocateRuntimeZeroPool (HashApiGetContextSize ());
    if (HashContext == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      NV_ASSERT_RETURN (!EFI_ERROR (Status), goto ExitComputeVarMeasurement, "%a: Not Enough Resources to allocate HashContext - %r", __FUNCTION__, Status);
    }
  }

  if (HashApiInit (HashContext) == FALSE) {
    DEBUG ((DEBUG_ERROR, "%a: HashApiInit Failed\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ExitComputeVarMeasurement;
  }

  Status = MeasureBootVars (VarName, VarGuid, Attributes, Data, DataSize);
  NV_ASSERT_EFI_ERROR_RETURN (Status, goto ExitComputeVarMeasurement);
  if (IncludeBootNext == TRUE) {
    Status = MeasureBootNextVar (VarName, VarGuid, Attributes, Data, DataSize);
    NV_ASSERT_EFI_ERROR_RETURN (Status, goto ExitComputeVarMeasurement);
  }

  Status = MeasureSecureDbVarsInternal (
             VarName,
             VarGuid,
             Attributes,
             Data,
             DataSize,
             SecureVarList,
             SecureVarCount
             );
  NV_ASSERT_EFI_ERROR_RETURN (Status, goto ExitComputeVarMeasurement);

  if (HashApiFinal (HashContext, Meas) == FALSE) {
    DEBUG ((DEBUG_ERROR, "Finalizing Hash Failed\n"));
    Status = EFI_DEVICE_ERROR;
    goto ExitComputeVarMeasurement;
  }

  Status = EFI_SUCCESS;

ExitComputeVarMeasurement:
  if ((BootOrder != NULL) && (BootOrder != Data)) {
    FreePool (BootOrder);
  }

  BootOrder = NULL;
  BootCount = 0;

  return Status;
}

EFI_STATUS
EFIAPI
ComputeVarMeasurementV0 (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL,
  OUT UINT8     *Meas
  )
{
  return ComputeVarMeasurementInternal (
           VarName,
           VarGuid,
           Attributes,
           Data,
           DataSize,
           SecureVarsV0,
           sizeof (SecureVarsV0) / sizeof (SecureVarsV0[0]),
           FALSE,
           Meas
           );
}

EFI_STATUS
EFIAPI
ComputeVarMeasurementV1 (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL,
  OUT UINT8     *Meas
  )
{
  return ComputeAllNvVarMeasurement (
           VarName,
           VarGuid,
           Attributes,
           Data,
           DataSize,
           Meas
           );
}

EFI_STATUS
EFIAPI
ComputeVarMeasurement (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL,
  OUT UINT8     *Meas
  )
{
  return ComputeVarMeasurementV1 (
           VarName,
           VarGuid,
           Attributes,
           Data,
           DataSize,
           Meas
           );
}
