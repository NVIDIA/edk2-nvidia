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

  Portions provided under the following terms:
  Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/
#include <ConfigurationManagerObject.h>

#include <Library/ArmLib.h>
#include <Library/ArmGicLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <IndustryStandard/DebugPort2Table.h>
#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>

#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "Platform.h"
#include <T234/T234Definitions.h>

#include "Dsdt.hex"
#include "SsdtPci.hex"

/** The platform configuration repository information.
*/
STATIC
EDKII_PLATFORM_REPOSITORY_INFO NVIDIAPlatformRepositoryInfo[EStdObjMax + EArmObjMax];

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
  // SPCR Table
  {
    EFI_ACPI_6_3_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE,
    EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSpcr),
    NULL,
    FixedPcdGet64(PcdAcpiTegraUartOemTableId),
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // MCFG Table
  {
    EFI_ACPI_6_3_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMcfg),
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
  // SSDT Table
  {
    EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt),
    (EFI_ACPI_DESCRIPTION_HEADER*)ssdtpci_aml_code,
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

/** The platform power management profile information.
*/
STATIC
CM_ARM_POWER_MANAGEMENT_PROFILE_INFO PmProfileInfo = {
  EFI_ACPI_6_3_PM_PROFILE_ENTERPRISE_SERVER
};

/** The platform GIC CPU interface information.
*/
STATIC
CM_ARM_GICC_INFO GicCInfo[] = {
  /*
    GICC_ENTRY (CPUInterfaceNumber, Mpidr, PmuIrq, VGicIrq, EnergyEfficiency, ProximityDomain)
  */
    GICC_ENTRY (0, GET_MPID (0, 0), 23, 25, 0, 0),
};

/** The platform GIC distributor information.
*/
STATIC
CM_ARM_GICD_INFO GicDInfo = {
  0,
  0,
  3
};

/** The platform GIC redistributor information.
*/
STATIC
CM_ARM_GIC_REDIST_INFO GicRedistInfo = {
  0,
  (ARM_GICR_CTLR_FRAME_SIZE + ARM_GICR_SGI_PPI_FRAME_SIZE) * T234_GIC_REDISTRIBUTOR_INSTANCES
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

// SPCR Serial Port
/** The platform SPCR serial port information.
*/
CM_ARM_SERIAL_PORT_INFO SpcrSerialPort = {
  FixedPcdGet64 (PcdTegra16550UartBaseT234),
  T234_UARTA_INTR,
  FixedPcdGet64 (PcdUartDefaultBaudRate),
  0,
  EFI_ACPI_DBG2_PORT_SUBTYPE_SERIAL_FULL_16550
};

/** PCI Configuration Space Info
*/
STATIC
CM_ARM_PCI_CONFIG_SPACE_INFO PciConfigInfo[] = {
  {
    // The physical base address for the PCI segment
    T234_PCIE_C1_CFG_BASE_ADDR,
    // The PCI segment group number
    1,
    // The start bus number
    T234_PCIE_BUS_MIN,
    // The end bus number
    T234_PCIE_BUS_MAX
  }
};

/** Cache Info
 */
STATIC
CM_ARM_CACHE_INFO CacheInfo[] = {
  // L4 Cache Info
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
  // L3 Cache Info
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
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[2]),
    .NextLevelOfCacheToken = CM_NULL_TOKEN,
    .Size                  = 0x40000,
    .NumberOfSets          = 512,
    .Associativity         = 8,
    .Attributes            = CACHE_ATTRIBUTES (
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
      EFI_ACPI_6_3_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
    ),
    .LineSize              = 64,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[3]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[2]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
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
    .Token                 = REFERENCE_TOKEN (CacheInfo[4]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[2]),
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

/** Hercules Core Cluster Resources
 */
STATIC
CM_ARM_OBJ_REF HerculesCoreClusterResources[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[1]) },
};

/** Hercules Core Resources
 */
STATIC
CM_ARM_OBJ_REF HerculesCoreResources[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[3]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[4]) },
};

/** Processor Hierarchy Info
 */
