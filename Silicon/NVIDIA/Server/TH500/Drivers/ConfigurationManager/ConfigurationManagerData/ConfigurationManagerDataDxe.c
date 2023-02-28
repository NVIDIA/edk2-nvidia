/** @file
  Configuration Manager Data Dxe

  Copyright (c) 2019 - 2023, NVIDIA Corporation. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/

#include <ConfigurationManagerDataDxePrivate.h>

// Platform CPU configuration
#define PLATFORM_MAX_CORES_PER_CLUSTER  (PcdGet32 (PcdTegraMaxCoresPerCluster))
#define PLATFORM_MAX_CLUSTERS           (PcdGet32 (PcdTegraMaxClusters))
#define PLATFORM_MAX_CPUS               (PLATFORM_MAX_CLUSTERS * \
                                         PLATFORM_MAX_CORES_PER_CLUSTER)

// ACPI Timer enable
#define ACPI_TIMER_INSTRUCTION_ENABLE  (PcdGet8 (PcdAcpiTimerEnabled))

/** The platform configuration repository information.
*/
STATIC
EDKII_PLATFORM_REPOSITORY_INFO  *NVIDIAPlatformRepositoryInfo;

// AML Patch protocol
NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol = NULL;

STATIC EFI_ACPI_DESCRIPTION_HEADER  *AcpiTableArray[] = {
  (EFI_ACPI_DESCRIPTION_HEADER *)dsdt_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)ssdtsocket1_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)ssdtsocket2_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)ssdtsocket3_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket0_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket1_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket2_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket3_aml_code
};

STATIC AML_OFFSET_TABLE_ENTRY  *OffsetTableArray[] = {
  DSDT_TH500_OffsetTable,
  SSDT_TH500_S1_OffsetTable,
  SSDT_TH500_S2_OffsetTable,
  SSDT_TH500_S3_OffsetTable,
  SSDT_BPMP_S0_OffsetTable,
  SSDT_BPMP_S1_OffsetTable,
  SSDT_BPMP_S2_OffsetTable,
  SSDT_BPMP_S3_OffsetTable
};

STATIC EFI_ACPI_DESCRIPTION_HEADER  *AcpiBpmpTableArray[] = {
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket0_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket1_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket2_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket3_aml_code
};

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
  // PPTT Table
  {
    EFI_ACPI_6_4_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_STRUCTURE_SIGNATURE,
    EFI_ACPI_6_4_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdPptt),
    NULL,
    0,
    FixedPcdGet64 (PcdAcpiDefaultOemRevision),
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
  GTDT_GTIMER_FLAGS,
  ARMARCH_TMR_HYPVIRT_PPI,
  GTDT_GTIMER_FLAGS
};

STATIC CM_ARM_GENERIC_WATCHDOG_INFO  Watchdog;

// SPCR Serial Port

/** The platform SPCR serial port information.
*/
STATIC
CM_ARM_SERIAL_PORT_INFO  SpcrSerialPort = {
  FixedPcdGet64 (PcdSbsaUartBaseTH500),                     // BaseAddress
  TH500_UART0_INTR,                                         // Interrupt
  FixedPcdGet64 (PcdUartDefaultBaudRate),                   // BaudRate
  FixedPcdGet32 (PL011UartClkInHz),                         // Clock
  EFI_ACPI_DBG2_PORT_SUBTYPE_SERIAL_ARM_SBSA_GENERIC_UART   // Port subtype
};

/** MRQ_PWR_LIMIT get sub-command (CMD_PWR_LIMIT_GET) packet
*/
#pragma pack (1)
typedef struct {
  UINT32    Command;
  UINT32    LimitId;
  UINT32    LimitSrc;
  UINT32    LimitType;
} MRQ_PWR_LIMIT_COMMAND_PACKET;
#pragma pack ()

