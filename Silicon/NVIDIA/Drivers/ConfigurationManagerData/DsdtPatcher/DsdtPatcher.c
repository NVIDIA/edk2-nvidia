/** @file
  Patches to the DSDT

  SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "DsdtPatcher.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/ConfigurationManagerDataLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/RasNsCommPcieDpcDataProtocol.h>

/** patch PLAT data in DSDT.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdatePlatInfo (
  IN NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol,
  IN TEGRA_PLATFORM_TYPE        PlatformType
  )
{
  EFI_STATUS            Status;
  NVIDIA_AML_NODE_INFO  AcpiNodeInfo;

  Status = PatchProtocol->FindNode (PatchProtocol, ACPI_PLAT_INFO, &AcpiNodeInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: PLAT node is not found for patching %a - %r\r\n", __FUNCTION__, ACPI_PLAT_INFO, Status));
    goto ErrorExit;
  }

  Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &PlatformType, AcpiNodeInfo.Size);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, ACPI_PLAT_INFO, Status));
  }

ErrorExit:
  return (Status == EFI_NOT_FOUND) ? EFI_SUCCESS : Status;
}

/** patch GED data in DSDT.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateGedInfo (
  IN NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol
  )
{
  EFI_STATUS                  Status;
  NVIDIA_AML_NODE_INFO        AcpiNodeInfo;
  RAS_PCIE_DPC_COMM_BUF_INFO  *DpcCommBuf = NULL;

  Status = gBS->LocateProtocol (
                  &gNVIDIARasNsCommPcieDpcDataProtocolGuid,
                  NULL,
                  (VOID **)&DpcCommBuf
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Couldn't get gNVIDIARasNsCommPcieDpcDataProtocolGuid protocol: %r\n",
      __FUNCTION__,
      Status
      ));
  }

  if (DpcCommBuf == NULL) {
    // Protocol installed NULL interface. Skip using it.
    return EFI_SUCCESS;
  }

  Status = PatchProtocol->FindNode (PatchProtocol, ACPI_GED1_SMR1, &AcpiNodeInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GED node is not found for patching %a - %r\r\n", __FUNCTION__, ACPI_GED1_SMR1, Status));
    goto ErrorExit;
  }

  Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &DpcCommBuf->PcieBase, 8);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, ACPI_GED1_SMR1, Status));
  }

ErrorExit:
  return (Status == EFI_NOT_FOUND) ? EFI_SUCCESS : Status;
}

STATIC CONST CHAR8  *QspiCompatibleInfo[] = {
  "nvidia,tegra186-qspi",
  NULL
};

/** patch QSPI1 data in DSDT.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateQspiInfo (
  IN NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol
  )
{
  EFI_STATUS            Status;
  INT32                 NodeOffset;
  NVIDIA_AML_NODE_INFO  AcpiNodeInfo;
  UINT8                 QspiStatus;

  NodeOffset = -1;
  Status     = DeviceTreeGetNextCompatibleNode (QspiCompatibleInfo, &NodeOffset);
  while (EFI_SUCCESS == Status) {
    Status = DeviceTreeGetNodeProperty (NodeOffset, "nvidia,secure-qspi-controller", NULL, NULL);
    if (Status == EFI_NOT_FOUND) {
      Status = PatchProtocol->FindNode (PatchProtocol, ACPI_QSPI1_STA, &AcpiNodeInfo);
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      if (AcpiNodeInfo.Size > sizeof (QspiStatus)) {
        Status = EFI_DEVICE_ERROR;
        goto ErrorExit;
      }

      QspiStatus = 0xF;
      Status     = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &QspiStatus, sizeof (QspiStatus));
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, ACPI_QSPI1_STA, Status));
      }
    }

    Status = DeviceTreeGetNextCompatibleNode (QspiCompatibleInfo, &NodeOffset);
  }

ErrorExit:
  return (Status == EFI_NOT_FOUND) ? EFI_SUCCESS : Status;
}

STATIC CONST CHAR8  *I2CCompatibleInfo[] = {
  "nvidia,tegra234-i2c",
  NULL
};

/** patch I2C3 and SSIF data in DSDT.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateSSIFInfo (
  IN NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol
  )
{
  EFI_STATUS            Status;
  INT32                 NodeOffset;
  INT32                 SubNodeOffset;
  NVIDIA_AML_NODE_INFO  AcpiNodeInfo;
  UINT8                 I2CStatus;
  UINT8                 SSIFStatus;

  NodeOffset = -1;
  Status     = DeviceTreeGetNextCompatibleNode (I2CCompatibleInfo, &NodeOffset);
  while (EFI_SUCCESS == Status) {
    Status = DeviceTreeGetNamedSubnode ("bmc-ssif", NodeOffset, &SubNodeOffset);
    if (!EFI_ERROR (Status)) {
      /* Update I2C3 Status */
      Status = PatchProtocol->FindNode (PatchProtocol, ACPI_I2C3_STA, &AcpiNodeInfo);
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      if (AcpiNodeInfo.Size > sizeof (I2CStatus)) {
        Status = EFI_DEVICE_ERROR;
        goto ErrorExit;
      }

      I2CStatus = 0xF;
      Status    = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &I2CStatus, sizeof (I2CStatus));
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, ACPI_I2C3_STA, Status));
        goto ErrorExit;
      }

      /* Update SSIF Status */
      Status = PatchProtocol->FindNode (PatchProtocol, ACPI_SSIF_STA, &AcpiNodeInfo);
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      if (AcpiNodeInfo.Size > sizeof (SSIFStatus)) {
        Status = EFI_DEVICE_ERROR;
        goto ErrorExit;
      }

      SSIFStatus = 0xF;
      Status     = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &I2CStatus, sizeof (SSIFStatus));
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, ACPI_SSIF_STA, Status));
      }
    }

    Status = DeviceTreeGetNextCompatibleNode (I2CCompatibleInfo, &NodeOffset);
  }

