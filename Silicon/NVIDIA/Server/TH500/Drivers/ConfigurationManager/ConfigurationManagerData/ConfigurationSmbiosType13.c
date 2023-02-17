/**
  Configuration Manager Data of SMBIOS Type 13 table.

  Copyright (c) 2013-2016 Intel Corporation.
  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>

#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>
#include "ConfigurationSmbiosPrivate.h"

#define ABBREVIATED_FORMAT     1
#define LANG_SIZE_ABBREVIATED  5
#define LANG_SIZE_RFC4646      6

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType13 = {
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType13),
  NULL
};

/**
  Convert RFC 4646 language code to Abbreviated format
  As per DMTF Spec: Version 3.6.0, in abbreviated Language format,
  each language string has two-character“ISO 639-1 Language Name”
  directly followed by the two-character “ISO 3166-1-alpha-2
  Territory Name.”

  @param   LangCode     The language code in RFC 4646 format.
  @param   AbbrLang     The language code in Abbreviated format.

**/
VOID
EFIAPI
ConvertToAbbr (
  IN  CHAR8  *LangCode,
  OUT CHAR8  *AbbrLang
  )
{
  UINT8  Offset;
  UINT8  Index;

  if ((LangCode == NULL) || (AbbrLang == NULL)) {
    return;
  }

  for (Offset = 0, Index = 0; LangCode[Offset] != '\0'; Offset++, Index++) {
    if (LangCode[Offset] == '-') {
      Index--;
      continue;
    }

    AbbrLang[Index] = LangCode[Offset];
  }

  AbbrLang[Index] = '\0';
  return;
}

/**
  Check whether the language is found in supported language list.

  @param   Languages     The installable language codes.
  @param   Offset        The offest of current language in the supported languages.

  @retval  TRUE          Supported.
  @retval  FALSE         Not Supported.

**/
BOOLEAN
EFIAPI
CurrentLanguageMatch (
  IN  CHAR8   *Languages,
  OUT UINT16  *Offset
  )
{
  EFI_STATUS  Status;
  CHAR8       *DefaultLang;
  CHAR8       *CurrentLang;
  CHAR8       *BestLanguage;
  CHAR8       *MatchLang;
  CHAR8       *EndMatchLang;
  UINTN       CompareLength;
  BOOLEAN     LangMatch;

  if (Languages == NULL) {
    return FALSE;
  }

  LangMatch = FALSE;
  Status    = GetEfiGlobalVariable2 (L"PlatformLang", (VOID **)&CurrentLang, NULL);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  DefaultLang = (CHAR8 *)PcdGetPtr (PcdUefiVariableDefaultPlatformLang);

  BestLanguage = GetBestLanguage (
                   Languages,
                   FALSE,
                   (CurrentLang != NULL) ? CurrentLang : "",
                   DefaultLang,
                   NULL
                   );
  if (BestLanguage != NULL) {
    //
    // Find the best matching RFC 4646 language, compute the offset.
    //
    LangMatch     = TRUE;
    CompareLength = AsciiStrLen (BestLanguage);
    for (MatchLang = Languages, (*Offset) = 0; *MatchLang != '\0'; (*Offset)++) {
      //
      // Seek to the end of current match language.
      //
      for (EndMatchLang = MatchLang; *EndMatchLang != '\0' && *EndMatchLang != ';'; EndMatchLang++) {
      }

      if ((EndMatchLang == MatchLang + CompareLength) && (AsciiStrnCmp (MatchLang, BestLanguage, CompareLength) == 0)) {
        //
        // Find the current best Language in the supported languages
        //
        break;
      }

      //
      // best language match be in the supported language.
      //
      ASSERT (*EndMatchLang == ';');
      MatchLang = EndMatchLang + 1;
    }

    FreePool (BestLanguage);
  }

  if (CurrentLang != NULL) {
    FreePool (CurrentLang);
  }

  return LangMatch;
}

