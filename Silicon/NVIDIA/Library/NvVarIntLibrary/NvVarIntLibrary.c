/** @file

  Library to compute the measurement of the Boot and Security Variables.

  The APIs can be called during a variable update (before the FVB Write) or
  at bootup to measure the variables on flash.

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/MmServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/HashApiLib.h>
#include <Library/BaseCryptLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Guid/ImageAuthentication.h>
#include <Protocol/SmmVariable.h>
#include <IndustryStandard/Tpm20.h>
#include <Library/AuthVariableLib.h>
#include <Library/PrintLib.h>
#include <Library/MmVarLib.h>

#define HEADER_SZ_BYTES  (1)

typedef struct {
  CHAR16      *VarName;
  EFI_GUID    *VarGuid;
  VOID        *Data;
  UINTN       Size;
  UINT32      Attr;
} MEASURE_VAR_TYPE;

STATIC HASH_API_CONTEXT  HashContext   = NULL;
STATIC VOID              **BootOptions = NULL;
STATIC UINTN             BootCount     = 0;
STATIC UINT16            *BootOrder;

STATIC MEASURE_VAR_TYPE  SecureVars[] = {
  { EFI_SECURE_BOOT_MODE_NAME,    &gEfiGlobalVariableGuid,        NULL, 0 },
  { EFI_PLATFORM_KEY_NAME,        &gEfiGlobalVariableGuid,        NULL, 0 },
  { EFI_KEY_EXCHANGE_KEY_NAME,    &gEfiGlobalVariableGuid,        NULL, 0 },
  { EFI_IMAGE_SECURITY_DATABASE,  &gEfiImageSecurityDatabaseGuid, NULL, 0 },
  { EFI_IMAGE_SECURITY_DATABASE1, &gEfiImageSecurityDatabaseGuid, NULL, 0 }
};

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
  ASSERT (BootOptions != NULL);

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
  }

ExitMeasureBootVars:
  if ((UpdatingBootOrder == TRUE)) {
    BootOrder = NULL;
    BootCount = 0;
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
EFI_STATUS
MeasureSecureDbVars (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL
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

  PayloadPtr  = Data;
  PayloadSize = DataSize;
  AppendWrite = FALSE;
  for (Index = 0; Index < (sizeof (SecureVars) / sizeof (SecureVars[0])); Index++) {
    DEBUG ((
      DEBUG_INFO,
      "%a: First add %s VarName \n",
      __FUNCTION__,
      SecureVars[Index].VarName
      ));
    if (HashApiUpdate (HashContext, SecureVars[Index].VarName, sizeof (SecureVars[Index].VarName)) != TRUE) {
      DEBUG ((DEBUG_ERROR, "%a:%d Failed to update Hash\n", __FUNCTION__, __LINE__));
      Status = EFI_UNSUPPORTED;
      goto ExitMeasureSecureBootVars;
    }

    if (HashApiUpdate (HashContext, SecureVars[Index].VarGuid, sizeof (SecureVars[Index].VarGuid)) != TRUE ) {
      DEBUG ((DEBUG_ERROR, "%a:%d Failed to update Hash\n", __FUNCTION__, __LINE__));
      Status = EFI_UNSUPPORTED;
      goto ExitMeasureSecureBootVars;
    }

    /* If this SetVariable call is to a variable we're monitoring and if its a
     * Write then use the new Data.
     * For an Append Write, setup a new buffer and update the hash.
     */
    if ((VarName != NULL) && (VarGuid != NULL) &&
        (StrCmp (SecureVars[Index].VarName, VarName) == 0) &&
        (CompareGuid (VarGuid, SecureVars[Index].VarGuid) == TRUE))
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
          SecureVars[Index].VarName
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
            SecureVars[Index].VarName,
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
          SecureVars[Index].Size = 0;
          if (SecureVars[Index].Data != NULL) {
            FreePool (SecureVars[Index].Data);
            SecureVars[Index].Data = NULL;
          }
        }

        continue;
      }
    }

    VarSize = 0;
    if ((DoesVariableExist (
           SecureVars[Index].VarName,
           SecureVars[Index].VarGuid,
           &VarSize,
           &SecureVars[Index].Attr
           ) == TRUE))
    {
      if ((SecureVars[Index].Attr & EFI_VARIABLE_NON_VOLATILE) == 0) {
        DEBUG ((
          DEBUG_INFO,
          "Variable %s is Volatile skip\n",
          SecureVars[Index].VarName
          ));
        continue;
      }

      /* The Variable Exists on flash, before reading it
       * check if our data buffer is large enough to hold it.
       */
      if ((VarSize != SecureVars[Index].Size) ||
          (SecureVars[Index].Data == NULL))
      {
        if (SecureVars[Index].Data != NULL) {
          FreePool (SecureVars[Index].Data);
          SecureVars[Index].Data = NULL;
        }

        Status = MmGetVariable3 (
                   SecureVars[Index].VarName,
                   SecureVars[Index].VarGuid,
                   &SecureVars[Index].Data,
                   &SecureVars[Index].Size,
                   &SecureVars[Index].Attr
                   );
        ASSERT_EFI_ERROR (Status);
      } else {
        ZeroMem (SecureVars[Index].Data, VarSize);
        Status = MmGetVariable (
                   SecureVars[Index].VarName,
                   SecureVars[Index].VarGuid,
                   SecureVars[Index].Data,
                   SecureVars[Index].Size
                   );
        ASSERT_EFI_ERROR (Status);
      }
    } else {
      DEBUG ((
        DEBUG_INFO,
        "%a: Failed to GetVariable %s %r\n",
        __FUNCTION__,
        SecureVars[Index].VarName,
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
        DEBUG ((DEBUG_ERROR, "Failed to Allocate Buf\n"));
        ASSERT (FALSE);
      }

      CopyMem (CurPtr, PayloadPtr, PayloadSize);
      DEBUG ((
        DEBUG_INFO,
        "Removing Duplicates: Orig %u Payload %u\n",
        VarSize,
        PayloadSize
        ));
      Status = RemoveDupSignatureList (
                 SecureVars[Index].Data,
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
        SecureVars[Index].Size = VarSize + PayloadSize;
        SecureVars[Index].Data = ReallocateRuntimePool (
                                   VarSize,
                                   SecureVars[Index].Size,
                                   SecureVars[Index].Data
                                   );
        CopyPtr  = SecureVars[Index].Data;
        CopyPtr += VarSize;
        CopyMem (CopyPtr, CurPtr, PayloadSize);
      }

      FreePool (CurPtr);
    }

    DEBUG ((
      DEBUG_INFO,
      "%a: Adding %s Size %u\n",
      __FUNCTION__,
      SecureVars[Index].VarName,
      SecureVars[Index].Size
      ));
    if (HashApiUpdate (HashContext, SecureVars[Index].Data, SecureVars[Index].Size) != TRUE) {
      DEBUG ((DEBUG_ERROR, "Failed to update Hash %r\n", Status));
      Status = EFI_UNSUPPORTED;
      goto ExitMeasureSecureBootVars;
    }
  }