ErrorExit:
  return (Status == EFI_NOT_FOUND) ? EFI_SUCCESS : Status;
}

STATIC CONST CHAR8  *BpmpIpcCompatibleInfo[] = {
  "nvidia,tegra264-bpmp-shmem",
  NULL
};

STATIC CHAR8  *BpmpIpcResources[] = {
  ACPI_MRQ0_TX,
  ACPI_MRQ0_RX,
};

/** patch MRQ0 BPMP IPC TX/RX resource data in DSDT.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateBpmpIpcInfo (
  IN NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol
  )
{
  EFI_STATUS                               Status;
  INT32                                    NodeOffset;
  NVIDIA_AML_NODE_INFO                     AcpiNodeInfo;
  EFI_ACPI_QWORD_ADDRESS_SPACE_DESCRIPTOR  Descriptor;
  NVIDIA_DEVICE_TREE_REGISTER_DATA         RegisterData;
  UINT32                                   NumRegisters;
  UINT64                                   BpmpShmemBase;
  UINTN                                    Index;

  NodeOffset = -1;
  Status     = DeviceTreeGetNextCompatibleNode (BpmpIpcCompatibleInfo, &NodeOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: no compat DTB node: %r\n", __FUNCTION__, Status));
    if (Status == EFI_NOT_FOUND) {
      return EFI_SUCCESS;
    }

    goto ErrorExit;
  }

  NumRegisters = 1;
  Status       = DeviceTreeGetRegisters (NodeOffset, &RegisterData, &NumRegisters);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: get registers failed: %r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  BpmpShmemBase = RegisterData.BaseAddress;

  for (Index = 0; Index < ARRAY_SIZE (BpmpIpcResources); Index++) {
    Status = PatchProtocol->FindNode (PatchProtocol, BpmpIpcResources[Index], &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      if ((Index == 0) && (Status == EFI_NOT_FOUND)) {
        DEBUG ((DEBUG_INFO, "%a: %a not found, skipping\n", __FUNCTION__, BpmpIpcResources[Index]));
        return EFI_SUCCESS;
      }

      DEBUG ((DEBUG_ERROR, "%a: finding %a failed: %r\n", __FUNCTION__, BpmpIpcResources[Index], Status));
      goto ErrorExit;
    }

    Status = PatchProtocol->GetNodeData (PatchProtocol, &AcpiNodeInfo, &Descriptor, sizeof (Descriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: %a get data failed: %r\n", __FUNCTION__, BpmpIpcResources[Index], Status));
      goto ErrorExit;
    }

    DEBUG ((DEBUG_INFO, "%a: %a min/max=0x%llx/0x%llx: %r\n", __FUNCTION__, BpmpIpcResources[Index], Descriptor.AddrRangeMin, Descriptor.AddrRangeMax, Status));

    Descriptor.AddrRangeMin += BpmpShmemBase;
    Descriptor.AddrRangeMax += BpmpShmemBase;

    DEBUG ((DEBUG_INFO, "%a: setting %a min/max to 0x%llx/0x%llx\n", __FUNCTION__, BpmpIpcResources[Index], Descriptor.AddrRangeMin, Descriptor.AddrRangeMax));

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &Descriptor, sizeof (Descriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: %a set data failed: %r\n", __FUNCTION__, BpmpIpcResources[Index], Status));
      goto ErrorExit;
    }
  }

ErrorExit:
  return Status;
}

/** patch EEPROMs data in DSDT.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateEepromInfo (
  IN NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol
  )
{
  EFI_STATUS            Status;
  INT32                 NodeOffset;
  INT32                 SubNodeOffset;
  NVIDIA_AML_NODE_INFO  AcpiNodeInfo;
  UINT8                 I2CStatus;
  UINT8                 EepromStatus;
  CONST CHAR8           *StatusString;

  NodeOffset = -1;
  Status     = DeviceTreeGetNextCompatibleNode (I2CCompatibleInfo, &NodeOffset);
  while (EFI_SUCCESS == Status) {
    Status = DeviceTreeGetNamedSubnode (
               "eeprom1",
               NodeOffset,
               &SubNodeOffset
               );
    if (!EFI_ERROR (Status)) {
      Status = DeviceTreeGetNodeProperty (
                 NodeOffset,
                 "status",
                 (CONST VOID **)&StatusString,
                 NULL
                 );
      if (!EFI_ERROR (Status) && (AsciiStrCmp (StatusString, "okay") == 0)) {
        // Find EEP1 node
        Status = PatchProtocol->FindNode (PatchProtocol, ACPI_EEP1_STA, &AcpiNodeInfo);
        if (!EFI_ERROR (Status) && (AcpiNodeInfo.Size == sizeof (EepromStatus))) {
          // Set EEP1 Status
          EepromStatus = 0xF;
          Status       = PatchProtocol->SetNodeData (
                                          PatchProtocol,
                                          &AcpiNodeInfo,
                                          &EepromStatus,
                                          sizeof (EepromStatus
                                                  )
                                          );
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\n", __FUNCTION__, ACPI_EEP1_STA, Status));
          }
        } else {
          DEBUG ((DEBUG_ERROR, "%a: Cannot find %a\n", __FUNCTION__, ACPI_EEP1_STA));
        }
      }
    }

    Status = DeviceTreeGetNamedSubnode (
               "eeprom2",
               NodeOffset,
               &SubNodeOffset
               );
    if (!EFI_ERROR (Status)) {
      Status = DeviceTreeGetNodeProperty (
                 NodeOffset,
                 "status",
                 (CONST VOID **)&StatusString,
                 NULL
                 );
      if (!EFI_ERROR (Status) && (AsciiStrCmp (StatusString, "okay") == 0)) {
        // Find I2CB node
        Status = PatchProtocol->FindNode (PatchProtocol, ACPI_I2CB_STA, &AcpiNodeInfo);
        if (!EFI_ERROR (Status) && (AcpiNodeInfo.Size == sizeof (I2CStatus))) {
          // Set I2CB Status
          I2CStatus = 0xF;
          Status    = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &I2CStatus, sizeof (I2CStatus));
          if (!EFI_ERROR (Status)) {
            // Find EEP2 node
            Status = PatchProtocol->FindNode (PatchProtocol, ACPI_EEP2_STA, &AcpiNodeInfo);
            if (!EFI_ERROR (Status) && (AcpiNodeInfo.Size == sizeof (EepromStatus))) {
              // Set EEP2 Status
              EepromStatus = 0xF;
              Status       = PatchProtocol->SetNodeData (
                                              PatchProtocol,
                                              &AcpiNodeInfo,
                                              &EepromStatus,
                                              sizeof (EepromStatus
                                                      )
                                              );
              if (EFI_ERROR (Status)) {
                DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\n", __FUNCTION__, ACPI_EEP2_STA, Status));
              }
            } else {
              DEBUG ((DEBUG_ERROR, "%a: Cannot find %a\n", __FUNCTION__, ACPI_EEP2_STA));
            }
          } else {
            DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\n", __FUNCTION__, ACPI_I2CB_STA, Status));
          }
        } else {
          DEBUG ((DEBUG_ERROR, "%a: Cannot find %a\n", __FUNCTION__, ACPI_I2CB_STA));
        }
      }
    }

    Status = DeviceTreeGetNextCompatibleNode (I2CCompatibleInfo, &NodeOffset);
  }

  return EFI_SUCCESS;
}

/** DSDT patcher function.

  The DSDT table is potentially patched with the following information:
    "_SB_.PLAT"
    "_SB_.GED1.SMR1"
    "_SB_.QSP1._STA"
    "_SB_.I2C3._STA"
    "_SB_.I2C3.SSIF._STA"
    "_SB_.I2CB._STA"
    "_SB_.I2C2.EEP1._STA"
    "_SB_.I2CB.EEP2._STA"

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
DsdtPatcher (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                 Status;
  NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol;
  TEGRA_PLATFORM_TYPE        PlatformType;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  Status = NvGetCmPatchProtocol (ParserHandle, &PatchProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PlatformType = TegraGetPlatform ();
  Status       = UpdatePlatInfo (PatchProtocol, PlatformType);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateGedInfo (PatchProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateQspiInfo (PatchProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateSSIFInfo (PatchProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateBpmpIpcInfo (PatchProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateEepromInfo (PatchProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

REGISTER_PARSER_FUNCTION (DsdtPatcher, NULL)
