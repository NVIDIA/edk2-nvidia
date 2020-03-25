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
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <IndustryStandard/DebugPort2Table.h>
#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>

#include <Protocol/ConfigurationManagerDataProtocol.h>

#include "Platform.h"

#include "Dsdt.hex"
#include <SsdtPci.hex>

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
    EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_2_FIXED_ACPI_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdFadt),
    NULL,
    FixedPcdGet64(PcdAcpiDefaultOemTableId),
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // GTDT Table
  {
    EFI_ACPI_6_2_GENERIC_TIMER_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_2_GENERIC_TIMER_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdGtdt),
    NULL,
    FixedPcdGet64(PcdAcpiDefaultOemTableId),
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // MADT Table
  {
    EFI_ACPI_6_2_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_6_2_MULTIPLE_APIC_DESCRIPTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMadt),
    NULL,
    FixedPcdGet64(PcdAcpiDefaultOemTableId),
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // SPCR Table
  {
    EFI_ACPI_6_2_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE,
    EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSpcr),
    NULL,
    FixedPcdGet64(PcdAcpiTegraUartOemTableId),
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // DSDT Table
  {
    EFI_ACPI_6_2_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
    0,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDsdt),
    (EFI_ACPI_DESCRIPTION_HEADER*)dsdt_aml_code,
    FixedPcdGet64(PcdAcpiDefaultOemTableId),
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // SSDT table describing the PCI root complex
  {
    EFI_ACPI_6_2_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
    0,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt),
    (EFI_ACPI_DESCRIPTION_HEADER*)ssdtpci_aml_code,
    FixedPcdGet64(PcdAcpiDefaultOemTableId),
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // PCI MCFG Table
  {
    EFI_ACPI_6_2_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE,
    EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_REVISION,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMcfg),
    NULL,
    FixedPcdGet64(PcdAcpiDefaultOemTableId),
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
};

/** The platform boot architecture information.
*/
STATIC
CM_ARM_BOOT_ARCH_INFO BootArchInfo = {
  EFI_ACPI_6_2_ARM_PSCI_COMPLIANT
};

/** The platform power management profile information.
*/
STATIC
CM_ARM_POWER_MANAGEMENT_PROFILE_INFO PmProfileInfo = {
  EFI_ACPI_6_2_PM_PROFILE_ENTERPRISE_SERVER
};

