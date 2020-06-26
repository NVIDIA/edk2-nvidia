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
#include <T186/T186Definitions.h>

#include "Dsdt.hex"

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
  }
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
    GIC_ENTRY (CPUInterfaceNumber, Mpidr, PmuIrq, VGicIrq, EnergyEfficiency)
  */
  GICC_ENTRY (0, GET_MPID (0, 0), 0x140, 25,     0),
  GICC_ENTRY (1, GET_MPID (0, 1), 0x141, 25,     0),
  GICC_ENTRY (2, GET_MPID (1, 0), 0x128, 25,     0),
  GICC_ENTRY (3, GET_MPID (1, 1), 0x129, 25,     0),
  GICC_ENTRY (4, GET_MPID (1, 2), 0x12A, 25,     0),
  GICC_ENTRY (5, GET_MPID (1, 3), 0x12B, 25,     0)
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

// SPCR Serial Port
/** The platform SPCR serial port information.
*/
CM_ARM_SERIAL_PORT_INFO SpcrSerialPort = {
  FixedPcdGet64 (PcdTegra16550UartBaseT186),
  T186_UARTA_INTR,
  FixedPcdGet64 (PcdUartDefaultBaudRate),
  0,
  EFI_ACPI_DBG2_PORT_SUBTYPE_SERIAL_FULL_16550
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
  UINTN GicInterruptInterfaceBase;

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

  GicInterruptInterfaceBase = PcdGet64(PcdGicInterruptInterfaceBase);
  NVIDIAPlatformRepositoryInfo[4].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicCInfo);
  NVIDIAPlatformRepositoryInfo[4].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[4].CmObjectSize = sizeof (GicCInfo);
  NVIDIAPlatformRepositoryInfo[4].CmObjectCount = sizeof (GicCInfo) / sizeof (CM_ARM_GICC_INFO);
  NVIDIAPlatformRepositoryInfo[4].CmObjectPtr = &GicCInfo;
  for(Index=0; Index<NVIDIAPlatformRepositoryInfo[4].CmObjectCount; Index++) {
    GicCInfo[Index].PhysicalBaseAddress =  GicInterruptInterfaceBase;
  }

  GicDInfo.PhysicalBaseAddress = PcdGet64 (PcdGicDistributorBase);
  NVIDIAPlatformRepositoryInfo[5].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicDInfo);
  NVIDIAPlatformRepositoryInfo[5].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[5].CmObjectSize = sizeof (GicDInfo);
  NVIDIAPlatformRepositoryInfo[5].CmObjectCount = sizeof (GicDInfo) / sizeof (CM_ARM_GICD_INFO);
  NVIDIAPlatformRepositoryInfo[5].CmObjectPtr = &GicDInfo;

  NVIDIAPlatformRepositoryInfo[6].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGenericTimerInfo);
  NVIDIAPlatformRepositoryInfo[6].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[6].CmObjectSize = sizeof (GenericTimerInfo);
  NVIDIAPlatformRepositoryInfo[6].CmObjectCount = sizeof (GenericTimerInfo) / sizeof (CM_ARM_GENERIC_TIMER_INFO);
  NVIDIAPlatformRepositoryInfo[6].CmObjectPtr = &GenericTimerInfo;

  NVIDIAPlatformRepositoryInfo[7].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjSerialConsolePortInfo);
  NVIDIAPlatformRepositoryInfo[7].CmObjectToken = CM_NULL_TOKEN;
  NVIDIAPlatformRepositoryInfo[7].CmObjectSize = sizeof (SpcrSerialPort);
  NVIDIAPlatformRepositoryInfo[7].CmObjectCount = sizeof (SpcrSerialPort) / sizeof (CM_ARM_SERIAL_PORT_INFO);
  NVIDIAPlatformRepositoryInfo[7].CmObjectPtr = &SpcrSerialPort;

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
  if (ChipID != T186_CHIP_ID) {
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