/** Initialize the Serial Port entries in the platform configuration repository and patch DSDT.
 *
 * @param Repo Pointer to a repo structure that will be added to and updated with the data updated
 *
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
UpdateSerialPortInfo (
  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  UINT32                          Index;
  UINT8                           SerialPortConfig;
  CM_STD_OBJ_ACPI_TABLE_INFO      *NewAcpiTables;

  SerialPortConfig = PcdGet8 (PcdSerialPortConfig);
  if ((PcdGet8 (PcdSerialTypeConfig) != NVIDIA_SERIAL_PORT_TYPE_SBSA) ||
      (SerialPortConfig == NVIDIA_SERIAL_PORT_DISABLED))
  {
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < PcdGet32 (PcdConfigMgrObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (NVIDIAPlatformRepositoryInfo[Index].CmObjectSize + (sizeof (CM_STD_OBJ_ACPI_TABLE_INFO)), NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr);
      if (NewAcpiTables == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        return Status;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      if (SerialPortConfig == NVIDIA_SERIAL_PORT_DBG2_SBSA) {
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_4_DEBUG_PORT_2_TABLE_SIGNATURE;
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = EFI_ACPI_DEBUG_PORT_2_TABLE_REVISION;
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDbg2);
      } else {
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_4_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE;
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_REVISION;
        NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSpcr);
      }

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData = NULL;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId    = PcdGet64 (PcdAcpiDefaultOemTableId);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision   = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }

  Repo = *PlatformRepositoryInfo;

  if (SerialPortConfig == NVIDIA_SERIAL_PORT_DBG2_SBSA) {
    Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjSerialDebugPortInfo);
  } else {
    Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjSerialConsolePortInfo);
  }

  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CM_ARM_SERIAL_PORT_INFO);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = &SpcrSerialPort;
  Repo++;

  *PlatformRepositoryInfo = Repo;
  return EFI_SUCCESS;
}

/** Initialize the additional sockets info in the platform configuration repository and patch SSDT.
 *
 * @param Repo Pointer to a repo structure that will be added to and updated with the data updated
 *
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
UpdateAdditionalSocketInfo (
  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  UINTN                           SocketId
  )
{
  EFI_STATUS                  Status;
  UINT32                      Index;
  CM_STD_OBJ_ACPI_TABLE_INFO  *NewAcpiTables;

  for (Index = 0; Index < PcdGet32 (PcdConfigMgrObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (NVIDIAPlatformRepositoryInfo[Index].CmObjectSize + (sizeof (CM_STD_OBJ_ACPI_TABLE_INFO)), NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr);

      if (NewAcpiTables == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        return Status;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)AcpiTableArray[SocketId];
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }

  return EFI_SUCCESS;
}

/** Initialize the socket info for tables needing bpmp in the platform configuration repository and patch SSDT.
 *
 * @param Repo Pointer to a repo structure that will be added to and updated with the data updated
 *
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
AddBpmpSocketInfo (
  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo,
  UINTN                           SocketId
  )
{
  EFI_STATUS                  Status;
  UINT32                      Index;
  CM_STD_OBJ_ACPI_TABLE_INFO  *NewAcpiTables;

  for (Index = 0; Index < PcdGet32 (PcdConfigMgrObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (NVIDIAPlatformRepositoryInfo[Index].CmObjectSize + (sizeof (CM_STD_OBJ_ACPI_TABLE_INFO)), NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr);

      if (NewAcpiTables == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        return Status;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)AcpiBpmpTableArray[SocketId];
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }

  return EFI_SUCCESS;
}

/** Initialize the ethernet controller entry in the platform configuration repository and patch SSDT.
 *
 * @param Repo Pointer to a repo structure that will be added to and updated with the data updated
 *
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
UpdateEthernetInfo (
  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo
  )
{
  EFI_STATUS                  Status;
  UINT32                      Index;
  CM_STD_OBJ_ACPI_TABLE_INFO  *NewAcpiTables;
  TEGRA_PLATFORM_TYPE         PlatformType;

  PlatformType = TegraGetPlatform ();
  if (PlatformType != TEGRA_PLATFORM_VDK) {
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < PcdGet32 (PcdConfigMgrObjMax); Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList)) {
      NewAcpiTables = (CM_STD_OBJ_ACPI_TABLE_INFO *)AllocateCopyPool (NVIDIAPlatformRepositoryInfo[Index].CmObjectSize + (sizeof (CM_STD_OBJ_ACPI_TABLE_INFO)), NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr);

      if (NewAcpiTables == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        return Status;
      }

      NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr = NewAcpiTables;

      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableSignature = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableRevision  = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData      = (EFI_ACPI_DESCRIPTION_HEADER *)ssdteth_aml_code;
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
      NVIDIAPlatformRepositoryInfo[Index].CmObjectCount++;
      NVIDIAPlatformRepositoryInfo[Index].CmObjectSize += sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);

      break;
    } else if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }
  }

  return EFI_SUCCESS;
}

/** patch GED data in DSDT.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateGedInfo (
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
      EFI_D_ERROR,
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
    return EFI_SUCCESS;
  }

  Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &DpcCommBuf->PcieBase, 8);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, ACPI_GED1_SMR1, Status));
  }

  return Status;
}

/** patch QSPI1 data in DSDT.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateQspiInfo (
  VOID
  )
{
  EFI_STATUS            Status;
  UINT32                NumberOfQspiControllers;
  UINT32                *QspiHandles;
  UINT32                Index;
  VOID                  *Dtb;
  INT32                 NodeOffset;
  NVIDIA_AML_NODE_INFO  AcpiNodeInfo;
  UINT8                 QspiStatus;

  NumberOfQspiControllers = 0;
  Status                  = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra186-qspi", NULL, &NumberOfQspiControllers);
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  QspiHandles = NULL;
  QspiHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfQspiControllers);
  if (QspiHandles == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra186-qspi", QspiHandles, &NumberOfQspiControllers);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  for (Index = 0; Index < NumberOfQspiControllers; Index++) {
    Status = GetDeviceTreeNode (QspiHandles[Index], &Dtb, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get device node info - %r\r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }

    if (NULL == fdt_getprop (Dtb, NodeOffset, "nvidia,secure-qspi-controller", NULL)) {
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
  }

ErrorExit:
  if (QspiHandles != NULL) {
    FreePool (QspiHandles);
  }

  return Status;
}

/** patch I2C3 and SSIF data in DSDT.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateSSIFInfo (
  VOID
  )
{
  EFI_STATUS            Status;
  UINT32                NumberOfI2CControllers;
  UINT32                *I2CHandles;
  UINT32                Index;
  VOID                  *Dtb;
  INT32                 NodeOffset;
  INT32                 SubNodeOffset;
  NVIDIA_AML_NODE_INFO  AcpiNodeInfo;
  UINT8                 I2CStatus;
  UINT8                 SSIFStatus;

  NumberOfI2CControllers = 0;
  Status                 = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra234-i2c", NULL, &NumberOfI2CControllers);
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  I2CHandles = NULL;
  I2CHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfI2CControllers);
  if (I2CHandles == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra234-i2c", I2CHandles, &NumberOfI2CControllers);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  for (Index = 0; Index < NumberOfI2CControllers; Index++) {
    Status = GetDeviceTreeNode (I2CHandles[Index], &Dtb, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get device node info - %r\r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }

    SubNodeOffset = fdt_subnode_offset (Dtb, NodeOffset, "bmc-ssif");
    if (SubNodeOffset != 0) {
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
  }

ErrorExit:
  if (I2CHandles != NULL) {
    FreePool (I2CHandles);
  }

  return Status;
}

/** patch TPM1 data in DSDT.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateTpmInfo (
  VOID
  )
{
  EFI_STATUS            Status;
  UINT32                NumberOfTpmControllers;
  VOID                  *Dtb;
  INT32                 NodeOffset;
  INT32                 BusNodeOffset;
  CONST VOID            *Property;
  UINT32                *TpmHandles;
  NVIDIA_AML_NODE_INFO  AcpiNodeInfo;
  UINT8                 TpmStatus;

  NumberOfTpmControllers = 0;
  Status                 = GetMatchingEnabledDeviceTreeNodes ("tcg,tpm_tis-spi", NULL, &NumberOfTpmControllers);
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  TpmHandles = NULL;
  TpmHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfTpmControllers);
  if (TpmHandles == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetMatchingEnabledDeviceTreeNodes ("tcg,tpm_tis-spi", TpmHandles, &NumberOfTpmControllers);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  // Only support one TPM per system
  ASSERT (NumberOfTpmControllers == 1);

  Status = GetDeviceTreeNode (TpmHandles[0], &Dtb, &NodeOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get TPM DT node - %r\r\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  //
  // Check if the bus that TPM is on is enabled
  //
  BusNodeOffset = fdt_parent_offset (Dtb, NodeOffset);
  if (BusNodeOffset != 0) {
    Property = fdt_getprop (Dtb, BusNodeOffset, "status", NULL);
    if ((Property != NULL) && (AsciiStrCmp (Property, "okay") != 0)) {
      DEBUG ((DEBUG_INFO, "%a: TPM is present but the bus is disabled\r\n", __FUNCTION__));
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }
  }

  //
  // Patch to enable TPM1 device
  //
  Status = PatchProtocol->FindNode (PatchProtocol, ACPI_TPM1_STA, &AcpiNodeInfo);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  if (AcpiNodeInfo.Size > sizeof (TpmStatus)) {
    Status = EFI_DEVICE_ERROR;
    goto ErrorExit;
  }

  TpmStatus = 0xF;
  Status    = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &TpmStatus, sizeof (TpmStatus));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, ACPI_TPM1_STA, Status));
  }

ErrorExit:
  if (TpmHandles != NULL) {
    FreePool (TpmHandles);
  }

  return Status;
}

/** patch OEM Table IDs in pre compile AML code

**/
STATIC
VOID
EFIAPI
PatchOemTableId (
  VOID
  )
{
  UINT32  AcpiTableArrayIndex;

  for (AcpiTableArrayIndex = 0; AcpiTableArrayIndex < ARRAY_SIZE (AcpiTableArray); AcpiTableArrayIndex++) {
    AcpiTableArray[AcpiTableArrayIndex]->OemTableId = PcdGet64 (PcdAcpiDefaultOemTableId);
  }
}

