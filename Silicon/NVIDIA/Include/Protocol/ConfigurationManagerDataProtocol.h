/** @file

  Copyright (c) 2019 - 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/

#ifndef CONFIGURATION_MANAGER_DATA_PROTOCOL_H__
#define CONFIGURATION_MANAGER_DATA_PROTOCOL_H__

#define NVIDIA_CONFIGURATION_MANAGER_DATA_PROTOCOL_GUID \
  { \
  0x1a8fd893, 0x4752, 0x40b9, { 0x9b, 0xc7, 0x75, 0x94, 0x04, 0xff, 0xcd, 0xff } \
  }

/** The configuration manager version
*/
#define CONFIGURATION_MANAGER_REVISION  CREATE_REVISION (1, 0)

/** The OEM ID
*/
#define CFG_MGR_OEM_ID  { 'N', 'V', 'I', 'D', 'I', 'A' }

/** A helper macro for populating the GIC CPU information
*/
#define GICC_ENTRY(                                                      \
                                                                         CPUInterfaceNumber,                                            \
                                                                         Mpidr,                                                         \
                                                                         PmuIrq,                                                        \
                                                                         VGicIrq,                                                       \
                                                                         EnergyEfficiency,                                              \
                                                                         ProximityDomain                                                \
                                                                         )  {\
    CPUInterfaceNumber,       /* UINT32  CPUInterfaceNumber           */ \
    CPUInterfaceNumber,       /* UINT32  AcpiProcessorUid             */ \
    EFI_ACPI_6_4_GIC_ENABLED, /* UINT32  Flags                        */ \
    0,                        /* UINT32  ParkingProtocolVersion       */ \
    PmuIrq,                   /* UINT32  PerformanceInterruptGsiv     */ \
    0,                        /* UINT64  ParkedAddress                */ \
    0,                        /* UINT64  PhysicalBaseAddress          */ \
    0,                        /* UINT64  GICV                         */ \
    0,                        /* UINT64  GICH                         */ \
    VGicIrq,                  /* UINT32  VGICMaintenanceInterrupt     */ \
    0,                        /* UINT64  GICRBaseAddress              */ \
    Mpidr,                    /* UINT64  MPIDR                        */ \
    EnergyEfficiency,         /* UINT8   ProcessorPowerEfficiencyClass*/ \
    0,                        /* UINT16  SpeOverflowInterrupt         */ \
    ProximityDomain,          /* UINT32  ProximityDomain              */ \
    0,                        /* UINT32  ClockDomain                  */ \
    EFI_ACPI_6_4_GICC_ENABLED /* UINT32  AffinityFlags                */ \
    }

/** A helper macro for populating the Processor Hierarchy Node flags
*/
#define PROC_NODE_FLAGS(                                                \
                                                                        PhysicalPackage,                                              \
                                                                        AcpiProcessorIdValid,                                         \
                                                                        ProcessorIsThread,                                            \
                                                                        NodeIsLeaf,                                                   \
                                                                        IdenticalImplementation                                       \
                                                                        )                                                             \
  (                                                                     \
    PhysicalPackage |                                                   \
    (AcpiProcessorIdValid << 1) |                                       \
    (ProcessorIsThread << 2) |                                          \
    (NodeIsLeaf << 3) |                                                 \
    (IdenticalImplementation << 4)                                      \
  )

/** A helper macro for populating the Cache Type Structure's attributes
*/
#define CACHE_ATTRIBUTES(                                               \
                                                                        AllocationType,                                               \
                                                                        CacheType,                                                    \
                                                                        WritePolicy                                                   \
                                                                        )                                                             \
  (                                                                     \
    AllocationType |                                                    \
    (CacheType << 2) |                                                  \
    (WritePolicy << 4)                                                  \
  )

/** A helper macro for mapping a reference token
*/
#define REFERENCE_TOKEN(Field)                                           \
  ((CM_OBJECT_TOKEN)(VOID*)&(Field))

/** A structure describing the platform configuration
    manager repository information
*/
typedef struct PlatformRepositoryInfo {
  // Configuration Manager Object ID
  CM_OBJECT_ID       CmObjectId;

  // Configuration Manager Object Token
  CM_OBJECT_TOKEN    CmObjectToken;

  // Configuration Manager Object Size
  UINT32             CmObjectSize;

  // Configuration Manager Object Count
  UINT32             CmObjectCount;

  // Configuration Manager Object Pointer
  VOID               *CmObjectPtr;
} EDKII_PLATFORM_REPOSITORY_INFO;

extern EFI_GUID  gNVIDIAConfigurationManagerDataProtocolGuid;

#endif // CONFIGURATION_MANAGER_DATA_PROTOCOL_H__
