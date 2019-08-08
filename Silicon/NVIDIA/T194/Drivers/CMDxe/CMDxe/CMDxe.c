/** @file
  Configuration Manager Dxe

  Copyright (c) 2019, NVIDIA Corporation. All rights reserved.
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

#include "CMDxe.h"

#include <IndustryStandard/DebugPort2Table.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/ArmLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/AcpiTable.h>

#include <ArmPlatform.h>
#include <AcpiTableGenerator.h>
#include <Protocol/ConfigurationManagerProtocol.h>

#include <Dsdt.hex>

#include "../CMDxe/Platform.h"


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
    2,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSpcr),
    NULL,
    FixedPcdGet64(PcdAcpiTegraUartOemTableId),
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  },
  // DSDT Table
  {
    EFI_ACPI_6_2_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
    2,
    CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDsdt),
    (EFI_ACPI_DESCRIPTION_HEADER*)dsdt_aml_code,
    FixedPcdGet64(PcdAcpiDefaultOemTableId),
    FixedPcdGet64(PcdAcpiDefaultOemRevision)
  }
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
  GICC_ENTRY (1, GET_MPID (0, 1), 0x141, 0,     0),
  GICC_ENTRY (2, GET_MPID (1, 0), 0,     0x128, 0),
  GICC_ENTRY (3, GET_MPID (1, 1), 0x129, 0,     0),
  GICC_ENTRY (4, GET_MPID (1, 2), 0x12A, 0,     0),
  GICC_ENTRY (5, GET_MPID (1, 3), 0x12B, 0,     0)
};

/** The platform GIC distributor information.
*/
STATIC
CM_ARM_GICD_INFO GicDInfo = {
  FixedPcdGet64 (PcdGicDistributorBase),
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
  FixedPcdGet64 (PcdSerialRegisterBase),
  0x72,
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
  ZeroMem(NVIDIAPlatformRepositoryInfo, sizeof (NVIDIAPlatformRepositoryInfo));

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

  NVIDIAPlatformRepositoryInfo[5].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGicDInfo);
  NVIDIAPlatformRepositoryInfo[5].CmObjectSize = sizeof (GicDInfo);
  NVIDIAPlatformRepositoryInfo[5].CmObjectCount = sizeof (GicDInfo) / sizeof (CM_ARM_GICD_INFO);
  NVIDIAPlatformRepositoryInfo[5].CmObjectPtr = &GicDInfo;

  NVIDIAPlatformRepositoryInfo[6].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjGenericTimerInfo);
  NVIDIAPlatformRepositoryInfo[6].CmObjectSize = sizeof (GenericTimerInfo);
  NVIDIAPlatformRepositoryInfo[6].CmObjectCount = sizeof (GenericTimerInfo) / sizeof (CM_ARM_GENERIC_TIMER_INFO);
  NVIDIAPlatformRepositoryInfo[6].CmObjectPtr = &GenericTimerInfo;

  NVIDIAPlatformRepositoryInfo[7].CmObjectId = CREATE_CM_ARM_OBJECT_ID (EArmObjSerialConsolePortInfo);
  NVIDIAPlatformRepositoryInfo[7].CmObjectSize = sizeof (SpcrSerialPort);
  NVIDIAPlatformRepositoryInfo[7].CmObjectCount = sizeof (SpcrSerialPort) / sizeof (CM_ARM_SERIAL_PORT_INFO);
  NVIDIAPlatformRepositoryInfo[7].CmObjectPtr = &SpcrSerialPort;

  return EFI_SUCCESS;
}

/** The GetObject function defines the interface implemented by the
    Configuration Manager Protocol for returning the Configuration
    Manager Objects.

  @param [in]      This        Pointer to the Configuration Manager Protocol.
  @param [in]      CmObjectId  The Configuration Manager Object ID.
  @param [in]      Token       An optional token identifying the object. If
                               unused this must be CM_NULL_TOKEN.
  @param [in, out] CmObject    Pointer to the Configuration Manager Object
                               descriptor describing the requested Object.

  @retval EFI_SUCCESS           Success.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_NOT_FOUND         The required object information is not found.
**/
EFI_STATUS
EFIAPI
NVIDIAPlatformGetObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  * CONST This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     * CONST CmObject
  )
{
  EFI_STATUS  Status;
  UINT32      Index;
  BOOLEAN     DataFound;

  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  DataFound = FALSE;

  for (Index = 0; Index < EStdObjMax + EArmObjMax; Index++) {
    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr == NULL) {
      break;
    }

    if (NVIDIAPlatformRepositoryInfo[Index].CmObjectId == CmObjectId) {
      DataFound = TRUE;
      break;
    }
  }

  if (DataFound) {
    Status = EFI_SUCCESS;
    CmObject->Size = NVIDIAPlatformRepositoryInfo[Index].CmObjectSize;
    CmObject->Data = NVIDIAPlatformRepositoryInfo[Index].CmObjectPtr;
    CmObject->ObjectId = NVIDIAPlatformRepositoryInfo[Index].CmObjectId;
    CmObject->Count = NVIDIAPlatformRepositoryInfo[Index].CmObjectCount;
    DEBUG ((
      DEBUG_INFO,
      "CmObject: ID = %d, Ptr = 0x%p, Size = %d, Count = %d\n",
      CmObject->ObjectId,
      CmObject->Data,
      CmObject->Size,
      CmObject->Count
      ));
  } else {
    Status = EFI_NOT_FOUND;
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: Not Found CmObject = 0x%x. Status = %r\n",
      CmObjectId,
      Status
      ));
  }

  return Status;
}

/** The SetObject function defines the interface implemented by the
    Configuration Manager Protocol for updating the Configuration
    Manager Objects.

  @param [in]      This        Pointer to the Configuration Manager Protocol.
  @param [in]      CmObjectId  The Configuration Manager Object ID.
  @param [in]      Token       An optional token identifying the object. If
                               unused this must be CM_NULL_TOKEN.
  @param [in]      CmObject    Pointer to the Configuration Manager Object
                               descriptor describing the Object.

  @retval EFI_UNSUPPORTED  This operation is not supported.
**/
EFI_STATUS
EFIAPI
NVIDIAPlatformSetObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  * CONST This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN        CM_OBJ_DESCRIPTOR                     * CONST CmObject
  )
{
  return EFI_UNSUPPORTED;
}

/** A structure describing the configuration manager protocol interface.
*/
STATIC
CONST
EDKII_CONFIGURATION_MANAGER_PROTOCOL NVIDIAPlatformConfigManagerProtocol = {
  CREATE_REVISION (1, 0),
  NVIDIAPlatformGetObject,
  NVIDIAPlatformSetObject,
  NVIDIAPlatformRepositoryInfo
};

/**
  Entrypoint of Configuration Manager Dxe.

  @param  ImageHandle
  @param  SystemTable

  @return EFI_SUCCESS
  @return EFI_LOAD_ERROR
  @return EFI_OUT_OF_RESOURCES

**/
EFI_STATUS
EFIAPI
ConfigurationManagerDxeInitialize (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE  * SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEdkiiConfigurationManagerProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  (VOID*)&NVIDIAPlatformConfigManagerProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: Failed to get Install Configuration Manager Protocol." \
      " Status = %r\n",
      Status
      ));
    goto error_handler;
  }

  Status = InitializePlatformRepository ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: Failed to initialize the Platform Configuration Repository." \
      " Status = %r\n",
      Status
      ));
  }

error_handler:
  return Status;
}