/**
  Get next language from language code list (with separator ';').

  @param  LangCode       Input: point to first language in the list. On
                         Otput: point to next language in the list, or
                                NULL if no more language in the list.
  @param  Lang           The first language in the list.

**/
VOID
EFIAPI
GetNextLanguage (
  IN OUT CHAR8  **LangCode,
  OUT CHAR8     *Lang
  )
{
  UINTN  Index;
  CHAR8  *StringPtr;

  ASSERT (LangCode != NULL);
  ASSERT (*LangCode != NULL);
  ASSERT (Lang != NULL);

  Index     = 0;
  StringPtr = *LangCode;
  while (StringPtr[Index] != 0 && StringPtr[Index] != ';') {
    Index++;
  }

  CopyMem (Lang, StringPtr, Index);
  Lang[Index] = 0;

  if (StringPtr[Index] == ';') {
    Index++;
  }

  *LangCode = StringPtr + Index;
}

/**
  Install CM object for SMBIOS Type 13

   @param[in, out] Private   Pointer to the private data of SMBIOS creators

   @return EFI_SUCCESS       Successful installation
   @retval !(EFI_SUCCESS)    Other errors

 **/
EFI_STATUS
EFIAPI
InstallSmbiosType13Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  CM_STD_BIOS_LANGUAGE_INFO       *BiosLanguageInfo;
  EFI_STATUS                      Status;
  CHAR8                           *LangCodes;
  UINT8                           CodeSize;
  INT8                            Index;
  UINTN                           Count;
  UINT16                          Offset;
  UINT8                           LangCount;
  CHAR8                           *Language;

  Repo   = Private->Repo;
  Status =  EFI_SUCCESS;

  // Get Supported language codes from PCD.
  LangCodes = ((CHAR8 *)PcdGetPtr (PcdUefiVariableDefaultPlatformLangCodes));

  CodeSize = AsciiStrLen (LangCodes);
  ASSERT (CodeSize > 0);

  // Count number of languages in the LangCodes string, using seperator ';'.
  LangCount = 1;
  for (Index = 0; Index < CodeSize; Index++) {
    if (LangCodes[Index] == ';') {
      LangCount++;
    }
  }

  // Check if platform language matches with any of the supported language code and find its offset.
  if (!CurrentLanguageMatch (LangCodes, &Offset)) {
    DEBUG ((DEBUG_ERROR, "%a: Matching Lang code for platform language not found\n", __FUNCTION__));
    return EFI_NOT_FOUND;
  }

  BiosLanguageInfo = (CM_STD_BIOS_LANGUAGE_INFO *)AllocateZeroPool (sizeof (CM_STD_BIOS_LANGUAGE_INFO));

  Language = (CHAR8 *)AllocateZeroPool (sizeof (CHAR8) * LANG_SIZE_RFC4646);

  if ((BiosLanguageInfo == NULL) || (Language == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto exit;
  }

  BiosLanguageInfo->SupportedLanguages = (CHAR8 **)AllocateZeroPool (LangCount * sizeof (CHAR8 *));
  ASSERT (BiosLanguageInfo->SupportedLanguages != NULL);

  for (Count = 0; Count < LangCount; Count++) {
    // Get each of the supported languages in RFC4646.
    GetNextLanguage (&LangCodes, Language);

    BiosLanguageInfo->SupportedLanguages[Count] = (CHAR8 *)AllocateZeroPool (sizeof (CHAR8) * LANG_SIZE_ABBREVIATED);

    ASSERT (BiosLanguageInfo->SupportedLanguages[Count] != NULL);
    // Convert language code from RFC4646 to Abbreviated language format.
    ConvertToAbbr (Language, BiosLanguageInfo->SupportedLanguages[Count]);
    ZeroMem (Language, LANG_SIZE_RFC4646);
  }

  BiosLanguageInfo->InstallableLanguages =  LangCount;

  BiosLanguageInfo->Flags = ABBREVIATED_FORMAT;

  // smbios table offset is 1 based, hence increment.
  BiosLanguageInfo->CurrentLanguage = ++Offset;

  BiosLanguageInfo->BiosLanguageInfoToken = REFERENCE_TOKEN (BiosLanguageInfo[0]);

  //
  // Add type 13 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType13,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;
  //
  // Install CM object for type 13
  //
  Repo->CmObjectId    = CREATE_CM_STD_OBJECT_ID (EStdObjBiosLanguageInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_STD_BIOS_LANGUAGE_INFO);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = BiosLanguageInfo;
  Repo++;

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

exit:
  // Free buffer.
  if (Language) {
    FreePool (Language);
  }

  return Status;
}
