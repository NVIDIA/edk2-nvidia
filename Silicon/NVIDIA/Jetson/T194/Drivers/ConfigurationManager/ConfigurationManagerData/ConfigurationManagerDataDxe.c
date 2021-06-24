/** @file
  Configuration Manager Data Dxe

  Copyright (c) 2019 - 2021, NVIDIA Corporation. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/
#include <ConfigurationManagerDataDxePrivate.h>

//Callback for AHCI controller connection
EFI_EVENT  EndOfDxeEvent;
EFI_HANDLE PciControllerHandle = 0;

//AML Patch protocol
NVIDIA_AML_PATCH_PROTOCOL      *PatchProtocol = NULL;
NVIDIA_AML_GENERATION_PROTOCOL *GenerationProtocol = NULL;

STATIC EFI_ACPI_DESCRIPTION_HEADER *AcpiTableArray[] = {
    (EFI_ACPI_DESCRIPTION_HEADER *)dsdt_aml_code,
    (EFI_ACPI_DESCRIPTION_HEADER *)ssdtpci_aml_code,
    (EFI_ACPI_DESCRIPTION_HEADER *)sdctemplate_aml_code
};

STATIC AML_OFFSET_TABLE_ENTRY *OffsetTableArray[] = {
    DSDT_TEGRA194_OffsetTable,
    SSDT_TEGRA194_OffsetTable,
    SSDT_SDCTEMP_OffsetTable
};

/** The platform configuration repository information.
*/
STATIC
EDKII_PLATFORM_REPOSITORY_INFO NVIDIAPlatformRepositoryInfo[EStdObjMax + EArmObjMax] = {0};

/** The platform configuration manager information.
*/
STATIC
CM_STD_OBJ_CONFIGURATION_MANAGER_INFO CmInfo = {
  CONFIGURATION_MANAGER_REVISION,
  CFG_MGR_OEM_ID
};

/** The platform ACPI table list.
*/
STATIC
CM_STD_OBJ_ACPI_TABLE_INFO CmAcpiTableList[] = {
  // FADT Table
  {
    EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdFadt),
    NULL,
    0,
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // GTDT Table
  {
    EFI_ACPI_6_3_GENERIC_TIMER_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_3_GENERIC_TIMER_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdGtdt),
    NULL,
    0,
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // MADT Table
  {
    EFI_ACPI_6_3_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_3_MULTIPLE_APIC_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMadt),
    NULL,
    0,
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // DSDT Table
  {
    EFI_ACPI_6_3_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_3_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDsdt),
    (EFI_ACPI_DESCRIPTION_HEADER*)dsdt_aml_code,
    0,
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // PPTT Table
  {
    EFI_ACPI_6_3_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_STRUCTURE_SIGNATURE,
    EFI_ACPI_6_3_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdPptt),
    NULL,
    0,
    FixedPcdGet64(PcdAcpiDefaultOemRevision),
  },
};

/** The platform boot architecture information.
*/
STATIC
CM_ARM_BOOT_ARCH_INFO BootArchInfo = {
  EFI_ACPI_6_3_ARM_PSCI_COMPLIANT
};

/** The platform boot architecture information.
*/
CM_ARM_FIXED_FEATURE_FLAGS            FixedFeatureFlags = {
  EFI_ACPI_6_3_PWR_BUTTON
};

/** The platform power management profile information.
*/
STATIC
CM_ARM_POWER_MANAGEMENT_PROFILE_INFO PmProfileInfo = {
  EFI_ACPI_6_3_PM_PROFILE_ENTERPRISE_SERVER
};

/** The platform GIC distributor information.
*/
STATIC
CM_ARM_GICD_INFO GicDInfo = {
  0,
  0,
  2
};

/** The platform generic timer information.
*/
STATIC
CM_ARM_GENERIC_TIMER_INFO GenericTimerInfo = {
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

/** Cache Info
 */
STATIC
CM_ARM_CACHE_INFO CacheInfo[] = {
  // L3 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[0]),
    .NextLevelOfCacheToken = CM_NULL_TOKEN,
    .Size                  = 0x400000,
    .NumberOfSets          = 4096,
    .Associativity         = 16,
    .Attributes            = CACHE_ATTRIBUTES (
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
    ),
    .LineSize              = 64,
  },
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[1]),
    .NextLevelOfCacheToken = CM_NULL_TOKEN,
    .Size                  = 0x200000,
    .NumberOfSets          = 2048,
    .Associativity         = 16,
    .Attributes            = CACHE_ATTRIBUTES (
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
    ),
    .LineSize              = 64,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[2]),
    .NextLevelOfCacheToken = CM_NULL_TOKEN,
    .Size                  = 0x20000,
    .NumberOfSets          = 512,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_ALLOCATION_READ,
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
    ),
    .LineSize              = 64,
  },
  // L1D Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[3]),
    .NextLevelOfCacheToken = CM_NULL_TOKEN,
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
    ),
    .LineSize              = 64,
  },
};

/** CCPLEX Resources
 */
STATIC
CM_ARM_OBJ_REF CcplexResources[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[0]) },
};

/** Carmel Core Cluster Resources
 */
STATIC
CM_ARM_OBJ_REF CarmelCoreClusterResources[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[1]) },
};

