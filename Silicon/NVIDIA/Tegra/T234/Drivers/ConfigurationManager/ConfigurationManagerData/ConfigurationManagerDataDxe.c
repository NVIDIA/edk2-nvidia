/** @file
  Configuration Manager Data Dxe

  Copyright (c) 2019 - 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/

#include <ConfigurationManagerObject.h>

#include <Library/ArmLib.h>
#include <Library/ArmGicLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/ConfigurationManagerLib.h>
#include <Library/DebugLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <libfdt.h>

#include <IndustryStandard/DebugPort2Table.h>

#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>

#include <Protocol/AmlPatchProtocol.h>
#include <Protocol/AmlGenerationProtocol.h>
#include <Protocol/ConfigurationManagerDataProtocol.h>

#include <NVIDIAConfiguration.h>

#include "Platform.h"
#include <T234/T234Definitions.h>

#include "Dsdt.hex"
#include "Dsdt.offset.h"
#include "SdcTemplate.hex"
#include "SdcTemplate.offset.h"

#define ACPI_PATCH_MAX_PATH  255
#define ACPI_SDCT_REG0       "SDCT.REG0"
#define ACPI_SDCT_UID        "SDCT._UID"
#define ACPI_SDCT_INT0       "SDCT.INT0"
#define ACPI_SDCT_RMV        "SDCT._RMV"

// Index at which Cores get listed in Proc Hierarchy Node
#define CORE_BEGIN_INDEX  4

// AML Patch protocol
NVIDIA_AML_PATCH_PROTOCOL       *PatchProtocol      = NULL;
NVIDIA_AML_GENERATION_PROTOCOL  *GenerationProtocol = NULL;

STATIC EFI_ACPI_DESCRIPTION_HEADER  *AcpiTableArray[] = {
  (EFI_ACPI_DESCRIPTION_HEADER *)dsdt_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)sdctemplate_aml_code
};

STATIC AML_OFFSET_TABLE_ENTRY  *OffsetTableArray[] = {
  DSDT_TEGRA234_OffsetTable,
  SSDT_SDCTEMP_OffsetTable
};

/** The platform configuration repository information.
*/
STATIC
EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo;

/** The platform configuration manager information.
*/
STATIC
CM_STD_OBJ_CONFIGURATION_MANAGER_INFO  CmInfo = {
  CONFIGURATION_MANAGER_REVISION,
  CFG_MGR_OEM_ID
};

/** The platform ACPI table list.
*/
STATIC
CM_STD_OBJ_ACPI_TABLE_INFO  CmAcpiTableList[] = {
  // FADT Table
  {
    EFI_ACPI_6_4_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_4_FIXED_ACPI_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdFadt),
    NULL,
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision)
  },
  // GTDT Table
  {
    EFI_ACPI_6_4_GENERIC_TIMER_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_4_GENERIC_TIMER_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdGtdt),
    NULL,
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision)
  },
  // MADT Table
  {
    EFI_ACPI_6_4_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_4_MULTIPLE_APIC_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMadt),
    NULL,
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision)
  },
  // DSDT Table
  {
    EFI_ACPI_6_4_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_4_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDsdt),
    (EFI_ACPI_DESCRIPTION_HEADER *)dsdt_aml_code,
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision)
  },
  // SSDT Table - Cpu Topology
  {
    EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdtCpuTopology),
    NULL,
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision)
  },
};

/** The platform boot architecture information.
*/
STATIC
CM_ARM_BOOT_ARCH_INFO  BootArchInfo = {
  EFI_ACPI_6_4_ARM_PSCI_COMPLIANT
};

/** The platform power management profile information.
*/
STATIC
CM_ARM_POWER_MANAGEMENT_PROFILE_INFO  PmProfileInfo = {
  EFI_ACPI_6_4_PM_PROFILE_ENTERPRISE_SERVER
};

/** The platform generic timer information.
*/
STATIC
CM_ARM_GENERIC_TIMER_INFO  GenericTimerInfo = {
  SYSTEM_COUNTER_BASE_ADDRESS,
  SYSTEM_COUNTER_READ_BASE,
  FixedPcdGet32 (PcdArmArchTimerSecIntrNum),
  GTDT_GTIMER_FLAGS,
  FixedPcdGet32 (PcdArmArchTimerIntrNum),
  GTDT_GTIMER_FLAGS,
  FixedPcdGet32 (PcdArmArchTimerVirtIntrNum),
  GTDT_GTIMER_FLAGS,
  FixedPcdGet32 (PcdArmArchTimerHypIntrNum),
  GTDT_GTIMER_FLAGS
};

