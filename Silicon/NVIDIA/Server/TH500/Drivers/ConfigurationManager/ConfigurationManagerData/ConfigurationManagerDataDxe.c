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
#include <Library/HobLib.h>

// Platform CPU configuration
#define PLATFORM_MAX_CORES_PER_CLUSTER  (PcdGet32 (PcdTegraMaxCoresPerCluster))
#define PLATFORM_MAX_CLUSTERS           (PcdGet32 (PcdTegraMaxClusters))
#define PLATFORM_MAX_CPUS               (PLATFORM_MAX_CLUSTERS * \
                                         PLATFORM_MAX_CORES_PER_CLUSTER)
#define PLATFORM_CPUS_PER_SOCKET        (PLATFORM_MAX_CPUS / PLATFORM_MAX_SOCKETS)

// ACPI Timer enable
#define ACPI_TIMER_INSTRUCTION_ENABLE  (PcdGet8 (PcdAcpiTimerEnabled))

STATIC UINTN  ThermalZoneCpu0_List[]  = { 0x00, 0x02, 0x04, 0x0E, MAX_UINTN };
STATIC UINTN  ThermalZoneCpu1_List[]  = { 0x06, 0x08, 0x0A, 0x0C, 0x1A, MAX_UINTN };
STATIC UINTN  ThermalZoneCpu2_List[]  = { 0x05, 0x12, 0x13, 0x1C, 0x20, 0x21, 0x1D, 0x03, 0x10, 0x11, 0x1E, 0x1F, MAX_UINTN };
STATIC UINTN  ThermalZoneCpu3_List[]  = { 0x07, 0x14, 0x15, 0x22, 0x23, 0x0B, 0x18, 0x19, 0x26, 0x27, 0x28, 0x29, 0x09, 0x16, 0x17, 0x24, 0x25, MAX_UINTN };
STATIC UINTN  ThermalZoneSoc0_List[]  = { 0x2A, 0x2B, 0x2D, 0x2C, 0x3B, 0x3A, 0x49, 0x2F, 0x2E, 0x3D, 0x3C, 0x4B, MAX_UINTN };
STATIC UINTN  ThermalZoneSoc1_List[]  = { 0x31, 0x30, 0x3F, 0x3E, 0x4D, 0x33, 0x32, 0x41, 0x40, 0x4F, 0x35, 0x34, 0x43, 0x42, 0x51, 0x36, 0x37, MAX_UINTN };
STATIC UINTN  ThermalZoneSoc2_List[]  = { 0x48, 0x38, 0x46, 0x4A, MAX_UINTN };
STATIC UINTN  ThermalZoneSoc3_List[]  = { 0x4C, 0x4E, 0x50, 0x44, 0x52, MAX_UINTN };
STATIC UINTN  ThermalZoneSoc4_List[]  = { MAX_UINTN };
STATIC UINTN  ThermalZoneTjMax_List[] = { 0x00, MAX_UINTN };

STATIC CONST THERMAL_ZONE_DATA  ThermalZoneData[] = {
  { TH500_THERMAL_ZONE_CPU0,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneCpu0_List,  L"Thermal Zone Skt%d CPU0"  },
  { TH500_THERMAL_ZONE_CPU1,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneCpu1_List,  L"Thermal Zone Skt%d CPU1"  },
  { TH500_THERMAL_ZONE_CPU2,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneCpu2_List,  L"Thermal Zone Skt%d CPU2"  },
  { TH500_THERMAL_ZONE_CPU3,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneCpu3_List,  L"Thermal Zone Skt%d CPU3"  },
  { TH500_THERMAL_ZONE_SOC0,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneSoc0_List,  L"Thermal Zone Skt%d SOC0"  },
  { TH500_THERMAL_ZONE_SOC1,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneSoc1_List,  L"Thermal Zone Skt%d SOC1"  },
  { TH500_THERMAL_ZONE_SOC2,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneSoc2_List,  L"Thermal Zone Skt%d SOC2"  },
  { TH500_THERMAL_ZONE_SOC3,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneSoc3_List,  L"Thermal Zone Skt%d SOC3"  },
  { TH500_THERMAL_ZONE_SOC4,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneSoc4_List,  L"Thermal Zone Skt%d SOC4"  },
  { TH500_THERMAL_ZONE_TJ_MAX, FixedPcdGetBool (PcdUseSinglePassiveThermalZone),  TRUE, ThermalZoneTjMax_List, L"Thermal Zone Skt%d TJMax" },
  { TH500_THERMAL_ZONE_TJ_MIN, FALSE,                                             TRUE, NULL,                  L"Thermal Zone Skt%d TJMin" },
  { TH500_THERMAL_ZONE_TJ_AVG, FALSE,                                             TRUE, NULL,                  L"Thermal Zone Skt%d TJAvg" }
};

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
    EFI_ACPI_6_5_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_5_MULTIPLE_APIC_DESCRIPTION_TABLE_REVISION,
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

