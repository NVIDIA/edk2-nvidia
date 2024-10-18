/** @file
*
*  Configuration Manager Token Protocol implementation.
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiDxe.h>

#include <Protocol/ConfigurationManagerTokenProtocol.h>

#include <Library/NVIDIADebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

#include "ConfigurationManagerTokenDxePrivate.h"

#define INITIAL_TOKEN_VALUE  (CM_NULL_TOKEN + 1)

/** ConfigManagerAllocateToken: allocates tokens to be used for upcoming entries

  This allocates tokens for future ConfigurationManager data, allowing tokens to
  be reserved before the data is ready to be added.

  @param [in]  This               The repo the tokens will belong to
  @param [in]  TokenCount         The number of tokens to allocate
  @param [out] TokenMap           Pointer to the allocated array of tokens,
                                  with TokenCount entries. Will be NULL if
                                  TokenCount is zero.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_OUT_OF_RESOURCES    Unable to allocate memory.
**/
STATIC
EFI_STATUS
EFIAPI
ConfigManagerAllocateTokens (
  IN NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL  *This,
  IN UINT32                                       TokenCount,
  OUT CM_OBJECT_TOKEN                             **TokenMap
  )
{
  CM_OBJECT_TOKEN                                           *LocalMap;
  UINTN                                                     Index;
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL_PRIVATE_DATA  *Private;

  if (TokenCount == 0) {
    if (TokenMap) {
      *TokenMap = NULL;
    }

    return EFI_SUCCESS;
  }

  NV_ASSERT_RETURN (This != NULL, return EFI_INVALID_PARAMETER, "%a: This pointer is NULL\n", __FUNCTION__);
  NV_ASSERT_RETURN (TokenMap != NULL, return EFI_INVALID_PARAMETER, "%a: TokenMap is NULL\n", __FUNCTION__);

  Private = NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL_PRIVATE_DATA_FROM_PROTOCOL (This);

  for (Index = 0; Index < TokenCount; Index++) {
    if ((Private->NextToken + Index) == CM_NULL_TOKEN) {
      DEBUG ((DEBUG_ERROR, "%a: Requested to add %u tokens, but adding %u new tokens to the existing tokens would overflow CM_NULL_TOKEN\n", __FUNCTION__, TokenCount, Index));
      return EFI_OUT_OF_RESOURCES;
    }
  }

  LocalMap = AllocatePool (sizeof (CM_OBJECT_TOKEN) * TokenCount);
  if (LocalMap == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate %u bytes for the new token map\n", __FUNCTION__, sizeof (CM_OBJECT_TOKEN) * TokenCount));
    return EFI_OUT_OF_RESOURCES;
  }

  for (Index = 0; Index < TokenCount; Index++) {
    LocalMap[Index] = Private->NextToken++;
  }

  *TokenMap = LocalMap;
  return EFI_SUCCESS;
}

typedef struct SANITY_CHECKER_INFO {
  UINT32    First;
  UINT32    Last;
  UINT64    *ValuesFound;
} SANITY_CHECKER_INFO;

/**
 * Checks if a given token is present in the sanity checker's range.
 *
 * @param Token The token to be checked.
 * @param Checker The sanity checker.
 *
 * @return EFI_SUCCESS if the token is present and not previously checked.
 *         EFI_DEVICE_ERROR if the token is present and has already been checked.
 *         EFI_INVALID_PARAMETER if the token is not in the sanity checker's range.
 */
STATIC
EFI_STATUS
EFIAPI
CheckRepoToken (
  IN  CM_OBJECT_TOKEN      Token,
  IN  SANITY_CHECKER_INFO  *Checker
  )
{
  UINT32  BitIndex;
  UINT32  Offset;

  if (Token == CM_NULL_TOKEN) {
    return EFI_SUCCESS;
  }

  if ((Token >= Checker->First) && (Token <= Checker->Last)) {
    BitIndex = Token - Checker->First;
    Offset   = BitIndex / 64;
    BitIndex = BitIndex % 64;
    DEBUG ((DEBUG_VERBOSE, "%a: Checking token %u (Offset %u, BitIndex %u)\n", __FUNCTION__, Token, Offset, BitIndex));
    if (Checker->ValuesFound[Offset] & (1ull << BitIndex)) {
      DEBUG ((DEBUG_ERROR, "%a: Token %u has already been seen\n", __FUNCTION__, Token));
      return EFI_DEVICE_ERROR;
    }

    Checker->ValuesFound[Offset] |= 1ull << BitIndex;
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_ERROR, "%a: Token %u is out of range\n", __FUNCTION__, Token));
  return EFI_INVALID_PARAMETER;
}

