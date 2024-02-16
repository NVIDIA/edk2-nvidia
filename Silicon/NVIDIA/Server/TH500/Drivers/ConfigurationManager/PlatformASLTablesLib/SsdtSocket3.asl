/** @file
  SSDT for TH500 Socket 3 devices

  SPDX-FileCopyrightText: Copyright (c) 2022-2023, NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Template for [SSDT] ACPI Table (AML byte code table)
**/

#include <TH500/TH500Definitions.h>

DefinitionBlock("SsdtSocket3.aml", "SSDT", 2, "NVIDIA", "TH500_S3", 0x00000001) {
  Scope(_SB) {
    //---------------------------------------------------------------------
    // MCF Devices
    //---------------------------------------------------------------------
    //MCF NVLINK Chiplet
    Device (MNV3)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x30)
    }
    //MCF C2C Chiplet
    Device (MC23)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x31)
    }
    //GPU SOC-HUB Chiplet
    Device (GSH3)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x32)
    }
    //PCIe SOC-HUB Chiplet
    Device (PSH3)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x33)
    }

    //---------------------------------------------------------------------
    // Coresight Devices
    //---------------------------------------------------------------------
    // Coresight STM
    Device (STM3)
    {
      Name (_HID, "ARMHC502")
      Name (_UID, 0x3)

      Name (_CRS, ResourceTemplate () {
        //STM control register
        QWordMemory (
          ResourceProducer, // ResourceUsage
          PosDecode, // Decode
          MinFixed, // Min range is fixed
          MaxFixed, // Max range is fixed
          NonCacheable, // Cacheable
          ReadWrite, // Read And Write
          0x0000000000000000, // Address Granularity - GRA
          0x00007F0000070000, // Address Minimum - MIN
          0x00007F0000070FFF, // Address Maximum - MAX
          0x0000000000000000, // Address Translation - TRA
          0x0000000000001000, // Range Length - LEN
          , // Resource Source Index
          , // Resource Source
          // Descriptor Name
        )

        //STM stimulus register
        QWordMemory (
          ResourceProducer, // ResourceUsage
          PosDecode, // Decode
          MinFixed, // Min range is fixed
          MaxFixed, // Max range is fixed
          NonCacheable, // Cacheable
          ReadWrite, // Read And Write
          0x0000000000000000, // Address Granularity - GRA
          0x00007F0001000000, // Address Minimum - MIN
          0x00007F0001FFFFFF, // Address Maximum - MAX
          0x0000000000000000, // Address Translation - TRA
          0x0000000001000000, // Range Length - LEN
          , // Resource Source Index
          , // Resource Source
          // Descriptor Name
        )
      })

      Name (_DSD, Package () {
        ToUUID ("ab02a46b-74c7-45a2-bd68-f7d344ef2153"), Package () {
          0, // Revision
          1, // Number of graphs
          Package () {
            1, // GraphID // CoreSight graphs UUID
            ToUUID ("3ecbc8b6-1d0e-4fb3-8107-e627f805c6cd") ,
            1, // Number of links
            Package () { // output port 0 connected to input port 1 FUN3.
              0, // source port
              1, // destination port
              \_SB.FUN3, // destination device
              1 // flow (source as output port)
            }
          }
        }
      })
    }

    //Coresight Major Funnel
    Device (FUN3)
    {
      Name (_HID , "ARMHC9FF")
      Name (_UID , 0x1007)
      Name (_CID , "ARMHC500")

      Name (_CRS , ResourceTemplate () {
        QWordMemory (
          ResourceProducer, // ResourceUsage
          PosDecode, // Decode
          MinFixed, // Min range is fixed
          MaxFixed, // Max range is fixed
          NonCacheable, // Cacheable
          ReadWrite, // Read And Write
          0x0000000000000000, // Address Granularity - GRA
          0x00007F0000030000, // Address Minimum - MIN
          0x00007F0000030FFF, // Address Maximum - MAX
          0x0000000000000000, // Address Translation - TRA
          0x0000000000001000, // Range Length - LEN
          , // Resource Source Index
          , // Resource Source
          // Descriptor Name
        )
      })

      Name (_DSD , Package () {
        ToUUID ("ab02a46b-74c7-45a2-bd68-f7d344ef2153") ,
        Package () {
          0, // Revision
          1, // Number of graphs
          Package () {
            1, // GraphID // CoreSight graphs UUID
            ToUUID ("3ecbc8b6-1d0e-4fb3-8107-e627f805c6cd") ,
            2, // Number of links
            Package () { // input port 1 to connected to output port 0 STM3.
              1, // source port
              0, // destination port
              \_SB.STM3, // destination device
              0 // flow (source as input port)
            },
            Package () { // output port 0 connected to input port 0 on ETF3.
              0, // source port
              0, // destination port
              \_SB.ETF3, // destination device
              1 // flow (source as output port)
            }
          }
        }
      })
    }

    // Coresight ETF
    Device (ETF3) {
      Name (_HID , "ARMHC97C")
      Name (_UID , 0x1006)
      Name (_CID , "ARMHC500")

      Name (_CRS , ResourceTemplate () {
        QWordMemory (
          ResourceProducer, // ResourceUsage
          PosDecode, // Decode
          MinFixed, // Min range is fixed
          MaxFixed, // Max range is fixed
          NonCacheable, // Cacheable
          ReadWrite, // Read And Write
          0x0000000000000000, // Address Granularity - GRA
          0x00007F0000040000, // Address Minimum - MIN
          0x00007F0000040FFF, // Address Maximum - MAX
          0x0000000000000000, // Address Translation - TRA
          0x0000000000001000, // Range Length - LEN
          , // Resource Source Index
          , // Resource Source
          // Descriptor Name
        )
      })

      Name (_DSD , Package () {
        ToUUID ("ab02a46b-74c7-45a2-bd68-f7d344ef2153") ,
        Package () {
          0, // Revision
          1, // Number of graphs
          Package () {
            1, // GraphID // CoreSight graphs UUID
            ToUUID ("3ecbc8b6-1d0e-4fb3-8107-e627f805c6cd") ,
            1, // Number of links
            Package () { // input port 0 connected to output port 1 FUN3.
              0, // source port
              0, // destination port
              \_SB.FUN3, // destination device
              0 // flow (source as input port)
            },
          }
        }
      })
    }

    //---------------------------------------------------------------------
    // SMMU0_CMDQV @ 0x300011200000
    //---------------------------------------------------------------------
    Device (SQ30) {
      Name (_HID, "NVDA200C")
      Name (_UID, 0xFFFFFFFF)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                              // Granularity
                     TH500_SMMU0_CMDQV_BASE_SOCKET_3,  // Range Minimum
                     TH500_SMMU0_CMDQV_LIMIT_SOCKET_3, // Range Maximum
                     0x0,                              // Translation Offset
                     TH500_SMMU0_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_SMMU0_CMDQV_INTR_SOCKET_3 }
      })
    }

    //---------------------------------------------------------------------
    // SMMU1_CMDQV @ 0x300012200000
    //---------------------------------------------------------------------
    Device (SQ31) {
      Name (_HID, "NVDA200C")
      Name (_UID, 0xFFFFFFFF)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                              // Granularity
                     TH500_SMMU1_CMDQV_BASE_SOCKET_3,  // Range Minimum
                     TH500_SMMU1_CMDQV_LIMIT_SOCKET_3, // Range Maximum
                     0x0,                              // Translation Offset
                     TH500_SMMU1_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_SMMU1_CMDQV_INTR_SOCKET_3 }
      })
    }

    //---------------------------------------------------------------------
    // SMMU2_CMDQV @ 0x300015200000
    //---------------------------------------------------------------------
    Device (SQ32) {
      Name (_HID, "NVDA200C")
      Name (_UID, 0xFFFFFFFF)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                              // Granularity
                     TH500_SMMU2_CMDQV_BASE_SOCKET_3,  // Range Minimum
                     TH500_SMMU2_CMDQV_LIMIT_SOCKET_3, // Range Maximum
                     0x0,                              // Translation Offset
                     TH500_SMMU2_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_SMMU2_CMDQV_INTR_SOCKET_3 }
      })
    }

    //---------------------------------------------------------------------
    // GSMMU0_CMDQV @ 0x300016200000
    //---------------------------------------------------------------------
    Device (GQ30) {
      Name (_HID, "NVDA200C")
      Name (_UID, 0xFFFFFFFF)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                               // Granularity
                     TH500_GSMMU0_CMDQV_BASE_SOCKET_3,  // Range Minimum
                     TH500_GSMMU0_CMDQV_LIMIT_SOCKET_3, // Range Maximum
                     0x0,                               // Translation Offset
                     TH500_GSMMU0_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_GSMMU0_CMDQV_INTR_SOCKET_3 }
      })
    }

    //---------------------------------------------------------------------
    // GSMMU1_CMDQV @ 0x300005200000
    //---------------------------------------------------------------------
    Device (GQ31) {
      Name (_HID, "NVDA200C")
      Name (_UID, 0xFFFFFFFF)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                               // Granularity
                     TH500_GSMMU1_CMDQV_BASE_SOCKET_3,  // Range Minimum
                     TH500_GSMMU1_CMDQV_LIMIT_SOCKET_3, // Range Maximum
                     0x0,                               // Translation Offset
                     TH500_GSMMU1_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_GSMMU1_CMDQV_INTR_SOCKET_3 }
      })
    }
  } //Scope(_SB)
}
