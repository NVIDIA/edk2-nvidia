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

/** A structure describing the platform configuration
    manager repository information
*/
typedef struct PlatformRepositoryInfo {
  // Configuration Manager Object ID
  CM_OBJECT_ID   CmObjectId;

  // Configuration Manager Object Size
  UINT32         CmObjectSize;

  // Configuration Manager Object Count
  UINT32         CmObjectCount;

  // Configuration Manager Object Pointer
  VOID         * CmObjectPtr;
} EDKII_PLATFORM_REPOSITORY_INFO;

#endif // CONFIGURATION_MANAGER_H__
