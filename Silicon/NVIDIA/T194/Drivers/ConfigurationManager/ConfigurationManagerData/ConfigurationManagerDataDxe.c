/** @file
  Configuration Manager Data Dxe

  Copyright (c) 2019 - 2020, NVIDIA Corporation. All rights reserved.
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
#include <ConfigurationManagerObject.h>

#include <Library/ArmLib.h>
#include <Library/DebugLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <IndustryStandard/DebugPort2Table.h>
#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>

#include <Protocol/ConfigurationManagerDataProtocol.h>

#include <NVIDIAConfiguration.h>

#include "Platform.h"
#include <T194/T194Definitions.h>

#include "Dsdt.hex"
#include "SsdtPci.hex"
#include "SsdtPciEmpty.hex"

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
  // DSDT Table
  {
    EFI_ACPI_6_3_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_3_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDsdt),
    (EFI_ACPI_DESCRIPTION_HEADER*)dsdt_aml_code,
    0,
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // SSDT table describing the PCI root complex
  {
    EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt),
    (EFI_ACPI_DESCRIPTION_HEADER*)ssdtpciempty_aml_code,
    0,
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // PCI MCFG Table
  {
    EFI_ACPI_6_3_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMcfg),
    NULL,
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
  GICC_ENTRY (0, GET_MPID (0, 0), 0x180, 25,   0, 0),
  GICC_ENTRY (1, GET_MPID (0, 1), 0x181, 25,   0, 0),
  GICC_ENTRY (2, GET_MPID (1, 0), 0x182, 25,   0, 0),
  GICC_ENTRY (3, GET_MPID (1, 1), 0x183, 25,   0, 0),
  GICC_ENTRY (4, GET_MPID (2, 0), 0x184, 25,   0, 0),
  GICC_ENTRY (5, GET_MPID (2, 1), 0x185, 25,   0, 0),
  GICC_ENTRY (6, GET_MPID (3, 0), 0x186, 25,   0, 0),
  GICC_ENTRY (7, GET_MPID (3, 1), 0x187, 25,   0, 0),
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

/** The platform SPCR serial port information.
*/
STATIC
CM_ARM_SERIAL_PORT_INFO SpcrSerialPort = {
  FixedPcdGet64 (PcdTegra16550UartBaseT194),
  T194_UARTB_INTR,
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
    T194_PCIE_C0_CFG_BASE_ADDR,
    // The PCI segment group number
    0,
    // The start bus number
    T194_PCIE_BUS_MIN,
    // The end bus number
    T194_PCIE_BUS_MAX,
  },

  {
    // The physical base address for the PCI segment
    T194_PCIE_C1_CFG_BASE_ADDR,
    // The PCI segment group number
    1,
    // The start bus number
    T194_PCIE_BUS_MIN,
    // The end bus number
    T194_PCIE_BUS_MAX,
  },

  {
    // The physical base address for the PCI segment
    T194_PCIE_C3_CFG_BASE_ADDR,
    // The PCI segment group number
    3,
    // The start bus number
    T194_PCIE_BUS_MIN,
    // The end bus number
    T194_PCIE_BUS_MAX,
  },

  {
    // The physical base address for the PCI segment
    T194_PCIE_C5_CFG_BASE_ADDR,
    // The PCI segment group number
    5,
    // The start bus number
    T194_PCIE_BUS_MIN,
    // The end bus number
    T194_PCIE_BUS_MAX,
  },

};

