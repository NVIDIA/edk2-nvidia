/** @file
  Configuration Manager Data of SMBIOS Type 8 table

  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
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

#include "ConfigurationSmbiosPrivate.h"

CM_STD_OBJ_SMBIOS_TABLE_INFO  CmSmbiosType8 = {
  SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION,
  CREATE_STD_SMBIOS_TABLE_GEN_ID (EStdSmbiosTableIdType08),
  NULL
};

/**
  Install CM object for SMBIOS Type 8

  @param[in, out] Private   Pointer to the private data of SMBIOS creators

  @return EFI_SUCCESS       Successful installation
  @retval !(EFI_SUCCESS)    Other errors

**/
EFI_STATUS
EFIAPI
InstallSmbiosType8Cm (
  IN OUT CM_SMBIOS_PRIVATE_DATA  *Private
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo    = Private->Repo;
  VOID                            *DtbBase = Private->DtbBase;
  CM_STD_PORT_CONNECTOR_INFO      *PortConnectorInfo;
  CONST VOID                      *Property;
  CONST CHAR8                     *PropertyStr;
  INT32                           Length;
  UINTN                           Index;
  UINTN                           NumPortConnectors;
  INT32                           NodeOffset;
  CHAR8                           Type8NodeStr[45];

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
                          sizeof (CM_STD_PORT_CONNECTOR_INFO) * (NumPortConnectors),
                          sizeof (CM_STD_PORT_CONNECTOR_INFO) * (NumPortConnectors + 1),
                          PortConnectorInfo
                          );
    if (PortConnectorInfo == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    PropertyStr = fdt_getprop (DtbBase, NodeOffset, "internal-reference-designator", &Length);
    if (PropertyStr != NULL) {
      PortConnectorInfo[NumPortConnectors].InternalReferenceDesignator = AllocateZeroPool (Length + 1);
      AsciiSPrint (PortConnectorInfo[NumPortConnectors].InternalReferenceDesignator, Length + 1, PropertyStr);
    }

    PropertyStr = fdt_getprop (DtbBase, NodeOffset, "external-reference-designator", &Length);
    if (PropertyStr != NULL) {
      PortConnectorInfo[NumPortConnectors].ExternalReferenceDesignator = AllocateZeroPool (Length + 1);
      AsciiSPrint (PortConnectorInfo[NumPortConnectors].ExternalReferenceDesignator, Length + 1, PropertyStr);
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
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < NumPortConnectors; Index++) {
    PortConnectorInfo[Index].CmObjectToken = REFERENCE_TOKEN (PortConnectorInfo[Index]);
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

  //
  // Install CM object for type 8
  //
  Repo->CmObjectId    = CREATE_CM_STD_OBJECT_ID (EStdObjPortConnectorInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_STD_PORT_CONNECTOR_INFO) * NumPortConnectors;
  Repo->CmObjectCount = NumPortConnectors;
  Repo->CmObjectPtr   = PortConnectorInfo;
  if ((UINTN)Repo < Private->RepoEnd) {
    Repo++;
  }

  ASSERT ((UINTN)Repo <= Private->RepoEnd);

  Private->Repo = Repo;

  return EFI_SUCCESS;
}
