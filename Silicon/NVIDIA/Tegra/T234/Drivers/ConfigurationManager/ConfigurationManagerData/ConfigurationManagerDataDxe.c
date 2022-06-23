/** @file
  Configuration Manager Data Dxe

  Copyright (c) 2019 - 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include "SsdtPci.hex"
#include "SsdtPci.offset.h"
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
  (EFI_ACPI_DESCRIPTION_HEADER *)ssdtpci_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)sdctemplate_aml_code
};

STATIC AML_OFFSET_TABLE_ENTRY  *OffsetTableArray[] = {
  DSDT_TEGRA234_OffsetTable,
  SSDT_TEGRA234_OffsetTable,
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
  // MCFG Table
  {
    EFI_ACPI_6_4_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMcfg),
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
  // SSDT Table
  {
    EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt),
    (EFI_ACPI_DESCRIPTION_HEADER *)ssdtpci_aml_code,
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

/** The platform GIC CPU interface information.
*/
STATIC
CM_ARM_GICC_INFO  GicCInfo[] = {
  /*
    GICC_ENTRY (CPUInterfaceNumber, Mpidr, PmuIrq, VGicIrq, EnergyEfficiency, ProximityDomain)
  */
  GICC_ENTRY (0,  GET_MPID (0, 0), 23, 25, 0, 0),
  GICC_ENTRY (1,  GET_MPID (0, 1), 23, 25, 0, 0),
  GICC_ENTRY (2,  GET_MPID (0, 2), 23, 25, 0, 0),
  GICC_ENTRY (3,  GET_MPID (0, 3), 23, 25, 0, 0),
  GICC_ENTRY (4,  GET_MPID (1, 0), 23, 25, 0, 0),
  GICC_ENTRY (5,  GET_MPID (1, 1), 23, 25, 0, 0),
  GICC_ENTRY (6,  GET_MPID (1, 2), 23, 25, 0, 0),
  GICC_ENTRY (7,  GET_MPID (1, 3), 23, 25, 0, 0),
  GICC_ENTRY (8,  GET_MPID (2, 0), 23, 25, 0, 0),
  GICC_ENTRY (9,  GET_MPID (2, 1), 23, 25, 0, 0),
  GICC_ENTRY (10, GET_MPID (2, 2), 23, 25, 0, 0),
  GICC_ENTRY (11, GET_MPID (2, 3), 23, 25, 0, 0),
};

/** The platform GIC distributor information.
*/
STATIC
CM_ARM_GICD_INFO  GicDInfo = {
  0,
  0,
  3
};