/** Empty PCI Configuration Space Info
*/
STATIC
CM_ARM_PCI_CONFIG_SPACE_INFO PciConfigInfoEmpty[] = {0};

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
  // Four Carmel Core Clusters
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
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreClusterResources),
  },
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[2]),
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
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreClusterResources),
  },
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[3]),
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
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreClusterResources),
  },
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[4]),
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
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreClusterResources),
  },
  // Eight Carmel Cores
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[5]),
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
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreResources),
  },
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[6]),
    .Flags                      = PROC_NODE_FLAGS (
      EFI_ACPI_6_3_PPTT_PACKAGE_NOT_PHYSICAL,
      EFI_ACPI_6_3_PPTT_PROCESSOR_ID_VALID,
      EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
      EFI_ACPI_6_3_PPTT_NODE_IS_LEAF,
      EFI_ACPI_6_3_PPTT_IMPLEMENTATION_NOT_IDENTICAL
    ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[1]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[1]),
    .NoOfPrivateResources       = 2,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreResources),
  },
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[7]),
    .Flags                      = PROC_NODE_FLAGS (
      EFI_ACPI_6_3_PPTT_PACKAGE_NOT_PHYSICAL,
      EFI_ACPI_6_3_PPTT_PROCESSOR_ID_VALID,
      EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
      EFI_ACPI_6_3_PPTT_NODE_IS_LEAF,
      EFI_ACPI_6_3_PPTT_IMPLEMENTATION_NOT_IDENTICAL
    ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[2]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[2]),
    .NoOfPrivateResources       = 2,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreResources),
  },
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[8]),
    .Flags                      = PROC_NODE_FLAGS (
      EFI_ACPI_6_3_PPTT_PACKAGE_NOT_PHYSICAL,
      EFI_ACPI_6_3_PPTT_PROCESSOR_ID_VALID,
      EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
      EFI_ACPI_6_3_PPTT_NODE_IS_LEAF,
      EFI_ACPI_6_3_PPTT_IMPLEMENTATION_NOT_IDENTICAL
    ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[2]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[3]),
    .NoOfPrivateResources       = 2,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreResources),
  },
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[9]),
    .Flags                      = PROC_NODE_FLAGS (
      EFI_ACPI_6_3_PPTT_PACKAGE_NOT_PHYSICAL,
      EFI_ACPI_6_3_PPTT_PROCESSOR_ID_VALID,
      EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
      EFI_ACPI_6_3_PPTT_NODE_IS_LEAF,
      EFI_ACPI_6_3_PPTT_IMPLEMENTATION_NOT_IDENTICAL
    ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[3]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[4]),
    .NoOfPrivateResources       = 2,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreResources),
  },
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[10]),
    .Flags                      = PROC_NODE_FLAGS (
      EFI_ACPI_6_3_PPTT_PACKAGE_NOT_PHYSICAL,
      EFI_ACPI_6_3_PPTT_PROCESSOR_ID_VALID,
      EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
      EFI_ACPI_6_3_PPTT_NODE_IS_LEAF,
      EFI_ACPI_6_3_PPTT_IMPLEMENTATION_NOT_IDENTICAL
    ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[3]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[5]),
    .NoOfPrivateResources       = 2,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreResources),
  },
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[11]),
    .Flags                      = PROC_NODE_FLAGS (
      EFI_ACPI_6_3_PPTT_PACKAGE_NOT_PHYSICAL,
      EFI_ACPI_6_3_PPTT_PROCESSOR_ID_VALID,
      EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
      EFI_ACPI_6_3_PPTT_NODE_IS_LEAF,
      EFI_ACPI_6_3_PPTT_IMPLEMENTATION_NOT_IDENTICAL
    ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[4]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[6]),
    .NoOfPrivateResources       = 2,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreResources),
  },
  {
    .Token                      = REFERENCE_TOKEN (ProcHierarchyInfo[12]),
    .Flags                      = PROC_NODE_FLAGS (
      EFI_ACPI_6_3_PPTT_PACKAGE_NOT_PHYSICAL,
      EFI_ACPI_6_3_PPTT_PROCESSOR_ID_VALID,
      EFI_ACPI_6_3_PPTT_PROCESSOR_IS_NOT_THREAD,
      EFI_ACPI_6_3_PPTT_NODE_IS_LEAF,
      EFI_ACPI_6_3_PPTT_IMPLEMENTATION_NOT_IDENTICAL
    ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[4]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[7]),
    .NoOfPrivateResources       = 2,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CarmelCoreResources),
  },
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
  EFI_STATUS               Status;
  NvidiaPcieEnableVariable VariableData;
  UINTN                    VariableSize;
  UINT32                   VariableAttributes;

  VariableSize = sizeof (VariableData);
  Status = gRT->GetVariable (NVIDIA_PCIE_ENABLE_IN_OS_VARIABLE_NAME, &gNVIDIATokenSpaceGuid,
                             &VariableAttributes, &VariableSize, (VOID *)&VariableData);
  if (EFI_ERROR (Status) || (VariableSize != sizeof (VariableData))) {
    return FALSE;
  }

  return (VariableData.Enabled == 1);
}