/** patch thermal zone temperature ranges data in SSDT.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateThermalZoneTempInfo (
  VOID
  )
{
  EFI_STATUS            Status;
  VOID                  *DtbBase;
  UINTN                 DtbSize;
  INTN                  NodeOffset;
  CONST UINT32          *Temp;
  INT32                 TempLen;
  UINT32                SocketId;
  CHAR8                 Buffer[16];
  UINTN                 CharCount;
  NVIDIA_AML_NODE_INFO  AcpiNodeInfo;
  UINT16                PsvTemp;
  UINT16                CrtTemp;

  Status = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NodeOffset = fdt_path_offset (DtbBase, "/firmware/acpi");
  if (NodeOffset < 0) {
    return EFI_SUCCESS;
  }

  PsvTemp = MAX_UINT16;
  CrtTemp = MAX_UINT16;

  Temp = NULL;
  Temp = (CONST UINT32 *)fdt_getprop (DtbBase, NodeOffset, "override-thermal-zone-passive-cooling-trip-point-temp", &TempLen);
  if ((Temp != NULL) && (TempLen == sizeof (UINT32))) {
    PsvTemp = (SwapBytes32 (*Temp) * 10) + 2732;
  }

  Temp = NULL;
  Temp = (CONST UINT32 *)fdt_getprop (DtbBase, NodeOffset, "override-thermal-zone-critical-point-temp", &TempLen);
  if ((Temp != NULL) && (TempLen == sizeof (UINT32))) {
    CrtTemp = (SwapBytes32 (*Temp) * 10) + 2732;
  }

  for (SocketId = 0; SocketId < PcdGet32 (PcdTegraMaxSockets); SocketId++) {
    if (!IsSocketEnabled (SocketId)) {
      continue;
    }

    if (PsvTemp != MAX_UINT16) {
      CharCount = AsciiSPrint (Buffer, sizeof (Buffer), "_SB_.BPM%01x.PSVT", SocketId);
      Status    = PatchProtocol->FindNode (PatchProtocol, Buffer, &AcpiNodeInfo);
      if (!EFI_ERROR (Status)) {
        if (AcpiNodeInfo.Size > sizeof (PsvTemp)) {
          continue;
        }

        Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &PsvTemp, sizeof (PsvTemp));
        if (EFI_ERROR (Status)) {
          return Status;
        }
      }
    }

    if (CrtTemp != MAX_UINT16) {
      CharCount = AsciiSPrint (Buffer, sizeof (Buffer), "_SB_.BPM%01x.CRTT", SocketId);
      Status    = PatchProtocol->FindNode (PatchProtocol, Buffer, &AcpiNodeInfo);
      if (!EFI_ERROR (Status)) {
        if (AcpiNodeInfo.Size > sizeof (CrtTemp)) {
          continue;
        }

        Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &CrtTemp, sizeof (CrtTemp));
        if (EFI_ERROR (Status)) {
          return Status;
        }
      }
    }
  }

  return EFI_SUCCESS;
}

/** patch MRQ_PWR_LIMIT data in DSDT.

  @param[in]     BpmpIpcProtocol     The instance of the NVIDIA_BPMP_IPC_PROTOCOL
  @param[in]     BpmpHandle          Bpmp handle
  @param[in]     SocketId            Socket Id

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdatePowerLimitInfo (
  IN  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol,
  IN  UINT32                    BpmpHandle,
  IN  UINT32                    SocketId
  )
{
  EFI_STATUS                    Status;
  NVIDIA_AML_NODE_INFO          AcpiNodeInfo;
  MRQ_PWR_LIMIT_COMMAND_PACKET  Request;
  UINT32                        PwrLimit;

  STATIC CHAR8 *CONST  AcpiMrqPwrLimitMinPatchName[] = {
    "_SB_.PM01.MINP",
    "_SB_.PM11.MINP",
    "_SB_.PM21.MINP",
    "_SB_.PM31.MINP",
  };

  STATIC CHAR8 *CONST  AcpiMrqPwrLimitMaxPatchName[] = {
    "_SB_.PM01.MAXP",
    "_SB_.PM11.MAXP",
    "_SB_.PM21.MAXP",
    "_SB_.PM31.MAXP",
  };

  // Get power meter limits
  Request.Command   = TH500_PWR_LIMIT_GET;
  Request.LimitId   = TH500_PWR_LIMIT_ID_TH500_INP_EDPC_MW;
  Request.LimitSrc  = TH500_PWR_LIMIT_SRC_INB;
  Request.LimitType = TH500_PWR_LIMIT_TYPE_BOUND_MAX;

  Status = BpmpIpcProtocol->Communicate (
                              BpmpIpcProtocol,
                              NULL,
                              BpmpHandle,
                              MRQ_PWR_LIMIT,
                              (VOID *)&Request,
                              sizeof (MRQ_PWR_LIMIT_COMMAND_PACKET),
                              (VOID *)&PwrLimit,
                              sizeof (UINT32),
                              NULL
                              );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error in BPMP communication for max pwr limit: %r\r\n", __FUNCTION__, Status));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

  if (PwrLimit == 0) {
    PwrLimit = 0xFFFFFFFF;
  }

  if (SocketId >= ARRAY_SIZE (AcpiMrqPwrLimitMaxPatchName)) {
    DEBUG ((DEBUG_ERROR, "%a: SocketId %d exceeding AcpiMrqPwrLimitMaxPatchName size\r\n", __FUNCTION__, SocketId));
    goto ErrorExit;
  }

  Status = PatchProtocol->FindNode (PatchProtocol, AcpiMrqPwrLimitMaxPatchName[SocketId], &AcpiNodeInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Max power limit node is not found for patching %a - %r\r\n",
      __FUNCTION__,
      AcpiMrqPwrLimitMaxPatchName[SocketId],
      Status
      ));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

  Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &PwrLimit, sizeof (PwrLimit));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, AcpiMrqPwrLimitMaxPatchName[SocketId], Status));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

  Request.LimitType = TH500_PWR_LIMIT_TYPE_BOUND_MIN;

  Status = BpmpIpcProtocol->Communicate (
                              BpmpIpcProtocol,
                              NULL,
                              BpmpHandle,
                              MRQ_PWR_LIMIT,
                              (VOID *)&Request,
                              sizeof (MRQ_PWR_LIMIT_COMMAND_PACKET),
                              (VOID *)&PwrLimit,
                              sizeof (UINT32),
                              NULL
                              );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error in BPMP communication for min pwr limit: %r\r\n", __FUNCTION__, Status));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

  if (SocketId >= ARRAY_SIZE (AcpiMrqPwrLimitMinPatchName)) {
    DEBUG ((DEBUG_ERROR, "%a: SocketId %d exceeding AcpiMrqPwrLimitMinPatchName size\r\n", __FUNCTION__, SocketId));
    goto ErrorExit;
  }

  Status = PatchProtocol->FindNode (PatchProtocol, AcpiMrqPwrLimitMinPatchName[SocketId], &AcpiNodeInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Min power limit node is not found for patching %a - %r\r\n",
      __FUNCTION__,
      AcpiMrqPwrLimitMinPatchName[SocketId],
      Status
      ));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

  Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &PwrLimit, sizeof (PwrLimit));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, AcpiMrqPwrLimitMinPatchName[SocketId], Status));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

ErrorExit:
  return Status;
}

/** patch ACPI Timer operator enable/disable status from Nvidia boot configuration in DSDT.

  @param[in]     SocketId            Socket Id

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateAcpiTimerOprInfo (
  IN  UINT32  SocketId
  )
{
  EFI_STATUS            Status;
  NVIDIA_AML_NODE_INFO  AcpiNodeInfo;
  UINT8                 AcpiTimerEnableFlag;

  STATIC CHAR8 *CONST  AcpiTimerInstructionEnableVarName[] = {
    "_SB_.BPM0.TIME",
    "_SB_.BPM1.TIME",
    "_SB_.BPM2.TIME",
    "_SB_.BPM3.TIME",
  };

  AcpiTimerEnableFlag = ACPI_TIMER_INSTRUCTION_ENABLE;

  if (SocketId >= ARRAY_SIZE (AcpiTimerInstructionEnableVarName)) {
    DEBUG ((DEBUG_ERROR, "%a: Index %d exceeding AcpiTimerInstructionEnableVarName size\r\n", __FUNCTION__, SocketId));
    Status = EFI_INVALID_PARAMETER;
    goto ErrorExit;
  }

  Status = PatchProtocol->FindNode (PatchProtocol, AcpiTimerInstructionEnableVarName[SocketId], &AcpiNodeInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Acpi timer enable node is not found for patching %a - %r\r\n", __FUNCTION__, AcpiTimerInstructionEnableVarName[SocketId], Status));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

  Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &AcpiTimerEnableFlag, sizeof (AcpiTimerEnableFlag));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, AcpiTimerInstructionEnableVarName[SocketId], Status));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

ErrorExit:
  return Status;
}

/** patch MRQ_TELEMETRY data in DSDT.

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateTelemetryInfo (
  VOID
  )
{
  EFI_STATUS                Status;
  NVIDIA_AML_NODE_INFO      AcpiNodeInfo;
  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpcProtocol;
  UINT64                    TelemetryDataBuffAddr;
  UINTN                     ResponseSize;
  UINT32                    BpmpHandle;
  VOID                      *Dtb;
  INT32                     NodeOffset;
  UINT32                    NumberOfMrqTelemeteryNodes;
  UINT32                    *MrqTelemetryHandles;
  UINT32                    Index;
  CONST VOID                *Property;
  INT32                     PropertySize;
  UINT32                    SocketId;

  STATIC CHAR8 *CONST  AcpiMrqTelemetryBufferPatchName[] = {
    "_SB_.BPM0.TBUF",
    "_SB_.BPM1.TBUF",
    "_SB_.BPM2.TBUF",
    "_SB_.BPM3.TBUF",
  };

  BpmpIpcProtocol            = NULL;
  TelemetryDataBuffAddr      = 0;
  ResponseSize               = sizeof (TelemetryDataBuffAddr);
  Dtb                        = NULL;
  MrqTelemetryHandles        = NULL;
  Property                   = NULL;
  PropertySize               = 0;
  SocketId                   = 0;
  NumberOfMrqTelemeteryNodes = 0;

  Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,th500-mrqtelemetry", NULL, &NumberOfMrqTelemeteryNodes);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "%a: nvidia,th500-mrqtelemetry nodes absent in device tree\r\n", __FUNCTION__));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    goto ErrorExit;
  }

  MrqTelemetryHandles = NULL;
  MrqTelemetryHandles = (UINT32 *)AllocatePool (sizeof (UINT32) * NumberOfMrqTelemeteryNodes);
  if (MrqTelemetryHandles == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,th500-mrqtelemetry", MrqTelemetryHandles, &NumberOfMrqTelemeteryNodes);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
  if (EFI_ERROR (Status)) {
    Status = EFI_NOT_READY;
    goto ErrorExit;
  }

  if (BpmpIpcProtocol == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto ErrorExit;
  }

  for (Index = 0; Index < NumberOfMrqTelemeteryNodes; Index++) {
    BpmpHandle = 0;
    Status     = GetDeviceTreeNode (MrqTelemetryHandles[Index], &Dtb, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get device node info - %r\r\n", __FUNCTION__, Status));
      goto ErrorExit;
    }

    Property = fdt_getprop (Dtb, NodeOffset, "nvidia,bpmp", &PropertySize);
    if ((Property == NULL) || (PropertySize < sizeof (UINT32))) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get Bpmp node phandle for index - %d\n", __FUNCTION__, Index));
      goto ErrorExit;
    } else {
      CopyMem ((VOID *)&BpmpHandle, Property, sizeof (UINT32));
      BpmpHandle = SwapBytes32 (BpmpHandle);
    }

    SocketId = 0;
    Property = fdt_getprop (Dtb, NodeOffset, "nvidia,hw-instance-id", &PropertySize);
    if ((Property == NULL) || (PropertySize < sizeof (UINT32))) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get Socket Id for index - %d\n", __FUNCTION__, Index));
      goto ErrorExit;
    } else {
      CopyMem ((VOID *)&SocketId, Property, sizeof (UINT32));
      SocketId = SwapBytes32 (SocketId);
    }

    if (SocketId >= PcdGet32 (PcdTegraMaxSockets)) {
      DEBUG ((DEBUG_ERROR, "%a: SocketId %d exceeds number of sockets\r\n", __FUNCTION__, SocketId));
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }

    if (!IsSocketEnabled (SocketId)) {
      continue;
    }

    Status = BpmpIpcProtocol->Communicate (
                                BpmpIpcProtocol,
                                NULL,
                                BpmpHandle,
                                MRQ_TELEMETRY,
                                NULL,
                                0,
                                (VOID *)&TelemetryDataBuffAddr,
                                ResponseSize,
                                NULL
                                );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error in BPMP communication: %r\r\n", __FUNCTION__, Status));
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }

    TelemetryDataBuffAddr = TH500_AMAP_GET_ADD (TelemetryDataBuffAddr, SocketId);

    if (Index >= ARRAY_SIZE (AcpiMrqTelemetryBufferPatchName)) {
      DEBUG ((DEBUG_ERROR, "%a: Index %d exceeding AcpiMrqTelemetryBufferPatchName size\r\n", __FUNCTION__, Index));
      goto ErrorExit;
    }

    Status = PatchProtocol->FindNode (PatchProtocol, AcpiMrqTelemetryBufferPatchName[Index], &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: MRQ_TELEMETRY node is not found for patching %a - %r\r\n", __FUNCTION__, AcpiMrqTelemetryBufferPatchName[Index], Status));
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &TelemetryDataBuffAddr, sizeof (TelemetryDataBuffAddr));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, AcpiMrqTelemetryBufferPatchName[Index], Status));
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }

    Status = UpdateAcpiTimerOprInfo (SocketId);
    if (EFI_ERROR (Status)) {
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }

    Status = UpdatePowerLimitInfo (BpmpIpcProtocol, BpmpHandle, SocketId);
    if (EFI_ERROR (Status)) {
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }
  }

ErrorExit:
  if (MrqTelemetryHandles != NULL) {
    FreePool (MrqTelemetryHandles);
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
  UINTN                           Index;
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  EDKII_PLATFORM_REPOSITORY_INFO  *RepoEnd;
  VOID                            *DtbBase;
  UINTN                           DtbSize;
  INTN                            NodeOffset;
  BOOLEAN                         SkipSlit;
  BOOLEAN                         SkipSrat;
  BOOLEAN                         SkipHmat;
  BOOLEAN                         SkipIort;
  BOOLEAN                         SkipMpam;
  BOOLEAN                         SkipApmt;
  BOOLEAN                         SkipSpmi;
  BOOLEAN                         SkipTpm2;
  UINTN                           SocketId;
  TEGRA_PLATFORM_TYPE             PlatformType;

  NVIDIAPlatformRepositoryInfo = (EDKII_PLATFORM_REPOSITORY_INFO *)AllocateZeroPool (sizeof (EDKII_PLATFORM_REPOSITORY_INFO) * PcdGet32 (PcdConfigMgrObjMax));

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
    if ((CmAcpiTableList[Index].AcpiTableSignature != EFI_ACPI_6_4_DEBUG_PORT_2_TABLE_SIGNATURE) &&
        (CmAcpiTableList[Index].AcpiTableSignature != EFI_ACPI_6_4_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE))
    {
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

  Status = UpdateGicInfo (&Repo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjGenericTimerInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (GenericTimerInfo);
  Repo->CmObjectCount = sizeof (GenericTimerInfo) / sizeof (CM_ARM_GENERIC_TIMER_INFO);
  Repo->CmObjectPtr   = &GenericTimerInfo;
  Repo++;

  if (TegraGetPlatform () != TEGRA_PLATFORM_VDK) {
    Watchdog.ControlFrameAddress = PcdGet64 (PcdGenericWatchdogControlBase);
    Watchdog.RefreshFrameAddress = PcdGet64 (PcdGenericWatchdogRefreshBase);
    Watchdog.TimerGSIV           = PcdGet32 (PcdGenericWatchdogEl2IntrNum);
    Watchdog.Flags               = SBSA_WATCHDOG_FLAGS;
    Repo->CmObjectId             = CREATE_CM_ARM_OBJECT_ID (EArmObjPlatformGenericWatchdogInfo);
    Repo->CmObjectToken          = CM_NULL_TOKEN;
    Repo->CmObjectSize           = sizeof (Watchdog);
    Repo->CmObjectCount          = sizeof (Watchdog) / sizeof (CM_ARM_GENERIC_WATCHDOG_INFO);
    Repo->CmObjectPtr            = &Watchdog;
    Repo++;
  }

  SkipSlit = FALSE;
  SkipSrat = FALSE;
  SkipHmat = FALSE;
  SkipIort = FALSE;
  SkipMpam = FALSE;
  SkipApmt = FALSE;
  SkipSpmi = FALSE;
  SkipTpm2 = FALSE;
  Status   = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NodeOffset = fdt_path_offset (DtbBase, "/firmware/uefi");
  if (NodeOffset >= 0) {
    if (NULL != fdt_get_property (DtbBase, NodeOffset, "skip-slit-table", NULL)) {
      SkipSlit = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip SLIT Table\r\n", __FUNCTION__));
    }

    if (NULL != fdt_get_property (DtbBase, NodeOffset, "skip-srat-table", NULL)) {
      SkipSrat = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip SRAT Table\r\n", __FUNCTION__));
    }

    if (NULL != fdt_get_property (DtbBase, NodeOffset, "skip-hmat-table", NULL)) {
      SkipHmat = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip HMAT Table\r\n", __FUNCTION__));
    }

    if (NULL != fdt_get_property (DtbBase, NodeOffset, "skip-iort-table", NULL)) {
      SkipIort = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip IORT Table\r\n", __FUNCTION__));
    }

    if (NULL != fdt_get_property (DtbBase, NodeOffset, "skip-mpam-table", NULL)) {
      SkipMpam = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip MPAM Table\r\n", __FUNCTION__));
    }

    if (NULL != fdt_get_property (DtbBase, NodeOffset, "skip-apmt-table", NULL)) {
      SkipApmt = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip APMT Table\r\n", __FUNCTION__));
    }

    if (NULL != fdt_get_property (DtbBase, NodeOffset, "skip-spmi-table", NULL)) {
      SkipSpmi = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip SPMI Table\r\n", __FUNCTION__));
    }

    if (NULL != fdt_get_property (DtbBase, NodeOffset, "skip-tpm2-table", NULL)) {
      SkipTpm2 = TRUE;
      DEBUG ((DEBUG_ERROR, "%a: Skip TPM2 Table\r\n", __FUNCTION__));
    }
  }

  Status = UpdateCpuInfo (&Repo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateSerialPortInfo (&Repo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateEthernetInfo (&Repo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateGedInfo ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateTelemetryInfo ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // SSDT for socket1 onwards
  for (SocketId = 1; SocketId < PcdGet32 (PcdTegraMaxSockets); SocketId++) {
    if (!IsSocketEnabled (SocketId)) {
      continue;
    }

    Status = UpdateAdditionalSocketInfo (&Repo, SocketId);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  // BPMP SSDT
  PlatformType = TegraGetPlatform ();
  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    for (SocketId = 0; SocketId < PcdGet32 (PcdTegraMaxSockets); SocketId++) {
      if (!IsSocketEnabled (SocketId)) {
        continue;
      }

      Status = AddBpmpSocketInfo (&Repo, SocketId);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }
  }

  Status = RegisterProtocolBasedObjects (NVIDIAPlatformRepositoryInfo, &Repo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!SkipIort) {
    Status = InstallIoRemappingTable (&Repo, (UINTN)RepoEnd, NVIDIAPlatformRepositoryInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if ((!SkipMpam) && (IsMpamEnabled ())) {
    Status = InstallMpamTable (&Repo, (UINTN)RepoEnd, NVIDIAPlatformRepositoryInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  Status = GenerateHbmMemPxmDmnMap ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!SkipSrat) {
    Status = InstallStaticResourceAffinityTable (&Repo, (UINTN)RepoEnd, NVIDIAPlatformRepositoryInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (!SkipSlit) {
    Status = InstallStaticLocalityInformationTable (NVIDIAPlatformRepositoryInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (!SkipHmat) {
    Status = InstallHeterogeneousMemoryAttributeTable (&Repo, (UINTN)RepoEnd, NVIDIAPlatformRepositoryInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (!SkipApmt) {
    Status = InstallArmPerformanceMonitoringUnitTable (NVIDIAPlatformRepositoryInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (!SkipSpmi) {
    Status = InstallServiceProcessorManagementInterfaceTable (&Repo, (UINTN)RepoEnd, NVIDIAPlatformRepositoryInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (!SkipTpm2) {
    Status = InstallTrustedComputingPlatform2Table (&Repo, (UINTN)RepoEnd, NVIDIAPlatformRepositoryInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  Status = InstallCmSmbiosTableList (&Repo, (UINTN)RepoEnd);
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
  if (ChipID != TH500_CHIP_ID) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gNVIDIAAmlPatchProtocolGuid, NULL, (VOID **)&PatchProtocol);
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

  Status = InitializeIoRemappingNodes ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = InitializePlatformRepository ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateQspiInfo ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateSSIFInfo ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateTpmInfo ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UpdateThermalZoneTempInfo ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PatchOemTableId ();

  return gBS->InstallMultipleProtocolInterfaces (
                &ImageHandle,
                &gNVIDIAConfigurationManagerDataProtocolGuid,
                (VOID *)NVIDIAPlatformRepositoryInfo,
                NULL
                );
}
