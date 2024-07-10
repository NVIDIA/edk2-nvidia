/** @file

  Wildcard string Library

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>                  // For AsciiStrStr, AsciiStrLen
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/WildcardStringLib.h>

#define WILDCARD_MAX_STRING_SIZE  0x100 // 256 bytes

/**
  @brief Checks if a given string matches a wildcard string.

  This function compares the given CheckString against the WildcardString
  and returns TRUE if there is a match, and FALSE otherwise. The WildcardString
  can contain any number of '*' wildcards, which match any number of characters.

  @param[in] WildcardString  A pointer to a null-terminated ASCII string that
                             contains the wildcard string.
  @param[in] CheckString     A pointer to a null-terminated ASCII string that
                             is checked against the wildcard string.

  @retval TRUE   The CheckString matches the WildcardString.
  @retval FALSE  The CheckString does not match the WildcardString.
**/
BOOLEAN
EFIAPI
WildcardStringMatchAscii (
  IN CONST CHAR8  *WildcardString,
  IN CONST CHAR8  *CheckString
  )
{
  CONST CHAR8  *CurrentCheckString;
  CHAR8        WildcardStackArray[WILDCARD_MAX_STRING_SIZE];
  CHAR8        *WildcardStringCopy;
  CHAR8        *NextToken;
  CHAR8        *TokenEnd;
  BOOLEAN      Result = FALSE; // Default to FALSE
  UINTN        WildcardStringSize;

  if ((WildcardString == NULL) || (CheckString == NULL)) {
    return FALSE;
  }

  WildcardStringSize = AsciiStrSize (WildcardString);

  // Match everything case
  if ((WildcardStringSize == 1) || ((WildcardStringSize == 2) && (*WildcardString == '*'))) {
    return TRUE;
  }

  // Allocate and copy WildcardString to WildcardStringCopy
  if (WildcardStringSize > WILDCARD_MAX_STRING_SIZE) {
    WildcardStringCopy = AllocatePool (WildcardStringSize);
    if (WildcardStringCopy == NULL) {
      return FALSE; // Allocation failed
    }
  } else {
    WildcardStringCopy = WildcardStackArray;
  }

  CopyMem (WildcardStringCopy, WildcardString, WildcardStringSize);

  CurrentCheckString = CheckString;
  NextToken          = WildcardStringCopy;

  while (*NextToken != '\0') {
    if (*NextToken == '*') {
      NextToken++;
      if (*NextToken == '\0') {
        // Advance to end of CurrentCheckString
        while (*CurrentCheckString != '\0') {
          CurrentCheckString++;
        }
      }

      continue;
    }

    TokenEnd = NextToken;
    while (*TokenEnd != '\0' && *TokenEnd != '*') {
      TokenEnd++;
    }

    // Temporarily terminate the current token
    CHAR8  Temp = *TokenEnd;
    *TokenEnd = '\0';

    CurrentCheckString = AsciiStrStr (CurrentCheckString, NextToken);
    if (NextToken == WildcardStringCopy) {
      // If this is the first token, we must match from the beginning of the CheckString
      if (CurrentCheckString != CheckString) {
        goto CLEANUP;
      }
    } else {
      // If this is not the first token, we must match from the end of the previous token
      if (CurrentCheckString == NULL) {
        goto CLEANUP;
      }
    }

    // Restore the token end character
    *TokenEnd = Temp;

    if (CurrentCheckString == NULL) {
      goto CLEANUP;
    }

    CurrentCheckString += (TokenEnd - NextToken);
    NextToken           = TokenEnd;
  }

  // Make sure we processed the entire CheckString
  if (*CurrentCheckString == '\0') {
    Result = TRUE; // All tokens found
  }

CLEANUP:
  if (WildcardStringCopy != WildcardStackArray) {
    FreePool (WildcardStringCopy);
  }

  return Result;
}
