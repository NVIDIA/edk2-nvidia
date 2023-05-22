/** @file
  SSDT for TH500 Socket 2 devices

  Copyright (c) 2022-2023, NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Template for [SSDT] ACPI Table (AML byte code table)
**/

#include <TH500/TH500Definitions.h>

DefinitionBlock("SsdtSocket2.aml", "SSDT", 2, "NVIDIA", "TH500_S2", 0x00000001) {
  Scope(_SB) {
    //---------------------------------------------------------------------
    // MCF Devices
    //---------------------------------------------------------------------
    //MCF NVLINK Chiplet
    Device (MNV2)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x20)
    }
    //MCF C2C Chiplet
    Device (MC22)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x21)
    }
    //GPU SOC-HUB Chiplet
    Device (GSH2)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x22)
    }
    //PCIe SOC-HUB Chiplet
    Device (PSH2)
    {
      Name (_HID, "NVDA1180")
      Name (_UID, 0x23)
    }

    //---------------------------------------------------------------------
    // Coresight Devices
    //---------------------------------------------------------------------
    // Coresight STM
    Device (STM2)
    {
      Name (_CID, "ARMHC502")

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
          0x0000770000070000, // Address Minimum - MIN
          0x0000770000070FFF, // Address Maximum - MAX
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
          0x0000770001000000, // Address Minimum - MIN
          0x0000770001FFFFFF, // Address Maximum - MAX
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
            Package () { // output port 0 connected to input port 1 FUN2.
              0, // source port
              1, // destination port
              \_SB.FUN2, // destination device
              1 // flow (source as output port)
            }
          }
        }
      })
    }

    //Coresight Major Funnel
    Device (FUN2)
    {
      Name (_HID , "ARMHC9FF")
      Name (_UID , 0x05)
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
          0x0000770000030000, // Address Minimum - MIN
          0x0000770000030FFF, // Address Maximum - MAX
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
            Package () { // input port 1 to connected to output port 0 STM2.
              1, // source port
              0, // destination port
              \_SB.STM2, // destination device
              0 // flow (source as input port)
            },
            Package () { // output port 0 connected to input port 0 on ETF2.
              0, // source port
              0, // destination port
              \_SB.ETF2, // destination device
              1 // flow (source as output port)
            }
          }
        }
      })
    }

    // Coresight ETF
    Device (ETF2) {
      Name (_HID , "ARMHC97C")
      Name (_UID , 0x04)
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
          0x0000770000040000, // Address Minimum - MIN
          0x0000770000040FFF, // Address Maximum - MAX
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
            Package () { // input port 0 connected to output port 1 FUN2.
              0, // source port
              0, // destination port
              \_SB.FUN2, // destination device
              0 // flow (source as input port)
            },
          }
        }
      })
    }

    //---------------------------------------------------------------------
    // SMMU0_CMDQV @ 0x200011200000
    //---------------------------------------------------------------------
    Device (SQ20) {
      Name (_HID, "NVDA200C")
      Name (_UID, TH500_SMMU0_BASE_SOCKET_2)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                              // Granularity
                     TH500_SMMU0_CMDQV_BASE_SOCKET_2,  // Range Minimum
                     TH500_SMMU0_CMDQV_LIMIT_SOCKET_2, // Range Maximum
                     0x0,                              // Translation Offset
                     TH500_SMMU0_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_SMMU0_CMDQV_INTR_SOCKET_2 }
      })
    }

    //---------------------------------------------------------------------
    // SMMU1_CMDQV @ 0x200012200000
    //---------------------------------------------------------------------
    Device (SQ21) {
      Name (_HID, "NVDA200C")
      Name (_UID, TH500_SMMU1_BASE_SOCKET_2)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                              // Granularity
                     TH500_SMMU1_CMDQV_BASE_SOCKET_2,  // Range Minimum
                     TH500_SMMU1_CMDQV_LIMIT_SOCKET_2, // Range Maximum
                     0x0,                              // Translation Offset
                     TH500_SMMU1_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_SMMU1_CMDQV_INTR_SOCKET_2 }
      })
    }

    //---------------------------------------------------------------------
    // SMMU2_CMDQV @ 0x200015200000
    //---------------------------------------------------------------------
    Device (SQ22) {
      Name (_HID, "NVDA200C")
      Name (_UID, TH500_SMMU2_BASE_SOCKET_2)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                              // Granularity
                     TH500_SMMU2_CMDQV_BASE_SOCKET_2,  // Range Minimum
                     TH500_SMMU2_CMDQV_LIMIT_SOCKET_2, // Range Maximum
                     0x0,                              // Translation Offset
                     TH500_SMMU2_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_SMMU2_CMDQV_INTR_SOCKET_2 }
      })
    }

    //---------------------------------------------------------------------
    // GSMMU0_CMDQV @ 0x200016200000
    //---------------------------------------------------------------------
    Device (GQ20) {
      Name (_HID, "NVDA200C")
      Name (_UID, TH500_GSMMU0_BASE_SOCKET_2)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                               // Granularity
                     TH500_GSMMU0_CMDQV_BASE_SOCKET_2,  // Range Minimum
                     TH500_GSMMU0_CMDQV_LIMIT_SOCKET_2, // Range Maximum
                     0x0,                               // Translation Offset
                     TH500_GSMMU0_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_GSMMU0_CMDQV_INTR_SOCKET_2 }
      })
    }

    //---------------------------------------------------------------------
    // GSMMU1_CMDQV @ 0x200005200000
    //---------------------------------------------------------------------
    Device (GQ21) {
      Name (_HID, "NVDA200C")
      Name (_UID, TH500_GSMMU1_BASE_SOCKET_2)
      Name (_CCA, 1)

      Name (_CRS, ResourceTemplate() {
        QWordMemory (ResourceConsumer, , MinFixed, MaxFixed, Cacheable, ReadWrite,
                     0x0,                               // Granularity
                     TH500_GSMMU1_CMDQV_BASE_SOCKET_2,  // Range Minimum
                     TH500_GSMMU1_CMDQV_LIMIT_SOCKET_2, // Range Maximum
                     0x0,                               // Translation Offset
                     TH500_GSMMU1_CMDQV_SIZE            // Length
                     )
        Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive) { TH500_GSMMU1_CMDQV_INTR_SOCKET_2 }
      })
    }
  } //Scope(_SB)
}
