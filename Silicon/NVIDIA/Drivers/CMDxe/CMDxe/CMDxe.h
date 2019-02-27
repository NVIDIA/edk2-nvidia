/** @file

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

#ifndef CONFIGURATION_MANAGER_H__
#define CONFIGURATION_MANAGER_H__

/** The configuration manager version
*/
#define CONFIGURATION_MANAGER_REVISION CREATE_REVISION (1, 0)

/** The OEM ID
*/
#define CFG_MGR_OEM_ID    { 'N', 'V', 'I', 'D', 'I', 'A' }

/** A helper macro for populating the GIC CPU information
*/
#define GICC_ENTRY(                                                      \
          CPUInterfaceNumber,                                            \
          Mpidr,                                                         \
          PmuIrq,                                                        \
          VGicIrq,                                                       \
          EnergyEfficiency                                               \
          ) {                                                            \
    CPUInterfaceNumber,       /* UINT32  CPUInterfaceNumber           */ \
    CPUInterfaceNumber,       /* UINT32  AcpiProcessorUid             */ \
    EFI_ACPI_6_2_GIC_ENABLED, /* UINT32  Flags                        */ \
    0,                        /* UINT32  ParkingProtocolVersion       */ \
    PmuIrq,                   /* UINT32  PerformanceInterruptGsiv     */ \
    0,                        /* UINT64  ParkedAddress                */ \
    FixedPcdGet64 (                                                      \
      PcdGicInterruptInterfaceBase                                       \
      ),                      /* UINT64  PhysicalBaseAddress          */ \
    0,                        /* UINT64  GICV                         */ \
    0,                        /* UINT64  GICH                         */ \
    VGicIrq,                  /* UINT32  VGICMaintenanceInterrupt     */ \
    0,                        /* UINT64  GICRBaseAddress              */ \
    Mpidr,                    /* UINT64  MPIDR                        */ \
    EnergyEfficiency          /* UINT8   ProcessorPowerEfficiencyClass*/ \
    }

/** A helper macro for returning configuration manager objects
*/
#define HANDLE_CM_OBJECT(CmObjectId, Object, Type, NameSpaceId)        \
  case CmObjectId: {                                                   \
    CmObject->Size = sizeof (Object);                                  \
    CmObject->Data = (VOID*)&Object;                                   \
    CmObject->ObjectId = CREATE_CM_OBJECT_ID(NameSpaceId, CmObjectId); \
    CmObject->Count = sizeof (Object) / sizeof (Type);                 \
    DEBUG ((                                                           \
      DEBUG_INFO,                                                      \
      #CmObjectId ": Ptr = 0x%p, Size = %d\n",                         \
      CmObject->Data,                                                  \
      CmObject->Size                                                   \
      ));                                                              \
    break;                                                             \
  }

/** A helper macro for returning configuration manager objects
    referenced by token
*/
#define HANDLE_CM_OBJECT_REF_BY_TOKEN(                                   \
          CmObjectId,                                                    \
          Object,                                                        \
          Type,                                                          \
          NameSpaceId,                                                   \
          Token,                                                         \
          HandlerProc                                                    \
          )                                                              \
  case CmObjectId: {                                                     \
    if (Token == CM_NULL_TOKEN) {                                        \
      CmObject->Size = sizeof (Object);                                  \
      CmObject->Data = (VOID*)&Object;                                   \
      CmObject->ObjectId = CREATE_CM_OBJECT_ID(NameSpaceId, CmObjectId); \
      CmObject->Count = sizeof (Object) / sizeof (Type);                 \
      DEBUG ((                                                           \
        DEBUG_INFO,                                                      \
        #CmObjectId ": Ptr = 0x%p, Size = %d\n",                         \
        CmObject->Data,                                                  \
        CmObject->Size                                                   \
        ));                                                              \
    } else {                                                             \
      Status = HandlerProc (This, CmObjectId, Token, CmObject);          \
      DEBUG ((                                                           \
        DEBUG_INFO,                                                      \
        #CmObjectId ": Token = 0x%p, Ptr = 0x%p, Size = %d\n",           \
        (VOID*)Token,                                                    \
        CmObject->Data,                                                  \
        CmObject->Size                                                   \
        ));                                                              \
    }                                                                    \
    break;                                                               \
  }

/** The number of CPUs
*/
#define PLAT_CPU_COUNT          6

/** The number of ACPI tables to install
*/
#define PLAT_ACPI_TABLE_COUNT   10

/** A structure describing the platform configuration
    manager repository information
*/
typedef struct PlatformRepositoryInfo {
  /// Configuration Manager Information
  CM_STD_OBJ_CONFIGURATION_MANAGER_INFO CmInfo;

  /// List of ACPI tables
  CM_STD_OBJ_ACPI_TABLE_INFO            CmAcpiTableList[PLAT_ACPI_TABLE_COUNT];

  /// Boot architecture information
  CM_ARM_BOOT_ARCH_INFO                 BootArchInfo;

  /// Power management profile information
  CM_ARM_POWER_MANAGEMENT_PROFILE_INFO  PmProfileInfo;

  /// GIC CPU interface information
  CM_ARM_GICC_INFO                      GicCInfo[PLAT_CPU_COUNT];

  /// GIC distributor information
  CM_ARM_GICD_INFO                      GicDInfo;

  /// Generic timer information
  CM_ARM_GENERIC_TIMER_INFO             GenericTimerInfo;

  /** Serial port information for the
      serial port console redirection port
  */
  CM_ARM_SERIAL_PORT_INFO               SpcrSerialPort;
} EDKII_PLATFORM_REPOSITORY_INFO;

#endif // CONFIGURATION_MANAGER_H__