EFI_STATUS
EFIAPI
UpdateSerialPortInfo (
  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo
  )
{
  EFI_STATUS                         Status;
  UINT32                             NumberOfSerialPorts;
  UINT32                             *SerialHandles;
  EDKII_PLATFORM_REPOSITORY_INFO     *Repo;
  CM_ARM_SERIAL_PORT_INFO            *SpcrSerialPort;
  NVIDIA_DEVICE_TREE_REGISTER_DATA   RegisterData;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA  InterruptData;
  UINT32                             Index;
  UINT32                             Size;
  UINT8                              SerialPortConfig;
  CM_STD_OBJ_ACPI_TABLE_INFO         *NewAcpiTables;
  CONST CHAR8                        *CompatibiltyString;

  SerialPortConfig = PcdGet8 (PcdSerialPortConfig);

  if (SerialPortConfig == NVIDIA_SERIAL_PORT_DISABLED) {
    return EFI_SUCCESS;
  }

  if (PcdGet8 (PcdSerialTypeConfig) == NVIDIA_SERIAL_PORT_TYPE_16550) {
    CompatibiltyString = "nvidia,tegra20-uart";
  } else {
    CompatibiltyString = "arm,sbsa-uart";
  }

  NumberOfSerialPorts = 0;
  Status              = GetMatchingEnabledDeviceTreeNodes (CompatibiltyString, NULL, &NumberOfSerialPorts);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    // Do not treat no serial ports as an error
    return EFI_SUCCESS;
  }

  SerialHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfSerialPorts);
  if (SerialHandles == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetMatchingEnabledDeviceTreeNodes (CompatibiltyString, SerialHandles, &NumberOfSerialPorts);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SpcrSerialPort = (CM_ARM_SERIAL_PORT_INFO *)AllocateZeroPool (sizeof (CM_ARM_SERIAL_PORT_INFO) * NumberOfSerialPorts);
  if (SpcrSerialPort == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (Index = 0; Index < NumberOfSerialPorts; Index++) {
    // Only one register space is expected
    Size   = 1;
    Status = GetDeviceTreeRegisters (SerialHandles[Index], &RegisterData, &Size);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    // Only one interrupt is expected
    Size   = 1;
    Status = GetDeviceTreeInterrupts (SerialHandles[Index], &InterruptData, &Size);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    SpcrSerialPort[Index].BaseAddress       = RegisterData.BaseAddress;
    SpcrSerialPort[Index].BaseAddressLength = RegisterData.Size;
    SpcrSerialPort[Index].Interrupt         = InterruptData.Interrupt + (InterruptData.Type == INTERRUPT_SPI_TYPE ?
                                                                         DEVICETREE_TO_ACPI_SPI_INTERRUPT_OFFSET :
                                                                         DEVICETREE_TO_ACPI_PPI_INTERRUPT_OFFSET);
    SpcrSerialPort[Index].BaudRate = FixedPcdGet64 (PcdUartDefaultBaudRate);
    if (PcdGet8 (PcdSerialTypeConfig) == NVIDIA_SERIAL_PORT_TYPE_SBSA) {
      SpcrSerialPort[Index].PortSubtype = EFI_ACPI_DBG2_PORT_SUBTYPE_SERIAL_ARM_SBSA_GENERIC_UART;
    } else {
      if (SerialPortConfig == NVIDIA_SERIAL_PORT_SPCR_FULL_16550) {
        SpcrSerialPort[Index].PortSubtype = EFI_ACPI_DBG2_PORT_SUBTYPE_SERIAL_FULL_16550;
      } else {
        SpcrSerialPort[Index].PortSubtype = EFI_ACPI_DBG2_PORT_SUBTYPE_SERIAL_NVIDIA_16550_UART;
      }
    }

    SpcrSerialPort[Index].Clock = FixedPcdGet32 (PL011UartClkInHz);
  }

  FreePool (SerialHandles);

  for (Index = 0; Index < PcdGet32 (PcdConfigMgrObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (NVIDIAPlatformRepositoryInfo[Index].CmObjectSize + (sizeof (CM_STD_OBJ_ACPI_TABLE_INFO)), NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr);
      if (NewAcpiTables == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        return Status;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      if ((SerialPortConfig == NVIDIA_SERIAL_PORT_DBG2_SBSA) ||
          (SerialPortConfig == NVIDIA_SERIAL_PORT_DBG2_NVIDIA_16550))
      {
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_4_DEBUG_PORT_2_TABLE_SIGNATURE;
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = EFI_ACPI_DEBUG_PORT_2_TABLE_REVISION;
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDbg2);
      } else {
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_4_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE;
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_REVISION;
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSpcr);
      }

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData = NULL;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId    = PcdGet64 (PcdAcpiTegraUartOemTableId);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision   = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }

  Repo = *PlatformRepositoryInfo;

  if ((SerialPortConfig == NVIDIA_SERIAL_PORT_DBG2_SBSA) ||
      (SerialPortConfig == NVIDIA_SERIAL_PORT_DBG2_NVIDIA_16550))
  {
    Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjSerialDebugPortInfo);
  } else {
    Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjSerialConsolePortInfo);
  }

  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_ARM_SERIAL_PORT_INFO) * NumberOfSerialPorts;
  Repo->CmObjectCount = NumberOfSerialPorts;
  Repo->CmObjectPtr   = SpcrSerialPort;
  Repo++;

  *PlatformRepositoryInfo = Repo;

  return EFI_SUCCESS;
}