ExitMeasureSecureBootVars:
  return Status;
}

/*
 * ComputeVarMeasurement
 * Util function to compute the new measurement for the monitored variables.
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
EFI_STATUS
ComputeVarMeasurement (
  IN  CHAR16    *VarName   OPTIONAL,
  IN  EFI_GUID  *VarGuid   OPTIONAL,
  IN  UINT32    Attributes OPTIONAL,
  IN  VOID      *Data      OPTIONAL,
  IN  UINTN     DataSize   OPTIONAL,
  OUT UINT8     *Meas
  )
{
  EFI_STATUS  Status;
  UINTN       Index;

  if (HashContext == NULL) {
    HashContext = AllocateRuntimeZeroPool (HashApiGetContextSize ());
    if (HashContext == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Not Enough Resources to allocate HashContext\n", __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
      ASSERT_EFI_ERROR (Status);
    }
  }

  if (HashApiInit (HashContext) == FALSE) {
    DEBUG ((DEBUG_ERROR, "%a: HashApiInit Failed\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    goto ExitComputeVarMeasurement;
  }

  Status = MeasureBootVars (VarName, VarGuid, Attributes, Data, DataSize);
  ASSERT_EFI_ERROR (Status);
  Status = MeasureSecureDbVars (VarName, VarGuid, Attributes, Data, DataSize);
  ASSERT_EFI_ERROR (Status);

  if (HashApiFinal (HashContext, Meas) == FALSE) {
    DEBUG ((DEBUG_ERROR, "Finalizing Hash Failed\n"));
    Status = EFI_DEVICE_ERROR;
  }

  if ((BootCount != 0) && (BootOptions != NULL)) {
    for (Index = 0; Index < BootCount; Index++) {
      if (BootOptions[Index] != NULL) {
        FreePool (BootOptions[Index]);
      }
    }

    FreePool (BootOptions);
    BootCount   = 0;
    BootOptions = NULL;
  }

  if (BootOrder != NULL) {
    FreePool (BootOrder);
  }

  Status = EFI_SUCCESS;

ExitComputeVarMeasurement:
  return Status;
}