STATIC
EFI_STATUS
EFIAPI
ConfigManagerSanityCheckRepoTokens (
  IN NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL  *This,
  IN  EDKII_PLATFORM_REPOSITORY_INFO              *Repo
  )
{
  EFI_STATUS                                                Status;
  SANITY_CHECKER_INFO                                       Checker;
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL_PRIVATE_DATA  *Private;
  UINT32                                                    RepoIndex;
  UINT32                                                    MapIndex;

  NV_ASSERT_RETURN (This != NULL, return EFI_INVALID_PARAMETER, "%a: This pointer is NULL\n", __FUNCTION__);

  Private = NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL_PRIVATE_DATA_FROM_PROTOCOL (This);

  // Allocate and Initialize the tracker
  Checker.First = INITIAL_TOKEN_VALUE;
  Checker.Last  = Private->NextToken - 1;
  DEBUG ((DEBUG_VERBOSE, "%a: Sanity checking tokens %u to %u. Allocating %u bytes\n", __FUNCTION__, Checker.First, Checker.Last, ((Checker.Last - Checker.First + 64) / 64) * sizeof (UINT64)));
  Checker.ValuesFound = AllocateZeroPool (((Checker.Last - Checker.First + 64) / 64) * sizeof (UINT64));
  if (Checker.ValuesFound == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Check all the tokens in Repo
  for (RepoIndex = 0; RepoIndex < Repo->EntryCount; RepoIndex++) {
    Status = CheckRepoToken (Repo->Entries[RepoIndex].Token, &Checker);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r for RepoIndex %x\n", __FUNCTION__, Status, RepoIndex));
      return Status;
    }

    if (Repo->Entries[RepoIndex].ElementTokenMap != NULL) {
      // Check each element in the ElementTokenMap
      DEBUG ((DEBUG_VERBOSE, "%a: Checking RepoIndex %x ElementCount %x\n", __FUNCTION__, RepoIndex, Repo->Entries[RepoIndex].CmObjectDesc.Count));
      for (MapIndex = 0; MapIndex < Repo->Entries[RepoIndex].CmObjectDesc.Count; MapIndex++) {
        Status = CheckRepoToken (Repo->Entries[RepoIndex].ElementTokenMap[MapIndex], &Checker);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a: Got %r for RepoIndex %x MapIndex %x\n", __FUNCTION__, Status, RepoIndex, MapIndex));
          return Status;
        }
      }
    }
  }

  FreePool (Checker.ValuesFound);

  return EFI_SUCCESS;
}

/**
  Initialize the Configuration Manager Token Protocol Driver.

  @param[in]  ImageHandle   of the loaded driver
  @param[in]  SystemTable   Pointer to the System Table

**/
EFI_STATUS
EFIAPI
ConfigurationManagerTokenProtocolInit (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                                                Status;
  NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL_PRIVATE_DATA  *Private;

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof (NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL_PRIVATE_DATA),
                  (VOID **)&Private
                  );

  if (EFI_ERROR (Status) || (Private == NULL)) {
    return EFI_OUT_OF_RESOURCES;
  }

  Private->Signature                                        = NVIDIA_CONFIGURATION_MANAGER_TOKEN_PROTOCOL_SIGNATURE;
  Private->NextToken                                        = INITIAL_TOKEN_VALUE;
  Private->ConfigurationManagerTokenProtocol.AllocateTokens = ConfigManagerAllocateTokens;
  Private->ConfigurationManagerTokenProtocol.SanityCheck    = ConfigManagerSanityCheckRepoTokens;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gNVIDIAConfigurationManagerTokenProtocolGuid,
                  &Private->ConfigurationManagerTokenProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r installing protocol\n", __FUNCTION__, Status));
  }

  return Status;
}