/** The platform GIC redistributor information.
*/
STATIC
CM_ARM_GIC_REDIST_INFO  GicRedistInfo = {
  0,
  (ARM_GICR_CTLR_FRAME_SIZE + ARM_GICR_SGI_PPI_FRAME_SIZE) * T234_GIC_REDISTRIBUTOR_INSTANCES
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

/** PCI Configuration Space Info
*/
STATIC
CM_ARM_PCI_CONFIG_SPACE_INFO  PciConfigInfo[] = {
  {
    // The physical base address for the PCI segment
    T234_PCIE_C1_CFG_BASE_ADDR,
    // The PCI segment group number
    1,
    // The start bus number
    T234_PCIE_BUS_MIN,
    // The end bus number
    T234_PCIE_BUS_MAX
  },

  {
    // The physical base address for the PCI segment
    T234_PCIE_C4_CFG_BASE_ADDR,
    // The PCI segment group number
    4,
    // The start bus number
    T234_PCIE_BUS_MIN,
    // The end bus number
    T234_PCIE_BUS_MAX
  },

  {
    // The physical base address for the PCI segment
    T234_PCIE_C5_CFG_BASE_ADDR,
    // The PCI segment group number
    5,
    // The start bus number
    T234_PCIE_BUS_MIN,
    // The end bus number
    T234_PCIE_BUS_MAX
  }
};

/** Cache Info
 */
STATIC
CM_ARM_CACHE_INFO  CacheInfo[] = {
  // L4 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[0]),
    .NextLevelOfCacheToken = CM_NULL_TOKEN,
    .Size                  = 0x400000,
    .NumberOfSets          = 4096,
    .Associativity         = 16,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 1,
  },
  // L3 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[1]),
    .NextLevelOfCacheToken = CM_NULL_TOKEN,
    .Size                  = 0x200000,
    .NumberOfSets          = 2048,
    .Associativity         = 16,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 2,
  },
  // L3 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[2]),
    .NextLevelOfCacheToken = CM_NULL_TOKEN,
    .Size                  = 0x200000,
    .NumberOfSets          = 2048,
    .Associativity         = 16,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 3,
  },
  // L3 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[3]),
    .NextLevelOfCacheToken = CM_NULL_TOKEN,
    .Size                  = 0x200000,
    .NumberOfSets          = 2048,
    .Associativity         = 16,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 4,
  },
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[4]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[1]),
    .Size                  = 0x40000,
    .NumberOfSets          = 512,
    .Associativity         = 8,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 5,
  },
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[5]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[4]),
    .Size                  = 0x40000,
    .NumberOfSets          = 512,
    .Associativity         = 8,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 6,
  },
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[6]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[4]),
    .Size                  = 0x40000,
    .NumberOfSets          = 512,
    .Associativity         = 8,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 7,
  },
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[7]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[4]),
    .Size                  = 0x40000,
    .NumberOfSets          = 512,
    .Associativity         = 8,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 8,
  },
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[8]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[2]),
    .Size                  = 0x40000,
    .NumberOfSets          = 512,
    .Associativity         = 8,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 9,
  },
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[9]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[2]),
    .Size                  = 0x40000,
    .NumberOfSets          = 512,
    .Associativity         = 8,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 10,
  },
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[10]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[2]),
    .Size                  = 0x40000,
    .NumberOfSets          = 512,
    .Associativity         = 8,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 11,
  },
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[11]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[2]),
    .Size                  = 0x40000,
    .NumberOfSets          = 512,
    .Associativity         = 8,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 12,
  },
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[12]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[3]),
    .Size                  = 0x40000,
    .NumberOfSets          = 512,
    .Associativity         = 8,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 13,
  },
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[13]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[3]),
    .Size                  = 0x40000,
    .NumberOfSets          = 512,
    .Associativity         = 8,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 14,
  },
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[14]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[3]),
    .Size                  = 0x40000,
    .NumberOfSets          = 512,
    .Associativity         = 8,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 15,
  },
  // L2 Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[15]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[3]),
    .Size                  = 0x40000,
    .NumberOfSets          = 512,
    .Associativity         = 8,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 16,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[16]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[4]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 17,
  },
  // L1D Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[17]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[4]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 18,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[18]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[5]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 19,
  },
  // L1D Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[19]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[5]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 20,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[20]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[6]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 21,
  },
  // L1D Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[21]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[6]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 22,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[22]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[7]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 23,
  },
  // L1D Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[23]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[7]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 24,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[24]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[8]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 25,
  },
  // L1D Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[25]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[8]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 26,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[26]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[9]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 27,
  },
  // L1D Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[27]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[9]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 28,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[28]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[10]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 29,
  },
  // L1D Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[29]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[10]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 30,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[30]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[11]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 31,
  },
  // L1D Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[31]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[11]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 32,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[32]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[12]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 33,
  },
  // L1D Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[33]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[12]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 34,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[34]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[13]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 35,
  },
  // L1D Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[35]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[13]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 36,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[36]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[14]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 37,
  },
  // L1D Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[37]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[14]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 38,
  },
  // L1I Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[38]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[15]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 39,
  },
  // L1D Cache Info
  {
    .Token                 = REFERENCE_TOKEN (CacheInfo[39]),
    .NextLevelOfCacheToken = REFERENCE_TOKEN (CacheInfo[15]),
    .Size                  = 0x10000,
    .NumberOfSets          = 256,
    .Associativity         = 4,
    .Attributes            = CACHE_ATTRIBUTES (
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,
                               EFI_ACPI_6_4_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK
                               ),
    .LineSize = 64,
    .CacheId  = 40,
  },
};

