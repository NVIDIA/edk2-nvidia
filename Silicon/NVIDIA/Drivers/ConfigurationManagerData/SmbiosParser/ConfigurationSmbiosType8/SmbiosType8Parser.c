/** @file
  Configuration Manager Data of SMBIOS Type 8 table.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <libfdt.h>
#include <Library/PrintLib.h>
#include <ConfigurationManagerObject.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "SmbiosParserPrivate.h"
#include "SmbiosType8Parser.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType8 = {
  SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType08),
  NULL
};

/**
  Install CM object for SMBIOS Type 8
  @param [in]  ParserHandle A handle to the parser instance.
  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType8Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
{
  VOID                           *DtbBase = Private->DtbBase;
  CM_SMBIOS_PORT_CONNECTOR_INFO  *PortConnectorInfo;
  CONST VOID                     *Property;
  CONST CHAR8                    *PropertyStr;
  INT32                          Length;
  UINTN                          Index;
  UINTN                          NumPortConnectors;
  INT32                          NodeOffset;
  CHAR8                          Type8NodeStr[45];
  EFI_STATUS                     Status;
  CM_OBJ_DESCRIPTOR              Desc;
  CM_OBJECT_TOKEN                *TokenMap;

  TokenMap          = NULL;
  NumPortConnectors = 0;
  PortConnectorInfo = NULL;
  for (Index = 0; ; Index++) {
    ZeroMem (Type8NodeStr, sizeof (Type8NodeStr));
    AsciiSPrint (Type8NodeStr, sizeof (Type8NodeStr), "/firmware/smbios/type8@%u", Index);
    NodeOffset = fdt_path_offset (DtbBase, Type8NodeStr);
    if (NodeOffset < 0) {
      break;
    }

    PortConnectorInfo = ReallocatePool (
                          sizeof (CM_SMBIOS_PORT_CONNECTOR_INFO) * (NumPortConnectors),
                          sizeof (CM_SMBIOS_PORT_CONNECTOR_INFO) * (NumPortConnectors + 1),
                          PortConnectorInfo
                          );
    if (PortConnectorInfo == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to reallocate space for a new port connector\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    PropertyStr = fdt_getprop (DtbBase, NodeOffset, "internal-reference-designator", &Length);
    if (PropertyStr != NULL) {
      PortConnectorInfo[NumPortConnectors].InternalReferenceDesignator = AllocateZeroPool (Length + 1);
      if (PortConnectorInfo[NumPortConnectors].InternalReferenceDesignator != NULL) {
        AsciiSPrint (PortConnectorInfo[NumPortConnectors].InternalReferenceDesignator, Length + 1, PropertyStr);
      }
    }

    PropertyStr = fdt_getprop (DtbBase, NodeOffset, "external-reference-designator", &Length);
    if (PropertyStr != NULL) {
      PortConnectorInfo[NumPortConnectors].ExternalReferenceDesignator = AllocateZeroPool (Length + 1);
      if (PortConnectorInfo[NumPortConnectors].ExternalReferenceDesignator != NULL) {
        AsciiSPrint (PortConnectorInfo[NumPortConnectors].ExternalReferenceDesignator, Length + 1, PropertyStr);
      }
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "internal-connector-type", &Length);
    if (Property != NULL) {
      PortConnectorInfo[NumPortConnectors].InternalConnectorType = (UINT8)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "external-connector-type", &Length);
    if (Property != NULL) {
      PortConnectorInfo[NumPortConnectors].ExternalConnectorType = (UINT8)fdt32_to_cpu (*(UINT32 *)Property);
    }

    Property = fdt_getprop (DtbBase, NodeOffset, "port-type", &Length);
    if (Property != NULL) {
      PortConnectorInfo[NumPortConnectors].PortType = (UINT8)fdt32_to_cpu (*(UINT32 *)Property);
    }

    NumPortConnectors++;
  }

  DEBUG ((DEBUG_INFO, "%a: Number of Port Connectors = %u\n", __FUNCTION__, NumPortConnectors));
  if (NumPortConnectors == 0) {
    Status = EFI_SUCCESS;
    goto CleanupAndReturn;
  }

  // Allocate Token Map
  Status = NvAllocateCmTokens (ParserHandle, NumPortConnectors, &TokenMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to allocate a token for SMBIOS Type 8: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  for (Index = 0; Index < NumPortConnectors; Index++) {
    PortConnectorInfo[Index].CmObjectToken =   TokenMap[Index];
  }

  //
  // Install CM object for type 8
  //
  Desc.ObjectId = CREATE_CM_SMBIOS_OBJECT_ID (ESmbiosObjPortConnectorInfo);
  Desc.Size     = sizeof (CM_SMBIOS_PORT_CONNECTOR_INFO) * NumPortConnectors;
  Desc.Count    = NumPortConnectors;
  Desc.Data     = PortConnectorInfo;
  Status        = NvAddMultipleCmObjWithTokens (ParserHandle, &Desc, TokenMap, CM_NULL_TOKEN);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to add Smbios Type 8 to ConfigManager: %r\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  //
  // Add type 8 to SMBIOS table list
  //
  CopyMem (
    &Private->CmSmbiosTableList[Private->CmSmbiosTableCount],
    &CmSmbiosType8,
    sizeof (CM_STD_OBJ_SMBIOS_TABLE_INFO)
    );
  Private->CmSmbiosTableCount++;

CleanupAndReturn:
  FREE_NON_NULL (TokenMap);
  FREE_NON_NULL (PortConnectorInfo);
  return Status;
}