/** The platform GIC CPU interface information.
*/
STATIC
CM_ARM_GICC_INFO GicCInfo[] = {
  /*
    GIC_ENTRY (CPUInterfaceNumber, Mpidr, PmuIrq, VGicIrq, EnergyEfficiency)
  */
  GICC_ENTRY (0, GET_MPID (0, 0), 0x140, 0,     0),
  /*GICC_ENTRY (1, GET_MPID (0, 1), 0x141, 0,     0),
  GICC_ENTRY (2, GET_MPID (1, 0), 0,     0x128, 0),
  GICC_ENTRY (3, GET_MPID (1, 1), 0x129, 0,     0),
  GICC_ENTRY (4, GET_MPID (1, 2), 0x12A, 0,     0),
  GICC_ENTRY (5, GET_MPID (1, 3), 0x12B, 0,     0)*/
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
  SIZE_64KB * 3
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
STATIC
CM_ARM_SERIAL_PORT_INFO SpcrSerialPort = {
  FixedPcdGet64 (PcdTegra16550UartBaseTH500),
  0x90,
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
    FixedPcdGet64 (PcdPciConfigurationSpaceBaseAddress),
    // The PCI segment group number
    0,
    // The start bus number
    FixedPcdGet32 (PcdPciBusMin),
    // The end bus number
    FixedPcdGet32 (PcdPciBusMax),
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
  NVIDIAPlatformRepositoryInfo[0].CmObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjCfgMgrInfo);
  NVIDIAPlatformRepositoryInfo[0].CmObjectSize = sizeof (CmInfo);
  NVIDIAPlatformRepositoryInfo[0].CmObjectCount = sizeof (CmInfo) / sizeof (CM_STD_OBJ_CONFIGURATION_MANAGER_INFO);
  NVIDIAPlatformRepositoryInfo[0].CmObjectPtr = &CmInfo;

  NVIDIAPlatformRepositoryInfo[1].CmObjectId = CREATE_CM_STD_OBJECT_ID (EStdObjAcpiTableList);
  NVIDIAPlatformRepositoryInfo[1].CmObjectSize = sizeof (CmAcpiTableList);
  NVIDIAPlatformRepositoryInfo[1].CmObjectCount = sizeof (CmAcpiTableList) / sizeof (CM_STD_OBJ_ACPI_TABLE_INFO);
  NVIDIAPlatformRepositoryInfo[1].CmObjectPtr = &CmAcpiTableList;

  NVIDIAPlatformRepositoryInfo[2].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjBootArchInfo);
  NVIDIAPlatformRepositoryInfo[2].CmObjectSize = sizeof (BootArchInfo);
  NVIDIAPlatformRepositoryInfo[2].CmObjectCount = sizeof (BootArchInfo) / sizeof (CM_ARM_BOOT_ARCH_INFO);
  NVIDIAPlatformRepositoryInfo[2].CmObjectPtr = &BootArchInfo;

  NVIDIAPlatformRepositoryInfo[3].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjPowerManagementProfileInfo);
  NVIDIAPlatformRepositoryInfo[3].CmObjectSize = sizeof (PmProfileInfo);
  NVIDIAPlatformRepositoryInfo[3].CmObjectCount = sizeof (PmProfileInfo) / sizeof (CM_ARM_POWER_MANAGEMENT_PROFILE_INFO);
  NVIDIAPlatformRepositoryInfo[3].CmObjectPtr = &PmProfileInfo;

  NVIDIAPlatformRepositoryInfo[4].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicCInfo);
  NVIDIAPlatformRepositoryInfo[4].CmObjectSize = sizeof (GicCInfo);
  NVIDIAPlatformRepositoryInfo[4].CmObjectCount = sizeof (GicCInfo) / sizeof (CM_ARM_GICC_INFO);
  NVIDIAPlatformRepositoryInfo[4].CmObjectPtr = &GicCInfo;

  GicDInfo.PhysicalBaseAddress = PcdGet64 (PcdGicDistributorBase);
  NVIDIAPlatformRepositoryInfo[5].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicDInfo);
  NVIDIAPlatformRepositoryInfo[5].CmObjectSize = sizeof (GicDInfo);
  NVIDIAPlatformRepositoryInfo[5].CmObjectCount = sizeof (GicDInfo) / sizeof (CM_ARM_GICD_INFO);
  NVIDIAPlatformRepositoryInfo[5].CmObjectPtr = &GicDInfo;

  GicRedistInfo.DiscoveryRangeBaseAddress = PcdGet64 (PcdGicRedistributorsBase);
  NVIDIAPlatformRepositoryInfo[6].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicRedistributorInfo);
  NVIDIAPlatformRepositoryInfo[6].CmObjectSize = sizeof (GicRedistInfo);
  NVIDIAPlatformRepositoryInfo[6].CmObjectCount = sizeof (GicRedistInfo) / sizeof (CM_ARM_GIC_REDIST_INFO);
  NVIDIAPlatformRepositoryInfo[6].CmObjectPtr = &GicRedistInfo;

  NVIDIAPlatformRepositoryInfo[7].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGenericTimerInfo);
  NVIDIAPlatformRepositoryInfo[7].CmObjectSize = sizeof (GenericTimerInfo);
  NVIDIAPlatformRepositoryInfo[7].CmObjectCount = sizeof (GenericTimerInfo) / sizeof (CM_ARM_GENERIC_TIMER_INFO);
  NVIDIAPlatformRepositoryInfo[7].CmObjectPtr = &GenericTimerInfo;

  NVIDIAPlatformRepositoryInfo[8].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjSerialConsolePortInfo);
  NVIDIAPlatformRepositoryInfo[8].CmObjectSize = sizeof (SpcrSerialPort);
  NVIDIAPlatformRepositoryInfo[8].CmObjectCount = sizeof (SpcrSerialPort) / sizeof (CM_ARM_SERIAL_PORT_INFO);
  NVIDIAPlatformRepositoryInfo[8].CmObjectPtr = &SpcrSerialPort;

  NVIDIAPlatformRepositoryInfo[9].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjPciConfigSpaceInfo);
  NVIDIAPlatformRepositoryInfo[9].CmObjectSize = sizeof (PciConfigInfo);
  NVIDIAPlatformRepositoryInfo[9].CmObjectCount = sizeof (PciConfigInfo) / sizeof (CM_ARM_PCI_CONFIG_SPACE_INFO);
  NVIDIAPlatformRepositoryInfo[9].CmObjectPtr = PciConfigInfo;

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
  if (ChipID != TH500_CHIP_ID) {
    return EFI_SUCCESS;
  }

  Status = InitializePlatformRepository ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return gBS->InstallProtocolInterface (&ImageHandle,
                                        &gNVIDIAConfigurationManagerDataProtocolGuid,
                                        EFI_NATIVE_INTERFACE,
                                        (VOID*)NVIDIAPlatformRepositoryInfo);
}