/** Carmel Core Resources
 */
STATIC
CM_ARM_OBJ_REF CarmelCoreResources[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[2]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[3]) },
};

/** Check if Pcie is enabled is kernel.
  @retval TRUE  - Enabled
  @retval FALSE - Disabled
 */
STATIC
BOOLEAN
EFIAPI
IsPcieEnabled ()
{
  return (PcdGet8 (PcdPcieEntryInAcpi) == 1);
}

/** Initialize the PCIe entries in the platform configuration repository and patch SSDT.
 *
 * @param Repo Pointer to a repo structure that will be added to and updated with the data updated
 *
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
UpdatePcieInfo (EDKII_PLATFORM_REPOSITORY_INFO **PlatformRepositoryInfo)
{
  EFI_STATUS                        Status;
  UINT32                            NumberOfPcieControllers;
  UINT32                            *PcieHandles;
  EDKII_PLATFORM_REPOSITORY_INFO    *Repo;
  UINT32                            Index;
  CM_ARM_PCI_CONFIG_SPACE_INFO      *PciConfigInfo;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  *RegisterData;
  UINT32                            RegisterIndex;
  UINT32                            RegisterSize;
  VOID                              *DeviceTreeBase;
  INT32                             NodeOffset;
  CONST UINT32                      *SegmentProp;
  CM_STD_OBJ_ACPI_TABLE_INFO        *NewAcpiTables;
  CHAR8                             AcpiPathString[ACPI_PATCH_MAX_PATH];
  NVIDIA_AML_NODE_INFO              AcpiNodeInfo;
  UINT8                             AcpiStatus;

  PciConfigInfo = NULL;
  PcieHandles = NULL;
  RegisterData = NULL;

  NumberOfPcieControllers = 0;
  Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra194-pcie", NULL, &NumberOfPcieControllers);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "No PCIe controller devices found\r\n"));
    Status = EFI_SUCCESS;
    goto Exit;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  PcieHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfPcieControllers);
  if (PcieHandles == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra194-pcie", PcieHandles, &NumberOfPcieControllers);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  PciConfigInfo = (CM_ARM_PCI_CONFIG_SPACE_INFO *)AllocatePool (sizeof (CM_ARM_PCI_CONFIG_SPACE_INFO) * NumberOfPcieControllers);
  if (PciConfigInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  RegisterSize = 0;
  for (Index = 0; Index < NumberOfPcieControllers; Index++) {
    Status = GetDeviceTreeRegisters (PcieHandles[Index], RegisterData, &RegisterSize);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      if (RegisterData != NULL) {
        FreePool (RegisterData);
        RegisterData = NULL;
      }
      RegisterData = (NVIDIA_DEVICE_TREE_REGISTER_DATA *) AllocatePool (sizeof (NVIDIA_DEVICE_TREE_REGISTER_DATA) * RegisterSize);
      if (RegisterData == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }
      Status = GetDeviceTreeRegisters (PcieHandles[Index], RegisterData, &RegisterSize);
      if (EFI_ERROR (Status)) {
        goto Exit;
      }
    } else if (EFI_ERROR (Status)) {
      goto Exit;
    }

    if (RegisterData == NULL) {
      ASSERT (FALSE);
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    for (RegisterIndex = 0; RegisterIndex < RegisterSize; RegisterIndex++) {
      if ((RegisterData[RegisterIndex].Name != NULL) &&
          (AsciiStrCmp (RegisterData[RegisterIndex].Name, "config") == 0)) {
        break;
      }
    }
    if (RegisterIndex == RegisterSize) {
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }

    Status = GetDeviceTreeNode (PcieHandles[Index], &DeviceTreeBase, &NodeOffset);
    if (EFI_ERROR (Status)) {
      goto Exit;
    }
    SegmentProp = fdt_getprop (DeviceTreeBase, NodeOffset, "linux,pci-domain", NULL);
    if (SegmentProp == NULL) {
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }

    PciConfigInfo[Index].BaseAddress = RegisterData[RegisterIndex].BaseAddress;
    PciConfigInfo[Index].StartBusNumber = T194_PCIE_BUS_MIN;
    PciConfigInfo[Index].EndBusNumber = T194_PCIE_BUS_MAX;
    PciConfigInfo[Index].PciSegmentGroupNumber = SwapBytes32 (*SegmentProp);
    //Attempt to locate the pcie entry in DSDT
    AsciiSPrint (AcpiPathString, sizeof (AcpiPathString), ACPI_PCI_STA_TEMPLATE, PciConfigInfo[Index].PciSegmentGroupNumber);
    Status = PatchProtocol->FindNode (PatchProtocol, AcpiPathString, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to find node %a, skipping patch\r\n", __FUNCTION__, AcpiPathString));
      continue;
    }
    if (AcpiNodeInfo.Size != sizeof (AcpiStatus)) {
      DEBUG ((DEBUG_ERROR, "%a: Unexpected size of node %a - %d, skipping patch\r\n", __FUNCTION__, AcpiPathString, AcpiNodeInfo.Size));
      continue;
    }

    AcpiStatus = 0xF;
    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &AcpiStatus, sizeof (AcpiStatus));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, AcpiPathString, Status));
    }
  }

  for (Index = 0; Index < (EStdObjMax + EArmObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (NVIDIAPlatformRepositoryInfo[Index].CmObjectSize + (2 * sizeof (CM_STD_OBJ_ACPI_TABLE_INFO)), NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr);
      if (NewAcpiTables == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision = EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData = (EFI_ACPI_DESCRIPTION_HEADER *)ssdtpci_aml_code;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId = PcdGet64(PcdAcpiDefaultOemTableId);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision = FixedPcdGet64(PcdAcpiDefaultOemRevision);
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_3_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision = EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_REVISION;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMcfg);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData = NULL;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId = PcdGet64(PcdAcpiDefaultOemTableId);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision = FixedPcdGet64(PcdAcpiDefaultOemRevision);
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }

  Repo = *PlatformRepositoryInfo;

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjPciConfigSpaceInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (CM_ARM_PCI_CONFIG_SPACE_INFO) * NumberOfPcieControllers;
  Repo->CmObjectCount = NumberOfPcieControllers;
  Repo->CmObjectPtr = PciConfigInfo;
  Repo++;

  *PlatformRepositoryInfo = Repo;

Exit:
  if (EFI_ERROR (Status)) {
    if (PciConfigInfo != NULL) {
      FreePool (PciConfigInfo);
    }
  }
  if (PcieHandles != NULL) {
    FreePool (PcieHandles);
  }
  if (RegisterData != NULL) {
    FreePool (RegisterData);
  }
  return Status;
}

//Callback to connect PCIe controller as this is needed if exposed as direct ACPI node and we didn't boot off it
STATIC
VOID
EFIAPI
OnEndOfDxe (
  EFI_EVENT           Event,
  VOID                *Context
  )
{
  gBS->ConnectController (PciControllerHandle, NULL, NULL, TRUE);
}


/** Initialize the AHCI entries in the platform configuration repository and patch SSDT.
 *
 * @param Repo Pointer to a repo structure that will be added to and updated with the data updated
 *
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
UpdateAhciInfo (EDKII_PLATFORM_REPOSITORY_INFO **PlatformRepositoryInfo)
{
  EFI_STATUS                        Status;
  UINT32                            NumberOfPlatformNodes;
  UINT32                            Index;
  CM_STD_OBJ_ACPI_TABLE_INFO        *NewAcpiTables;
  UINTN                             NumOfHandles;
  EFI_HANDLE                        *HandleBuffer = NULL;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL   *RootBridgeIo;
  BOOLEAN                           PciControllerConnected = FALSE;

  NumberOfPlatformNodes = 0;
  Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,p2972-0000", NULL, &NumberOfPlatformNodes);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "AHCI support not present on this platform\r\n"));
    Status = EFI_SUCCESS;
    goto Exit;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  Status = gBS->LocateHandleBuffer (ByProtocol,
                                    &gEfiPciRootBridgeIoProtocolGuid,
                                    NULL,
                                    &NumOfHandles,
                                    &HandleBuffer);
  if (EFI_ERROR (Status) || (NumOfHandles == 0)) {
    DEBUG ((DEBUG_ERROR, "%a, Failed to LocateHandleBuffer %r\r\n", __FUNCTION__, Status));
    Status = EFI_SUCCESS;
    goto Exit;
  }

  for (Index = 0; Index < NumOfHandles; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index],
                                  &gEfiPciRootBridgeIoProtocolGuid,
                                  (VOID **)&RootBridgeIo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a, Failed to handle protocol %r\r\n", __FUNCTION__, Status));
      continue;
    }

    if (RootBridgeIo->SegmentNumber == AHCI_PCIE_SEGMENT) {
      PciControllerHandle = HandleBuffer[Index];
      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_CALLBACK,
                      OnEndOfDxe,
                      NULL,
                      &gEfiEndOfDxeEventGroupGuid,
                      &EndOfDxeEvent);
      ASSERT_EFI_ERROR (Status);
      PciControllerConnected = TRUE;
      break;
    }
  }

  if (!PciControllerConnected) {
    Status = EFI_SUCCESS;
    goto Exit;
  }

  for (Index = 0; Index < (EStdObjMax + EArmObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (NVIDIAPlatformRepositoryInfo[Index].CmObjectSize + (sizeof (CM_STD_OBJ_ACPI_TABLE_INFO)), NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr);
      if (NewAcpiTables == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision = EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData = (EFI_ACPI_DESCRIPTION_HEADER *)ssdtahci_aml_code;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId = PcdGet64(PcdAcpiDefaultOemTableId);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision = FixedPcdGet64(PcdAcpiDefaultOemRevision);
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }

  Status = EFI_SUCCESS;

Exit:
  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
  }
  return Status;
}

/** Initialize the Serial Port entries in the platform configuration repository and patch DSDT.
 *
 * @param Repo Pointer to a repo structure that will be added to and updated with the data updated
 *
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
UpdateSerialPortInfo (EDKII_PLATFORM_REPOSITORY_INFO **PlatformRepositoryInfo)
{
  EFI_STATUS                        Status;
  UINT32                            NumberOfSerialPorts;
  UINT32                            *SerialHandles;
  EDKII_PLATFORM_REPOSITORY_INFO    *Repo;
  CM_ARM_SERIAL_PORT_INFO           *SpcrSerialPort;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  RegisterData;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA InterruptData;
  UINT32                            Index;
  UINT32                            Size;
  UINT8                             SerialPortConfig;
  CM_STD_OBJ_ACPI_TABLE_INFO        *NewAcpiTables;


  SerialPortConfig = PcdGet8 (PcdSerialPortConfig);
  if (PcdGet8 (PcdSerialTypeConfig) != NVIDIA_SERIAL_PORT_TYPE_16550 ||
      SerialPortConfig == NVIDIA_SERIAL_PORT_DISABLED) {
    return EFI_SUCCESS;
  }

  NumberOfSerialPorts = 0;
  Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra20-uart", NULL, &NumberOfSerialPorts);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  SerialHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfSerialPorts);
  if (SerialHandles == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra20-uart", SerialHandles, &NumberOfSerialPorts);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SpcrSerialPort = (CM_ARM_SERIAL_PORT_INFO *)AllocateZeroPool (sizeof (CM_ARM_SERIAL_PORT_INFO) * NumberOfSerialPorts);
  if (SpcrSerialPort == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (Index = 0; Index < NumberOfSerialPorts; Index++) {

    //Only one register space is expected
    Size = 1;
    Status = GetDeviceTreeRegisters (SerialHandles[Index], &RegisterData, &Size);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    //Only one interrupt is expected
    Size = 1;
    Status = GetDeviceTreeInterrupts (SerialHandles[Index], &InterruptData, &Size);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    SpcrSerialPort[Index].BaseAddress = RegisterData.BaseAddress;
    SpcrSerialPort[Index].BaseAddressLength = RegisterData.Size;
    SpcrSerialPort[Index].Interrupt = InterruptData.Interrupt + DEVICETREE_TO_ACPI_INTERRUPT_OFFSET;
    SpcrSerialPort[Index].BaudRate = FixedPcdGet64 (PcdUartDefaultBaudRate);
    if (SerialPortConfig == NVIDIA_SERIAL_PORT_SPCR_FULL_16550) {
      SpcrSerialPort[Index].PortSubtype = EFI_ACPI_DBG2_PORT_SUBTYPE_SERIAL_FULL_16550;
    } else {
      SpcrSerialPort[Index].PortSubtype = EFI_ACPI_DBG2_PORT_SUBTYPE_SERIAL_NVIDIA_16550_UART;
    }
    SpcrSerialPort[Index].Clock = 0;
  }
  FreePool (SerialHandles);

  for (Index = 0; Index < (EStdObjMax + EArmObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (NVIDIAPlatformRepositoryInfo[Index].CmObjectSize + (sizeof (CM_STD_OBJ_ACPI_TABLE_INFO)), NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr);
      if (NewAcpiTables == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        return Status;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      if (SerialPortConfig == NVIDIA_SERIAL_PORT_DBG2_NVIDIA_16550) {
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_3_DEBUG_PORT_2_TABLE_SIGNATURE;
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision = EFI_ACPI_DEBUG_PORT_2_TABLE_REVISION;
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDbg2);
      } else {
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_3_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE;
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision = EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_REVISION;
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSpcr);
      }
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData = NULL;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId = PcdGet64(PcdAcpiTegraUartOemTableId);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision = FixedPcdGet64(PcdAcpiDefaultOemRevision);
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }


  Repo = *PlatformRepositoryInfo;

  if (SerialPortConfig == NVIDIA_SERIAL_PORT_DBG2_NVIDIA_16550) {
    Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjSerialDebugPortInfo);
  } else {
    Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjSerialConsolePortInfo);
  }
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (CM_ARM_SERIAL_PORT_INFO);
  Repo->CmObjectCount = NumberOfSerialPorts;
  Repo->CmObjectPtr = SpcrSerialPort;
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
InitializeSsdtTable ()
{
  EFI_STATUS                  Status;
  EFI_ACPI_DESCRIPTION_HEADER SsdtTableHeader;

  SsdtTableHeader.Signature = EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
  SsdtTableHeader.Length = sizeof(EFI_ACPI_DESCRIPTION_HEADER);
  SsdtTableHeader.Revision = EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
  SsdtTableHeader.Checksum = 0;
  CopyMem(SsdtTableHeader.OemId, PcdGetPtr(PcdAcpiDefaultOemId), sizeof(SsdtTableHeader.OemId));
  SsdtTableHeader.OemTableId = PcdGet64(PcdAcpiDefaultOemTableId);
  SsdtTableHeader.OemRevision = FixedPcdGet64(PcdAcpiDefaultOemRevision);
  SsdtTableHeader.CreatorId = FixedPcdGet32(PcdAcpiDefaultCreatorId);
  SsdtTableHeader.CreatorRevision = FixedPcdGet32(PcdAcpiDefaultCreatorRevision);

  Status = GenerationProtocol->InitializeTable(GenerationProtocol, &SsdtTableHeader);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return GenerationProtocol->StartScope(GenerationProtocol, "_SB");
}

/** Finalize new SSDT table.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
FinalizeSsdtTable ()
{
  EFI_STATUS                  Status;
  UINT32                      Index;
  EFI_ACPI_DESCRIPTION_HEADER *TestTable;
  CM_STD_OBJ_ACPI_TABLE_INFO  *NewAcpiTables;

  Status = GenerationProtocol->EndScope(GenerationProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = GenerationProtocol->GetTable(GenerationProtocol, &TestTable);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < (EStdObjMax + EArmObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (NVIDIAPlatformRepositoryInfo[Index].CmObjectSize + sizeof (CM_STD_OBJ_ACPI_TABLE_INFO), NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr);
      if (NewAcpiTables == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = TestTable->Signature;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision = TestTable->Revision;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData = (EFI_ACPI_DESCRIPTION_HEADER *)TestTable;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId = TestTable->OemTableId;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision = TestTable->OemRevision;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
      Status = EFI_SUCCESS;
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
UpdateSdhciInfo ()
{
  EFI_STATUS                                    Status;
  UINT32                                        NumberOfSdhciPorts;
  UINT32                                        *SdhciHandles;
  NVIDIA_DEVICE_TREE_REGISTER_DATA              RegisterData;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA             InterruptData;
  UINT32                                        Size;
  UINT32                                        Index;
  CHAR8                                         SdcPathString[ACPI_PATCH_MAX_PATH];
  NVIDIA_AML_NODE_INFO                          AcpiNodeInfo;
  EFI_ACPI_32_BIT_FIXED_MEMORY_RANGE_DESCRIPTOR MemoryDescriptor;
  EFI_ACPI_EXTENDED_INTERRUPT_DESCRIPTOR        InterruptDescriptor;

  NumberOfSdhciPorts = 0;
  Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra194-sdhci", NULL, &NumberOfSdhciPorts);
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

  Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra194-sdhci", SdhciHandles, &NumberOfSdhciPorts);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  for (Index = 0; Index < NumberOfSdhciPorts; Index++) {
    // Only one register space is expected
    Size = 1;
    Status = GetDeviceTreeRegisters (SdhciHandles[Index], &RegisterData, &Size);
    if (EFI_ERROR (Status)) {
      goto ErrorExit;
    }

    // Only one interrupt is expected
    Size = 1;
    Status = GetDeviceTreeInterrupts (SdhciHandles[Index], &InterruptData, &Size);
    if (EFI_ERROR (Status)) {
      goto ErrorExit;
    }

    Status = PatchProtocol->FindNode(PatchProtocol, ACPI_SDCT_UID, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_SDCT_UID));
      goto ErrorExit;
    }

    Status = PatchProtocol->SetNodeData(PatchProtocol, &AcpiNodeInfo, &Index, AcpiNodeInfo.Size);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_SDCT_UID));
      goto ErrorExit;
    }


    Status = PatchProtocol->FindNode(PatchProtocol, ACPI_SDCT_REG0, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_SDCT_REG0));
      goto ErrorExit;
    }
    if (AcpiNodeInfo.Size != sizeof (MemoryDescriptor)) {
      DEBUG ((DEBUG_ERROR, "%a: Unexpected size of node %a - %d\n", __FUNCTION__, ACPI_SDCT_REG0, AcpiNodeInfo.Size));
      goto ErrorExit;
    }

    Status = PatchProtocol->GetNodeData(PatchProtocol, &AcpiNodeInfo, &MemoryDescriptor, sizeof (MemoryDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get data for %a\n", __FUNCTION__, ACPI_SDCT_REG0));
      goto ErrorExit;
    }


    MemoryDescriptor.BaseAddress = RegisterData.BaseAddress;
    MemoryDescriptor.Length = RegisterData.Size;

    Status = PatchProtocol->SetNodeData(PatchProtocol, &AcpiNodeInfo, &MemoryDescriptor, sizeof (MemoryDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_SDCT_REG0));
      goto ErrorExit;
    }

    Status = PatchProtocol->FindNode(PatchProtocol, ACPI_SDCT_INT0, &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, ACPI_SDCT_INT0));
      goto ErrorExit;
    }
    if (AcpiNodeInfo.Size != sizeof (InterruptDescriptor)) {
      DEBUG ((DEBUG_ERROR, "%a: Unexpected size of node %a - %d\n", __FUNCTION__, ACPI_SDCT_INT0, AcpiNodeInfo.Size));
      goto ErrorExit;
    }

    Status = PatchProtocol->GetNodeData(PatchProtocol, &AcpiNodeInfo, &InterruptDescriptor, sizeof (InterruptDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get data for %a\n", __FUNCTION__, ACPI_SDCT_INT0));
      goto ErrorExit;
    }

    // Interrupts in the device tree are encoded relative to a starting address of 0x20
    InterruptDescriptor.InterruptNumber[0] = InterruptData.Interrupt + 0x20;

    Status = PatchProtocol->SetNodeData(PatchProtocol, &AcpiNodeInfo, &InterruptDescriptor, sizeof (InterruptDescriptor));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to set data for %a\n", __FUNCTION__, ACPI_SDCT_INT0));
      goto ErrorExit;
    }

    Status = PatchProtocol->FindNode(PatchProtocol, "SDCT", &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to find the node %a\n", __FUNCTION__, "SDCT"));
      goto ErrorExit;
    }

    AsciiSPrint (SdcPathString, sizeof (SdcPathString), "SDC%d", Index);
    Status = PatchProtocol->UpdateNodeName(PatchProtocol, &AcpiNodeInfo, SdcPathString);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to update name to %a\n", __FUNCTION__, SdcPathString));
      goto ErrorExit;
    }

    Status = GenerationProtocol->AppendDevice(GenerationProtocol, (EFI_ACPI_DESCRIPTION_HEADER *)sdctemplate_aml_code);
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

/** patch Fan data in DSDT.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateFanInfo ()
{
  EFI_STATUS                        Status;
  INT32                             FanOffset;
  UINT32                            FanHandle;
  INT32                             PwmOffset;
  CONST UINT32                      *FanPwm;
  INT32                             PwmLength;
  UINT32                            FanPwmHandle;
  VOID                              *DeviceTreeBase;
  UINT32                            PwmHandle;
  NVIDIA_DEVICE_TREE_REGISTER_DATA  RegisterData;
  UINT32                            Size;
  NVIDIA_AML_NODE_INFO              AcpiNodeInfo;
  UINT8                             FanStatus;

  Size = 1;
  Status = GetMatchingEnabledDeviceTreeNodes ("pwm-fan", &FanHandle, &Size);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = GetDeviceTreeNode (FanHandle, &DeviceTreeBase, &FanOffset);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FanPwm = fdt_getprop (DeviceTreeBase, FanOffset, "pwms", &PwmLength);
  if (FanPwm == NULL) {
    return EFI_SUCCESS;
  }

  if (PwmLength < sizeof (UINT32)){
    return EFI_SUCCESS;
  }

  FanPwmHandle = SwapBytes32 (FanPwm[0]);
  PwmOffset = fdt_node_offset_by_phandle (DeviceTreeBase, FanPwmHandle);
  if (PwmOffset < 0) {
    return EFI_UNSUPPORTED;
  }

  Status = GetDeviceTreeHandle (DeviceTreeBase, PwmOffset, &PwmHandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //Only one register space is expected
  Size = 1;
  Status = GetDeviceTreeRegisters (PwmHandle, &RegisterData, &Size);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PatchProtocol->FindNode (PatchProtocol, ACPI_FAN_FANR, &AcpiNodeInfo);
  if (EFI_ERROR (Status)) {
    //If fan node isn't in ACPI return success as there is nothing to patch
    return EFI_SUCCESS;
  }

  if (AcpiNodeInfo.Size > sizeof (RegisterData.Size)) {
    return EFI_DEVICE_ERROR;
  }

  Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &RegisterData.BaseAddress, AcpiNodeInfo.Size);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, ACPI_FAN_FANR, Status));
  }

  Status = PatchProtocol->FindNode (PatchProtocol, ACPI_FAN_STA, &AcpiNodeInfo);
  if (EFI_ERROR (Status)) {
    //If fan node isn't in ACPI return success as there is nothing to patch
    return EFI_SUCCESS;
  }

  if (AcpiNodeInfo.Size > sizeof (FanStatus)) {
    return EFI_DEVICE_ERROR;
  }

  FanStatus = 0xF;
  Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &FanStatus, sizeof(FanStatus));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, ACPI_FAN_STA, Status));
  }
  return Status;
}

/** Initialize the cpu entries in the platform configuration repository.
 *
 * @param Repo Pointer to a repo structure that will be added to and updated with the data updated
 *
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
UpdateCpuInfo (EDKII_PLATFORM_REPOSITORY_INFO **PlatformRepositoryInfo)
{
  UINT32 NumCpus;
  UINT32 Index;
  UINT32 ProcHierarchyIndex;
  CM_ARM_GICC_INFO *GicCInfo;
  CM_ARM_PROC_HIERARCHY_INFO *ProcHierarchyInfo;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  CM_OBJECT_TOKEN *ClusterTokenMap;
  UINT32 MpIdr;

  Repo = *PlatformRepositoryInfo;

  NumCpus = GetNumberOfEnabledCpuCores ();

  GicCInfo = AllocateZeroPool (sizeof (CM_ARM_GICC_INFO) * NumCpus);
  if (GicCInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //Can not have more unique clusters than cpus
  ProcHierarchyInfo = AllocateZeroPool (sizeof (CM_ARM_PROC_HIERARCHY_INFO) * (NumCpus + NumCpus + 1));
  if (ProcHierarchyInfo == NULL) {
    FreePool (GicCInfo);
    return EFI_OUT_OF_RESOURCES;
  }

  ClusterTokenMap = (CM_OBJECT_TOKEN *)AllocateZeroPool (sizeof (CM_OBJECT_TOKEN) * 0x100);
  if (ClusterTokenMap == NULL) {
    FreePool (GicCInfo);
    FreePool (ProcHierarchyInfo);
    return EFI_OUT_OF_RESOURCES;
  }

  //Build top level node
  ProcHierarchyIndex = 0;
  ProcHierarchyInfo[ProcHierarchyIndex].Token         = REFERENCE_TOKEN (ProcHierarchyInfo[ProcHierarchyIndex]);
  ProcHierarchyInfo[ProcHierarchyIndex].Flags         = PROC_NODE_FLAGS (
                                                          EFI_ACPI_6_3_PPTT_PACKAGE_PHYSICAL,
                                                          EFI_ACPI_6_3_PPTT_PROCESSOR_ID_INVALID,
                                                          EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                          EFI_ACPI_6_3_PPTT_NODE_IS_NOT_LEAF,
                                                          EFI_ACPI_6_3_PPTT_IMPLEMENTATION_IDENTICAL
                                                        );
  ProcHierarchyInfo[ProcHierarchyIndex].ParentToken   = CM_NULL_TOKEN;
  ProcHierarchyInfo[ProcHierarchyIndex].GicCToken     = CM_NULL_TOKEN;
  ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources = ARRAY_SIZE (CcplexResources);
  ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = REFERENCE_TOKEN (CcplexResources);
  ProcHierarchyIndex++;

  for (Index = 0; Index < NumCpus; Index++) {
    MpIdr = ConvertCpuLogicalToMpidr (Index);
    if (ClusterTokenMap [GET_CLUSTER_ID (MpIdr)] == 0) {
      //Build cluster node
      ProcHierarchyInfo[ProcHierarchyIndex].Token         = REFERENCE_TOKEN (ProcHierarchyInfo[ProcHierarchyIndex]);
      ProcHierarchyInfo[ProcHierarchyIndex].Flags         = PROC_NODE_FLAGS (
                                                              EFI_ACPI_6_3_PPTT_PACKAGE_NOT_PHYSICAL,
                                                              EFI_ACPI_6_3_PPTT_PROCESSOR_ID_INVALID,
                                                              EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                              EFI_ACPI_6_3_PPTT_NODE_IS_NOT_LEAF,
                                                              EFI_ACPI_6_3_PPTT_IMPLEMENTATION_IDENTICAL
                                                            );
      ProcHierarchyInfo[ProcHierarchyIndex].ParentToken   = REFERENCE_TOKEN (ProcHierarchyInfo[0]);
      ProcHierarchyInfo[ProcHierarchyIndex].GicCToken     = CM_NULL_TOKEN;
      ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources = ARRAY_SIZE (CarmelCoreClusterResources);
      ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreClusterResources);
      ClusterTokenMap [GET_CLUSTER_ID (MpIdr)] = ProcHierarchyInfo[ProcHierarchyIndex].Token;
      ProcHierarchyIndex++;
    }

    //Build cpu core node
    ProcHierarchyInfo[ProcHierarchyIndex].Token         = REFERENCE_TOKEN (ProcHierarchyInfo[ProcHierarchyIndex]);
    ProcHierarchyInfo[ProcHierarchyIndex].Flags         = PROC_NODE_FLAGS (
                                                            EFI_ACPI_6_3_PPTT_PACKAGE_NOT_PHYSICAL,
                                                            EFI_ACPI_6_3_PPTT_PROCESSOR_ID_VALID,
                                                            EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
                                                            EFI_ACPI_6_3_PPTT_NODE_IS_LEAF,
                                                            EFI_ACPI_6_3_PPTT_IMPLEMENTATION_NOT_IDENTICAL
                                                          );
    ProcHierarchyInfo[ProcHierarchyIndex].ParentToken   = ClusterTokenMap [GET_CLUSTER_ID (MpIdr)];
    ProcHierarchyInfo[ProcHierarchyIndex].GicCToken     = REFERENCE_TOKEN (GicCInfo[Index]);
    ProcHierarchyInfo[ProcHierarchyIndex].NoOfPrivateResources = ARRAY_SIZE (CarmelCoreResources);
    ProcHierarchyInfo[ProcHierarchyIndex].PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreResources);
    ProcHierarchyIndex++;

    GicCInfo[Index].CPUInterfaceNumber = Index;
    GicCInfo[Index].AcpiProcessorUid = Index;
    GicCInfo[Index].Flags = EFI_ACPI_6_3_GIC_ENABLED;
    GicCInfo[Index].ParkingProtocolVersion = 0;
    GicCInfo[Index].PerformanceInterruptGsiv = T194_PMU_BASE_INTERRUPT + Index;
    GicCInfo[Index].ParkedAddress = 0;
    GicCInfo[Index].PhysicalBaseAddress = PcdGet64(PcdGicInterruptInterfaceBase);
    GicCInfo[Index].GICV = 0;
    GicCInfo[Index].GICH = 0;
    GicCInfo[Index].VGICMaintenanceInterrupt = T194_VIRT_MAINT_INT;
    GicCInfo[Index].GICRBaseAddress = 0;
    //Only bits 23:0 are valid in the ACPI table
    GicCInfo[Index].MPIDR = MpIdr & 0xFFFFFF;
    GicCInfo[Index].ProcessorPowerEfficiencyClass = 0;
    GicCInfo[Index].SpeOverflowInterrupt = 0;
    GicCInfo[Index].ProximityDomain = 0;
    GicCInfo[Index].ClockDomain = 0;
    GicCInfo[Index].AffinityFlags = EFI_ACPI_6_3_GICC_ENABLED;
  }

  FreePool (ClusterTokenMap);

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicCInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = NumCpus * sizeof (CM_ARM_GICC_INFO);
  Repo->CmObjectCount = NumCpus;
  Repo->CmObjectPtr = GicCInfo;
  Repo++;

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjCacheInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (CacheInfo);
  Repo->CmObjectCount = ARRAY_SIZE (CacheInfo);
  Repo->CmObjectPtr = &CacheInfo;
  Repo++;

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (CcplexResources);
  Repo->CmObjectSize = sizeof (CcplexResources);
  Repo->CmObjectCount = ARRAY_SIZE (CcplexResources);
  Repo->CmObjectPtr = &CcplexResources;
  Repo++;

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (CarmelCoreClusterResources);
  Repo->CmObjectSize = sizeof (CarmelCoreClusterResources);
  Repo->CmObjectCount = ARRAY_SIZE (CarmelCoreClusterResources);
  Repo->CmObjectPtr = &CarmelCoreClusterResources;
  Repo++;

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (CarmelCoreResources);
  Repo->CmObjectSize = sizeof (CarmelCoreResources);
  Repo->CmObjectCount = ARRAY_SIZE (CarmelCoreResources);
  Repo->CmObjectPtr = &CarmelCoreResources;
  Repo++;

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjProcHierarchyInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (CM_ARM_PROC_HIERARCHY_INFO) * (ProcHierarchyIndex);
  Repo->CmObjectCount = ProcHierarchyIndex;
  Repo->CmObjectPtr = ProcHierarchyInfo;
  Repo++;

  *PlatformRepositoryInfo = Repo;
  return EFI_SUCCESS;
}

/** Initialize the platform configuration repository.
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
InitializePlatformRepository ()
{
  EFI_STATUS Status;
  UINTN Index;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  EDKII_PLATFORM_REPOSITORY_INFO  *RepoEnd;

  Repo = &NVIDIAPlatformRepositoryInfo[0];
  RepoEnd = &NVIDIAPlatformRepositoryInfo[EStdObjMax + EArmObjMax];

  Repo->CmObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjCfgMgrInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (CmInfo);
  Repo->CmObjectCount = sizeof (CmInfo) / sizeof (CM_STD_OBJ_CONFIGURATION_MANAGER_INFO);
  Repo->CmObjectPtr = &CmInfo;
  Repo++;

  Repo->CmObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (CmAcpiTableList);
  Repo->CmObjectCount = sizeof (CmAcpiTableList) / sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
  Repo->CmObjectPtr = &CmAcpiTableList;
  for(Index=0; Index<Repo->CmObjectCount; Index++) {
    if (CmAcpiTableList[Index].AcpiTableSignature != EFI_ACPI_6_3_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE) {
      CmAcpiTableList[Index].OemTableId =  PcdGet64(PcdAcpiDefaultOemTableId);
    }
  }
  Repo++;

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjBootArchInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (BootArchInfo);
  Repo->CmObjectCount = sizeof (BootArchInfo) / sizeof (CM_ARM_BOOT_ARCH_INFO);
  Repo->CmObjectPtr = &BootArchInfo;
  Repo++;

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjFixedFeatureFlags);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (FixedFeatureFlags);
  Repo->CmObjectCount = sizeof (FixedFeatureFlags) / sizeof (CM_ARM_FIXED_FEATURE_FLAGS);
  Repo->CmObjectPtr = &FixedFeatureFlags;
  Repo++;

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjPowerManagementProfileInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (PmProfileInfo);
  Repo->CmObjectCount = sizeof (PmProfileInfo) / sizeof (CM_ARM_POWER_MANAGEMENT_PROFILE_INFO);
  Repo->CmObjectPtr = &PmProfileInfo;
  Repo++;

  GicDInfo.PhysicalBaseAddress = PcdGet64 (PcdGicDistributorBase);
  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicDInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (GicDInfo);
  Repo->CmObjectCount = sizeof (GicDInfo) / sizeof (CM_ARM_GICD_INFO);
  Repo->CmObjectPtr = &GicDInfo;
  Repo++;

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGenericTimerInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (GenericTimerInfo);
  Repo->CmObjectCount = sizeof (GenericTimerInfo) / sizeof (CM_ARM_GENERIC_TIMER_INFO);
  Repo->CmObjectPtr = &GenericTimerInfo;
  Repo++;

  Status = UpdateCpuInfo (&Repo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateSerialPortInfo (&Repo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (IsPcieEnabled ()) {
    Status = UpdatePcieInfo (&Repo);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  } else {
    Status = UpdateAhciInfo (&Repo);
    if (EFI_ERROR (Status)) {
      return Status;
    }
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

  //Don't generate error if fan patching fails
  UpdateFanInfo (&Repo);

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
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE  * SystemTable
  )
{
  UINTN      ChipID;
  EFI_STATUS Status;

  ChipID = TegraGetChipID();
  if (ChipID != T194_CHIP_ID) {
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

  return gBS->InstallMultipleProtocolInterfaces (&ImageHandle,
                                                 &gNVIDIAConfigurationManagerDataProtocolGuid,
                                                 (VOID*)NVIDIAPlatformRepositoryInfo,
                                                 NULL);
}