/** Apply platform specific CM overrides.
 */
STATIC
VOID
EFIAPI
ApplyConfigurationManagerOverrides ()
{
  UINT32 Count;

  if (IsPcieEnabled ()) {
    for (Count = 0; Count < sizeof (CmAcpiTableList)/sizeof (CmAcpiTableList[0]); Count++) {
      if (CmAcpiTableList[Count].AcpiTableSignature == EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE &&
          CmAcpiTableList[Count].AcpiTableData == (EFI_ACPI_DESCRIPTION_HEADER*)ssdtpciempty_aml_code) {
        CmAcpiTableList[Count].AcpiTableData = (EFI_ACPI_DESCRIPTION_HEADER*)ssdtpci_aml_code;
        break;
      }
    }

    for (Count = 0; Count < sizeof (NVIDIAPlatformRepositoryInfo)/sizeof (NVIDIAPlatformRepositoryInfo[0]); Count++) {
      if (NVIDIAPlatformRepositoryInfo[Count].CmObjectId == CREATE_CM_ARM_OBJECT_ID (EArmObjPciConfigSpaceInfo)) {
        NVIDIAPlatformRepositoryInfo[Count].CmObjectSize = sizeof (PciConfigInfo);
        NVIDIAPlatformRepositoryInfo[Count].CmObjectCount = sizeof (PciConfigInfo) / sizeof (CM_ARM_PCI_CONFIG_SPACE_INFO);
        NVIDIAPlatformRepositoryInfo[Count].CmObjectPtr = &PciConfigInfo;
        break;
      }
    }
  }
}

/** Initialize the platform configuration repository.
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
InitializePlatformRepository ()
{
  UINTN Index;
  UINTN GicInterruptInterfaceBase;
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

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjPowerManagementProfileInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (PmProfileInfo);
  Repo->CmObjectCount = sizeof (PmProfileInfo) / sizeof (CM_ARM_POWER_MANAGEMENT_PROFILE_INFO);
  Repo->CmObjectPtr = &PmProfileInfo;
  Repo++;

  GicInterruptInterfaceBase = PcdGet64(PcdGicInterruptInterfaceBase);
  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicCInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (GicCInfo);
  Repo->CmObjectCount = sizeof (GicCInfo) / sizeof (CM_ARM_GICC_INFO);
  Repo->CmObjectPtr = &GicCInfo;
  for(Index=0; Index<Repo->CmObjectCount; Index++) {
    GicCInfo[Index].PhysicalBaseAddress =  GicInterruptInterfaceBase;
  }
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

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjSerialConsolePortInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (SpcrSerialPort);
  Repo->CmObjectCount = sizeof (SpcrSerialPort) / sizeof (CM_ARM_SERIAL_PORT_INFO);
  Repo->CmObjectPtr = &SpcrSerialPort;
  Repo++;

  Repo->CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjPciConfigSpaceInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize = sizeof (PciConfigInfoEmpty);
  Repo->CmObjectCount = sizeof (PciConfigInfoEmpty) / sizeof (CM_ARM_PCI_CONFIG_SPACE_INFO);
  Repo->CmObjectPtr = &PciConfigInfoEmpty;
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
  Repo->CmObjectSize = sizeof (ProcHierarchyInfo);
  Repo->CmObjectCount = ARRAY_SIZE (ProcHierarchyInfo);
  Repo->CmObjectPtr = &ProcHierarchyInfo;
  Repo++;

  ApplyConfigurationManagerOverrides ();

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

  Status = InitializePlatformRepository ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return gBS->InstallMultipleProtocolInterfaces (&ImageHandle,
                                                 &gNVIDIAConfigurationManagerDataProtocolGuid,
                                                 (VOID*)NVIDIAPlatformRepositoryInfo,
                                                 NULL);
}