/** CCPLEX Resources
 */
STATIC
CM_ARM_OBJ_REF  CcplexResources[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[0]) },
};

/** Hercules Core Cluster Resources
 */
STATIC
CM_ARM_OBJ_REF  HerculesCoreClusterResources0[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[1]) },
};

STATIC
CM_ARM_OBJ_REF  HerculesCoreClusterResources1[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[2]) },
};

STATIC
CM_ARM_OBJ_REF  HerculesCoreClusterResources2[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[3]) },
};

/** Hercules Core Resources
 */
STATIC
CM_ARM_OBJ_REF  HerculesCoreResources00[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[4])  },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[16]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[17]) },
};

STATIC
CM_ARM_OBJ_REF  HerculesCoreResources01[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[5])  },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[18]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[19]) },
};

STATIC
CM_ARM_OBJ_REF  HerculesCoreResources02[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[6])  },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[20]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[21]) },
};

STATIC
CM_ARM_OBJ_REF  HerculesCoreResources03[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[7])  },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[22]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[23]) },
};

STATIC
CM_ARM_OBJ_REF  HerculesCoreResources10[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[8])  },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[24]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[25]) },
};

STATIC
CM_ARM_OBJ_REF  HerculesCoreResources11[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[9])  },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[26]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[27]) },
};

STATIC
CM_ARM_OBJ_REF  HerculesCoreResources12[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[10]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[28]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[29]) },
};

STATIC
CM_ARM_OBJ_REF  HerculesCoreResources13[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[11]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[30]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[31]) },
};

STATIC
CM_ARM_OBJ_REF  HerculesCoreResources20[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[12]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[32]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[33]) },
};

STATIC
CM_ARM_OBJ_REF  HerculesCoreResources21[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[13]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[34]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[35]) },
};

STATIC
CM_ARM_OBJ_REF  HerculesCoreResources22[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[14]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[36]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[37]) },
};

STATIC
CM_ARM_OBJ_REF  HerculesCoreResources23[] = {
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[15]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[38]) },
  { .ReferenceToken = REFERENCE_TOKEN (CacheInfo[39]) },
};

/** Processor Hierarchy Info
 */