/** Initialize new SSDT table.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
InitializeSsdtTable (
  )
{
  EFI_STATUS                   Status;
  EFI_ACPI_DESCRIPTION_HEADER  SsdtTableHeader;

  SsdtTableHeader.Signature = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
  SsdtTableHeader.Length    = sizeof (EFI_ACPI_DESCRIPTION_HEADER);
  SsdtTableHeader.Revision  = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
  SsdtTableHeader.Checksum  = 0;
  CopyMem (SsdtTableHeader.OemId, PcdGetPtr (PcdAcpiDefaultOemId), sizeof (SsdtTableHeader.OemId));
  SsdtTableHeader.OemTableId      = PcdGet64 (PcdAcpiDefaultOemTableId);
  SsdtTableHeader.OemRevision     = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  SsdtTableHeader.CreatorId       = FixedPcdGet32 (PcdAcpiDefaultCreatorId);
  SsdtTableHeader.CreatorRevision = FixedPcdGet32 (PcdAcpiDefaultCreatorRevision);

  Status = GenerationProtocol->InitializeTable (GenerationProtocol, &SsdtTableHeader);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return GenerationProtocol->StartScope (GenerationProtocol, "_SB");
}

/** Finalize new SSDT table.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
FinalizeSsdtTable (
  )
{
  EFI_STATUS                   Status;
  UINT32                       Index;
  EFI_ACPI_DESCRIPTION_HEADER  *TestTable;
  CM_STD_OBJ_ACPI_TABLE_INFO   *NewAcpiTables;

  Status = GenerationProtocol->EndScope (GenerationProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = GenerationProtocol->GetTable (GenerationProtocol, &TestTable);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < PcdGet32 (PcdConfigMgrObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (NVIDIAPlatformRepositoryInfo[Index].CmObjectSize + sizeof (CM_STD_OBJ_ACPI_TABLE_INFO), NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr);
      if (NewAcpiTables == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = TestTable->Signature;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = TestTable->Revision;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)TestTable;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId         = TestTable->OemTableId;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision        = TestTable->OemRevision;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
      Status                                            = EFI_SUCCESS;
      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      Status = EFI_UNSUPPORTED;
      break;
    }
  }

  return Status;
}

/** Find SDHCI data in the DeviceTree and add to a new SSDT table.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateSdhciInfo (
  )
{
  EFI_STATUS                                     Status;
  UINT32                                         NumberOfSdhciPorts;
  UINT32                                         *SdhciHandles;
  NVIDIA_DEVICE_TREE_REGISTER_DATA               RegisterData;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA              InterruptData;
  UINT32                                         Size;
  UINT32                                         Index;
  CHAR8                                          SdcPathString[ACPI_PATCH_MAX_PATH];
  NVIDIA_AML_NODE_INFO                           AcpiNodeInfo;
  EFI_ACPI_32_BIT_FIXED_MEMORY_RANGE_DESCRIPTOR  MemoryDescriptor;
  EFI_ACPI_EXTENDED_INTERRUPT_DESCRIPTOR         InterruptDescriptor;
  VOID                                           *DeviceTreeBase;
  INT32                                          NodeOffset;
  UINT32                                         Removable;

  NumberOfSdhciPorts = 0;
  Status             = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra234-sdhci", NULL, &NumberOfSdhciPorts);
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  SdhciHandles = NULL;
  SdhciHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfSdhciPorts);
  if (SdhciHandles == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra234-sdhci", SdhciHandles, &NumberOfSdhciPorts);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  for (Index = 0; Index < NumberOfSdhciPorts; Index++) {
    // Only one register space is expected
    Size   = 1;
    Status = GetDeviceTreeRegisters (SdhciHandles[Index], &RegisterData, &Size);
    if (EFI_ERROR (Status)) {
      goto ErrorExit;
    }

    // Only one interrupt is expected
    Size   = 1;
    Status = GetDeviceTreeInterrupts (SdhciHandles[Index], &InterruptData, &Size);
    if (EFI_ERROR (Status)) {
      goto ErrorExit;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, ACPI_SDCT_UID, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_SDCT_UID));
      goto ErrorExit;
    }

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &Index, AcpiNodeInfo.Size);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_SDCT_UID));
      goto ErrorExit;
    }

    GetDeviceTreeNode (SdhciHandles[Index], &DeviceTreeBase, &NodeOffset);
    if (NULL != fdt_getprop (DeviceTreeBase, NodeOffset, "non-removable", NULL)) {
      Removable = 0;
    } else {
      Removable = 1;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, ACPI_SDCT_RMV, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_SDCT_RMV));
      goto ErrorExit;
    }

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &Removable, AcpiNodeInfo.Size);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_SDCT_RMV));
      goto ErrorExit;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, ACPI_SDCT_REG0, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_SDCT_REG0));
      goto ErrorExit;
    }

    if (AcpiNodeInfo.Size != sizeof (MemoryDescriptor)) {
      DEBUG ((DEBUG_ERROR, "%a: Unexpected size of node %a - %d\n", __FUNCTION__, ACPI_SDCT_REG0, AcpiNodeInfo.Size));
      goto ErrorExit;
    }

    Status = PatchProtocol->GetNodeData (PatchProtocol, &AcpiNodeInfo, &MemoryDescriptor, sizeof (MemoryDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get data for %a\n", __FUNCTION__, ACPI_SDCT_REG0));
      goto ErrorExit;
    }

    MemoryDescriptor.BaseAddress = RegisterData.BaseAddress;
    MemoryDescriptor.Length      = RegisterData.Size;

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &MemoryDescriptor, sizeof (MemoryDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_SDCT_REG0));
      goto ErrorExit;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, ACPI_SDCT_INT0, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_SDCT_INT0));
      goto ErrorExit;
    }

    if (AcpiNodeInfo.Size != sizeof (InterruptDescriptor)) {
      DEBUG ((DEBUG_ERROR, "%a: Unexpected size of node %a - %d\n", __FUNCTION__, ACPI_SDCT_INT0, AcpiNodeInfo.Size));
      goto ErrorExit;
    }

    Status = PatchProtocol->GetNodeData (PatchProtocol, &AcpiNodeInfo, &InterruptDescriptor, sizeof (InterruptDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get data for %a\n", __FUNCTION__, ACPI_SDCT_INT0));
      goto ErrorExit;
    }

    InterruptDescriptor.InterruptNumber[0] = InterruptData.Interrupt + (InterruptData.Type == INTERRUPT_SPI_TYPE ?
                                                                        DEVICETREE_TO_ACPI_SPI_INTERRUPT_OFFSET :
                                                                        DEVICETREE_TO_ACPI_PPI_INTERRUPT_OFFSET);

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &InterruptDescriptor, sizeof (InterruptDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_SDCT_INT0));
      goto ErrorExit;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, "SDCT", &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, "SDCT"));
      goto ErrorExit;
    }

    AsciiSPrint (SdcPathString, sizeof (SdcPathString), "SDC%u", Index);
    Status = PatchProtocol->UpdateNodeName (PatchProtocol, &AcpiNodeInfo, SdcPathString);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to update name to %a\n", __FUNCTION__, SdcPathString));
      goto ErrorExit;
    }

    Status = GenerationProtocol->AppendDevice (GenerationProtocol, (EFI_ACPI_DESCRIPTION_HEADER *)sdctemplate_aml_code);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to append device %a\n", __FUNCTION__, SdcPathString));
      goto ErrorExit;
    }
  }

ErrorExit:
  if (SdhciHandles != NULL) {
    FreePool (SdhciHandles);
  }

  return Status;
}

/** Initialize the platform configuration repository.
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
InitializePlatformRepository (
  VOID
  )
{
  UINTN       Index;
  EFI_STATUS  Status;

  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  EDKII_PLATFORM_REPOSITORY_INFO  *RepoEnd;

  NVIDIAPlatformRepositoryInfo = (EDKII_PLATFORM_REPOSITORY_INFO *)AllocateZeroPool (sizeof (EDKII_PLATFORM_REPOSITORY_INFO) * PcdGet32 (PcdConfigMgrObjMax));
  if (NVIDIAPlatformRepositoryInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Repo    = NVIDIAPlatformRepositoryInfo;
  RepoEnd = Repo + PcdGet32 (PcdConfigMgrObjMax);

  Repo->CmObjectId    = CREATE_CM_STD_OBJECT_ID (EStdObjCfgMgrInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CmInfo);
  Repo->CmObjectCount = sizeof (CmInfo) / sizeof (CM_STD_OBJ_CONFIGURATION_MANAGER_INFO);
  Repo->CmObjectPtr   = &CmInfo;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CmAcpiTableList);
  Repo->CmObjectCount = sizeof (CmAcpiTableList) / sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
  Repo->CmObjectPtr   = &CmAcpiTableList;
  for (Index = 0; Index < Repo->CmObjectCount; Index++) {
    if (CmAcpiTableList[Index].AcpiTableSignature != EFI_ACPI_6_4_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE) {
      CmAcpiTableList[Index].OemTableId =  PcdGet64 (PcdAcpiDefaultOemTableId);
    }
  }

  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjBootArchInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (BootArchInfo);
  Repo->CmObjectCount = sizeof (BootArchInfo) / sizeof (CM_ARM_BOOT_ARCH_INFO);
  Repo->CmObjectPtr   = &BootArchInfo;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjPowerManagementProfileInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (PmProfileInfo);
  Repo->CmObjectCount = sizeof (PmProfileInfo) / sizeof (CM_ARM_POWER_MANAGEMENT_PROFILE_INFO);
  Repo->CmObjectPtr   = &PmProfileInfo;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjGenericTimerInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (GenericTimerInfo);
  Repo->CmObjectCount = sizeof (GenericTimerInfo) / sizeof (CM_ARM_GENERIC_TIMER_INFO);
  Repo->CmObjectPtr   = &GenericTimerInfo;
  Repo++;

  Status = UpdateSerialPortInfo (&Repo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateGicInfo (&Repo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = RegisterProtocolBasedObjects (NVIDIAPlatformRepositoryInfo, &Repo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = InitializeSsdtTable ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateSdhciInfo ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = FinalizeSsdtTable ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT ((UINTN)Repo <= (UINTN)RepoEnd);

  return EFI_SUCCESS;
}

/**
  Entrypoint of Configuration Manager Data Dxe.

  @param  ImageHandle
  @param  SystemTable

  @return EFI_SUCCESS
  @return EFI_LOAD_ERROR
  @return EFI_OUT_OF_RESOURCES

**/
EFI_STATUS
EFIAPI
ConfigurationManagerDataDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINTN       ChipID;
  EFI_STATUS  Status;

  ChipID = TegraGetChipID ();
  if (ChipID != T234_CHIP_ID) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gNVIDIAAmlPatchProtocolGuid, NULL, (VOID **)&PatchProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->LocateProtocol (&gNVIDIAAmlGenerationProtocolGuid, NULL, (VOID **)&GenerationProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PatchProtocol->RegisterAmlTables (
                            PatchProtocol,
                            AcpiTableArray,
                            OffsetTableArray,
                            ARRAY_SIZE (AcpiTableArray)
                            );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = InitializePlatformRepository ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return gBS->InstallMultipleProtocolInterfaces (
                &ImageHandle,
                &gNVIDIAConfigurationManagerDataProtocolGuid,
                (VOID *)NVIDIAPlatformRepositoryInfo,
                NULL
                );
}