/** Initialize the socket info for tables needing bpmp in the platform configuration repository and patch SSDT.
 *
 * @param Repo Pointer to a repo structure that will be added to and updated with the data updated
 *
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
AddAcpiTable (
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepositoryInfo,
  EFI_ACPI_DESCRIPTION_HEADER     *AcpiTable
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
      NewAcpiTables[NVIDIAPlatformRepositoryInfo[Index].CmObjectCount].AcpiTableData      = AcpiTable;
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

/** Install SSDT for TPM

  @param Repo Pointer to a repo structure that will be added to and updated with the data updated

  @retval EFI_SUCCESS       Success
  @retval !(EFI_SUCCESS)    Other errors

**/
STATIC
EFI_STATUS
EFIAPI
UpdateTpmInfo (
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepositoryInfo
  )
{
  EFI_STATUS  Status;
  UINT32      ManufacturerID;

  if (!PcdGetBool (PcdTpmEnable)) {
    return EFI_SUCCESS;
  }

  //
  // Check if TPM is accessible
  //
  Status = Tpm2GetCapabilityManufactureID (&ManufacturerID);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: TPM is inaccessible - %r\n", __FUNCTION__, Status));
    return EFI_SUCCESS;
  }

  //
  // Install SSDT with TPM node
  //
  Status = AddAcpiTable (PlatformRepositoryInfo, (EFI_ACPI_DESCRIPTION_HEADER *)ssdttpm_aml_code);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to add SSDT for TPM - %r\r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
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
    patch thermal zone coefficients in SSDT
    Installs the BPMP SSDT

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateThermalZoneInfoAndInstallSsdt (
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepositoryInfo
  )
{
  EFI_STATUS                   Status;
  VOID                         *DtbBase;
  UINTN                        DtbSize;
  INTN                         NodeOffset;
  CONST UINT32                 *Temp;
  INT32                        TempLen;
  UINT32                       SocketId;
  UINT16                       PsvTemp;
  UINT16                       CrtTemp;
  UINT8                        ThermCoeff1;
  UINT8                        ThermCoeff2;
  UINT32                       FastSampPeriod;
  AML_ROOT_NODE_HANDLE         RootNode;
  AML_OBJECT_NODE_HANDLE       ScopeNode;
  AML_OBJECT_NODE_HANDLE       TZNode;
  AML_OBJECT_NODE_HANDLE       Node;
  EFI_ACPI_DESCRIPTION_HEADER  *BpmpTable;
  CHAR8                        ThermalZoneString[ACPI_PATCH_MAX_PATH];
  CHAR16                       UnicodeString[MAX_UNICODE_STRING_LEN];
  UINTN                        ThermalZoneIndex;
  UINTN                        ThermalZoneUid = 0;
  UINTN                        DevicesPerZone;
  UINTN                        DeviceIndex;
  UINTN                        TotalZones;
  UINTN                        DevicesPerSubZone;
  UINTN                        SubZoneIndex;
  UINTN                        CurrentDevice;
  UINTN                        FirstAvailCorePerSkt;
  UINTN                        CurrentCpu;

  Status = DtPlatformLoadDtb (&DtbBase, &DtbSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NodeOffset = fdt_path_offset (DtbBase, "/firmware/acpi");
  if (NodeOffset < 0) {
    return EFI_SUCCESS;
  }

  PsvTemp        = MAX_UINT16;
  CrtTemp        = MAX_UINT16;
  FastSampPeriod = MAX_UINT32;
  ThermCoeff1    = MAX_UINT8;
  ThermCoeff2    = MAX_UINT8;

  Temp = NULL;
  Temp = (CONST UINT32 *)fdt_getprop (DtbBase, NodeOffset, "override-thermal-zone-passive-cooling-trip-point-temp", &TempLen);
  if ((Temp != NULL) && (TempLen == sizeof (UINT32)) && (SwapBytes32 (*Temp) != MAX_UINT16)) {
    PsvTemp = SwapBytes32 (*Temp) + 2732;
  }

  if (PsvTemp == MAX_UINT16) {
    PsvTemp = TH500_THERMAL_ZONE_PSV + 2732;
  }

  Temp = NULL;
  Temp = (CONST UINT32 *)fdt_getprop (DtbBase, NodeOffset, "override-thermal-zone-critical-point-temp", &TempLen);
  if ((Temp != NULL) && (TempLen == sizeof (UINT32)) && (SwapBytes32 (*Temp) != MAX_UINT16)) {
    CrtTemp = SwapBytes32 (*Temp) + 2732;
  }

  if (CrtTemp == MAX_UINT16) {
    CrtTemp = TH500_THERMAL_ZONE_CRT + 2732;
  }

  Temp = NULL;
  Temp = (CONST UINT32 *)fdt_getprop (DtbBase, NodeOffset, "override-thermal-coefficient-tc1", &TempLen);
  if ((Temp != NULL) && (TempLen == sizeof (UINT32))) {
    ThermCoeff1 = SwapBytes32 (*Temp);
  }

  if (ThermCoeff1 == MAX_UINT8) {
    ThermCoeff1 = TH500_THERMAL_ZONE_TC1;
  }

  Temp = NULL;
  Temp = (CONST UINT32 *)fdt_getprop (DtbBase, NodeOffset, "override-thermal-coefficient-tc2", &TempLen);
  if ((Temp != NULL) && (TempLen == sizeof (UINT32))) {
    ThermCoeff2 = SwapBytes32 (*Temp);
  }

  if (ThermCoeff2 == MAX_UINT8) {
    ThermCoeff2 = TH500_THERMAL_ZONE_TC2;
  }

  Temp = NULL;
  Temp = (CONST UINT32 *)fdt_getprop (DtbBase, NodeOffset, "override-thermal-fast-sampling-period", &TempLen);
  if ((Temp != NULL) && (TempLen == sizeof (UINT32))) {
    FastSampPeriod = SwapBytes32 (*Temp);
  }

  if (FastSampPeriod == MAX_UINT32) {
    FastSampPeriod = TH500_THERMAL_ZONE_TFP;
  }

  for (SocketId = 0; SocketId < PcdGet32 (PcdTegraMaxSockets); SocketId++) {
    if (!IsSocketEnabled (SocketId)) {
      continue;
    }

    Status = AmlParseDefinitionBlock (AcpiBpmpTableArray[SocketId], &RootNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to open BPMP socket ACPI table - %r\r\n", Status));
      return Status;
    }

    Status = AmlFindNode (RootNode, "_SB", &ScopeNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to find node %a\r\n", ThermalZoneString));
      ASSERT_EFI_ERROR (Status);
      return Status;
    }

    for (ThermalZoneIndex = 0; ThermalZoneIndex < ARRAY_SIZE (ThermalZoneData); ThermalZoneIndex++) {
      DevicesPerZone = 0;
      if (ThermalZoneData[ThermalZoneIndex].PassiveSupported &&
          (ThermalZoneData[ThermalZoneIndex].PassiveCpus != NULL))
      {
        DevicesPerZone = 0;
        while (ThermalZoneData[ThermalZoneIndex].PassiveCpus[DevicesPerZone] != MAX_UINTN) {
          DevicesPerZone++;
        }
      }

      TotalZones = (DevicesPerZone + (MAX_DEVICES_PER_THERMAL_ZONE - 1)) / MAX_DEVICES_PER_THERMAL_ZONE;
      if (TotalZones == 0) {
        TotalZones = 1;
      }

      CurrentDevice     = 0;
      DevicesPerSubZone = DevicesPerZone / TotalZones;
      if ((DevicesPerZone % TotalZones) != 0) {
        DevicesPerSubZone++;
      }

      if (ThermalZoneData[ThermalZoneIndex].SocketFormatString != NULL) {
        UnicodeSPrint (
          UnicodeString,
          sizeof (UnicodeString),
          ThermalZoneData[ThermalZoneIndex].SocketFormatString,
          SocketId
          );
      }

      for (DeviceIndex = 0; DeviceIndex < TotalZones; DeviceIndex++) {
        AsciiSPrint (ThermalZoneString, sizeof (ThermalZoneString), "TZ%02x", ThermalZoneUid);
        ThermalZoneUid++;
        Status = AmlCodeGenThermalZone (ThermalZoneString, ScopeNode, &TZNode);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Failed to create thermal zone - %r\r\n", Status));
          ASSERT_EFI_ERROR (Status);
          return Status;
        }

        AsciiSPrint (ThermalZoneString, sizeof (ThermalZoneString), "\\_SB.BPM%0u.TEMP", SocketId);
        Status = AmlCodeGenMethodRetNameStringIntegerArgument (
                   "_TMP",
                   ThermalZoneString,
                   0,
                   FALSE,
                   0,
                   ThermalZoneData[ThermalZoneIndex].ZoneId,
                   TZNode,
                   NULL
                   );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Failed to create TMP method - %r\r\n", Status));
          ASSERT_EFI_ERROR (Status);
          return Status;
        }

        Status = AmlCodeGenNameInteger ("_TZP", TEMP_POLL_TIME_100MS, TZNode, NULL);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Failed to create _TZP node - %r\r\n", Status));
          ASSERT_EFI_ERROR (Status);
          return Status;
        }

        if (ThermalZoneData[ThermalZoneIndex].SocketFormatString != NULL) {
          Status = AmlCodeGenNameUnicodeString ("_STR", UnicodeString, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _STR node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }
        }

        if (ThermalZoneData[ThermalZoneIndex].CriticalSupported) {
          Status = AmlCodeGenNameInteger ("_CRT", CrtTemp, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _CRT node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }
        }

        if (ThermalZoneData[ThermalZoneIndex].PassiveSupported && (DevicesPerZone != 0)) {
          Status = AmlCodeGenNameInteger ("_PSV", PsvTemp, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _PSV node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }

          Status = AmlCodeGenNameInteger ("_TC1", ThermCoeff1, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _TC1 node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }

          Status = AmlCodeGenNameInteger ("_TC2", ThermCoeff2, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _TC2 node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }

          Status = AmlCodeGenNameInteger ("_TSP", TH500_THERMAL_ZONE_TSP, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _TSP node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }

          Status = AmlCodeGenNameInteger ("_TFP", FastSampPeriod, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "-->Failed to create _TFP node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }

          Status = AmlCodeGenNamePackage ("_PSL", TZNode, &Node);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _PSL node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }

          for (SubZoneIndex = 0; SubZoneIndex < DevicesPerSubZone; SubZoneIndex++) {
            CurrentCpu = ThermalZoneData[ThermalZoneIndex].PassiveCpus[CurrentDevice];
            if (CurrentCpu == MAX_UINTN) {
              break;
            }

            // treat TJMAX zone as a special case
            if (ThermalZoneData[ThermalZoneIndex].ZoneId == TH500_THERMAL_ZONE_TJ_MAX ) {
              FirstAvailCorePerSkt = 0;
              while (FirstAvailCorePerSkt < PLATFORM_CPUS_PER_SOCKET) {
                if (IsCoreEnabled (FirstAvailCorePerSkt + SocketId * PLATFORM_CPUS_PER_SOCKET)) {
                  CurrentCpu = FirstAvailCorePerSkt;
                  break;
                }

                FirstAvailCorePerSkt++;
              }

              ASSERT (FirstAvailCorePerSkt != PLATFORM_CPUS_PER_SOCKET);
            } else if (!IsCoreEnabled (CurrentCpu + SocketId * PLATFORM_CPUS_PER_SOCKET)) {
              CurrentDevice++;
              continue;
            }

            AsciiSPrint (ThermalZoneString, sizeof (ThermalZoneString), "\\_SB_.C%03x.C%03x", SocketId, CurrentCpu);
            Status = AmlAddNameStringToNamedPackage (ThermalZoneString, Node);
            if (EFI_ERROR (Status)) {
              DEBUG ((DEBUG_ERROR, "Failed to add %a to _PSL node - %r\r\n", ThermalZoneString, Status));
              ASSERT_EFI_ERROR (Status);
              return Status;
            }

            CurrentDevice++;
          }
        }
      }
    }

    Status = AmlSerializeDefinitionBlock (RootNode, &BpmpTable);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Serialize BPMP socket ACPI table - %r\r\n", Status));
      return Status;
    }

    Status = AddAcpiTable (PlatformRepositoryInfo, BpmpTable);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to add BPMP socket ACPI table - %r\r\n", Status));
      return Status;
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
    DEBUG ((DEBUG_ERROR, "%a: SocketId %u exceeding AcpiMrqPwrLimitMaxPatchName size\r\n", __FUNCTION__, SocketId));
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
    DEBUG ((DEBUG_ERROR, "%a: SocketId %u exceeding AcpiMrqPwrLimitMinPatchName size\r\n", __FUNCTION__, SocketId));
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
    DEBUG ((DEBUG_ERROR, "%a: Index %u exceeding AcpiTimerInstructionEnableVarName size\r\n", __FUNCTION__, SocketId));
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

