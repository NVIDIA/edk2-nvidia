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
  Private->NextToken                                        = (CM_NULL_TOKEN + 1);
  Private->ConfigurationManagerTokenProtocol.AllocateTokens = ConfigManagerAllocateTokens;

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