STATIC
CM_ARM_PROC_HIERARCHY_INFO  ProcHierarchyInfo[] = {
  // CCPLEX
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[0]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_INVALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_NOT_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_IDENTICAL
               ),
    .ParentToken                = CM_NULL_TOKEN,
    .GicCToken                  = CM_NULL_TOKEN,
    .NoOfPrivateResources       = 1,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (CcplexResources),
  },
  // Hercules Core Clusters
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[1]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_INVALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_NOT_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[0]),
    .GicCToken                  = CM_NULL_TOKEN,
    .NoOfPrivateResources       = 1,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreClusterResources0),
  },
  // Hercules Core Clusters
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[2]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_INVALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_NOT_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[0]),
    .GicCToken                  = CM_NULL_TOKEN,
    .NoOfPrivateResources       = 1,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreClusterResources1),
  },
  // Hercules Core Clusters
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[3]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_INVALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_NOT_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[0]),
    .GicCToken                  = CM_NULL_TOKEN,
    .NoOfPrivateResources       = 1,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreClusterResources2),
  },
  // Hercules Cores
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[4]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[1]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[0]),
    .NoOfPrivateResources       = 3,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreResources00),
  },
  // Hercules Cores
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[5]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[1]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[1]),
    .NoOfPrivateResources       = 3,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreResources01),
  },
  // Hercules Cores
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[6]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[1]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[2]),
    .NoOfPrivateResources       = 3,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreResources02),
  },
  // Hercules Cores
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[7]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[1]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[3]),
    .NoOfPrivateResources       = 3,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreResources03),
  },
  // Hercules Cores
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[8]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[2]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[4]),
    .NoOfPrivateResources       = 3,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreResources10),
  },
  // Hercules Cores
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[9]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[2]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[5]),
    .NoOfPrivateResources       = 3,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreResources11),
  },
  // Hercules Cores
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[10]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[2]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[6]),
    .NoOfPrivateResources       = 3,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreResources12),
  },
  // Hercules Cores
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[11]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[2]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[7]),
    .NoOfPrivateResources       = 3,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreResources13),
  },
  // Hercules Cores
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[12]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[3]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[8]),
    .NoOfPrivateResources       = 3,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreResources20),
  },
  // Hercules Cores
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[13]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[3]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[9]),
    .NoOfPrivateResources       = 3,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreResources21),
  },
  // Hercules Cores
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[14]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[3]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[10]),
    .NoOfPrivateResources       = 3,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreResources22),
  },
  // Hercules Cores
  {
    .Token = REFERENCE_TOKEN (ProcHierarchyInfo[13]),
    .Flags = PROC_NODE_FLAGS (
               EFI_ACPI_6_4_PPTT_PACKAGE_NOT_PHYSICAL,
               EFI_ACPI_6_4_PPTT_PROCESSOR_ID_VALID,
               EFI_ACPI_6_4_PPTT_PROCESSOR_IS_NOT_THREAD,
               EFI_ACPI_6_4_PPTT_NODE_IS_LEAF,
               EFI_ACPI_6_4_PPTT_IMPLEMENTATION_NOT_IDENTICAL
               ),
    .ParentToken                = REFERENCE_TOKEN (ProcHierarchyInfo[3]),
    .GicCToken                  = REFERENCE_TOKEN (GicCInfo[11]),
    .NoOfPrivateResources       = 3,
    .PrivateResourcesArrayToken = REFERENCE_TOKEN (HerculesCoreResources23),
  },
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

    AsciiSPrint (SdcPathString, sizeof (SdcPathString), "SDC%d", Index);
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

