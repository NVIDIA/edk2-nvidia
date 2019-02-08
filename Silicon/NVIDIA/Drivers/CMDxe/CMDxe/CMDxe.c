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
#include <Protocol/AcpiTable.h>

#include <ArmPlatform.h>
#include <AcpiTableGenerator.h>
#include <Protocol/ConfigurationManagerProtocol.h>

#include <Dsdt.hex>

#include "../CMDxe/Platform.h"


/** The platform configuration repository information.
*/
STATIC
EDKII_PLATFORM_REPOSITORY_INFO NVIDIAPlatformRepositoryInfo = {
  /// Configuration Manager information
  { CONFIGURATION_MANAGER_REVISION, CFG_MGR_OEM_ID },

  // ACPI Table List
  {
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
  },

  // Boot architecture information
  { EFI_ACPI_6_2_ARM_PSCI_COMPLIANT },            // BootArchFlags

  // Power management profile information
  { EFI_ACPI_6_2_PM_PROFILE_ENTERPRISE_SERVER },  // PowerManagement Profile

  /* GIC CPU Interface information
     GIC_ENTRY (CPUInterfaceNumber, Mpidr, PmuIrq, VGicIrq, EnergyEfficiency)
  */
  {
    GICC_ENTRY (0, GET_MPID (0, 0), 0x140, 0,     0),
    GICC_ENTRY (1, GET_MPID (0, 1), 0x141, 0,     0),
    GICC_ENTRY (2, GET_MPID (1, 0), 0,     0x128, 0),
    GICC_ENTRY (3, GET_MPID (1, 1), 0x129, 0,     0),
    GICC_ENTRY (4, GET_MPID (1, 2), 0x12A, 0,     0),
    GICC_ENTRY (5, GET_MPID (1, 3), 0x12B, 0,     0)
  },

  // GIC Distributor Info
  {
    FixedPcdGet64 (PcdGicDistributorBase),  // UINT64  PhysicalBaseAddress
    0,                                      // UINT32  SystemVectorBase
    2                                       // UINT8   GicVersion
  },

  // Generic Timer Info
  {
    // The physical base address for the counter control frame
    SYSTEM_COUNTER_BASE_ADDRESS,
    // The physical base address for the counter read frame
    SYSTEM_COUNTER_READ_BASE,
    // The secure PL1 timer interrupt
    FixedPcdGet32 (PcdArmArchTimerSecIntrNum),
    // The secure PL1 timer flags
    GTDT_GTIMER_FLAGS,
    // The non-secure PL1 timer interrupt
    FixedPcdGet32 (PcdArmArchTimerIntrNum),
    // The non-secure PL1 timer flags
    GTDT_GTIMER_FLAGS,
    // The virtual timer interrupt
    FixedPcdGet32 (PcdArmArchTimerVirtIntrNum),
    // The virtual timer flags
    GTDT_GTIMER_FLAGS,
    // The non-secure PL2 timer interrupt
    FixedPcdGet32 (PcdArmArchTimerHypIntrNum),
    // The non-secure PL2 timer flags
    GTDT_GTIMER_FLAGS
  },

  // SPCR Serial Port
  {
    FixedPcdGet64 (PcdSerialRegisterBase),            // UINT64  BaseAddress
    0x90,                                             // UINT32  Interrupt
    FixedPcdGet64 (PcdUartDefaultBaudRate),           // UINT64  BaudRate
    0,                                                // UINT32  Clock
    EFI_ACPI_DBG2_PORT_SUBTYPE_SERIAL_FULL_16550      // UINT16  Port subtype
  }
};

