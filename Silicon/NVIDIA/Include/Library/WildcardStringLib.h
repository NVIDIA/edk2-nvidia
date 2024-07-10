/** @file

  Wildcard string Library

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef WILDCARD_STRING_LIB__
#define WILDCARD_STRING_LIB__

#include <Base.h>

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
  );

#endif // WILDCARD_STRING_LIB__