/** Initialize the cpu entries in the platform configuration repository.
 *
 * @param Repo Pointer to a repo structure that will be added to and updated with the data updated
 *
  @retval EFI_SUCCESS   Success
**/
STATIC
EFI_STATUS
EFIAPI
UpdateCpuInfo (
  EDKII_PLATFORM_REPOSITORY_INFO  **PlatformRepositoryInfo
  )
{
  EFI_STATUS                      Status;
  UINT32                          NumCpus;
  UINT32                          Index;
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  UINT32                          *CpuIdleHandles;
  UINT32                          NumberOfCpuIdles;
  UINT32                          NumberOfLpiStates;
  CM_OBJECT_TOKEN                 LpiToken;
  CM_OBJECT_TOKEN                 *LpiTokenMap;
  CM_ARM_LPI_INFO                 *LpiInfo;
  VOID                            *DeviceTreeBase;
  INT32                           NodeOffset;
  CONST VOID                      *Property;
  INT32                           PropertyLen;
  UINT32                          WakeupLatencyUs;

  Repo = *PlatformRepositoryInfo;

  NumCpus = GetNumberOfEnabledCpuCores ();

  // Build LPI stuctures
  NumberOfCpuIdles = 0;

  Status = GetMatchingEnabledDeviceTreeNodes ("arm,idle-state", NULL, &NumberOfCpuIdles);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    NumberOfCpuIdles = 0;
  } else {
    CpuIdleHandles = AllocateZeroPool (sizeof (UINT32) * NumberOfCpuIdles);
    if (CpuIdleHandles == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate array for cpuidle cores\r\n", __FUNCTION__));
      return EFI_OUT_OF_RESOURCES;
    }

    Status = GetMatchingEnabledDeviceTreeNodes ("arm,idle-state", CpuIdleHandles, &NumberOfCpuIdles);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get cpuidle cores %r\r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  // 1 extra for WFI state
  LpiTokenMap = AllocateZeroPool (sizeof (CM_OBJECT_TOKEN) * (NumberOfCpuIdles + 1));
  if (LpiTokenMap == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate array for lpi token map\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  LpiToken = REFERENCE_TOKEN (LpiTokenMap);

  LpiInfo = AllocateZeroPool (sizeof (CM_ARM_LPI_INFO) * (NumberOfCpuIdles + 1));
  if (LpiInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate array for lpi info\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  for (Index = 0; Index <= NumberOfCpuIdles; Index++) {
    LpiTokenMap[Index] = REFERENCE_TOKEN (LpiInfo[Index]);
  }

  NumberOfLpiStates = 0;

  // Create WFI entry
  LpiInfo[NumberOfLpiStates].MinResidency                          = 1;
  LpiInfo[NumberOfLpiStates].WorstCaseWakeLatency                  = 1;
  LpiInfo[NumberOfLpiStates].Flags                                 = 1;
  LpiInfo[NumberOfLpiStates].ArchFlags                             = 0;
  LpiInfo[NumberOfLpiStates].EnableParentState                     = FALSE;
  LpiInfo[NumberOfLpiStates].IsInteger                             = FALSE;
  LpiInfo[NumberOfLpiStates].RegisterEntryMethod.AccessSize        = 3;
  LpiInfo[NumberOfLpiStates].RegisterEntryMethod.Address           = 0xFFFFFFFF;
  LpiInfo[NumberOfLpiStates].RegisterEntryMethod.AddressSpaceId    = EFI_ACPI_6_3_FUNCTIONAL_FIXED_HARDWARE;
  LpiInfo[NumberOfLpiStates].RegisterEntryMethod.RegisterBitOffset = 0;
  LpiInfo[NumberOfLpiStates].RegisterEntryMethod.RegisterBitWidth  = 0x20;
  CopyMem (LpiInfo[NumberOfLpiStates].StateName, "WFI", sizeof ("WFI"));

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjLpiInfo);
  Repo->CmObjectToken = REFERENCE_TOKEN (LpiInfo[NumberOfLpiStates]);
  Repo->CmObjectSize  = sizeof (CM_ARM_LPI_INFO);
  Repo->CmObjectCount = 1;
  Repo->CmObjectPtr   = &LpiInfo[NumberOfLpiStates];
  Repo++;
  NumberOfLpiStates++;

  for (Index = 0; Index < NumberOfCpuIdles; Index++) {
    Status = GetDeviceTreeNode (CpuIdleHandles[Index], &DeviceTreeBase, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to get idle state node - %r\r\n", Status));
      continue;
    }

    Property = fdt_getprop (DeviceTreeBase, NodeOffset, "arm,psci-suspend-param", NULL);
    if (Property == NULL) {
      DEBUG ((DEBUG_ERROR, "Failed to get psci-suspend-param\r\n"));
      continue;
    }

    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.Address = SwapBytes32 (*(CONST UINT32 *)Property);

    Property = fdt_getprop (DeviceTreeBase, NodeOffset, "min-residency-us", NULL);
    if (Property == NULL) {
      DEBUG ((DEBUG_ERROR, "Failed to get min-residency-us\r\n"));
      continue;
    }

    LpiInfo[NumberOfLpiStates].MinResidency = SwapBytes32 (*(CONST UINT32 *)Property);

    Property = fdt_getprop (DeviceTreeBase, NodeOffset, "wakeup-latency-us", NULL);
    if (Property == NULL) {
      Property = fdt_getprop (DeviceTreeBase, NodeOffset, "entry-latency-us", NULL);
      if (Property == NULL) {
        DEBUG ((DEBUG_ERROR, "Failed to get entry-latency-us\r\n"));
        continue;
      }

      WakeupLatencyUs = SwapBytes32 (*(CONST UINT32 *)Property);
      Property        = fdt_getprop (DeviceTreeBase, NodeOffset, "exit-latency-us", NULL);
      if (Property == NULL) {
        DEBUG ((DEBUG_ERROR, "Failed to get exit-latency-us\r\n"));
        continue;
      }

      WakeupLatencyUs += SwapBytes32 (*(CONST UINT32 *)Property);
    } else {
      WakeupLatencyUs = SwapBytes32 (*(CONST UINT32 *)Property);
    }

    LpiInfo[NumberOfLpiStates].WorstCaseWakeLatency = WakeupLatencyUs;

    LpiInfo[NumberOfLpiStates].Flags                                 = 1;
    LpiInfo[NumberOfLpiStates].ArchFlags                             = 1;
    LpiInfo[NumberOfLpiStates].EnableParentState                     = TRUE;
    LpiInfo[NumberOfLpiStates].IsInteger                             = FALSE;
    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.AccessSize        = 3;
    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.AddressSpaceId    = EFI_ACPI_6_3_FUNCTIONAL_FIXED_HARDWARE;
    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.RegisterBitOffset = 0;
    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.RegisterBitWidth  = 0x20;
    Property                                                         = fdt_getprop (DeviceTreeBase, NodeOffset, "idle-state-name", &PropertyLen);
    if (Property != NULL) {
      CopyMem (LpiInfo[NumberOfLpiStates].StateName, Property, PropertyLen);
    }

    Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjLpiInfo);
    Repo->CmObjectToken = REFERENCE_TOKEN (LpiInfo[NumberOfLpiStates]);
    Repo->CmObjectSize  = sizeof (CM_ARM_LPI_INFO);
    Repo->CmObjectCount = 1;
    Repo->CmObjectPtr   = &LpiInfo[NumberOfLpiStates];
    Repo++;

    NumberOfLpiStates++;
  }

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (LpiTokenMap);
  Repo->CmObjectSize  = sizeof (CM_OBJECT_TOKEN) * NumberOfLpiStates;
  Repo->CmObjectCount = NumberOfLpiStates;
  Repo->CmObjectPtr   = LpiTokenMap;
  Repo++;

  // Populate the Core nodes in Proc Hierarchy with Lpi Token
  for (Index = CORE_BEGIN_INDEX;
       Index < CORE_BEGIN_INDEX + NumCpus;
       Index++)
  {
    ProcHierarchyInfo[Index].LpiToken = LpiToken;
  }

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCacheInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (CacheInfo);
  Repo->CmObjectCount = ARRAY_SIZE (CacheInfo);
  Repo->CmObjectPtr   = &CacheInfo;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (CcplexResources);
  Repo->CmObjectSize  = sizeof (CcplexResources);
  Repo->CmObjectCount = ARRAY_SIZE (CcplexResources);
  Repo->CmObjectPtr   = &CcplexResources;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreClusterResources0);
  Repo->CmObjectSize  = sizeof (HerculesCoreClusterResources0);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreClusterResources0);
  Repo->CmObjectPtr   = &HerculesCoreClusterResources0;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreClusterResources1);
  Repo->CmObjectSize  = sizeof (HerculesCoreClusterResources1);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreClusterResources1);
  Repo->CmObjectPtr   = &HerculesCoreClusterResources1;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreClusterResources2);
  Repo->CmObjectSize  = sizeof (HerculesCoreClusterResources2);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreClusterResources2);
  Repo->CmObjectPtr   = &HerculesCoreClusterResources2;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreResources00);
  Repo->CmObjectSize  = sizeof (HerculesCoreResources00);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreResources00);
  Repo->CmObjectPtr   = &HerculesCoreResources00;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreResources01);
  Repo->CmObjectSize  = sizeof (HerculesCoreResources01);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreResources01);
  Repo->CmObjectPtr   = &HerculesCoreResources01;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreResources02);
  Repo->CmObjectSize  = sizeof (HerculesCoreResources02);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreResources02);
  Repo->CmObjectPtr   = &HerculesCoreResources02;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreResources03);
  Repo->CmObjectSize  = sizeof (HerculesCoreResources03);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreResources03);
  Repo->CmObjectPtr   = &HerculesCoreResources03;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreResources10);
  Repo->CmObjectSize  = sizeof (HerculesCoreResources10);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreResources10);
  Repo->CmObjectPtr   = &HerculesCoreResources10;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreResources11);
  Repo->CmObjectSize  = sizeof (HerculesCoreResources11);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreResources11);
  Repo->CmObjectPtr   = &HerculesCoreResources11;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreResources12);
  Repo->CmObjectSize  = sizeof (HerculesCoreResources12);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreResources12);
  Repo->CmObjectPtr   = &HerculesCoreResources12;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreResources13);
  Repo->CmObjectSize  = sizeof (HerculesCoreResources13);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreResources13);
  Repo->CmObjectPtr   = &HerculesCoreResources13;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreResources20);
  Repo->CmObjectSize  = sizeof (HerculesCoreResources20);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreResources20);
  Repo->CmObjectPtr   = &HerculesCoreResources20;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreResources21);
  Repo->CmObjectSize  = sizeof (HerculesCoreResources21);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreResources21);
  Repo->CmObjectPtr   = &HerculesCoreResources21;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreResources22);
  Repo->CmObjectSize  = sizeof (HerculesCoreResources22);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreResources22);
  Repo->CmObjectPtr   = &HerculesCoreResources22;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjCmRef);
  Repo->CmObjectToken = REFERENCE_TOKEN (HerculesCoreResources23);
  Repo->CmObjectSize  = sizeof (HerculesCoreResources23);
  Repo->CmObjectCount = ARRAY_SIZE (HerculesCoreResources23);
  Repo->CmObjectPtr   = &HerculesCoreResources23;
  Repo++;

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjProcHierarchyInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (ProcHierarchyInfo);
  Repo->CmObjectCount = ARRAY_SIZE (ProcHierarchyInfo);
  Repo->CmObjectPtr   = &ProcHierarchyInfo;
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

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjGicCInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (GicCInfo);
  Repo->CmObjectCount = sizeof (GicCInfo) / sizeof (CM_ARM_GICC_INFO);
  Repo->CmObjectPtr   = &GicCInfo;
  Repo++;

  GicDInfo.PhysicalBaseAddress = PcdGet64 (PcdGicDistributorBase);
  Repo->CmObjectId             = CREATE_CM_ARM_OBJECT_ID (EArmObjGicDInfo);
  Repo->CmObjectToken          = CM_NULL_TOKEN;
  Repo->CmObjectSize           = sizeof (GicDInfo);
  Repo->CmObjectCount          = sizeof (GicDInfo) / sizeof (CM_ARM_GICD_INFO);
  Repo->CmObjectPtr            = &GicDInfo;
  Repo++;

  GicRedistInfo.DiscoveryRangeBaseAddress = PcdGet64 (PcdGicRedistributorsBase);
  Repo->CmObjectId                        = CREATE_CM_ARM_OBJECT_ID (EArmObjGicRedistributorInfo);
  Repo->CmObjectToken                     = CM_NULL_TOKEN;
  Repo->CmObjectSize                      = sizeof (GicRedistInfo);
  Repo->CmObjectCount                     = sizeof (GicRedistInfo) / sizeof (CM_ARM_GIC_REDIST_INFO);
  Repo->CmObjectPtr                       = &GicRedistInfo;
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

  Status = UpdateCpuInfo (&Repo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Repo->CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjPciConfigSpaceInfo);
  Repo->CmObjectToken = CM_NULL_TOKEN;
  Repo->CmObjectSize  = sizeof (PciConfigInfo);
  Repo->CmObjectCount = sizeof (PciConfigInfo) / sizeof (CM_ARM_PCI_CONFIG_SPACE_INFO);
  Repo->CmObjectPtr   = &PciConfigInfo;
  Repo++;

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