/** Return a standard namespace object.

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
GetStandardNameSpaceObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  * CONST This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     * CONST CmObject
  )
{
  EFI_STATUS                        Status;
  EDKII_PLATFORM_REPOSITORY_INFO  * PlatformRepo;

  Status = EFI_SUCCESS;
  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }
  PlatformRepo = This->PlatRepoInfo;

  switch (GET_CM_OBJECT_ID (CmObjectId)) {
    HANDLE_CM_OBJECT (EStdObjCfgMgrInfo, PlatformRepo->CmInfo, CM_STD_OBJ_CONFIGURATION_MANAGER_INFO, EObjNameSpaceStandard);
    HANDLE_CM_OBJECT (EStdObjAcpiTableList, PlatformRepo->CmAcpiTableList, CM_STD_OBJ_ACPI_TABLE_INFO, EObjNameSpaceStandard);

    default: {
      Status = EFI_NOT_FOUND;
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: Object 0x%x. Status = %r\n",
        CmObjectId,
        Status
        ));
      break;
    }
  }

  return Status;
}

/** Return an ARM namespace object.

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
GetArmNameSpaceObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  * CONST This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     * CONST CmObject
  )
{
  EFI_STATUS                        Status;
  EDKII_PLATFORM_REPOSITORY_INFO  * PlatformRepo;

  Status = EFI_SUCCESS;
  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }
  PlatformRepo = This->PlatRepoInfo;

  switch (GET_CM_OBJECT_ID (CmObjectId)) {
    HANDLE_CM_OBJECT (EArmObjBootArchInfo, PlatformRepo->BootArchInfo, CM_ARM_BOOT_ARCH_INFO, EObjNameSpaceArm);
    HANDLE_CM_OBJECT (EArmObjPowerManagementProfileInfo, PlatformRepo->PmProfileInfo, CM_ARM_POWER_MANAGEMENT_PROFILE_INFO, EObjNameSpaceArm);
    HANDLE_CM_OBJECT (EArmObjGicCInfo, PlatformRepo->GicCInfo, CM_ARM_GICC_INFO, EObjNameSpaceArm);
    HANDLE_CM_OBJECT (EArmObjGicDInfo, PlatformRepo->GicDInfo, CM_ARM_GICD_INFO, EObjNameSpaceArm);
    HANDLE_CM_OBJECT (EArmObjGenericTimerInfo, PlatformRepo->GenericTimerInfo, CM_ARM_GENERIC_TIMER_INFO, EObjNameSpaceArm);
    HANDLE_CM_OBJECT (EArmObjSerialConsolePortInfo, PlatformRepo->SpcrSerialPort, CM_ARM_SERIAL_PORT_INFO, EObjNameSpaceArm);

    default: {
      Status = EFI_NOT_FOUND;
      DEBUG ((
        DEBUG_INFO,
        "INFO: Object 0x%x. Status = %r\n",
        CmObjectId,
        Status
        ));
      break;
    }
  }//switch

  return Status;
}

/** Return an OEM namespace object.

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
GetOemNameSpaceObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  * CONST This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     * CONST CmObject
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;
  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  switch (GET_CM_OBJECT_ID (CmObjectId)) {
    default: {
      Status = EFI_NOT_FOUND;
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: Object 0x%x. Status = %r\n",
        CmObjectId,
        Status
        ));
      break;
    }
  }

  return Status;
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

  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  switch (GET_CM_NAMESPACE_ID (CmObjectId)) {
    case EObjNameSpaceStandard:
      Status = GetStandardNameSpaceObject (This, CmObjectId, Token, CmObject);
      break;
    case EObjNameSpaceArm:
      Status = GetArmNameSpaceObject (This, CmObjectId, Token, CmObject);
      break;
    case EObjNameSpaceOem:
      Status = GetOemNameSpaceObject (This, CmObjectId, Token, CmObject);
      break;
    default: {
      Status = EFI_INVALID_PARAMETER;
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: Unknown Namespace Object = 0x%x. Status = %r\n",
        CmObjectId,
        Status
        ));
      break;
    }
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
  &NVIDIAPlatformRepositoryInfo
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

error_handler:
  return Status;
}