STATIC
CM_ARM_PROC_HIERARCHY_INFO ProcHierarchyInfo[] = {
  // CCPLEX
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[0]),
    .Flags                      = PROC_NODE_FLAGS (
      EFI_ACPI_6_3_PPTT_PACKAGE_PHYSICAL,
      EFI_ACPI_6_3_PPTT_PROCESSOR_ID_INVALID,
      EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
      EFI_ACPI_6_3_PPTT_NODE_IS_NOT_LEAF,
      EFI_ACPI_6_3_PPTT_IMPLEMENTATION_IDENTICAL
    ),
    .ParentToken                = CM_NULL_TOKEN,
    .GicCToken                  = CM_NULL_TOKEN,
    .NoOfPrivateResources       = 1,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CcplexResources),
  },
  // Hercules Core Clusters
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[1]),
    .Flags                      = PROC_NODE_FLAGS (
      EFI_ACPI_6_3_PPTT_PACKAGE_NOT_PHYSICAL,
      EFI_ACPI_6_3_PPTT_PROCESSOR_ID_INVALID,
      EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
      EFI_ACPI_6_3_PPTT_NODE_IS_NOT_LEAF,
      EFI_ACPI_6_3_PPTT_IMPLEMENTATION_IDENTICAL
    ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[0]),
    .GicCToken                  = CM_NULL_TOKEN,
    .NoOfPrivateResources       = 1,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreClusterResources),
  },
  // Hercules Cores
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[2]),
    .Flags                      = PROC_NODE_FLAGS (
      EFI_ACPI_6_3_PPTT_PACKAGE_NOT_PHYSICAL,
      EFI_ACPI_6_3_PPTT_PROCESSOR_ID_VALID,
      EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
      EFI_ACPI_6_3_PPTT_NODE_IS_LEAF,
      EFI_ACPI_6_3_PPTT_IMPLEMENTATION_NOT_IDENTICAL
    ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[1]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[0]),
    .NoOfPrivateResources       = 2,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreResources),
  },
};

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
  UINTN Index;

  NVIDIAPlatformRepositoryInfo[0].CmObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjCfgMgrInfo);
  NVIDIAPlatformRepositoryInfo[0].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[0].CmObjectSize = sizeof (CmInfo);
  NVIDIAPlatformRepositoryInfo[0].CmObjectCount = sizeof (CmInfo) / sizeof (CM_STD_OBJ_CONFIGURATION_MANAGER_INFO);
  NVIDIAPlatformRepositoryInfo[0].CmObjectPtr = &CmInfo;

  NVIDIAPlatformRepositoryInfo[1].CmObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList);
  NVIDIAPlatformRepositoryInfo[1].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[1].CmObjectSize = sizeof (CmAcpiTableList);
  NVIDIAPlatformRepositoryInfo[1].CmObjectCount = sizeof (CmAcpiTableList) / sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
  NVIDIAPlatformRepositoryInfo[1].CmObjectPtr = &CmAcpiTableList;
  for(Index=0; Index<NVIDIAPlatformRepositoryInfo[1].CmObjectCount; Index++) {
    if (CmAcpiTableList[Index].AcpiTableSignature != EFI_ACPI_6_3_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE) {
      CmAcpiTableList[Index].OemTableId =  PcdGet64(PcdAcpiDefaultOemTableId);
    }
  }

  NVIDIAPlatformRepositoryInfo[2].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjBootArchInfo);
  NVIDIAPlatformRepositoryInfo[2].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[2].CmObjectSize = sizeof (BootArchInfo);
  NVIDIAPlatformRepositoryInfo[2].CmObjectCount = sizeof (BootArchInfo) / sizeof (CM_ARM_BOOT_ARCH_INFO);
  NVIDIAPlatformRepositoryInfo[2].CmObjectPtr = &BootArchInfo;

  NVIDIAPlatformRepositoryInfo[3].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjPowerManagementProfileInfo);
  NVIDIAPlatformRepositoryInfo[3].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[3].CmObjectSize = sizeof (PmProfileInfo);
  NVIDIAPlatformRepositoryInfo[3].CmObjectCount = sizeof (PmProfileInfo) / sizeof (CM_ARM_POWER_MANAGEMENT_PROFILE_INFO);
  NVIDIAPlatformRepositoryInfo[3].CmObjectPtr = &PmProfileInfo;

  NVIDIAPlatformRepositoryInfo[4].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicCInfo);
  NVIDIAPlatformRepositoryInfo[4].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[4].CmObjectSize = sizeof (GicCInfo);
  NVIDIAPlatformRepositoryInfo[4].CmObjectCount = sizeof (GicCInfo) / sizeof (CM_ARM_GICC_INFO);
  NVIDIAPlatformRepositoryInfo[4].CmObjectPtr = &GicCInfo;

  GicDInfo.PhysicalBaseAddress = PcdGet64 (PcdGicDistributorBase);
  NVIDIAPlatformRepositoryInfo[5].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicDInfo);
  NVIDIAPlatformRepositoryInfo[5].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[5].CmObjectSize = sizeof (GicDInfo);
  NVIDIAPlatformRepositoryInfo[5].CmObjectCount = sizeof (GicDInfo) / sizeof (CM_ARM_GICD_INFO);
  NVIDIAPlatformRepositoryInfo[5].CmObjectPtr = &GicDInfo;

  GicRedistInfo.DiscoveryRangeBaseAddress = PcdGet64 (PcdGicRedistributorsBase);
  NVIDIAPlatformRepositoryInfo[6].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicRedistributorInfo);
  NVIDIAPlatformRepositoryInfo[6].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[6].CmObjectSize = sizeof (GicRedistInfo);
  NVIDIAPlatformRepositoryInfo[6].CmObjectCount = sizeof (GicRedistInfo) / sizeof (CM_ARM_GIC_REDIST_INFO);
  NVIDIAPlatformRepositoryInfo[6].CmObjectPtr = &GicRedistInfo;

  NVIDIAPlatformRepositoryInfo[7].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGenericTimerInfo);
  NVIDIAPlatformRepositoryInfo[7].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[7].CmObjectSize = sizeof (GenericTimerInfo);
  NVIDIAPlatformRepositoryInfo[7].CmObjectCount = sizeof (GenericTimerInfo) / sizeof (CM_ARM_GENERIC_TIMER_INFO);
  NVIDIAPlatformRepositoryInfo[7].CmObjectPtr = &GenericTimerInfo;

  NVIDIAPlatformRepositoryInfo[8].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjSerialConsolePortInfo);
  NVIDIAPlatformRepositoryInfo[8].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[8].CmObjectSize = sizeof (SpcrSerialPort);
  NVIDIAPlatformRepositoryInfo[8].CmObjectCount = sizeof (SpcrSerialPort) / sizeof (CM_ARM_SERIAL_PORT_INFO);
  NVIDIAPlatformRepositoryInfo[8].CmObjectPtr = &SpcrSerialPort;

  NVIDIAPlatformRepositoryInfo[9].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjCacheInfo);
  NVIDIAPlatformRepositoryInfo[9].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[9].CmObjectSize = sizeof (CacheInfo);
  NVIDIAPlatformRepositoryInfo[9].CmObjectCount = ARRAY_SIZE (CacheInfo);
  NVIDIAPlatformRepositoryInfo[9].CmObjectPtr = &CacheInfo;

  NVIDIAPlatformRepositoryInfo[10].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  NVIDIAPlatformRepositoryInfo[10].CmObjectToken = REFERENCE_TOKEN (CcplexResources);
  NVIDIAPlatformRepositoryInfo[10].CmObjectSize = sizeof (CcplexResources);
  NVIDIAPlatformRepositoryInfo[10].CmObjectCount = ARRAY_SIZE (CcplexResources);
  NVIDIAPlatformRepositoryInfo[10].CmObjectPtr = &CcplexResources;

  NVIDIAPlatformRepositoryInfo[11].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  NVIDIAPlatformRepositoryInfo[11].CmObjectToken = REFERENCE_TOKEN (HerculesCoreClusterResources);
  NVIDIAPlatformRepositoryInfo[11].CmObjectSize = sizeof (HerculesCoreClusterResources);
  NVIDIAPlatformRepositoryInfo[11].CmObjectCount = ARRAY_SIZE (HerculesCoreClusterResources);
  NVIDIAPlatformRepositoryInfo[11].CmObjectPtr = &HerculesCoreClusterResources;

  NVIDIAPlatformRepositoryInfo[12].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  NVIDIAPlatformRepositoryInfo[12].CmObjectToken = REFERENCE_TOKEN (HerculesCoreResources);
  NVIDIAPlatformRepositoryInfo[12].CmObjectSize = sizeof (HerculesCoreResources);
  NVIDIAPlatformRepositoryInfo[12].CmObjectCount = ARRAY_SIZE (HerculesCoreResources);
  NVIDIAPlatformRepositoryInfo[12].CmObjectPtr = &HerculesCoreResources;

  NVIDIAPlatformRepositoryInfo[13].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjProcHierarchyInfo);
  NVIDIAPlatformRepositoryInfo[13].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[13].CmObjectSize = sizeof (ProcHierarchyInfo);
  NVIDIAPlatformRepositoryInfo[13].CmObjectCount = ARRAY_SIZE (ProcHierarchyInfo);
  NVIDIAPlatformRepositoryInfo[13].CmObjectPtr = &ProcHierarchyInfo;

  NVIDIAPlatformRepositoryInfo[14].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjPciConfigSpaceInfo);
  NVIDIAPlatformRepositoryInfo[14].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[14].CmObjectSize = sizeof (PciConfigInfo);
  NVIDIAPlatformRepositoryInfo[14].CmObjectCount = sizeof (PciConfigInfo) / sizeof (CM_ARM_PCI_CONFIG_SPACE_INFO);
  NVIDIAPlatformRepositoryInfo[14].CmObjectPtr = &PciConfigInfo;

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
  if (ChipID != T234_CHIP_ID) {
    return EFI_SUCCESS;
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