/** patch _STA to enable/disable power meter device

  @param[in]     SocketId                Socket Id
  @param[in]     TelemetryDataBuffAddr   Telemetry Data Buff Addr

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdatePowerMeterStaInfo (
  IN  UINT32  SocketId,
  IN  UINT64  TelemetryDataBuffAddr
  )
{
  EFI_STATUS            Status;
  NVIDIA_AML_NODE_INFO  AcpiNodeInfo;
  UINT32                Index;
  UINT32                TelLayoutValidFlags0;
  UINT32                TelLayoutValidFlags1;
  UINT32                TelLayoutValidFlags2;
  UINT32                PwrMeterIndex;
  UINT8                 PwrMeterStatus;
  UINT32                *TelemetryData;

  STATIC CHAR8 *CONST  AcpiPwrMeterStaPatchName[] = {
    "_SB_.PM00._STA",
    "_SB_.PM01._STA",
    "_SB_.PM02._STA",
    "_SB_.PM03._STA",
    "_SB_.PM10._STA",
    "_SB_.PM11._STA",
    "_SB_.PM12._STA",
    "_SB_.PM13._STA",
    "_SB_.PM20._STA",
    "_SB_.PM21._STA",
    "_SB_.PM22._STA",
    "_SB_.PM23._STA",
    "_SB_.PM30._STA",
    "_SB_.PM31._STA",
    "_SB_.PM32._STA",
    "_SB_.PM33._STA",
  };

  Status               = EFI_SUCCESS;
  TelemetryData        = NULL;
  TelemetryData        = (UINT32 *)TelemetryDataBuffAddr;
  TelLayoutValidFlags0 = TelemetryData[TH500_TEL_LAYOUT_VALID_FLAGS0_IDX];
  TelLayoutValidFlags1 = TelemetryData[TH500_TEL_LAYOUT_VALID_FLAGS1_IDX];
  TelLayoutValidFlags2 = TelemetryData[TH500_TEL_LAYOUT_VALID_FLAGS2_IDX];

  if (SocketId >= ((ARRAY_SIZE (AcpiPwrMeterStaPatchName)) / TH500_MAX_PWR_METER)) {
    DEBUG ((DEBUG_ERROR, "%a: Index %u exceeding AcpiPwrMeterStaPatchName size\r\n", __FUNCTION__, SocketId));
    Status = EFI_INVALID_PARAMETER;
    goto ErrorExit;
  }

  for (Index = 0; Index < TH500_MAX_PWR_METER; Index++) {
    if ((TelLayoutValidFlags0 & (TH500_MODULE_PWR_IDX_VALID_FLAG << Index)) ||
        (TelLayoutValidFlags2 & (TH500_MODULE_PWR_1SEC_IDX_VALID_FLAG << Index)))
    {
      PwrMeterIndex = (SocketId * TH500_MAX_PWR_METER) + Index;
      Status        = PatchProtocol->FindNode (PatchProtocol, AcpiPwrMeterStaPatchName[PwrMeterIndex], &AcpiNodeInfo);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Acpi pwr meter sta node is not found for patching %a - %r\r\n",
          __FUNCTION__,
          AcpiPwrMeterStaPatchName[PwrMeterIndex],
          Status
          ));
        Status = EFI_SUCCESS;
        goto ErrorExit;
      }

      PwrMeterStatus = 0xF;
      Status         = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &PwrMeterStatus, sizeof (PwrMeterStatus));
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, AcpiPwrMeterStaPatchName[PwrMeterIndex], Status));
        Status = EFI_SUCCESS;
        goto ErrorExit;
      }
    }
  }

ErrorExit:
  return Status;
}

/** Get the Dram speed from the telemtry data and update the dram info in the
    PlatformResourceData Hob.

  @param[in]     SocketId                Socket Id
  @param[in]     TelemetryDataBuffAddr   Telemetry Data Buff Addr

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateDramSpeed (
  IN  UINT32  SocketId,
  IN  UINT64  TelemetryDataBuffAddr
  )
{
  TEGRA_DRAM_DEVICE_INFO  *DramInfo;
  VOID                    *Hob;
  UINT32                  *TelemetryData;

  TelemetryData = NULL;
  TelemetryData = (UINT32 *)TelemetryDataBuffAddr;
  Hob           = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    DramInfo                    = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->DramDeviceInfo;
    DramInfo[SocketId].SpeedKhz = TelemetryData[TH500_TEL_LAYOUT_DRAM_RATE_IDX];
    DEBUG ((DEBUG_INFO, "Setting Dram Speed to %u for Socket %u\n", DramInfo[SocketId].SpeedKhz, SocketId));
  } else {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
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
      DEBUG ((DEBUG_ERROR, "%a: Failed to get Bpmp node phandle for index - %u\n", __FUNCTION__, Index));
      goto ErrorExit;
    } else {
      CopyMem ((VOID *)&BpmpHandle, Property, sizeof (UINT32));
      BpmpHandle = SwapBytes32 (BpmpHandle);
    }

    SocketId = 0;
    Property = fdt_getprop (Dtb, NodeOffset, "nvidia,hw-instance-id", &PropertySize);
    if ((Property == NULL) || (PropertySize < sizeof (UINT32))) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get Socket Id for index - %u\n", __FUNCTION__, Index));
      goto ErrorExit;
    } else {
      CopyMem ((VOID *)&SocketId, Property, sizeof (UINT32));
      SocketId = SwapBytes32 (SocketId);
    }

    if (SocketId >= PcdGet32 (PcdTegraMaxSockets)) {
      DEBUG ((DEBUG_ERROR, "%a: SocketId %u exceeds number of sockets\r\n", __FUNCTION__, SocketId));
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
      DEBUG ((DEBUG_ERROR, "%a: Index %u exceeding AcpiMrqTelemetryBufferPatchName size\r\n", __FUNCTION__, Index));
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

    Status = UpdatePowerMeterStaInfo (SocketId, TelemetryDataBuffAddr);
    if (EFI_ERROR (Status)) {
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

    Status = UpdateDramSpeed (SocketId, TelemetryDataBuffAddr);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to update Dram speed %r\n", __FUNCTION__, Status));
      Status = EFI_SUCCESS;
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

  Status = UpdateTpmInfo (NVIDIAPlatformRepositoryInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // SSDT for socket1 onwards
  for (SocketId = 1; SocketId < PcdGet32 (PcdTegraMaxSockets); SocketId++) {
    if (!IsSocketEnabled (SocketId)) {
      continue;
    }

    Status =  AddAcpiTable (NVIDIAPlatformRepositoryInfo, (EFI_ACPI_DESCRIPTION_HEADER *)AcpiTableArray[SocketId]);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  PlatformType = TegraGetPlatform ();
  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    Status = UpdateThermalZoneInfoAndInstallSsdt (NVIDIAPlatformRepositoryInfo);
    if (EFI_ERROR (Status)) {
      ASSERT_EFI_ERROR (Status);
      return Status;
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
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    return Status;
  }

  if (!SkipSrat) {
    Status = InstallStaticResourceAffinityTable (&Repo, (UINTN)RepoEnd, NVIDIAPlatformRepositoryInfo);
    if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
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
    Status = InstallHeterogeneousMemoryAttributeTable (NVIDIAPlatformRepositoryInfo);
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

  Status = InstallCmSmbiosTableList (&Repo, (UINTN)RepoEnd, NVIDIAPlatformRepositoryInfo);
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

  PatchOemTableId ();

  return gBS->InstallMultipleProtocolInterfaces (
                &ImageHandle,
                &gNVIDIAConfigurationManagerDataProtocolGuid,
                (VOID *)NVIDIAPlatformRepositoryInfo,
                NULL
                );
}
