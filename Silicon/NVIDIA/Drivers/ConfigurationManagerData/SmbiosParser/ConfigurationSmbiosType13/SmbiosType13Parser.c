/** @file
  Configuration Manager Data of SMBIOS Type 13 table.

  Copyright (c) 2013-2016 Intel Corporation.
  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

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
#include "SmbiosParserPrivate.h"

#define ABBREVIATED_FORMAT     1
#define LANG_SIZE_ABBREVIATED  5
#define LANG_SIZE_RFC4646      6

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType13 = {
  SMBIOS_TYPE_BIOS_LANGUAGE_INFORMATION,
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
   @param [in]  ParserHandle A handle to the parser instance.
   @param[in, out] Private   Pointer to the private data of SMBIOS creators

   @return EFI_SUCCESS       Successful installation
   @retval !(EFI_SUCCESS)    Other errors

 **/
EFI_STATUS
EFIAPI
InstallSmbiosType13Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  CM_SMBIOS_BIOS_LANGUAGE_INFO  *BiosLanguageInfo;
  EFI_STATUS                    Status;
  CHAR8                         *LangCodes;
  UINT8                         CodeSize;
  INT8                          Index;
  UINTN                         Count;
  UINT16                        Offset;
  UINT8                         LangCount;
  CHAR8                         *Language;
  CM_OBJECT_TOKEN               *TokenMap;
  CM_OBJ_DESCRIPTOR             Desc;

  TokenMap         = NULL;
  BiosLanguageInfo = NULL;
  Language         = NULL;
  Status           =  EFI_SUCCESS;

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

  BiosLanguageInfo = (CM_SMBIOS_BIOS_LANGUAGE_INFO *)AllocateZeroPool (sizeof (CM_SMBIOS_BIOS_LANGUAGE_INFO));

  Language = (CHAR8 *)AllocateZeroPool (sizeof (CHAR8) * LANG_SIZE_RFC4646);

  if ((BiosLanguageInfo == NULL) || (Language == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
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

  // Allocate Token Map
  Status = NvAllocateCmTokens (ParserHandle, 1, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 13: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  BiosLanguageInfo->BiosLanguageInfoToken = TokenMap[0];

  //
  // Install CM object for type 13
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjBiosLanguageInfo);
  Desc.Size     = sizeof (CM_SMBIOS_BIOS_LANGUAGE_INFO);
  Desc.Count    = 1;
  Desc.Data     = BiosLanguageInfo;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 13 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  //
  // Add type 13 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType13,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

CleanupAndReturn:
  FREE_NON_NULL (TokenMap);
  FREE_NON_NULL (BiosLanguageInfo);
  FREE_NON_NULL (Language);

  return Status;
}
